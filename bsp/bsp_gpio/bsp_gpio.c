#include "bsp_gpio.h"

esp_err_t bsp_gpio_config(const bsp_gpio_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << config->pin),
        .mode = config->mode,
        .pull_up_en = config->pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = config->intr_type,
    };

    return gpio_config(&io_config);
}

esp_err_t bsp_gpio_set_level(gpio_num_t pin, uint32_t level)
{
    return gpio_set_level(pin, level);
}

int bsp_gpio_get_level(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

esp_err_t bsp_gpio_reset(gpio_num_t pin)
{
    return gpio_reset_pin(pin);
}
