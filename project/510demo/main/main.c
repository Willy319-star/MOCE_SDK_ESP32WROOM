#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_oled.h"
#include "driver_mpu6050.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"

#define TAG "510demo"

#define MPU6050_I2C_ADDR            DRIVER_MPU6050_I2C_ADDR_AD0_LOW
#define MPU6050_SCL_SPEED_HZ         400000
#define MPU6050_ACCEL_RANGE          DRIVER_MPU6050_ACCEL_RANGE_4G
#define MPU6050_GYRO_RANGE           DRIVER_MPU6050_GYRO_RANGE_500DPS
#define MPU6050_DLPF                 DRIVER_MPU6050_DLPF_BW_44HZ
#define MPU6050_SAMPLE_RATE_DIVIDER   9

#define TOF_I2C_ADDR                 DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT
#define TOF_SCL_SPEED_HZ             400000
#define TOF_TIMEOUT_MS               50
#define TOF_MEASUREMENT_BUDGET_US    33000
#define TOF_CONTINUOUS_PERIOD_MS     100

#define MPU_TILT_THRESHOLD_DEG       30.0f
#define TOF_OFFSET_MM                30
#define TOF_TRIGGER_MIN_MM           1
#define TOF_TRIGGER_MAX_MM           29

#define LOOP_PERIOD_MS               100

typedef enum {
    APP_STATE_SAFE = 0,
    APP_STATE_ALERT,
} app_state_t;

static app_state_t s_state = APP_STATE_SAFE;
static bool s_mpu_alert = false;
static bool s_tof_alert = false;

static void oled_draw_face(bool alert)
{
    driver_oled_clear();

    int cx = DRIVER_OLED_WIDTH / 2;
    int cy = DRIVER_OLED_HEIGHT / 2 - 4;

    driver_oled_draw_pixel(cx - 12, cy - 8, true);
    driver_oled_draw_pixel(cx + 12, cy - 8, true);
    driver_oled_draw_rect(cx - 18, cy - 16, 36, 32, false);

    if (alert) {
        for (int x = -14; x <= 14; ++x) {
            int y = (int)(8.0f * sqrtf(1.0f - ((float)(x * x) / (14.0f * 14.0f))));
            driver_oled_draw_pixel(cx + x, cy + 8 - y, true);
        }
        driver_oled_draw_string(30, 54, "WARNING", true);
    } else {
        for (int x = -14; x <= 14; ++x) {
            int y = (int)(8.0f * sqrtf(1.0f - ((float)(x * x) / (14.0f * 14.0f))));
            driver_oled_draw_pixel(cx + x, cy + 8 + y, true);
        }
    }

    (void)driver_oled_flush();
}

static void app_show_safe(void)
{
    oled_draw_face(false);
}

static void app_show_alert(void)
{
    oled_draw_face(true);
}

static void app_speak(const char *text)
{
    if (text == NULL) {
        return;
    }
    esp_err_t err = driver_tw_tts_speak(text);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TTS speak failed: %s", esp_err_to_name(err));
    }
}

static bool mpu6050_is_alert(float *out_tilt_deg)
{
    driver_mpu6050_data_t data;
    esp_err_t err = driver_mpu6050_read(&data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 read failed: %s", esp_err_to_name(err));
        return false;
    }

    float ax = data.accel_x_g;
    float ay = data.accel_y_g;
    float az = data.accel_z_g;
    float tilt_x = atan2f(ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI;
    float tilt_y = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / (float)M_PI;
    float tilt = fabsf(tilt_x) > fabsf(tilt_y) ? tilt_x : tilt_y;

    if (out_tilt_deg != NULL) {
        *out_tilt_deg = tilt;
    }
    return fabsf(tilt) > MPU_TILT_THRESHOLD_DEG;
}

static bool tof_is_alert(uint16_t *out_raw_mm)
{
    driver_tof2000c_vl53l0x_result_t result;
    esp_err_t err = driver_tof2000c_vl53l0x_read_continuous(&result);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "VL53L0X read failed: %s", esp_err_to_name(err));
        return false;
    }

    if (out_raw_mm != NULL) {
        *out_raw_mm = result.distance_mm;
    }

    if (result.timeout) {
        return false;
    }

    if (result.distance_mm <= TOF_OFFSET_MM) {
        return false;
    }

    uint16_t calibrated = (uint16_t)(result.distance_mm - TOF_OFFSET_MM);
    return (calibrated >= TOF_TRIGGER_MIN_MM && calibrated <= TOF_TRIGGER_MAX_MM);
}

static void update_state_and_output(bool mpu_alert, bool tof_alert)
{
    app_state_t next_state = (mpu_alert || tof_alert) ? APP_STATE_ALERT : APP_STATE_SAFE;

    if (next_state == s_state) {
        return;
    }

    s_state = next_state;

    if (s_state == APP_STATE_ALERT) {
        app_show_alert();
        if (mpu_alert) {
            app_speak("我要翻了");
        }
        if (tof_alert) {
            app_speak("我要撞了");
        }
    } else {
        app_show_safe();
    }
}

static void init_oled(void)
{
    esp_err_t err = driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(err));
    }
    (void)driver_oled_set_power(true);
    (void)driver_oled_clear_screen();
    app_show_safe();
}

static void init_tts(void)
{
    driver_tw_tts_config_t cfg;
    driver_tw_tts_get_default_config(&cfg);
    cfg.encoding = DRIVER_TW_TTS_ENCODING_UTF8;

    esp_err_t err = driver_tw_tts_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TTS init failed: %s", esp_err_to_name(err));
        return;
    }

    (void)driver_tw_tts_set_volume(1);
    app_speak("你好，莫测上线了");
}

static void init_sensors(void)
{
    driver_mpu6050_config_t mpu_cfg = {
        .i2c_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = MPU6050_SCL_SPEED_HZ,
        .accel_range = MPU6050_ACCEL_RANGE,
        .gyro_range = MPU6050_GYRO_RANGE,
        .dlpf = MPU6050_DLPF,
        .sample_rate_divider = MPU6050_SAMPLE_RATE_DIVIDER,
    };

    driver_tof2000c_vl53l0x_config_t tof_cfg;
    driver_tof2000c_vl53l0x_get_default_config(&tof_cfg);
    tof_cfg.i2c_address = TOF_I2C_ADDR;
    tof_cfg.scl_speed_hz = TOF_SCL_SPEED_HZ;
    tof_cfg.timeout_ms = TOF_TIMEOUT_MS;
    tof_cfg.measurement_timing_budget_us = TOF_MEASUREMENT_BUDGET_US;

    esp_err_t err = driver_mpu6050_init(&mpu_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(err));
    }

    err = driver_tof2000c_vl53l0x_init(&tof_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed: %s", esp_err_to_name(err));
    } else {
        (void)driver_tof2000c_vl53l0x_start_continuous(TOF_CONTINUOUS_PERIOD_MS);
    }
}

void app_main(void)
{
    service_device_init();

    init_oled();
    init_tts();
    init_sensors();

    while (1) {
        bool mpu_alert = false;
        bool tof_alert = false;
        float tilt_deg = 0.0f;
        uint16_t raw_mm = 0;

        mpu_alert = mpu6050_is_alert(&tilt_deg);
        tof_alert = tof_is_alert(&raw_mm);

        if (mpu_alert != s_mpu_alert) {
            s_mpu_alert = mpu_alert;
            if (mpu_alert && s_state == APP_STATE_SAFE) {
                ESP_LOGI(TAG, "MPU alert entered");
            }
        }

        if (tof_alert != s_tof_alert) {
            s_tof_alert = tof_alert;
            if (tof_alert && s_state == APP_STATE_SAFE) {
                ESP_LOGI(TAG, "TOF alert entered");
            }
        }

        update_state_and_output(s_mpu_alert, s_tof_alert);
        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}