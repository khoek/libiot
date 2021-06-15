#ifndef __LIB_LIBIOT_H
#define __LIB_LIBIOT_H

#include <mqtt_client.h>

#define IOT_MQTT_ROOT_NAMESPACE "hoek/"

// e.g. is 'hoek/cmd/<cmd>'.
#define IOT_MQTT_COMMAND_TOPIC(cmd_literal) \
    IOT_MQTT_ROOT_NAMESPACE "cmd/" cmd_literal
// e.g. 'hoek/iot/<device_name>/<name>'.
#define IOT_MQTT_DEVICE_TOPIC(device_name_literal, name_literal) \
    IOT_MQTT_ROOT_NAMESPACE "iot/" device_name_literal "/" name_literal

struct node_config {
    const char *name;

    // WiFi
    const char *ssid;
    const char *pass;

    // MQTT
    const char *uri;
    const char *cert;
    const char *key;
    const char *mqtt_pass;
    void (*mqtt_cb)(esp_mqtt_event_handle_t event);

    // App init - called before wifi/mqtt init.
    void (*app_init)();
    // App run - called after wifi/mqtt init.
    void (*app_run)();
};

void libiot_startup(struct node_config *cfg);

#endif