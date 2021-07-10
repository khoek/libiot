#ifndef __LIB__LIBIOT_MQTT_H
#define __LIB__LIBIOT_MQTT_H

#include <mqtt_client.h>
#include <stdarg.h>

#include "private.h"

#define MQTT_TOPIC_INFO(literal) "_info/" literal
#define MQTT_TOPIC_CMD(literal) "_cmd/" literal

// If `mqtt_task_stack_size` is not positive then `CONFIG_MQTT_TASK_STACK_SIZE` is used.
//
// Returns when MQTT has connected succesfully.
void mqtt_init(const char *uri, const char *cert, const char *key, const char *name, const char *pass,
               int mqtt_task_stack_size, void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event));

void mqtt_send_ping_resp();
void mqtt_send_refresh_resp();

#endif