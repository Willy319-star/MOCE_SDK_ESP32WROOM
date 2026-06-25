# ESP32-WROOM MPU6050 Demo

This example reads MPU6050 data from the CH32 CAN gateway.

Data path:

- MPU6050 connects to CH32 over I2C
- CH32 runs `D:\MOCE_SDK_CH32\examples\ch32_gateway`
- CH32 sends MPU6050 samples to ESP32-WROOM over CAN

ESP32-WROOM CAN wiring from `boards/my_board_esp32wroom/board.h`:

- CAN TX: GPIO5
- CAN RX: GPIO4
- Bitrate: 50 kbit/s
- Serial monitor: 115200 baud

CH32 gateway frame format:

- `0x350`: sample count + raw acceleration X/Y/Z
- `0x351`: sample count + raw angular velocity X/Y/Z
- `0x352`: sample count + raw temperature

The ESP32 serial output prints acceleration in `g` and `m/s^2`, angular
velocity in `dps`, and temperature in `C`.

Build:

```powershell
.\tools\build.ps1 example_esp32wroom/mpu6050_demo esp32 my_board_esp32wroom
```
