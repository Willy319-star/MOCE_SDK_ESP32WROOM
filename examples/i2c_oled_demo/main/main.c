#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"

#include "driver_oled.h"

static const char *TAG = "i2c_oled_demo";

#ifndef CONFIG_OLED_DEMO_PROFILE_13
#define OLED_DEMO_PROFILE DRIVER_OLED_PROFILE_096_SSD1306
#else
#define OLED_DEMO_PROFILE DRIVER_OLED_PROFILE_13_SH1106
#endif

static int triangle_wave(uint32_t tick, uint32_t period, int amplitude)
{
    uint32_t phase = tick % period;
    uint32_t half = period / 2;
    int value = phase < half ? (int)phase : (int)(period - phase);
    return (value * amplitude * 2) / (int)half - amplitude;
}

static void draw_filled_ellipse(int cx, int cy, int rx, int ry, bool on)
{
    for (int y = -ry; y <= ry; y++) {
        for (int x = -rx; x <= rx; x++) {
            if ((x * x * ry * ry) + (y * y * rx * rx) <= (rx * rx * ry * ry)) {
                driver_oled_draw_pixel(cx + x, cy + y, on);
            }
        }
    }
}

static void draw_eye(int cx, int cy, int blink_height, int pupil_dx)
{
    if (blink_height <= 2) {
        driver_oled_draw_hline(cx - 18, cy, 37, true);
        driver_oled_draw_hline(cx - 14, cy + 1, 29, true);
        return;
    }

    draw_filled_ellipse(cx, cy, 22, blink_height, true);
    draw_filled_ellipse(cx + pupil_dx, cy + 1, 7, blink_height > 10 ? 8 : 4, false);
    driver_oled_fill_rect(cx + pupil_dx - 3, cy - 4, 3, 3, true);
}

static void draw_demo_frame(uint32_t counter)
{
    uint32_t motion_phase = counter % 96;
    int head_dx = 0;
    int head_dy = 0;
    if (motion_phase < 48) {
        head_dx = triangle_wave(motion_phase, 48, 8);
    } else {
        head_dy = triangle_wave(motion_phase - 48, 48, 5);
    }

    int pupil_dx = triangle_wave(counter + 9, 48, 4);
    int blink_phase = counter % 36;
    int eye_height = 15;
    if (blink_phase >= 28) {
        eye_height = 15 - (int)(blink_phase - 27) * 4;
    }
    if (eye_height < 1) {
        eye_height = 1;
    }

    driver_oled_clear();
    draw_eye(38 + head_dx, 32 + head_dy, eye_height, pupil_dx);
    draw_eye(90 + head_dx, 32 + head_dy, eye_height, pupil_dx);
}

void app_main(void)
{
    ESP_LOGI(TAG, "start oled demo");
    ESP_ERROR_CHECK(driver_oled_init_profile(OLED_DEMO_PROFILE));
    ESP_ERROR_CHECK(driver_oled_clear_screen());

    uint32_t counter = 0;
    while (1) {
        draw_demo_frame(counter++);
        esp_err_t err = driver_oled_flush();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "flush failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(70));
    }
}
