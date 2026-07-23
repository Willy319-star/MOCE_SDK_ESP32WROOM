#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH32_SYN6288_CAN_ID_START        0x430U
#define CH32_SYN6288_CAN_ID_DATA         0x431U
#define CH32_SYN6288_CAN_ID_ACK          0x500U

#define CH32_SYN6288_PROTOCOL_VERSION    0x01U
#define CH32_SYN6288_ACK_PHASE_START     0x30U
#define CH32_SYN6288_ACK_PHASE_COMPLETE  0x31U
#define CH32_SYN6288_DATA_PAYLOAD_SIZE   5U

typedef struct {
    uint8_t phase;
    uint16_t source_id;
    uint8_t result;
    uint8_t transfer_id;
    uint8_t detail;
    uint16_t processed_len;
} ch32_syn6288_ack_t;

uint16_t ch32_syn6288_protocol_crc16(const uint8_t *data, size_t len);

void ch32_syn6288_protocol_build_start(uint8_t transfer_id,
                                       uint16_t total_len,
                                       uint16_t crc16,
                                       uint8_t out[8]);

size_t ch32_syn6288_protocol_build_data(uint8_t transfer_id,
                                        uint16_t sequence,
                                        const uint8_t *payload,
                                        size_t payload_len,
                                        uint8_t out[8]);

size_t ch32_syn6288_protocol_chunk_count(size_t frame_len);

bool ch32_syn6288_protocol_parse_ack(const uint8_t *data,
                                     size_t len,
                                     ch32_syn6288_ack_t *ack);

#ifdef __cplusplus
}
#endif

