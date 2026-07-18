# Servo Test0 Final Recipe

## MOCE_RECIPE_CONTRACT
recipe_id: servo_test0_final
purpose: Sweep four CH32-controlled servo channels through 0, 90, 180, and 90 degrees.
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - ch32_servo_gateway_final
protocol_contracts:
  - ch32_servo_gateway_final
user_goal:
  - ESP32WROOM 通过 CAN 控制 CH32 舵机驱动板
required_behavior:
  - ESP32 waits for the CH32 servo HELLO frame.
  - ESP32 sends angle commands to channels 0..3.
  - ESP32 checks ACK for every channel command.
esp32_responsibilities:
  - Initialize CAN at 50 kbit/s.
  - Detect DEVICE_TYPE_SERVO from HELLO frames.
  - Encode documented servo angle command frames.
  - Log ACK success or timeout per channel.
ch32_responsibilities:
  - Generate 20 ms servo PWM locally.
  - Map ch0=PA6, ch1=PA7, ch2=PB6, ch3=PB7.
serial_log:
  - hello servo node=<id> fw=<version> cap=<flags>
  - cmd servo node=<id> angle=<degree> ch0=<OK|FAILED> ch1=<OK|FAILED> ch2=<OK|FAILED> ch3=<OK|FAILED>
state_machine:
  - wait_hello
  - sweep_servo_angles
  - warn_on_ack_timeout
failure_behavior:
  - Missing HELLO blocks command start.
  - Missing ACK marks that channel command FAILED and continues status logging.
must_not_include:
  - ESP32 direct PWM servo generation.
  - OLED, motor, MPU6050, TOF, WiFi, or Bluetooth behavior.
compile_command: .\tools\build.ps1 example_final/servo_test0_final esp32 my_board_esp32wroom
hardware_test_status: untested
## END_MOCE_RECIPE_CONTRACT
