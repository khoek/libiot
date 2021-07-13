#include "gpio.h"

#include <driver/gpio.h>

#define LED_PIN GPIO_NUM_13

void gpio_led_set_state(bool enabled) {
    gpio_set_level(LED_PIN, enabled ? 1 : 0);
}

void gpio_init() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_led_set_state(false);
}
