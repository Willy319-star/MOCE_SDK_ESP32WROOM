# VC02 Voice Direct UART Module

## MOCE_MODULE_CONTRACT
module_id: vc02_direct_uart_final
display_name: VC02 Voice Recognition Direct UART
category: input
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_direct_uart_debug
gateway_required: false
component_paths:
  - components_esp32wroom/vc02_direct_uart_final
example_paths:
  - example_final/vc02_direct_test0_final
protocol_contracts:
  - docs/context/protocols/vc02_direct_uart_final.md
hardware_interface: uart
downstream_interface: uart
required_resources:
  - ESP32-WROOM UART1 external serial header
  - ESP32 GPIO17 TX connected to VC02 RX
  - ESP32 GPIO16 RX connected to VC02 TX
  - Common GND and valid VC02 power
capabilities:
  - Initialize ESP32 UART for VC02 direct hardware verification.
  - Receive VC02 UART bytes directly on ESP32.
  - Parse fixed VC02 command bytes into semantic commands.
  - Dispatch recognized commands through application callbacks.
unsupported:
  - CH32 gateway validation.
  - Direct control of OLED, motor, servo, MPU6050, or VL53L0X from the parser.
  - VC02 firmware generation or flashing.
  - Treating this direct UART debug module as the final CH32 gateway architecture.
user_phrases:
  - 直连 VC02 测试
  - ESP32WROOM 直接读取 VC02 串口
  - 验证 VC02 是否会发送串口命令
  - 语音识别串口数据解析
failure_policy: warn
safe_defaults:
  - Default UART is UART1.
  - Default TX is GPIO17.
  - Default RX is GPIO16.
  - Default baud is 115200.
  - Unknown bytes are logged and ignored.
composition_notes:
  - This module is for VC02 bring-up and bench validation.
  - For final products using CH32, prefer `ch32_vc02_gateway_final`.
  - Callback slots mirror the CH32 gateway version so parser behavior can be compared across direct and bridged paths.
forbidden_contamination:
  - CAN/TWAI bridge assumptions.
  - CH32 firmware requirements.
  - OLED font tables, motor PWM, servo PWM, MPU6050 parsing, or TOF ranging logic inside the VC02 parser.
validation_status: board_passed
## END_MOCE_MODULE_CONTRACT
