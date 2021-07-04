#ifndef __LIB__LIBIOT_JSON_BUILDER_H
#define __LIB__LIBIOT_JSON_BUILDER_H

#include <cJSON.h>
#include <esp_app_format.h>
#include <esp_log.h>
#include <esp_system.h>

#include "private.h"
#include "reset_info.h"

char *json_build_system_id();
char *json_build_last_reset();

#endif