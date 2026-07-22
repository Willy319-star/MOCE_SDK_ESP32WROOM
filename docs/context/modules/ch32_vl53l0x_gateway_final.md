# VL53L0X TOF CH32 Gateway Module

## MOCE_MODULE_CONTRACT
module_id: ch32_vl53l0x_gateway_final
display_name: VL53L0X Laser Distance Over CH32 I2C Bridge
category: sensor
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_vl53l0x_gateway_final
example_paths:
  - example_final/tof_test0_final
protocol_contracts:
  - docs/context/protocols/ch32_vl53l0x_gateway_final.md
hardware_interface: can_gateway
downstream_interface: i2c
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h or component defaults GPIO5/GPIO4
  - CH32 running examples_final/CH32_I2C_bridge_generic
  - VL53L0X connected to CH32 I2C bus
  - Common GND and valid module power
capabilities:
  - Probe VL53L0X address 0x29 through CH32.
  - Verify model ID 0xEE through CH32.
  - Initialize VL53L0X through CH32 I2C register transactions.
  - Read distance in millimeters.
  - Report OUT_OF_RANGE separately from communication failure.
unsupported:
  - ESP32-WROOM direct I2C connection to VL53L0X.
  - CH32-side sensor-specific measurement algorithm in this module.
  - OLED rendering or motor/servo actions.
user_phrases:
  - è¯»å–æ¿€å…‰æµ‹è·
  - æ˜¾ç¤ºè·ç¦»
  - ESP32WROOM é€šè¿‡ CH32 è¯»å– VL53L0X
  - é€šè¿‡ CAN-I2C æ¡¥æŽ¥è¯»å–æµ‹è·æ¨¡å—
failure_policy: retry
safe_defaults:
  - Default CH32 node id is 1.
  - Default VL53L0X address is 0x29.
  - Default CAN bitrate is 50 kbit/s.
  - Re-run initialization after runtime communication failure.
  - Do not treat OUT_OF_RANGE as a broken sensor.
composition_notes:
  - Final applications can combine this module with OLED, voice, servo, or motor modules by consuming `ch32_vl53l0x_status_t` and distance results.
  - A future shared CAN manager may own TWAI initialization; this component currently owns TWAI only for the minimal standalone example.
  - Keep the ESP32-side state fields so a multi-CH32 project can map node id, I2C address, device model, and health status.
forbidden_contamination:
  - ESP32-S3 target settings.
  - Direct ESP32 I2C driver calls for VL53L0X.
  - CH32 firmware modifications embedded inside ESP32 examples.
  - OLED font tables or display-specific behavior in this TOF module.
validation_status: board_passed
## END_MOCE_MODULE_CONTRACT

