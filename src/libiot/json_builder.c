#include "json_builder.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <libesp/json.h>

#include "reset_info.h"

char *libiot_json_build_state_up() {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);

    wifi_ps_type_t ps_type;
    esp_wifi_get_ps(&ps_type);

    const char *ps_type_str = "?";
    switch (ps_type) {
        case WIFI_PS_NONE: {
            ps_type_str = "none";
            break;
        }
        case WIFI_PS_MIN_MODEM: {
            ps_type_str = "min_modem";
            break;
        }
        case WIFI_PS_MAX_MODEM: {
            ps_type_str = "max_modem";
            break;
        }
    }

    cJSON *json_root;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_root, json_fail);
    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_root, "state", "up",
                                            json_fail);

    cJSON *json_wifi;
    cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_root, "wifi", &json_wifi, json_fail);
    cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_wifi, "rssi", ap.rssi, json_fail);
    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_wifi, "ps_type", ps_type_str,
                                            json_fail);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

json_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

char *libiot_json_build_state_down() {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);

    cJSON *json_root;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_root, json_fail);
    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_root, "state", "down",
                                            json_fail);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

json_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

typedef struct heap_cap_desc {
    const char *name;
    uint32_t code;
} heap_cap_desc_t;

// Note that "any" includes all heaps, and that the other capabilities
// are not mutually exclusive either.
const heap_cap_desc_t HEAP_CAPS[] = {
    {.name = "any", .code = 0},
    {.name = "exec", .code = MALLOC_CAP_EXEC},
    {.name = "8bit", .code = MALLOC_CAP_8BIT},
    /*
    NOTE: The following are unused in the current version of ESP-IDF.
    {.name = "pid2", .code = MALLOC_CAP_PID2},
    {.name = "pid3", .code = MALLOC_CAP_PID3},
    {.name = "pid4", .code = MALLOC_CAP_PID4},
    {.name = "pid5", .code = MALLOC_CAP_PID5},
    {.name = "pid6", .code = MALLOC_CAP_PID6},
    {.name = "pid7", .code = MALLOC_CAP_PID7},
    */
    {.name = "spi_ram", .code = MALLOC_CAP_SPIRAM},
    {.name = "internal", .code = MALLOC_CAP_INTERNAL},
    {.name = "default", .code = MALLOC_CAP_DEFAULT},
    {.name = "iram_8bit", .code = MALLOC_CAP_IRAM_8BIT},
    {.name = "retention", .code = MALLOC_CAP_RETENTION},
    /*
    NOTE: Not a real heap capability.
    {.name = "invalid", .code = MALLOC_CAP_INVALID}
    */
};

char *libiot_json_build_mem_check() {
    bool heap_integrity_sound = heap_caps_check_integrity_all(true);
    if (heap_integrity_sound) {
        ESP_LOGI(TAG, "heap integrity sound");
    } else {
        ESP_LOGE(TAG, "heap integrity unsound!");
    }

    // Obtain all of the allocation information first, so that the JSON
    // structures we are about to allocate do not affect the reported results.
    //
    // Note that there is no locking here, so the reported allocation
    // information for the capabilities may not be altogether consistent because
    // of allocations occuring on another task or core.
    multi_heap_info_t heap_info[sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t)];
    for (size_t i = 0; i < sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t); i++) {
        heap_caps_get_info(&heap_info[i], HEAP_CAPS[i].code);
    }

    cJSON *json_root;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_root, json_fail);
    cJSON_INSERT_BOOL_INTO_OBJ_OR_GOTO(json_root, "heap_integrity_sound",
                                       heap_integrity_sound, json_fail);

    cJSON *json_heap_capabilities;
    cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_root, "heap_capabilities",
                                        &json_heap_capabilities, json_fail);

    for (size_t i = 0; i < sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t); i++) {
        multi_heap_info_t info;
        heap_caps_get_info(&info, HEAP_CAPS[i].code);

        cJSON *json_cap;
        cJSON_INSERT_OBJ_INTO_ARRAY_OR_GOTO(json_heap_capabilities, &json_cap,
                                            json_fail);

        cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_cap, "name",
                                                HEAP_CAPS[i].name, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "total_free_bytes",
                                             info.total_free_bytes, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "total_allocated_bytes",
                                             info.total_allocated_bytes,
                                             json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "largest_free_block",
                                             info.largest_free_block,
                                             json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "minimum_free_bytes",
                                             info.minimum_free_bytes,
                                             json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "allocated_blocks",
                                             info.allocated_blocks, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "free_blocks",
                                             info.free_blocks, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_cap, "total_blocks",
                                             info.total_blocks, json_fail);
    }

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

json_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

const char *lookup_ota_state(esp_ota_img_states_t state) {
    // Note that "not_present" is also possible at the call site of
    // this function, if the OTA library reports no data recorded.
    switch (state) {
        case ESP_OTA_IMG_NEW: {
            return "new";
        }
        case ESP_OTA_IMG_PENDING_VERIFY: {
            return "pending_verify";
        }
        case ESP_OTA_IMG_VALID: {
            return "valid";
        }
        case ESP_OTA_IMG_INVALID: {
            return "invalid";
        }
        case ESP_OTA_IMG_ABORTED: {
            return "aborted";
        }
        case ESP_OTA_IMG_UNDEFINED: {
            return "undefined";
        }
        default: {
            return "?";
        }
    }
}

cJSON *build_part_type(esp_partition_type_t type,
                       esp_partition_subtype_t subtype) {
    cJSON *json_type;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_type, json_fail);

    cJSON *json_subtype;
    cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_type, "subtype", &json_subtype,
                                      json_fail);

    const char *type_name_str = "?";
    const char *subtype_name_str = "?";

    switch (type) {
        case ESP_PARTITION_TYPE_APP: {
            type_name_str = "app";

            switch (subtype) {
                case ESP_PARTITION_SUBTYPE_APP_FACTORY: {
                    subtype_name_str = "factory";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_APP_TEST: {
                    subtype_name_str = "test";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_APP_OTA_0:
                case ESP_PARTITION_SUBTYPE_APP_OTA_1:
                case ESP_PARTITION_SUBTYPE_APP_OTA_2:
                case ESP_PARTITION_SUBTYPE_APP_OTA_3:
                case ESP_PARTITION_SUBTYPE_APP_OTA_4:
                case ESP_PARTITION_SUBTYPE_APP_OTA_5:
                case ESP_PARTITION_SUBTYPE_APP_OTA_6:
                case ESP_PARTITION_SUBTYPE_APP_OTA_7:
                case ESP_PARTITION_SUBTYPE_APP_OTA_8:
                case ESP_PARTITION_SUBTYPE_APP_OTA_9:
                case ESP_PARTITION_SUBTYPE_APP_OTA_10:
                case ESP_PARTITION_SUBTYPE_APP_OTA_11:
                case ESP_PARTITION_SUBTYPE_APP_OTA_12:
                case ESP_PARTITION_SUBTYPE_APP_OTA_13:
                case ESP_PARTITION_SUBTYPE_APP_OTA_14:
                case ESP_PARTITION_SUBTYPE_APP_OTA_15: {
                    subtype_name_str = "ota";

                    cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(
                        json_subtype, "id",
                        subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN, json_fail);

                    break;
                }
                default: {
                    break;
                }
            }

            break;
        }
        case ESP_PARTITION_TYPE_DATA: {
            type_name_str = "data";

            switch (subtype) {
                case ESP_PARTITION_SUBTYPE_DATA_OTA: {
                    subtype_name_str = "ota";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_PHY: {
                    subtype_name_str = "phy";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_NVS: {
                    subtype_name_str = "nvs";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: {
                    subtype_name_str = "core_dump";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: {
                    subtype_name_str = "nvs_keys";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: {
                    subtype_name_str = "efuse_em";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD: {
                    subtype_name_str = "esphttpd";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_FAT: {
                    subtype_name_str = "fat";
                    break;
                }
                case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: {
                    subtype_name_str = "spiffs";
                    break;
                }
                default: {
                    break;
                }
            }

            break;
        }
        default: {
            break;
        }
    }

    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_type, "name", type_name_str,
                                            json_fail);
    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_subtype, "name",
                                            subtype_name_str, json_fail);

    return json_type;

json_fail:
    cJSON_Delete(json_type);
    return NULL;
}

static bool add_partitions_to_array(cJSON *json_list,
                                    esp_partition_iterator_t it) {
    while (it) {
        const esp_partition_t *part = esp_partition_get(it);

        cJSON *json_part;
        cJSON_INSERT_OBJ_INTO_ARRAY_OR_GOTO(json_list, &json_part, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_part, "flash_chip_id",
                                             part->flash_chip->chip_id,
                                             json_fail);

        cJSON *json_type = build_part_type(part->type, part->subtype);
        if (!json_type) {
            goto json_fail;
        }
        cJSON_AddItemToObject(json_part, "type", json_type);

        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_part, "address",
                                             part->address, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_part, "size", part->size,
                                             json_fail);
        cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_part, "label", part->label,
                                                json_fail);
        cJSON_INSERT_BOOL_INTO_OBJ_OR_GOTO(json_part, "encrypted",
                                           part->encrypted, json_fail);

        const char *ota_state_name = "not_present";
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(part, &ota_state) == ESP_OK) {
            ota_state_name = lookup_ota_state(ota_state);
        }
        cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_part, "ota_state",
                                                ota_state_name, json_fail);

        it = esp_partition_next(it);
    }

    if (it) {
        esp_partition_iterator_release(it);
    }
    return true;

json_fail:
    if (it) {
        esp_partition_iterator_release(it);
    }
    return false;
}

static bool add_partition_address_to_object(cJSON *json_partitions,
                                            const char *name,
                                            const esp_partition_t *part) {
    if (part) {
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_partitions, name,
                                             part->address, json_fail);
    } else {
        cJSON_INSERT_NULL_INTO_OBJ_OR_GOTO(json_partitions, name, json_fail);
    }

    return true;

json_fail:
    return false;
}

char *libiot_json_build_system_id() {
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

    const char *chip_model = "Unknown";
    switch (chip_info.model) {
        case CHIP_ESP32: {
            chip_model = "ESP32";
            break;
        }
        case CHIP_ESP32S2: {
            chip_model = "ESP32-S2";
            break;
        }
        case CHIP_ESP32S3: {
            chip_model = "ESP32-S3";
            break;
        }
        case CHIP_ESP32C3: {
            chip_model = "ESP32-C3";
            break;
        }
        default: {
            break;
        }
    }

    cJSON *json_root;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_root, json_fail);

    {
        cJSON *json_software;
        cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_root, "software", &json_software,
                                          json_fail);

        {
            cJSON *json_app_desc;
            cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_software, "app_desc",
                                              &json_app_desc, json_fail);
            cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_app_desc, "magic_word",
                                                 app_desc->magic_word,
                                                 json_fail);
            cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_app_desc,
                                                 "secure_version",
                                                 app_desc->secure_version,
                                                 json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_app_desc, "version",
                                                    app_desc->version,
                                                    json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_app_desc,
                                                    "project_name",
                                                    app_desc->project_name,
                                                    json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_app_desc, "time",
                                                    app_desc->time, json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_app_desc, "date",
                                                    app_desc->date, json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_app_desc, "idf_ver",
                                                    app_desc->idf_ver,
                                                    json_fail);

            cJSON *json_app_elf_sha256;
            cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_app_desc, "app_elf_sha256",
                                                &json_app_elf_sha256,
                                                json_fail);
            for (int i = 0; i < sizeof(app_desc->app_elf_sha256); i++) {
                cJSON_INSERT_NUMBER_INTO_ARRAY_OR_GOTO(
                    json_app_elf_sha256, app_desc->app_elf_sha256[i],
                    json_fail);
            }
        }

        {
            cJSON *json_partitions;
            cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_software, "partitions",
                                              &json_partitions, json_fail);
            cJSON_INSERT_BOOL_INTO_OBJ_OR_GOTO(
                json_partitions, "is_rollback_possible",
                esp_ota_check_rollback_is_possible(), json_fail);

            if (!add_partition_address_to_object(
                    json_partitions, "boot", esp_ota_get_boot_partition())) {
                goto json_fail;
            }

            if (!add_partition_address_to_object(
                    json_partitions, "running",
                    esp_ota_get_running_partition())) {
                goto json_fail;
            }

            if (!add_partition_address_to_object(
                    json_partitions, "last_invalid",
                    esp_ota_get_last_invalid_partition())) {
                goto json_fail;
            }

            if (!add_partition_address_to_object(
                    json_partitions, "next_update",
                    esp_ota_get_next_update_partition(NULL))) {
                goto json_fail;
            }

            cJSON *json_list;
            cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_partitions, "list",
                                                &json_list, json_fail);

            // TODO In the version of esp-idf after v4.3 the flag
            // `ESP_PARTITION_TYPE_ANY`
            //      will be introduced, simplifying this.

            if (!add_partitions_to_array(
                    json_list,
                    esp_partition_find(ESP_PARTITION_TYPE_APP,
                                       ESP_PARTITION_SUBTYPE_ANY, NULL))) {
                goto json_fail;
            }

            if (!add_partitions_to_array(
                    json_list,
                    esp_partition_find(ESP_PARTITION_TYPE_DATA,
                                       ESP_PARTITION_SUBTYPE_ANY, NULL))) {
                goto json_fail;
            }
        }

        {
            cJSON *json_idf_version;
            cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_software, "idf_version",
                                              &json_idf_version, json_fail);
            cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_idf_version, "major",
                                                 ESP_IDF_VERSION_MAJOR,
                                                 json_fail);
            cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_idf_version, "minor",
                                                 ESP_IDF_VERSION_MINOR,
                                                 json_fail);
            cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_idf_version, "patch",
                                                 ESP_IDF_VERSION_PATCH,
                                                 json_fail);
            cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_idf_version, "blob",
                                                    IDF_VER, json_fail);
        }
    }

    {
        cJSON *json_efuse;
        cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_root, "efuse", &json_efuse,
                                          json_fail);

        cJSON *json_err;
        cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_efuse, "err", &json_err,
                                          json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_err, "mac_default",
                                             err_mac_default, json_fail);
#ifdef REPORT_MAC_CUSTOM_BLK3
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_err, "mac_custom",
                                             err_mac_custom, json_fail);
#endif

        cJSON *json_mac_default;
        cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_efuse, "mac_default",
                                            &json_mac_default, json_fail);
        for (size_t i = 0; i < sizeof(mac_default); i++) {
            cJSON_INSERT_NUMBER_INTO_ARRAY_OR_GOTO(json_mac_default,
                                                   mac_default[i], json_fail);
        }

#ifdef REPORT_MAC_CUSTOM_BLK3
        cJSON *json_mac_custom;
        cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_efuse, "mac_custom",
                                            &json_mac_custom, json_fail);
        for (size_t i = 0; i < sizeof(mac_custom); i++) {
            cJSON_INSERT_NUMBER_INTO_ARRAY_OR_GOTO(json_err, mac_custom[i],
                                                   json_fail);
        }
#endif
    }

    {
        cJSON *json_chip;
        cJSON_INSERT_OBJ_INTO_OBJ_OR_GOTO(json_root, "chip", &json_chip,
                                          json_fail);
        cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_chip, "model", chip_model,
                                                json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_chip, "cores",
                                             chip_info.cores, json_fail);
        cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_chip, "revision",
                                             chip_info.revision, json_fail);

        cJSON *json_features;
        cJSON_INSERT_ARRAY_INTO_OBJ_OR_GOTO(json_chip, "features",
                                            &json_features, json_fail);

        if (chip_info.features & CHIP_FEATURE_EMB_FLASH) {
            cJSON_INSERT_STRINGREF_INTO_ARRAY_OR_GOTO(json_features,
                                                      "EMB_FLASH", json_fail);
        }

        if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
            cJSON_INSERT_STRINGREF_INTO_ARRAY_OR_GOTO(json_features, "WIFI_BGN",
                                                      json_fail);
        }

        if (chip_info.features & CHIP_FEATURE_BLE) {
            cJSON_INSERT_STRINGREF_INTO_ARRAY_OR_GOTO(json_features, "BLE",
                                                      json_fail);
        }

        if (chip_info.features & CHIP_FEATURE_BT) {
            cJSON_INSERT_STRINGREF_INTO_ARRAY_OR_GOTO(json_features, "BT",
                                                      json_fail);
        }
    }

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

json_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

char *libiot_json_build_last_reset() {
    reset_info_t *reset_info = libiot_reset_info_get();

    cJSON *json_root;
    cJSON_CREATE_ROOT_OBJ_OR_GOTO(&json_root, json_fail);
    cJSON_INSERT_STRINGREF_INTO_OBJ_OR_GOTO(json_root, "reason",
                                            reset_info->reason, json_fail);
    cJSON_INSERT_NUMBER_INTO_OBJ_OR_GOTO(json_root, "code", reset_info->raw,
                                         json_fail);
    cJSON_INSERT_BOOL_INTO_OBJ_OR_GOTO(json_root, "exceptional",
                                       reset_info->exceptional, json_fail);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

json_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}
