#include "ch32_mpu6050_gateway.h"

#include <stddef.h>
#include <string.h>

static bool node_id_is_valid(uint8_t node_id)
{
    return node_id > 0U && node_id <= CH32_MPU6050_NODE_ID_MAX;
}

bool ch32_mpu6050_build_probe(uint8_t i2c_address, uint8_t data[2])
{
    if (data == NULL || i2c_address > 0x7FU) {
        return false;
    }
    data[0] = CH32_MPU6050_I2C_CMD_PROBE;
    data[1] = i2c_address;
    return true;
}

bool ch32_mpu6050_build_write_reg(uint8_t i2c_address,
                                  uint8_t reg,
                                  uint8_t value,
                                  uint8_t data[5])
{
    if (data == NULL || i2c_address > 0x7FU) {
        return false;
    }
    data[0] = CH32_MPU6050_I2C_CMD_WRITE_REG;
    data[1] = i2c_address;
    data[2] = reg;
    data[3] = 1U;
    data[4] = value;
    return true;
}

bool ch32_mpu6050_build_read_regs(uint8_t i2c_address,
                                  uint8_t reg,
                                  uint8_t length,
                                  uint8_t request_id,
                                  uint8_t data[5])
{
    if (data == NULL || i2c_address > 0x7FU ||
        length == 0U || length > 32U) {
        return false;
    }
    data[0] = CH32_MPU6050_I2C_CMD_READ_REGS;
    data[1] = i2c_address;
    data[2] = reg;
    data[3] = length;
    data[4] = request_id;
    return true;
}

bool ch32_mpu6050_parse_hello(uint32_t can_id,
                              const uint8_t *data,
                              uint8_t dlc,
                              uint8_t expected_node_id,
                              ch32_mpu6050_hello_t *hello)
{
    if (data == NULL || hello == NULL || dlc < 4U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_MPU6050_CAN_ID_HELLO(expected_node_id) ||
        data[0] != CH32_MPU6050_DEVICE_TYPE ||
        data[1] != expected_node_id) {
        return false;
    }

    hello->device_type = data[0];
    hello->node_id = data[1];
    hello->fw_version = data[2];
    hello->capability_flags = data[3];
    return true;
}

bool ch32_mpu6050_parse_ack(uint32_t can_id,
                            const uint8_t *data,
                            uint8_t dlc,
                            uint8_t expected_node_id,
                            uint8_t expected_command,
                            ch32_mpu6050_ack_t *ack)
{
    if (data == NULL || ack == NULL || dlc < 4U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_MPU6050_CAN_ID_ACK(expected_node_id) ||
        data[0] != expected_command ||
        data[2] != expected_node_id ||
        data[3] != CH32_MPU6050_DEVICE_TYPE) {
        return false;
    }

    ack->command = data[0];
    ack->result = data[1] != 0U;
    ack->node_id = data[2];
    ack->device_type = data[3];
    return true;
}

bool ch32_mpu6050_parse_read_chunk(uint32_t can_id,
                                   const uint8_t *data,
                                   uint8_t dlc,
                                   uint8_t expected_node_id,
                                   ch32_mpu6050_read_chunk_t *chunk)
{
    if (data == NULL || chunk == NULL || dlc != 8U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_MPU6050_CAN_ID_STATUS(expected_node_id) ||
        data[0] != CH32_MPU6050_I2C_STATUS_READ_CHUNK ||
        data[3] == 0U || data[3] > 4U) {
        return false;
    }

    chunk->request_id = data[1];
    chunk->offset = data[2];
    chunk->length = data[3];
    memset(chunk->data, 0, sizeof(chunk->data));
    memcpy(chunk->data, &data[4], chunk->length);
    return true;
}

bool ch32_mpu6050_parse_read_done(uint32_t can_id,
                                  const uint8_t *data,
                                  uint8_t dlc,
                                  uint8_t expected_node_id,
                                  ch32_mpu6050_read_done_t *done)
{
    if (data == NULL || done == NULL || dlc != 8U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_MPU6050_CAN_ID_STATUS(expected_node_id) ||
        data[0] != CH32_MPU6050_I2C_STATUS_READ_DONE) {
        return false;
    }

    done->request_id = data[1];
    done->i2c_address = data[2];
    done->reg = data[3];
    done->requested_length = data[4];
    done->result = data[5] != 0U;
    done->processed_length = data[6];
    return true;
}

