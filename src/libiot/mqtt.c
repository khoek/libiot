#include "mqtt.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <mqtt_client.h>

#include "gpio.h"
#include "reset_info.h"

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

#define INFO_TOPIC_PREFIX "_info"

#define TOPIC_SUBSCRIBE_WILDCARD_FMT IOT_MQTT_DEVICE_TOPIC("%s", "#")
#define TOPIC_STATUS_FMT IOT_MQTT_DEVICE_TOPIC("%s", INFO_TOPIC_PREFIX "/status")
#define TOPIC_id_FMT IOT_MQTT_DEVICE_TOPIC("%s", INFO_TOPIC_PREFIX "/id")
#define TOPIC_LAST_RESET_FMT IOT_MQTT_DEVICE_TOPIC("%s", INFO_TOPIC_PREFIX "/last_reset")
#define TOPIC_RESTART_FMT IOT_MQTT_DEVICE_TOPIC("%s", INFO_TOPIC_PREFIX "/restart")

#define STATUS_UP "up"
#define STATUS_DOWN "down"

static char topic_subscribe_wildcard_buff[256];
static char topic_status_buff[256];
static char topic_id_buff[256];
static char topic_last_reset_buff[256];
static char topic_restart_buff[256];
static char *msg_id = NULL;
static char *msg_last_reset = NULL;

static void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event);
static StaticSemaphore_t startup_sem_buffer;
static SemaphoreHandle_t startup_sem;

static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "mqtt event: base='%s', event_id=%d", base, event_id);

    bool did_connect = false;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            // TODO Could this technically fail to arrive? Do something
            // fancy here to periodically check?
            assert(esp_mqtt_client_subscribe(client, topic_subscribe_wildcard_buff, 0) >= 0);
            assert(esp_mqtt_client_publish(client, topic_status_buff, STATUS_UP, 0, 2, 1) >= 0);

            // Publish device hardware information and the last reset reason
            if (msg_id) {
                assert(esp_mqtt_client_publish(client, topic_id_buff, msg_id, 0, 2, 1) >= 0);
            }
            if (msg_last_reset) {
                assert(esp_mqtt_client_publish(client, topic_last_reset_buff, msg_last_reset, 0, 2, 1) >= 0);
            }

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
            if (!strncmp(topic_restart_buff, event->topic, event->topic_len)) {
                // Any message to this topic will trigger a restart of the ESP32.
                ESP_LOGE(TAG, "mqtt restart");
                esp_restart();
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

char *print_msg_id() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    const esp_app_desc_t *app_desc = esp_ota_get_app_description();

    uint8_t mac_default[6];
    memset(mac_default, 0, sizeof(mac_default));
    // Even if this fails, we use the value of the zero-memset'ed arrays.
    esp_err_t err_mac_default = esp_efuse_mac_get_default(mac_default);

#ifdef REPORT_MAC_CUSTOM_BLK3
    uint8_t mac_custom[6];
    memset(mac_custom, 0, sizeof(mac_custom));
    // Even if this fails, we use the value of the zero-memset'ed arrays.
    esp_err_t err_mac_custom = esp_efuse_mac_get_custom(mac_custom);
#endif

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto print_msg_id_fail;
    }

    cJSON *json_software = cJSON_CreateObject();
    if (!json_software) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_root, "software", json_software);

    cJSON *json_app_desc = cJSON_CreateObject();
    if (!json_app_desc) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_software, "app_desc", json_app_desc);

    cJSON *json_app_desc_magic = cJSON_CreateNumber((double) app_desc->magic_word);
    if (!json_app_desc_magic) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "magic_word", json_app_desc_magic);

    cJSON *json_app_desc_secure_version = cJSON_CreateNumber((double) app_desc->secure_version);
    if (!json_app_desc_secure_version) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "secure_version", json_app_desc_secure_version);

    cJSON *json_app_desc_version = cJSON_CreateStringReference(app_desc->version);
    if (!json_app_desc_version) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "version", json_app_desc_version);

    cJSON *json_app_desc_project_name = cJSON_CreateStringReference(app_desc->project_name);
    if (!json_app_desc_project_name) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "project_name", json_app_desc_project_name);

    cJSON *json_app_desc_time = cJSON_CreateStringReference(app_desc->time);
    if (!json_app_desc_time) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "time", json_app_desc_time);

    cJSON *json_app_desc_date = cJSON_CreateStringReference(app_desc->date);
    if (!json_app_desc_date) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "date", json_app_desc_date);

    cJSON *json_app_desc_idf_ver = cJSON_CreateStringReference(app_desc->idf_ver);
    if (!json_app_desc_idf_ver) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "idf_ver", json_app_desc_idf_ver);

    cJSON *json_app_desc_app_elf_sha256 = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "app_elf_sha256", json_app_desc_app_elf_sha256);

    for (int i = 0; i < sizeof(app_desc->app_elf_sha256); i++) {
        cJSON *json_num = cJSON_CreateNumber(app_desc->app_elf_sha256[i]);
        if (!json_num) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_app_desc_app_elf_sha256, json_num);
    }

    cJSON *json_idf = cJSON_CreateObject();
    if (!json_idf) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_software, "idf_version", json_idf);

    cJSON *json_idf_major = cJSON_CreateNumber((double) ESP_IDF_VERSION_MAJOR);
    if (!json_idf_major) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "major", json_idf_major);

    cJSON *json_idf_minor = cJSON_CreateNumber((double) ESP_IDF_VERSION_MINOR);
    if (!json_idf_minor) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "minor", json_idf_minor);

    cJSON *json_idf_patch = cJSON_CreateNumber((double) ESP_IDF_VERSION_PATCH);
    if (!json_idf_patch) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "patch", json_idf_patch);

    cJSON *json_idf_blob = cJSON_CreateStringReference(IDF_VER);
    if (!json_idf_blob) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "blob", json_idf_blob);

    cJSON *json_efuse = cJSON_CreateObject();
    if (!json_efuse) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_root, "efuse", json_efuse);

    cJSON *json_efuse_err = cJSON_CreateObject();
    if (!json_efuse_err) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "err", json_efuse_err);

    cJSON *json_efuse_err_mac_default = cJSON_CreateNumber((double) err_mac_default);
    if (!json_efuse_err_mac_default) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_efuse_err, "mac_default", json_efuse_err_mac_default);

#ifdef REPORT_MAC_CUSTOM_BLK3
    cJSON *json_efuse_err_mac_custom = cJSON_CreateNumber((double) err_mac_custom);
    if (!json_efuse_err_mac_custom) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_efuse_err, "mac_custom", json_efuse_err_mac_custom);
#endif

    cJSON *json_mac_default = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "mac_default", json_mac_default);

    for (int i = 0; i < sizeof(mac_default); i++) {
        cJSON *json_num = cJSON_CreateNumber(mac_default[i]);
        if (!json_num) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_mac_default, json_num);
    }

#ifdef REPORT_MAC_CUSTOM_BLK3
    cJSON *json_mac_custom = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "mac_custom", json_mac_custom);

    for (int i = 0; i < sizeof(mac_custom); i++) {
        cJSON *json_num = cJSON_CreateNumber(mac_custom[i]);
        if (!json_num) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_mac_custom, json_num);
    }
#endif

    cJSON *json_chip = cJSON_CreateObject();
    if (!json_chip) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_root, "chip", json_chip);

    const char *model;
    switch (chip_info.model) {
        case CHIP_ESP32: {
            model = "ESP32";
            break;
        }
        case CHIP_ESP32S2: {
            model = "ESP32-S2";
            break;
        }
        default: {
            model = "Unknown";
            break;
        }
    }
    cJSON *json_model = cJSON_CreateStringReference(model);
    if (!json_model) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "model", json_model);

    cJSON *json_features = cJSON_CreateArray();
    if (!json_features) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "features", json_features);

    cJSON *json_feature;

    if (chip_info.features & CHIP_FEATURE_EMB_FLASH) {
        json_feature = cJSON_CreateStringReference("EMB_FLASH");
        if (!json_feature) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
        json_feature = cJSON_CreateStringReference("WIFI_BGN");
        if (!json_feature) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_BLE) {
        json_feature = cJSON_CreateStringReference("BLE");
        if (!json_feature) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_BT) {
        json_feature = cJSON_CreateStringReference("BT");
        if (!json_feature) {
            goto print_msg_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    cJSON *json_cores = cJSON_CreateNumber((double) chip_info.cores);
    if (!json_cores) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "cores", json_cores);

    cJSON *json_revision = cJSON_CreateNumber((double) chip_info.revision);
    if (!json_revision) {
        goto print_msg_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "revision", json_revision);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

print_msg_id_fail:
    ESP_LOGE(TAG, "print_msg_id(): JSON fail");

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

char *print_msg_last_reset() {
    reset_info_t *reset_info = reset_info_get();

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto print_msg_last_reset_fail;
    }

    cJSON *json_reason = cJSON_CreateStringReference(reset_info->reason);
    if (!json_reason) {
        goto print_msg_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "reason", json_reason);

    cJSON *json_code = cJSON_CreateNumber(reset_info->raw);
    if (!json_code) {
        goto print_msg_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "code", json_code);

    cJSON *json_exceptional = cJSON_CreateBool(reset_info->exceptional);
    if (!json_exceptional) {
        goto print_msg_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "exceptional", json_exceptional);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

print_msg_last_reset_fail:
    ESP_LOGE(TAG, "print_msg_last_reset(): JSON fail");

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

void mqtt_init(const char *uri, const char *cert, const char *key, const char *name, const char *pass,
               int mqtt_task_stack_size, void (*cb)(esp_mqtt_event_handle_t event)) {
    mqtt_event_handler_cb = cb;

    snprintf(topic_subscribe_wildcard_buff, sizeof(topic_subscribe_wildcard_buff), TOPIC_SUBSCRIBE_WILDCARD_FMT, name);
    snprintf(topic_status_buff, sizeof(topic_status_buff), TOPIC_STATUS_FMT, name);
    snprintf(topic_id_buff, sizeof(topic_id_buff), TOPIC_id_FMT, name);
    snprintf(topic_last_reset_buff, sizeof(topic_last_reset_buff), TOPIC_LAST_RESET_FMT, name);
    snprintf(topic_restart_buff, sizeof(topic_restart_buff), TOPIC_RESTART_FMT, name);

    msg_id = print_msg_id();
    msg_last_reset = print_msg_last_reset();

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
        .lwt_topic = topic_status_buff,
        .lwt_msg = STATUS_DOWN,
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

esp_mqtt_client_handle_t mqtt_get_client() {
    return client;
}