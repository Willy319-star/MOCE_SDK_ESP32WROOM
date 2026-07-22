# VC02 Gateway Test Validation

- Example path: `example_final/vc02_test0_final`
- Helper path: `components_esp32wroom/ch32_vc02_gateway_final`
- Protocol: `docs/context/protocols/ch32_vc02_gateway_final.md`
- Module context: `docs/context/modules/ch32_vc02_gateway_final.md`
- Board: `my_board_esp32wroom`
- Target: `esp32`
- Required CH32 firmware: VC02 UART bridge firmware, not available locally yet
- Compile status: compile_passed
- Hardware test status: untested

## Driver Structure

The CH32 bridge VC02 component is split into:

- `components_esp32wroom/ch32_vc02_gateway_final/include/ch32_vc02_gateway_final.h`
- `components_esp32wroom/ch32_vc02_gateway_final/ch32_vc02_gateway_final.c`

The example should call this component API instead of duplicating VC02 command parsing in `main.c`.

## Expected ESP32 Serial Logs

Startup:

```text
==== ESP32-WROOM -> CH32 UART bridge -> VC02 action mapping demo ====
ESP32 CAN: TX=GPIO5 RX=GPIO4 bitrate=50 kbit/s
VC02 UART is on CH32 side. Expected CH32 UART baud=115200
TWAI started, waiting for VC02 bytes forwarded by CH32
```

Recognized VC02 command:

```text
VC02_EVENT name=<command> uart="<vc02 bytes>" meaning="<meaning>" slot=<action_slot> raw=[...]
ACTION_SLOT <action_slot>: <meaning> -> replace this callback with a real module function raw=[...]
```

Unknown or partial bytes:

```text
VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
```

## Validation Checklist

- ESP32 example builds for `esp32`, not `esp32s3`.
- Example `main.c` calls `ch32_vc02_gateway_final` parser APIs instead of duplicating command parsing.
- ESP32 does not directly initialize UART for VC02 in this gateway example.
- VC02 parser does not directly call OLED, motor, servo, MPU6050, or VL53L0X drivers.
- Command-to-action integration is represented by callback registration.
- CH32 protocol is marked draft until CH32 bridge firmware confirms the contract.

## Known Limits

- Hardware behavior is not validated yet because the CH32 VC02 bridge source and flashed firmware are not available locally.
- This example owns TWAI/CAN initialization. A complete project with several CH32 nodes should use a shared CAN manager or adapt this component behind the shared manager.
- This is a single-module driver/example validation file. No recipe context file is generated for VC02 at this stage.
