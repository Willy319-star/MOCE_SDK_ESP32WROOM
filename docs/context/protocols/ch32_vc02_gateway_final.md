# CH32 UART Bridge VC02 Protocol

This protocol document records the ESP32-side expected contract for VC02 over a CH32 UART bridge.

The CH32 VC02 bridge firmware is not available locally yet, so this contract is marked as draft. Before MOCE Designer treats this as a fixed capability, CH32 firmware must confirm the CAN IDs, payload fields, node-id behavior, ACK/status semantics, and UART forwarding chunk rules.

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_vc02_gateway_final
kit_id: moce_esp32wroom_ch32_uart_vc02
gateway_board: ch32_uart_bridge
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: n/a
capability_id: ch32_vc02_gateway_final_command
downstream_module: vc02_module
ch32_firmware_status: draft
commands:
  - name: ping_bridge
    id: 0x730
    direction: esp32_to_ch32
    payload:
      - op:uint8:n/a:0x01 in current ESP32 example
      - seq:uint8:n/a:0-255
    ack:
      required: true
      timeout_ms: 600
responses:
  - name: bridge_status
    id: 0x732
    direction: ch32_to_esp32
    payload:
      - raw:uint8[]:bytes:CH32 firmware must define fields
  - name: vc02_uart_rx
    id: 0x733
    direction: ch32_to_esp32
    payload:
      - uart_bytes:uint8[]:bytes:1-8 raw bytes forwarded from VC02 UART RX
  - name: bridge_ack
    id: 0x734
    direction: ch32_to_esp32
    payload:
      - raw:uint8[]:bytes:CH32 firmware must define fields
failure_policy: warn
safe_defaults:
  - Do not dispatch a voice command unless its UART byte sequence is recognized.
  - Unknown VC02 bytes are logged and ignored.
  - Missing CH32 ACK/status must not trigger downstream module actions.
  - Keep VC02 parser independent from downstream module state machines.
unsupported:
  - Direct ESP32-WROOM UART access to VC02 in this gateway capability.
  - Inferring CH32 status fields before CH32 firmware confirms them.
  - Generating VC02 firmware or changing VC02 command words.
serial_log:
  - VC02_EVENT name=<command> uart="<bytes>" meaning="<meaning>" slot=<slot> raw=[...]
  - ACTION_SLOT <slot>: <meaning> -> replace this callback with a real module function raw=[...]
  - VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
  - VC02_DRIVER_STATUS chunks=<n> bytes=<n> matched=<n> dispatched=<n> no_match=<n> ascii_window="<recent bytes>"
  - VC02_BRIDGE_STATUS raw=[...]
  - VC02_BRIDGE_ACK raw=[...]
## END_MOCE_CH32_PROTOCOL_CONTRACT

## VC02 Command Bytes

| Driver command | Expected VC02 UART bytes | Meaning |
| --- | --- | --- |
| `WAKEUP` | `TX WK 00` or `00 TX WK 00` | Wakeup detected |
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

## CH32 Information Still Needed

The ESP32 component currently needs the future CH32 VC02 bridge code to confirm:

- Whether `0x730`, `0x732`, `0x733`, and `0x734` are the final CAN IDs.
- Whether a CH32 node id is included in status, ACK, or UART forwarding frames.
- Whether VC02 bytes are forwarded as raw binary bytes, ASCII text, or both.
- The maximum UART byte count forwarded in one CAN frame.
- Whether forwarded UART frames can split a VC02 command across multiple CAN frames.
- The exact status payload fields.
- The exact ACK payload fields and failure codes.
- The confirmed VC02 UART baud rate and UART format on the CH32 side.
