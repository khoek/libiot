#include "mqtt.h"

#include <esp_log.h>
#include <mqtt_client.h>
#include <stdio.h>

#include "certs.h"
#include "gpio.h"
#include "json_builder.h"

#ifndef LIBIOT_DISABLE_OTA
#include "ota.h"
#endif

static char device_topic_root[64];
static size_t device_topic_root_len;

static void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event);
static StaticSemaphore_t startup_sem_buffer;
static SemaphoreHandle_t startup_sem;

static esp_mqtt_client_handle_t client = NULL;

static void send_resp(const char *suffix, char *msg, bool retain) {
    assert(msg);
    libiot_mqtt_publish_local(suffix, 2, retain ? 1 : 0, msg);
    free(msg);
}

void mqtt_send_ping_resp() {
    send_resp(MQTT_TOPIC_INFO("status"), json_build_state_up(), true);
}

void mqtt_send_refresh_resp() {
    send_resp(MQTT_TOPIC_INFO("id"), json_build_system_id(), true);
    send_resp(MQTT_TOPIC_INFO("last_reset"), json_build_last_reset(), true);
}

void mqtt_send_mem_check_resp() {
    send_resp(MQTT_TOPIC_INFO("mem_check"), json_build_mem_check(), false);
}

// Note: `topic_suffix` must be null terminated, but `topic` need not be, and `len` is the length of the latter.
static bool matches_local_topic(const char *topic_suffix, const char *topic, size_t len) {
    if (len < device_topic_root_len || memcmp(device_topic_root, topic, device_topic_root_len)) {
        return false;
    }

    topic += device_topic_root_len;
    len -= device_topic_root_len;

    if (!len || *topic != '/') {
        return false;
    }

    topic++;
    len--;

    while (*topic_suffix) {
        if (!len || *topic_suffix != *topic) {
            return false;
        }
        topic_suffix++;
        topic++;
        len--;
    }

    if (len) {
        return false;
    }

    return true;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "mqtt event: base='%s', event_id=%d", base, event_id);

    bool did_connect = false;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            // TODO Could this technically fail to arrive? Do something
            // fancy here to periodically check?
            libiot_mqtt_subscribe(IOT_MQTT_COMMAND_TOPIC("ping"), 0);
            libiot_mqtt_subscribe_local("#", 0);

            // Send the up status message and WiFi RSSI info
            mqtt_send_ping_resp();

            // Publish device hardware information and the last reset reason
            mqtt_send_refresh_resp();

            did_connect = true;

            gpio_led_set_state(true);
            ESP_LOGI(TAG, "mqtt connected, up status published");
            break;
        }
        case MQTT_EVENT_DISCONNECTED: {
            // Note that multiple instances of this event type may arrive without first
            // being connected again---indeed, the underlying esp-mqtt library issues
            // a disconnect event whenever it fails to reconnect.

            // Note: we never explicitly publish a `STATUS_DOWN` message, since if it
            // ever arrives it's wrong! (Instead `STATUS_DOWN` is our LWT, and we
            // public `STATUS_UP` whenever we come back up.)

            gpio_led_set_state(false);
            ESP_LOGI(TAG, "mqtt disconnected");
            break;
        }
        case MQTT_EVENT_DATA: {
            if (matches_local_topic(MQTT_TOPIC_CMD("restart"), event->topic, event->topic_len)) {
                // Any message to this topic will trigger a restart of the ESP32.
                ESP_LOGW(TAG, "mqtt: restart");
                esp_restart();
            }

#ifndef LIBIOT_DISABLE_OTA
            if (matches_local_topic(MQTT_TOPIC_CMD("ota"), event->topic, event->topic_len)) {
                // Message to begin OTA
                ESP_LOGW(TAG, "mqtt: ota");

                char *dup = strndup(event->data, event->data_len);
                ota_dispatch_request(dup);
            }
#endif

            if (!strncmp(IOT_MQTT_COMMAND_TOPIC("ping"), event->topic, event->topic_len)) {
                // Re-publish up status whenever pinged
                ESP_LOGI(TAG, "mqtt: ping");
                mqtt_send_ping_resp();
            }

            if (matches_local_topic(MQTT_TOPIC_CMD("refresh"), event->topic, event->topic_len)) {
                // Re-publish up hardware information whenever refreshed
                ESP_LOGI(TAG, "mqtt: refresh");
                mqtt_send_refresh_resp();
            }

            if (matches_local_topic(MQTT_TOPIC_CMD("mem_check"), event->topic, event->topic_len)) {
                // Perform a memory integrity check, and also report the current heap state.
                ESP_LOGI(TAG, "mqtt: mem_check");
                mqtt_send_mem_check_resp();
            }

            break;
        }
        default: {
            break;
        }
    }

    if (mqtt_event_handler_cb) {
        mqtt_event_handler_cb(event);
    }

    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
    if (stack_remaining <= 0) {
        ESP_LOGE(TAG, "mqtt task stack overflow detected!");
    }
    ESP_LOGD(TAG, "mqtt task stack words remaining: %u", stack_remaining);

    if (did_connect) {
        xSemaphoreGive(&startup_sem_buffer);
    }
}

void mqtt_init(const char *uri, const char *cert, const char *key, const char *name, const char *pass,
               int mqtt_task_stack_size, void (*cb)(esp_mqtt_event_handle_t event)) {
    mqtt_event_handler_cb = cb;

    device_topic_root_len = snprintf(device_topic_root, sizeof(device_topic_root), IOT_MQTT_DEVICE_TOPIC_ROOT("%s"), name);
    assert(device_topic_root_len + 1 <= sizeof(device_topic_root));

    char *lwt_topic;
    assert(asprintf(&lwt_topic, "%s/" MQTT_TOPIC_INFO("status"), device_topic_root) >= 0);
    char *lwt_msg = json_build_state_down();

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = uri,
        .cert_pem = CERT_AUTHORITY_ENDPOINT,
        .client_cert_pem = cert,
        .client_key_pem = key,
        .username = name,
        .password = pass,

        // Keepalive interval in seconds (this is the time between ping messages exchanged with the
        // broker, i.e. roughly how long it will take for other devices to observe that we are down.)
        .keepalive = 1,

        // If `task_stack` is not positive then `CONFIG_MQTT_TASK_STACK_SIZE` is used by esp-mqtt.
        .task_stack = mqtt_task_stack_size,

        // "Last Will and Testament" status (down) message
        .lwt_topic = lwt_topic,
        .lwt_msg = lwt_msg,
        .lwt_qos = 2,
        .lwt_retain = 1,
    };

    startup_sem = xSemaphoreCreateBinaryStatic(&startup_sem_buffer);

    client = esp_mqtt_client_init(&mqtt_cfg);

    free(lwt_topic);
    free(lwt_msg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    while (xSemaphoreTake(startup_sem, portMAX_DELAY) == pdFALSE)
        ;
}

static void build_full_topic_suffix(char *buff, size_t buff_len, const char *topic_suffix) {
    int len = snprintf(buff, buff_len, "%s/%s", device_topic_root, topic_suffix);
    assert(len + 1 <= buff_len);
}

#define TOPIC_BUFF_SIZE 64

void libiot_mqtt_subscribe(const char *topic, int qos) {
    assert(esp_mqtt_client_subscribe(client, topic, qos) >= 0);
}

void libiot_mqtt_subscribe_local(const char *topic_suffix, int qos) {
    char topic_buff[TOPIC_BUFF_SIZE];
    build_full_topic_suffix(topic_buff, sizeof(topic_buff), topic_suffix);
    libiot_mqtt_subscribe(topic_buff, qos);
}

void libiot_mqtt_publish(const char *topic, int qos, int retain, const char *msg) {
    assert(esp_mqtt_client_publish(client, topic, msg, 0, qos, retain) >= 0);
}

void libiot_mqtt_publish_local(const char *topic_suffix, int qos, int retain, const char *msg) {
    char topic_buff[TOPIC_BUFF_SIZE];
    build_full_topic_suffix(topic_buff, sizeof(topic_buff), topic_suffix);
    libiot_mqtt_publish(topic_buff, qos, retain, msg);
}

void libiot_mqtt_publishv_local(const char *topic_suffix, int qos, int retain, const char *fmt, va_list va) {
    char *msg;
    if (vasprintf(&msg, fmt, va) < 0) {
        return;
    }

    libiot_mqtt_publish_local(topic_suffix, qos, retain, msg);

    free(msg);
}

void libiot_mqtt_publishf_local(const char *topic_suffix, int qos, int retain, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);

    libiot_mqtt_publishv_local(topic_suffix, qos, retain, fmt, va);

    va_end(va);
}
