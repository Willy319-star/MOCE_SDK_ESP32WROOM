#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "driver_oled.h"
#include "driver_mpu6050.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"

#define TAG "510demo"

#define I2C_SCL_SPEED_HZ                 (400000)
#define OLED_I2C_ADDR                    DRIVER_OLED_DEFAULT_I2C_ADDR
#define MPU6050_I2C_ADDR                 DRIVER_MPU6050_I2C_ADDR_AD0_LOW
#define VL53L0X_I2C_ADDR                 DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT

#define ALERT_TILT_THRESHOLD_DEG         (30.0f)
#define ALERT_DISTANCE_MIN_MM            (31)
#define ALERT_DISTANCE_MAX_MM            (59)
#define VL53L0X_PHYSICAL_OFFSET_MM       (30)

#define STARTUP_TTS_TEXT                 "你好，莫测上线了"
#define ALERT_TILT_TTS_TEXT              "我要翻了"
#define ALERT_DISTANCE_TTS_TEXT          "我要撞了"

#define POLL_PERIOD_MS                   (100)

typedef enum {
    APP_STATE_SAFE = 0,
    APP_STATE_ALERT,
} app_state_t;

typedef struct {
    bool tilt_alert;
    bool distance_alert;
    bool any_alert;
    float tilt_deg;
    uint16_t raw_distance_mm;
    int calibrated_distance_mm;
} sensor_status_t;

static app_state_t s_app_state = APP_STATE_SAFE;

static void oled_draw_face(bool alert)
{
    driver_oled_clear();

    const int cx = 64;
    const int cy = 32;
    const int r = 18;

    driver_oled_draw_rect(cx - r, cy - r, r * 2, r * 2, true);
    driver_oled_draw_pixel(cx - 6, cy - 4, true);
    driver_oled_draw_pixel(cx + 6, cy - 4, true);

    if (alert) {
        driver_oled_draw_hline(cx - 8, cy + 8, 16, true);
        driver_oled_draw_vline(cx - 8, cy + 8, 9, true);
        driver_oled_draw_vline(cx + 8, cy + 8, 9, true);
    } else {
        driver_oled_draw_hline(cx - 8, cy + 6, 16, true);
        driver_oled_draw_vline(cx - 8, cy, 7, true);
        driver_oled_draw_vline(cx + 8, cy, 7, true);
    }

    if (alert) {
        driver_oled_draw_string(34, 54, "WARNING", true);
    }

    driver_oled_flush();
}

static void app_show_state(app_state_t state)
{
    if (state == APP_STATE_ALERT) {
        oled_draw_face(true);
    } else {
        oled_draw_face(false);
    }
}

static void sensors_init_after_startup_tts(void)
{
    driver_mpu6050_config_t mpu_cfg = {
        .i2c_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = I2C_SCL_SPEED_HZ,
        .accel_range = DRIVER_MPU6050_ACCEL_RANGE_4G,
        .gyro_range = DRIVER_MPU6050_GYRO_RANGE_500DPS,
        .dlpf = DRIVER_MPU6050_DLPF_BW_44HZ,
        .sample_rate_divider = 9,
    };

    driver_tof2000c_vl53l0x_config_t tof_cfg;
    driver_tof2000c_vl53l0x_get_default_config(&tof_cfg);
    tof_cfg.i2c_address = VL53L0X_I2C_ADDR;
    tof_cfg.scl_speed_hz = I2C_SCL_SPEED_HZ;
    tof_cfg.timeout_ms = 50;
    tof_cfg.measurement_timing_budget_us = 33000;

    esp_err_t err = driver_mpu6050_init(&mpu_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(err));
    }

    err = driver_tof2000c_vl53l0x_init(&tof_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed: %s", esp_err_to_name(err));
    } else {
        driver_tof2000c_vl53l0x_start_continuous(0);
    }
}

static sensor_status_t read_sensor_status(void)
{
    sensor_status_t status = {0};

    driver_mpu6050_data_t mpu_data;
    if (driver_mpu6050_read(&mpu_data) == ESP_OK) {
        float ax = mpu_data.accel_x_g;
        float ay = mpu_data.accel_y_g;
        float az = mpu_data.accel_z_g;

        float denom = sqrtf(ax * ax + ay * ay + az * az);
        if (denom > 0.001f) {
            float tilt_rad = acosf(fmaxf(fminf(fabsf(az) / denom, 1.0f), 0.0f));
            status.tilt_deg = tilt_rad * 180.0f / (float)M_PI;
            status.tilt_alert = (status.tilt_deg > ALERT_TILT_THRESHOLD_DEG);
        }
    }

    driver_tof2000c_vl53l0x_result_t tof_res;
    if (driver_tof2000c_vl53l0x_read_continuous(&tof_res) == ESP_OK && !tof_res.timeout) {
        status.raw_distance_mm = tof_res.distance_mm;
        if (status.raw_distance_mm > VL53L0X_PHYSICAL_OFFSET_MM) {
            status.calibrated_distance_mm = (int)status.raw_distance_mm - VL53L0X_PHYSICAL_OFFSET_MM;
            status.distance_alert = (status.calibrated_distance_mm > 0 &&
                                     status.calibrated_distance_mm >= ALERT_DISTANCE_MIN_MM &&
                                     status.calibrated_distance_mm <= ALERT_DISTANCE_MAX_MM);
        }
    }

    status.any_alert = status.tilt_alert || status.distance_alert;
    return status;
}

static void alert_announce_once(const sensor_status_t *status)
{
    if (status->tilt_alert) {
        driver_tw_tts_speak(ALERT_TILT_TTS_TEXT);
    }
    if (status->distance_alert) {
        driver_tw_tts_speak(ALERT_DISTANCE_TTS_TEXT);
    }
}

static void app_update_state(const sensor_status_t *status)
{
    app_state_t next_state = status->any_alert ? APP_STATE_ALERT : APP_STATE_SAFE;
    if (next_state == s_app_state) {
        return;
    }

    s_app_state = next_state;
    app_show_state(s_app_state);

    if (s_app_state == APP_STATE_ALERT) {
        alert_announce_once(status);
    }
}

void app_main(void)
{
    esp_err_t err = driver_oled_init(&(driver_oled_config_t) {
        .controller = DRIVER_OLED_CONTROLLER_SSD1306,
        .i2c_address = OLED_I2C_ADDR,
        .scl_speed_hz = I2C_SCL_SPEED_HZ,
        .flip_x = false,
        .flip_y = false,
        .invert = false,
        .contrast = 0x7f,
    });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(err));
    }

    err = driver_tw_tts_init_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TW-TTS init failed: %s", esp_err_to_name(err));
    }
    (void)driver_tw_tts_set_volume(1);

    app_show_state(APP_STATE_SAFE);
    (void)driver_tw_tts_speak(STARTUP_TTS_TEXT);

    sensors_init_after_startup_tts();

    while (true) {
        sensor_status_t status = read_sensor_status();

        static bool last_tilt_alert = false;
        static bool last_distance_alert = false;

        if (status.tilt_alert && !last_tilt_alert && s_app_state == APP_STATE_ALERT) {
            driver_tw_tts_speak(ALERT_TILT_TTS_TEXT);
        }
        if (status.distance_alert && !last_distance_alert && s_app_state == APP_STATE_ALERT) {
            driver_tw_tts_speak(ALERT_DISTANCE_TTS_TEXT);
        }

        last_tilt_alert = status.tilt_alert;
        last_distance_alert = status.distance_alert;

        app_update_state(&status);
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}