#ifndef __LIB__LIBIOT_RESET_INFO_H
#define __LIB__LIBIOT_RESET_INFO_H

#include <esp_system.h>

#include "private.h"

typedef struct reset_info {
    esp_reset_reason_t raw;
    const char *reason;
    bool exceptional;
} reset_info_t;

void reset_info_init();
reset_info_t *reset_info_get();

#endif