#pragma once

#include "private.h"

esp_err_t libiot_init_ota();
void libiot_ota_dispatch_request(char *manifest_json);
