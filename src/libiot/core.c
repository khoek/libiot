#include <esp32/rom/rtc.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <libesp.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <sys/cdefs.h>

#include "gpio.h"
#include "libiot.h"
#include "mqtt.h"
#include "ota.h"
#include "reset_info.h"
#include "sntp.h"
#include "wifi.h"

static char *instance_uuid = NULL;
static uint64_t start_epoch_time_ms = 0;

const char *libiot_get_instance_uuid() {
    return instance_uuid;
}

uint64_t libiot_get_start_epoch_time_ms() {
    return start_epoch_time_ms;
}

// Must be called after `start_sntp()`.
static void init_id() {
    uuid_t uuid;
    util_generate_uuid4(&uuid);
    util_print_uuid(&instance_uuid, &uuid);

    uint64_t current_ms = util_current_epoch_time_ms();
    uint64_t uptime_ms = esp_timer_get_time() / 1000;
    if (uptime_ms > current_ms) {
        current_ms = 0;
    } else {
        current_ms -= uptime_ms;
    }
    start_epoch_time_ms = current_ms;
}

static esp_err_t init_nvs() {
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES
        || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static __unused esp_err_t init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "failed to initialize SPIFFS (%s)",
                     esp_err_to_name(ret));
        }
    }
    return ret;
}

static void run_app(const node_config_t *cfg) {
    ESP_LOGI(TAG, "startup");

    libiot_init_reset_info();
    libiot_init_gpio();
    ESP_ERROR_CHECK(init_nvs());

#ifdef LIBIOT_ENABLE_SPIFFS
    ESP_ERROR_CHECK(init_spiffs());
#endif

#ifndef LIBIOT_DISABLE_OTA
    ESP_ERROR_CHECK(libiot_init_ota());
#endif

    // Called even if wifi/mqtt will not be started in order to initialize
    // logging structures before calls to access them may be made during
    // `cfg->app_init`.
    libiot_init_mqtt(cfg->name);

    if (cfg->app_init) {
        cfg->app_init();
    }

#ifndef LIBIOT_DISABLE_WIFI
    ESP_LOGI(TAG, "init wifi/mqtt");
    if (cfg->ssid) {
        libiot_start_wifi(cfg->ssid, cfg->pass, cfg->name, cfg->ps_type);

        libiot_init_sntp();
        // This function blocks until the network time has been synced for the
        // first time.
        libiot_start_sntp();

        init_id();

        // Note that if `cfg->mqtt_task_stack_size == 0` then a default is used.
        if (cfg->uri) {
            libiot_start_mqtt(cfg->uri, cfg->cert, cfg->key, cfg->name,
                              cfg->mqtt_pass, cfg->mqtt_task_stack_size,
                              cfg->mqtt_cb);
        } else {
            ESP_LOGI(TAG, "mqtt disabled");
        }
    } else {
        ESP_LOGW(
            TAG,
            "wifi disabled -- `#define LIBIOT_DISABLE_WIFI` to save code size");
    }
#endif

    ESP_LOGI(TAG, "startup finished, calling app_run()");
    if (cfg->app_run) {
        cfg->app_run();
    }
    ESP_LOGI(TAG, "app_run() returned, cleaning up");
}

static void task_run_app(void *arg) {
    run_app((const node_config_t *) arg);

    vTaskDelete(NULL);
}

#define STARTUP_TASK_STACK_SIZE 32768
#define STARTUP_TASK_PRIORITY 5

void libiot_startup(const node_config_t *cfg) {
    xTaskCreate(&task_run_app, "libiot_run_app", STARTUP_TASK_STACK_SIZE,
                (void *) cfg, STARTUP_TASK_PRIORITY, NULL);
}

void libiot_logf_error(const char *tag, const char *format, ...) {
    va_list va;
    va_start(va, format);

    char *msg;
    assert(vasprintf(&msg, format, va) >= 0);

    va_end(va);

    ESP_LOGE(tag, "%s", msg);
    libiot_mqtt_publishf_local(MQTT_TOPIC_INFO("error"), 2, 0, "%s: %s", tag,
                               msg);

    free(msg);
}
