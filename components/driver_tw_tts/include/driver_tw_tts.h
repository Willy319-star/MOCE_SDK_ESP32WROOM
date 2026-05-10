#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_TW_TTS_DEFAULT_TX_TIMEOUT_MS 1000
#define DRIVER_TW_TTS_DEFAULT_BOOT_DELAY_MS 300

typedef enum {
    DRIVER_TW_TTS_ENCODING_GB2312 = 0x00,
    DRIVER_TW_TTS_ENCODING_GBK = 0x01,
    DRIVER_TW_TTS_ENCODING_UTF8 = 0x04,
} driver_tw_tts_encoding_t;

typedef struct {
    bsp_uart_config_t uart;
    uint32_t tx_timeout_ms;
    uint32_t boot_delay_ms;
    driver_tw_tts_encoding_t encoding;
} driver_tw_tts_config_t;

void driver_tw_tts_get_default_config(driver_tw_tts_config_t *config);

esp_err_t driver_tw_tts_init(const driver_tw_tts_config_t *config);
esp_err_t driver_tw_tts_init_default(void);
esp_err_t driver_tw_tts_deinit(void);
bool driver_tw_tts_is_initialized(void);

esp_err_t driver_tw_tts_write(const void *data, size_t len);
esp_err_t driver_tw_tts_write_string(const char *text);
esp_err_t driver_tw_tts_send_command(uint8_t command);
esp_err_t driver_tw_tts_speak(const char *text);
esp_err_t driver_tw_tts_speak_with_prefix(const char *prefix, const char *text);
esp_err_t driver_tw_tts_stop(void);
esp_err_t driver_tw_tts_pause(void);
esp_err_t driver_tw_tts_resume(void);
esp_err_t driver_tw_tts_query_status(void);
esp_err_t driver_tw_tts_set_volume(uint8_t volume);
esp_err_t driver_tw_tts_set_tone(uint8_t tone);

#ifdef __cplusplus
}
#endif
