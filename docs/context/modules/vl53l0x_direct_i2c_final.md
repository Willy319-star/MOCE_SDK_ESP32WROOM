# VL53L0X Direct I2C Module

## MOCE_MODULE_CONTRACT
module_id: vl53l0x_direct_i2c_final
display_name: VL53L0X Laser Distance Direct I2C
category: sensor
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_direct_i2c
gateway_required: false
component_paths:
  - components_esp32wroom/vl53l0x_direct_i2c_final
example_paths:
  - example_final/tof_direct_test0_final
protocol_contracts:
  - docs/context/protocols/vl53l0x_direct_i2c_final.md
hardware_interface: i2c
downstream_interface: i2c
required_resources:
  - ESP32-WROOM I2C0 SDA GPIO21
  - ESP32-WROOM I2C0 SCL GPIO22
  - VL53L0X address 0x29
  - Common GND and valid module power
capabilities:
  - Probe VL53L0X address 0x29 directly from ESP32-WROOM.
  - Verify model ID 0xEE.
  - Initialize VL53L0X directly over I2C.
  - Read distance in millimeters.
  - Report OUT_OF_RANGE separately from communication failure.
unsupported:
  - CH32 gateway discovery or CAN routing.
  - OLED rendering or motor/servo actions.
  - Multiple VL53L0X sensors with XSHUT address reassignment.
user_phrases:
  - ESP32WROOM direct VL53L0X distance measurement
  - direct read VL53L0X
  - VL53L0X connected to ESP32 I2C
failure_policy: retry
safe_defaults:
  - Default I2C address is 0x29.
  - Default I2C speed is 100 kHz.
  - Re-run initialization after runtime communication failure.
  - Do not treat OUT_OF_RANGE as a broken sensor.
composition_notes:
  - This direct variant is useful for board bring-up and hardware isolation.
  - Final gateway-based products should prefer `ch32_vl53l0x_gateway_final` unless the board contract explicitly selects direct I2C.
  - Keep distance reading as a driver API so voice, OLED, motor, or safety recipes can consume the result later without copying register logic.
forbidden_contamination:
  - CAN gateway code.
  - CH32 firmware assumptions.
  - OLED font tables or display behavior.
  - Motor, servo, VC02, or MPU6050 behavior unless a separate recipe selects those modules.
validation_status: compile_passed
## END_MOCE_MODULE_CONTRACT



