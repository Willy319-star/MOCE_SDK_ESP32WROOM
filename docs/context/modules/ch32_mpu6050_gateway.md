# CH32 MPU6050 Gateway Module

## MOCE_MODULE_CONTRACT
module_id: ch32_mpu6050_gateway
display_name: CH32 MPU6050 Sensor Gateway
category: sensor
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - components_esp32wroom/ch32_mpu6050_gateway
example_paths:
  - example_final/mpu6050_ch32_gateway_test
protocol_contracts:
  - docs/context/protocols/ch32_mpu6050_gateway.md
hardware_interface: can_gateway
downstream_interface: i2c
required_resources:
  - ESP32-WROOM CAN TX/RX pins from board.h at 50 kbit/s
  - CH32 I2C gateway running CH32_I2C_gateway_vl test firmware
  - CH32 I2C1 PB6 SCL and PB7 SDA
  - MPU6050 with AD0 low at 7-bit I2C address 0x68
capabilities:
  - Probe the downstream MPU6050 through the CH32 gateway.
  - Write MPU6050 configuration registers through acknowledged CH32 commands.
  - Read 1 through 32 consecutive register bytes using request IDs, four-byte data chunks, a completion status, and a command ACK.
  - Read and report acceleration, angular velocity, and temperature every 500 ms in the standalone example.
unsupported:
  - Direct ESP32-WROOM I2C access to the MPU6050.
  - DMP firmware, quaternion output, attitude calculation, or sensor fusion.
  - Calibrated motion measurements, FIFO streaming, or interrupt-driven sampling.
user_phrases:
  - 让 ESP32 通过 CH32 读取 MPU6050
  - 每 500 毫秒读取一次加速度和陀螺仪
  - 通过 CAN 获取 MPU6050 温度和运动数据
failure_policy: retry
safe_defaults:
  - Wait for the CH32 HELLO frame before accessing the sensor.
  - Use CH32 node ID 1 for the standalone test.
  - Use MPU6050 7-bit I2C address 0x68 and require WHO_AM_I 0x68.
  - Use the plus or minus 2 g accelerometer range and plus or minus 250 degrees-per-second gyroscope range.
  - Require read completion, all requested chunks, and a successful ACK within 1000 ms.
  - Log failed samples and continue the 500 ms sampling loop.
composition_notes:
  - MPU6050 data may drive later display or control recipes on the ESP32, but only through separately selected gateway capabilities.
  - Multiple CH32 boards on one CAN bus must use distinct node IDs.
forbidden_contamination:
  - ESP32 direct MPU6050 I2C initialization or reads.
  - OLED display behavior unless separately selected.
  - SYN6288 speech behavior unless separately selected.
  - Unselected motor, servo, WiFi, or Bluetooth behavior.
validation_status: integrated_passed
## END_MOCE_MODULE_CONTRACT
