#ifndef __LIB__LIBIOT_WIFI_H
#define __LIB__LIBIOT_WIFI_H

#include "private.h"

// Returns when WiFi has connected succesfully.
void wifi_init(const char *ssid, const char *pass, const char *name, wifi_ps_type_t ps_type);

#endif