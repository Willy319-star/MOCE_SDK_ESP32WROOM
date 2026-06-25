# esp32_rx_can

ESP32-WROOM CAN receive example for `boards/my_board_esp32wroom`.

## Hardware

- ESP32 CAN_TX: GPIO5, schematic net `CAN_TX`
- ESP32 CAN_RX: GPIO4, schematic net `CAN_RX`
- Bitrate: 50 kbit/s

Connect the two CAN boards like this:

- CANH to CANH
- CANL to CANL
- GND to GND

Both boards in the provided schematics include a 120 ohm CAN termination resistor.
For a two-node bench test this is normally correct.

## Build

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
Remove-Item Env:IDF_TARGET -ErrorAction SilentlyContinue
.\tools\build.ps1 example_esp32wroom/esp32_rx_can esp32 my_board_esp32wroom
```

## Serial output

When frames arrive, the serial monitor prints lines like:

```text
can_rx #1 std data id=0x00000123 dlc=8 data=[01 02 03 04 05 06 07 08]
```
