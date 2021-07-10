#include "reset_info.h"

#include <esp_system.h>

static reset_info_t generate_reset_info() {
    esp_reset_reason_t raw = esp_reset_reason();
    switch (raw) {
        // Reset due to power-on event
        case ESP_RST_POWERON: {
            return (reset_info_t){.raw = raw, .reason = "Power on", .exceptional = false};
        }

        // Software reset via esp_restart
        case ESP_RST_SW: {
            return (reset_info_t){.raw = raw, .reason = "Software commanded restart", .exceptional = false};
        }

        // Software reset due to exception/panic
        case ESP_RST_PANIC: {
            return (reset_info_t){.raw = raw, .reason = "Software panic", .exceptional = true};
        }

        // Reset (software or hardware) due to interrupt watchdog
        case ESP_RST_INT_WDT:
        // Reset due to task watchdog
        case ESP_RST_TASK_WDT:
        // Reset due to other watchdogs
        case ESP_RST_WDT: {
            return (reset_info_t){.raw = raw, .reason = "Watchdog violation", .exceptional = true};
        }

        // Brownout reset (software or hardware)
        case ESP_RST_BROWNOUT: {
            return (reset_info_t){.raw = raw, .reason = "Power brownout", .exceptional = true};
        }

        // Reset reason can not be determined
        case ESP_RST_UNKNOWN:
        // Reset by external pin (not applicable for ESP32)
        case ESP_RST_EXT:
        // Reset over SDIO
        case ESP_RST_SDIO:
        // Reset after exiting deep sleep mode
        case ESP_RST_DEEPSLEEP: {
            return (reset_info_t){.raw = raw, .reason = "Impossible cause", .exceptional = true};
        }
    }

    // Unknown
    return (reset_info_t){.raw = raw, .reason = "<Unknown>", .exceptional = true};
}

static bool inited = false;
static reset_info_t reset_info;

reset_info_t *reset_info_get() {
    assert(inited);
    return &reset_info;
}

void reset_info_init() {
    reset_info = generate_reset_info();
    inited = true;
}