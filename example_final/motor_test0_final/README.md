# motor_test0_final

Minimal ESP32-WROOM example for the fixed CH32 motor gateway.

The ESP32-WROOM does not generate motor PWM. It only:

- waits for a CH32 motor gateway HELLO frame
- sends documented CAN motor duty commands
- checks `0x500 + NODE_ID` ACK frames
- prints serial status

Build:

```powershell
.\tools\build.ps1 example_final/motor_test0_final esp32 my_board_esp32wroom
```
