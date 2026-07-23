# mpu6050_ch32_gateway_test

Independent ESP32-WROOM test for an MPU6050 connected to the CH32 generic
I2C gateway.

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32 node 1 -- I2C --> MPU6050
```

- ESP32 CAN pins come from `boards/my_board_esp32wroom/board.h`.
- Default CH32 node ID: `1`.
- CAN IDs: status `0x101`, I2C command `0x201`, ACK `0x501`, HELLO `0x701`.
- MPU6050 7-bit I2C address: `0x68`.
- The example checks `WHO_AM_I`, then prints acceleration, gyroscope, and
  temperature values every 500 ms.

The ESP32 does not initialize a local I2C controller. Register operations are
encoded as documented CAN requests for the CH32 gateway and checked using
CH32 status and ACK frames.

Build from the repository root:

```powershell
.\tools\build.ps1 example_final/mpu6050_ch32_gateway_test esp32 my_board_esp32wroom
```

