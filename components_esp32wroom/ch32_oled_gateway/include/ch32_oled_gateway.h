#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_OLED_DEVICE_TYPE              0x01U
#define CH32_OLED_NODE_ID_MAX              10U
#define CH32_OLED_DEFAULT_NODE_ID          1U
#define CH32_OLED_CAN_BITRATE              50000U
#define CH32_OLED_DEFAULT_I2C_ADDRESS      0x3CU
#define CH32_OLED_CAN_FRAME_MAX_DLC        8U
#define CH32_OLED_WRITE_RAW_MAX_PAYLOAD    5U

#define CH32_OLED_I2C_CMD_PROBE            0x02U
#define CH32_OLED_I2C_CMD_WRITE_RAW        0x05U

#define CH32_OLED_CAN_ID_STATUS(node_id) \
    (0x100U + (uint32_t)(node_id))
#define CH32_OLED_CAN_ID_I2C_CMD(node_id) \
    (0x200U + (uint32_t)(node_id))
#define CH32_OLED_CAN_ID_ACK(node_id) \
    (0x500U + (uint32_t)(node_id))
#define CH32_OLED_CAN_ID_HELLO(node_id) \
    (0x700U + (uint32_t)(node_id))

typedef struct {
    uint8_t node_id;
    uint8_t device_type;
    uint8_t fw_version;
    uint8_t capability_flags;
} ch32_oled_hello_t;

typedef struct {
    uint8_t command;
    bool result;
    uint8_t node_id;
    uint8_t device_type;
} ch32_oled_ack_t;

bool ch32_oled_build_probe(uint8_t i2c_address, uint8_t data[2]);

bool ch32_oled_build_write_raw(uint8_t i2c_address,
                               const uint8_t *payload,
                               uint8_t payload_len,
                               uint8_t data[CH32_OLED_CAN_FRAME_MAX_DLC],
                               uint8_t *out_dlc);

bool ch32_oled_parse_hello(uint32_t can_id,
                           const uint8_t *data,
                           uint8_t dlc,
                           uint8_t expected_node_id,
                           ch32_oled_hello_t *hello);

bool ch32_oled_parse_ack(uint32_t can_id,
                         const uint8_t *data,
                         uint8_t dlc,
                         uint8_t expected_node_id,
                         uint8_t expected_command,
                         ch32_oled_ack_t *ack);

#ifdef __cplusplus
}
#endif

