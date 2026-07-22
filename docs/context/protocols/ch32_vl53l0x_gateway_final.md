# CH32 I2C Bridge VL53L0X Protocol

## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: ch32_vl53l0x_gateway_final
kit_id: moce_esp32wroom_ch32_i2c_vl53l0x
gateway_board: ch32_generic_i2c_bridge
esp_board:
  - my_board_esp32wroom
transport: can
transport_config:
  bitrate_or_baud: 50000
  frame_endian: little
capability_id: vl53l0x_tof_distance_over_ch32
downstream_module: vl53l0x_laser_distance_sensor
ch32_firmware_status: fixed
commands:
  - name: wait_gateway_hello
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:gateway identity
      - node_id:uint8:n/a:1
      - fw_version:uint8:n/a:bridge firmware version
      - capability_flags:uint8:bitmask:i2c bridge capability
    ack:
      required: false
      timeout_ms: 1500
  - name: probe_i2c_address
    id: bridge-defined command frame
    direction: esp32_to_ch32
    payload:
      - op:uint8:operation:probe
      - node_id:uint8:n/a:1
      - addr7:uint8:i2c_address:0x29
    ack:
      required: true
      timeout_ms: 300
  - name: write_i2c_register
    id: bridge-defined command frame
    direction: esp32_to_ch32
    payload:
      - op:uint8:operation:write
      - node_id:uint8:n/a:1
      - addr7:uint8:i2c_address:0x29
      - reg:uint8:register:0x00-0xFF
      - len:uint8:bytes:1-4
      - data:uint8[]:bytes:len
    ack:
      required: true
      timeout_ms: 300
  - name: read_i2c_registers
    id: bridge-defined command frame
    direction: esp32_to_ch32
    payload:
      - op:uint8:operation:read
      - node_id:uint8:n/a:1
      - addr7:uint8:i2c_address:0x29
      - reg:uint8:register:0x00-0xFF
      - len:uint8:bytes:1-4 in current ESP32 helper path
    ack:
      required: true
      timeout_ms: 300
responses:
  - name: gateway_hello
    id: 0x700 + NODE_ID
    direction: ch32_to_esp32
    payload:
      - device_type:uint8:n/a:gateway identity
      - node_id:uint8:n/a:1
      - fw_version:uint8:n/a:bridge firmware version
      - capability_flags:uint8:bitmask:i2c bridge capability
  - name: i2c_transaction_result
    id: bridge-defined response frame
    direction: ch32_to_esp32
    payload:
      - op:uint8:operation:probe|write|read
      - result:uint8:boolean_or_error:0 means failure, non-zero means success
      - node_id:uint8:n/a:1
      - addr7:uint8:i2c_address:0x29
      - len:uint8:bytes:0-4
      - data:uint8[]:bytes:read data when op=read
failure_policy: retry
safe_defaults:
  - Wait for CH32 HELLO before claiming the bridge is online.
  - Treat missing I2C ACK as ADDR_NOT_FOUND.
  - Treat model ID read failure separately from model ID mismatch.
  - Treat out-of-range distance as a valid sensor response, not as module damage.
  - Never output a valid distance unless the read result is OK.
unsupported:
  - ESP32-WROOM direct I2C access to VL53L0X in this capability.
  - CH32-side VL53L0X initialization logic for this protocol contract.
  - OLED font rendering, MPU6050 parsing, motor control, or UI composition.
serial_log:
  - gateway wait result=OK
  - gateway wait result=TIMEOUT
  - tof init result=OK
  - tof init result=ADDR_NOT_FOUND
  - tof init result=MODEL_ID_READ_FAIL
  - tof init result=MODEL_ID_MISMATCH
  - tof init result=CONFIG_FAIL
  - tof distance=<mm> mm raw=<raw> result=OK
  - tof distance out_of_range raw=<raw> result=OUT_OF_RANGE
  - tof read result=<reason>, reinitializing
## END_MOCE_CH32_PROTOCOL_CONTRACT

## Notes For Designer

This protocol exists so Designer knows that the VL53L0X logic belongs on ESP32-WROOM, while CH32 remains a bridge. If a future CH32 firmware exposes a fully generic I2C transaction protocol with larger chunking, update this protocol file first, then update the component implementation.
