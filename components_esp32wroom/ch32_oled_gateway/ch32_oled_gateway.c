#include "ch32_oled_gateway.h"

#include <stddef.h>
#include <string.h>

static bool node_id_is_valid(uint8_t node_id)
{
    return node_id > 0U && node_id <= CH32_OLED_NODE_ID_MAX;
}

bool ch32_oled_build_probe(uint8_t i2c_address, uint8_t data[2])
{
    if (data == NULL || i2c_address > 0x7FU) {
        return false;
    }

    data[0] = CH32_OLED_I2C_CMD_PROBE;
    data[1] = i2c_address;
    return true;
}

bool ch32_oled_build_write_raw(uint8_t i2c_address,
                               const uint8_t *payload,
                               uint8_t payload_len,
                               uint8_t data[CH32_OLED_CAN_FRAME_MAX_DLC],
                               uint8_t *out_dlc)
{
    if (payload == NULL || data == NULL || out_dlc == NULL ||
        i2c_address > 0x7FU || payload_len == 0U ||
        payload_len > CH32_OLED_WRITE_RAW_MAX_PAYLOAD) {
        return false;
    }

    memset(data, 0, CH32_OLED_CAN_FRAME_MAX_DLC);
    data[0] = CH32_OLED_I2C_CMD_WRITE_RAW;
    data[1] = i2c_address;
    data[2] = payload_len;
    memcpy(&data[3], payload, payload_len);
    *out_dlc = (uint8_t)(3U + payload_len);
    return true;
}

bool ch32_oled_parse_hello(uint32_t can_id,
                           const uint8_t *data,
                           uint8_t dlc,
                           uint8_t expected_node_id,
                           ch32_oled_hello_t *hello)
{
    if (data == NULL || hello == NULL || dlc < 4U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_OLED_CAN_ID_HELLO(expected_node_id) ||
        data[0] != CH32_OLED_DEVICE_TYPE ||
        data[1] != expected_node_id) {
        return false;
    }

    hello->device_type = data[0];
    hello->node_id = data[1];
    hello->fw_version = data[2];
    hello->capability_flags = data[3];
    return true;
}

bool ch32_oled_parse_ack(uint32_t can_id,
                         const uint8_t *data,
                         uint8_t dlc,
                         uint8_t expected_node_id,
                         uint8_t expected_command,
                         ch32_oled_ack_t *ack)
{
    if (data == NULL || ack == NULL || dlc < 4U ||
        !node_id_is_valid(expected_node_id) ||
        can_id != CH32_OLED_CAN_ID_ACK(expected_node_id) ||
        data[0] != expected_command ||
        data[2] != expected_node_id ||
        data[3] != CH32_OLED_DEVICE_TYPE) {
        return false;
    }

    ack->command = data[0];
    ack->result = data[1] != 0U;
    ack->node_id = data[2];
    ack->device_type = data[3];
    return true;
}

