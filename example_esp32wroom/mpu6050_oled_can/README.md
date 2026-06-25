# ESP32-WROOM MPU6050 OLED CAN Demo

This demo combines `mpu6050_can` and `oled_can`.

Data path:

- MPU6050 connects to CH32 over I2C
- OLED connects to CH32 over I2C
- CH32 runs `D:\MOCE_SDK_CH32\examples\ch32_gateway`
- ESP32-WROOM talks to CH32 over CAN

The ESP32 receives MPU6050 CAN frames from CH32, prints the decoded values on
serial, then sends OLED CAN commands back to CH32 to display the latest sample.

OLED text uses the CH32 gateway's tiny 5x7 font, which currently supports
letters and digits. Signs are shown as `P` or `N`.

Build:

```powershell
.\tools\build.ps1 example_esp32wroom/mpu6050_oled_can esp32 my_board_esp32wroom
```
