#ifndef __LIB__LIBIOT_MQTT_H
#define __LIB__LIBIOT_MQTT_H

#include <mqtt_client.h>

#include "private.h"

// If `mqtt_task_stack_size` is not positive then `CONFIG_MQTT_TASK_STACK_SIZE` is used.
//
// Returns when MQTT has connected succesfully.
void mqtt_init(const char *uri, const char *cert, const char *key, const char *name, const char *pass,
               int mqtt_task_stack_size, void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event));

#endif