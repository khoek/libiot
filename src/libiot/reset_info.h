#pragma once

#include <esp_system.h>

#include "private.h"

typedef struct reset_info {
    esp_reset_reason_t raw;
    const char *reason;
    bool exceptional;
} reset_info_t;

void libiot_init_reset_info();
reset_info_t *libiot_reset_info_get();
