# tof_test0_final

Minimal ESP32-WROOM example for reading a VL53L0X laser distance module through a CH32 generic I2C bridge.

## Burn Targets

- ESP32-WROOM: burn this example, `example_final/tof_test0_final`.
- CH32: burn `MOCE_SDK_CH32/examples_final/CH32_I2C_bridge_generic`.

## Hardware Topology

`ESP32-WROOM -> CAN -> CH32 -> I2C -> VL53L0X`

Default ESP32 CAN pins:

- CAN TX: GPIO5
- CAN RX: GPIO4
- CAN bitrate: 50 kbit/s

Default VL53L0X identity:

- I2C 7-bit address: `0x29`
- I2C write address byte: `0x52`
- Expected model ID: `0xEE`

## Power-On Logic

1. ESP32 initializes TWAI/CAN through `ch32_vl53l0x_init()`.
2. ESP32 waits for CH32 bridge HELLO through `ch32_vl53l0x_wait_gateway()`.
3. ESP32 asks CH32 to probe I2C address `0x29`.
4. ESP32 asks CH32 to read the VL53L0X model ID register.
5. ESP32 sends the minimal VL53L0X configuration sequence through CH32.
6. After initialization succeeds, ESP32 reads distance every 500 ms.

The example intentionally keeps `main.c` thin. Sensor state and CAN bridge operations are kept in `components_esp32wroom/ch32_vl53l0x_gateway_final`.

## Expected Serial Logs

Normal startup:

```text
==== ESP32-WROOM VL53L0X over CH32 generic I2C bridge ====
twai started, waiting for CH32 I2C bridge HELLO
gateway wait result=OK
tof init result=OK
tof distance=<number> mm raw=<number> result=OK
```

Out of range:

```text
tof distance out_of_range raw=<number> result=OUT_OF_RANGE
```

This means the sensor answered, but the target is outside its valid range. It is not the same as a broken module.

Common failure logs:

- `gateway wait result=TIMEOUT`: ESP32 did not see CH32 HELLO. Check CAN wiring, CH32 firmware, power, node ID, and bitrate.
- `tof init result=ADDR_NOT_FOUND`: CH32 did not receive ACK from I2C address `0x29`.
- `tof init result=MODEL_ID_READ_FAIL`: address may respond, but model ID read failed.
- `tof init result=MODEL_ID_MISMATCH`: model ID was read but was not `0xEE`.
- `tof init result=CONFIG_FAIL`: model ID was correct, but the configuration sequence failed.
- `tof read result=COMM_FAIL`: runtime CAN or I2C transaction failed.

## Build Command

From this example directory:

```powershell
idf.py -p COM4 -D MOCE_BOARD=my_board_esp32wroom set-target esp32
idf.py -p COM4 -D MOCE_BOARD=my_board_esp32wroom build flash monitor
```

Change `COM4` to the actual ESP32 serial port.

## Final Project Integration

The final project should reuse the component API instead of copying this example. A project-level device discovery table can use:

- `ch32_node_id`
- `tof_addr7`
- `model_id`
- `gateway_online`
- `device_found`
- `initialized`
- `error_count`
- `last_result`

Those fields let the final project know which CH32 node is connected to the VL53L0X, whether the sensor is alive, and whether the latest failure is a communication failure or only an out-of-range measurement.

