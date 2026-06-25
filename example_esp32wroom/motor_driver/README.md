# ESP32-WROOM DRV8833 Motor Driver Example

This example drives a DRV8833 dual H-bridge module with four PWM inputs.

## Wiring

Use the ESP32-WROOM board `PWM_B` header because `PWM_A1..PWM_A4` map to ESP32 input-only pins.

| ESP32-WROOM signal | ESP32 GPIO | DRV8833 signal |
| --- | ---: | --- |
| PWM_B1 | GPIO32 | AIN1 |
| PWM_B2 | GPIO33 | AIN2 |
| PWM_B3 | GPIO25 | BIN1 |
| PWM_B4 | GPIO26 | BIN2 |
| +5V | - | +5V / VM |
| GND / AGND | - | GND |

This wiring matches the first working rotating demo.

## Behavior

The program prints status at 115200 baud and keeps both motors running forward
together at 100% duty.

PWM frequency is 20 kHz with 10-bit duty resolution.
