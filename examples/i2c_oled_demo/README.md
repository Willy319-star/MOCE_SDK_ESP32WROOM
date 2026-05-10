# I2C OLED Demo

This example tests the SDK OLED component on the board I2C bus.

Default wiring comes from the selected board profile:

- ESP32-S3 default board: SDA GPIO47, SCL GPIO21
- I2C address: 0x3c

Supported profiles:

- 0.96 inch SSD1306, 128x64
- 1.3 inch SH1106, 128x64

Build the default 0.96 inch SSD1306 demo:

```sh
./tools/build.sh examples/i2c_oled_demo esp32s3 my_board_esp32s3
```

To test a 1.3 inch SH1106 module, run `idf.py menuconfig` in this example and select:

```text
I2C OLED Demo -> OLED profile -> 1.3 inch SH1106 128x64
```

Then build and flash again.
