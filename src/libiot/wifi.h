#ifndef LIBIOT_WIFI_H
#define LIBIOT_WIFI_H

#include "private.h"

// Returns when WiFi has connected succesfully.
void wifi_init(const char *ssid, const char *pass, const char *name);
const char *wifi_get_local_ip();

#endif