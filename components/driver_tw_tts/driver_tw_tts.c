#include "driver_tw_tts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "driver_tw_tts";

#define TW_TTS_FRAME_HEAD        0xfd
#define TW_TTS_CMD_START        0x01
#define TW_TTS_CMD_STOP         0x02
#define TW_TTS_CMD_PAUSE        0x03
#define TW_TTS_CMD_RESUME       0x04
#define TW_TTS_CMD_QUERY_STATUS 0x21

static bool s_initialized;
static uint32_t s_tx_timeout_ms = DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS;
static driver_tw_tts_encoding_t s_encoding = DRIVER_TW_TTS_ENCODING_UTF8;

static esp_err_t send_frame(uint8_t command, const uint8_t *payload, size_t payload_len)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "TW-TTS is not initialized");
    ESP_RETURN_ON_FALSE(payload != NULL || payload_len == 0, ESP_ERR_INVALID_ARG, TAG, "payload is NULL");

    size_t data_len = 1 + payload_len;
    ESP_RETURN_ON_FALSE(data_len <= UINT16_MAX, ESP_ERR_INVALID_SIZE, TAG, "frame too long");

    size_t frame_len = 3 + data_len;
    uint8_t *frame = malloc(frame_len);
    ESP_RETURN_ON_FALSE(frame != NULL, ESP_ERR_NO_MEM, TAG, "no memory for frame");

    frame[0] = TW_TTS_FRAME_HEAD;
    frame[1] = (uint8_t)(data_len >> 8);
    frame[2] = (uint8_t)data_len;
    frame[3] = command;
    if (payload_len > 0) {
        memcpy(&frame[4], payload, payload_len);
    }

    esp_err_t err = bsp_uart_write(frame, frame_len, s_tx_timeout_ms);
    free(frame);
    return err;
}

static esp_err_t send_synthesis_text(const char *text, size_t len)
{
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    ESP_RETURN_ON_FALSE(len <= (UINT16_MAX - 2), ESP_ERR_INVALID_SIZE, TAG, "text too long");

    size_t payload_len = 1 + len;
    uint8_t *payload = malloc(payload_len);
    ESP_RETURN_ON_FALSE(payload != NULL, ESP_ERR_NO_MEM, TAG, "no memory for payload");

    payload[0] = (uint8_t)s_encoding;
    memcpy(&payload[1], text, len);
    esp_err_t err = send_frame(TW_TTS_CMD_START, payload, payload_len);
    free(payload);
    return err;
}

void driver_tw_tts_get_default_config(driver_tw_tts_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    bsp_uart_get_default_config(&config->uart);
    config->tx_timeout_ms = DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS;
    config->boot_delay_ms = DRIVER_TW_TTS_DEFAULT_BOOT_DELAY_MS;
    config->encoding = DRIVER_TW_TTS_ENCODING_UTF8;
}

esp_err_t driver_tw_tts_init(const driver_tw_tts_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_uart_init(&config->uart), TAG, "bsp_uart_init failed");
    s_tx_timeout_ms = config->tx_timeout_ms == 0 ? DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS : config->tx_timeout_ms;
    s_encoding = config->encoding;

    if (config->boot_delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(config->boot_delay_ms));
    }

    s_initialized = true;
    ESP_LOGI(TAG, "TW-TTS driver initialized");
    return ESP_OK;
}

esp_err_t driver_tw_tts_init_default(void)
{
    driver_tw_tts_config_t config;
    driver_tw_tts_get_default_config(&config);
    return driver_tw_tts_init(&config);
}

esp_err_t driver_tw_tts_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = bsp_uart_deinit();
    if (err == ESP_OK) {
        s_initialized = false;
    }
    return err;
}

bool driver_tw_tts_is_initialized(void)
{
    return s_initialized;
}

esp_err_t driver_tw_tts_write(const void *data, size_t len)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "TW-TTS is not initialized");
    return bsp_uart_write(data, len, s_tx_timeout_ms);
}

esp_err_t driver_tw_tts_write_string(const char *text)
{
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    return driver_tw_tts_write(text, strlen(text));
}

esp_err_t driver_tw_tts_send_command(uint8_t command)
{
    return send_frame(command, NULL, 0);
}

esp_err_t driver_tw_tts_speak(const char *text)
{
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is NULL");
    return send_synthesis_text(text, strlen(text));
}

esp_err_t driver_tw_tts_speak_with_prefix(const char *prefix, const char *text)
{
    ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is NULL");

    size_t prefix_len = (prefix == NULL) ? 0 : strlen(prefix);
    size_t text_len = strlen(text);
    ESP_RETURN_ON_FALSE(prefix_len <= SIZE_MAX - text_len, ESP_ERR_INVALID_SIZE, TAG, "text too long");

    size_t total_len = prefix_len + text_len;
    char *buffer = malloc(total_len + 1);
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_NO_MEM, TAG, "no memory for prefixed text");

    if (prefix_len > 0) {
        memcpy(buffer, prefix, prefix_len);
    }
    memcpy(buffer + prefix_len, text, text_len);
    buffer[total_len] = '\0';

    esp_err_t err = send_synthesis_text(buffer, total_len);
    free(buffer);
    return err;
}

esp_err_t driver_tw_tts_stop(void)
{
    return driver_tw_tts_send_command(TW_TTS_CMD_STOP);
}

esp_err_t driver_tw_tts_pause(void)
{
    return driver_tw_tts_send_command(TW_TTS_CMD_PAUSE);
}

esp_err_t driver_tw_tts_resume(void)
{
    return driver_tw_tts_send_command(TW_TTS_CMD_RESUME);
}

esp_err_t driver_tw_tts_query_status(void)
{
    return driver_tw_tts_send_command(TW_TTS_CMD_QUERY_STATUS);
}

esp_err_t driver_tw_tts_set_volume(uint8_t volume)
{
    ESP_RETURN_ON_FALSE(volume <= 9, ESP_ERR_INVALID_ARG, TAG, "invalid volume");
    char text[5];
    snprintf(text, sizeof(text), "[v%u]", volume);
    return driver_tw_tts_speak(text);
}

esp_err_t driver_tw_tts_set_tone(uint8_t tone)
{
    ESP_RETURN_ON_FALSE(tone <= 9, ESP_ERR_INVALID_ARG, TAG, "invalid tone");
    char text[5];
    snprintf(text, sizeof(text), "[t%u]", tone);
    return driver_tw_tts_speak(text);
}
