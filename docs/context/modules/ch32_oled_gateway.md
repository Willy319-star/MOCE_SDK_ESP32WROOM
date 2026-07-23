# CH32 SSD1315 OLED Gateway Module

## MOCE_MODULE_CONTRACT
module_id: ch32_oled_gateway
display_name: CH32 SSD1315 OLED Gateway
category: display
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_oled_gateway
example_paths:
  - example_final/oled_ch32_gateway_test
protocol_contracts:
  - docs/context/protocols/ch32_oled_gateway.md
hardware_interface: can_gateway
downstream_interface: i2c
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h at 50 kbit/s
  - CH32 I2C gateway running CH32_I2C_gateway_vl test firmware
  - CH32 I2C1 PB6 SCL and PB7 SDA
  - SSD1315 128x64 OLED at 7-bit I2C address 0x3C
capabilities:
  - Probe the downstream SSD1315 address through the CH32 gateway.
  - Send SSD1315 command and display-data bytes through acknowledged CH32 raw-I2C-write commands.
  - Initialize, clear, position, and display the example text Hello World.
unsupported:
  - Direct ESP32-WROOM I2C access to the OLED.
  - Reading display memory or confirming the pixels physically visible on the panel.
  - Arbitrary fonts, graphics, or Unicode text without additional ESP32-side rendering data.
user_phrases:
  - 让 ESP32 通过 CH32 控制 OLED
  - 在 OLED 上显示 Hello World
  - 通过 CAN 把显示数据转发给 CH32
failure_policy: retry
safe_defaults:
  - Wait for the CH32 HELLO frame before sending OLED commands.
  - Use CH32 node ID 1 for the standalone test.
  - Use SSD1315 7-bit I2C address 0x3C.
  - Require a successful CH32 ACK within 1000 ms for every probe or raw write.
  - Retry the complete OLED initialization after a failed command.
composition_notes:
  - OLED and another gateway module may share one CAN bus only when their CH32 boards use different node IDs or otherwise non-conflicting CAN IDs.
  - The standalone example assumes one CH32 I2C gateway at node ID 1.
forbidden_contamination:
  - ESP32 direct OLED I2C initialization or writes.
  - MPU6050 sampling or register configuration unless separately selected.
  - SYN6288 UART transfer behavior unless separately selected.
  - ESP32-S3 direct-driver examples.
validation_status: integrated_passed
## END_MOCE_MODULE_CONTRACT
