#include "ota.h"

#include <cJSON.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <libiot.h>

#include "mqtt.h"

#define RECV_TIMEOUT_MS 5000
#define TASK_STACK_DEPTH 8192
#define QUEUE_LENGTH 16

static StaticQueue_t ota_cmd_queue_static;
static uint8_t ota_cmd_queue_buff[QUEUE_LENGTH * sizeof(char *)];
static QueueHandle_t ota_cmd_queue;

#define MILESTONE_BYTES 100000

static bool perform_update(const char *url, const char *ca_cert_pem) {
    ESP_LOGI(TAG, "ota: start (%s)", url);
    libiot_mqtt_publishf_local(MQTT_TOPIC_INFO("ota"), 2, 0, "{\"state\":\"start\"}");

    esp_err_t err;
    const char *fail_msg = NULL;

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = (char *) ca_cert_pem,
        .timeout_ms = RECV_TIMEOUT_MS,
        .buffer_size_tx = 1024,  // Note: needed for chunky google cloud signed URLs
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        // FIXME do we want this?
        // .partial_http_download = true,
        // .max_http_request_size = CONFIG_EXAMPLE_HTTP_REQUEST_SIZE,
    };

    esp_https_ota_handle_t handle;
    err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        fail_msg = "begin failed";
        goto ota_end_skip_abort;
    }

    int32_t last_milestone_count = -1;
    while (1) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // Note `esp_https_ota_perform` returns after every read operation, and needs to be called again
        // until it does not return `ESP_ERR_HTTPS_OTA_IN_PROGRESS`.

        uint32_t byte_count = esp_https_ota_get_image_len_read(handle);
        int32_t milestone_count = byte_count / MILESTONE_BYTES;
        if (milestone_count > last_milestone_count) {
            uint32_t kb_count = byte_count / 1000;
            ESP_LOGI(TAG, "ota: read %d kB", kb_count);
            libiot_mqtt_publishf_local(MQTT_TOPIC_INFO("ota"), 2, 0, "{\"state\":\"in_progress\", \"rx_kb\":%d}", kb_count);
            last_milestone_count = milestone_count;
        }
    }

    if (esp_https_ota_is_complete_data_received(handle) != true) {
        fail_msg = "complete data was not received";
        goto ota_end;
    }

    if (err != ESP_OK) {
        fail_msg = "perform failed";
        goto ota_end;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            fail_msg = "image validation failed, image is corrupted";
        } else {
            fail_msg = "finish failed";
        }

        goto ota_end_skip_abort;
    }

    ESP_LOGI(TAG, "ota: upgrade successful");
    libiot_mqtt_publishf_local(MQTT_TOPIC_INFO("ota"), 2, 0, "{\"state\":\"done\"}");
    return true;

ota_end:
    esp_https_ota_abort(handle);

ota_end_skip_abort:
    libiot_logf_error(TAG, "ota: %s (0x%X)", fail_msg ? fail_msg : "???", err);
    libiot_mqtt_publishf_local(MQTT_TOPIC_INFO("ota"), 2, 0, "{\"state\":\"fail\"}");
    return false;
}

static const char *process_cmd_update(const cJSON *json_root) {
    const cJSON *json_url = cJSON_GetObjectItemCaseSensitive(json_root, "url");
    if (!json_url || !cJSON_IsString(json_url)) {
        return "update: no `url` or not a string!";
    }

    const cJSON *json_ca_cert = cJSON_GetObjectItemCaseSensitive(json_root, "ca_cert");
    if (!json_ca_cert || !cJSON_IsString(json_ca_cert)) {
        return "update: no `ca_cert` or not a string!";
    }

    perform_update(json_url->valuestring, json_ca_cert->valuestring);

    return NULL;
}

static const char *process_cmd_validate(const cJSON *json_root) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        libiot_logf_error(TAG, "ota: validate failed with error code (0x%X)", err);
    }

    return NULL;
}

static const char *process_cmd_rollback(const cJSON *json_root) {
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (err != ESP_OK) {
        libiot_logf_error(TAG, "ota: rollback failed with error code (0x%X)", err);
    }

    return NULL;
}

static void process_cmd(const char *manifest_json) {
    ESP_LOGI(TAG, "ota: reading cmd (%d bytes)", strlen(manifest_json));

    const char *fail_msg = NULL;

    cJSON *json_root = cJSON_Parse(manifest_json);
    if (!json_root) {
        fail_msg = "JSON parse error";
        goto process_cmd_out;
    }

    const cJSON *json_type = cJSON_GetObjectItemCaseSensitive(json_root, "type");
    if (!json_type || !cJSON_IsString(json_type)) {
        fail_msg = "no `type` or not a string!";
        goto process_cmd_out;
    }

    if (!strcmp(json_type->valuestring, "update")) {
        fail_msg = process_cmd_update(json_root);
    } else if (!strcmp(json_type->valuestring, "validate")) {
        fail_msg = process_cmd_validate(json_root);
    } else if (!strcmp(json_type->valuestring, "rollback")) {
        fail_msg = process_cmd_rollback(json_root);
    } else {
        fail_msg = "unknown cmd `type`";
    }

process_cmd_out:
    if (fail_msg) {
        libiot_logf_error(TAG, "ota: %s", fail_msg);
    }

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
}

static void task_run(void *unused) {
    while (1) {
        char *cmd_json;
        while (xQueueReceive(ota_cmd_queue, &cmd_json, portMAX_DELAY) == pdFALSE)
            ;

        process_cmd(cmd_json);
        free(cmd_json);

        // Refresh the published partition states
        mqtt_send_refresh_resp();
    }
}

void ota_dispatch_request(char *cmd_json) {
    if (!ota_cmd_queue) {
        libiot_logf_error(TAG, "ota: dispatch request with ota not initialized!");
        return;
    }

    if (xQueueSend(ota_cmd_queue, &cmd_json, 0) != pdTRUE) {
        libiot_logf_error(TAG, "ota: can't queue manifest");
    }
}

esp_err_t ota_init() {
    ota_cmd_queue = xQueueCreateStatic(QUEUE_LENGTH, sizeof(char *), ota_cmd_queue_buff, &ota_cmd_queue_static);

    if (xTaskCreate(task_run, "ota_task", TASK_STACK_DEPTH, NULL, 5, NULL) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}