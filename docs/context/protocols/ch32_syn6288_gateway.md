# CH32 SYN6288E UART Gateway Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_syn6288_gateway
kit_id: moce_esp32wroom_ch32_gateway
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: little
capability_id: ch32_syn6288_gateway
downstream_module: SYN6288E serial text-to-speech module on CH32 PA9 UART
ch32_firmware_status: test_firmware
commands:
  - name: syn6288_transfer_start
    id: 0x430
    direction: esp32_to_ch32
    payload:
      - transfer_id:uint8:n/a:0..255
      - total_length:uint16:bytes:1..4096
      - crc16_ccitt_false:uint16:n/a:0..65535
      - protocol_version:uint8:n/a:1
      - reserved:uint16:n/a:0
    ack:
      required: true
      timeout_ms: 500
  - name: syn6288_transfer_data
    id: 0x431
    direction: esp32_to_ch32
    payload:
      - transfer_id:uint8:n/a:0..255
      - sequence:uint16:fragment_index:0..819
      - raw_frame_bytes:uint8[1..5]:bytes:opaque_SYN6288E_frame_data
    ack:
      required: true
      timeout_ms: 6000
responses:
  - name: gateway_ack
    id: 0x500
    direction: ch32_to_esp32
    payload:
      - phase:uint8:n/a:0x30_start_or_0x31_complete
      - source_id:uint16:n/a:0x430_or_0x431
      - result:uint8:boolean:0..1
      - transfer_id:uint8:n/a:0..255
      - detail:uint8:error_code:0..10
      - processed_length:uint16:bytes:0..4096
failure_policy: warn
safe_defaults:
  - Allow one active transfer at a time.
  - Require a successful START ACK before sending DATA frames.
  - Require monotonically increasing DATA sequence values starting at zero.
  - Use a 500 ms CH32 inter-fragment timeout.
  - Do not automatically replay after a missing completion ACK.
unsupported:
  - ESP32-WROOM direct SYN6288E UART initialization or writes.
  - Frames larger than 4096 bytes or concurrent transfers.
  - SYN6288E UART receive, Ready, Busy, or playback-complete feedback.
  - Automatic UTF-8 to GBK conversion or arbitrary text-frame construction.
serial_log:
  - transfer=<id> length=<bytes> fragments=<count> crc=<crc16>
  - ACK phase=<phase> source=<can_id> result=<result> transfer=<id> detail=<detail> processed=<bytes>
  - TWAI state=<state> tx_err=<count> rx_err=<count>
  - one-shot SYN6288E transfer completed successfully
## END_MOCE_CH32_PROTOCOL_CONTRACT

All messages are 11-bit standard CAN data frames. START uses DLC 8. DATA uses
DLC 4 through 8 and carries one through five opaque SYN6288E bytes. Completion
is implied when the declared total length is received.

CRC uses CRC-16/CCITT-FALSE with polynomial `0x1021`, initial value `0xFFFF`,
no reflection, and XOR output `0x0000`. The raw payload includes every byte of
the complete SYN6288E UART frame, including its own trailing XOR checksum when
required. The CH32 forwards the reassembled bytes unchanged on USART1 PA9 at
9600 baud, 8 data bits, no parity, and 1 stop bit.

ACK detail codes are: `0` success, `1` invalid DLC, `2` invalid length, `3`
busy, `4` transfer ID/state error, `5` sequence error, `6` fragment-length
error, `7` fragment timeout, `8` CRC error, `9` UART timeout, and `10`
unsupported protocol version.
