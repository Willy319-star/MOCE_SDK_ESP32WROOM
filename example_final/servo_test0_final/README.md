# servo_test0_final

Minimal ESP32-WROOM example for the fixed CH32 servo gateway.

The ESP32-WROOM does not generate servo PWM. It only:

- waits for a CH32 servo gateway HELLO frame
- sends documented CAN servo angle commands
- checks `0x500 + NODE_ID` ACK frames
- prints serial status

Build:

```powershell
.\tools\build.ps1 example_final/servo_test0_final esp32 my_board_esp32wroom
```
