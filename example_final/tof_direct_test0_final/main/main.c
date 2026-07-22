#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vl53l0x_direct_i2c_final.h"

static const char *TAG = "tof_direct_test0_final";

static void print_status(const char *prefix)
{
    vl53l0x_direct_status_t status;
    vl53l0x_direct_get_status(&status);
    ESP_LOGI(TAG,
             "%s state=%s addr=0x%02X model=0x%02X i2c=%d device=%d init=%d tx=%lu rx=%lu errors=%lu scan_found=%u last=%s raw=%u mm=%u",
             prefix,
             vl53l0x_direct_state_text(status.state),
             status.tof_addr7,
             status.model_id,
             status.i2c_ready ? 1 : 0,
             status.device_found ? 1 : 0,
             status.initialized ? 1 : 0,
             (unsigned long)status.i2c_tx_count,
             (unsigned long)status.i2c_rx_count,
             (unsigned long)status.error_count,
             status.last_scan_found_count,
             vl53l0x_direct_result_text(status.last_result),
             status.last_raw_distance_mm,
             status.last_distance_mm);
}

static void print_bus_levels(const char *prefix)
{
    int sda = -1;
    int scl = -1;
    vl53l0x_direct_get_bus_levels(&sda, &scl);
    printf("tof i2c bus %s sda=%d scl=%d idle_ok=%d\n", prefix, sda, scl, (sda == 1 && scl == 1) ? 1 : 0);
}

static void print_i2c_scan(void)
{
    uint8_t found[VL53L0X_DIRECT_SCAN_MAX_RESULTS] = {0};
    uint8_t found_count = vl53l0x_direct_scan(found, VL53L0X_DIRECT_SCAN_MAX_RESULTS);

    printf("tof i2c scan found=%u addr:", found_count);
    uint8_t shown = found_count;
    if (shown > VL53L0X_DIRECT_SCAN_MAX_RESULTS) {
        shown = VL53L0X_DIRECT_SCAN_MAX_RESULTS;
    }
    for (uint8_t i = 0; i < shown; ++i) {
        printf(" 0x%02X", found[i]);
    }
    if (found_count > VL53L0X_DIRECT_SCAN_MAX_RESULTS) {
        printf(" ...");
    }
    printf("\n");
}

void app_main(void)
{
    printf("\n==== ESP32-WROOM tof_direct_test0_final ====" "\n");
    printf("Connection: ESP32-WROOM GPIO21 SDA -> VL53L0X SDA, GPIO22 SCL -> VL53L0X SCL\n");
    printf("VL53L0X address: addr7=0x29 addr8_write=0x52 model_id=0xEE\n");
    printf("I2C direct mode: 100 kHz, bus-level check and scan on address failure\n\n");

    vl53l0x_direct_config_t config;
    vl53l0x_direct_default_config(&config);

    esp_err_t err = vl53l0x_direct_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tof direct i2c init failed err=%s", esp_err_to_name(err));
        print_status("init_fail");
        return;
    }
    ESP_LOGI(TAG, "i2c initialized, probing VL53L0X");
    print_bus_levels("after_i2c_init");
    print_status("after_i2c_init");

    uint32_t init_attempt = 0;
    while (true) {
        init_attempt++;
        vl53l0x_direct_result_t begin = vl53l0x_direct_begin();
        ESP_LOGI(TAG, "tof init result=%s", vl53l0x_direct_result_text(begin));
        print_status("after_init");

        if (begin == VL53L0X_DIRECT_RESULT_OK) {
            break;
        }

        if (begin == VL53L0X_DIRECT_RESULT_ADDR_NOT_FOUND && (init_attempt == 1 || (init_attempt % 5U) == 0U)) {
            print_bus_levels("addr_not_found");
            print_i2c_scan();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) {
        uint16_t distance = 0;
        uint16_t raw = 0;
        vl53l0x_direct_result_t result = vl53l0x_direct_read_distance(&distance, &raw);
        if (result == VL53L0X_DIRECT_RESULT_OK) {
            ESP_LOGI(TAG, "tof distance=%u mm raw=%u result=OK", distance, raw);
        } else if (result == VL53L0X_DIRECT_RESULT_OUT_OF_RANGE) {
            ESP_LOGW(TAG, "tof distance out_of_range raw=%u result=OUT_OF_RANGE", raw);
        } else {
            ESP_LOGW(TAG, "tof read result=%s, reinitializing", vl53l0x_direct_result_text(result));
            print_status("read_fail");
            (void)vl53l0x_direct_begin();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
