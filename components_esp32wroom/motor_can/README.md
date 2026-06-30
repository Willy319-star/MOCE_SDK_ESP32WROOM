# ESP32-WROOM Motor CAN Demo

This example sends motor/PWM commands from ESP32-WROOM to the CH32 gateway over
CAN.

Data path:

- Motor driver board connects to CH32
- CH32 runs a gateway firmware that accepts CAN motor/PWM commands
- ESP32-WROOM sends motor commands over CAN

Protocol used by this demo:

- Standard CAN ID: `0x420`
- `data[0]`: channel, `0` for motor A and `1` for motor B
- `data[1..2]`: duty in permille, little-endian
- `1000` means 100% duty

The current `D:\MOCE_SDK_CH32\examples\ch32_gateway` code accepts this frame
format with the following motor mapping:

- `channel=0`: motor A, PA4 software PWM, PA5 direction low
- `channel=1`: motor B, PA6/TIM3_CH1 hardware PWM, PA7 direction low

This ESP32 demo sends both channels at 100% duty once per second.

Build:

```powershell
.\tools\build.ps1 example_esp32wroom/motor_can esp32 my_board_esp32wroom
```
