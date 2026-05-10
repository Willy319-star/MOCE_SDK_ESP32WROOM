#include "driver_mpu6050.h"

#include <string.h>

#include "bsp_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1a
#define MPU6050_REG_GYRO_CONFIG     0x1b
#define MPU6050_REG_ACCEL_CONFIG    0x1c
#define MPU6050_REG_ACCEL_XOUT_H    0x3b
#define MPU6050_REG_PWR_MGMT_1      0x6b
#define MPU6050_REG_WHO_AM_I        0x75

#define MPU6050_RESET_BIT           0x80
#define MPU6050_SLEEP_BIT           0x40
#define MPU6050_CLKSEL_PLL_XGYRO    0x01

#define MPU6050_TIMEOUT_MS          1000
#define MPU6050_RESET_DELAY_MS      100
#define MPU6050_STARTUP_DELAY_MS    20

static const char *TAG = "driver_mpu6050";

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_inited = false;
static driver_mpu6050_config_t s_config;

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static float accel_lsb_per_g(driver_mpu6050_accel_range_t range)
{
    static const float scale[] = {
        [DRIVER_MPU6050_ACCEL_RANGE_2G] = 16384.0f,
        [DRIVER_MPU6050_ACCEL_RANGE_4G] = 8192.0f,
        [DRIVER_MPU6050_ACCEL_RANGE_8G] = 4096.0f,
        [DRIVER_MPU6050_ACCEL_RANGE_16G] = 2048.0f,
    };

    return range <= DRIVER_MPU6050_ACCEL_RANGE_16G ? scale[range] : scale[DRIVER_MPU6050_ACCEL_RANGE_2G];
}

static float gyro_lsb_per_dps(driver_mpu6050_gyro_range_t range)
{
    static const float scale[] = {
        [DRIVER_MPU6050_GYRO_RANGE_250DPS] = 131.0f,
        [DRIVER_MPU6050_GYRO_RANGE_500DPS] = 65.5f,
        [DRIVER_MPU6050_GYRO_RANGE_1000DPS] = 32.8f,
        [DRIVER_MPU6050_GYRO_RANGE_2000DPS] = 16.4f,
    };

    return range <= DRIVER_MPU6050_GYRO_RANGE_2000DPS ? scale[range] : scale[DRIVER_MPU6050_GYRO_RANGE_250DPS];
}

static bool config_is_valid(const driver_mpu6050_config_t *config)
{
    return config != NULL &&
           (config->i2c_address == DRIVER_MPU6050_I2C_ADDR_AD0_LOW ||
            config->i2c_address == DRIVER_MPU6050_I2C_ADDR_AD0_HIGH) &&
           config->accel_range <= DRIVER_MPU6050_ACCEL_RANGE_16G &&
           config->gyro_range <= DRIVER_MPU6050_GYRO_RANGE_2000DPS &&
           config->dlpf <= DRIVER_MPU6050_DLPF_BW_5HZ;
}

static esp_err_t write_reg_byte(uint8_t reg, uint8_t value)
{
    if (!s_inited || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return bsp_i2c_write_reg_byte(s_dev, reg, value, MPU6050_TIMEOUT_MS);
}

esp_err_t driver_mpu6050_probe(uint8_t i2c_address)
{
    if (i2c_address != DRIVER_MPU6050_I2C_ADDR_AD0_LOW && i2c_address != DRIVER_MPU6050_I2C_ADDR_AD0_HIGH) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_i2c_probe(i2c_address, MPU6050_TIMEOUT_MS);
}

esp_err_t driver_mpu6050_init(const driver_mpu6050_config_t *config)
{
    if (!config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = bsp_i2c_add_device_7bit(config->i2c_address, config->scl_speed_hz, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    s_config = *config;
    s_inited = true;

    err = driver_mpu6050_reset();
    if (err != ESP_OK) {
        goto fail;
    }

    vTaskDelay(pdMS_TO_TICKS(MPU6050_STARTUP_DELAY_MS));

    uint8_t who_am_i = 0;
    err = driver_mpu6050_read_who_am_i(&who_am_i);
    if (err != ESP_OK) {
        goto fail;
    }
    if (who_am_i != DRIVER_MPU6050_WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I: 0x%02x", who_am_i);
        err = ESP_ERR_NOT_FOUND;
        goto fail;
    }

    err = driver_mpu6050_set_sleep(false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wake failed: %s", esp_err_to_name(err));
        goto fail;
    }
    err = driver_mpu6050_set_dlpf(config->dlpf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set dlpf failed: %s", esp_err_to_name(err));
        goto fail;
    }
    err = driver_mpu6050_set_sample_rate_divider(config->sample_rate_divider);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set sample rate failed: %s", esp_err_to_name(err));
        goto fail;
    }
    err = driver_mpu6050_set_accel_range(config->accel_range);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set accel range failed: %s", esp_err_to_name(err));
        goto fail;
    }
    err = driver_mpu6050_set_gyro_range(config->gyro_range);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set gyro range failed: %s", esp_err_to_name(err));
        goto fail;
    }

    ESP_LOGI(TAG, "MPU6050 initialized at I2C address 0x%02x", config->i2c_address);
    return ESP_OK;

fail:
    if (s_dev != NULL) {
        bsp_i2c_remove_device(s_dev);
        s_dev = NULL;
    }
    s_inited = false;
    memset(&s_config, 0, sizeof(s_config));
    return err;
}

esp_err_t driver_mpu6050_init_default(void)
{
    driver_mpu6050_config_t config = {
        .i2c_address = DRIVER_MPU6050_I2C_ADDR_AD0_LOW,
        .scl_speed_hz = 400000,
        .accel_range = DRIVER_MPU6050_ACCEL_RANGE_2G,
        .gyro_range = DRIVER_MPU6050_GYRO_RANGE_250DPS,
        .dlpf = DRIVER_MPU6050_DLPF_BW_44HZ,
        .sample_rate_divider = 9,
    };

    return driver_mpu6050_init(&config);
}

esp_err_t driver_mpu6050_deinit(void)
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
    memset(&s_config, 0, sizeof(s_config));
    return ESP_OK;
}

bool driver_mpu6050_is_initialized(void)
{
    return s_inited;
}

esp_err_t driver_mpu6050_read_who_am_i(uint8_t *who_am_i)
{
    if (!s_inited || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (who_am_i == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return bsp_i2c_read_reg_byte(s_dev, MPU6050_REG_WHO_AM_I, who_am_i, MPU6050_TIMEOUT_MS);
}

esp_err_t driver_mpu6050_reset(void)
{
    if (!s_inited && s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_PWR_MGMT_1, MPU6050_RESET_BIT, MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(MPU6050_RESET_DELAY_MS));
    return bsp_i2c_write_reg_byte(s_dev, MPU6050_REG_PWR_MGMT_1, MPU6050_CLKSEL_PLL_XGYRO, MPU6050_TIMEOUT_MS);
}

esp_err_t driver_mpu6050_set_sleep(bool enable)
{
    uint8_t value = enable ? (MPU6050_SLEEP_BIT | MPU6050_CLKSEL_PLL_XGYRO) : MPU6050_CLKSEL_PLL_XGYRO;
    return write_reg_byte(MPU6050_REG_PWR_MGMT_1, value);
}

esp_err_t driver_mpu6050_set_accel_range(driver_mpu6050_accel_range_t range)
{
    if (range > DRIVER_MPU6050_ACCEL_RANGE_16G) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = write_reg_byte(MPU6050_REG_ACCEL_CONFIG, ((uint8_t)range) << 3);
    if (err == ESP_OK) {
        s_config.accel_range = range;
    }
    return err;
}

esp_err_t driver_mpu6050_set_gyro_range(driver_mpu6050_gyro_range_t range)
{
    if (range > DRIVER_MPU6050_GYRO_RANGE_2000DPS) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = write_reg_byte(MPU6050_REG_GYRO_CONFIG, ((uint8_t)range) << 3);
    if (err == ESP_OK) {
        s_config.gyro_range = range;
    }
    return err;
}

esp_err_t driver_mpu6050_set_dlpf(driver_mpu6050_dlpf_t dlpf)
{
    if (dlpf > DRIVER_MPU6050_DLPF_BW_5HZ) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = write_reg_byte(MPU6050_REG_CONFIG, (uint8_t)dlpf);
    if (err == ESP_OK) {
        s_config.dlpf = dlpf;
    }
    return err;
}

esp_err_t driver_mpu6050_set_sample_rate_divider(uint8_t divider)
{
    esp_err_t err = write_reg_byte(MPU6050_REG_SMPLRT_DIV, divider);
    if (err == ESP_OK) {
        s_config.sample_rate_divider = divider;
    }
    return err;
}

esp_err_t driver_mpu6050_read_raw(driver_mpu6050_raw_data_t *data)
{
    if (!s_inited || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[14];
    esp_err_t err = bsp_i2c_read_reg(s_dev, MPU6050_REG_ACCEL_XOUT_H, buffer, sizeof(buffer), MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    data->accel_x = read_i16_be(&buffer[0]);
    data->accel_y = read_i16_be(&buffer[2]);
    data->accel_z = read_i16_be(&buffer[4]);
    data->temperature = read_i16_be(&buffer[6]);
    data->gyro_x = read_i16_be(&buffer[8]);
    data->gyro_y = read_i16_be(&buffer[10]);
    data->gyro_z = read_i16_be(&buffer[12]);
    return ESP_OK;
}

esp_err_t driver_mpu6050_read(driver_mpu6050_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    driver_mpu6050_raw_data_t raw;
    esp_err_t err = driver_mpu6050_read_raw(&raw);
    if (err != ESP_OK) {
        return err;
    }

    float accel_scale = accel_lsb_per_g(s_config.accel_range);
    float gyro_scale = gyro_lsb_per_dps(s_config.gyro_range);

    data->accel_x_g = (float)raw.accel_x / accel_scale;
    data->accel_y_g = (float)raw.accel_y / accel_scale;
    data->accel_z_g = (float)raw.accel_z / accel_scale;
    data->temperature_c = ((float)raw.temperature / 340.0f) + 36.53f;
    data->gyro_x_dps = (float)raw.gyro_x / gyro_scale;
    data->gyro_y_dps = (float)raw.gyro_y / gyro_scale;
    data->gyro_z_dps = (float)raw.gyro_z / gyro_scale;
    return ESP_OK;
}
