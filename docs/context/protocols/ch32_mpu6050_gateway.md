# CH32 MPU6050 Gateway Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_mpu6050_gateway
kit_id: moce_esp32wroom_ch32_gateway
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: n/a
capability_id: ch32_mpu6050_gateway
downstream_module: MPU6050 with AD0 low at I2C address 0x68
ch32_firmware_status: test_firmware
commands:
  - name: probe_mpu6050
    id: 0x200 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - command:uint8:n/a:0x02
      - i2c_address:uint8:7_bit_address:0x68
    ack:
      required: true
      timeout_ms: 1000
  - name: write_mpu6050_register
    id: 0x200 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - command:uint8:n/a:0x03
      - i2c_address:uint8:7_bit_address:0x68
      - register:uint8:register_address:0x00..0xFF
      - payload_length:uint8:bytes:1
      - value:uint8:register_value:0x00..0xFF
    ack:
      required: true
      timeout_ms: 1000
  - name: read_mpu6050_registers
    id: 0x200 + NODE_ID
    direction: esp32_to_ch32
    payload:
      - command:uint8:n/a:0x04
      - i2c_address:uint8:7_bit_address:0x68
      - first_register:uint8:register_address:0x00..0xFF
      - requested_length:uint8:bytes:1..32
      - request_id:uint8:n/a:1..255
    ack:
      required: true
      timeout_ms: 1000
responses:
  - name: hello_or_heartbeat
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:0x01
      - node_id:uint8:n/a:1..10
      - fw_version:uint8:n/a:2
      - capability_flags:uint8:bitmask:0x7F
      - reserved:uint8[4]:n/a:0
  - name: read_data_chunk
    id: 0x100 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - status_type:uint8:n/a:0x05
      - request_id:uint8:n/a:1..255
      - offset:uint8:bytes:0..31
      - chunk_length:uint8:bytes:1..4
      - register_data:uint8[4]:register_bytes:unused_bytes_zero
  - name: read_complete
    id: 0x100 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - status_type:uint8:n/a:0x06
      - request_id:uint8:n/a:1..255
      - i2c_address:uint8:7_bit_address:0x68
      - first_register:uint8:register_address:0x00..0xFF
      - requested_length:uint8:bytes:1..32
      - result:uint8:boolean:0..1
      - processed_length:uint8:bytes:0..32
      - reserved:uint8:n/a:0
  - name: command_ack
    id: 0x500 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - command:uint8:n/a:0x02_or_0x03_or_0x04
      - result:uint8:boolean:0..1
      - node_id:uint8:n/a:1..10
      - device_type:uint8:n/a:0x01
      - reserved:uint8[4]:n/a:0
failure_policy: retry
safe_defaults:
  - Use node ID 1 only for the standalone test.
  - Wait for HELLO before probing or configuring the sensor.
  - Require WHO_AM_I 0x68 before starting periodic samples.
  - Require every requested byte, a successful read-complete frame, and a successful command ACK.
  - Use a 1000 ms transaction deadline and a 500 ms application sample period.
unsupported:
  - ESP32-WROOM direct MPU6050 I2C initialization or reads.
  - Register reads longer than 32 bytes in one request.
  - DMP, quaternion, attitude, FIFO, interrupt, or calibration protocols.
serial_log:
  - ESP32 -> CAN -> CH32 node <id> -> MPU6050
  - CH32 node <id> online, firmware=<version> capabilities=<flags>
  - MPU6050 initialized, WHO_AM_I=0x68
  - mpu6050 <sample> ax=<g> ay=<g> az=<g> gx=<dps> gy=<dps> gz=<dps> temp=<C>
  - Read reg <register> failed: done=<value> read_ok=<value> ack=<value> ack_ok=<value> bytes=<received>/<requested>
## END_MOCE_CH32_PROTOCOL_CONTRACT

All messages are 11-bit standard CAN data frames. MPU6050 register data is
returned in the sensor's native byte order; the example combines each high
byte followed by its low byte into signed 16-bit samples. The example uses
accelerometer scale `16384 LSB/g`, gyroscope scale `131 LSB/(degree/s)`, and
temperature conversion `raw / 340 + 36.53`.
