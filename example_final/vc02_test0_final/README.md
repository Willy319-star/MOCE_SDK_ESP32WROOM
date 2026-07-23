# vc02_test0_final

Minimal ESP32-WROOM example for receiving VC02 voice-recognition UART bytes through a CH32 UART bridge.

## Burn Targets

- ESP32-WROOM: burn this example, `example_final/vc02_test0_final`.
- CH32: burn the future VC02 UART bridge firmware that forwards VC02 UART RX bytes to ESP32 over CAN. That CH32 firmware is not present in this repository yet.

## Hardware Topology

`ESP32-WROOM -> CAN -> CH32 -> UART -> VC02`

Default ESP32 CAN pins:

- CAN TX: GPIO5
- CAN RX: GPIO4
- CAN bitrate: 50 kbit/s

Expected VC02 UART setting on CH32 side:

- Baud: 115200
- Format: 8N1

## Power-On Logic

1. ESP32 initializes TWAI/CAN.
2. ESP32 initializes the VC02 parser through `ch32_vc02_init()`.
3. ESP32 registers callback slots for each known VC02 command.
4. ESP32 sends a periodic ping to the expected CH32 VC02 bridge.
5. When CH32 forwards VC02 UART bytes on CAN ID `0x733`, ESP32 feeds those bytes into `ch32_vc02_feed_bytes()`.
6. If the driver recognizes a command, ESP32 prints `VC02_EVENT` and dispatches the registered callback.
7. The demo callback prints the action slot instead of controlling real modules.

The example intentionally keeps hardware action logic out of `main.c`. In a final project, replace the demo callbacks with calls to selected module drivers.

## Expected Serial Logs

Startup:

```text
==== ESP32-WROOM -> CH32 UART bridge -> VC02 action mapping demo ====
ESP32 CAN: TX=GPIO5 RX=GPIO4 bitrate=50 kbit/s
VC02 UART is on CH32 side. Expected CH32 UART baud=115200
TWAI started, waiting for VC02 bytes forwarded by CH32
```

Recognized command:

```text
VC02_EVENT name=OLED_CLEAR uart="TX CL OL ED" meaning="clear OLED screen" slot=slot_oled_clear raw=[54 58 20 43 4C 20 4F 4C]
ACTION_SLOT slot_oled_clear: clear OLED screen -> replace this callback with a real module function raw=[54 58 20 43 4C 20 4F 4C]
```

No matched command yet:

```text
VC02_RX no_match chunks=<n> bytes=<n> ascii_window="<recent bytes>"
```

Periodic parser status:

```text
VC02_DRIVER_STATUS chunks=<n> bytes=<n> matched=<n> dispatched=<n> no_match=<n> ascii_window="<recent bytes>"
```

Bridge status or ACK, if CH32 provides those frames:

```text
VC02_BRIDGE_STATUS raw=[...]
VC02_BRIDGE_ACK raw=[...]
```

## Build Command

From this example directory:

```powershell
idf.py -p COM4 -D MOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -p COM4 -D MOCE_BOARD=my_board_esp32wroom build flash monitor
```

Change `COM4` to the actual ESP32 serial port.

## Final Project Integration

The final project should reuse the component API instead of copying parser logic. The intended replacement point is the callback registration table in `register_vc02_actions()`.

Examples:

- `CH32_VC02_CMD_OLED_CLEAR` can call the OLED gateway driver if OLED is selected.
- `CH32_VC02_CMD_MOTOR_START` can call the motor gateway driver if motor is selected.
- `CH32_VC02_CMD_VL53L0X_SHOW` can request or display the latest distance if the VL53L0X module is selected.
- If a module is not selected, keep the callback as a safe no-op with a clear log.

This is a single-module example, not a recipe. It demonstrates command recognition and callback dispatch only.

## Known Limits

- CH32 VC02 bridge protocol is not verified locally because the CH32 source is not available yet.
- This example owns TWAI/CAN initialization. A complete project with several CH32 nodes should use a shared CAN manager or adapt this component behind that manager.
- The demo does not control downstream modules. It only shows where those calls belong.
