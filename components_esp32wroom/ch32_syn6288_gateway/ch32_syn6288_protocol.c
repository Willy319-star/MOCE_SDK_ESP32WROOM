#include "ch32_syn6288_protocol.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static void write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8U);
}

uint16_t ch32_syn6288_protocol_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    if (data == NULL && len != 0U) {
        return 0U;
    }

    for (size_t i = 0U; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8U;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? (uint16_t)((crc << 1U) ^ 0x1021U)
                      : (uint16_t)(crc << 1U);
        }
    }

    return crc;
}

void ch32_syn6288_protocol_build_start(uint8_t transfer_id,
                                       uint16_t total_len,
                                       uint16_t crc16,
                                       uint8_t out[8])
{
    if (out == NULL) {
        return;
    }

    out[0] = transfer_id;
    write_u16_le(&out[1], total_len);
    write_u16_le(&out[3], crc16);
    out[5] = CH32_SYN6288_PROTOCOL_VERSION;
    out[6] = 0U;
    out[7] = 0U;
}

size_t ch32_syn6288_protocol_build_data(uint8_t transfer_id,
                                        uint16_t sequence,
                                        const uint8_t *payload,
                                        size_t payload_len,
                                        uint8_t out[8])
{
    if (out == NULL || payload == NULL || payload_len == 0U ||
        payload_len > CH32_SYN6288_DATA_PAYLOAD_SIZE) {
        return 0U;
    }

    out[0] = transfer_id;
    write_u16_le(&out[1], sequence);
    memcpy(&out[3], payload, payload_len);
    return 3U + payload_len;
}

size_t ch32_syn6288_protocol_chunk_count(size_t frame_len)
{
    if (frame_len == 0U) {
        return 0U;
    }
    return (frame_len + CH32_SYN6288_DATA_PAYLOAD_SIZE - 1U) /
           CH32_SYN6288_DATA_PAYLOAD_SIZE;
}

bool ch32_syn6288_protocol_parse_ack(const uint8_t *data,
                                     size_t len,
                                     ch32_syn6288_ack_t *ack)
{
    if (data == NULL || ack == NULL || len != 8U) {
        return false;
    }

    ack->phase = data[0];
    ack->source_id = read_u16_le(&data[1]);
    ack->result = data[3];
    ack->transfer_id = data[4];
    ack->detail = data[5];
    ack->processed_len = read_u16_le(&data[6]);
    return true;
}

