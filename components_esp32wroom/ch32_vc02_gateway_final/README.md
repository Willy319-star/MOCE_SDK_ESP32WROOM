# ch32_vc02_gateway_final

ESP32-WROOM side helper component for a VC02 voice recognition module connected through a CH32 UART bridge.

Hardware path:

`ESP32-WROOM -> CAN -> CH32 UART bridge -> UART -> VC02`

The component is now a normal `.h + .c` driver:

- `include/ch32_vc02_gateway_final.h`: public types and APIs.
- `ch32_vc02_gateway_final.c`: command table, byte parser, callback dispatch, and ping payload builder.

## Current CH32 Dependency

The CH32 VC02 bridge firmware is not present in this repository yet. The current ESP32 driver and example therefore use an expected bridge contract:

- CAN bitrate: 50 kbit/s.
- VC02 UART baud on CH32 side: 115200.
- ESP32 sends ping on CAN ID `0x730`.
- CH32 sends status on CAN ID `0x732`.
- CH32 forwards VC02 UART RX bytes on CAN ID `0x733`.
- CH32 sends ACK on CAN ID `0x734`.
- CH32 must preserve raw VC02 bytes and chunk order.

Before treating this as a fixed production protocol, the CH32 bridge code must confirm the CAN IDs, payload layout, node-id behavior, ACK fields, status fields, UART baud, and maximum forwarded UART chunk length.

## What This Component Does

- Defines the VC02 command table used by the current firmware image.
- Converts VC02 UART byte streams forwarded over CAN into semantic commands.
- Accepts the observed wakeup form `TX WK 00` and remains compatible with `00 TX WK 00`.
- Maintains a rolling receive window so command bytes split across CAN frames can still be matched.
- Provides callback registration so final applications can bind voice commands to other module drivers without changing the parser.
- Exposes counters for received chunks, bytes, matched events, dispatched events, and unmatched chunks.

## Public API

- `ch32_vc02_init()` initializes parser state and callback table.
- `ch32_vc02_register_action()` binds a semantic VC02 command to an application callback.
- `ch32_vc02_feed_bytes()` feeds UART bytes forwarded by CH32 and returns a recognized event when a known command appears.
- `ch32_vc02_dispatch()` invokes the registered callback for a recognized event.
- `ch32_vc02_build_ping()` builds the current expected ping payload for the CH32 bridge.
- `ch32_vc02_cmd_name()` and `ch32_vc02_action_slot()` format recognized commands for logs.

## AI Agent Integration Notes

VC02 should be treated as a command source:

```text
VC02 firmware -> CH32 UART bridge -> ESP32 parser -> semantic command -> application callback -> selected module driver
```

Do not place OLED, motor, servo, MPU6050, or VL53L0X control code inside the VC02 parser. If the user says "clear screen", the VC02 parser should only produce `CH32_VC02_CMD_OLED_CLEAR`; the final application decides whether an OLED module is selected and what function should run.

Related context files:

- `docs/context/protocols/ch32_vc02_gateway_final.md`
- `docs/context/modules/ch32_vc02_gateway_final.md`
- `docs/context/validation/vc02_test0_final.md`
