# ESP32-WROOM demo

This demo integrates three CH32G6U6 CAN gateway nodes:

- MPU6050 node: reads MPU6050 over I2C and sends samples to ESP32 over CAN.
- OLED node: receives OLED text commands from ESP32 over CAN and drives the 0.96 inch OLED over I2C.
- Motor node: receives motor duty commands from ESP32 over CAN and drives two motors.

CAN protocol used by this demo:

- MPU6050 accel: `0x350`
- MPU6050 gyro: `0x351`
- MPU6050 temperature: `0x352`
- OLED clear: `0x410`
- OLED text: `0x411`
- Motor duty: `0x420`
- Gateway ACK: `0x500`

Motor duty policy:

- Tilt `< 20 deg`: 100%
- Tilt `20..29 deg`: 80%
- Tilt `30..39 deg`: 60%
- Tilt `40..49 deg`: 40%
- Tilt `50..59 deg`: 20%
- Tilt `>= 60 deg`: 0%, motors stop

OLED alternates every few seconds between MPU6050 data and motor duty/tilt status.
