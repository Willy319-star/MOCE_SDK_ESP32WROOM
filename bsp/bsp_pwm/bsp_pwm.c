#include "bsp_pwm.h"

esp_err_t bsp_pwm_timer_init(const bsp_pwm_timer_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t timer_config = {
        .speed_mode = config->speed_mode,
        .duty_resolution = config->duty_resolution,
        .timer_num = config->timer_num,
        .freq_hz = config->frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    return ledc_timer_config(&timer_config);
}

esp_err_t bsp_pwm_channel_init(const bsp_pwm_channel_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ledc_channel_config_t channel_config = {
        .gpio_num = config->gpio_num,
        .speed_mode = config->speed_mode,
        .channel = config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->timer_num,
        .duty = config->duty,
        .hpoint = 0,
    };

    return ledc_channel_config(&channel_config);
}

esp_err_t bsp_pwm_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty)
{
    esp_err_t err = ledc_set_duty(speed_mode, channel, duty);
    if (err != ESP_OK) {
        return err;
    }

    return ledc_update_duty(speed_mode, channel);
}

uint32_t bsp_pwm_max_duty(ledc_timer_bit_t duty_resolution)
{
    return (1U << duty_resolution) - 1U;
}
