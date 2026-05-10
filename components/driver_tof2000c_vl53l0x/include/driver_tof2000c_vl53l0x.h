#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT 0x29
#define DRIVER_TOF2000C_VL53L0X_MODEL_ID         0xee

typedef struct {
    uint8_t i2c_address;
    uint32_t scl_speed_hz;
    uint16_t timeout_ms;
    uint32_t measurement_timing_budget_us;
} driver_tof2000c_vl53l0x_config_t;

typedef struct {
    uint16_t distance_mm;
    uint8_t range_status;
    bool timeout;
} driver_tof2000c_vl53l0x_result_t;

void driver_tof2000c_vl53l0x_get_default_config(driver_tof2000c_vl53l0x_config_t *config);

esp_err_t driver_tof2000c_vl53l0x_probe(uint8_t i2c_address);
esp_err_t driver_tof2000c_vl53l0x_init(const driver_tof2000c_vl53l0x_config_t *config);
esp_err_t driver_tof2000c_vl53l0x_init_default(void);
esp_err_t driver_tof2000c_vl53l0x_deinit(void);
bool driver_tof2000c_vl53l0x_is_initialized(void);

esp_err_t driver_tof2000c_vl53l0x_read_model_id(uint8_t *model_id);
esp_err_t driver_tof2000c_vl53l0x_set_timeout(uint16_t timeout_ms);
bool driver_tof2000c_vl53l0x_timeout_occurred(void);

esp_err_t driver_tof2000c_vl53l0x_start_continuous(uint32_t period_ms);
esp_err_t driver_tof2000c_vl53l0x_stop_continuous(void);
esp_err_t driver_tof2000c_vl53l0x_read_continuous(driver_tof2000c_vl53l0x_result_t *result);
esp_err_t driver_tof2000c_vl53l0x_read_single(driver_tof2000c_vl53l0x_result_t *result);

#ifdef __cplusplus
}
#endif
