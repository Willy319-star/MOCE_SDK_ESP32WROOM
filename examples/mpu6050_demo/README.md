# MPU6050 Demo

This example tests the SDK MPU6050 component on the board I2C bus.

Default device settings:

- I2C address: `0x68` when AD0 is low
- Accelerometer range: +/-2 g
- Gyroscope range: +/-250 dps
- DLPF: 44 Hz

ESP32-S3-CAM board wiring:

- SDA: GPIO47
- SCL: GPIO21
- VCC: 3V3
- GND: GND

Build for ESP32-S3:

```sh
./tools/build.sh examples/mpu6050_demo esp32s3 my_board_esp32s3
```

Build for ESP32:

```sh
./tools/build.sh examples/mpu6050_demo esp32 my_board_esp32
```
