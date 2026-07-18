# CH32 Servo Gateway Final Module

## MOCE_MODULE_CONTRACT
module_id: ch32_servo_gateway_final
display_name: CH32 Four-Channel Servo Gateway
category: actuator
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_servo_gateway_final
example_paths:
  - example_final/servo_test0_final
protocol_contracts:
  - docs/context/protocols/ch32_servo_gateway_final.md
hardware_interface: can_gateway
downstream_interface: pwm
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h
  - CH32 servo gateway running fixed CH32_servo_gateway firmware
  - External servo power suitable for connected servos
capabilities:
  - Set four servo channels to 0..180 degree target angles through CH32 CAN commands.
unsupported:
  - Direct ESP32-WROOM PWM output to servo pins.
  - Reading servo position feedback.
user_phrases:
  - 控制四路舵机
  - 让 CH32 舵机板转到指定角度
  - ESP32WROOM 通过 CAN 控制舵机
failure_policy: warn
safe_defaults:
  - Start only after servo gateway HELLO is received.
  - Clamp angle commands to 0..180 degrees.
composition_notes:
  - Can be composed with sensor gateway capabilities when ESP32 only consumes reported sensor data and sends documented servo commands.
forbidden_contamination:
  - Direct motor PWM control.
  - Direct OLED I2C control.
  - ESP32-S3 direct-driver examples.
validation_status: compile_passed
## END_MOCE_MODULE_CONTRACT
