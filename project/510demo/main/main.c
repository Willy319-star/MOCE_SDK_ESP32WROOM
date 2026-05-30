#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "board.h"

static const char *TAG = "510demo";

#define LOOP_DELAY_MS                100
#define MPU_TILT_THRESHOLD_DEG       30.0f
#define TOF_OFFSET_MM                30u
#define TOF_ALERT_MIN_MM             31u
#define TOF_ALERT_MAX_MM             59u

typedef enum {
    APP_STATE_SAFE = 0,
    APP_STATE_ALERT,
} app_state_t;

typedef struct {
    bool tilt_triggered;
    bool tof_triggered;
    float tilt_abs_deg;
    uint16_t raw_distance_mm;
    uint16_t calibrated_distance_mm;
    bool distance_valid;
} sensor_status_t;

static void draw_face(bool alert)
{
    driver_oled_clear();

    int cx = DRIVER_OLED_WIDTH / 2;
    int cy = DRIVER_OLED_HEIGHT / 2 - 2;
    int r = 18;

    driver_oled_draw_rect(cx - r, cy - r, r * 2, r * 2, true);
    driver_oled_draw_pixel(cx - 7, cy - 5, true);
    driver_oled_draw_pixel(cx + 7, cy - 5, true);

    if (!alert) {
        driver_oled_draw_hline(cx - 7, cy + 5, 15, true);
        driver_oled_draw_pixel(cx - 8, cy + 6, true);
        driver_oled_draw_pixel(cx + 8, cy + 6, true);
        driver_oled_draw_pixel(cx - 9, cy + 7, true);
        driver_oled_draw_pixel(cx + 9, cy + 7, true);
    } else {
        driver_oled_draw_hline(cx - 7, cy + 8, 15, true);
        driver_oled_draw_pixel(cx - 8, cy + 7, true);
        driver_oled_draw_pixel(cx + 8, cy + 7, true);
        driver_oled_draw_pixel(cx - 9, cy + 6, true);
        driver_oled_draw_pixel(cx + 9, cy + 6, true);
        driver_oled_draw_string(34, 54, "WARNING", true);
    }

    driver_oled_flush();
}

static float compute_tilt_abs_deg(void)
{
    driver_mpu6050_data_t data;
    if (driver_mpu6050_read(&data) != ESP_OK) {
        return 0.0f;
    }

    float ax = data.accel_x_g;
    float ay = data.accel_y_g;
    float az = data.accel_z_g;

    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm <= 0.001f) {
        return 0.0f;
    }

    ax /= norm;
    ay /= norm;
    az /= norm;

    float tilt_x = atan2f(ax, az) * 180.0f / (float)M_PI;
    float tilt_y = atan2f(ay, az) * 180.0f / (float)M_PI;
    float abs_x = fabsf(tilt_x);
    float abs_y = fabsf(tilt_y);
    return abs_x > abs_y ? abs_x : abs_y;
}

static sensor_status_t read_sensors(void)
{
    sensor_status_t status = {0};

    status.tilt_abs_deg = compute_tilt_abs_deg();
    status.tilt_triggered = status.tilt_abs_deg > MPU_TILT_THRESHOLD_DEG;

    driver_tof2000c_vl53l0x_result_t tof = {0};
    if (driver_tof2000c_vl53l0x_read_continuous(&tof) == ESP_OK && !tof.timeout) {
        status.raw_distance_mm = tof.distance_mm;
        if (status.raw_distance_mm > TOF_OFFSET_MM) {
            status.calibrated_distance_mm = status.raw_distance_mm - TOF_OFFSET_MM;
            status.distance_valid = true;
            status.tof_triggered = (status.calibrated_distance_mm >= TOF_ALERT_MIN_MM &&
                                    status.calibrated_distance_mm <= TOF_ALERT_MAX_MM);
        }
    }

    return status;
}

static void speak_alert_once(bool tilt_triggered, bool tof_triggered)
{
    if (tilt_triggered) {
        ESP_LOGI(TAG, "MPU6050 alert: 我要翻了");
        driver_tw_tts_speak("我要翻了");
    }
    if (tof_triggered) {
        ESP_LOGI(TAG, "VL53L0X alert: 我要撞了");
        driver_tw_tts_speak("我要撞了");
    }
}

void app_main(void)
{
    esp_err_t ret;

    ret = driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = driver_tw_tts_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = driver_tw_tts_set_volume(1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS volume set failed: %s", esp_err_to_name(ret));
    }

    draw_face(false);

    ret = driver_tw_tts_speak("你好，莫测上线了");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS welcome failed: %s", esp_err_to_name(ret));
    }

    ret = driver_mpu6050_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(ret));
    }

    ret = driver_tof2000c_vl53l0x_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed: %s", esp_err_to_name(ret));
    } else {
        ret = driver_tof2000c_vl53l0x_start_continuous(100);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "VL53L0X continuous start failed: %s", esp_err_to_name(ret));
        }
    }

    app_state_t current_state = APP_STATE_SAFE;
    bool tilt_alert_latched = false;
    bool tof_alert_latched = false;

    while (1) {
        sensor_status_t status = read_sensors();

        bool any_alert = status.tilt_triggered || status.tof_triggered;

        if (any_alert && current_state == APP_STATE_SAFE) {
            current_state = APP_STATE_ALERT;
            draw_face(true);

            bool tilt_new = status.tilt_triggered && !tilt_alert_latched;
            bool tof_new = status.tof_triggered && !tof_alert_latched;
            speak_alert_once(tilt_new, tof_new);
        } else if (!any_alert && current_state == APP_STATE_ALERT) {
            current_state = APP_STATE_SAFE;
            draw_face(false);
        }

        tilt_alert_latched = status.tilt_triggered;
        tof_alert_latched = status.tof_triggered;

        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}