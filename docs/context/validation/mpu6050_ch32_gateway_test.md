# MPU6050 CH32 Gateway Test Validation

- Example path: `example_final/mpu6050_ch32_gateway_test`
- Helper path: `components_esp32wroom/ch32_mpu6050_gateway`
- Protocol: `docs/context/protocols/ch32_mpu6050_gateway.md`
- Compile status: `compile_passed`
- Hardware test status: `integrated_passed`
- ESP-IDF environment: `D:\Espressif\v6.0.1\esp-idf\esp-idf`
- ESP target and board: `esp32`, `my_board_esp32wroom`
- Generated application: `mpu6050_ch32_gateway_test.bin`, size `0x298D0`
- CH32 test firmware: `D:\Moce.ai\MOCE_SDK_CH32\examples_final\CH32_I2C_gateway_vl`
- CH32 firmware status: `test_firmware`

## Verified Hardware Path

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32 node 1
              -- I2C1 PB6/PB7 --> MPU6050 AD0 low at 0x68
```

The ESP32 and CH32 images were flashed to hardware. The gateway and sensor
probe succeeded, `WHO_AM_I` returned `0x68`, and the ESP32 continuously
reported changing acceleration, angular-velocity, and temperature samples at
approximately 500 ms intervals.

## Build Command

Run from `example_final/mpu6050_ch32_gateway_test`:

```bat
call D:\Espressif\v6.0.1\idf-env.bat
idf.py -DMOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -DMOCE_BOARD=my_board_esp32wroom build
```

## Expected ESP32 Serial Logs

- `ESP32 -> CAN -> CH32 node 1 -> MPU6050`
- `CAN 50 kbit/s TX=GPIO5 RX=GPIO4, MPU6050 address=0x68`
- `CH32 node 1 online, firmware=2 capabilities=0x7F`
- `MPU6050 initialized, WHO_AM_I=0x68`
- `mpu6050 #<sample> ax=<g> ay=<g> az=<g> gx=<dps> gy=<dps> gz=<dps> temp=<C>`

## Known Limits

- The ESP32 never initializes or reads a local MPU6050 I2C controller.
- One gateway register-read request is limited to 32 bytes and each CAN status
  frame carries at most four returned register bytes.
- The example uses the plus or minus 2 g and plus or minus 250 degree-per-second
  ranges without user calibration.
- Values are raw scale conversions, not fused attitude, quaternion, or
  calibrated pose estimates.
- A failed sample is logged and skipped; the loop continues at 500 ms.
- The standalone test uses CH32 node ID 1. Multiple CH32 boards on one CAN bus
  require distinct node IDs.
- `MOCE_RECIPE_CONTRACT` is outside the current nine-document scope.
