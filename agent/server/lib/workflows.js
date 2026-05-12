const sdkBoundary = `只允许向 project/ 下创建或修改应用工程。不要修改 components/、boards/、tools/、env/、third_party/、examples/。优先复用当前 SDK 的 BSP、driver_* 和 service_* 接口。`;

const skillsetSummary = `
Skillset 1: 产品规划、需求澄清、任务拆解、器件选型、硬件资源规划、搭建框图、用户 Q&A。
Skillset 2: 硬件模块家族设计生产、硬件搭建、嵌入式开发、测试调试、physical agent 上位机测试预留、结构装配和整机联调。
Skillset 3: 从框图到电路原理图优化、电路板量产设计、datasheet 关键信息提取、AI 原理图生成接口预留。
`;

function slugifyProjectName(input) {
  const base = String(input || 'robot_agent_app')
    .toLowerCase()
    .replace(/[^a-z0-9_ -]+/g, '')
    .trim()
    .replace(/\s+/g, '_')
    .replace(/-+/g, '_');
  return base || 'robot_agent_app';
}

function componentNames(sdkSummary) {
  return (sdkSummary.components || []).map((component) => component.name);
}

function availableApiText(sdkSummary) {
  return (sdkSummary.components || [])
    .map((component) => {
      const apis = component.api
        .map((api) => `${api.header}: ${api.functions.join(', ')}`)
        .join('\n');
      return `${component.name}\n${apis}`;
    })
    .join('\n\n');
}

function detectCapabilities(requirement, sdkSummary) {
  const text = String(requirement || '').toLowerCase();
  const components = new Set();
  const notes = [];

  if (/led|灯|亮度|闪烁|呼吸/.test(text)) {
    components.add('driver_led');
    notes.push('LED 状态、亮度、反馈提示');
  }
  if (/button|按键|按钮|短按|长按/.test(text)) {
    components.add('driver_button');
    notes.push('按键输入和模式切换');
  }
  if (/servo|舵机|关节|云台/.test(text)) {
    components.add('driver_servo');
    notes.push('舵机执行机构');
  }
  if (/oled|display|屏|显示/.test(text)) {
    components.add('driver_oled');
    notes.push('本地状态显示');
  }
  if (/mpu|imu|姿态|陀螺|加速度/.test(text)) {
    components.add('driver_mpu6050');
    notes.push('IMU 姿态/运动感知');
  }
  if (/tof|vl53|distance|测距|避障|距离/.test(text)) {
    components.add('driver_tof2000c_vl53l0x');
    notes.push('ToF 测距/避障');
  }
  if (/tts|语音|播报|喇叭/.test(text)) {
    components.add('driver_tw_tts');
    notes.push('语音播报模块');
  }
  if (/wifi|tcp|udp|网络|上位机|remote|远程/.test(text)) {
    components.add('service_wifi');
    notes.push('WiFi 网络和上位机通信');
  }

  const known = new Set(componentNames(sdkSummary));
  return {
    components: [...components].filter((name) => known.has(name)),
    notes
  };
}

function boardResourceLines(sdkSummary) {
  const board = (sdkSummary.boards || []).find((item) => item.name.includes('esp32s3')) || (sdkSummary.boards || [])[0];
  if (!board) {
    return ['当前未扫描到 boards/ 下的板级定义。'];
  }

  const define = board.defines || {};
  const groups = [
    ['LED', ['BOARD_LED_GPIO', 'BOARD_LED_PWM_CHANNEL', 'BOARD_LED_PWM_FREQUENCY_HZ']],
    ['Button', ['BOARD_BUTTON_GPIO', 'BOARD_BUTTON_ACTIVE_LEVEL']],
    ['Servo', ['BOARD_SERVO_GPIO_0', 'BOARD_SERVO_GPIO_1', 'BOARD_SERVO_PWM_FREQUENCY_HZ']],
    ['I2C', ['BOARD_I2C_SDA_GPIO', 'BOARD_I2C_SCL_GPIO', 'BOARD_I2C_FREQUENCY_HZ']],
    ['UART', ['BOARD_UART_TX_GPIO', 'BOARD_UART_RX_GPIO', 'BOARD_UART_BAUD_RATE']]
  ];

  return [
    `默认参考板卡：${board.name}`,
    ...groups.map(([name, keys]) => {
      const values = keys
        .filter((key) => Object.prototype.hasOwnProperty.call(define, key))
        .map((key) => `${key}=${define[key]}`)
        .join(', ');
      return values ? `${name}: ${values}` : `${name}: 当前板级文件未声明`;
    })
  ];
}

function buildMermaid(capabilities) {
  const nodes = [
    '用户需求',
    'Moce Hardware Agent',
    '任务拆解',
    '器件选型',
    '硬件资源规划',
    'ESP32 SDK 应用'
  ];
  if (capabilities.components.includes('service_wifi')) {
    nodes.push('上位机/physical agent 接口');
  }
  if (capabilities.components.some((name) => name.startsWith('driver_'))) {
    nodes.push('硬件模块与传感器');
  }

  return [
    'flowchart LR',
    '  A[用户需求] --> B[Moce Hardware Agent]',
    '  B --> C[任务拆解]',
    '  C --> D[器件选型]',
    '  D --> E[硬件资源规划]',
    '  E --> F[ESP32 SDK 应用]',
    capabilities.components.includes('service_wifi') ? '  F <--> G[上位机 / physical agent 接口]' : '',
    capabilities.components.some((name) => name.startsWith('driver_')) ? '  F --> H[硬件模块与传感器]' : ''
  ].filter(Boolean).join('\n');
}

export function normalizeAgentText(text) {
  return String(text || '').trim();
}

export function parseGeneratedFiles(text) {
  const raw = String(text || '');
  const files = [];
  const fileMarker = /^===== FILE:\s*(project\/[^\r\n=]+?)\s*=====\s*$/gm;
  const matches = [...raw.matchAll(fileMarker)];

  for (let index = 0; index < matches.length; index += 1) {
    const current = matches[index];
    const next = matches[index + 1];
    const pathText = current[1].trim();
    const start = current.index + current[0].length;
    const end = next ? next.index : raw.length;
    let content = raw.slice(start, end).trim();
    const fenced = content.match(/^```[a-zA-Z0-9_-]*\s*([\s\S]*?)```$/);
    if (fenced) {
      content = fenced[1].trim();
    }
    if (pathText && content) {
      files.push({ path: pathText, content });
    }
  }

  return files;
}

export function buildPlanningPrompt({ requirement, sdkSummary, skillFocus }) {
  return {
    messages: [
      {
        role: 'system',
        content: `你是 Moce SDK 的机器人/智能硬件开发 agent。${sdkBoundary}\n${skillsetSummary}\n输出必须结构化、具体、可执行。`
      },
      {
        role: 'user',
        content: [
          `当前聚焦：${skillFocus}`,
          `用户需求：\n${requirement || '(空)'}`,
          `当前 SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `BSP：${(sdkSummary.bsp || []).join(', ')}`,
          `Boards：${(sdkSummary.boards || []).map((board) => board.name).join(', ')}`,
          `Examples：${(sdkSummary.examples || []).join(', ')}`,
          '请输出：需求澄清问题、任务拆解、器件选型、硬件资源规划、Mermaid 框图、嵌入式开发计划、测试调试计划、风险和下一步。'
        ].join('\n\n')
      }
    ]
  };
}

export function buildChatPrompt({ question, requirement, currentPlan, sdkSummary }) {
  return {
    messages: [
      {
        role: 'system',
        content: `你是 Moce SDK 的硬件产品开发问答 agent。${sdkBoundary}`
      },
      {
        role: 'user',
        content: [
          `用户需求：${requirement || '(未填写)'}`,
          `当前规划：${currentPlan || '(暂无)'}`,
          `SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `问题：${question || '(空)'}`
        ].join('\n\n')
      }
    ]
  };
}

export function buildCodegenPrompt({ requirement, projectName, plan, sdkSummary }) {
  return {
    messages: [
      {
        role: 'system',
        content: [
          '你是 Moce SDK 的嵌入式应用代码生成 agent。',
          sdkBoundary,
          '生成完整可编译 ESP-IDF 应用工程。必须包含 project/<name>/CMakeLists.txt、project/<name>/main/CMakeLists.txt、project/<name>/main/main.c。',
          '只输出文件路径和完整文件内容，不输出 SDK 目录下文件。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `工程名：project/${slugifyProjectName(projectName || 'robot_agent_app')}/`,
          `用户需求：${requirement || '(未填写)'}`,
          `当前规划：${plan || '(暂无)'}`,
          `可用 API：\n${availableApiText(sdkSummary)}`
        ].join('\n\n')
      }
    ]
  };
}

export function fallbackPlanning(requirement, sdkSummary) {
  const capabilities = detectCapabilities(requirement, sdkSummary);
  const selected = capabilities.components.length > 0 ? capabilities.components : ['service_device'];
  const diagram = buildMermaid(capabilities);

  return [
    '# Moce Hardware Agent 规划草案',
    '',
    '## 需求理解',
    requirement ? `- 原始需求：${requirement}` : '- 尚未填写具体产品需求，当前生成通用机器人/智能硬件开发流程。',
    `- 推荐首轮 SDK 能力：${selected.join(', ')}`,
    '',
    '## 需要确认的问题',
    '- 产品形态：桌面设备、移动机器人、机械臂、传感器节点，还是其它形态？',
    '- 供电方式：USB、锂电池、外部 DC，是否需要充电和电源保护？',
    '- 是否需要网络/上位机/手机端控制？延迟和稳定性要求是多少？',
    '- 传感器精度、刷新率、安装位置、环境光/振动/温度条件是否有约束？',
    '- 是否只做开发板验证，还是要进入 PCB 和量产设计？',
    '',
    '## 任务拆解',
    '1. 固化产品需求：功能、性能、尺寸、成本、供电、安全边界。',
    '2. 拆解硬件模块：主控、电源、传感器、执行器、显示/交互、通信、调试接口。',
    '3. 完成器件选型：优先匹配当前 SDK 已有 driver/service，缺口列入待开发模块。',
    '4. 做硬件资源规划：GPIO/I2C/UART/PWM/ADC/CAN/RS485 等资源分配和冲突检查。',
    '5. 生成搭建框图与接线表：用于面包板/开发板验证。',
    '6. 生成 ESP-IDF 应用工程：只写入 project/ 下，复用 SDK 组件。',
    '7. 编译、烧录、日志监控、测试调试。',
    '8. 预留 physical agent 上位机接口，用于后续自动化测试。',
    '',
    '## 器件选型建议',
    ...capabilities.notes.map((note) => `- ${note}`),
    capabilities.notes.length === 0 ? '- 当前需求未命中具体器件关键词，建议先从 ESP32-S3 主控、按键、LED、OLED、常用 I2C 传感器开始验证。' : '',
    '',
    '## 硬件资源规划',
    ...boardResourceLines(sdkSummary).map((line) => `- ${line}`),
    '',
    '## 搭建框图',
    '```mermaid',
    diagram,
    '```',
    '',
    '## 嵌入式开发计划',
    '- 初始化 service_device 或所需 driver/service。',
    '- 将业务逻辑拆成输入处理、状态机、输出控制、诊断日志四块。',
    '- 若涉及网络或上位机，先定义最小 TCP/UDP/串口协议，再接入 physical agent。',
    '- 每个硬件模块先跑 examples/ 中相近 demo，再组合进 project/ 工程。',
    '',
    '## 测试调试计划',
    '- 上电检查：供电、电流、串口日志、I2C 地址扫描、GPIO 电平。',
    '- 单模块测试：传感器读数、执行器动作、显示刷新、网络连接。',
    '- 整机测试：模式切换、异常断连、长时间运行、边界输入。',
    '- 量产前补充：ESD、电源纹波、连接器可靠性、烧录治具、版本追踪。',
    '',
    '## 风险',
    '- 当前 SDK 组件覆盖的是开发板级验证能力，PCB/原理图自动生成仍需人工审核。',
    '- physical agent 暂无现成仓库接口，首版只能保留适配层。',
    '- datasheet 抽取和 AI 原理图生成对准确性要求高，后续需要元件库和规则校验。'
  ].filter(Boolean).join('\n');
}

export function fallbackChat(question, sdkSummary) {
  const q = String(question || '');
  if (/组件|api|接口|driver|service/i.test(q)) {
    return [
      '当前 SDK 已扫描到这些组件：',
      componentNames(sdkSummary).map((name) => `- ${name}`).join('\n'),
      '',
      '建议优先从 examples/ 中找相近 demo，再在 project/ 下创建应用工程。'
    ].join('\n');
  }

  if (/引脚|gpio|资源|i2c|uart|pwm/i.test(q)) {
    return boardResourceLines(sdkSummary).map((line) => `- ${line}`).join('\n');
  }

  return [
    '我会按当前 SDK 的边界回答：只基于已有 BSP、driver_*、service_* 能力规划，不建议直接改 SDK 内部。',
    '如果问题涉及具体产品，请补充产品形态、输入输出模块、供电方式、是否联网、是否量产。'
  ].join('\n');
}

function makeCmakeRoot(projectName) {
  return `cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    \${CMAKE_CURRENT_LIST_DIR}/../../bsp
    \${CMAKE_CURRENT_LIST_DIR}/../../components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${projectName})
`;
}

function makeMainCmake(requires) {
  return `idf_component_register(
    SRCS "main.c"
    REQUIRES ${requires.join(' ')}
)
`;
}

function makeMainC(requirement, capabilities) {
  const includes = [
    '#include "freertos/FreeRTOS.h"',
    '#include "freertos/task.h"',
    '#include "esp_log.h"',
    '#include "service_device.h"'
  ];
  if (capabilities.components.includes('driver_led')) includes.push('#include "driver_led.h"');
  if (capabilities.components.includes('driver_button')) includes.push('#include "driver_button.h"');
  if (capabilities.components.includes('driver_oled')) includes.push('#include "driver_oled.h"');
  if (capabilities.components.includes('driver_servo')) includes.push('#include "driver_servo.h"');
  if (capabilities.components.includes('driver_mpu6050')) includes.push('#include "driver_mpu6050.h"');
  if (capabilities.components.includes('driver_tof2000c_vl53l0x')) includes.push('#include "driver_tof2000c_vl53l0x.h"');

  const setup = ['    service_device_init();'];
  if (capabilities.components.includes('driver_led')) setup.push('    driver_led_set_brightness(50);', '    driver_led_set(1);');
  if (capabilities.components.includes('driver_oled')) setup.push('    driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);', '    driver_oled_clear_screen();');
  if (capabilities.components.includes('driver_mpu6050')) setup.push('    driver_mpu6050_init_default();');
  if (capabilities.components.includes('driver_tof2000c_vl53l0x')) setup.push('    driver_tof2000c_vl53l0x_init_default();');

  const loop = [];
  if (capabilities.components.includes('driver_button')) {
    loop.push(
      '        driver_button_process();',
      '        driver_button_event_t event = driver_button_get_event();',
      '        if (event == DRIVER_BUTTON_EVENT_SHORT_PRESS) {',
      '            ESP_LOGI(TAG, "button short press");',
      capabilities.components.includes('driver_led') ? '            driver_led_toggle();' : '            /* TODO: handle short press */',
      '        }'
    );
  }
  if (capabilities.components.includes('driver_mpu6050')) {
    loop.push(
      '        driver_mpu6050_data_t imu;',
      '        if (driver_mpu6050_read(&imu) == ESP_OK) {',
      '            ESP_LOGI(TAG, "imu ax=%.2f ay=%.2f az=%.2f", imu.accel_x_g, imu.accel_y_g, imu.accel_z_g);',
      '        }'
    );
  }
  if (capabilities.components.includes('driver_tof2000c_vl53l0x')) {
    loop.push(
      '        driver_tof2000c_vl53l0x_result_t range;',
      '        if (driver_tof2000c_vl53l0x_read_single(&range) == ESP_OK) {',
      '            ESP_LOGI(TAG, "distance=%u mm", range.distance_mm);',
      '        }'
    );
  }
  if (loop.length === 0) {
    loop.push('        ESP_LOGI(TAG, "agent scaffold heartbeat");');
  }
  loop.push('        vTaskDelay(pdMS_TO_TICKS(500));');

  return `${includes.join('\n')}

static const char *TAG = "moce_agent_app";

void app_main(void)
{
    ESP_LOGI(TAG, "Moce agent generated scaffold start");
    ESP_LOGI(TAG, "requirement: ${String(requirement || 'not specified').replaceAll('\\', '\\\\').replaceAll('"', '\\"')}");

${setup.join('\n')}

    while (1) {
${loop.join('\n')}
    }
}
`;
}

export function fallbackCodegen(requirement, projectName, sdkSummary) {
  const name = slugifyProjectName(projectName || 'robot_agent_app');
  const capabilities = detectCapabilities(requirement, sdkSummary);
  const requires = new Set(['service_device']);
  for (const component of capabilities.components) {
    requires.add(component);
  }

  return {
    projectName: name,
    files: [
      {
        path: `project/${name}/CMakeLists.txt`,
        content: makeCmakeRoot(name)
      },
      {
        path: `project/${name}/main/CMakeLists.txt`,
        content: makeMainCmake([...requires])
      },
      {
        path: `project/${name}/main/main.c`,
        content: makeMainC(requirement, capabilities)
      }
    ]
  };
}
