#include "ch32_servo_gateway_final.h"

static void put_u16_le(uint8_t *data, uint8_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xFFU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
}

bool ch32_servo_build_angle_cmd(uint8_t channel, uint16_t angle_deg,
                                uint8_t data[CH32_SERVO_FRAME_DLC])
{
    if (data == NULL || channel >= CH32_SERVO_CHANNEL_COUNT) {
        return false;
    }

    if (angle_deg > 180U) {
        angle_deg = 180U;
    }

    for (uint8_t i = 0U; i < CH32_SERVO_FRAME_DLC; ++i) {
        data[i] = 0U;
    }

    data[0] = channel;
    put_u16_le(data, 1U, angle_deg);
    return true;
}

bool ch32_servo_parse_hello(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                            ch32_servo_hello_t *hello)
{
    if (data == NULL || hello == NULL || dlc < 4U) {
        return false;
    }

    uint8_t node_id = data[1];
    if (node_id == 0U || node_id > CH32_SERVO_NODE_ID_MAX) {
        return false;
    }
    if (can_id != CH32_SERVO_CAN_ID_HELLO(node_id)) {
        return false;
    }
    if (data[0] != CH32_SERVO_DEVICE_TYPE) {
        return false;
    }

    hello->node_id = node_id;
    hello->device_type = data[0];
    hello->fw_version = data[2];
    hello->capability_flags = data[3];
    return true;
}

bool ch32_servo_parse_ack(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                          uint8_t expected_node_id, uint8_t expected_channel,
                          ch32_servo_ack_t *ack)
{
    if (data == NULL || ack == NULL || dlc < 4U) {
        return false;
    }
    if (expected_node_id == 0U || expected_node_id > CH32_SERVO_NODE_ID_MAX) {
        return false;
    }
    if (expected_channel >= CH32_SERVO_CHANNEL_COUNT) {
        return false;
    }
    if (can_id != CH32_SERVO_CAN_ID_ACK(expected_node_id)) {
        return false;
    }
    if (data[0] != expected_channel || data[2] != expected_node_id ||
        data[3] != CH32_SERVO_DEVICE_TYPE) {
        return false;
    }

    ack->channel = data[0];
    ack->result = data[1] != 0U;
    ack->node_id = data[2];
    ack->device_type = data[3];
    return true;
}
