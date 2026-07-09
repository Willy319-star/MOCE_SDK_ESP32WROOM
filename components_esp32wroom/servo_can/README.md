# ESP32-WROOM Servo CAN Demo

This demo sends servo angle commands from ESP32-WROOM to a CH32 CAN gateway.

The CH32 servo reference example uses software PWM:

- Period: 20 ms
- 0 degrees: 1000 us pulse
- 90 degrees: 1500 us pulse
- 180 degrees: 2000 us pulse
- Output pins in the reference: PB6, PB7, PA6, PA7

Proposed CAN protocol:

- Standard CAN ID: `0x430`
- `data[0]`: servo channel, `0..3`
- `data[1..2]`: angle, `0..180`, little-endian uint16
- Gateway ACK: standard CAN ID `0x500`
- ACK `data[0]`: `0x30`
- ACK `data[1..2]`: source ID `0x430`, little-endian
- ACK `data[3]`: result, non-zero means success

The CH32 gateway maps channels as `ch0=PA6`, `ch1=PA7`, `ch2=PB6`, `ch3=PB7`.

The demo sweeps all four channels through `0 -> 90 -> 180 -> 90` once per second.
