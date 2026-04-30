#include "bsp_servo.h"

#include <stdbool.h>
#include <stddef.h>

#include "board.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "bsp_servo";

static const int s_servo_gpios[BOARD_SERVO_COUNT] = {
    BOARD_SERVO_GPIO_0,
    BOARD_SERVO_GPIO_1,
};

static const ledc_channel_t s_servo_channels[BOARD_SERVO_COUNT] = {
    BOARD_SERVO_PWM_CHANNEL_0,
    BOARD_SERVO_PWM_CHANNEL_1,
};

static bool s_servo_inited = false;
static uint16_t s_servo_angle[BOARD_SERVO_COUNT] = {0};
static uint16_t s_servo_pulse_us[BOARD_SERVO_COUNT] = {0};

static bool servo_id_is_valid(bsp_servo_id_t servo)
{
    return servo >= BSP_SERVO_0 && servo < BOARD_SERVO_COUNT;
}

static uint32_t servo_max_duty(void)
{
    return (1U << BOARD_SERVO_PWM_DUTY_RES) - 1U;
}

static uint32_t servo_period_us(void)
{
    return 1000000U / BOARD_SERVO_PWM_FREQUENCY_HZ;
}

static uint32_t servo_pulse_to_duty(uint16_t pulse_us)
{
    uint64_t duty = (uint64_t)pulse_us * servo_max_duty();
    return (uint32_t)((duty + (servo_period_us() / 2U)) / servo_period_us());
}

static uint16_t servo_angle_to_pulse(uint16_t angle_deg)
{
    if (angle_deg > BOARD_SERVO_MAX_ANGLE_DEG) {
        angle_deg = BOARD_SERVO_MAX_ANGLE_DEG;
    }

    uint32_t angle_span = BOARD_SERVO_MAX_ANGLE_DEG - BOARD_SERVO_MIN_ANGLE_DEG;
    uint32_t pulse_span = BOARD_SERVO_MAX_PULSE_US - BOARD_SERVO_MIN_PULSE_US;
    uint32_t scaled_angle = angle_deg - BOARD_SERVO_MIN_ANGLE_DEG;

    return (uint16_t)(BOARD_SERVO_MIN_PULSE_US +
                      ((scaled_angle * pulse_span) + (angle_span / 2U)) / angle_span);
}

esp_err_t bsp_servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = BOARD_SERVO_PWM_MODE,
        .duty_resolution = BOARD_SERVO_PWM_DUTY_RES,
        .timer_num = BOARD_SERVO_PWM_TIMER,
        .freq_hz = BOARD_SERVO_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return err;
    }

    for (size_t i = 0; i < BOARD_SERVO_COUNT; i++) {
        ledc_channel_config_t channel_config = {
            .gpio_num = s_servo_gpios[i],
            .speed_mode = BOARD_SERVO_PWM_MODE,
            .channel = s_servo_channels[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = BOARD_SERVO_PWM_TIMER,
            .duty = servo_pulse_to_duty(BOARD_SERVO_CENTER_PULSE_US),
            .hpoint = 0,
        };

        err = ledc_channel_config(&channel_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config servo %u failed: %s", (unsigned)i, esp_err_to_name(err));
            return err;
        }

        s_servo_angle[i] = (BOARD_SERVO_MIN_ANGLE_DEG + BOARD_SERVO_MAX_ANGLE_DEG) / 2U;
        s_servo_pulse_us[i] = BOARD_SERVO_CENTER_PULSE_US;
    }

    s_servo_inited = true;
    ESP_LOGI(TAG, "servo PWM initialized: GPIO %d/%d", s_servo_gpios[0], s_servo_gpios[1]);

    return ESP_OK;
}

esp_err_t bsp_servo_set_pulse_us(bsp_servo_id_t servo, uint16_t pulse_us)
{
    if (!s_servo_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!servo_id_is_valid(servo)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (pulse_us < BOARD_SERVO_MIN_PULSE_US) {
        pulse_us = BOARD_SERVO_MIN_PULSE_US;
    }
    if (pulse_us > BOARD_SERVO_MAX_PULSE_US) {
        pulse_us = BOARD_SERVO_MAX_PULSE_US;
    }

    esp_err_t err = ledc_set_duty(BOARD_SERVO_PWM_MODE, s_servo_channels[servo], servo_pulse_to_duty(pulse_us));
    if (err != ESP_OK) {
        return err;
    }

    err = ledc_update_duty(BOARD_SERVO_PWM_MODE, s_servo_channels[servo]);
    if (err != ESP_OK) {
        return err;
    }

    s_servo_pulse_us[servo] = pulse_us;

    uint32_t pulse_span = BOARD_SERVO_MAX_PULSE_US - BOARD_SERVO_MIN_PULSE_US;
    uint32_t angle_span = BOARD_SERVO_MAX_ANGLE_DEG - BOARD_SERVO_MIN_ANGLE_DEG;
    s_servo_angle[servo] = BOARD_SERVO_MIN_ANGLE_DEG +
                           (((pulse_us - BOARD_SERVO_MIN_PULSE_US) * angle_span) + (pulse_span / 2U)) / pulse_span;

    return ESP_OK;
}

esp_err_t bsp_servo_set_angle(bsp_servo_id_t servo, uint16_t angle_deg)
{
    if (!servo_id_is_valid(servo)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (angle_deg > BOARD_SERVO_MAX_ANGLE_DEG) {
        angle_deg = BOARD_SERVO_MAX_ANGLE_DEG;
    }

    esp_err_t err = bsp_servo_set_pulse_us(servo, servo_angle_to_pulse(angle_deg));
    if (err == ESP_OK) {
        s_servo_angle[servo] = angle_deg;
    }

    return err;
}

uint16_t bsp_servo_get_angle(bsp_servo_id_t servo)
{
    if (!servo_id_is_valid(servo)) {
        return 0;
    }

    return s_servo_angle[servo];
}

uint16_t bsp_servo_get_pulse_us(bsp_servo_id_t servo)
{
    if (!servo_id_is_valid(servo)) {
        return 0;
    }

    return s_servo_pulse_us[servo];
}
