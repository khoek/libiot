#include "json_builder.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include "reset_info.h"

char *json_build_state_up() {
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

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto build_state_up_fail;
    }

    cJSON *json_state = cJSON_CreateStringReference("up");
    if (!json_state) {
        goto build_state_up_fail;
    }
    cJSON_AddItemToObject(json_root, "state", json_state);

    cJSON *json_wifi = cJSON_CreateObject();
    if (!json_wifi) {
        goto build_state_up_fail;
    }
    cJSON_AddItemToObject(json_root, "wifi", json_wifi);

    cJSON *json_wifi_rssi = cJSON_CreateNumber(ap.rssi);
    if (!json_wifi_rssi) {
        goto build_state_up_fail;
    }
    cJSON_AddItemToObject(json_wifi, "rssi", json_wifi_rssi);

    cJSON *json_ps_type = cJSON_CreateStringReference(ps_type_str);
    if (!json_ps_type) {
        goto build_state_up_fail;
    }
    cJSON_AddItemToObject(json_wifi, "ps_type", json_ps_type);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

build_state_up_fail:
    ESP_LOGE(TAG, "%s: JSON fail", __func__);

    // It is safe to call this with `json_root == NULL`.
    cJSON_Delete(json_root);
    return NULL;
}

char *json_build_state_down() {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto build_state_down_fail;
    }

    cJSON *json_state = cJSON_CreateStringReference("down");
    if (!json_state) {
        goto build_state_down_fail;
    }
    cJSON_AddItemToObject(json_root, "state", json_state);

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

build_state_down_fail:
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

char *json_build_mem_check() {
    bool heap_integrity_sound = heap_caps_check_integrity_all(true);
    if (heap_integrity_sound) {
        ESP_LOGI(TAG, "heap integrity sound");
    } else {
        ESP_LOGE(TAG, "heap integrity unsound!");
    }

    // Obtain all of the allocation information first, so that the JSON structures
    // we are about to allocate do not affect the reported results.
    //
    // Note that there is no locking here, so the reported allocation information
    // for the capabilities may not be altogether consistent because of allocations
    // occuring on another task or core.
    multi_heap_info_t heap_info[sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t)];
    for (size_t i = 0; i < sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t); i++) {
        heap_caps_get_info(&heap_info[i], HEAP_CAPS[i].code);
    }

    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        goto build_state_down_fail;
    }

    cJSON *json_heap_integrity_sound = cJSON_CreateBool(heap_integrity_sound);
    if (!json_heap_integrity_sound) {
        goto build_state_down_fail;
    }
    cJSON_AddItemToObject(json_root, "heap_integrity_sound", json_heap_integrity_sound);

    cJSON *json_heap_capabilities = cJSON_CreateArray();
    if (!json_heap_capabilities) {
        goto build_state_down_fail;
    }
    cJSON_AddItemToObject(json_root, "heap_capabilities", json_heap_capabilities);

    for (size_t i = 0; i < sizeof(HEAP_CAPS) / sizeof(heap_cap_desc_t); i++) {
        multi_heap_info_t info;
        heap_caps_get_info(&info, HEAP_CAPS[i].code);

        cJSON *json_cap = cJSON_CreateObject();
        if (!json_cap) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToArray(json_heap_capabilities, json_cap);

        cJSON *json_name = cJSON_CreateStringReference(HEAP_CAPS[i].name);
        if (!json_name) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "name", json_name);

        cJSON *json_total_free_bytes = cJSON_CreateNumber(info.total_free_bytes);
        if (!json_total_free_bytes) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "total_free_bytes", json_total_free_bytes);

        cJSON *json_total_allocated_bytes = cJSON_CreateNumber(info.total_allocated_bytes);
        if (!json_total_allocated_bytes) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "total_allocated_bytes", json_total_allocated_bytes);

        cJSON *json_largest_free_block = cJSON_CreateNumber(info.largest_free_block);
        if (!json_largest_free_block) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "largest_free_block", json_largest_free_block);

        cJSON *json_minimum_free_bytes = cJSON_CreateNumber(info.minimum_free_bytes);
        if (!json_minimum_free_bytes) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "minimum_free_bytes", json_minimum_free_bytes);

        cJSON *json_allocated_blocks = cJSON_CreateNumber(info.allocated_blocks);
        if (!json_allocated_blocks) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "allocated_blocks", json_allocated_blocks);

        cJSON *json_free_blocks = cJSON_CreateNumber(info.free_blocks);
        if (!json_free_blocks) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "free_blocks", json_free_blocks);

        cJSON *json_total_blocks = cJSON_CreateNumber(info.total_blocks);
        if (!json_total_blocks) {
            goto build_state_down_fail;
        }
        cJSON_AddItemToObject(json_cap, "total_blocks", json_total_blocks);
    }

    char *msg = cJSON_PrintUnformatted(json_root);
    cJSON_Delete(json_root);
    return msg;

build_state_down_fail:
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

cJSON *build_part_type(esp_partition_type_t type, esp_partition_subtype_t subtype) {
    cJSON *json_type = cJSON_CreateObject();
    if (!json_type) {
        goto build_part_type_fail;
    }

    cJSON *json_subtype = cJSON_CreateObject();
    if (!json_subtype) {
        goto build_part_type_fail;
    }
    cJSON_AddItemToObject(json_type, "subtype", json_subtype);

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

                    cJSON *json_ota_num = cJSON_CreateNumber(subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
                    if (!json_ota_num) {
                        goto build_part_type_fail;
                    }
                    cJSON_AddItemToObject(json_subtype, "id", json_ota_num);

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

    cJSON *json_type_name = cJSON_CreateStringReference(type_name_str);
    if (!json_type_name) {
        goto build_part_type_fail;
    }
    cJSON_AddItemToObject(json_type, "name", json_type_name);

    cJSON *json_subtype_name = cJSON_CreateStringReference(subtype_name_str);
    if (!json_subtype_name) {
        goto build_part_type_fail;
    }
    cJSON_AddItemToObject(json_subtype, "name", json_subtype_name);

    return json_type;

build_part_type_fail:
    cJSON_Delete(json_type);
    return NULL;
}

static bool add_partitions_to_array(cJSON *json_partitions_list, esp_partition_iterator_t it) {
    while (it) {
        const esp_partition_t *part = esp_partition_get(it);

        cJSON *json_part = cJSON_CreateObject();
        if (!json_part) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToArray(json_partitions_list, json_part);

        cJSON *json_flash_chip_id = cJSON_CreateNumber(part->flash_chip->chip_id);
        if (!json_flash_chip_id) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "flash_chip_id", json_flash_chip_id);

        cJSON *json_type = build_part_type(part->type, part->subtype);
        if (!json_type) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "type", json_type);

        cJSON *json_address = cJSON_CreateNumber(part->address);
        if (!json_address) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "address", json_address);

        cJSON *json_size = cJSON_CreateNumber(part->size);
        if (!json_size) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "size", json_size);

        cJSON *json_label = cJSON_CreateStringReference(part->label);
        if (!json_label) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "label", json_label);

        cJSON *json_encrypted = cJSON_CreateBool(part->encrypted);
        if (!json_encrypted) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "encrypted", json_encrypted);

        cJSON *json_ota_state;
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(part, &ota_state) == ESP_OK) {
            json_ota_state = cJSON_CreateStringReference(lookup_ota_state(ota_state));
        } else {
            json_ota_state = cJSON_CreateStringReference("not_present");
        }
        if (!json_ota_state) {
            goto add_partitions_to_array_fail;
        }
        cJSON_AddItemToObject(json_part, "ota_state", json_ota_state);

        it = esp_partition_next(it);
    }

    if (it) {
        esp_partition_iterator_release(it);
    }
    return true;

add_partitions_to_array_fail:
    if (it) {
        esp_partition_iterator_release(it);
    }
    return false;
}

static bool add_partition_address_to_object(cJSON *json_partitions, const char *name, const esp_partition_t *part) {
    cJSON *json;
    if (part) {
        json = cJSON_CreateNumber(part->address);
    } else {
        json = cJSON_CreateNull();
    }
    if (!json) {
        goto add_partition_address_to_object_fail;
    }
    cJSON_AddItemToObject(json_partitions, name, json);

    return true;

add_partition_address_to_object_fail:
    return false;
}

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

    cJSON *json_partitions = cJSON_CreateObject();
    if (!json_partitions) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_software, "partitions", json_partitions);

    cJSON *json_partitions_list = cJSON_CreateArray();
    if (!json_partitions_list) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_partitions, "list", json_partitions_list);

    // TODO In the version of esp-idf after v4.3 the flag `ESP_PARTITION_TYPE_ANY`
    // will be introduced, simplifying this.

    if (!add_partitions_to_array(json_partitions_list, esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL))) {
        goto build_id_fail;
    }

    if (!add_partitions_to_array(json_partitions_list, esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL))) {
        goto build_id_fail;
    }

    if (!add_partition_address_to_object(json_partitions, "boot", esp_ota_get_boot_partition())) {
        goto build_id_fail;
    }

    if (!add_partition_address_to_object(json_partitions, "running", esp_ota_get_running_partition())) {
        goto build_id_fail;
    }

    if (!add_partition_address_to_object(json_partitions, "last_invalid", esp_ota_get_last_invalid_partition())) {
        goto build_id_fail;
    }

    if (!add_partition_address_to_object(json_partitions, "next_update", esp_ota_get_next_update_partition(NULL))) {
        goto build_id_fail;
    }

    cJSON *json_partitions_is_rollback_possible = cJSON_CreateBool(esp_ota_check_rollback_is_possible());
    if (!json_partitions_is_rollback_possible) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_partitions, "is_rollback_possible", json_partitions_is_rollback_possible);

    cJSON *json_idf_version = cJSON_CreateObject();
    if (!json_idf_version) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_software, "idf_version", json_idf_version);

    cJSON *json_idf_major = cJSON_CreateNumber((double) ESP_IDF_VERSION_MAJOR);
    if (!json_idf_major) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf_version, "major", json_idf_major);

    cJSON *json_idf_minor = cJSON_CreateNumber((double) ESP_IDF_VERSION_MINOR);
    if (!json_idf_minor) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf_version, "minor", json_idf_minor);

    cJSON *json_idf_patch = cJSON_CreateNumber((double) ESP_IDF_VERSION_PATCH);
    if (!json_idf_patch) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf_version, "patch", json_idf_patch);

    cJSON *json_idf_blob = cJSON_CreateStringReference(IDF_VER);
    if (!json_idf_blob) {
        goto build_id_fail;
    }
    cJSON_AddItemToObject(json_idf_version, "blob", json_idf_blob);

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
