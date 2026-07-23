#include "ch32_vc02_gateway_final.h"

#include <string.h>

const ch32_vc02_command_info_t ch32_vc02_command_table[] = {
    {CH32_VC02_CMD_WAKEUP, "WAKEUP", "TX WK 00", "TXWK00", "voice wakeup detected", "slot_voice_wakeup"},
    {CH32_VC02_CMD_OLED_CLEAR, "OLED_CLEAR", "TX CL OL ED", "TXCLOLED", "clear OLED screen", "slot_oled_clear"},
    {CH32_VC02_CMD_MOTOR_START, "MOTOR_START", "TX SA MO TO", "TXSAMOTO", "start motor", "slot_motor_start"},
    {CH32_VC02_CMD_OLED_REFRESH, "OLED_REFRESH", "TX RF OL ED", "TXRFOLED", "refresh OLED screen", "slot_oled_refresh"},
    {CH32_VC02_CMD_SERVO_START, "SERVO_START", "TX SA SE VO", "TXSASEVO", "start servo", "slot_servo_start"},
    {CH32_VC02_CMD_MPU6050_SHOW, "MPU6050_SHOW", "TX SA MP U0", "TXSAMPU0", "show MPU6050 angle data", "slot_mpu6050_show"},
    {CH32_VC02_CMD_SYN_STOP, "SYN_STOP", "TX SO SY NO", "TXSOSYNO", "stop voice broadcast", "slot_voice_broadcast_stop"},
    {CH32_VC02_CMD_SYN_START, "SYN_START", "TX SA SY NO", "TXSASYNO", "start voice broadcast", "slot_voice_broadcast_start"},
    {CH32_VC02_CMD_MOTOR_STOP, "MOTOR_STOP", "TX SO MO TO", "TXSOMOTO", "stop motor", "slot_motor_stop"},
    {CH32_VC02_CMD_SERVO_STOP, "SERVO_STOP", "TX SO SE VO", "TXSOSEVO", "stop servo", "slot_servo_stop"},
    {CH32_VC02_CMD_VL53L0X_SHOW, "VL53L0X_SHOW", "TX SA VL 00", "TXSAVL00", "show VL53L0X distance data", "slot_vl53l0x_show"},
};

const size_t ch32_vc02_command_count = sizeof(ch32_vc02_command_table) / sizeof(ch32_vc02_command_table[0]);

static bool is_printable(uint8_t byte)
{
    return byte >= 0x20U && byte <= 0x7EU;
}

static char upper_char(char c)
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

static void append_normalized_byte(ch32_vc02_driver_t *driver, uint8_t byte)
{
    if (driver == NULL) {
        return;
    }
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
                    sizeof(driver->normalized_stream), upper_char((char)byte));
    }
}

static void append_ascii_byte(ch32_vc02_driver_t *driver, uint8_t byte)
{
    if (driver == NULL) {
        return;
    }
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

static bool command_matches(const ch32_vc02_driver_t *driver, const ch32_vc02_command_info_t *info)
{
    if (driver == NULL || info == NULL) {
        return false;
    }
    if (stream_suffix(driver->normalized_stream, driver->normalized_len, info->normalized_code)) {
        return true;
    }
    if (info->cmd == CH32_VC02_CMD_WAKEUP) {
        return stream_suffix(driver->normalized_stream, driver->normalized_len, "00TXWK00");
    }
    return false;
}

void ch32_vc02_init(ch32_vc02_driver_t *driver)
{
    if (driver == NULL) {
        return;
    }
    memset(driver, 0, sizeof(*driver));
    for (uint8_t i = 0U; i < CH32_VC02_CMD_COUNT; ++i) {
        driver->bindings[i].cmd = (ch32_vc02_cmd_t)i;
    }
}

bool ch32_vc02_register_action(ch32_vc02_driver_t *driver,
                               ch32_vc02_cmd_t cmd,
                               ch32_vc02_action_cb_t callback,
                               void *user_ctx)
{
    if (driver == NULL || cmd <= CH32_VC02_CMD_NONE || cmd >= CH32_VC02_CMD_COUNT) {
        return false;
    }
    driver->bindings[cmd].cmd = cmd;
    driver->bindings[cmd].callback = callback;
    driver->bindings[cmd].user_ctx = user_ctx;
    return true;
}

bool ch32_vc02_feed_bytes(ch32_vc02_driver_t *driver,
                          const uint8_t *bytes,
                          uint8_t len,
                          ch32_vc02_event_t *event)
{
    if (driver == NULL || bytes == NULL || len == 0U || event == NULL) {
        return false;
    }

    memset(event, 0, sizeof(*event));
    driver->rx_chunks++;
    driver->rx_bytes += len;

    uint8_t copy_len = len < CH32_VC02_RAW_MAX ? len : CH32_VC02_RAW_MAX;
    memcpy(event->raw, bytes, copy_len);
    event->raw_len = copy_len;

    for (uint8_t i = 0U; i < len; ++i) {
        append_ascii_byte(driver, bytes[i]);
        append_normalized_byte(driver, bytes[i]);
    }

    for (size_t i = 0U; i < ch32_vc02_command_count; ++i) {
        const ch32_vc02_command_info_t *info = &ch32_vc02_command_table[i];
        if (command_matches(driver, info)) {
            event->cmd = info->cmd;
            event->info = info;
            driver->matched_events++;
            return true;
        }
    }

    driver->no_match_chunks++;
    return false;
}

bool ch32_vc02_dispatch(ch32_vc02_driver_t *driver, const ch32_vc02_event_t *event)
{
    if (driver == NULL || event == NULL || event->cmd <= CH32_VC02_CMD_NONE ||
        event->cmd >= CH32_VC02_CMD_COUNT) {
        return false;
    }
    ch32_vc02_action_cb_t callback = driver->bindings[event->cmd].callback;
    if (callback == NULL) {
        return false;
    }
    callback(event, driver->bindings[event->cmd].user_ctx);
    driver->dispatched_events++;
    return true;
}

bool ch32_vc02_build_ping(uint8_t seq, uint8_t data[8])
{
    if (data == NULL) {
        return false;
    }
    memset(data, 0, 8U);
    data[0] = 0x01U;
    data[1] = seq;
    return true;
}

const ch32_vc02_command_info_t *ch32_vc02_command_info(ch32_vc02_cmd_t cmd)
{
    for (size_t i = 0U; i < ch32_vc02_command_count; ++i) {
        if (ch32_vc02_command_table[i].cmd == cmd) {
            return &ch32_vc02_command_table[i];
        }
    }
    return NULL;
}

const char *ch32_vc02_cmd_name(ch32_vc02_cmd_t cmd)
{
    const ch32_vc02_command_info_t *info = ch32_vc02_command_info(cmd);
    return info != NULL ? info->name : "NONE";
}

const char *ch32_vc02_action_slot(ch32_vc02_cmd_t cmd)
{
    const ch32_vc02_command_info_t *info = ch32_vc02_command_info(cmd);
    return info != NULL ? info->action_slot : "slot_none";
}
