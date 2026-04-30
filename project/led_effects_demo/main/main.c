#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "service_device.h"
#include "led_effects.h"

static const char *TAG = "led_effects_demo";

void app_main(void)
{
    service_device_init();
    led_effects_init();

    ESP_LOGI(TAG, "led effects demo started");

    while (1) {
        led_effects_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
