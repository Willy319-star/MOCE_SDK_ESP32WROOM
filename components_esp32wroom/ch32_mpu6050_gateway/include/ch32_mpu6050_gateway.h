#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_MPU6050_DEVICE_TYPE              0x01U
#define CH32_MPU6050_NODE_ID_MAX              10U
#define CH32_MPU6050_DEFAULT_NODE_ID          1U
#define CH32_MPU6050_CAN_BITRATE              50000U
#define CH32_MPU6050_DEFAULT_I2C_ADDRESS      0x68U

#define CH32_MPU6050_I2C_CMD_PROBE            0x02U
#define CH32_MPU6050_I2C_CMD_WRITE_REG        0x03U
#define CH32_MPU6050_I2C_CMD_READ_REGS        0x04U
#define CH32_MPU6050_I2C_STATUS_READ_CHUNK    0x05U
#define CH32_MPU6050_I2C_STATUS_READ_DONE     0x06U

#define CH32_MPU6050_CAN_ID_STATUS(node_id) \
    (0x100U + (uint32_t)(node_id))
#define CH32_MPU6050_CAN_ID_I2C_CMD(node_id) \
    (0x200U + (uint32_t)(node_id))
#define CH32_MPU6050_CAN_ID_ACK(node_id) \
    (0x500U + (uint32_t)(node_id))
#define CH32_MPU6050_CAN_ID_HELLO(node_id) \
    (0x700U + (uint32_t)(node_id))

typedef struct {
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
} ch32_mpu6050_hello_t;

typedef struct {
    uint8_t command;
    bool result;
    uint8_t node_id;
    uint8_t device_type;
} ch32_mpu6050_ack_t;

typedef struct {
    uint8_t request_id;
    uint8_t offset;
    uint8_t length;
    uint8_t data[4];
} ch32_mpu6050_read_chunk_t;

typedef struct {
    uint8_t request_id;
    uint8_t i2c_address;
    uint8_t reg;
    uint8_t requested_length;
    bool result;
    uint8_t processed_length;
} ch32_mpu6050_read_done_t;

bool ch32_mpu6050_build_probe(uint8_t i2c_address, uint8_t data[2]);

bool ch32_mpu6050_build_write_reg(uint8_t i2c_address,
                                  uint8_t reg,
                                  uint8_t value,
                                  uint8_t data[5]);

bool ch32_mpu6050_build_read_regs(uint8_t i2c_address,
                                  uint8_t reg,
                                  uint8_t length,
                                  uint8_t request_id,
                                  uint8_t data[5]);

bool ch32_mpu6050_parse_hello(uint32_t can_id,
                              const uint8_t *data,
                              uint8_t dlc,
                              uint8_t expected_node_id,
                              ch32_mpu6050_hello_t *hello);

bool ch32_mpu6050_parse_ack(uint32_t can_id,
                            const uint8_t *data,
                            uint8_t dlc,
                            uint8_t expected_node_id,
                            uint8_t expected_command,
                            ch32_mpu6050_ack_t *ack);

bool ch32_mpu6050_parse_read_chunk(uint32_t can_id,
                                   const uint8_t *data,
                                   uint8_t dlc,
                                   uint8_t expected_node_id,
                                   ch32_mpu6050_read_chunk_t *chunk);

bool ch32_mpu6050_parse_read_done(uint32_t can_id,
                                  const uint8_t *data,
                                  uint8_t dlc,
                                  uint8_t expected_node_id,
                                  ch32_mpu6050_read_done_t *done);

#ifdef __cplusplus
}
#endif

