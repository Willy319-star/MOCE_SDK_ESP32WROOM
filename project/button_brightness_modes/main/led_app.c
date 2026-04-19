#include "led_app.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_button.h"
#include "bsp_led.h"

#define LED_APP_SLOW_BLINK_HALF_PERIOD_MS  500U
#define LED_APP_FAST_BLINK_HALF_PERIOD_MS  100U
#define LED_APP_BREATH_STEP_MS             25U
#define LED_APP_BREATH_STEP_PERCENT        2U

static const char *TAG = "button_brightness";

typedef struct {
    led_app_mode_t mode;
    led_app_brightness_t brightness;
    bool blink_on;
    uint8_t breath_value;
    int8_t breath_delta;
    TickType_t last_update_tick;
} led_app_state_t;

static led_app_state_t s_app;

static const uint8_t s_brightness_percent[LED_APP_BRIGHTNESS_MAX] = {
    [LED_APP_BRIGHTNESS_20] = 20,
    [LED_APP_BRIGHTNESS_50] = 50,
    [LED_APP_BRIGHTNESS_100] = 100,
};

static const char *led_app_mode_name(led_app_mode_t mode)
{
    switch (mode) {
    case LED_APP_MODE_SOLID_ON:
        return "SOLID_ON";
    case LED_APP_MODE_SLOW_BLINK:
        return "SLOW_BLINK";
    case LED_APP_MODE_FAST_BLINK:
        return "FAST_BLINK";
    case LED_APP_MODE_BREATH:
        return "BREATH";
    default:
        return "UNKNOWN";
    }
}

static uint8_t led_app_current_brightness_percent(void)
{
    if (s_app.brightness >= LED_APP_BRIGHTNESS_MAX) {
        return 50;
    }

    return s_brightness_percent[s_app.brightness];
}

static void led_app_set_output(bool on)
{
    bsp_led_set_brightness(on ? led_app_current_brightness_percent() : 0);
}

static void led_app_reset_timing(void)
{
    s_app.last_update_tick = xTaskGetTickCount();
}

static void led_app_apply_mode_immediate(void)
{
    s_app.blink_on = false;
    s_app.breath_value = 0;
    s_app.breath_delta = LED_APP_BREATH_STEP_PERCENT;
    led_app_reset_timing();

    switch (s_app.mode) {
    case LED_APP_MODE_SOLID_ON:
        bsp_led_set_brightness(led_app_current_brightness_percent());
        break;
    case LED_APP_MODE_SLOW_BLINK:
    case LED_APP_MODE_FAST_BLINK:
    case LED_APP_MODE_BREATH:
        bsp_led_set_brightness(0);
        break;
    default:
        break;
    }
}

static void led_app_next_mode(void)
{
    s_app.mode = (led_app_mode_t)((s_app.mode + 1) % LED_APP_MODE_MAX);
    led_app_apply_mode_immediate();
    ESP_LOGI(TAG, "mode -> %s, brightness -> %u%%",
             led_app_mode_name(s_app.mode),
             led_app_current_brightness_percent());
}

static void led_app_next_brightness(void)
{
    s_app.brightness = (led_app_brightness_t)((s_app.brightness + 1) % LED_APP_BRIGHTNESS_MAX);

    if (s_app.mode == LED_APP_MODE_SOLID_ON || s_app.blink_on) {
        bsp_led_set_brightness(led_app_current_brightness_percent());
    }

    ESP_LOGI(TAG, "brightness -> %u%%, mode -> %s",
             led_app_current_brightness_percent(),
             led_app_mode_name(s_app.mode));
}

static void led_app_process_button(void)
{
    bsp_button_process();

    bsp_button_event_t event = bsp_button_get_event();
    switch (event) {
    case BSP_BUTTON_EVENT_SHORT_PRESS:
        led_app_next_mode();
        break;
    case BSP_BUTTON_EVENT_LONG_PRESS:
        led_app_next_brightness();
        break;
    default:
        break;
    }
}

static void led_app_process_blink(uint32_t half_period_ms)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t half_period_ticks = pdMS_TO_TICKS(half_period_ms);

    if ((now - s_app.last_update_tick) < half_period_ticks) {
        return;
    }

    s_app.last_update_tick = now;
    s_app.blink_on = !s_app.blink_on;
    led_app_set_output(s_app.blink_on);
}

static void led_app_process_breath(void)
{
    TickType_t now = xTaskGetTickCount();
    uint8_t max_brightness = led_app_current_brightness_percent();

    if ((now - s_app.last_update_tick) < pdMS_TO_TICKS(LED_APP_BREATH_STEP_MS)) {
        return;
    }

    s_app.last_update_tick = now;

    int next = (int)s_app.breath_value + s_app.breath_delta;
    if (next >= max_brightness) {
        next = max_brightness;
        s_app.breath_delta = -(int8_t)LED_APP_BREATH_STEP_PERCENT;
    } else if (next <= 0) {
        next = 0;
        s_app.breath_delta = LED_APP_BREATH_STEP_PERCENT;
    }

    s_app.breath_value = (uint8_t)next;
    bsp_led_set_brightness(s_app.breath_value);
}

static void led_app_process_led(void)
{
    switch (s_app.mode) {
    case LED_APP_MODE_SOLID_ON:
        break;
    case LED_APP_MODE_SLOW_BLINK:
        led_app_process_blink(LED_APP_SLOW_BLINK_HALF_PERIOD_MS);
        break;
    case LED_APP_MODE_FAST_BLINK:
        led_app_process_blink(LED_APP_FAST_BLINK_HALF_PERIOD_MS);
        break;
    case LED_APP_MODE_BREATH:
        led_app_process_breath();
        break;
    default:
        break;
    }
}

void led_app_init(void)
{
    bsp_button_init();

    s_app.mode = LED_APP_MODE_SOLID_ON;
    s_app.brightness = LED_APP_BRIGHTNESS_50;
    led_app_apply_mode_immediate();

    ESP_LOGI(TAG, "initial mode -> %s, brightness -> %u%%",
             led_app_mode_name(s_app.mode),
             led_app_current_brightness_percent());
}

void led_app_process(void)
{
    led_app_process_button();
    led_app_process_led();
}
