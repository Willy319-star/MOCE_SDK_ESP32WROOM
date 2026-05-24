#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"

static const char *TAG = "510demo";

#define I2C_SCL_SPEED_HZ            400000
#define MPU6050_TILT_THRESHOLD_DEG  30.0f
#define TOF_OFFSET_MM                30
#define TOF_ALARM_MIN_MM             1
#define TOF_ALARM_MAX_MM             29
#define STATUS_POLL_MS               100
#define ALERT_RETRY_MS               200
#define TFT_BOOT_DELAY_MS            300

typedef enum {
    APP_STATE_SAFE = 0,
    APP_STATE_ALERT,
} app_state_t;

typedef struct {
    bool tilt_triggered;
    bool tof_triggered;
    bool alert;
    float tilt_deg;
    uint16_t raw_distance_mm;
    uint16_t calibrated_distance_mm;
} sensor_snapshot_t;

static void draw_face(bool alert)
{
    driver_oled_clear();

    const int cx = DRIVER_OLED_WIDTH / 2;
    const int cy = DRIVER_OLED_HEIGHT / 2 - 4;

    driver_oled_draw_rect(cx - 22, cy - 18, 8, 8, true);
    driver_oled_draw_rect(cx + 14, cy - 18, 8, 8, true);
    driver_oled_fill_rect(cx - 22, cy - 18, 3, 3, true);
    driver_oled_fill_rect(cx + 14, cy - 18, 3, 3, true);

    driver_oled_draw_pixel(cx - 1, cy - 1, true);
    driver_oled_draw_pixel(cx, cy - 1, true);
    driver_oled_draw_pixel(cx + 1, cy - 1, true);

    if (alert) {
        driver_oled_draw_hline(cx - 14, cy + 13, 29, true);
        driver_oled_draw_vline(cx - 14, cy + 9, 5, true);
        driver_oled_draw_vline(cx + 14, cy + 9, 5, true);
        driver_oled_draw_string(32, DRIVER_OLED_HEIGHT - 12, "WARNING", true);
    } else {
        driver_oled_draw_hline(cx - 14, cy + 10, 29, true);
        driver_oled_draw_vline(cx - 14, cy + 10, 5, true);
        driver_oled_draw_vline(cx + 14, cy + 10, 5, true);
    }

    if (driver_oled_flush() != ESP_OK) {
        ESP_LOGW(TAG, "OLED flush failed");
    }
}

static esp_err_t init_oled(void)
{
    esp_err_t ret = driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    driver_oled_config_t cfg = {
        .controller = DRIVER_OLED_CONTROLLER_SSD1306,
        .i2c_address = DRIVER_OLED_DEFAULT_I2C_ADDR,
        .scl_speed_hz = I2C_SCL_SPEED_HZ,
        .flip_x = false,
        .flip_y = false,
        .invert = false,
        .contrast = 0x7F,
    };
    return driver_oled_init(&cfg);
}

static esp_err_t init_tts(void)
{
    esp_err_t ret = driver_tw_tts_init_default();
    if (ret != ESP_OK) {
        driver_tw_tts_config_t cfg;
        driver_tw_tts_get_default_config(&cfg);
        cfg.encoding = DRIVER_TW_TTS_ENCODING_UTF8;
        cfg.tx_timeout_ms = DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS;
        cfg.boot_delay_ms = TFT_BOOT_DELAY_MS;
        ret = driver_tw_tts_init(&cfg);
    }

    if (ret == ESP_OK) {
        ret = driver_tw_tts_set_volume(1);
    }
    return ret;
}

static esp_err_t init_mpu6050(void)
{
    esp_err_t ret = driver_mpu6050_init_default();
    if (ret == ESP_OK) {
        return ret;
    }

    driver_mpu6050_config_t cfg = {
        .i2c_address = DRIVER_MPU6050_I2C_ADDR_AD0_LOW,
        .scl_speed_hz = I2C_SCL_SPEED_HZ,
        .accel_range = DRIVER_MPU6050_ACCEL_RANGE_2G,
        .gyro_range = DRIVER_MPU6050_GYRO_RANGE_250DPS,
        .dlpf = DRIVER_MPU6050_DLPF_BW_44HZ,
        .sample_rate_divider = 9,
    };
    return driver_mpu6050_init(&cfg);
}

static esp_err_t init_tof(void)
{
    esp_err_t ret = driver_tof2000c_vl53l0x_init_default();
    if (ret == ESP_OK) {
        return ret;
    }

    driver_tof2000c_vl53l0x_config_t cfg;
    driver_tof2000c_vl53l0x_get_default_config(&cfg);
    cfg.i2c_address = DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT;
    cfg.scl_speed_hz = I2C_SCL_SPEED_HZ;
    cfg.timeout_ms = 50;
    cfg.measurement_timing_budget_us = 33000;
    return driver_tof2000c_vl53l0x_init(&cfg);
}

static void get_tilt_snapshot(sensor_snapshot_t *snapshot)
{
    driver_mpu6050_data_t data;
    snapshot->tilt_triggered = false;
    snapshot->tilt_deg = 0.0f;

    if (driver_mpu6050_read(&data) != ESP_OK) {
        return;
    }

    snapshot->tilt_deg = atan2f(data.accel_x_g, data.accel_z_g) * 180.0f / (float)M_PI;
    if (fabsf(snapshot->tilt_deg) > MPU6050_TILT_THRESHOLD_DEG) {
        snapshot->tilt_triggered = true;
    }
}

static void get_tof_snapshot(sensor_snapshot_t *snapshot)
{
    driver_tof2000c_vl53l0x_result_t result = {0};
    snapshot->tof_triggered = false;
    snapshot->raw_distance_mm = 0;
    snapshot->calibrated_distance_mm = 0;

    if (driver_tof2000c_vl53l0x_read_continuous(&result) != ESP_OK) {
        if (driver_tof2000c_vl53l0x_read_single(&result) != ESP_OK) {
            return;
        }
    }

    snapshot->raw_distance_mm = result.distance_mm;

    if (result.distance_mm > TOF_OFFSET_MM) {
        snapshot->calibrated_distance_mm = result.distance_mm - TOF_OFFSET_MM;
        if (snapshot->calibrated_distance_mm >= TOF_ALARM_MIN_MM &&
            snapshot->calibrated_distance_mm <= TOF_ALARM_MAX_MM) {
            snapshot->tof_triggered = true;
        }
    }
}

static sensor_snapshot_t read_sensors(void)
{
    sensor_snapshot_t snapshot = {0};
    get_tilt_snapshot(&snapshot);
    get_tof_snapshot(&snapshot);
    snapshot.alert = snapshot.tilt_triggered || snapshot.tof_triggered;
    return snapshot;
}

static void speak_alert_once(const sensor_snapshot_t *snapshot)
{
    if (snapshot->tilt_triggered) {
        ESP_LOGI(TAG, "tilt alert: %.2f deg", (double)snapshot->tilt_deg);
        if (driver_tw_tts_speak("我要翻了") != ESP_OK) {
            ESP_LOGW(TAG, "TTS speak failed");
        }
    }

    if (snapshot->tof_triggered) {
        ESP_LOGI(TAG, "tof alert: raw=%u mm, calibrated=%u mm",
                 snapshot->raw_distance_mm, snapshot->calibrated_distance_mm);
        if (driver_tw_tts_speak("我要撞了") != ESP_OK) {
            ESP_LOGW(TAG, "TTS speak failed");
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "app start");

    if (init_oled() != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed");
    }
    draw_face(false);

    if (init_tts() == ESP_OK) {
        driver_tw_tts_speak("你好，莫测上线了");
    } else {
        ESP_LOGE(TAG, "TTS init failed");
    }

    if (init_mpu6050() != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed");
    }
    if (init_tof() != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X init failed");
    }

    app_state_t state = APP_STATE_SAFE;
    bool alert_announced = false;

    while (true) {
        sensor_snapshot_t snapshot = read_sensors();
        app_state_t next_state = snapshot.alert ? APP_STATE_ALERT : APP_STATE_SAFE;

        if (next_state != state) {
            state = next_state;
            alert_announced = false;
            draw_face(state == APP_STATE_ALERT);

            if (state == APP_STATE_ALERT) {
                speak_alert_once(&snapshot);
                alert_announced = true;
            }
        } else if (state == APP_STATE_ALERT && !alert_announced) {
            speak_alert_once(&snapshot);
            alert_announced = true;
        }

        vTaskDelay(pdMS_TO_TICKS(state == APP_STATE_ALERT ? ALERT_RETRY_MS : STATUS_POLL_MS));
    }
}