#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <mdns.h>
#include <string.h>

// Negative means infinite retries
#define NUM_RETRIES -1

static StaticEventGroup_t wifi_event_group_static;
static EventGroupHandle_t wifi_event_group;

static StaticSemaphore_t local_ip_mutex_static;
static SemaphoreHandle_t local_ip_mutex;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT (1ULL << 0)
#define WIFI_FAIL_BIT (1ULL << 1)

static int retry_count = 0;
static char *local_ip = NULL;
static char *hostname = NULL;

const char *libiot_get_local_ip() {
    while (xSemaphoreTake(local_ip_mutex, portMAX_DELAY) == pdFALSE)
        ;

    char *local_ip_copy = strdup(local_ip);

    xSemaphoreGive(local_ip_mutex);

    return local_ip_copy;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "hostname set to: %s", hostname);

        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));

        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        while (xSemaphoreTake(local_ip_mutex, portMAX_DELAY) == pdFALSE)
            ;

        void *to_free = local_ip;
        local_ip = NULL;

        xSemaphoreGive(local_ip_mutex);

        free(to_free);

        if (NUM_RETRIES < 0 || retry_count < NUM_RETRIES) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGW(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "failed to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        char *buff = (char *) malloc(16);
        snprintf(buff, 16, IPSTR, IP2STR(&event->ip_info.ip));

        while (xSemaphoreTake(local_ip_mutex, portMAX_DELAY) == pdFALSE)
            ;

        assert(!local_ip);
        local_ip = buff;

        xSemaphoreGive(local_ip_mutex);

        ESP_LOGI(TAG, "(%d retries) got ip: %s", retry_count, local_ip);
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(const char *ssid, const char *pass, wifi_ps_type_t ps_type) {
    ESP_LOGI(TAG, "wifi init start");

    wifi_event_group = xEventGroupCreateStatic(&wifi_event_group_static);
    local_ip_mutex = xSemaphoreCreateMutexStatic(&local_ip_mutex_static);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    assert(strlen(ssid) < 32);  // Remember the null byte! (hence strict)
    assert(strlen(pass) < 64);  // Remember the null byte! (hence strict)
    strcpy((char *) &wifi_config.sta.ssid, ssid);
    strcpy((char *) &wifi_config.sta.password, pass);
    /* Setting a password implies station will connect to all security modes including WEP/WPA.
     * However these modes are deprecated and not advisable to be used. Incase your Access point
     * doesn't support WPA2, these mode can be enabled by commenting below line */
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(ps_type));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi init done, waiting for connection");

    // TODO Can comment out below (and boot without already having WiFi) once (I think) the fix for
    // https://github.com/espressif/esp-idf/issues/6878 makes its way to ESP-IDF.

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to AP SSID: %s", ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "failed to connect to SSID: %s", pass);
    } else {
        ESP_LOGE(TAG, "unexpected event: 0x%X", bits);
    }
}

void wifi_init(const char *ssid, const char *pass, const char *name, wifi_ps_type_t ps_type) {
    hostname = strdup(name);
    wifi_init_sta(ssid, pass, ps_type);
}
