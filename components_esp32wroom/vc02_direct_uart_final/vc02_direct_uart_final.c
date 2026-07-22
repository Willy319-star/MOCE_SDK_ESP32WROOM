#include "vc02_direct_uart_final.h"

#include <string.h>

#include "freertos/FreeRTOS.h"

const vc02_direct_command_info_t vc02_direct_command_table[] = {
    {VC02_DIRECT_CMD_WAKEUP, "WAKEUP", "TX WK 00", "TXWK00", "voice wakeup detected", "slot_voice_wakeup"},
    {VC02_DIRECT_CMD_OLED_CLEAR, "OLED_CLEAR", "TX CL OL ED", "TXCLOLED", "clear OLED screen", "slot_oled_clear"},
    {VC02_DIRECT_CMD_MOTOR_START, "MOTOR_START", "TX SA MO TO", "TXSAMOTO", "start motor", "slot_motor_start"},
    {VC02_DIRECT_CMD_OLED_REFRESH, "OLED_REFRESH", "TX RF OL ED", "TXRFOLED", "refresh OLED screen", "slot_oled_refresh"},
    {VC02_DIRECT_CMD_SERVO_START, "SERVO_START", "TX SA SE VO", "TXSASEVO", "start servo", "slot_servo_start"},
    {VC02_DIRECT_CMD_MPU6050_SHOW, "MPU6050_SHOW", "TX SA MP U0", "TXSAMPU0", "show MPU6050 angle data", "slot_mpu6050_show"},
    {VC02_DIRECT_CMD_SYN_STOP, "SYN_STOP", "TX SO SY NO", "TXSOSYNO", "stop voice broadcast", "slot_voice_broadcast_stop"},
    {VC02_DIRECT_CMD_SYN_START, "SYN_START", "TX SA SY NO", "TXSASYNO", "start voice broadcast", "slot_voice_broadcast_start"},
    {VC02_DIRECT_CMD_MOTOR_STOP, "MOTOR_STOP", "TX SO MO TO", "TXSOMOTO", "stop motor", "slot_motor_stop"},
    {VC02_DIRECT_CMD_SERVO_STOP, "SERVO_STOP", "TX SO SE VO", "TXSOSEVO", "stop servo", "slot_servo_stop"},
    {VC02_DIRECT_CMD_VL53L0X_SHOW, "VL53L0X_SHOW", "TX SA VL 00", "TXSAVL00", "show VL53L0X distance data", "slot_vl53l0x_show"},
};

const size_t vc02_direct_command_count = sizeof(vc02_direct_command_table) / sizeof(vc02_direct_command_table[0]);

static bool is_printable(uint8_t byte)
{
    return byte >= 0x20U && byte <= 0x7EU;
}

static char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - ('a' - 'A')) : c;
}

static void append_char(char *buf, size_t *len, size_t cap, char c)
{
    if (buf == NULL || len == NULL || cap == 0U) {
        return;
    }
    if (*len + 1U >= cap) {
        memmove(buf, buf + 1U, cap - 2U);
        *len = cap - 2U;
    }
    buf[*len] = c;
    *len += 1U;
    buf[*len] = '\0';
}

static void append_normalized_byte(vc02_direct_uart_final_t *driver, uint8_t byte)
{
    if (byte == 0U) {
        append_char(driver->normalized_stream, &driver->normalized_len,
                    sizeof(driver->normalized_stream), '0');
        append_char(driver->normalized_stream, &driver->normalized_len,
                    sizeof(driver->normalized_stream), '0');
        return;
    }

    if (byte == ' ' || byte == '\r' || byte == '\n' || byte == '\t') {
        return;
    }

    if (is_printable(byte)) {
        append_char(driver->normalized_stream, &driver->normalized_len,
                    sizeof(driver->normalized_stream), to_upper((char)byte));
    }
}

static void append_ascii_byte(vc02_direct_uart_final_t *driver, uint8_t byte)
{
    append_char(driver->ascii_window, &driver->ascii_len,
                sizeof(driver->ascii_window),
                is_printable(byte) ? (char)byte : '.');
}

static bool stream_suffix(const char *stream, size_t stream_len, const char *suffix)
{
    size_t suffix_len = suffix != NULL ? strlen(suffix) : 0U;
    if (stream == NULL || suffix_len == 0U || stream_len < suffix_len) {
        return false;
    }
    return memcmp(stream + stream_len - suffix_len, suffix, suffix_len) == 0;
}

vc02_direct_uart_final_config_t vc02_direct_uart_final_default_config(void)
{
    return (vc02_direct_uart_final_config_t) {
        .uart_port = UART_NUM_1,
        .tx_gpio = 17,
        .rx_gpio = 16,
        .baud_rate = VC02_DIRECT_UART_DEFAULT_BAUD,
        .rx_buffer_size = 512,
        .tx_buffer_size = 0,
    };
}

esp_err_t vc02_direct_uart_final_init(vc02_direct_uart_final_t *driver, const vc02_direct_uart_final_config_t *config)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    vc02_direct_uart_final_config_t cfg = config != NULL ? *config : vc02_direct_uart_final_default_config();
    memset(driver, 0, sizeof(*driver));
    driver->config = cfg;
    for (uint8_t i = 0U; i < VC02_DIRECT_CMD_COUNT; ++i) {
        driver->bindings[i].cmd = (vc02_direct_cmd_t)i;
    }

    uart_config_t uart_cfg = {
        .baud_rate = (int)cfg.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(cfg.uart_port, cfg.rx_buffer_size, cfg.tx_buffer_size, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(uart_param_config(cfg.uart_port, &uart_cfg), "vc02_direct_uart_final", "uart_param_config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(cfg.uart_port, cfg.tx_gpio, cfg.rx_gpio,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        "vc02_direct_uart_final", "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_flush_input(cfg.uart_port), "vc02_direct_uart_final", "uart_flush_input failed");

    driver->uart_ready = true;
    return ESP_OK;
}

esp_err_t vc02_direct_uart_final_deinit(vc02_direct_uart_final_t *driver)
{
    if (driver == NULL || !driver->uart_ready) {
        return ESP_OK;
    }
    esp_err_t err = uart_driver_delete(driver->config.uart_port);
    driver->uart_ready = false;
    return err;
}

bool vc02_direct_register_action(vc02_direct_uart_final_t *driver,
                                 vc02_direct_cmd_t cmd,
                                 vc02_direct_action_cb_t callback,
                                 void *user_ctx)
{
    if (driver == NULL || cmd <= VC02_DIRECT_CMD_NONE || cmd >= VC02_DIRECT_CMD_COUNT) {
        return false;
    }
    driver->bindings[cmd].cmd = cmd;
    driver->bindings[cmd].callback = callback;
    driver->bindings[cmd].user_ctx = user_ctx;
    return true;
}

bool vc02_direct_feed_bytes(vc02_direct_uart_final_t *driver,
                            const uint8_t *bytes,
                            uint8_t len,
                            vc02_direct_event_t *event)
{
    if (driver == NULL || bytes == NULL || len == 0U || event == NULL) {
        return false;
    }

    memset(event, 0, sizeof(*event));
    driver->rx_chunks++;
    driver->rx_bytes += len;

    uint8_t copy_len = len < VC02_DIRECT_UART_RAW_MAX ? len : VC02_DIRECT_UART_RAW_MAX;
    memcpy(event->raw, bytes, copy_len);
    event->raw_len = copy_len;

    for (uint8_t i = 0U; i < len; ++i) {
        append_ascii_byte(driver, bytes[i]);
        append_normalized_byte(driver, bytes[i]);
    }

    for (size_t i = 0U; i < vc02_direct_command_count; ++i) {
        const vc02_direct_command_info_t *info = &vc02_direct_command_table[i];
        if (stream_suffix(driver->normalized_stream, driver->normalized_len,
                          info->normalized_code)) {
            event->cmd = info->cmd;
            event->info = info;
            driver->matched_events++;
            return true;
        }
    }

    driver->no_match_chunks++;
    return false;
}

esp_err_t vc02_direct_read_event(vc02_direct_uart_final_t *driver,
                                 vc02_direct_event_t *event,
                                 uint32_t timeout_ms)
{
    if (driver == NULL || event == NULL || !driver->uart_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[VC02_DIRECT_UART_RAW_MAX] = {0};
    int got = uart_read_bytes(driver->config.uart_port, buf, sizeof(buf), pdMS_TO_TICKS(timeout_ms));
    if (got < 0) {
        return ESP_FAIL;
    }
    if (got == 0) {
        driver->uart_read_timeouts++;
        return ESP_ERR_TIMEOUT;
    }

    return vc02_direct_feed_bytes(driver, buf, (uint8_t)got, event) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool vc02_direct_dispatch(vc02_direct_uart_final_t *driver, const vc02_direct_event_t *event)
{
    if (driver == NULL || event == NULL || event->cmd <= VC02_DIRECT_CMD_NONE ||
        event->cmd >= VC02_DIRECT_CMD_COUNT) {
        return false;
    }
    vc02_direct_action_cb_t callback = driver->bindings[event->cmd].callback;
    if (callback == NULL) {
        return false;
    }
    callback(event, driver->bindings[event->cmd].user_ctx);
    driver->dispatched_events++;
    return true;
}

const vc02_direct_command_info_t *vc02_direct_command_info(vc02_direct_cmd_t cmd)
{
    for (size_t i = 0U; i < vc02_direct_command_count; ++i) {
        if (vc02_direct_command_table[i].cmd == cmd) {
            return &vc02_direct_command_table[i];
        }
    }
    return NULL;
}

const char *vc02_direct_cmd_name(vc02_direct_cmd_t cmd)
{
    const vc02_direct_command_info_t *info = vc02_direct_command_info(cmd);
    return info != NULL ? info->name : "NONE";
}

const char *vc02_direct_action_slot(vc02_direct_cmd_t cmd)
{
    const vc02_direct_command_info_t *info = vc02_direct_command_info(cmd);
    return info != NULL ? info->action_slot : "slot_none";
}
