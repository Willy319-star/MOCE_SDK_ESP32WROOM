#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_SYN6288_MAX_FRAME_SIZE                  4096U
#define CH32_SYN6288_DEFAULT_BITRATE                 50000U
#define CH32_SYN6288_DEFAULT_START_ACK_TIMEOUT_MS    500U
#define CH32_SYN6288_DEFAULT_FINAL_ACK_TIMEOUT_MS    6000U
#define CH32_SYN6288_DEFAULT_CAN_TX_GPIO             GPIO_NUM_5
#define CH32_SYN6288_DEFAULT_CAN_RX_GPIO             GPIO_NUM_4

typedef struct ch32_syn6288_gateway *ch32_syn6288_gateway_handle_t;

typedef struct {
    gpio_num_t can_tx_gpio;
    gpio_num_t can_rx_gpio;
    uint32_t bitrate;
    uint32_t start_ack_timeout_ms;
    uint32_t final_ack_timeout_ms;
} ch32_syn6288_gateway_config_t;

#define CH32_SYN6288_GATEWAY_CONFIG_DEFAULT()                 \
    {                                                         \
        .can_tx_gpio = CH32_SYN6288_DEFAULT_CAN_TX_GPIO,       \
        .can_rx_gpio = CH32_SYN6288_DEFAULT_CAN_RX_GPIO,       \
        .bitrate = CH32_SYN6288_DEFAULT_BITRATE,               \
        .start_ack_timeout_ms =                                \
            CH32_SYN6288_DEFAULT_START_ACK_TIMEOUT_MS,         \
        .final_ack_timeout_ms =                                \
            CH32_SYN6288_DEFAULT_FINAL_ACK_TIMEOUT_MS,         \
    }

esp_err_t ch32_syn6288_gateway_create(
    const ch32_syn6288_gateway_config_t *config,
    ch32_syn6288_gateway_handle_t *out_handle);

esp_err_t ch32_syn6288_gateway_send_raw(
    ch32_syn6288_gateway_handle_t handle,
    const uint8_t *frame,
    size_t frame_len);

esp_err_t ch32_syn6288_gateway_delete(
    ch32_syn6288_gateway_handle_t handle);

#ifdef __cplusplus
}
#endif

