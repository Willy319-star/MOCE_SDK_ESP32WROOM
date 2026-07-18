# servo_test0

ESP32-WROOM controls `D:\MOCE_SDK_CH32\examples_final\CH32_servo_gateway`
over CAN.

Protocol:

- CH32 HELLO/heartbeat: `0x700 + NODE_ID`
- Servo command: `0x400 + NODE_ID`
- ACK: `0x500 + NODE_ID`
- Default CH32 servo `NODE_ID = 3`

Servo command payload:

- `data[0]`: channel, `0..3`
- `data[1..2]`: angle, little-endian uint16, `0..180`

Build:

```powershell
.\tools\build.ps1 example_final/servo_test0 esp32 my_board_esp32wroom
```
