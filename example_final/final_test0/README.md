# ESP32-WROOM Final Test 0

This example matches the CH32 `examples_final` CAN node protocol.

- I2C node default: `NODE_ID=1`, `DEVICE_TYPE=0x01`
- Motor node default: `NODE_ID=2`, `DEVICE_TYPE=0x02`
- Servo node default: `NODE_ID=3`, `DEVICE_TYPE=0x03`

The ESP32 listens for `0x700 + NODE_ID` HELLO frames, prints discovered nodes,
then periodically:

- sends I2C scan to `0x200 + i2c_node`
- sends motor duty commands to `0x300 + motor_node`
- sends servo angle commands to `0x400 + servo_node`

ACK frames are expected at `0x500 + NODE_ID`.
