#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "service_device.h"
#include "led_effects.h"

static const char *TAG = "led_effects_demo";

static void led_effects_demo_run_once(void)
{
    ESP_LOGI(TAG, "effect: %s", led_effects_mode_name(LED_EFFECT_SOLID_ON));
    led_effects_apply_solid(1, 2000);

    ESP_LOGI(TAG, "effect: %s", led_effects_mode_name(LED_EFFECT_SOLID_OFF));
    led_effects_apply_solid(0, 2000);

    ESP_LOGI(TAG, "effect: %s 1Hz", led_effects_mode_name(LED_EFFECT_BLINK));
    led_effects_blink(LED_BLINK_FREQ_1HZ, 4000);

    ESP_LOGI(TAG, "effect: %s 2Hz", led_effects_mode_name(LED_EFFECT_BLINK));
    led_effects_blink(LED_BLINK_FREQ_2HZ, 4000);

    ESP_LOGI(TAG, "effect: %s 5Hz", led_effects_mode_name(LED_EFFECT_BLINK));
    led_effects_blink(LED_BLINK_FREQ_5HZ, 4000);

    ESP_LOGI(TAG, "effect: %s", led_effects_mode_name(LED_EFFECT_BREATH));
    led_effects_breath(8000);
}

void app_main(void)
{
    service_device_init();
    ESP_LOGI(TAG, "LED effects demo started");

    while (1) {
        led_effects_demo_run_once();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
