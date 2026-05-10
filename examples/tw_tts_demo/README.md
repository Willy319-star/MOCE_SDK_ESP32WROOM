# TW-TTS Demo

This example tests the `driver_tw_tts` component with a UART TW-TTS voice module.

Default ESP32-S3 wiring:

| ESP32-S3 | TW-TTS module |
| --- | --- |
| GPIO38 UART1 TX | RX |
| GPIO39 UART1 RX | TX, optional |
| GND | GND |
| 5V/3V3 | VCC, follow the module label |

The default baud rate is `9600`.

Build for ESP32-S3:

```sh
./tools/build.sh examples/tw_tts_demo esp32s3 my_board_esp32s3
```

Flash:

```sh
./tools/flash.sh examples/tw_tts_demo /dev/ttyACM0
```

Notes:

- `driver_tw_tts_speak()` sends the documented `0xFD + length + command + encoding + text` frame.
- The default synthesis encoding is UTF-8. If your TW-TTS board requires GBK/GB2312 for Chinese, set `config.encoding` and pass text bytes in that encoding.
- `driver_tw_tts_write()` is kept as a raw UART write helper for low-level debugging.
- If your module uses a different baud rate or pins, update `BOARD_UART_*` in `boards/<board>/board.h` or override `driver_tw_tts_config_t` in `main.c`.
