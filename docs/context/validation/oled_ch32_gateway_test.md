# OLED CH32 Gateway Test Validation

- Example path: `example_final/oled_ch32_gateway_test`
- Helper path: `components_esp32wroom/ch32_oled_gateway`
- Protocol: `docs/context/protocols/ch32_oled_gateway.md`
- Compile status: `compile_passed`
- Hardware test status: `integrated_passed`
- ESP-IDF environment: `D:\Espressif\v6.0.1\esp-idf\esp-idf`
- ESP target and board: `esp32`, `my_board_esp32wroom`
- Generated application: `oled_ch32_gateway_test.bin`, size `0x29280`
- CH32 test firmware: `D:\Moce.ai\MOCE_SDK_CH32\examples_final\CH32_I2C_gateway_vl`
- CH32 firmware status: `test_firmware`

## Verified Hardware Path

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32 node 1
              -- I2C1 PB6/PB7 --> SSD1315 128x64 OLED at 0x3C
```

The ESP32 and CH32 images were flashed to hardware. The CH32 gateway was
detected, the OLED probe succeeded, and the panel displayed `Hello World`.

## Build Command

Run from `example_final/oled_ch32_gateway_test`:

```bat
call D:\Espressif\v6.0.1\idf-env.bat
idf.py -DMOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -DMOCE_BOARD=my_board_esp32wroom build
```

## Expected ESP32 Serial Logs

- `ESP32 -> CAN -> CH32 node 1 -> SSD1315 OLED`
- `CAN 50 kbit/s TX=GPIO5 RX=GPIO4, OLED address=0x3C`
- `CH32 node 1 online, firmware=2 capabilities=0x7F`
- `SSD1315 detected`
- `OLED now displays: Hello World`

## Known Limits

- The ESP32 never initializes or writes a local OLED I2C controller.
- Every OLED operation crosses CAN and checks a CH32 ACK, so full-screen
  updates are slower than a direct local I2C framebuffer transfer.
- One raw-write command carries at most five I2C bytes; the example uses one
  control byte plus at most four OLED bytes.
- The example font contains only the glyphs needed for `Hello World`.
- The standalone test uses CH32 node ID 1. Multiple CH32 boards on one CAN bus
  require distinct node IDs.
- `MOCE_RECIPE_CONTRACT` is outside the current nine-document scope.
