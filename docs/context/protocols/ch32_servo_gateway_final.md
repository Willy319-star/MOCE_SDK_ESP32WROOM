# CH32 Servo Gateway Final Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_servo_gateway_final
kit_id: moce_esp32wroom_ch32_gateway
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: little
capability_id: ch32_servo_gateway_final
downstream_module: four_channel_servo_driver
ch32_firmware_status: fixed
commands:
  - name: set_servo_angle
    id: 0x400 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - channel:uint8:index:0..3
      - angle:uint16:degree:0..180
      - reserved:uint8[5]:n/a:0
    ack:
      required: true
      timeout_ms: 600
responses:
  - name: hello_or_heartbeat
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:0x03
      - node_id:uint8:n/a:1..10
      - fw_version:uint8:n/a:1
      - capability_flags:uint8:bitmask:0x0F
      - reserved:uint8[4]:n/a:0
  - name: command_ack
    id: 0x500 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - command_type:uint8:index:channel
      - result:uint8:boolean:0..1
      - node_id:uint8:n/a:1..10
      - device_type:uint8:n/a:0x03
      - reserved:uint8[4]:n/a:0
failure_policy: warn
safe_defaults:
  - Wait for HELLO before sending servo commands.
  - Clamp angle to 0..180 degrees.
  - Treat missing ACK as failed command and keep logging status.
unsupported:
  - ESP32-WROOM direct servo PWM generation for this gateway module.
  - Closed-loop servo position feedback.
serial_log:
  - hello servo node=<id> fw=<version> cap=<flags>
  - cmd servo node=<id> angle=<degree> ch0=<OK|FAILED> ch1=<OK|FAILED> ch2=<OK|FAILED> ch3=<OK|FAILED>
## END_MOCE_CH32_PROTOCOL_CONTRACT
