#pragma once

//////// Config options

// Disables OTA updates
// #define LIBIOT_DISABLE_OTA

// Enable SPIFFS
// #define LIBIOT_ENABLE_SPIFFS

// Enables the MQTT watchdog
// #define LIBIOT_ENABLE_MQTT_WATCHDOG

////////

// NOTE In practice we require the following in `sdkconfig`:
// * CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ=240
//      (Otherwise TLS sockets can time out and be reset.)
// * CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=8192
//      (Otherwise our certificate chain might be too large to send during SSL
//      auth.)
// * CONFIG_MQTT_BUFFER_SIZE=4096
//      (Otherwise the url and certificate in OTA packets may be too big to
//      recieve.)
// * CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
//      (In order to enable OTA rollback.)

// NOTE We also will likely want:
// * CONFIG_ESP32_RTC_CLOCK_SOURCE_INTERNAL_8MD256=y
//      (In order to improve oscillator stability.)
// * CONFIG_COMPILER_STACK_CHECK_MODE_STRONG=y
//      (Passes `-fstack-protector-strong` to GCC.)

#include <mqtt_client.h>
#include <sys/cdefs.h>

#ifndef IOT_MQTT_ROOT_NAMESPACE
#define IOT_MQTT_ROOT_NAMESPACE "hoek/"
#endif

// e.g. 'hoek/cmd/<cmd>'.
#define IOT_MQTT_COMMAND_TOPIC(cmd_literal) \
    IOT_MQTT_ROOT_NAMESPACE "cmd/" cmd_literal

// e.g. 'hoek/iot/<device_name>'.
#define IOT_MQTT_DEVICE_TOPIC_ROOT(device_name_literal) \
    IOT_MQTT_ROOT_NAMESPACE "iot/" device_name_literal

// e.g. 'hoek/iot/<device_name>/<name>'.
#define IOT_MQTT_DEVICE_TOPIC(device_name_literal, name_literal) \
    IOT_MQTT_DEVICE_TOPIC_ROOT(device_name_literal)              \
    "/" name_literal

typedef struct node_config {
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
    int mqtt_task_stack_size;

    // App init - called before wifi or mqtt has been started. May be NULL.
    void (*app_init)();
    // App entry point - called after wifi and mqtt connected. May be NULL.
    void (*app_run)();
} node_config_t;

void libiot_startup(const node_config_t *cfg);

// Returns NULL if called before `app_run()` has been invoked.
const char *libiot_get_instance_uuid();

// Returns 0 if called before `app_run()` has been invoked.
uint64_t libiot_get_start_epoch_time_ms();

// Send: * an error to the console
//       * an mqtt message to 'hoek/iot/<device_name>/_info/error'.
//
// Note: this function calls `libiot_mqtt_publish()`, and so only returns
// once the message has actually has been sent, or there was a failure.
void libiot_logf_error(const char *tag, const char *format, ...)
    __printflike(2, 3);

const char *libiot_get_local_ip();

/// MQTT Subscribe
/// Just like esp-mqtt, these functions **block** until they complete, or there
/// is a failure.

// Subscribes to '<topic>'
void libiot_mqtt_subscribe(const char *topic, int qos);
// Subscribes to 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_subscribe_local(const char *topic_suffix, int qos);

/// MQTT Publish
/// Just like esp-mqtt, these functions **block** until they complete, or there
/// is a failure.

// Publishes under '<topic>'.
void libiot_mqtt_publish(const char *topic, int qos, int retain,
                         const char *msg);
// Publishes under 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_publish_local(const char *topic_suffix, int qos, int retain,
                               const char *msg);
void libiot_mqtt_publishv_local(const char *topic_suffix, int qos, int retain,
                                const char *fmt, va_list va);
void libiot_mqtt_publishf_local(const char *topic_suffix, int qos, int retain,
                                const char *fmt, ...) __printflike(4, 5);

/// MQTT Enqueue
/// Just like esp-mqtt, these functions **do not block**, simply enqueuing a
/// message to be sent.

// Publishes under '<topic>'
void libiot_mqtt_enqueue(const char *topic, int qos, int retain,
                         const char *msg);
// Publishes under 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_enqueue_local(const char *topic_suffix, int qos, int retain,
                               const char *msg);
void libiot_mqtt_enqueuev_local(const char *topic_suffix, int qos, int retain,
                                const char *fmt, va_list va);
void libiot_mqtt_enqueuef_local(const char *topic_suffix, int qos, int retain,
                                const char *fmt, ...) __printflike(4, 5);

// Convenience method to obtain 'hoek/iot/<device_name>/<topic_suffix>'
void libiot_mqtt_build_local_topic_from_suffix(char *buff, size_t buff_len,
                                               const char *topic_suffix);
