#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_SERVO_DEVICE_TYPE         0x03U
#define CH32_SERVO_NODE_ID_MAX         10U
#define CH32_SERVO_CHANNEL_COUNT       4U
#define CH32_SERVO_DEFAULT_NODE_ID     3U
#define CH32_SERVO_CAN_BITRATE         50000U
#define CH32_SERVO_FRAME_DLC           8U

#define CH32_SERVO_CAN_ID_CMD(node_id)   (0x400U + (uint32_t)(node_id))
#define CH32_SERVO_CAN_ID_ACK(node_id)   (0x500U + (uint32_t)(node_id))
#define CH32_SERVO_CAN_ID_HELLO(node_id) (0x700U + (uint32_t)(node_id))

typedef struct {
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
} ch32_servo_hello_t;

typedef struct {
    uint8_t channel;
    bool result;
    uint8_t node_id;
    uint8_t device_type;
} ch32_servo_ack_t;

bool ch32_servo_build_angle_cmd(uint8_t channel, uint16_t angle_deg,
                                uint8_t data[CH32_SERVO_FRAME_DLC]);
bool ch32_servo_parse_hello(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                            ch32_servo_hello_t *hello);
bool ch32_servo_parse_ack(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                          uint8_t expected_node_id, uint8_t expected_channel,
                          ch32_servo_ack_t *ack);

#ifdef __cplusplus
}
#endif
