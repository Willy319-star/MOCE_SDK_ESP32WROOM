# CH32 SYN6288E UART Gateway Module

## MOCE_MODULE_CONTRACT
module_id: ch32_syn6288_gateway
display_name: CH32 SYN6288E Speech Gateway
category: output
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_syn6288_gateway
example_paths:
  - example_final/syn6288_ch32_gateway_test
protocol_contracts:
  - docs/context/protocols/ch32_syn6288_gateway.md
hardware_interface: can_gateway
downstream_interface: uart
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h at 50 kbit/s
  - CH32 UART gateway running CH32_UART_gateway test firmware
  - CH32 USART1 PA9 TX at 9600 baud 8N1
  - SYN6288E serial text-to-speech module
capabilities:
  - Transfer one preassembled opaque SYN6288E UART frame of 1 through 4096 bytes.
  - Validate the transfer with CRC-16/CCITT-FALSE and CH32 START and completion ACKs.
  - Play the hardware-tested one-shot SYN6288E frame for the text 宇音天下.
unsupported:
  - Direct ESP32-WROOM UART access to the SYN6288E.
  - Building arbitrary SYN6288E text frames or converting UTF-8 text to GBK.
  - SYN6288E Ready, Busy, playback-complete, or other UART receive feedback.
  - Concurrent speech transfers.
user_phrases:
  - 让 ESP32 通过 CH32 控制 SYN6288E 播放语音
  - 通过 CAN 把完整语音帧转发到 SYN6288E
  - 让 SYN6288E 播放宇音天下
failure_policy: warn
safe_defaults:
  - Allow only one active transfer.
  - Wait up to 500 ms for the START ACK and 6000 ms for the completion ACK.
  - Do not replay automatically after a completion-ACK timeout because the audio may already have played.
  - Wait 3000 ms after power-up before the standalone example sends its frame.
composition_notes:
  - Complete the current speech transfer before another gateway capability starts a high-traffic CAN transaction.
  - The fixed 0x430, 0x431, and 0x500 CAN IDs must not conflict with another node on the same bus.
forbidden_contamination:
  - ESP32 direct UART initialization or writes for the SYN6288E.
  - OLED, MPU6050, TOF, motor, WiFi, or Bluetooth features unless separately selected.
  - Automatic application-level replay after a final ACK timeout.
validation_status: integrated_passed
## END_MOCE_MODULE_CONTRACT
