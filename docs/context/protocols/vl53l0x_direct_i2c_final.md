# VL53L0X Direct I2C Protocol

## MOCE_DIRECT_I2C_PROTOCOL_CONTRACT
protocol_id: vl53l0x_direct_i2c_final
kit_id: moce_esp32wroom_direct_i2c_vl53l0x
gateway_board: none
esp_board:
  - my_board_esp32wroom
transport: i2c
transport_config:
  bitrate_or_baud: 100000
  frame_endian: big_register_value
capability_id: vl53l0x_direct_i2c_distance
downstream_module: vl53l0x_laser_distance_sensor
ch32_firmware_status: not_used
commands:
  - name: probe_i2c_address
    id: i2c_probe_0x29
    direction: esp32_to_sensor
    payload:
      - addr7:uint8:i2c_address:0x29
    ack:
      required: true
      timeout_ms: 300
  - name: read_model_id
    id: register_read_0xC0
    direction: esp32_to_sensor
    payload:
      - reg:uint8:register:0xC0
      - len:uint8:bytes:1
    ack:
      required: true
      timeout_ms: 300
  - name: write_init_register
    id: register_write
    direction: esp32_to_sensor
    payload:
      - reg:uint8:register:0x00-0xFF
      - value:uint8:register_value:0x00-0xFF
    ack:
      required: true
      timeout_ms: 300
  - name: start_single_measurement
    id: register_write_0x00_0x01
    direction: esp32_to_sensor
    payload:
      - reg:uint8:register:0x00
      - value:uint8:register_value:0x01
    ack:
      required: true
      timeout_ms: 300
responses:
  - name: model_id
    id: register_0xC0_value
    direction: sensor_to_esp32
    payload:
      - model_id:uint8:n/a:expected 0xEE
  - name: measurement_ready
    id: register_0x14_status
    direction: sensor_to_esp32
    payload:
      - status:uint8:bitmask:bit0 ready
  - name: distance_mm
    id: register_0x1E_0x1F
    direction: sensor_to_esp32
    payload:
      - distance:uint16:mm:big endian raw value
failure_policy: retry
safe_defaults:
  - Probe address before initialization.
  - Verify model ID before returning READY.
  - Treat missing ACK as ADDR_NOT_FOUND.
  - Treat out-of-range distance separately from broken hardware.
  - Never print a valid distance unless the read result is OK.
unsupported:
  - CH32 node discovery or CAN frame parsing.
  - Multiple sensor address reassignment.
  - Full ST API calibration flow.
serial_log:
  - i2c initialized, probing VL53L0X
  - tof init result=OK
  - tof init result=ADDR_NOT_FOUND
  - tof init result=MODEL_ID_READ_FAIL
  - tof init result=MODEL_ID_MISMATCH
  - tof init result=CONFIG_FAIL
  - tof distance=<mm> mm raw=<raw> result=OK
  - tof distance out_of_range raw=<raw> result=OUT_OF_RANGE
  - tof read result=<reason>, reinitializing
## END_MOCE_DIRECT_I2C_PROTOCOL_CONTRACT
