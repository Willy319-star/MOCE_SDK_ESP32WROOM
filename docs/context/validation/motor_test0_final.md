# Motor Test0 Final Validation

- Example path: `example_final/motor_test0_final`
- Helper path: `components_esp32wroom/ch32_motor_gateway_final`
- Protocol: `docs/context/protocols/ch32_motor_gateway_final.md`
- Compile status: compile_passed
- Hardware test status: untested
- Expected CH32 firmware: `D:\MOCE_SDK_CH32\examples_final\CH32_motor_gateway`
- Expected ESP32 serial logs:
  - `hello motor node=2 fw=1 cap=0x03`
  - `cmd motor node=2 duty=600 dir=0 ch0=OK ch1=OK`
- Known limits:
  - ESP32 does not directly generate motor PWM.
  - This is open-loop duty control only.
  - No encoder, speed, current, distance, or position feedback is available.
  - Missing ACK logs failure and does not assume motor state changed.
  - Full build exceeded the tool wait window; continuing the existing build with single-thread CMake/Ninja completed and generated `motor_test0_final.bin`.
