#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_MPU6050_I2C_ADDR_AD0_LOW   0x68
#define DRIVER_MPU6050_I2C_ADDR_AD0_HIGH  0x69
#define DRIVER_MPU6050_I2C_ADDR_DEFAULT   DRIVER_MPU6050_I2C_ADDR_AD0_LOW
#define DRIVER_MPU6050_WHO_AM_I_VALUE     0x68
#define DRIVER_MPU6050_WHO_AM_I_VALUE_ALT 0x74

typedef enum {
    DRIVER_MPU6050_ACCEL_RANGE_2G = 0,
    DRIVER_MPU6050_ACCEL_RANGE_4G,
    DRIVER_MPU6050_ACCEL_RANGE_8G,
    DRIVER_MPU6050_ACCEL_RANGE_16G,
} driver_mpu6050_accel_range_t;

typedef enum {
    DRIVER_MPU6050_GYRO_RANGE_250DPS = 0,
    DRIVER_MPU6050_GYRO_RANGE_500DPS,
    DRIVER_MPU6050_GYRO_RANGE_1000DPS,
    DRIVER_MPU6050_GYRO_RANGE_2000DPS,
} driver_mpu6050_gyro_range_t;

typedef enum {
    DRIVER_MPU6050_DLPF_BW_260HZ = 0,
    DRIVER_MPU6050_DLPF_BW_184HZ,
    DRIVER_MPU6050_DLPF_BW_94HZ,
    DRIVER_MPU6050_DLPF_BW_44HZ,
    DRIVER_MPU6050_DLPF_BW_21HZ,
    DRIVER_MPU6050_DLPF_BW_10HZ,
    DRIVER_MPU6050_DLPF_BW_5HZ,
} driver_mpu6050_dlpf_t;

typedef struct {
    uint8_t i2c_address;
    uint32_t scl_speed_hz;
    driver_mpu6050_accel_range_t accel_range;
    driver_mpu6050_gyro_range_t gyro_range;
    driver_mpu6050_dlpf_t dlpf;
    uint8_t sample_rate_divider;
} driver_mpu6050_config_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temperature;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} driver_mpu6050_raw_data_t;

typedef struct {
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float temperature_c;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
} driver_mpu6050_data_t;

esp_err_t driver_mpu6050_init(const driver_mpu6050_config_t *config);
esp_err_t driver_mpu6050_init_default(void);
esp_err_t driver_mpu6050_deinit(void);
bool driver_mpu6050_is_initialized(void);

esp_err_t driver_mpu6050_probe(uint8_t i2c_address);
esp_err_t driver_mpu6050_read_who_am_i(uint8_t *who_am_i);
esp_err_t driver_mpu6050_reset(void);
esp_err_t driver_mpu6050_set_sleep(bool enable);
esp_err_t driver_mpu6050_set_accel_range(driver_mpu6050_accel_range_t range);
esp_err_t driver_mpu6050_set_gyro_range(driver_mpu6050_gyro_range_t range);
esp_err_t driver_mpu6050_set_dlpf(driver_mpu6050_dlpf_t dlpf);
esp_err_t driver_mpu6050_set_sample_rate_divider(uint8_t divider);

esp_err_t driver_mpu6050_read_raw(driver_mpu6050_raw_data_t *data);
esp_err_t driver_mpu6050_read(driver_mpu6050_data_t *data);

#ifdef __cplusplus
}
#endif
