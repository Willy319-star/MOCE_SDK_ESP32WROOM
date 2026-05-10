#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"

#include "driver_mpu6050.h"

static const char *TAG = "mpu6050_demo";

void app_main(void)
{
    ESP_LOGI(TAG, "start MPU6050 demo");

    ESP_ERROR_CHECK(driver_mpu6050_init_default());

    uint8_t who_am_i = 0;
    ESP_ERROR_CHECK(driver_mpu6050_read_who_am_i(&who_am_i));
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02x", who_am_i);

    while (1) {
        driver_mpu6050_data_t data;
        esp_err_t err = driver_mpu6050_read(&data);
        if (err == ESP_OK) {
            ESP_LOGI(TAG,
                     "accel[g] x=%.3f y=%.3f z=%.3f, gyro[dps] x=%.2f y=%.2f z=%.2f, temp=%.2f C",
                     data.accel_x_g,
                     data.accel_y_g,
                     data.accel_z_g,
                     data.gyro_x_dps,
                     data.gyro_y_dps,
                     data.gyro_z_dps,
                     data.temperature_c);
        } else {
            ESP_LOGE(TAG, "read failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
