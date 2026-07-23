# SYN6288E CH32 Gateway Test Validation

- Example path: `example_final/syn6288_ch32_gateway_test`
- Helper path: `components_esp32wroom/ch32_syn6288_gateway`
- Protocol: `docs/context/protocols/ch32_syn6288_gateway.md`
- Compile status: `compile_passed`
- Hardware test status: `integrated_passed`
- ESP-IDF environment: `D:\Espressif\v6.0.1\esp-idf\esp-idf`
- ESP target and board: `esp32`, `my_board_esp32wroom`
- Generated application: `syn6288_ch32_gateway_test.bin`, size `0x29500`
- CH32 test firmware: `D:\Moce.ai\CH32_test\examples\CH32_UART_gateway`
- CH32 firmware status: `test_firmware`

## Verified Hardware Path

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32
              -- USART1 PA9 9600 8N1 TX --> SYN6288E
```

The ESP32 and CH32 images were flashed to hardware. The START ACK and final
completion ACK succeeded, the CH32 forwarded all 14 bytes, and the SYN6288E
played the expected speech for `宇音天下`.

## Build Command

Run from `example_final/syn6288_ch32_gateway_test`:

```bat
call D:\Espressif\v6.0.1\idf-env.bat
idf.py -DMOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -DMOCE_BOARD=my_board_esp32wroom build
```

## Expected ESP32 Serial Logs

- `ESP32-WROOM -> CAN -> CH32 -> PA9 UART -> SYN6288E`
- `waiting 3000 ms for SYN6288E power-up`
- `transfer=<id> length=14 fragments=3 crc=0x5C99`
- `ACK phase=0x30 source=0x430 result=1 transfer=<id> detail=0 processed=0`
- `ACK phase=0x31 source=0x431 result=1 transfer=<id> detail=0 processed=14`
- `one-shot SYN6288E transfer completed successfully`

## Known Limits

- The example sends the fixed frame
  `FD 00 0B 01 01 D3 EE D2 F4 CC EC CF C2 C0`; `C0` is its SYN6288E XOR
  checksum.
- The ESP32 component transfers opaque bytes and does not build arbitrary
  Chinese text frames or perform UTF-8-to-GBK conversion.
- Only one transfer may be active and the complete frame is limited to 4096
  bytes.
- CH32 uses PA9 TX only. The gateway cannot observe SYN6288E Ready, Busy, or
  playback-complete responses.
- A missing final ACK is not automatically retried because the audio may
  already have played.
- The SYN gateway uses fixed CAN IDs `0x430`, `0x431`, and `0x500`; another
  device using the same IDs would conflict on a shared bus.
- `MOCE_RECIPE_CONTRACT` is outside the current nine-document scope.
