# ch32_vl53l0x_gateway

ESP32-WROOM side helper component for a VL53L0X laser distance module connected through a CH32 generic I2C bridge.

The ESP32-WROOM does not directly drive the VL53L0X I2C pins. It sends CAN commands to the CH32 bridge, the CH32 converts those commands into I2C transactions, and the result is returned to ESP32-WROOM over CAN.

## What This Component Does

- Initializes ESP32 TWAI/CAN with the board defaults: TX=GPIO5, RX=GPIO4, 50 kbit/s.
- Waits for the CH32 I2C bridge HELLO frame.
- Probes VL53L0X address `0x29` through the CH32 bridge.
- Reads the VL53L0X model ID and expects `0xEE`.
- Sends the minimal VL53L0X register sequence through the bridge.
- Reads distance periodically and separates communication failure from out-of-range measurement.
- Exposes a status structure so a complete project can build a device table later.

## Public API

- `ch32_vl53l0x_default_config()` fills default node, address, timeout, and CAN GPIO values.
- `ch32_vl53l0x_init()` starts the ESP32 TWAI/CAN node.
- `ch32_vl53l0x_wait_gateway()` waits for CH32 bridge HELLO.
- `ch32_vl53l0x_probe()` checks whether the VL53L0X address responds.
- `ch32_vl53l0x_begin()` probes, checks model ID, and configures the sensor.
- `ch32_vl53l0x_read_distance()` returns distance in millimeters or a typed failure result.
- `ch32_vl53l0x_get_status()` returns current node, device, model, CAN counters, error count, and last result.
- `ch32_vl53l0x_state_text()` and `ch32_vl53l0x_result_text()` format status for serial logs.

## What This Component Does Not Do

- It does not initialize ESP32 I2C.
- It does not directly access the physical VL53L0X I2C bus.
- It does not implement OLED display, MPU6050, motor, or other device behavior.
- It does not define the complete multi-CH32 device discovery table. It only exposes enough status fields for the final project to build that table.

## Integration Notes

Use this component when the hardware topology is:

`ESP32-WROOM -> CAN -> CH32 generic I2C bridge -> I2C -> VL53L0X`

For the final multi-module project, keep the common CAN ownership in the project-level scheduler if one exists. This component currently owns TWAI for the minimal example because the repository does not yet provide a shared CAN manager for ESP32-WROOM gateway modules.

Related context files:

- `docs/context/protocols/ch32_i2c_bridge_vl53l0x.md`
- `docs/context/modules/vl53l0x_tof_ch32_gateway.md`
- `docs/context/examples/tof_vl53l0x_gateway_test.md`
- `docs/context/validation/tof_vl53l0x_gateway_test.md`
