#include "bsp_led.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "board.h"

static const char *TAG = "bsp_led";

static bool s_led_inited = false;
static uint8_t s_brightness_percent = 0;

/* 根据分辨率计算最大 duty */
static uint32_t bsp_led_get_max_duty(void)
{
    return (1U << BOARD_LED_PWM_DUTY_RES) - 1U;
}

static uint32_t bsp_led_percent_to_duty(uint8_t percent)
{
    uint32_t max_duty = bsp_led_get_max_duty();

    if (percent >= 100) {
        return max_duty;
    }

    return (max_duty * percent) / 100U;
}

void bsp_led_init(void)
{
    esp_err_t err;

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BOARD_LED_PWM_MODE,
        .duty_resolution  = BOARD_LED_PWM_DUTY_RES,
        .timer_num        = BOARD_LED_PWM_TIMER,
        .freq_hz          = BOARD_LED_PWM_FREQUENCY_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return;
    }

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = BOARD_LED_GPIO,
        .speed_mode = BOARD_LED_PWM_MODE,
        .channel    = BOARD_LED_PWM_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = BOARD_LED_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        return;
    }

    s_led_inited = true;
    s_brightness_percent = 0;

    ESP_LOGI(TAG, "LED PWM initialized on GPIO %d", BOARD_LED_GPIO);
}

void bsp_led_set_duty(uint32_t duty)
{
    if (!s_led_inited) {
        ESP_LOGW(TAG, "bsp_led_set_duty called before init");
        return;
    }

    uint32_t max_duty = bsp_led_get_max_duty();
    if (duty > max_duty) {
        duty = max_duty;
    }

    esp_err_t err;
    err = ledc_set_duty(BOARD_LED_PWM_MODE, BOARD_LED_PWM_CHANNEL, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: %s", esp_err_to_name(err));
        return;
    }

    err = ledc_update_duty(BOARD_LED_PWM_MODE, BOARD_LED_PWM_CHANNEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed: %s", esp_err_to_name(err));
        return;
    }

    s_brightness_percent = (uint8_t)((duty * 100U) / max_duty);
}

void bsp_led_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    uint32_t duty = bsp_led_percent_to_duty(percent);
    bsp_led_set_duty(duty);
}

uint8_t bsp_led_get_brightness(void)
{
    return s_brightness_percent;
}

void bsp_led_set(int on)
{
    bsp_led_set_brightness(on ? 100 : 0);
}

void bsp_led_toggle(void)
{
    if (s_brightness_percent > 0) {
        bsp_led_set_brightness(0);
    } else {
        bsp_led_set_brightness(100);
    }
}