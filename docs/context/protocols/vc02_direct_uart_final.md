# ESP32 UART VC02 Direct Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: vc02_direct_uart_final
kit_id: moce_esp32wroom_direct_uart_vc02
gateway_board: none
esp_board:
  - my_board_esp32wroom
transport: uart
transport_config:
  bitrate_or_baud: 115200
  frame_endian: n/a
capability_id: vc02_direct_uart_final_command
downstream_module: vc02_module
ch32_firmware_status: n/a
commands:
  - name: none
    id: n/a
    direction: n/a
    payload:
      - VC02 only sends UART command bytes to ESP32 in this direct test.
    ack:
      required: false
      timeout_ms: 0
responses:
  - name: vc02_uart_rx
    id: n/a
    direction: vc02_to_esp32
    payload:
      - uart_bytes:uint8[]:bytes:VC02 command bytes
failure_policy: warn
safe_defaults:
  - Do not dispatch a voice command unless its UART byte sequence is recognized.
  - Unknown VC02 bytes are logged and ignored.
  - No selected downstream module action is executed by this direct parser.
unsupported:
  - CH32 CAN forwarding.
  - CH32 ACK/status handling.
  - VC02 firmware generation or command-word modification.
serial_log:
  - VC02_EVENT name=<command> uart="<bytes>" meaning="<meaning>" slot=<slot> raw=[...]
  - ACTION_SLOT <slot>: <meaning> -> replace this callback with a selected module API raw=[...]
  - VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
  - VC02_DIRECT_STATUS uart_ready=1 uart=UART1 tx_gpio=17 rx_gpio=16 baud=115200 chunks=<n> bytes=<n> matched=<n> dispatched=<n> no_match=<n> ascii_window="<recent bytes>"
## END_MOCE_CH32_PROTOCOL_CONTRACT

## VC02 Command Bytes

| Driver command | Expected VC02 UART bytes | Meaning |
| --- | --- | --- |
| `WAKEUP` | `TX WK 00` | Wakeup detected |
| `OLED_CLEAR` | `TX CL OL ED` | Clear OLED screen |
| `MOTOR_START` | `TX SA MO TO` | Start motor |
| `OLED_REFRESH` | `TX RF OL ED` | Refresh OLED screen |
| `SERVO_START` | `TX SA SE VO` | Start servo |
| `MPU6050_SHOW` | `TX SA MP U0` | Show MPU6050 angle data |
| `SYN_STOP` | `TX SO SY NO` | Stop voice broadcast |
| `SYN_START` | `TX SA SY NO` | Start voice broadcast |
| `MOTOR_STOP` | `TX SO MO TO` | Stop motor |
| `SERVO_STOP` | `TX SO SE VO` | Stop servo |
| `VL53L0X_SHOW` | `TX SA VL 00` | Show VL53L0X distance data |

## Notes

This file uses the protocol-contract block so MOCE Designer can consume the same metadata shape as gateway modules. It is intentionally marked `gateway_board: none` and `ch32_firmware_status: n/a` because this is a direct UART bring-up path, not the final CH32 gateway path.
