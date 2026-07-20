# CH32 Motor Gateway Final Module

## MOCE_MODULE_CONTRACT
module_id: ch32_motor_gateway_final
display_name: CH32 Two-Channel Motor Gateway
category: actuator
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_motor_gateway_final
example_paths:
  - example_final/motor_test0_final
protocol_contracts:
  - docs/context/protocols/ch32_motor_gateway_final.md
hardware_interface: can_gateway
downstream_interface: pwm
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h
  - CH32 motor gateway running fixed CH32_motor_gateway firmware
  - External motor power suitable for connected motors
capabilities:
  - Set two DC motor channels to open-loop duty and direction through CH32 CAN commands.
  - Stop either channel by sending duty 0.
unsupported:
  - Direct ESP32-WROOM PWM output to motor driver pins.
  - Closed-loop speed, distance, or position control.
user_phrases:
  - 控制两路电机
  - 让 CH32 电机驱动板转动
  - ESP32WROOM 通过 CAN 控制电机 duty
failure_policy: safe_stop
safe_defaults:
  - Start only after motor gateway HELLO is received.
  - Clamp duty commands to 0..1000 permille.
  - Use duty 0 for stop.
composition_notes:
  - Can be composed with sensor gateway capabilities when ESP32 receives sensor data and sends documented motor commands.
  - Safety-critical recipes should stop motors when sensor data or ACK times out.
forbidden_contamination:
  - Direct servo PWM control.
  - Direct OLED I2C control.
  - ESP32-S3 direct-driver examples.
validation_status: compile_passed
## END_MOCE_MODULE_CONTRACT
