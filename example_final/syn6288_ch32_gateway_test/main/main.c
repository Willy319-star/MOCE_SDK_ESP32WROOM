#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "ch32_syn6288_gateway.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "syn6288_gateway_test";

#define SYN6288_POWER_UP_DELAY_MS 3000U

/*
 * Hardware-verified SYN6288E legacy frame for the GBK text "宇音天下".
 * The length includes command, parameter, text, and XOR checksum.
 */
static const uint8_t SYN6288_TEST_FRAME[] = {
    0xFD, 0x00, 0x0B, 0x01, 0x01,
    0xD3, 0xEE, 0xD2, 0xF4, 0xCC, 0xEC, 0xCF, 0xC2, 0xC0,
};

void app_main(void)
{
    ch32_syn6288_gateway_config_t config =
        CH32_SYN6288_GATEWAY_CONFIG_DEFAULT();
    ch32_syn6288_gateway_handle_t gateway = NULL;
    size_t fragment_count =
        (sizeof(SYN6288_TEST_FRAME) + 4U) / 5U;

    config.can_tx_gpio = BOARD_CAN_TX_GPIO;
    config.can_rx_gpio = BOARD_CAN_RX_GPIO;

    ESP_LOGI(TAG,
             "ESP32-WROOM -> CAN -> CH32 -> PA9 UART -> SYN6288E");
    ESP_LOGI(TAG,
             "CAN=50 kbit/s TX=GPIO%d RX=GPIO%d "
             "frame_len=%u fragments=%u",
             BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO,
             (unsigned)sizeof(SYN6288_TEST_FRAME),
             (unsigned)fragment_count);

    esp_err_t result =
        ch32_syn6288_gateway_create(&config, &gateway);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "waiting %u ms for SYN6288E power-up",
                 (unsigned)SYN6288_POWER_UP_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(SYN6288_POWER_UP_DELAY_MS));
        result = ch32_syn6288_gateway_send_raw(
            gateway, SYN6288_TEST_FRAME,
            sizeof(SYN6288_TEST_FRAME));
    }

    if (result == ESP_OK) {
        ESP_LOGI(TAG,
                 "one-shot SYN6288E transfer completed successfully");
    } else {
        ESP_LOGE(TAG,
                 "one-shot SYN6288E transfer failed: %s (0x%x)",
                 esp_err_to_name(result), (unsigned)result);
        ESP_LOGE(TAG,
                 "no application-level replay will be attempted");
    }

    if (gateway != NULL) {
        esp_err_t delete_result =
            ch32_syn6288_gateway_delete(gateway);
        if (delete_result != ESP_OK) {
            ESP_LOGW(TAG, "gateway delete warning: %s",
                     esp_err_to_name(delete_result));
        }
    }

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

