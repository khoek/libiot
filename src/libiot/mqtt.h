#ifndef LIBIOT_MQTT_H
#define LIBIOT_MQTT_H

#include <mqtt_client.h>

#include "private.h"

// Returns when MQTT has connected succesfully.
void mqtt_init(const char *uri, const char *cert, const char *key, const char *name, const char *pass,
               void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event));

#endif