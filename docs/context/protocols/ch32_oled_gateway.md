# CH32 SSD1315 OLED Gateway Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_oled_gateway
kit_id: moce_esp32wroom_ch32_gateway
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: n/a
capability_id: ch32_oled_gateway
downstream_module: SSD1315 128x64 OLED at I2C address 0x3C
ch32_firmware_status: test_firmware
commands:
  - name: probe_oled
    id: 0x200 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - command:uint8:n/a:0x02
      - i2c_address:uint8:7_bit_address:0x3C
    ack:
      required: true
      timeout_ms: 1000
  - name: write_oled_raw
    id: 0x200 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - command:uint8:n/a:0x05
      - i2c_address:uint8:7_bit_address:0x3C
      - payload_length:uint8:bytes:1..5
      - raw_i2c_payload:uint8[1..5]:bytes:SSD1315_control_and_data
    ack:
      required: true
      timeout_ms: 1000
responses:
  - name: hello_or_heartbeat
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:0x01
      - node_id:uint8:n/a:1..10
      - fw_version:uint8:n/a:2
      - capability_flags:uint8:bitmask:0x7F
      - reserved:uint8[4]:n/a:0
  - name: probe_status
    id: 0x100 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - status_type:uint8:n/a:0x02
      - i2c_address:uint8:7_bit_address:0x3C
      - result:uint8:boolean:0..1
      - reserved:uint8[5]:n/a:0
  - name: raw_write_status
    id: 0x100 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - status_type:uint8:n/a:0x07
      - i2c_address:uint8:7_bit_address:0x3C
      - register_placeholder:uint8:n/a:0
      - payload_length:uint8:bytes:1..5
      - result:uint8:boolean:0..1
      - reserved:uint8[3]:n/a:0
  - name: command_ack
    id: 0x500 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - command:uint8:n/a:0x02_or_0x05
      - result:uint8:boolean:0..1
      - node_id:uint8:n/a:1..10
      - device_type:uint8:n/a:0x01
      - reserved:uint8[4]:n/a:0
failure_policy: retry
safe_defaults:
  - Use node ID 1 only for the standalone test.
  - Wait for HELLO before sending the first OLED command.
  - Treat a failed or missing ACK as a failed OLED operation.
  - Send at most five downstream I2C bytes per raw-write command.
  - Retry the complete OLED initialization after an operation fails.
unsupported:
  - ESP32-WROOM direct OLED I2C initialization or writes.
  - Raw-write payloads longer than five bytes in one CAN command.
  - Display-memory readback or physical-pixel confirmation.
serial_log:
  - ESP32 -> CAN -> CH32 node <id> -> SSD1315 OLED
  - CH32 node <id> online, firmware=<version> capabilities=<flags>
  - SSD1315 detected
  - OLED now displays: Hello World
  - ACK timeout for command <command>
## END_MOCE_CH32_PROTOCOL_CONTRACT

All messages are 11-bit standard CAN data frames. The OLED uses SSD1315
control byte `0x00` for commands and `0x40` for display data. Because the CH32
raw-write payload limit is five bytes, the example sends one control byte plus
at most four SSD1315 command or data bytes per acknowledged CAN transaction.
