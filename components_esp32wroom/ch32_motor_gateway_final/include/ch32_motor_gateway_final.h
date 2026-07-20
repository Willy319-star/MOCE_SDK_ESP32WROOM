#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_MOTOR_DEVICE_TYPE         0x02U
#define CH32_MOTOR_NODE_ID_MAX         10U
#define CH32_MOTOR_CHANNEL_COUNT       2U
#define CH32_MOTOR_DEFAULT_NODE_ID     2U
#define CH32_MOTOR_CAN_BITRATE         50000U
#define CH32_MOTOR_FRAME_DLC           8U
#define CH32_MOTOR_DUTY_MAX_PERMILLE   1000U
#define CH32_MOTOR_DIR_FORWARD         0U
#define CH32_MOTOR_DIR_REVERSE         1U

#define CH32_MOTOR_CAN_ID_CMD(node_id)   (0x300U + (uint32_t)(node_id))
#define CH32_MOTOR_CAN_ID_ACK(node_id)   (0x500U + (uint32_t)(node_id))
#define CH32_MOTOR_CAN_ID_HELLO(node_id) (0x700U + (uint32_t)(node_id))

typedef struct {
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
} ch32_motor_hello_t;

typedef struct {
    uint8_t channel;
    bool result;
    uint8_t node_id;
    uint8_t device_type;
} ch32_motor_ack_t;

bool ch32_motor_build_set_duty_cmd(uint8_t channel, uint16_t duty_permille,
                                   uint8_t direction,
                                   uint8_t data[CH32_MOTOR_FRAME_DLC]);
bool ch32_motor_build_stop_cmd(uint8_t channel, uint8_t data[CH32_MOTOR_FRAME_DLC]);
bool ch32_motor_parse_hello(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                            ch32_motor_hello_t *hello);
bool ch32_motor_parse_ack(uint32_t can_id, const uint8_t *data, uint8_t dlc,
                          uint8_t expected_node_id, uint8_t expected_channel,
                          ch32_motor_ack_t *ack);

#ifdef __cplusplus
}
#endif
