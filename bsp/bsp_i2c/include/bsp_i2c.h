#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t device_address;
    uint32_t scl_speed_hz;
    i2c_addr_bit_len_t addr_bit_len;
    uint32_t scl_wait_us;
} bsp_i2c_device_config_t;

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
bool bsp_i2c_is_initialized(void);
i2c_master_bus_handle_t bsp_i2c_get_bus_handle(void);

esp_err_t bsp_i2c_add_device(const bsp_i2c_device_config_t *config, i2c_master_dev_handle_t *out_handle);
esp_err_t bsp_i2c_add_device_7bit(uint8_t device_address, uint32_t scl_speed_hz, i2c_master_dev_handle_t *out_handle);
esp_err_t bsp_i2c_remove_device(i2c_master_dev_handle_t dev_handle);

esp_err_t bsp_i2c_probe(uint8_t device_address, int timeout_ms);
esp_err_t bsp_i2c_bus_reset(void);

esp_err_t bsp_i2c_write(i2c_master_dev_handle_t dev_handle, const uint8_t *data, size_t len, int timeout_ms);
esp_err_t bsp_i2c_read(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len, int timeout_ms);
esp_err_t bsp_i2c_write_read(i2c_master_dev_handle_t dev_handle,
                             const uint8_t *write_data,
                             size_t write_len,
                             uint8_t *read_data,
                             size_t read_len,
                             int timeout_ms);

esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg, const uint8_t *data, size_t len, int timeout_ms);
esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *data, size_t len, int timeout_ms);
esp_err_t bsp_i2c_write_reg_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value, int timeout_ms);
esp_err_t bsp_i2c_read_reg_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *value, int timeout_ms);

#ifdef __cplusplus
}
#endif
