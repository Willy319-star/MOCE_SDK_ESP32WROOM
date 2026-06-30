# ESP32-WROOM OLED Example

This example controls the 0.96 inch SSD1306 OLED through the CH32 CAN
gateway example at `D:\MOCE_SDK_CH32\examples\ch32_gateway`.

CAN wiring:

- ESP32 CAN TX: `BOARD_CAN_TX_GPIO` / GPIO5
- ESP32 CAN RX: `BOARD_CAN_RX_GPIO` / GPIO4
- Bitrate: 50 kbit/s
- CH32 OLED clear command: standard CAN ID `0x410`
- CH32 OLED text command: standard CAN ID `0x411`

On boot, ESP32 sends the OLED gateway commands to display:

```text
moceai666
```

Build:

```powershell
.\tools\build.ps1 example_esp32wroom/oled esp32 my_board_esp32wroom
```
