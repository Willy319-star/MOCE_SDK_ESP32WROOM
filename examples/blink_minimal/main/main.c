#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "service_device.h"
#include "driver_led.h"

static const char *TAG = "blink_minimal";

void app_main(void)
{
    service_device_init();

    while (1) {
        for (int i = 0; i <= 100; i += 10) {
            ESP_LOGI(TAG, "brightness = %d%%", i);
            driver_led_set_brightness((uint8_t)i);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        for (int i = 100; i >= 0; i -= 10) {
            ESP_LOGI(TAG, "brightness = %d%%", i);
            driver_led_set_brightness((uint8_t)i);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
}