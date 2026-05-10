#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"

#include "driver_oled.h"

static const char *TAG = "oled_096_demo";

static void draw_static_header(void)
{
    driver_oled_draw_rect(0, 0, DRIVER_OLED_WIDTH, DRIVER_OLED_HEIGHT, true);
    driver_oled_draw_string(8, 8, "MOCE OLED", true);
    driver_oled_draw_string(8, 22, "0.96 SSD1306", true);
}

static void draw_progress_bar(uint32_t tick)
{
    int width = (int)(tick % 105U);

    driver_oled_draw_rect(11, 49, 106, 8, true);
    driver_oled_fill_rect(12, 50, width, 6, true);
}

static void draw_frame(uint32_t counter)
{
    char text[24];

    driver_oled_clear();
    draw_static_header();

    snprintf(text, sizeof(text), "CNT %lu", (unsigned long)counter);
    driver_oled_draw_string(8, 36, text, true);

    draw_progress_bar(counter * 7U);
}

void app_main(void)
{
    ESP_LOGI(TAG, "start 0.96 inch OLED demo");

    ESP_ERROR_CHECK(driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306));
    ESP_ERROR_CHECK(driver_oled_clear_screen());

    uint32_t counter = 0;

    while (1) {
        draw_frame(counter++);

        esp_err_t err = driver_oled_flush();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "flush failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
