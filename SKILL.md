---
name: skill_ch32
description: Guide Codex to create or review MOCE WROOM firmware assets where ESP32-WROOM only talks to a fixed CH32 gateway board, and the CH32 gateway controls downstream modules such as OLED, TOF, motors, sensors, and future kit boards. Use when adding WROOM module support, CH32 gateway protocol metadata, ESP32-side examples, recipe context, or validation notes for MOCE Designer.
---

# MOCE WROOM CH32 Gateway Firmware Skill

Use this skill when creating or reviewing firmware assets for WROOM-based MOCE kits.

The fixed rule is:

ESP32-WROOM only controls the CH32 gateway board. The CH32 gateway firmware is already written and fixed. The CH32 gateway then controls downstream modules through CAN, I2C, PWM, GPIO, UART, ADC, or other local interfaces.

For MOCE Designer today, generate and maintain only the ESP32-WROOM side firmware and the machine-readable contracts that describe what the CH32 gateway can do.

## One: Architecture Boundary

Never generate ESP32-WROOM code that directly drives downstream modules behind the CH32 gateway.

Forbidden in ESP32-WROOM application code unless a separate verified board contract explicitly says otherwise:

- Direct OLED I2C initialization.
- Direct TOF I2C initialization.
- Direct motor PWM control.
- Direct sensor GPIO/ADC/PWM control for gateway-side modules.
- Direct reuse of ESP32-S3 driver examples just because the feature name matches.

Required WROOM architecture:

```text
User requirement
-> ESP32-WROOM application logic
-> CH32 gateway protocol command
-> CH32 fixed firmware
-> downstream module behavior
-> CH32 status/ACK frame
-> ESP32-WROOM state update/log/display decision
```

If a user says "ESP controls OLED", interpret it as "ESP32-WROOM sends an OLED command to CH32 gateway".

## Two: What To Submit

For each new WROOM kit capability, produce these assets:

1. ESP32-WROOM gateway-side component or helper API.
2. A minimal ESP32-WROOM example that sends one CH32 command and checks ACK/status.
3. A recipe example for common user behavior when needed.
4. CH32 protocol metadata describing command IDs, payload fields, response fields, timeout, and failure behavior.
5. MOCE module context card.
6. MOCE recipe context card.
7. Validation note with compile status, hardware test status, expected serial logs, and known limits.

Do not submit only one large mixed demo. A complex kit can have a composition recipe, but every downstream module still needs its own small protocol capability.

## Three: Directory Expectations

Prefer these paths unless the repository already has a stronger convention:

- ESP32-WROOM gateway helper component: `components_esp32wroom/<gateway_or_capability_id>/`
- ESP32-WROOM minimal example: `example_esp32wroom/<recipe_id>/`
- Module context: `docs/context/modules/<module_id>.md`
- Recipe context: `docs/context/examples/<recipe_id>.md`
- CH32 protocol contract: `docs/context/protocols/<protocol_id>.md`
- Board or kit context: `docs/context/boards/<board_or_kit_id>.md`

Do not place WROOM gateway modules under ESP32-S3 direct-driver folders.

## Four: ESP32-WROOM Code Responsibilities

ESP32-WROOM code may do:

- Initialize board, CAN/UART gateway transport, timers, logs, and app state.
- Encode CH32 command frames.
- Send commands to CH32.
- Receive and parse ACK/status/data frames from CH32.
- Apply user-level state machines.
- Apply safety behavior based on CH32-reported sensor data.
- Print serial logs.
- Retry or enter safe state on CH32 timeout.

ESP32-WROOM code must not do:

- Initialize downstream OLED/TOF/motor/sensor drivers directly.
- Assume CH32 supports a command that is not in protocol metadata.
- Invent payload fields or response fields.
- Treat a conflict note or unsupported phrase as a selected module.
- Generate CH32 firmware unless the user explicitly asks for CH32 firmware work in a separate task.

## Five: CH32 Protocol Contract

Every gateway capability must include a protocol contract block. Keep IDs stable.

```markdown
## MOCE_CH32_PROTOCOL_CONTRACT
protocol_id: <stable_protocol_id>
kit_id: <kit_or_board_id>
gateway_board: ch32_gateway
esp_board:
  - my_board_esp32wroom
transport: can | uart | spi | other
transport_config:
  bitrate_or_baud: <value>
  frame_endian: little | big | n/a
capability_id: <module_capability_id>
downstream_module: <human_module_name>
ch32_firmware_status: fixed | test_firmware | draft
commands:
  - name: <command_name>
    id: <hex_or_decimal_id>
    direction: esp32_to_ch32
    payload:
      - <field_name:type:unit:range>
    ack:
      required: true | false
      timeout_ms: <number>
responses:
  - name: <response_name>
    id: <hex_or_decimal_id>
    direction: ch32_to_esp32
    payload:
      - <field_name:type:unit:range>
failure_policy: warn | retry | safe_stop | block_start
safe_defaults:
  - <default value or behavior>
unsupported:
  - <common user request that this protocol cannot satisfy>
serial_log:
  - <expected ESP32 log fields>
## END_MOCE_CH32_PROTOCOL_CONTRACT
```

If the CH32 protocol is unknown, stop and request the protocol document. Do not infer command IDs.

## Six: Module Context Contract

For WROOM, a downstream module is selected as a CH32 gateway capability, not as an ESP32 direct driver.

```markdown
## MOCE_MODULE_CONTRACT
module_id: <stable_id>
display_name: <human_name>
category: input | output | actuator | display | communication | storage | board | power | gateway | sensor
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
gateway_required: true
component_paths:
  - <esp32_wroom_gateway_component_path>
example_paths:
  - <esp32_wroom_example_path>
protocol_contracts:
  - <protocol_contract_path_or_id>
hardware_interface: can_gateway
downstream_interface: gpio | i2c | spi | uart | pwm | adc | can | other
required_resources:
  - ESP32-WROOM gateway transport resource
  - CH32-side downstream resource if known
capabilities:
  - <small atomic capability exposed to users>
unsupported:
  - <unsupported request>
user_phrases:
  - <plain user wording>
failure_policy: warn | retry | safe_stop | block_start
safe_defaults:
  - <default timing/threshold/duty/text>
composition_notes:
  - <how this capability combines with other gateway capabilities>
forbidden_contamination:
  - <features that must not appear unless separately selected>
validation_status: untested | compile_passed | bench_passed | board_passed | integrated_passed
## END_MOCE_MODULE_CONTRACT
```

## Seven: Recipe Context Contract

Every WROOM recipe must state the gateway path and selected protocol capabilities.

```markdown
## MOCE_RECIPE_CONTRACT
recipe_id: <stable_id>
purpose: <one sentence>
supported_boards:
  - my_board_esp32wroom
architecture: esp32_wroom_to_ch32_gateway
selected_modules:
  - ch32_gateway
  - <gateway_capability_id>
protocol_contracts:
  - <protocol_contract_id>
user_goal:
  - <plain user wording this recipe satisfies>
required_behavior:
  - <observable behavior>
esp32_responsibilities:
  - <state machine / command send / ACK check / serial log>
ch32_responsibilities:
  - <fixed downstream action, described but not generated>
serial_log:
  - <expected ESP32 log line or fields>
state_machine:
  - none | <short state list>
failure_behavior:
  - <what ESP32 does on timeout, bad ACK, sensor failure, or command failure>
must_not_include:
  - <unselected modules/features>
compile_command: <if known>
hardware_test_status: untested | bench_passed | board_passed | integrated_passed
## END_MOCE_RECIPE_CONTRACT
```

## Eight: Common Capability Patterns

OLED through CH32:

- ESP32 sends display text, page, or status command.
- CH32 updates OLED.
- ESP32 logs command result and ACK.
- ESP32 must not include direct OLED I2C driver calls.

TOF through CH32:

- ESP32 requests distance or receives periodic distance frames.
- CH32 reads TOF locally.
- Safety logic may run on ESP32 if distance is reported by CH32.
- If TOF is safety-critical and data timeout occurs, ESP32 enters safe stop.

Motor through CH32:

- ESP32 sends duty, direction, stop, or mode command.
- CH32 drives motor hardware locally.
- DC motor without encoder supports open-loop duty and direction only.
- Precise distance, speed, or position control requires encoder protocol and tested closed-loop recipe.

LED through CH32:

- If LED is on WROOM board, it can be ESP32 local only when board metadata confirms it.
- If LED is on a downstream module, use CH32 command.
- Do not add motor, TOF, MPU, button, WiFi, or Bluetooth to an LED-only recipe unless selected.

Sensor through CH32:

- ESP32 receives values from CH32 status/data frames.
- ESP32 may display, log, or make state-machine decisions from those values.
- Sensor init/read code belongs to CH32 fixed firmware, not ESP32 generated firmware.

## Nine: User Language Mapping

Map beginner language to gateway capabilities:

- "屏幕显示 hello" -> CH32 OLED display text command.
- "测一下距离" -> CH32 TOF distance read/status capability.
- "靠近就停" -> CH32 TOF data plus CH32 motor command, with ESP32 safety state.
- "电机慢慢转" -> CH32 motor duty command.
- "让灯闪一下" -> local WROOM LED only if board metadata confirms; otherwise CH32 LED command.
- "把这些接到 ESP 上控制" -> ESP32 sends commands to CH32, CH32 controls modules.

Unsupported or incomplete examples:

- "精准走 30cm" requires encoder protocol and closed-loop recipe.
- "识别图像" requires camera and recognition pipeline.
- "手机蓝牙控制" requires Bluetooth protocol support.
- "直接让 ESP32 读 OLED/TOF" conflicts with WROOM gateway architecture.

When adjusting a user request, record the adjustment clearly. Keep the main goal when possible.

## Ten: Validation Checklist

Before approving a WROOM gateway submission:

- The ESP32-WROOM code builds.
- The example sends only documented CH32 commands.
- Every sent command checks ACK/status or defines why ACK is not required.
- Timeouts have explicit behavior.
- Safety-critical missing data enters safe state.
- No ESP32 direct downstream driver calls appear.
- No unselected modules appear in code, context, resource planning, or diagrams.
- Module context contract exists.
- Recipe context contract exists.
- CH32 protocol contract exists.
- User-language triggers exist.
- Unsupported phrases exist.
- Hardware test status is recorded.
- Serial logs are explicit and checkable.

Do not approve a submission only because it compiles.

## Eleven: Review Output Order

When reviewing a WROOM gateway submission, answer in this order:

1. Accepted gateway capabilities and recipe IDs.
2. Missing CH32 protocol contracts.
3. ESP32 direct-driver boundary violations.
4. Hidden feature contamination risks.
5. ACK, timeout, and failure behavior risks.
6. Compile and hardware test status.
7. Required changes before MOCE Designer can consume it.

## Twelve: Final Effect

After this skill is followed, MOCE Designer should be able to treat every WROOM module as a CH32 gateway capability. A user can describe a simple or complex behavior in plain language, and the App can select modules, plan resources, draw the topology, and generate ESP32-WROOM firmware without guessing downstream drivers. The generated ESP32 code should only speak to CH32, while CH32 remains the fixed module-control layer.
