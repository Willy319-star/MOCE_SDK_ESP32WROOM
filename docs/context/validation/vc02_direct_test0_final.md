# VC02 Direct UART Test Validation

- Example path: `example_final/vc02_direct_test0_final`
- Helper path: `components_esp32wroom/vc02_direct_uart_final`
- Protocol: `docs/context/protocols/vc02_direct_uart_final.md`
- Module context: `docs/context/modules/vc02_direct_uart_final.md`
- Board: `my_board_esp32wroom`
- Target: `esp32`
- Compile status: compile_passed
- Hardware test status: board_passed

## Hardware Wiring

```text
ESP32 GPIO17 TX -> VC02 RX
ESP32 GPIO16 RX <- VC02 TX
GND common
VC02 powered according to module requirement
```

## Expected ESP32 Serial Logs

Startup:

```text
==== ESP32-WROOM direct UART -> VC02 action mapping demo v2 ====
VC02 UART direct wiring: ESP32 GPIO17 TX -> VC02 RX, ESP32 GPIO16 RX <- VC02 TX
VC02 UART config: UART1 baud=115200 8N1
VC02 UART init OK, waiting for UART bytes
```

Recognized command:

```text
VC02_EVENT name=VL53L0X_SHOW uart="TX SA VL 00" meaning="show VL53L0X distance data" slot=slot_vl53l0x_show raw=[...]
ACTION_SLOT slot_vl53l0x_show: show VL53L0X distance data -> replace this callback with a selected module API raw=[...]
```

No matched command:

```text
VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
```

## Observed Bench Behavior

Direct UART testing has shown that VC02 can send command text such as:

- `TX WK 00`
- `TX RF OL ED`
- `TX SA MO TO`
- `TX SO MO TO`
- `TX SA MP U0`
- `TX SA VL 00`

The direct driver recognizes these byte sequences and dispatches callbacks. Idle status printing is suppressed unless new UART data changes parser state.

## Known Limits

- This direct version proves VC02 UART output and ESP32 parsing only.
- It does not validate CH32 CAN/UART bridge behavior.
- It does not execute real OLED, motor, servo, MPU6050, or VL53L0X actions.
