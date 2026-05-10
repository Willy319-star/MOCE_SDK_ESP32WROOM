#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t pin;
    gpio_mode_t mode;
    bool pull_up;
    bool pull_down;
    gpio_int_type_t intr_type;
} bsp_gpio_config_t;

esp_err_t bsp_gpio_config(const bsp_gpio_config_t *config);
esp_err_t bsp_gpio_set_level(gpio_num_t pin, uint32_t level);
int bsp_gpio_get_level(gpio_num_t pin);
esp_err_t bsp_gpio_reset(gpio_num_t pin);

#ifdef __cplusplus
}
#endif
