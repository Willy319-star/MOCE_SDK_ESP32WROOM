#include "driver_tof2000c_vl53l0x.h"

#include <string.h>

#include "bsp_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define VL53L0X_REG_SYSRANGE_START                        0x00
#define VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG                0x01
#define VL53L0X_REG_SYSTEM_INTERMEASUREMENT_PERIOD        0x04
#define VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO          0x0a
#define VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR                0x0b
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS               0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS                   0x14
#define VL53L0X_REG_RESULT_RANGE_MM                       0x1e
#define VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH               0x84
#define VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0      0xb0
#define VL53L0X_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET      0x4f
#define VL53L0X_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD   0x4e
#define VL53L0X_REG_GLOBAL_CONFIG_REF_EN_START_SELECT     0xb6
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID               0xc0
#define VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV      0x89
#define VL53L0X_REG_MSRC_CONFIG_CONTROL                   0x60
#define VL53L0X_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD       0x70
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD         0x50
#define VL53L0X_REG_MSRC_CONFIG_TIMEOUT_MACROP            0x46
#define VL53L0X_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI    0x51
#define VL53L0X_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI  0x71
#define VL53L0X_REG_OSC_CALIBRATE_VAL                     0xf8

#define VL53L0X_TIMEOUT_MS_DEFAULT        2000
#define VL53L0X_TIMEOUT_MS_I2C            1000
#define VL53L0X_TIMING_BUDGET_US_DEFAULT  33000
#define VL53L0X_START_OVERHEAD_US         1910
#define VL53L0X_END_OVERHEAD_US           960
#define VL53L0X_MSRC_OVERHEAD_US          660
#define VL53L0X_TCC_OVERHEAD_US           590
#define VL53L0X_DSS_OVERHEAD_US           690
#define VL53L0X_PRE_RANGE_OVERHEAD_US     660
#define VL53L0X_FINAL_RANGE_OVERHEAD_US   550
#define VL53L0X_MIN_TIMING_BUDGET_US      20000

static const char *TAG = "driver_tof2000c_vl53l0x";

typedef struct {
    bool tcc;
    bool msrc;
    bool dss;
    bool pre_range;
    bool final_range;
} sequence_enables_t;

typedef struct {
    uint16_t pre_range_vcsel_period_pclks;
    uint16_t final_range_vcsel_period_pclks;
    uint16_t msrc_dss_tcc_mclks;
    uint32_t msrc_dss_tcc_us;
    uint16_t pre_range_mclks;
    uint32_t pre_range_us;
    uint16_t final_range_mclks;
    uint32_t final_range_us;
} sequence_timeouts_t;

static i2c_master_dev_handle_t s_dev;
static bool s_inited;
static uint8_t s_stop_variable;
static uint16_t s_timeout_ms = VL53L0X_TIMEOUT_MS_DEFAULT;
static bool s_timeout_occurred;

static esp_err_t read_reg(uint8_t reg, uint8_t *value)
{
    if (s_dev == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_i2c_read_reg_byte(s_dev, reg, value, VL53L0X_TIMEOUT_MS_I2C);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return bsp_i2c_write_reg_byte(s_dev, reg, value, VL53L0X_TIMEOUT_MS_I2C);
}

static esp_err_t read_multi(uint8_t reg, uint8_t *data, size_t len)
{
    if (s_dev == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_i2c_read_reg(s_dev, reg, data, len, VL53L0X_TIMEOUT_MS_I2C);
}

static esp_err_t write_multi(uint8_t reg, const uint8_t *data, size_t len)
{
    if (s_dev == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_i2c_write_reg(s_dev, reg, data, len, VL53L0X_TIMEOUT_MS_I2C);
}

static esp_err_t read_u16(uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    esp_err_t err = read_multi(reg, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }
    *value = ((uint16_t)data[0] << 8) | data[1];
    return ESP_OK;
}

static esp_err_t write_u16(uint8_t reg, uint16_t value)
{
    uint8_t data[2] = { value >> 8, value & 0xff };
    return write_multi(reg, data, sizeof(data));
}

static esp_err_t write_u32(uint8_t reg, uint32_t value)
{
    uint8_t data[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)value,
    };
    return write_multi(reg, data, sizeof(data));
}

static uint32_t timeout_start_ticks(void)
{
    return xTaskGetTickCount();
}

static bool timeout_expired(uint32_t start_ticks)
{
    if (s_timeout_ms == 0) {
        return false;
    }
    return (xTaskGetTickCount() - start_ticks) > pdMS_TO_TICKS(s_timeout_ms);
}

static uint8_t decode_vcsel_period(uint8_t reg_val)
{
    return (reg_val + 1) << 1;
}

static uint32_t calc_macro_period_ns(uint8_t vcsel_period_pclks)
{
    return ((uint32_t)2304 * vcsel_period_pclks * 1655 + 500) / 1000;
}

static uint32_t timeout_mclks_to_us(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks)
{
    uint32_t macro_period_ns = calc_macro_period_ns(vcsel_period_pclks);
    return ((timeout_period_mclks * macro_period_ns) + 500) / 1000;
}

static uint32_t timeout_us_to_mclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks)
{
    uint32_t macro_period_ns = calc_macro_period_ns(vcsel_period_pclks);
    return ((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns;
}

static uint16_t decode_timeout(uint16_t reg_val)
{
    return (uint16_t)((reg_val & 0x00ff) << ((reg_val & 0xff00) >> 8)) + 1;
}

static uint16_t encode_timeout(uint32_t timeout_mclks)
{
    if (timeout_mclks == 0) {
        return 0;
    }

    uint32_t ls_byte = timeout_mclks - 1;
    uint16_t ms_byte = 0;
    while ((ls_byte & 0xffffff00) > 0) {
        ls_byte >>= 1;
        ms_byte++;
    }
    return (ms_byte << 8) | (ls_byte & 0xff);
}

static esp_err_t get_spad_info(uint8_t *count, bool *type_is_aperture)
{
    uint8_t value = 0;
    esp_err_t err;

    err = write_reg(0x80, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x00);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x06);
    if (err != ESP_OK) return err;
    err = read_reg(0x83, &value);
    if (err != ESP_OK) return err;
    err = write_reg(0x83, value | 0x04);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x07);
    if (err != ESP_OK) return err;
    err = write_reg(0x81, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0x80, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0x94, 0x6b);
    if (err != ESP_OK) return err;
    err = write_reg(0x83, 0x00);
    if (err != ESP_OK) return err;

    uint32_t start = timeout_start_ticks();
    do {
        err = read_reg(0x83, &value);
        if (err != ESP_OK) return err;
        if (timeout_expired(start)) {
            s_timeout_occurred = true;
            ESP_LOGE(TAG, "timeout waiting for SPAD info ready, reg 0x83=0x%02x", value);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (value == 0x00);

    err = write_reg(0x83, 0x01);
    if (err != ESP_OK) return err;
    err = read_reg(0x92, &value);
    if (err != ESP_OK) return err;

    *count = value & 0x7f;
    *type_is_aperture = (value >> 7) & 0x01;

    err = write_reg(0x81, 0x00);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x06);
    if (err != ESP_OK) return err;
    err = read_reg(0x83, &value);
    if (err != ESP_OK) return err;
    err = write_reg(0x83, value & ~0x04);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00);
    if (err != ESP_OK) return err;
    return write_reg(0x80, 0x00);
}

static esp_err_t get_sequence_step_enables(sequence_enables_t *enables)
{
    uint8_t sequence_config = 0;
    esp_err_t err = read_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, &sequence_config);
    if (err != ESP_OK) {
        return err;
    }

    enables->tcc = (sequence_config >> 4) & 0x1;
    enables->dss = (sequence_config >> 3) & 0x1;
    enables->msrc = (sequence_config >> 2) & 0x1;
    enables->pre_range = (sequence_config >> 6) & 0x1;
    enables->final_range = (sequence_config >> 7) & 0x1;
    return ESP_OK;
}

static esp_err_t get_sequence_step_timeouts(const sequence_enables_t *enables, sequence_timeouts_t *timeouts)
{
    uint8_t val = 0;
    uint16_t raw = 0;
    esp_err_t err;

    err = read_reg(VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD, &val);
    if (err != ESP_OK) return err;
    timeouts->pre_range_vcsel_period_pclks = decode_vcsel_period(val);

    err = read_reg(VL53L0X_REG_MSRC_CONFIG_TIMEOUT_MACROP, &val);
    if (err != ESP_OK) return err;
    timeouts->msrc_dss_tcc_mclks = val + 1;
    timeouts->msrc_dss_tcc_us = timeout_mclks_to_us(timeouts->msrc_dss_tcc_mclks,
                                                    timeouts->pre_range_vcsel_period_pclks);

    err = read_u16(VL53L0X_REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI, &raw);
    if (err != ESP_OK) return err;
    timeouts->pre_range_mclks = decode_timeout(raw);
    timeouts->pre_range_us = timeout_mclks_to_us(timeouts->pre_range_mclks,
                                                 timeouts->pre_range_vcsel_period_pclks);

    err = read_reg(VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD, &val);
    if (err != ESP_OK) return err;
    timeouts->final_range_vcsel_period_pclks = decode_vcsel_period(val);

    err = read_u16(VL53L0X_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, &raw);
    if (err != ESP_OK) return err;
    timeouts->final_range_mclks = decode_timeout(raw);
    if (enables->pre_range) {
        timeouts->final_range_mclks -= timeouts->pre_range_mclks;
    }
    timeouts->final_range_us = timeout_mclks_to_us(timeouts->final_range_mclks,
                                                   timeouts->final_range_vcsel_period_pclks);
    return ESP_OK;
}

static esp_err_t set_measurement_timing_budget(uint32_t budget_us)
{
    if (budget_us < VL53L0X_MIN_TIMING_BUDGET_US) {
        return ESP_ERR_INVALID_ARG;
    }

    sequence_enables_t enables;
    sequence_timeouts_t timeouts;
    esp_err_t err = get_sequence_step_enables(&enables);
    if (err != ESP_OK) return err;
    err = get_sequence_step_timeouts(&enables, &timeouts);
    if (err != ESP_OK) return err;

    uint32_t used_budget_us = VL53L0X_START_OVERHEAD_US + VL53L0X_END_OVERHEAD_US;
    if (enables.tcc) {
        used_budget_us += timeouts.msrc_dss_tcc_us + VL53L0X_TCC_OVERHEAD_US;
    }
    if (enables.dss) {
        used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + VL53L0X_DSS_OVERHEAD_US);
    } else if (enables.msrc) {
        used_budget_us += timeouts.msrc_dss_tcc_us + VL53L0X_MSRC_OVERHEAD_US;
    }
    if (enables.pre_range) {
        used_budget_us += timeouts.pre_range_us + VL53L0X_PRE_RANGE_OVERHEAD_US;
    }
    if (!enables.final_range) {
        return ESP_OK;
    }
    if (used_budget_us > budget_us) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t final_range_timeout_us = budget_us - used_budget_us - VL53L0X_FINAL_RANGE_OVERHEAD_US;
    uint32_t final_range_timeout_mclks = timeout_us_to_mclks(final_range_timeout_us,
                                                             timeouts.final_range_vcsel_period_pclks);
    if (enables.pre_range) {
        final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    return write_u16(VL53L0X_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
                     encode_timeout(final_range_timeout_mclks));
}

static esp_err_t perform_single_ref_calibration(uint8_t vhv_init_byte)
{
    esp_err_t err = write_reg(VL53L0X_REG_SYSRANGE_START, 0x01 | vhv_init_byte);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = 0;
    uint32_t start = timeout_start_ticks();
    do {
        err = read_reg(VL53L0X_REG_RESULT_INTERRUPT_STATUS, &value);
        if (err != ESP_OK) {
            return err;
        }
        if (timeout_expired(start)) {
            s_timeout_occurred = true;
            ESP_LOGE(TAG,
                     "timeout during ref calibration vhv=0x%02x, interrupt status=0x%02x",
                     vhv_init_byte,
                     value);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((value & 0x07) == 0);

    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (err != ESP_OK) {
        return err;
    }
    return write_reg(VL53L0X_REG_SYSRANGE_START, 0x00);
}

static esp_err_t apply_tuning_settings(void)
{
    esp_err_t err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x09, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x10, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x11, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x24, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x25, 0xff); if (err != ESP_OK) return err;
    err = write_reg(0x75, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x4e, 0x2c); if (err != ESP_OK) return err;
    err = write_reg(0x48, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x30, 0x20); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x30, 0x09); if (err != ESP_OK) return err;
    err = write_reg(0x54, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x31, 0x04); if (err != ESP_OK) return err;
    err = write_reg(0x32, 0x03); if (err != ESP_OK) return err;
    err = write_reg(0x40, 0x83); if (err != ESP_OK) return err;
    err = write_reg(0x46, 0x25); if (err != ESP_OK) return err;
    err = write_reg(0x60, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x27, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x50, 0x06); if (err != ESP_OK) return err;
    err = write_reg(0x51, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x52, 0x96); if (err != ESP_OK) return err;
    err = write_reg(0x56, 0x08); if (err != ESP_OK) return err;
    err = write_reg(0x57, 0x30); if (err != ESP_OK) return err;
    err = write_reg(0x61, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x62, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x64, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x65, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x66, 0xa0); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x22, 0x32); if (err != ESP_OK) return err;
    err = write_reg(0x47, 0x14); if (err != ESP_OK) return err;
    err = write_reg(0x49, 0xff); if (err != ESP_OK) return err;
    err = write_reg(0x4a, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x7a, 0x0a); if (err != ESP_OK) return err;
    err = write_reg(0x7b, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x78, 0x21); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x23, 0x34); if (err != ESP_OK) return err;
    err = write_reg(0x42, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x44, 0xff); if (err != ESP_OK) return err;
    err = write_reg(0x45, 0x26); if (err != ESP_OK) return err;
    err = write_reg(0x46, 0x05); if (err != ESP_OK) return err;
    err = write_reg(0x40, 0x40); if (err != ESP_OK) return err;
    err = write_reg(0x0e, 0x06); if (err != ESP_OK) return err;
    err = write_reg(0x20, 0x1a); if (err != ESP_OK) return err;
    err = write_reg(0x43, 0x40); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x34, 0x03); if (err != ESP_OK) return err;
    err = write_reg(0x35, 0x44); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x31, 0x04); if (err != ESP_OK) return err;
    err = write_reg(0x4b, 0x09); if (err != ESP_OK) return err;
    err = write_reg(0x4c, 0x05); if (err != ESP_OK) return err;
    err = write_reg(0x4d, 0x04); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x44, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x45, 0x20); if (err != ESP_OK) return err;
    err = write_reg(0x47, 0x08); if (err != ESP_OK) return err;
    err = write_reg(0x48, 0x28); if (err != ESP_OK) return err;
    err = write_reg(0x67, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x70, 0x04); if (err != ESP_OK) return err;
    err = write_reg(0x71, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x72, 0xfe); if (err != ESP_OK) return err;
    err = write_reg(0x76, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x77, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x0d, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x80, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x01, 0xf8); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x8e, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    return write_reg(0x80, 0x00);
}

void driver_tof2000c_vl53l0x_get_default_config(driver_tof2000c_vl53l0x_config_t *config)
{
    if (config == NULL) {
        return;
    }
    *config = (driver_tof2000c_vl53l0x_config_t) {
        .i2c_address = DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT,
        .scl_speed_hz = 400000,
        .timeout_ms = VL53L0X_TIMEOUT_MS_DEFAULT,
        .measurement_timing_budget_us = VL53L0X_TIMING_BUDGET_US_DEFAULT,
    };
}

esp_err_t driver_tof2000c_vl53l0x_probe(uint8_t i2c_address)
{
    return bsp_i2c_probe(i2c_address, VL53L0X_TIMEOUT_MS_I2C);
}

esp_err_t driver_tof2000c_vl53l0x_init(const driver_tof2000c_vl53l0x_config_t *config)
{
    if (config == NULL || config->i2c_address > 0x7f) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = bsp_i2c_add_device_7bit(config->i2c_address, config->scl_speed_hz, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    s_timeout_ms = config->timeout_ms;
    s_timeout_occurred = false;

    uint8_t model_id = 0;
    err = read_reg(VL53L0X_REG_IDENTIFICATION_MODEL_ID, &model_id);
    if (err != ESP_OK) {
        goto fail;
    }
    if (model_id != DRIVER_TOF2000C_VL53L0X_MODEL_ID) {
        ESP_LOGE(TAG, "unexpected model id: 0x%02x", model_id);
        err = ESP_ERR_NOT_FOUND;
        goto fail;
    }
    ESP_LOGI(TAG, "model id ok: 0x%02x", model_id);

    uint8_t value = 0;
    err = read_reg(VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &value);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, value | 0x01);
    if (err != ESP_OK) goto fail;

    err = write_reg(0x88, 0x00);
    if (err != ESP_OK) goto fail;
    err = write_reg(0x80, 0x01);
    if (err != ESP_OK) goto fail;
    err = write_reg(0xff, 0x01);
    if (err != ESP_OK) goto fail;
    err = write_reg(0x00, 0x00);
    if (err != ESP_OK) goto fail;
    err = read_reg(0x91, &s_stop_variable);
    if (err != ESP_OK) goto fail;
    err = write_reg(0x00, 0x01);
    if (err != ESP_OK) goto fail;
    err = write_reg(0xff, 0x00);
    if (err != ESP_OK) goto fail;
    err = write_reg(0x80, 0x00);
    if (err != ESP_OK) goto fail;

    err = read_reg(VL53L0X_REG_MSRC_CONFIG_CONTROL, &value);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_MSRC_CONFIG_CONTROL, value | 0x12);
    if (err != ESP_OK) goto fail;

    err = write_u16(VL53L0X_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32);
    if (err != ESP_OK) goto fail;

    err = write_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0xff);
    if (err != ESP_OK) goto fail;

    uint8_t spad_count = 0;
    bool spad_type_is_aperture = false;
    ESP_LOGI(TAG, "reading SPAD info");
    err = get_spad_info(&spad_count, &spad_type_is_aperture);
    if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "SPAD count=%u aperture=%d", spad_count, spad_type_is_aperture);

    uint8_t ref_spad_map[6] = {0};
    err = read_multi(VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, sizeof(ref_spad_map));
    if (err != ESP_OK) goto fail;

    err = write_reg(0xff, 0x01);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2c);
    if (err != ESP_OK) goto fail;
    err = write_reg(0xff, 0x00);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_GLOBAL_CONFIG_REF_EN_START_SELECT, 0xb4);
    if (err != ESP_OK) goto fail;

    uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0;
    uint8_t spads_enabled = 0;
    for (uint8_t i = 0; i < 48; i++) {
        if (i < first_spad_to_enable || spads_enabled == spad_count) {
            ref_spad_map[i / 8] &= ~(1 << (i % 8));
        } else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1) {
            spads_enabled++;
        }
    }
    err = write_multi(VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, sizeof(ref_spad_map));
    if (err != ESP_OK) goto fail;

    ESP_LOGI(TAG, "applying tuning settings");
    err = apply_tuning_settings();
    if (err != ESP_OK) goto fail;

    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    if (err != ESP_OK) goto fail;
    err = read_reg(VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH, &value);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH, value & ~0x10);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (err != ESP_OK) goto fail;

    err = write_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0xe8);
    if (err != ESP_OK) goto fail;

    ESP_LOGI(TAG, "setting timing budget %lu us", (unsigned long)config->measurement_timing_budget_us);
    err = set_measurement_timing_budget(config->measurement_timing_budget_us);
    if (err != ESP_OK) goto fail;

    err = write_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "performing VHV calibration");
    err = perform_single_ref_calibration(0x40);
    if (err != ESP_OK) goto fail;
    err = write_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "performing phase calibration");
    err = perform_single_ref_calibration(0x00);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "phase calibration timed out; continuing with VHV calibration only");
        write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
        write_reg(VL53L0X_REG_SYSRANGE_START, 0x00);
    }
    err = write_reg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0xe8);
    if (err != ESP_OK) goto fail;

    s_inited = true;
    ESP_LOGI(TAG, "TOF2000C/VL53L0X initialized at I2C address 0x%02x", config->i2c_address);
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
    if (s_dev != NULL) {
        bsp_i2c_remove_device(s_dev);
        s_dev = NULL;
    }
    s_inited = false;
    return err;
}

esp_err_t driver_tof2000c_vl53l0x_init_default(void)
{
    driver_tof2000c_vl53l0x_config_t config;
    driver_tof2000c_vl53l0x_get_default_config(&config);
    return driver_tof2000c_vl53l0x_init(&config);
}

esp_err_t driver_tof2000c_vl53l0x_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }
    esp_err_t err = bsp_i2c_remove_device(s_dev);
    if (err != ESP_OK) {
        return err;
    }
    s_dev = NULL;
    s_inited = false;
    return ESP_OK;
}

bool driver_tof2000c_vl53l0x_is_initialized(void)
{
    return s_inited;
}

esp_err_t driver_tof2000c_vl53l0x_read_model_id(uint8_t *model_id)
{
    if (!s_inited || model_id == NULL) {
        return model_id == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }
    return read_reg(VL53L0X_REG_IDENTIFICATION_MODEL_ID, model_id);
}

esp_err_t driver_tof2000c_vl53l0x_set_timeout(uint16_t timeout_ms)
{
    s_timeout_ms = timeout_ms;
    return ESP_OK;
}

bool driver_tof2000c_vl53l0x_timeout_occurred(void)
{
    bool occurred = s_timeout_occurred;
    s_timeout_occurred = false;
    return occurred;
}

esp_err_t driver_tof2000c_vl53l0x_start_continuous(uint32_t period_ms)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    err = write_reg(0x80, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x91, s_stop_variable); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x80, 0x00); if (err != ESP_OK) return err;
    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (err != ESP_OK) return err;

    if (period_ms != 0) {
        uint16_t osc_calibrate_val = 0;
        err = read_u16(VL53L0X_REG_OSC_CALIBRATE_VAL, &osc_calibrate_val);
        if (err != ESP_OK) return err;
        uint32_t period = period_ms;
        if (osc_calibrate_val != 0) {
            period *= osc_calibrate_val;
        }
        err = write_u32(VL53L0X_REG_SYSTEM_INTERMEASUREMENT_PERIOD, period);
        if (err != ESP_OK) return err;
        return write_reg(VL53L0X_REG_SYSRANGE_START, 0x04);
    }

    return write_reg(VL53L0X_REG_SYSRANGE_START, 0x02);
}

esp_err_t driver_tof2000c_vl53l0x_stop_continuous(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = write_reg(VL53L0X_REG_SYSRANGE_START, 0x01);
    if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x91, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x01); if (err != ESP_OK) return err;
    return write_reg(0xff, 0x00);
}

esp_err_t driver_tof2000c_vl53l0x_read_continuous(driver_tof2000c_vl53l0x_result_t *result)
{
    if (!s_inited || result == NULL) {
        return result == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }

    s_timeout_occurred = false;
    uint8_t status = 0;
    uint32_t start = timeout_start_ticks();
    do {
        esp_err_t err = read_reg(VL53L0X_REG_RESULT_INTERRUPT_STATUS, &status);
        if (err != ESP_OK) return err;
        if (timeout_expired(start)) {
            s_timeout_occurred = true;
            result->timeout = true;
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((status & 0x07) == 0);

    uint8_t range_status = 0;
    esp_err_t err = read_reg(VL53L0X_REG_RESULT_RANGE_STATUS, &range_status);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t distance_mm = 0;
    err = read_u16(VL53L0X_REG_RESULT_RANGE_MM, &distance_mm);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (err != ESP_OK) {
        return err;
    }

    result->range_status = (range_status >> 3) & 0x0f;
    result->distance_mm = distance_mm;
    result->timeout = false;
    return ESP_OK;
}

esp_err_t driver_tof2000c_vl53l0x_read_single(driver_tof2000c_vl53l0x_result_t *result)
{
    if (!s_inited || result == NULL) {
        return result == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    err = write_reg(0x80, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x91, s_stop_variable); if (err != ESP_OK) return err;
    err = write_reg(0x00, 0x01); if (err != ESP_OK) return err;
    err = write_reg(0xff, 0x00); if (err != ESP_OK) return err;
    err = write_reg(0x80, 0x00); if (err != ESP_OK) return err;
    err = write_reg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (err != ESP_OK) return err;

    err = write_reg(VL53L0X_REG_SYSRANGE_START, 0x01);
    if (err != ESP_OK) return err;

    uint8_t value = 0;
    uint32_t start = timeout_start_ticks();
    do {
        err = read_reg(VL53L0X_REG_SYSRANGE_START, &value);
        if (err != ESP_OK) return err;
        if (timeout_expired(start)) {
            s_timeout_occurred = true;
            result->timeout = true;
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (value & 0x01);

    return driver_tof2000c_vl53l0x_read_continuous(result);
}
