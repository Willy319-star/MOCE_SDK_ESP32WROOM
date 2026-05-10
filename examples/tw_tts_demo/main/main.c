#include "driver_tw_tts.h"
#include "bsp_uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tw_tts_demo";

static void log_tts_response(void)
{
    uint8_t data[32];
    int len = bsp_uart_read(data, sizeof(data), 100);
    if (len <= 0) {
        return;
    }

    char hex[sizeof(data) * 3 + 1] = {0};
    for (int i = 0; i < len; i++) {
        snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X ", data[i]);
    }
    ESP_LOGI(TAG, "TW-TTS RX %d bytes: %s", len, hex);
}

static void speak_and_wait(const char *text, uint32_t wait_ms)
{
    ESP_LOGI(TAG, "send text: %s", text);
    ESP_ERROR_CHECK(driver_tw_tts_speak(text));
    log_tts_response();
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
}

void app_main(void)
{
    ESP_LOGI(TAG, "start TW-TTS demo");

    driver_tw_tts_config_t config;
    driver_tw_tts_get_default_config(&config);

    /*
     * ESP32-S3 default wiring:
     *   ESP UART1 TX GPIO38 -> TW-TTS RX
     *   ESP UART1 RX GPIO39 -> TW-TTS TX, optional
     *   GND           -> GND
     *   5V/3V3        -> module VCC, follow the module label
    */
    config.uart.baud_rate = 9600;
    config.encoding = DRIVER_TW_TTS_ENCODING_UTF8;
    ESP_ERROR_CHECK(driver_tw_tts_init(&config));

    ESP_LOGI(TAG, "UART%d TX GPIO%d RX GPIO%d baud %d",
             config.uart.port,
             config.uart.tx_gpio,
             config.uart.rx_gpio,
             config.uart.baud_rate);
    ESP_ERROR_CHECK(driver_tw_tts_set_volume(6));
    vTaskDelay(pdMS_TO_TICKS(200));

    while (1) {
        speak_and_wait("TW TTS module test.", 3000);
        speak_and_wait("Hello, this is Moce ESP32 SDK.", 4000);

        /*
         * Keep sending every few seconds so wiring can be probed with a logic analyzer
         * or USB-TTL adapter without racing the boot-time messages.
         */
        speak_and_wait("UART text to speech is ready.", 5000);
    }
}
