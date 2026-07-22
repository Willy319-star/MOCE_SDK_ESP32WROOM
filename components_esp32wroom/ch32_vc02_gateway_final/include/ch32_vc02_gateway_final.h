#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_VC02_CAN_BITRATE_HZ      50000U
#define CH32_VC02_UART_BAUD           115200U
#define CH32_VC02_CAN_ID_PING         0x730U
#define CH32_VC02_CAN_ID_STATUS       0x732U
#define CH32_VC02_CAN_ID_UART_RX      0x733U
#define CH32_VC02_CAN_ID_ACK          0x734U

#define CH32_VC02_STREAM_MAX          96U
#define CH32_VC02_RAW_MAX             8U

typedef enum {
    CH32_VC02_CMD_NONE = 0,
    CH32_VC02_CMD_WAKEUP,
    CH32_VC02_CMD_OLED_CLEAR,
    CH32_VC02_CMD_MOTOR_START,
    CH32_VC02_CMD_OLED_REFRESH,
    CH32_VC02_CMD_SERVO_START,
    CH32_VC02_CMD_MPU6050_SHOW,
    CH32_VC02_CMD_SYN_STOP,
    CH32_VC02_CMD_SYN_START,
    CH32_VC02_CMD_MOTOR_STOP,
    CH32_VC02_CMD_SERVO_STOP,
    CH32_VC02_CMD_VL53L0X_SHOW,
    CH32_VC02_CMD_COUNT,
} ch32_vc02_cmd_t;

typedef struct {
    ch32_vc02_cmd_t cmd;
    const char *name;
    const char *vc02_uart_text;
    const char *normalized_code;
    const char *meaning;
    const char *action_slot;
} ch32_vc02_command_info_t;

typedef struct {
    ch32_vc02_cmd_t cmd;
    const ch32_vc02_command_info_t *info;
    uint8_t raw[CH32_VC02_RAW_MAX];
    uint8_t raw_len;
} ch32_vc02_event_t;

typedef void (*ch32_vc02_action_cb_t)(const ch32_vc02_event_t *event, void *user_ctx);

typedef struct {
    ch32_vc02_cmd_t cmd;
    ch32_vc02_action_cb_t callback;
    void *user_ctx;
} ch32_vc02_action_binding_t;

typedef struct {
    char normalized_stream[CH32_VC02_STREAM_MAX];
    size_t normalized_len;
    char ascii_window[CH32_VC02_STREAM_MAX];
    size_t ascii_len;
    ch32_vc02_action_binding_t bindings[CH32_VC02_CMD_COUNT];
    uint32_t rx_bytes;
    uint32_t rx_chunks;
    uint32_t matched_events;
    uint32_t dispatched_events;
    uint32_t no_match_chunks;
} ch32_vc02_driver_t;

extern const ch32_vc02_command_info_t ch32_vc02_command_table[];
extern const size_t ch32_vc02_command_count;

void ch32_vc02_init(ch32_vc02_driver_t *driver);
bool ch32_vc02_register_action(ch32_vc02_driver_t *driver,
                               ch32_vc02_cmd_t cmd,
                               ch32_vc02_action_cb_t callback,
                               void *user_ctx);
bool ch32_vc02_feed_bytes(ch32_vc02_driver_t *driver,
                          const uint8_t *bytes,
                          uint8_t len,
                          ch32_vc02_event_t *event);
bool ch32_vc02_dispatch(ch32_vc02_driver_t *driver, const ch32_vc02_event_t *event);
bool ch32_vc02_build_ping(uint8_t seq, uint8_t data[8]);
const ch32_vc02_command_info_t *ch32_vc02_command_info(ch32_vc02_cmd_t cmd);
const char *ch32_vc02_cmd_name(ch32_vc02_cmd_t cmd);
const char *ch32_vc02_action_slot(ch32_vc02_cmd_t cmd);

#ifdef __cplusplus
}
#endif
