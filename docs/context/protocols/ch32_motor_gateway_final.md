# CH32 Motor Gateway Final Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_motor_gateway_final
kit_id: moce_esp32wroom_ch32_gateway
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: little
capability_id: ch32_motor_gateway_final
downstream_module: two_channel_dc_motor_driver
ch32_firmware_status: fixed
commands:
  - name: set_motor_duty
    id: 0x300 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - channel:uint8:index:0..1
      - duty_permille:uint16:permille:0..1000
      - direction:uint8:boolean:0..1
      - reserved:uint8[4]:n/a:0
    ack:
      required: true
      timeout_ms: 600
responses:
  - name: hello_or_heartbeat
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:0x02
      - node_id:uint8:n/a:1..10
      - fw_version:uint8:n/a:1
      - capability_flags:uint8:bitmask:0x03
      - reserved:uint8[4]:n/a:0
  - name: command_ack
    id: 0x500 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - command_type:uint8:index:channel
      - result:uint8:boolean:0..1
      - node_id:uint8:n/a:1..10
      - device_type:uint8:n/a:0x02
      - reserved:uint8[4]:n/a:0
failure_policy: safe_stop
safe_defaults:
  - Wait for HELLO before sending motor commands.
  - Clamp duty to 0..1000 permille.
  - Use duty 0 as stop command.
  - Treat missing ACK as failed command and do not assume motor state changed.
unsupported:
  - ESP32-WROOM direct motor PWM generation for this gateway module.
  - Closed-loop speed, distance, or position control without an encoder protocol.
serial_log:
  - hello motor node=<id> fw=<version> cap=<flags>
  - cmd motor node=<id> duty=<permille> dir=<direction> ch0=<OK|FAILED> ch1=<OK|FAILED>
## END_MOCE_CH32_PROTOCOL_CONTRACT
