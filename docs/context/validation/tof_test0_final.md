# TOF VL53L0X Gateway Test Validation

- Example path: `example_final/tof_test0_final`
- Helper path: `components_esp32wroom/ch32_vl53l0x_gateway_final`
- Protocol: `docs/context/protocols/ch32_vl53l0x_gateway_final.md`
- Board: `my_board_esp32wroom`
- Target: `esp32`
- Required CH32 firmware: `MOCE_SDK_CH32/examples_final/CH32_I2C_bridge_generic`
- Compile status: compile_not_rerun_in_this_format_pass
- Hardware test status: board_passed

## Expected ESP32 Serial Logs

```text
==== ESP32-WROOM VL53L0X over CH32 generic I2C bridge ====
twai started, waiting for CH32 I2C bridge HELLO
gateway wait result=OK
tof init result=OK
tof distance=<number> mm raw=<number> result=OK
```

Out of range is a valid sensor response:

```text
tof distance out_of_range raw=<number> result=OUT_OF_RANGE
```

## Failure Logs

- `gateway wait result=TIMEOUT`: CH32 HELLO was not received. Check CAN wiring, CH32 firmware, node ID, power, and bitrate.
- `tof init result=ADDR_NOT_FOUND`: CH32 did not receive I2C ACK from address `0x29`.
- `tof init result=MODEL_ID_READ_FAIL`: I2C transaction path exists but model ID read failed.
- `tof init result=MODEL_ID_MISMATCH`: model register returned a value other than `0xEE`.
- `tof init result=CONFIG_FAIL`: model ID was correct but one or more setup writes failed.
- `tof read result=COMM_FAIL`: runtime CAN/I2C transaction failed.
- `tof distance out_of_range`: not a broken module; target is beyond reliable measurement range.

## Validation Checklist

- ESP32 example builds for `esp32`, not `esp32s3`.
- Example `main.c` calls the component API instead of duplicating the driver logic.
- ESP32 does not directly initialize I2C for VL53L0X.
- CH32 firmware is treated as a fixed generic I2C bridge.
- Serial logs include clear startup, init, measurement, out-of-range, and failure states.

## Known Limits

- This minimal example owns TWAI/CAN initialization. A complete project with several CH32 nodes should use a shared CAN manager or adapt this component behind the shared manager.
- Current helper assumes one VL53L0X node/address pair by default: node `1`, address `0x29`.
- CH32 bridge response framing must remain compatible with the component implementation.

