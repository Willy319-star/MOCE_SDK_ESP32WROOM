#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_VL53L0X_DEFAULT_NODE_ID       1U
#define CH32_VL53L0X_DEFAULT_I2C_ADDR7     0x29U
#define CH32_VL53L0X_EXPECTED_MODEL_ID     0xEEU
#define CH32_VL53L0X_CAN_BITRATE_HZ        50000U

typedef enum {
    CH32_VL53L0X_STATE_RESET = 0,
    CH32_VL53L0X_STATE_CAN_READY,
    CH32_VL53L0X_STATE_GATEWAY_ONLINE,
    CH32_VL53L0X_STATE_DEVICE_FOUND,
    CH32_VL53L0X_STATE_READY,
    CH32_VL53L0X_STATE_ERROR,
} ch32_vl53l0x_state_t;

typedef enum {
    CH32_VL53L0X_RESULT_OK = 0,
    CH32_VL53L0X_RESULT_OUT_OF_RANGE,
    CH32_VL53L0X_RESULT_ADDR_NOT_FOUND,
    CH32_VL53L0X_RESULT_MODEL_ID_READ_FAIL,
    CH32_VL53L0X_RESULT_MODEL_ID_MISMATCH,
    CH32_VL53L0X_RESULT_CONFIG_FAIL,
    CH32_VL53L0X_RESULT_START_TIMEOUT,
    CH32_VL53L0X_RESULT_MEASURE_TIMEOUT,
    CH32_VL53L0X_RESULT_COMM_FAIL,
    CH32_VL53L0X_RESULT_NOT_READY,
    CH32_VL53L0X_RESULT_BAD_ARG,
} ch32_vl53l0x_result_t;

typedef struct {
    gpio_num_t can_tx_gpio;
    gpio_num_t can_rx_gpio;
    uint8_t ch32_node_id;
    uint8_t tof_addr7;
    uint32_t command_timeout_ms;
    uint32_t gateway_wait_ms;
} ch32_vl53l0x_config_t;

typedef struct {
    ch32_vl53l0x_state_t state;
    uint8_t ch32_node_id;
    uint8_t tof_addr7;
    uint8_t model_id;
    bool gateway_online;
    bool device_found;
    bool initialized;
    uint32_t can_rx_count;
    uint32_t can_tx_count;
    uint32_t error_count;
    uint16_t last_distance_mm;
    uint16_t last_raw_distance_mm;
    ch32_vl53l0x_result_t last_result;
} ch32_vl53l0x_status_t;

void ch32_vl53l0x_default_config(ch32_vl53l0x_config_t *config);
esp_err_t ch32_vl53l0x_init(const ch32_vl53l0x_config_t *config);
esp_err_t ch32_vl53l0x_wait_gateway(uint32_t timeout_ms);
ch32_vl53l0x_result_t ch32_vl53l0x_probe(void);
ch32_vl53l0x_result_t ch32_vl53l0x_begin(void);
ch32_vl53l0x_result_t ch32_vl53l0x_read_distance(uint16_t *distance_mm, uint16_t *raw_distance_mm);
void ch32_vl53l0x_get_status(ch32_vl53l0x_status_t *status);
const char *ch32_vl53l0x_state_text(ch32_vl53l0x_state_t state);
const char *ch32_vl53l0x_result_text(ch32_vl53l0x_result_t result);

#ifdef __cplusplus
}
#endif
