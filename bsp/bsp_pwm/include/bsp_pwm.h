#pragma once

#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer_num;
    ledc_timer_bit_t duty_resolution;
    uint32_t frequency_hz;
} bsp_pwm_timer_config_t;

typedef struct {
    int gpio_num;
    ledc_mode_t speed_mode;
    ledc_channel_t channel;
    ledc_timer_t timer_num;
    uint32_t duty;
} bsp_pwm_channel_config_t;

esp_err_t bsp_pwm_timer_init(const bsp_pwm_timer_config_t *config);
esp_err_t bsp_pwm_channel_init(const bsp_pwm_channel_config_t *config);
esp_err_t bsp_pwm_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty);
uint32_t bsp_pwm_max_duty(ledc_timer_bit_t duty_resolution);

#ifdef __cplusplus
}
#endif
