# Motor Test0 Final Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: motor_test0_final
purpose: Sweep two CH32-controlled motor channels through open-loop duty values.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_motor_gateway_final
protocol_contracts:
  - ch32_motor_gateway_final
user_goal:
  - ESP32WROOM 通过 CAN 控制 CH32 电机驱动板
required_behavior:
  - ESP32 waits for the CH32 motor HELLO frame.
  - ESP32 sends duty commands to channels 0 and 1.
  - ESP32 checks ACK for every channel command.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Detect DEVICE_TYPE_MOTOR from HELLO frames.
  - Encode documented motor duty command frames.
  - Log ACK success or timeout per channel.
ch32_responsibilities:
  - Generate motor PWM locally.
  - Map ch0=PA7 PWM + PA5 DIR and ch1=PA6 PWM + PA4 DIR.
serial_log:
  - hello motor node=<id> fw=<version> cap=<flags>
  - cmd motor node=<id> duty=<permille> dir=0 ch0=<OK|FAILED> ch1=<OK|FAILED>
state_machine:
  - wait_hello
  - sweep_motor_duty
  - warn_on_ack_timeout
failure_behavior:
  - Missing HELLO blocks command start.
  - Missing ACK marks that channel command FAILED and does not assume the motor changed state.
  - Safety recipes should send duty 0 when input data or ACKs time out.
must_not_include:
  - ESP32 direct PWM motor generation.
  - OLED, servo, MPU6050, TOF, WiFi, or Bluetooth behavior.
compile_command: .\tools\build.ps1 example_final/motor_test0_final esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
