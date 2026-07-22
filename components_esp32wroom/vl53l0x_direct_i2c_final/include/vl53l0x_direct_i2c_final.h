#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VL53L0X_DIRECT_DEFAULT_I2C_ADDR7      0x29U
#define VL53L0X_DIRECT_EXPECTED_MODEL_ID      0xEEU
#define VL53L0X_DIRECT_DEFAULT_I2C_SPEED_HZ   100000U
#define VL53L0X_DIRECT_SCAN_MAX_RESULTS       16U

typedef enum {
    VL53L0X_DIRECT_STATE_RESET = 0,
    VL53L0X_DIRECT_STATE_I2C_READY,
    VL53L0X_DIRECT_STATE_DEVICE_FOUND,
    VL53L0X_DIRECT_STATE_READY,
    VL53L0X_DIRECT_STATE_ERROR,
} vl53l0x_direct_state_t;

typedef enum {
    VL53L0X_DIRECT_RESULT_OK = 0,
    VL53L0X_DIRECT_RESULT_OUT_OF_RANGE,
    VL53L0X_DIRECT_RESULT_ADDR_NOT_FOUND,
    VL53L0X_DIRECT_RESULT_MODEL_ID_READ_FAIL,
    VL53L0X_DIRECT_RESULT_MODEL_ID_MISMATCH,
    VL53L0X_DIRECT_RESULT_CONFIG_FAIL,
    VL53L0X_DIRECT_RESULT_START_TIMEOUT,
    VL53L0X_DIRECT_RESULT_MEASURE_TIMEOUT,
    VL53L0X_DIRECT_RESULT_COMM_FAIL,
    VL53L0X_DIRECT_RESULT_NOT_READY,
    VL53L0X_DIRECT_RESULT_BAD_ARG,
} vl53l0x_direct_result_t;

typedef struct {
    uint8_t tof_addr7;
    uint32_t i2c_speed_hz;
    uint32_t command_timeout_ms;
} vl53l0x_direct_config_t;

typedef struct {
    vl53l0x_direct_state_t state;
    uint8_t tof_addr7;
    uint8_t model_id;
    bool i2c_ready;
    bool device_found;
    bool initialized;
    uint32_t i2c_tx_count;
    uint32_t i2c_rx_count;
    uint32_t error_count;
    uint8_t last_scan_found_count;
    uint8_t last_scan_addresses[VL53L0X_DIRECT_SCAN_MAX_RESULTS];
    uint16_t last_distance_mm;
    uint16_t last_raw_distance_mm;
    vl53l0x_direct_result_t last_result;
} vl53l0x_direct_status_t;

void vl53l0x_direct_default_config(vl53l0x_direct_config_t *config);
esp_err_t vl53l0x_direct_init(const vl53l0x_direct_config_t *config);
uint8_t vl53l0x_direct_scan(uint8_t *addresses, uint8_t max_addresses);
vl53l0x_direct_result_t vl53l0x_direct_probe(void);
vl53l0x_direct_result_t vl53l0x_direct_begin(void);
vl53l0x_direct_result_t vl53l0x_direct_read_distance(uint16_t *distance_mm, uint16_t *raw_distance_mm);
void vl53l0x_direct_get_bus_levels(int *sda_level, int *scl_level);
void vl53l0x_direct_get_status(vl53l0x_direct_status_t *status);
const char *vl53l0x_direct_state_text(vl53l0x_direct_state_t state);
const char *vl53l0x_direct_result_text(vl53l0x_direct_result_t result);

#ifdef __cplusplus
}
#endif
