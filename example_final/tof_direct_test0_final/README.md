# tof_direct_test0_final

Minimal ESP32-WROOM example for directly reading a VL53L0X laser distance sensor over I2C.

This is the direct-wiring version. It does not use CH32 and does not replace `example_final/tof_test0_final`, which is the CH32 gateway version.

## Build

```powershell
.\tools\build.ps1 example_final/tof_direct_test0_final esp32 my_board_esp32wroom
```

Or from the example directory after ESP-IDF export:

```powershell
idf.py -D MOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -p COMx -D MOCE_BOARD=my_board_esp32wroom build flash monitor
```

## Wiring

- ESP32 GPIO21 SDA -> VL53L0X SDA
- ESP32 GPIO22 SCL -> VL53L0X SCL
- ESP32 GND -> VL53L0X GND
- Use a valid power supply for the VL53L0X breakout board.

## Runtime Logic

1. Initialize ESP32-WROOM I2C through `vl53l0x_direct_init()`.
2. Probe address `0x29`; if no ACK is received, scan `0x03` through `0x77` and print found addresses.
3. Read model ID register `0xC0`; expected value is `0xEE`.
4. Run a small VL53L0X init sequence.
5. Read distance every 500 ms.
6. Print valid distance, out-of-range, and failures separately.

## Expected Logs

```text
==== ESP32-WROOM tof_direct_test0_final ====
i2c initialized, probing VL53L0X
tof init result=OK
tof distance=<number> mm raw=<number> result=OK
```

Out-of-range is not a module failure:

```text
tof distance out_of_range raw=<number> result=OUT_OF_RANGE
```

## Failure Logs

- `tof init result=ADDR_NOT_FOUND`: no ACK at `0x29`; the example then prints `tof i2c scan found=<n> addr: ...`.
- `tof init result=MODEL_ID_READ_FAIL`: address path exists but register read failed.
- `tof init result=MODEL_ID_MISMATCH`: model ID is not `0xEE`.
- `tof init result=CONFIG_FAIL`: initialization sequence failed.
- `tof read result=COMM_FAIL`: runtime I2C transaction failed.
- `tof read result=MEASURE_TIMEOUT`: ranging result was not ready before timeout.
