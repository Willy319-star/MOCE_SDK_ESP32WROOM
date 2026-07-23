# oled_ch32_gateway_test

Independent ESP32-WROOM test for an SSD1315 128x64 OLED connected to the
CH32 generic I2C gateway.

```text
ESP32-WROOM -- CAN 50 kbit/s --> CH32 node 1 -- I2C --> SSD1315 OLED
```

- ESP32 CAN pins come from `boards/my_board_esp32wroom/board.h`.
- Default CH32 node ID: `1`.
- CAN IDs: status `0x101`, I2C command `0x201`, ACK `0x501`, HELLO `0x701`.
- OLED 7-bit I2C address: `0x3C`.
- Expected display: `Hello World`.

The ESP32 does not initialize a local I2C controller. Every OLED command is
encoded as a documented CAN request for the CH32 gateway, and every request
checks the CH32 ACK.

Build from the repository root:

```powershell
.\tools\build.ps1 example_final/oled_ch32_gateway_test esp32 my_board_esp32wroom
```

