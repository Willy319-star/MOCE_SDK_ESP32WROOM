# TOF Direct I2C Test Validation

- Example path: `example_final/tof_direct_test0_final`
- Helper path: `components_esp32wroom/vl53l0x_direct_i2c_final`
- Protocol: `docs/context/protocols/vl53l0x_direct_i2c_final.md`
- Board: `my_board_esp32wroom`
- Target: `esp32`
- CH32 firmware: not used
- Compile status: compile_passed
- Hardware test status: untested

## Expected ESP32 Serial Logs

```text
==== ESP32-WROOM tof_direct_test0_final ====
i2c initialized, probing VL53L0X
tof init result=OK
tof distance=<number> mm raw=<number> result=OK
```

Out-of-range is a valid sensor response:

```text
tof distance out_of_range raw=<number> result=OUT_OF_RANGE
```

## Failure Logs

- `tof init result=ADDR_NOT_FOUND`: no ACK at address `0x29`; check SDA/SCL, power, GND, address, and pullups.
- `tof init result=MODEL_ID_READ_FAIL`: I2C path exists but model register `0xC0` could not be read.
- `tof init result=MODEL_ID_MISMATCH`: model register returned a value other than `0xEE`.
- `tof init result=CONFIG_FAIL`: model ID was correct but one or more setup writes failed.
- `tof read result=COMM_FAIL`: runtime I2C transaction failed.
- `tof read result=MEASURE_TIMEOUT`: ranging status did not become ready before timeout.
- `tof distance out_of_range`: not a broken module; target is beyond reliable measurement range.

## Validation Checklist

- ESP32 example targets `esp32`, not `esp32s3`.
- Example `main.c` calls the direct component API instead of duplicating VL53L0X register logic.
- Direct I2C is explicit in the module context and does not claim CH32 gateway compatibility.
- Serial logs include startup, init, I2C scan on address failure, measurement, out-of-range, and failure states.

## Known Limits

- This direct variant is for hardware bring-up or direct-I2C kits. It does not replace the CH32 gateway capability.
- The helper assumes one VL53L0X at address `0x29`.
- The init sequence is intentionally small and based on the already verified minimal project behavior, not the full vendor API.

