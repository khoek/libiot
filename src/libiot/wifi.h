#ifndef __LIB__LIBIOT_WIFI_H
#define __LIB__LIBIOT_WIFI_H

#include "private.h"

// Returns when WiFi has connected succesfully.
void wifi_init(const char *ssid, const char *pass, const char *name);

#endif