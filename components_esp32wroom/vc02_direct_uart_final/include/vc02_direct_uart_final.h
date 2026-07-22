#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VC02_DIRECT_UART_DEFAULT_BAUD         115200U
#define VC02_DIRECT_UART_STREAM_MAX          96U
#define VC02_DIRECT_UART_RAW_MAX             32U

typedef enum {
    VC02_DIRECT_CMD_NONE = 0,
    VC02_DIRECT_CMD_WAKEUP,
    VC02_DIRECT_CMD_OLED_CLEAR,
    VC02_DIRECT_CMD_MOTOR_START,
    VC02_DIRECT_CMD_OLED_REFRESH,
    VC02_DIRECT_CMD_SERVO_START,
    VC02_DIRECT_CMD_MPU6050_SHOW,
    VC02_DIRECT_CMD_SYN_STOP,
    VC02_DIRECT_CMD_SYN_START,
    VC02_DIRECT_CMD_MOTOR_STOP,
    VC02_DIRECT_CMD_SERVO_STOP,
    VC02_DIRECT_CMD_VL53L0X_SHOW,
    VC02_DIRECT_CMD_COUNT,
} vc02_direct_cmd_t;

typedef struct {
    vc02_direct_cmd_t cmd;
    const char *name;
    const char *vc02_uart_text;
    const char *normalized_code;
    const char *meaning;
    const char *action_slot;
} vc02_direct_command_info_t;

typedef struct {
    vc02_direct_cmd_t cmd;
    const vc02_direct_command_info_t *info;
    uint8_t raw[VC02_DIRECT_UART_RAW_MAX];
    uint8_t raw_len;
} vc02_direct_event_t;

typedef void (*vc02_direct_action_cb_t)(const vc02_direct_event_t *event, void *user_ctx);

typedef struct {
    uart_port_t uart_port;
    int tx_gpio;
    int rx_gpio;
    uint32_t baud_rate;
    int rx_buffer_size;
    int tx_buffer_size;
} vc02_direct_uart_final_config_t;

typedef struct {
    vc02_direct_cmd_t cmd;
    vc02_direct_action_cb_t callback;
    void *user_ctx;
} vc02_direct_action_binding_t;

typedef struct {
    vc02_direct_uart_final_config_t config;
    bool uart_ready;
    char normalized_stream[VC02_DIRECT_UART_STREAM_MAX];
    size_t normalized_len;
    char ascii_window[VC02_DIRECT_UART_STREAM_MAX];
    size_t ascii_len;
    vc02_direct_action_binding_t bindings[VC02_DIRECT_CMD_COUNT];
    uint32_t rx_bytes;
    uint32_t rx_chunks;
    uint32_t matched_events;
    uint32_t dispatched_events;
    uint32_t no_match_chunks;
    uint32_t uart_read_timeouts;
} vc02_direct_uart_final_t;

extern const vc02_direct_command_info_t vc02_direct_command_table[];
extern const size_t vc02_direct_command_count;

vc02_direct_uart_final_config_t vc02_direct_uart_final_default_config(void);
esp_err_t vc02_direct_uart_final_init(vc02_direct_uart_final_t *driver, const vc02_direct_uart_final_config_t *config);
esp_err_t vc02_direct_uart_final_deinit(vc02_direct_uart_final_t *driver);
bool vc02_direct_register_action(vc02_direct_uart_final_t *driver,
                                 vc02_direct_cmd_t cmd,
                                 vc02_direct_action_cb_t callback,
                                 void *user_ctx);
bool vc02_direct_feed_bytes(vc02_direct_uart_final_t *driver,
                            const uint8_t *bytes,
                            uint8_t len,
                            vc02_direct_event_t *event);
esp_err_t vc02_direct_read_event(vc02_direct_uart_final_t *driver,
                                 vc02_direct_event_t *event,
                                 uint32_t timeout_ms);
bool vc02_direct_dispatch(vc02_direct_uart_final_t *driver, const vc02_direct_event_t *event);
const vc02_direct_command_info_t *vc02_direct_command_info(vc02_direct_cmd_t cmd);
const char *vc02_direct_cmd_name(vc02_direct_cmd_t cmd);
const char *vc02_direct_action_slot(vc02_direct_cmd_t cmd);

#ifdef __cplusplus
}
#endif
