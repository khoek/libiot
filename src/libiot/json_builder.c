#include <cJSON.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "reset_info.h"

char *json_build_system_id() {
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
        goto build_id_fail;
    }

    cJSON *json_software = cJSON_CreateObject();
    if (!json_software) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_root, "software", json_software);

    cJSON *json_app_desc = cJSON_CreateObject();
    if (!json_app_desc) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_software, "app_desc", json_app_desc);

    cJSON *json_app_desc_magic = cJSON_CreateNumber((double) app_desc->magic_word);
    if (!json_app_desc_magic) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "magic_word", json_app_desc_magic);

    cJSON *json_app_desc_secure_version = cJSON_CreateNumber((double) app_desc->secure_version);
    if (!json_app_desc_secure_version) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "secure_version", json_app_desc_secure_version);

    cJSON *json_app_desc_version = cJSON_CreateStringReference(app_desc->version);
    if (!json_app_desc_version) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "version", json_app_desc_version);

    cJSON *json_app_desc_project_name = cJSON_CreateStringReference(app_desc->project_name);
    if (!json_app_desc_project_name) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "project_name", json_app_desc_project_name);

    cJSON *json_app_desc_time = cJSON_CreateStringReference(app_desc->time);
    if (!json_app_desc_time) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "time", json_app_desc_time);

    cJSON *json_app_desc_date = cJSON_CreateStringReference(app_desc->date);
    if (!json_app_desc_date) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "date", json_app_desc_date);

    cJSON *json_app_desc_idf_ver = cJSON_CreateStringReference(app_desc->idf_ver);
    if (!json_app_desc_idf_ver) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "idf_ver", json_app_desc_idf_ver);

    cJSON *json_app_desc_app_elf_sha256 = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_app_desc, "app_elf_sha256", json_app_desc_app_elf_sha256);

    for (int i = 0; i < sizeof(app_desc->app_elf_sha256); i++) {
        cJSON *json_num = cJSON_CreateNumber(app_desc->app_elf_sha256[i]);
        if (!json_num) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_app_desc_app_elf_sha256, json_num);
    }

    cJSON *json_idf = cJSON_CreateObject();
    if (!json_idf) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_software, "idf_version", json_idf);

    cJSON *json_idf_major = cJSON_CreateNumber((double) ESP_IDF_VERSION_MAJOR);
    if (!json_idf_major) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "major", json_idf_major);

    cJSON *json_idf_minor = cJSON_CreateNumber((double) ESP_IDF_VERSION_MINOR);
    if (!json_idf_minor) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "minor", json_idf_minor);

    cJSON *json_idf_patch = cJSON_CreateNumber((double) ESP_IDF_VERSION_PATCH);
    if (!json_idf_patch) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "patch", json_idf_patch);

    cJSON *json_idf_blob = cJSON_CreateStringReference(IDF_VER);
    if (!json_idf_blob) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf, "blob", json_idf_blob);

    cJSON *json_efuse = cJSON_CreateObject();
    if (!json_efuse) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_root, "efuse", json_efuse);

    cJSON *json_efuse_err = cJSON_CreateObject();
    if (!json_efuse_err) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "err", json_efuse_err);

    cJSON *json_efuse_err_mac_default = cJSON_CreateNumber((double) err_mac_default);
    if (!json_efuse_err_mac_default) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_efuse_err, "mac_default", json_efuse_err_mac_default);

#ifdef REPORT_MAC_CUSTOM_BLK3
    cJSON *json_efuse_err_mac_custom = cJSON_CreateNumber((double) err_mac_custom);
    if (!json_efuse_err_mac_custom) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_efuse_err, "mac_custom", json_efuse_err_mac_custom);
#endif

    cJSON *json_mac_default = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "mac_default", json_mac_default);

    for (int i = 0; i < sizeof(mac_default); i++) {
        cJSON *json_num = cJSON_CreateNumber(mac_default[i]);
        if (!json_num) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_mac_default, json_num);
    }

#ifdef REPORT_MAC_CUSTOM_BLK3
    cJSON *json_mac_custom = cJSON_CreateArray();
    if (!json_app_desc_app_elf_sha256) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_efuse, "mac_custom", json_mac_custom);

    for (int i = 0; i < sizeof(mac_custom); i++) {
        cJSON *json_num = cJSON_CreateNumber(mac_custom[i]);
        if (!json_num) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_mac_custom, json_num);
    }
#endif

    cJSON *json_chip = cJSON_CreateObject();
    if (!json_chip) {
        goto build_id_fail;
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
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "model", json_model);

    cJSON *json_features = cJSON_CreateArray();
    if (!json_features) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "features", json_features);

    cJSON *json_feature;

    if (chip_info.features & CHIP_FEATURE_EMB_FLASH) {
        json_feature = cJSON_CreateStringReference("EMB_FLASH");
        if (!json_feature) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
        json_feature = cJSON_CreateStringReference("WIFI_BGN");
        if (!json_feature) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_BLE) {
        json_feature = cJSON_CreateStringReference("BLE");
        if (!json_feature) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    if (chip_info.features & CHIP_FEATURE_BT) {
        json_feature = cJSON_CreateStringReference("BT");
        if (!json_feature) {
            goto build_id_fail;
        }
        cJSON_AddItemToArray(json_features, json_feature);
    }

    cJSON *json_cores = cJSON_CreateNumber((double) chip_info.cores);
    if (!json_cores) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "cores", json_cores);

    cJSON *json_revision = cJSON_CreateNumber((double) chip_info.revision);
    if (!json_revision) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_chip, "revision", json_revision);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

build_id_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

char *json_build_last_reset() {
    reset_info_t *reset_info = reset_info_get();

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto build_last_reset_fail;
    }

    cJSON *json_reason = cJSON_CreateStringReference(reset_info->reason);
    if (!json_reason) {
        goto build_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "reason", json_reason);

    cJSON *json_code = cJSON_CreateNumber(reset_info->raw);
    if (!json_code) {
        goto build_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "code", json_code);

    cJSON *json_exceptional = cJSON_CreateBool(reset_info->exceptional);
    if (!json_exceptional) {
        goto build_last_reset_fail;
    }
    cJSON_AddItemToObject(json_root, "exceptional", json_exceptional);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

build_last_reset_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}