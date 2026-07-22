#include <stdio.h>

#include "ch32_vl53l0x_gateway_final.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tof_gateway_test";

static void print_status(const char *prefix)
{
    ch32_vl53l0x_status_t status;
    ch32_vl53l0x_get_status(&status);
    ESP_LOGI(TAG,
             "%s state=%s node=%u addr=0x%02X model=0x%02X gateway=%d device=%d init=%d can_rx=%lu can_tx=%lu errors=%lu last=%s raw=%u mm=%u",
             prefix,
             ch32_vl53l0x_state_text(status.state),
             status.ch32_node_id,
             status.tof_addr7,
             status.model_id,
             status.gateway_online ? 1 : 0,
             status.device_found ? 1 : 0,
             status.initialized ? 1 : 0,
             (unsigned long)status.can_rx_count,
             (unsigned long)status.can_tx_count,
             (unsigned long)status.error_count,
             ch32_vl53l0x_result_text(status.last_result),
             status.last_raw_distance_mm,
             status.last_distance_mm);
}

void app_main(void)
{
    printf("\n==== ESP32-WROOM VL53L0X over CH32 generic I2C bridge ====\n");
    printf("Required CH32 firmware: examples_final/CH32_I2C_bridge_generic\n");
    printf("ESP32 CAN: TX=GPIO5 RX=GPIO4 bitrate=50 kbit/s\n");
    printf("VL53L0X address: addr7=0x29 addr8_write=0x52 model_id=0xEE\n\n");

    ch32_vl53l0x_config_t config;
    ch32_vl53l0x_default_config(&config);

    esp_err_t err = ch32_vl53l0x_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CAN init failed err=%s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "twai started, waiting for CH32 I2C bridge HELLO");

    err = ch32_vl53l0x_wait_gateway(config.gateway_wait_ms);
    ESP_LOGI(TAG, "gateway wait result=%s", err == ESP_OK ? "OK" : "TIMEOUT");
    print_status("after_gateway_wait");

    while (true) {
        ch32_vl53l0x_result_t begin = ch32_vl53l0x_begin();
        ESP_LOGI(TAG, "tof init result=%s", ch32_vl53l0x_result_text(begin));
        print_status("after_init");

        if (begin == CH32_VL53L0X_RESULT_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) {
        uint16_t distance = 0;
        uint16_t raw = 0;
        ch32_vl53l0x_result_t result = ch32_vl53l0x_read_distance(&distance, &raw);
        if (result == CH32_VL53L0X_RESULT_OK) {
            ESP_LOGI(TAG, "tof distance=%u mm raw=%u result=OK", distance, raw);
        } else if (result == CH32_VL53L0X_RESULT_OUT_OF_RANGE) {
            ESP_LOGW(TAG, "tof distance out_of_range raw=%u result=OUT_OF_RANGE", raw);
        } else {
            ESP_LOGW(TAG, "tof read result=%s, reinitializing", ch32_vl53l0x_result_text(result));
            print_status("read_fail");
            ch32_vl53l0x_begin();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
