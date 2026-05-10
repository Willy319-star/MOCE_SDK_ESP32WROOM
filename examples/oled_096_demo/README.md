# 0.96 Inch OLED Demo

This example tests a 0.96 inch 128x64 SSD1306 OLED on the board I2C bus.

Default board wiring:

- ESP32-S3 board: SDA GPIO47, SCL GPIO21
- OLED I2C address: 0x3c
- OLED profile: `DRIVER_OLED_PROFILE_096_SSD1306`

Build:

```sh
./tools/build.sh examples/oled_096_demo esp32s3 my_board_esp32s3
```

Flash after build from the example directory, or use the flash command printed by ESP-IDF.
