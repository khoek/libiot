#ifndef __LIB_LIBIOT_H
#define __LIB_LIBIOT_H

//////// Config options

// Disables OTA updates
// #define LIBIOT_DISABLE_OTA

////////

// NOTE In practice we require the following in `sdkconfig`:
// * CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=240 (Otherwise TLS sockets can time out and be reset.)
// * CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=8192 (Otherwise our certificate chain might be too large to send during SSL auth.)
// * CONFIG_MQTT_BUFFER_SIZE=4096 (Otherwise the certificate in OTA packets may be too big to recieve.)
// * CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y (In order to enable OTA rollback.)

#include <mqtt_client.h>

#ifndef IOT_MQTT_ROOT_NAMESPACE
#define IOT_MQTT_ROOT_NAMESPACE "hoek/"
#endif

// e.g. is 'hoek/cmd/<cmd>'.
#define IOT_MQTT_COMMAND_TOPIC(cmd_literal) \
    IOT_MQTT_ROOT_NAMESPACE "cmd/" cmd_literal

// e.g. 'hoek/iot/<device_name>'.
#define IOT_MQTT_DEVICE_TOPIC_ROOT(device_name_literal) \
    IOT_MQTT_ROOT_NAMESPACE "iot/" device_name_literal

// e.g. 'hoek/iot/<device_name>/<name>'.
#define IOT_MQTT_DEVICE_TOPIC(device_name_literal, name_literal) \
    IOT_MQTT_DEVICE_TOPIC_ROOT(device_name_literal)              \
    "/" name_literal

struct node_config {
    const char *name;

    // WiFi
    const char *ssid;
    const char *pass;
    wifi_ps_type_t ps_type;

    // MQTT
    const char *uri;
    const char *cert;
    const char *key;
    const char *mqtt_pass;
    void (*mqtt_cb)(esp_mqtt_event_handle_t event);

    // Options (not setting these yields reasonable defaults)
    bool enable_spiffs;
    int mqtt_task_stack_size;

    // App init - called before wifi/mqtt init.
    void (*app_init)();
    // App run - called after wifi/mqtt init.
    void (*app_run)();
};

void libiot_startup(struct node_config *cfg);

// Send: * an error to the console
//       * an mqtt message to 'hoek/iot/<device_name>/_info/error'.
void libiot_logf_error(const char *tag, const char *format, ...) __attribute__((format(printf, 2, 3)));

const char *libiot_get_local_ip();

// Subscribes to '<topic>'
void libiot_mqtt_subscribe(const char *topic, int qos);
// Subscribes to 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_subscribe_local(const char *topic_suffix, int qos);

// Publishes under '<topic>'
void libiot_mqtt_publish(const char *topic, int qos, int retain, const char *msg);
// Publishes under 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_publish_local(const char *topic_suffix, int qos, int retain, const char *msg);
void libiot_mqtt_publishv_local(const char *topic_suffix, int qos, int retain, const char *fmt, va_list va);
void libiot_mqtt_publishf_local(const char *topic_suffix, int qos, int retain, const char *fmt, ...) __attribute__((format(printf, 4, 5)));

#endif