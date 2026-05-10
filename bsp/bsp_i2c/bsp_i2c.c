#include "bsp_i2c.h"

#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "esp_log.h"

static const char *TAG = "bsp_i2c";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static bool s_inited = false;

static bool i2c_addr_7bit_is_valid(uint16_t address)
{
    return address <= 0x7f;
}

static int i2c_timeout_or_default(int timeout_ms)
{
    return timeout_ms >= 0 ? timeout_ms : BOARD_I2C_TIMEOUT_MS;
}

esp_err_t bsp_i2c_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = BOARD_I2C_GLITCH_IGNORE_CNT,
        .intr_priority = 0,
        .trans_queue_depth = BOARD_I2C_TRANS_QUEUE_DEPTH,
        .flags.enable_internal_pullup = BOARD_I2C_ENABLE_INTERNAL_PULLUP,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "I2C%d initialized: SDA GPIO %d, SCL GPIO %d, default speed %lu Hz",
             BOARD_I2C_PORT,
             BOARD_I2C_SDA_GPIO,
             BOARD_I2C_SCL_GPIO,
             (unsigned long)BOARD_I2C_FREQUENCY_HZ);
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    esp_err_t err = i2c_del_master_bus(s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_del_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    s_i2c_bus = NULL;
    s_inited = false;
    return ESP_OK;
}

bool bsp_i2c_is_initialized(void)
{
    return s_inited;
}

i2c_master_bus_handle_t bsp_i2c_get_bus_handle(void)
{
    return s_i2c_bus;
}

esp_err_t bsp_i2c_add_device(const bsp_i2c_device_config_t *config, i2c_master_dev_handle_t *out_handle)
{
    if (config == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->addr_bit_len == I2C_ADDR_BIT_LEN_7 && !i2c_addr_7bit_is_valid(config->device_address)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = config->addr_bit_len,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz > 0 ? config->scl_speed_hz : BOARD_I2C_FREQUENCY_HZ,
        .scl_wait_us = config->scl_wait_us,
    };

    err = i2c_master_bus_add_device(s_i2c_bus, &dev_config, out_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add I2C device 0x%02x failed: %s", config->device_address, esp_err_to_name(err));
    }
    return err;
}

esp_err_t bsp_i2c_add_device_7bit(uint8_t device_address, uint32_t scl_speed_hz, i2c_master_dev_handle_t *out_handle)
{
    bsp_i2c_device_config_t config = {
        .device_address = device_address,
        .scl_speed_hz = scl_speed_hz,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .scl_wait_us = 0,
    };

    return bsp_i2c_add_device(&config, out_handle);
}

esp_err_t bsp_i2c_remove_device(i2c_master_dev_handle_t dev_handle)
{
    if (dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t bsp_i2c_probe(uint8_t device_address, int timeout_ms)
{
    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) {
        return err;
    }

    return i2c_master_probe(s_i2c_bus, device_address, i2c_timeout_or_default(timeout_ms));
}

esp_err_t bsp_i2c_bus_reset(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_bus_reset(s_i2c_bus);
}

esp_err_t bsp_i2c_write(i2c_master_dev_handle_t dev_handle, const uint8_t *data, size_t len, int timeout_ms)
{
    if (dev_handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit(dev_handle, data, len, i2c_timeout_or_default(timeout_ms));
}

esp_err_t bsp_i2c_read(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len, int timeout_ms)
{
    if (dev_handle == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_receive(dev_handle, data, len, i2c_timeout_or_default(timeout_ms));
}

esp_err_t bsp_i2c_write_read(i2c_master_dev_handle_t dev_handle,
                             const uint8_t *write_data,
                             size_t write_len,
                             uint8_t *read_data,
                             size_t read_len,
                             int timeout_ms)
{
    if (dev_handle == NULL || write_data == NULL || write_len == 0 || read_data == NULL || read_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(dev_handle,
                                       write_data,
                                       write_len,
                                       read_data,
                                       read_len,
                                       i2c_timeout_or_default(timeout_ms));
}

esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg, const uint8_t *data, size_t len, int timeout_ms)
{
    if (dev_handle == NULL || (data == NULL && len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[1 + BOARD_I2C_MAX_REGISTER_WRITE_LEN];
    if (len > BOARD_I2C_MAX_REGISTER_WRITE_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    buffer[0] = reg;
    if (len > 0) {
        memcpy(&buffer[1], data, len);
    }

    return bsp_i2c_write(dev_handle, buffer, len + 1, timeout_ms);
}

esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *data, size_t len, int timeout_ms)
{
    return bsp_i2c_write_read(dev_handle, &reg, 1, data, len, timeout_ms);
}

esp_err_t bsp_i2c_write_reg_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value, int timeout_ms)
{
    return bsp_i2c_write_reg(dev_handle, reg, &value, 1, timeout_ms);
}

esp_err_t bsp_i2c_read_reg_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *value, int timeout_ms)
{
    return bsp_i2c_read_reg(dev_handle, reg, value, 1, timeout_ms);
}
