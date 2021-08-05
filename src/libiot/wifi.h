#pragma once

#include "private.h"

// Returns when WiFi has connected succesfully.
void libiot_start_wifi(const char *ssid, const char *pass, const char *name,
                       wifi_ps_type_t ps_type);
