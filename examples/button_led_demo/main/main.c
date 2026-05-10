#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "service_device.h"
#include "driver_led.h"
#include "driver_button.h"

static const char *TAG = "button_led_demo";

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK_1HZ,
    LED_MODE_BLINK_2HZ,
    LED_MODE_BLINK_5HZ,
    LED_MODE_MAX
} led_mode_t;

static led_mode_t s_led_mode = LED_MODE_OFF;
static bool s_led_on = false;
static TickType_t s_last_toggle_tick = 0;

static const char *led_mode_to_string(led_mode_t mode)
{
    switch (mode) {
    case LED_MODE_OFF:
        return "OFF";
    case LED_MODE_ON:
        return "ON";
    case LED_MODE_BLINK_1HZ:
        return "BLINK_1HZ";
    case LED_MODE_BLINK_2HZ:
        return "BLINK_2HZ";
    case LED_MODE_BLINK_5HZ:
        return "BLINK_5HZ";
    default:
        return "UNKNOWN";
    }
}

static void led_mode_apply_immediate(led_mode_t mode)
{
    switch (mode) {
    case LED_MODE_OFF:
        driver_led_set(0);
        s_led_on = false;
        break;

    case LED_MODE_ON:
        driver_led_set(1);
        s_led_on = true;
        break;

    case LED_MODE_BLINK_1HZ:
    case LED_MODE_BLINK_2HZ:
    case LED_MODE_BLINK_5HZ:
        s_led_on = false;
        driver_led_set(0);
        s_last_toggle_tick = xTaskGetTickCount();
        break;

    default:
        break;
    }
}

static void led_mode_next(void)
{
    s_led_mode = (led_mode_t)((s_led_mode + 1) % LED_MODE_MAX);
    ESP_LOGI(TAG, "switch mode -> %s", led_mode_to_string(s_led_mode));
    led_mode_apply_immediate(s_led_mode);
}

static void led_mode_process(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t interval_ticks = 0;

    switch (s_led_mode) {
    case LED_MODE_OFF:
    case LED_MODE_ON:
        return;

    case LED_MODE_BLINK_1HZ:
        /* 整体周期 1s，亮灭各 500ms */
        interval_ticks = pdMS_TO_TICKS(500);
        break;

    case LED_MODE_BLINK_2HZ:
        /* 整体周期 0.5s，亮灭各 250ms */
        interval_ticks = pdMS_TO_TICKS(250);
        break;

    case LED_MODE_BLINK_5HZ:
        /* 整体周期 0.2s，亮灭各 100ms */
        interval_ticks = pdMS_TO_TICKS(100);
        break;

    default:
        return;
    }

    if ((now - s_last_toggle_tick) >= interval_ticks) {
        s_last_toggle_tick = now;
        s_led_on = !s_led_on;
        driver_led_set(s_led_on ? 1 : 0);
    }
}

static void button_process(void)
{
    driver_button_process();

    driver_button_event_t ev = driver_button_get_event();
    if (ev == DRIVER_BUTTON_EVENT_SHORT_PRESS) {
        led_mode_next();
    }
}

void app_main(void)
{
    service_device_init();

    /* 默认上电进入长暗模式 */
    s_led_mode = LED_MODE_OFF;
    led_mode_apply_immediate(s_led_mode);

    ESP_LOGI(TAG, "button led demo started");
    ESP_LOGI(TAG, "initial mode -> %s", led_mode_to_string(s_led_mode));

    while (1) {
        button_process();
        led_mode_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}