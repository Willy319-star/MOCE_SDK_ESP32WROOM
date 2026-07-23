#include "vl53l0x_direct_i2c_final.h"

#include <string.h>

#include "board.h"
#include "bsp_i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "vl53l0x_direct"

#define REG_SYSRANGE_START       0x00U
#define REG_RESULT_RANGE_STATUS  0x14U
#define REG_MODEL_ID             0xC0U

typedef struct {
    uint8_t reg;
    uint8_t value;
} reg_write_t;

static const reg_write_t k_init_table[] = {
    {0x88, 0x00}, {0x80, 0x01}, {0xFF, 0x01}, {0x00, 0x00},
    {0x91, 0x3C}, {0x00, 0x01}, {0xFF, 0x00}, {0x80, 0x00},
    {0x60, 0x00}, {0x44, 0x00}, {0x01, 0xFF}, {0x80, 0x01},
    {0xFF, 0x01}, {0x00, 0x00}, {0x91, 0x3C}, {0x00, 0x01},
    {0xFF, 0x00}, {0x80, 0x00},
};

static vl53l0x_direct_config_t s_cfg;
static vl53l0x_direct_status_t s_status;
static i2c_master_dev_handle_t s_dev;

static vl53l0x_direct_result_t set_result(vl53l0x_direct_result_t result)
{
    s_status.last_result = result;
    if (result != VL53L0X_DIRECT_RESULT_OK && result != VL53L0X_DIRECT_RESULT_OUT_OF_RANGE) {
        s_status.error_count++;
        s_status.state = VL53L0X_DIRECT_STATE_ERROR;
    }
    return result;
}

static bool write_reg(uint8_t reg, uint8_t value)
{
    esp_err_t err = bsp_i2c_write_reg_byte(s_dev, reg, value, (int)s_cfg.command_timeout_ms);
    if (err == ESP_OK) {
        s_status.i2c_tx_count++;
        return true;
    }
    ESP_LOGD(TAG, "write reg 0x%02X failed: %s", reg, esp_err_to_name(err));
    return false;
}

static bool read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }

    esp_err_t err = bsp_i2c_read_reg(s_dev, reg, data, len, (int)s_cfg.command_timeout_ms);
    if (err == ESP_OK) {
        s_status.i2c_tx_count++;
        s_status.i2c_rx_count++;
        return true;
    }
    ESP_LOGD(TAG, "read reg 0x%02X len=%u failed: %s", reg, len, esp_err_to_name(err));
    return false;
}

void vl53l0x_direct_default_config(vl53l0x_direct_config_t *config)
{
    if (config == NULL) {
        return;
    }
    config->tof_addr7 = VL53L0X_DIRECT_DEFAULT_I2C_ADDR7;
    config->i2c_speed_hz = VL53L0X_DIRECT_DEFAULT_I2C_SPEED_HZ;
    config->command_timeout_ms = 300U;
}

esp_err_t vl53l0x_direct_init(const vl53l0x_direct_config_t *config)
{
    vl53l0x_direct_config_t local;
    if (config == NULL) {
        vl53l0x_direct_default_config(&local);
        config = &local;
    }

    s_cfg = *config;
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = VL53L0X_DIRECT_STATE_RESET;
    s_status.tof_addr7 = s_cfg.tof_addr7;
    s_status.last_result = VL53L0X_DIRECT_RESULT_NOT_READY;

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) {
        set_result(VL53L0X_DIRECT_RESULT_COMM_FAIL);
        return err;
    }

    s_status.i2c_ready = true;
    s_status.state = VL53L0X_DIRECT_STATE_I2C_READY;

    vTaskDelay(pdMS_TO_TICKS(80));

    err = bsp_i2c_add_device_7bit(s_cfg.tof_addr7, s_cfg.i2c_speed_hz, &s_dev);
    if (err != ESP_OK) {
        set_result(VL53L0X_DIRECT_RESULT_COMM_FAIL);
        return err;
    }

    return ESP_OK;
}

uint8_t vl53l0x_direct_scan(uint8_t *addresses, uint8_t max_addresses)
{
    uint8_t found = 0;

    for (uint8_t addr = 0x03U; addr <= 0x77U; ++addr) {
        esp_err_t err = bsp_i2c_probe(addr, 50);
        s_status.i2c_tx_count++;
        if (err == ESP_OK) {
            if (addresses != NULL && found < max_addresses) {
                addresses[found] = addr;
            }
            found++;
        }
    }

    s_status.last_scan_found_count = found;
    memset(s_status.last_scan_addresses, 0, sizeof(s_status.last_scan_addresses));
    if (addresses != NULL) {
        uint8_t copy_count = found;
        if (copy_count > max_addresses) {
            copy_count = max_addresses;
        }
        if (copy_count > VL53L0X_DIRECT_SCAN_MAX_RESULTS) {
            copy_count = VL53L0X_DIRECT_SCAN_MAX_RESULTS;
        }
        memcpy(s_status.last_scan_addresses, addresses, copy_count);
    }

    return found;
}

vl53l0x_direct_result_t vl53l0x_direct_probe(void)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        esp_err_t err = bsp_i2c_probe(s_cfg.tof_addr7, (int)s_cfg.command_timeout_ms);
        s_status.i2c_tx_count++;
        if (err == ESP_OK) {
            s_status.device_found = true;
            s_status.state = VL53L0X_DIRECT_STATE_DEVICE_FOUND;
            return set_result(VL53L0X_DIRECT_RESULT_OK);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    s_status.device_found = false;
    return set_result(VL53L0X_DIRECT_RESULT_ADDR_NOT_FOUND);
}

vl53l0x_direct_result_t vl53l0x_direct_begin(void)
{
    vl53l0x_direct_result_t probe = vl53l0x_direct_probe();
    if (probe != VL53L0X_DIRECT_RESULT_OK) {
        return probe;
    }

    uint8_t model = 0;
    if (!read_regs(REG_MODEL_ID, &model, 1)) {
        return set_result(VL53L0X_DIRECT_RESULT_MODEL_ID_READ_FAIL);
    }
    s_status.model_id = model;
    if (model != VL53L0X_DIRECT_EXPECTED_MODEL_ID) {
        return set_result(VL53L0X_DIRECT_RESULT_MODEL_ID_MISMATCH);
    }

    for (size_t i = 0; i < sizeof(k_init_table) / sizeof(k_init_table[0]); ++i) {
        if (!write_reg(k_init_table[i].reg, k_init_table[i].value)) {
            return set_result(VL53L0X_DIRECT_RESULT_CONFIG_FAIL);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    s_status.initialized = true;
    s_status.state = VL53L0X_DIRECT_STATE_READY;
    return set_result(VL53L0X_DIRECT_RESULT_OK);
}

vl53l0x_direct_result_t vl53l0x_direct_read_distance(uint16_t *distance_mm, uint16_t *raw_distance_mm)
{
    if (distance_mm == NULL) {
        return set_result(VL53L0X_DIRECT_RESULT_BAD_ARG);
    }
    if (!s_status.initialized) {
        return set_result(VL53L0X_DIRECT_RESULT_NOT_READY);
    }

    if (!write_reg(REG_SYSRANGE_START, 0x01)) {
        return set_result(VL53L0X_DIRECT_RESULT_COMM_FAIL);
    }

    uint8_t status = 0;
    int ready_try = 0;
    for (; ready_try < 25; ++ready_try) {
        if (!read_regs(REG_RESULT_RANGE_STATUS, &status, 1)) {
            return set_result(VL53L0X_DIRECT_RESULT_COMM_FAIL);
        }
        if ((status & 0x01U) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (ready_try >= 25) {
        return set_result(VL53L0X_DIRECT_RESULT_MEASURE_TIMEOUT);
    }

    uint8_t buf[2] = {0};
    if (!read_regs((uint8_t)(REG_RESULT_RANGE_STATUS + 10U), buf, sizeof(buf))) {
        return set_result(VL53L0X_DIRECT_RESULT_COMM_FAIL);
    }

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    s_status.last_raw_distance_mm = raw;
    if (raw_distance_mm != NULL) {
        *raw_distance_mm = raw;
    }

    if (raw == 0 || raw == 0xFFFFU || raw > 2000U) {
        s_status.last_distance_mm = raw;
        *distance_mm = raw;
        return set_result(VL53L0X_DIRECT_RESULT_OUT_OF_RANGE);
    }

    s_status.last_distance_mm = raw;
    *distance_mm = raw;
    return set_result(VL53L0X_DIRECT_RESULT_OK);
}

void vl53l0x_direct_get_bus_levels(int *sda_level, int *scl_level)
{
    if (sda_level != NULL) {
        *sda_level = gpio_get_level(BOARD_I2C_SDA_GPIO);
    }
    if (scl_level != NULL) {
        *scl_level = gpio_get_level(BOARD_I2C_SCL_GPIO);
    }
}

void vl53l0x_direct_get_status(vl53l0x_direct_status_t *status)
{
    if (status != NULL) {
        *status = s_status;
    }
}

const char *vl53l0x_direct_state_text(vl53l0x_direct_state_t state)
{
    switch (state) {
    case VL53L0X_DIRECT_STATE_RESET: return "RESET";
    case VL53L0X_DIRECT_STATE_I2C_READY: return "I2C_READY";
    case VL53L0X_DIRECT_STATE_DEVICE_FOUND: return "DEVICE_FOUND";
    case VL53L0X_DIRECT_STATE_READY: return "READY";
    case VL53L0X_DIRECT_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

const char *vl53l0x_direct_result_text(vl53l0x_direct_result_t result)
{
    switch (result) {
    case VL53L0X_DIRECT_RESULT_OK: return "OK";
    case VL53L0X_DIRECT_RESULT_OUT_OF_RANGE: return "OUT_OF_RANGE";
    case VL53L0X_DIRECT_RESULT_ADDR_NOT_FOUND: return "ADDR_NOT_FOUND";
    case VL53L0X_DIRECT_RESULT_MODEL_ID_READ_FAIL: return "MODEL_ID_READ_FAIL";
    case VL53L0X_DIRECT_RESULT_MODEL_ID_MISMATCH: return "MODEL_ID_MISMATCH";
    case VL53L0X_DIRECT_RESULT_CONFIG_FAIL: return "CONFIG_FAIL";
    case VL53L0X_DIRECT_RESULT_START_TIMEOUT: return "START_TIMEOUT";
    case VL53L0X_DIRECT_RESULT_MEASURE_TIMEOUT: return "MEASURE_TIMEOUT";
    case VL53L0X_DIRECT_RESULT_COMM_FAIL: return "COMM_FAIL";
    case VL53L0X_DIRECT_RESULT_NOT_READY: return "NOT_READY";
    case VL53L0X_DIRECT_RESULT_BAD_ARG: return "BAD_ARG";
    default: return "UNKNOWN";
    }
}

