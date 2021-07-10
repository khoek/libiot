#include "mqtt.h"

#include <esp_log.h>
#include <mqtt_client.h>
#include <stdio.h>

#include "gpio.h"
#include "json_builder.h"

#ifndef LIBIOT_DISABLE_OTA
#include "ota.h"
#endif

/*
    The certficate hierachy works like this (each bullet point is a certificate):
        * hoek.io Root Certificate Authority
            * IOT Device Authority
                * buzzer
                * powermeter
                * <etc.>
            * Endpoint Authority
                * storagebox.local (MQTT broker)
    
    The endpoint authority certficate and root certificate form a chain encoded in
    the string `CERT_AUTHORITY_ENDPOINT` below.
*/

#define CERT_AUTHORITY_ENDPOINT \
    "-----BEGIN CERTIFICATE-----\n\
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\n\
GhQGyA==\n\
-----END CERTIFICATE-----\n\
-----BEGIN CERTIFICATE-----\n\
MIIFcTCCA1mgAwIBAgIUF2GRQo6ngKDJS+/g4u+7+52MSvAwDQYJKoZIhvcNAQEM\n\
BQAwOTEYMBYGA1UEAxMPaG9lay5pbyBSb290IENBMRAwDgYDVQQKEwdob2VrLmlv\n\
MQswCQYDVQQGEwJBVTAgFw0yMDExMTcxNzIwMTJaGA85OTk5MTIzMTIzNTk1OVow\n\
OTEYMBYGA1UEAxMPaG9lay5pbyBSb290IENBMRAwDgYDVQQKEwdob2VrLmlvMQsw\n\
CQYDVQQGEwJBVTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANHwZh4A\n\
LRVp6+cRUrm8EVG/1tA3cxUrJ/4Vs9/s04rYhPOdztQDlj8gGBTukzX6K+lzDe+H\n\
jqRW6DUCRBX20t3je85O8SoYjeknmz8aM2haZA5Zct7sP8Ts/VVHvUd3pZejLpAr\n\
K6AsxibuxV7K9g2j13HJtOmPaGbgw4xvMIK2Gfu6P7naQMiEDGGy2G12xtiUqhV5\n\
Vn/V7IxBlrJS6JGvyqn1c1nSECzhFFcQcVbe3rVfXyQ+eRZN2nHbBEXhQZXWgwpQ\n\
9r0FInzxbu83U9+0whmjcMzxZNHbHAFTvitUwF5J9V50pzM6Rq9epEbiGqlb7Rlz\n\
RO39VkcPNytXgpPrFjheq9kNShzgDsZc8OFxd12WTcK4BaGR7IOfVQqlN/T29YAQ\n\
ccMNbQ0APrLMrbccba5U52zx2BuEr8ynwVAyzNKjsPtE0fSJ95gxe6FavfBTwBi1\n\
jAH0eBKstZwO6z5l7Bs15zeZ1i89BxIKjOJG6o3l00xp8fDN/w2uRjIcKsoB5l2+\n\
aoJldOn4q290auGYZb+MfXP7rDYui8x/wkWOIy6nALTQOsXPE6W8MLRJZs3p44W2\n\
n0B1PaJBx4MVoPKMqd+nbqCMYP1S4CHcwnaK9gevAq/ZbVfSOLdrJZz3Dth54Va8\n\
MV6xOVOphoFLdyPAsmvJ8q/xHF5YzHZ6Cp+zAgMBAAGjbzBtMA8GA1UdEwEB/wQF\n\
MAMBAf8wKgYDVR0RBCMwIYYPaHR0cHM6Ly9ob2VrLmlvgQ5rZWVsZXlAaG9lay5p\n\
bzAPBgNVHQ8BAf8EBQMDBwYAMB0GA1UdDgQWBBTROHmIsg8u4h21r9SremjUDEbB\n\
wjANBgkqhkiG9w0BAQwFAAOCAgEABurbIdU5yIdUwZjjD+p3QprxU4hYLNzg8SOs\n\
lUnA7OoO9lUAFQjMI5j0QJT4gx1y5V0pxmBxS1dry6wep25vuHuw1fBk+NbMsu7U\n\
ygNEK4F/E5h8mfW33QbEUjyLzOTWrpo1SNoc4peNnffeqh6PKX2IVAVf9e1VI+C8\n\
fjfEEo152QfeHxE1SZhhnDsqrJl/uuPUD1Hep0XEDOwWog6tln66pEr9AH6okBDh\n\
5rJwIojWoyBz1tZBmZeiyayocQ56mrNERaOWzCDzIBRDmOR2w+j0rTIMTxXJADfw\n\
bbcgVaBi5doSGAMoGltPW3VJ7juo7wobfG1KcF8DsERPTRrTFExGj2WMHLLhD1Qq\n\
Val5JEdKgpRrRdNlW4zIX2hs/UUvJn9e4DM8LROpa0+ZULadaRu3kZ0a+W2ONyhr\n\
3kEUUdeL6NHIkfmoY2dzA2mZ9u5d1JeilZXtu1AbyHKl9QfEPzz5G/m+0xrCT5Pd\n\
+UBYgPNboJqqt+fpA46L2/PbobOzvxfturk+6V2ECb5Q0e7qDch/5If2eYJQNRFt\n\
CdJTMkJvdxM7dl8PrDi499Zl3DUaZ10/quaufw3j+pel2oLszSkrkEAsggdndArG\n\
+U6zNlGKlg+OuvePpUHJNXZPEBMuUTwYJKB9oDYfo8O579UbE81U6WhxkrSx3Mlh\n\
p97lq3A=\n\
-----END CERTIFICATE-----"

static char device_topic_root[64];
static size_t device_topic_root_len;

static void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event);
static StaticSemaphore_t startup_sem_buffer;
static SemaphoreHandle_t startup_sem;

static esp_mqtt_client_handle_t client = NULL;

static void send_resp(const char *suffix, char *msg) {
    assert(msg);
    libiot_mqtt_publish_local(suffix, 2, 1, msg);
    free(msg);
}

void mqtt_send_ping_resp() {
    send_resp(MQTT_TOPIC_INFO("status"), json_build_state_up());
}

void mqtt_send_refresh_resp() {
    send_resp(MQTT_TOPIC_INFO("id"), json_build_system_id());
    send_resp(MQTT_TOPIC_INFO("last_reset"), json_build_last_reset());
}

// `topic_suffix` must be null terminated, but `topic` need not be, and `len` is the length of the latter.
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

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = uri,
        .cert_pem = CERT_AUTHORITY_ENDPOINT,
        .client_cert_pem = cert,
        .client_key_pem = key,
        .username = name,
        .password = pass,

        // Keepalive interval in seconds (this is the time between ping messages exchanged with the
        // broker, i.e. how long it will take for other devices to observe that we are down.)
        .keepalive = 5,

        // If `task_stack` is not positive then `CONFIG_MQTT_TASK_STACK_SIZE` is used by esp-mqtt.
        .task_stack = mqtt_task_stack_size,

        // "Last Will and Testament" status (down) message
        .lwt_topic = lwt_topic,
        .lwt_msg = json_build_state_down(),
        .lwt_qos = 2,
        .lwt_retain = 1,
    };

    startup_sem = xSemaphoreCreateBinaryStatic(&startup_sem_buffer);

    client = esp_mqtt_client_init(&mqtt_cfg);
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