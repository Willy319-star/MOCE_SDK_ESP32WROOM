#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "service_device.h"
#include "led_app.h"

static const char *TAG = "button_brightness_modes";

void app_main(void)
{
    service_device_init();
    led_app_init();

    ESP_LOGI(TAG, "short press: switch mode, long press: change brightness");

    while (1) {
        led_app_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
