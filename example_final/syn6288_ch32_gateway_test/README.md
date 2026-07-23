# syn6288_ch32_gateway_test

Independent ESP32-WROOM test for sending one preassembled SYN6288E UART
frame through a CH32 CAN gateway.

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32 -- PA9 UART 9600 8N1 --> SYN6288E
```

The ESP32 does not configure or write a downstream UART. It sends the complete
opaque SYN6288E frame over CAN, checks the CH32 START and completion ACKs, and
does not replay automatically after a final ACK timeout.

- ESP32 CAN pins come from `boards/my_board_esp32wroom/board.h`.
- CAN IDs: START `0x430`, DATA `0x431`, ACK `0x500`.
- Test frame: `FD 00 0B 01 01 D3 EE D2 F4 CC EC CF C2 C0`.
- Expected speech: `宇音天下`.

Build from the repository root:

```powershell
.\tools\build.ps1 example_final/syn6288_ch32_gateway_test esp32 my_board_esp32wroom
```

