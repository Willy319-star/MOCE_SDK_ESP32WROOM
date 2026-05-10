#include "driver_led.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp_pwm.h"
#include "esp_err.h"
#include "esp_log.h"
#include "board.h"

static const char *TAG = "driver_led";

static bool s_led_inited = false;
static uint8_t s_brightness_percent = 0;

/* 根据分辨率计算最大 duty */
static uint32_t driver_led_get_max_duty(void)
{
    return bsp_pwm_max_duty(BOARD_LED_PWM_DUTY_RES);
}

static uint32_t driver_led_percent_to_duty(uint8_t percent)
{
    uint32_t max_duty = driver_led_get_max_duty();

    if (percent >= 100) {
        return max_duty;
    }

    return (max_duty * percent) / 100U;
}

void driver_led_init(void)
{
    esp_err_t err;

    bsp_pwm_timer_config_t pwm_timer = {
        .speed_mode       = BOARD_LED_PWM_MODE,
        .duty_resolution  = BOARD_LED_PWM_DUTY_RES,
        .timer_num        = BOARD_LED_PWM_TIMER,
        .frequency_hz     = BOARD_LED_PWM_FREQUENCY_HZ,
    };
    err = bsp_pwm_timer_init(&pwm_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pwm timer init failed: %s", esp_err_to_name(err));
        return;
    }

    bsp_pwm_channel_config_t pwm_channel = {
        .gpio_num   = BOARD_LED_GPIO,
        .speed_mode = BOARD_LED_PWM_MODE,
        .channel    = BOARD_LED_PWM_CHANNEL,
        .timer_num  = BOARD_LED_PWM_TIMER,
        .duty       = 0,
    };
    err = bsp_pwm_channel_init(&pwm_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pwm channel init failed: %s", esp_err_to_name(err));
        return;
    }

    s_led_inited = true;
    s_brightness_percent = 0;

    ESP_LOGI(TAG, "LED PWM initialized on GPIO %d", BOARD_LED_GPIO);
}

void driver_led_set_duty(uint32_t duty)
{
    if (!s_led_inited) {
        ESP_LOGW(TAG, "driver_led_set_duty called before init");
        return;
    }

    uint32_t max_duty = driver_led_get_max_duty();
    if (duty > max_duty) {
        duty = max_duty;
    }

    esp_err_t err;
    err = bsp_pwm_set_duty(BOARD_LED_PWM_MODE, BOARD_LED_PWM_CHANNEL, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pwm set duty failed: %s", esp_err_to_name(err));
        return;
    }

    s_brightness_percent = (uint8_t)((duty * 100U) / max_duty);
}

void driver_led_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    uint32_t duty = driver_led_percent_to_duty(percent);
    driver_led_set_duty(duty);
}

uint8_t driver_led_get_brightness(void)
{
    return s_brightness_percent;
}

void driver_led_set(int on)
{
    driver_led_set_brightness(on ? 100 : 0);
}

void driver_led_toggle(void)
{
    if (s_brightness_percent > 0) {
        driver_led_set_brightness(0);
    } else {
        driver_led_set_brightness(100);
    }
}
