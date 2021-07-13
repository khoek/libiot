#ifndef __LIB__LIBIOT_OTA_H
#define __LIB__LIBIOT_OTA_H

#include "private.h"

void ota_dispatch_request(char *manifest_json);
esp_err_t ota_init();

#endif
