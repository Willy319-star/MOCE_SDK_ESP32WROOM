# Servo Test0 Final Validation

- Example path: `example_final/servo_test0_final`
- Helper path: `components_esp32wroom/ch32_servo_gateway_final`
- Protocol: `docs/context/protocols/ch32_servo_gateway_final.md`
- Compile status: compile_passed
- Hardware test status: untested
- Expected CH32 firmware: `D:\MOCE_SDK_CH32\examples_final\CH32_servo_gateway`
- Expected ESP32 serial logs:
  - `hello servo node=3 fw=1 cap=0x0F`
  - `cmd servo node=3 angle=90 ch0=OK ch1=OK ch2=OK ch3=OK`
- Known limits:
  - ESP32 does not directly generate servo PWM.
  - No servo position feedback is available.
  - Missing ACK only logs failure; it does not reset the CAN node in this minimal example.
  - Full parallel build hit a local Windows compiler out-of-memory error; single-thread CMake/Ninja build completed and generated `servo_test0_final.bin`.
