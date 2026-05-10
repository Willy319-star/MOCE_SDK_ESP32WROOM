#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t port;
    int tx_gpio;
    int rx_gpio;
    int rts_gpio;
    int cts_gpio;
    int baud_rate;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    int event_queue_size;
} bsp_uart_config_t;

void bsp_uart_get_default_config(bsp_uart_config_t *config);

esp_err_t bsp_uart_init(const bsp_uart_config_t *config);
esp_err_t bsp_uart_init_default(void);
esp_err_t bsp_uart_deinit(void);
bool bsp_uart_is_initialized(void);
uart_port_t bsp_uart_get_port(void);

esp_err_t bsp_uart_write(const void *data, size_t len, uint32_t timeout_ms);
esp_err_t bsp_uart_write_string(const char *text, uint32_t timeout_ms);
int bsp_uart_read(void *data, size_t len, uint32_t timeout_ms);
esp_err_t bsp_uart_flush_input(void);

#ifdef __cplusplus
}
#endif
