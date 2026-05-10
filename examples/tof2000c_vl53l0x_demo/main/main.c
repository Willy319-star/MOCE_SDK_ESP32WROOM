#include "driver_tof2000c_vl53l0x.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tof2000c_vl53l0x_demo";

void app_main(void)
{
    ESP_LOGI(TAG, "start TOF2000C-VL53L0X demo");

    esp_err_t err = driver_tof2000c_vl53l0x_probe(DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "probe 0x%02x failed: %s",
                 DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT,
                 esp_err_to_name(err));
        ESP_LOGE(TAG, "check VCC/GND/SDA/SCL and pullups");
        ESP_ERROR_CHECK(err);
    }

    driver_tof2000c_vl53l0x_config_t config;
    driver_tof2000c_vl53l0x_get_default_config(&config);
    config.timeout_ms = 2000;
    config.measurement_timing_budget_us = 33000;
    err = driver_tof2000c_vl53l0x_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "keep running so monitor logs are visible; reset after fixing wiring/power");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    uint8_t model_id = 0;
    ESP_ERROR_CHECK(driver_tof2000c_vl53l0x_read_model_id(&model_id));
    ESP_LOGI(TAG, "model id = 0x%02x", model_id);
    ESP_ERROR_CHECK(driver_tof2000c_vl53l0x_start_continuous(100));
    ESP_LOGI(TAG, "continuous timed ranging started");

    while (1) {
        driver_tof2000c_vl53l0x_result_t result;
        err = driver_tof2000c_vl53l0x_read_continuous(&result);
        if (err == ESP_OK) {
            ESP_LOGI(TAG,
                     "distance=%u mm range_status=0x%02x",
                     result.distance_mm,
                     result.range_status);
        } else {
            ESP_LOGE(TAG, "read distance failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
