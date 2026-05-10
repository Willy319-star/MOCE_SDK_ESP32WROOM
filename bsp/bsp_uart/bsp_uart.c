#include "bsp_uart.h"

#include <string.h>

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "bsp_uart";

#ifndef BOARD_UART_PORT
#define BOARD_UART_PORT UART_NUM_1
#endif

#ifndef BOARD_UART_TX_GPIO
#define BOARD_UART_TX_GPIO UART_PIN_NO_CHANGE
#endif

#ifndef BOARD_UART_RX_GPIO
#define BOARD_UART_RX_GPIO UART_PIN_NO_CHANGE
#endif

#ifndef BOARD_UART_BAUD_RATE
#define BOARD_UART_BAUD_RATE 9600
#endif

#ifndef BOARD_UART_RX_BUFFER_SIZE
#define BOARD_UART_RX_BUFFER_SIZE 256
#endif

#ifndef BOARD_UART_TX_BUFFER_SIZE
#define BOARD_UART_TX_BUFFER_SIZE 0
#endif

#ifndef BOARD_UART_EVENT_QUEUE_SIZE
#define BOARD_UART_EVENT_QUEUE_SIZE 0
#endif

static bool s_initialized;
static uart_port_t s_port = BOARD_UART_PORT;

void bsp_uart_get_default_config(bsp_uart_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (bsp_uart_config_t) {
        .port = BOARD_UART_PORT,
        .tx_gpio = BOARD_UART_TX_GPIO,
        .rx_gpio = BOARD_UART_RX_GPIO,
        .rts_gpio = UART_PIN_NO_CHANGE,
        .cts_gpio = UART_PIN_NO_CHANGE,
        .baud_rate = BOARD_UART_BAUD_RATE,
        .rx_buffer_size = BOARD_UART_RX_BUFFER_SIZE,
        .tx_buffer_size = BOARD_UART_TX_BUFFER_SIZE,
        .event_queue_size = BOARD_UART_EVENT_QUEUE_SIZE,
    };
}

esp_err_t bsp_uart_init(const bsp_uart_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(config->baud_rate > 0, ESP_ERR_INVALID_ARG, TAG, "invalid baud rate");
    ESP_RETURN_ON_FALSE(config->rx_buffer_size > 0, ESP_ERR_INVALID_ARG, TAG, "rx buffer is too small");

    if (s_initialized) {
        if (s_port == config->port) {
            return ESP_OK;
        }
        return ESP_ERR_INVALID_STATE;
    }

    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_param_config(config->port, &uart_config), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(config->port,
                                     config->tx_gpio,
                                     config->rx_gpio,
                                     config->rts_gpio,
                                     config->cts_gpio),
                        TAG,
                        "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_driver_install(config->port,
                                            config->rx_buffer_size,
                                            config->tx_buffer_size,
                                            config->event_queue_size,
                                            NULL,
                                            0),
                        TAG,
                        "uart_driver_install failed");

    s_port = config->port;
    s_initialized = true;
    ESP_LOGI(TAG, "UART%d initialized: TX GPIO %d, RX GPIO %d, baud %d",
             s_port,
             config->tx_gpio,
             config->rx_gpio,
             config->baud_rate);
    return ESP_OK;
}

esp_err_t bsp_uart_init_default(void)
{
    bsp_uart_config_t config;
    bsp_uart_get_default_config(&config);
    return bsp_uart_init(&config);
}

esp_err_t bsp_uart_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(uart_driver_delete(s_port), TAG, "uart_driver_delete failed");
    s_initialized = false;
    return ESP_OK;
}

bool bsp_uart_is_initialized(void)
{
    return s_initialized;
}

uart_port_t bsp_uart_get_port(void)
{
    return s_port;
}

esp_err_t bsp_uart_write(const void *data, size_t len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "UART is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL || len == 0, ESP_ERR_INVALID_ARG, TAG, "data is NULL");

    if (len == 0) {
        return ESP_OK;
    }

    int written = uart_write_bytes(s_port, data, len);
    ESP_RETURN_ON_FALSE(written == (int)len, ESP_FAIL, TAG, "uart_write_bytes failed");

    return uart_wait_tx_done(s_port, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t bsp_uart_write_string(const char *text, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    return bsp_uart_write(text, strlen(text), timeout_ms);
}

int bsp_uart_read(void *data, size_t len, uint32_t timeout_ms)
{
    if (!s_initialized || data == NULL || len == 0) {
        return -1;
    }

    return uart_read_bytes(s_port, data, len, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t bsp_uart_flush_input(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "UART is not initialized");
    return uart_flush_input(s_port);
}
