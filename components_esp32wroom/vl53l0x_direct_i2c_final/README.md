# vl53l0x_direct_i2c_final

ESP32-WROOM direct I2C helper for a VL53L0X laser distance sensor.

This is the direct-connection variant. It is intentionally separate from `components_esp32wroom/ch32_vl53l0x_gateway_final`, which is the normal CH32 gateway version.

## Hardware

- ESP32-WROOM I2C SDA: `BOARD_I2C_SDA_GPIO` (`GPIO21` on `my_board_esp32wroom`)
- ESP32-WROOM I2C SCL: `BOARD_I2C_SCL_GPIO` (`GPIO22` on `my_board_esp32wroom`)
- VL53L0X default I2C address: `0x29`
- Common GND and valid module power are required.

## API

- `vl53l0x_direct_default_config()` fills address, speed, and timeout defaults.
- `vl53l0x_direct_init()` initializes the board I2C bus and adds the VL53L0X device.
- `vl53l0x_direct_probe()` checks address ACK.
- `vl53l0x_direct_begin()` probes, reads model ID `0xEE`, and writes the init table.
- `vl53l0x_direct_read_distance()` starts one ranging operation and returns millimeters.
- `vl53l0x_direct_get_status()` exposes state, model, counters, last result, and last distance.
- `vl53l0x_direct_state_text()` and `vl53l0x_direct_result_text()` format logs.

## Failure Behavior

- `ADDR_NOT_FOUND`: no ACK at `0x29`; check SDA/SCL, power, GND, and address.
- `MODEL_ID_READ_FAIL`: address path responded, but register `0xC0` could not be read.
- `MODEL_ID_MISMATCH`: model register was read but was not `0xEE`.
- `CONFIG_FAIL`: model ID was correct but one init write failed.
- `MEASURE_TIMEOUT`: measurement did not become ready in time.
- `OUT_OF_RANGE`: valid sensor path, but measured value is beyond the configured usable range; this is not treated as a broken module.

## Related Context

- `docs/context/modules/vl53l0x_direct_i2c_final.md`
- `docs/context/protocols/vl53l0x_direct_i2c_final.md`
- `docs/context/validation/tof_direct_test0_final.md`
