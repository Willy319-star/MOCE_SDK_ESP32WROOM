# VC02 Voice CH32 Gateway Module

## MOCE_MODULE_CONTRACT
module_id: ch32_vc02_gateway_final
display_name: VC02 Voice Recognition Over CH32 UART Bridge
category: input
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_vc02_gateway_final
example_paths:
  - example_final/vc02_test0_final
protocol_contracts:
  - docs/context/protocols/ch32_vc02_gateway_final.md
hardware_interface: can_gateway
downstream_interface: uart
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h or component defaults GPIO5/GPIO4
  - CH32 running VC02 UART bridge firmware
  - VC02 connected to CH32 UART
  - Common GND and valid VC02 power
capabilities:
  - Receive VC02 UART command bytes forwarded by CH32.
  - Parse fixed VC02 command bytes into semantic ESP32 commands.
  - Dispatch semantic commands through application-registered callbacks.
  - Keep VC02 parsing separate from OLED, motor, servo, MPU6050, and VL53L0X drivers.
unsupported:
  - ESP32-WROOM direct UART connection to VC02 in this gateway capability.
  - VC02 firmware generation or firmware flashing.
  - Direct OLED, motor, servo, MPU6050, or VL53L0X control from the VC02 parser.
  - Treating unverified CH32 CAN IDs as production-fixed without CH32 protocol confirmation.
user_phrases:
  - 使用 VC02 语音识别
  - 通过语音清屏
  - 通过语音启动电机
  - 通过语音停止电机
  - 通过语音显示距离
  - ESP32WROOM 通过 CH32 读取 VC02 指令
failure_policy: warn
safe_defaults:
  - Default CAN bitrate is 50 kbit/s.
  - Default VC02 UART baud is 115200 on the CH32 side.
  - Unknown VC02 bytes are logged and ignored.
  - Recognized commands without registered callbacks are logged and ignored.
  - If the CH32 bridge times out, do not execute downstream actions.
composition_notes:
  - VC02 is an input capability. It should produce semantic commands, not directly own downstream modules.
  - Final applications should bind VC02 commands to selected module APIs through callbacks.
  - Keep callback slots close to command names: `slot_oled_clear`, `slot_motor_start`, `slot_vl53l0x_show`, and similar.
  - If a selected downstream module is absent or unhealthy, the callback should log the blocked action and keep the system safe.
  - If a new VC02 command is added in firmware, add the UART byte pattern, semantic enum, and action slot in the driver before writing product logic.
  - A future multi-CH32 project should route CH32 frames by node id before feeding VC02 UART bytes into this driver.
forbidden_contamination:
  - ESP32-S3 target settings.
  - ESP32 local UART VC02 implementation in this CH32 gateway module.
  - OLED font tables or display rendering inside the VC02 parser.
  - Motor PWM, servo PWM, I2C sensor register logic, or TOF ranging logic inside the VC02 parser.
  - Recipe-level behavior that assumes OLED, motor, servo, MPU6050, or VL53L0X is selected.
validation_status: compile_passed
## END_MOCE_MODULE_CONTRACT
