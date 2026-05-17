#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"

static const char *TAG = "510demo";

#define LOOP_PERIOD_MS                 100
#define SAFE_TILT_THRESHOLD_DEG         30.0f
#define TOF_ALERT_MAX_CALIB_MM          29
#define TOF_CALIBRATION_OFFSET_MM       30

typedef enum {
    APP_STATE_SAFE = 0,
    APP_STATE_ALERT,
} app_state_t;

typedef enum {
    TRIGGER_NONE = 0,
    TRIGGER_TILT,
    TRIGGER_TOF,
} trigger_source_t;

typedef struct {
    bool tilt_triggered;
    bool tof_triggered;
    app_state_t state;
} fusion_state_t;

static void draw_face(bool alert)
{
    driver_oled_clear();

    const int cx = 64;
    const int cy = 30;
    const int r = 18;

    driver_oled_draw_rect(cx - 24, cy - 22, 48, 44, true);

    driver_oled_draw_pixel(cx - 6, cy - 4, true);
    driver_oled_draw_pixel(cx + 6, cy - 4, true);

    if (alert) {
        driver_oled_draw_hline(cx - 8, cy + 8, 16, true);
        driver_oled_draw_vline(cx - 8, cy + 1, 8, true);
        driver_oled_draw_vline(cx + 8, cy + 1, 8, true);
    } else {
        for (int x = -8; x <= 8; x++) {
            int y = (int)(sqrtf((float)(r * r - x * x)) / 2.0f);
            driver_oled_draw_pixel(cx + x, cy + 8 + y, true);
            driver_oled_draw_pixel(cx + x, cy + 8 + y + 1, true);
        }
    }

    if (alert) {
        driver_oled_draw_string(34, 52, "WARNING", true);
    }

    driver_oled_flush();
}

static esp_err_t tts_speak_blocking(const char *text)
{
    if (!driver_tw_tts_is_initialized()) {
        return ESP_FAIL;
    }
    return driver_tw_tts_speak(text);
}

static bool read_tilt_triggered(float *out_deg)
{
    driver_mpu6050_data_t data;
    if (driver_mpu6050_read(&data) != ESP_OK) {
        return false;
    }

    float ax = data.accel_x_g;
    float ay = data.accel_y_g;
    float az = data.accel_z_g;

    float roll = atan2f(ay, az) * 57.2957795f;
    float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2957795f;

    float tilt = fabsf(roll);
    if (fabsf(pitch) > tilt) {
        tilt = fabsf(pitch);
    }

    if (out_deg) {
        *out_deg = tilt;
    }

    return tilt > SAFE_TILT_THRESHOLD_DEG;
}

static bool read_tof_triggered(uint16_t *raw_mm, int *calib_mm)
{
    driver_tof2000c_vl53l0x_result_t result;
    if (driver_tof2000c_vl53l0x_read_continuous(&result) != ESP_OK) {
        return false;
    }

    if (raw_mm) {
        *raw_mm = result.distance_mm;
    }

    if (result.timeout) {
        if (calib_mm) {
            *calib_mm = -1;
        }
        return false;
    }

    if (result.distance_mm <= TOF_CALIBRATION_OFFSET_MM) {
        if (calib_mm) {
            *calib_mm = 0;
        }
        return false;
    }

    int calibrated = (int)result.distance_mm - TOF_CALIBRATION_OFFSET_MM;
    if (calib_mm) {
        *calib_mm = calibrated;
    }

    return (calibrated > 0 && calibrated < TOF_ALERT_MAX_CALIB_MM);
}

static void update_state_and_notify(fusion_state_t *st, bool tilt_alarm, bool tof_alarm)
{
    app_state_t next_state = (tilt_alarm || tof_alarm) ? APP_STATE_ALERT : APP_STATE_SAFE;
    if (next_state == st->state) {
        return;
    }

    st->state = next_state;
    draw_face(st->state == APP_STATE_ALERT);

    if (st->state == APP_STATE_ALERT) {
        if (tilt_alarm) {
            ESP_LOGW(TAG, "Tilt alarm entered");
            tts_speak_blocking("我要翻了");
        }
        if (tof_alarm) {
            ESP_LOGW(TAG, "ToF alarm entered");
            tts_speak_blocking("我要撞了");
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting 510demo");

    service_device_init();

    driver_oled_config_t oled_cfg = {
        .controller = DRIVER_OLED_CONTROLLER_SSD1306,
        .i2c_address = DRIVER_OLED_DEFAULT_I2C_ADDR,
        .scl_speed_hz = 400000,
        .flip_x = false,
        .flip_y = false,
        .invert = false,
        .contrast = 0x7F,
    };

    esp_err_t ret = driver_oled_init(&oled_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(ret));
        return;
    }
    driver_oled_set_power(true);
    draw_face(false);

    driver_tw_tts_config_t tts_cfg;
    driver_tw_tts_get_default_config(&tts_cfg);
    tts_cfg.tx_timeout_ms = DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS;
    tts_cfg.boot_delay_ms = DRIVER_TW_TTS_DEFAULT_BOOT_DELAY_MS;
    tts_cfg.encoding = DRIVER_TW_TTS_ENCODING_UTF8;

    ret = driver_tw_tts_init(&tts_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TTS init failed: %s", esp_err_to_name(ret));
        return;
    }

    driver_tw_tts_set_volume(1);
    tts_speak_blocking("你好，莫测上线了");
    vTaskDelay(pdMS_TO_TICKS(500));

    driver_mpu6050_config_t mpu_cfg = {
        .i2c_address = DRIVER_MPU6050_I2C_ADDR_AD0_LOW,
        .scl_speed_hz = 400000,
        .accel_range = DRIVER_MPU6050_ACCEL_RANGE_4G,
        .gyro_range = DRIVER_MPU6050_GYRO_RANGE_500DPS,
        .dlpf = DRIVER_MPU6050_DLPF_BW_94HZ,
        .sample_rate_divider = 9,
    };

    ret = driver_mpu6050_init(&mpu_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(ret));
        return;
    }

    driver_tof2000c_vl53l0x_config_t tof_cfg;
    driver_tof2000c_vl53l0x_get_default_config(&tof_cfg);
    tof_cfg.i2c_address = DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT;
    tof_cfg.scl_speed_hz = 400000;
    tof_cfg.timeout_ms = 50;
    tof_cfg.measurement_timing_budget_us = 33000;

    ret = driver_tof2000c_vl53l0x_init(&tof_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = driver_tof2000c_vl53l0x_start_continuous(50);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X start continuous failed: %s", esp_err_to_name(ret));
        return;
    }

    fusion_state_t fusion = {
        .tilt_triggered = false,
        .tof_triggered = false,
        .state = APP_STATE_SAFE,
    };

    draw_face(false);

    while (1) {
        float tilt_deg = 0.0f;
        uint16_t raw_mm = 0;
        int calib_mm = 0;

        fusion.tilt_triggered = read_tilt_triggered(&tilt_deg);
        fusion.tof_triggered = read_tof_triggered(&raw_mm, &calib_mm);

        bool tilt_alarm = fusion.tilt_triggered;
        bool tof_alarm = fusion.tof_triggered;

        update_state_and_notify(&fusion, tilt_alarm, tof_alarm);

        ESP_LOGI(TAG, "state=%s tilt=%.1f tof_raw=%u tof_calib=%d tilt_alarm=%d tof_alarm=%d",
                 fusion.state == APP_STATE_ALERT ? "ALERT" : "SAFE",
                 (double)tilt_deg, raw_mm, calib_mm, (int)tilt_alarm, (int)tof_alarm);

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}
