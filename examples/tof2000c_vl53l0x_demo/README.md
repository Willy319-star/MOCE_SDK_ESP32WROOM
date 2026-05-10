# TOF2000C-VL53L0X Demo

This example tests the `driver_tof2000c_vl53l0x` component with a TOF2000C / VL53L0X I2C distance sensor.

Default I2C address: `0x29`.

Default ESP32-S3 wiring uses the board I2C bus:

| ESP32-S3 | TOF2000C-VL53L0X |
| --- | --- |
| GPIO47 SDA | SDA |
| GPIO21 SCL | SCL |
| GND | GND |
| 3V3/VCC | VIN/VCC, follow the module label |

Build for ESP32-S3:

```sh
./tools/build.sh examples/tof2000c_vl53l0x_demo esp32s3 my_board_esp32s3
```

Flash:

```sh
./tools/flash.sh examples/tof2000c_vl53l0x_demo /dev/ttyACM0
```

Monitor:

```sh
./tools/monitor.sh examples/tof2000c_vl53l0x_demo /dev/ttyACM0
```

Expected log:

```text
model id = 0xee
distance=123 mm range_status=0x00
```

If probe fails, check wiring, power, and whether the module needs external pullups on SDA/SCL.
