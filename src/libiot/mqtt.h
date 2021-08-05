#pragma once

#include <mqtt_client.h>
#include <stdarg.h>

#include "private.h"

#define MQTT_TOPIC_INFO(literal) "_info/" literal
#define MQTT_TOPIC_CMD(literal) "_cmd/" literal

// Called even if mqtt will not be started in order to initialize logging
// structures.
void libiot_init_mqtt(const char *name);

// If `mqtt_task_stack_size` is not positive then `CONFIG_MQTT_TASK_STACK_SIZE`
// is used.
//
// Returns when MQTT has connected succesfully.
void libiot_start_mqtt(
    const char *uri, const char *cert, const char *key, const char *name,
    const char *pass, int mqtt_task_stack_size,
    void (*mqtt_event_handler_cb)(esp_mqtt_event_handle_t event));

void libiot_mqtt_send_ping_resp();
void libiot_mqtt_send_refresh_resp();
void libiot_mqtt_send_mem_check_resp();
