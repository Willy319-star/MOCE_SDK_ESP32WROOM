const sdkBoundary = `只允许向 project/ 下创建或修改应用工程。不要修改 components/、boards/、tools/、env/、third_party/、examples/。优先复用当前 SDK 的 BSP、driver_* 和 service_* 接口。`;

const skillsetSummary = `
Skillset 1: 产品规划、需求澄清、功能分析、器件选型、硬件资源规划、搭建框图、用户 Q&A。
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
        .map((api) => {
          const declarations = (api.declarations && api.declarations.length > 0)
            ? `Declarations:\n${api.declarations.join('\n\n')}\n`
            : '';
          const signatures = (api.prototypes && api.prototypes.length > 0)
            ? api.prototypes.join('\n')
            : api.functions.join(', ');
          return `${api.header}:\n${declarations}Functions:\n${signatures}`;
        })
        .join('\n');
      return `${component.name}\n${apis}`;
    })
    .join('\n\n');
}

function detectCapabilities(requirement, sdkSummary) {
  const text = String(requirement || '').toLowerCase();
  const components = new Set();
  const notes = [];

  if (/(^|[^a-z])led([^a-z]|$)|灯|亮度|闪烁|呼吸/.test(text)) {
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

function boardResourceLines(sdkSummary, boardName = '') {
  const board = (sdkSummary.boards || []).find((item) => item.name === boardName)
    || (sdkSummary.boards || []).find((item) => item.name.includes('esp32s3'))
    || (sdkSummary.boards || [])[0];
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
    '功能分析',
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
    '  B --> C[功能分析]',
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

function selectedComponentText(capabilities) {
  if (capabilities.components.length === 0) {
    return '未命中具体组件，请根据需求继续判断。';
  }
  return capabilities.components.join(', ');
}

export function buildHardwareBuildPrompt({ requirement, sdkSummary, boardName, analysis = '', componentSelection = '', resourcePlan = '' }) {
  return {
    messages: [
      {
        role: 'system',
        content: [
          `你是 Moce SDK 的硬件搭建 agent。${sdkBoundary}`,
          skillsetSummary,
          '本阶段只做硬件搭建：根据已接受的硬件资源规划、器件接口和目标板卡资源，生成模块连接框图、接线清单和上电检查。',
          '不要重新做完整开发规划，不要覆盖或重新分配硬件资源规划中的引脚、接口和地址。',
          'Mermaid 框图必须体现 ESP32/目标板、模块、接口类型、GPIO/通道/地址、电源和共地关系。',
          '如果模块接口或电源参数不确定，必须标为待确认。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `目标板卡：${boardName || '未指定'}`,
          `用户需求：\n${requirement || '(空)'}`,
          `当前 SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `BSP：${(sdkSummary.bsp || []).join(', ')}`,
          `Boards：${(sdkSummary.boards || []).map((board) => board.name).join(', ')}`,
          `Examples：${(sdkSummary.examples || []).join(', ')}`,
          `目标板卡资源：\n${boardResourceLines(sdkSummary, boardName).join('\n')}`,
          analysis ? `已接受的功能分析：\n${analysis}` : '已接受的功能分析：暂无，请先按需求分析和硬件功能拆解 skill 补齐。',
          componentSelection ? `已接受的器件选型：\n${componentSelection}` : '已接受的器件选型：暂无，请先完成器件选型。',
          resourcePlan ? `已接受的硬件资源规划：\n${resourcePlan}` : '已接受的硬件资源规划：暂无，请先完成硬件资源规划。',
          [
            '请只输出硬件搭建文档，格式如下：',
            '## 硬件搭建摘要',
            '## 模块连接框图',
            '```mermaid',
            'flowchart LR',
            '  MCU[目标板/ESP32] -- 接口/引脚/地址 --> MODULE[模块]',
            '```',
            '## 模块接线清单',
            '| 模块 | 模块接口/管脚 | 目标板接口/宏 | GPIO/通道/地址 | 供电 | 共地 | 状态 | 备注 |',
            '## 接口分组',
            '| 接口/总线 | 连接模块 | 引脚/端口 | 地址/参数 | 注意事项 |',
            '## 电源和共地',
            '| 模块 | 供电电压 | 峰值电流/功耗 | 供电来源 | 共地要求 | 风险 |',
            '## 上电前检查',
            '## 单模块验证顺序',
            '## 待确认问题',
            '',
            '状态只能使用：可接线、共享可用、待确认、冲突、缺口。'
          ].join('\n')
        ].join('\n\n')
      }
    ]
  };
}

export function buildHardwareResourcePlanningPrompt({ requirement, analysis, componentSelection, resourceNotes = '', sdkSummary, boardName }) {
  return {
    messages: [
      {
        role: 'system',
        content: [
          '你是 Moce SDK 的硬件资源规划 agent。',
          sdkBoundary,
          '本阶段只做目标板卡上的接口、引脚和资源分配，不生成固件代码。',
          '必须基于已接受的器件选型，为每个器件或模块分配板级资源。',
          '必须使用目标板卡已有 BOARD_* 宏和已扫描到的 board 定义，不要猜 GPIO。',
          '必须检查 GPIO、I2C 地址、UART、PWM/LEDC、PCNT/编码器等资源冲突。',
          '如果资源不足或不确定，必须明确标为缺口或待确认。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `目标板卡：${boardName || '未指定'}`,
          `用户需求：\n${requirement || '(空)'}`,
          `已接受的功能分析：\n${analysis || '(空)'}`,
          `已接受的器件选型：\n${componentSelection || '(空)'}`,
          resourceNotes ? `用户对资源规划的补充/调整要求：\n${resourceNotes}` : '用户对资源规划的补充/调整要求：暂无',
          `当前 SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `BSP：${(sdkSummary.bsp || []).join(', ')}`,
          `Boards：${(sdkSummary.boards || []).map((board) => board.name).join(', ')}`,
          `Examples：${(sdkSummary.examples || []).join(', ')}`,
          `目标板卡资源：\n${boardResourceLines(sdkSummary, boardName).join('\n')}`,
          [
            '请只输出硬件资源规划，格式如下：',
            '## 硬件资源规划摘要',
            '## 接口和引脚分配',
            '| 功能/器件 | SDK 组件 | 接口 | 板级资源/宏 | GPIO/通道/地址 | 共享/独占 | 状态 | 备注 |',
            '## 共享总线规划',
            '| 总线 | 引脚/端口 | 挂载器件 | 地址/参数 | 冲突检查 | 备注 |',
            '## 独占资源规划',
            '| 资源类型 | 资源 | 使用者 | 参数 | 状态 | 备注 |',
            '## 冲突检查',
            '| 检查项 | 结果 | 说明 | 处理建议 |',
            '## 接线和初始化建议',
            '| 顺序 | 模块 | 接线/初始化动作 | 依赖 | 验证方式 |',
            '## 待确认问题',
            '## 下一步建议',
            '',
            '状态只能使用：已分配、共享可用、冲突、待确认、缺口。'
          ].join('\n')
        ].join('\n\n')
      }
    ]
  };
}

export function buildComponentSelectionPrompt({ requirement, analysis, selectionNotes = '', sdkSummary, boardName }) {
  return {
    messages: [
      {
        role: 'system',
        content: [
          '你是 Moce SDK 的器件选型 agent。',
          sdkBoundary,
          '本阶段只做器件或模块选型，不生成固件代码。',
          '必须基于已接受的功能分析，为每个硬件功能推荐器件或模块。',
          '必须输出关键参数、接口资源、SDK 适配状态、候选比较、风险和待确认项。',
          '如果资料不足或参数不确定，必须明确标为待确认，不要臆造。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `目标板卡：${boardName || '未指定'}`,
          `用户需求：\n${requirement || '(空)'}`,
          `已接受的功能分析：\n${analysis || '(空)'}`,
          selectionNotes ? `用户对选型的补充/调整要求：\n${selectionNotes}` : '用户对选型的补充/调整要求：暂无',
          `当前 SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `BSP：${(sdkSummary.bsp || []).join(', ')}`,
          `Boards：${(sdkSummary.boards || []).map((board) => board.name).join(', ')}`,
          `Examples：${(sdkSummary.examples || []).join(', ')}`,
          `目标板卡资源：\n${boardResourceLines(sdkSummary, boardName).join('\n')}`,
          [
            '请只输出器件选型，格式如下：',
            '## 器件选型摘要',
            '## 功能到器件映射',
            '| 功能 | 推荐器件/模块 | 类型 | 接口 | 供电/逻辑电平 | SDK 组件 | 板级资源 | 推荐等级 | 备注 |',
            '## 候选器件比较',
            '| 功能 | 候选项 | 关键参数 | 优点 | 缺点/风险 | 资料来源 | 结论 |',
            '## 关键参数清单',
            '| 器件/模块 | 供电 | 接口 | 地址/通道 | 性能参数 | 电流/功耗 | 尺寸/结构 | 待确认 |',
            '## SDK 适配和资源占用',
            '| 器件/模块 | SDK 支持状态 | 推荐组件/API | 需要资源 | 示例工程 | 适配工作量 | 风险 |',
            '## 不推荐或淘汰项',
            '## 待确认问题',
            '## 下一步建议',
            '',
            '推荐等级只能使用：推荐、可选、待确认、不推荐。'
          ].join('\n')
        ].join('\n\n')
      }
    ]
  };
}

export function buildRequirementAnalysisPrompt({ requirement, sdkSummary, boardName }) {
  const capabilities = detectCapabilities(requirement, sdkSummary);
  return {
    messages: [
      {
        role: 'system',
        content: [
          '你是 Moce SDK 的需求分析和硬件功能拆解 agent。',
          sdkBoundary,
          '本阶段只做功能分析，不生成固件代码，不进入完整开发规划。',
          '必须把自然语言需求拆成需要硬件实现的功能，并映射到当前 SDK 能力。',
          '用户需求中出现的每个硬件模块都必须逐项覆盖；如果当前 SDK 不支持，必须明确标为缺口或待确认。',
          '硬件资源必须以目标板卡资源为准，不要猜 GPIO。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `目标板卡：${boardName || '未指定'}`,
          `用户需求：\n${requirement || '(空)'}`,
          `根据需求初步命中的 SDK 组件：${selectedComponentText(capabilities)}`,
          `当前 SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `BSP：${(sdkSummary.bsp || []).join(', ')}`,
          `Boards：${(sdkSummary.boards || []).map((board) => board.name).join(', ')}`,
          `Examples：${(sdkSummary.examples || []).join(', ')}`,
          `目标板卡资源：\n${boardResourceLines(sdkSummary, boardName).join('\n')}`,
          [
            '请只输出功能分析，格式如下：',
            '## 需求摘要',
            '## 硬件功能清单',
            '| 功能 | 类型 | 用户行为/系统行为 | SDK 组件 | 板级资源 | 状态 | 备注 |',
            '## SDK 能力映射',
            '| 需求项 | 推荐组件/API | 示例或参考 | 支持状态 | 风险 |',
            '## 待确认问题',
            '## 缺口和风险',
            '## 建议的下一步',
            '',
            '状态只能使用：已支持、部分支持、缺口、待确认。'
          ].join('\n')
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
          `当前硬件上下文：${currentPlan || '(暂无)'}`,
          `SDK 组件：${componentNames(sdkSummary).join(', ')}`,
          `问题：${question || '(空)'}`
        ].join('\n\n')
      }
    ]
  };
}

export function buildCodegenPrompt({ requirement, projectName, plan, sdkSummary, globalPrompt = '' }) {
  const globalConstraint = String(globalPrompt || '').trim();
  return {
    messages: [
      {
        role: 'system',
        content: [
          '你是 Moce SDK 的嵌入式应用代码生成 agent。',
          sdkBoundary,
          globalConstraint
            ? `以下是 ./prompt/prompt0.md，本轮生成固件草稿必须逐条遵守，优先级高于用户需求中的冲突内容：\n${globalConstraint}`
            : '本轮生成固件草稿必须遵守 ./prompt/prompt0.md 的全局开发约束；如果文件缺失，也必须只输出 project/ 下应用工程。',
          '生成完整可编译 ESP-IDF 应用工程。必须包含 project/<name>/CMakeLists.txt、project/<name>/main/CMakeLists.txt、project/<name>/main/main.c。',
          '根 CMakeLists.txt 必须设置 EXTRA_COMPONENT_DIRS 到 ../../bsp 和 ../../components，并 include $ENV{IDF_PATH}/tools/cmake/project.cmake。',
          'main/CMakeLists.txt 必须使用 idf_component_register(SRCS "main.c" REQUIRES ...)，禁止使用 register_component()、COMPONENT_SRCS、COMPONENT_REQUIRES 等旧式写法。',
          '必须严格使用下方提供的 C 函数原型、结构体字段、类型名和枚举名；不要改名，不要臆造 wrapper，不要把 void 返回值当成 esp_err_t。',
          '输出格式必须严格使用文件块，不要输出解释文字：',
          '===== FILE: project/<name>/CMakeLists.txt =====',
          '<完整文件内容>',
          '===== FILE: project/<name>/main/CMakeLists.txt =====',
          '<完整文件内容>',
          '===== FILE: project/<name>/main/main.c =====',
          '<完整文件内容>',
          '只输出 project/ 下文件，不输出 SDK 目录下文件。'
        ].join('\n')
      },
      {
        role: 'user',
        content: [
          `工程名：project/${slugifyProjectName(projectName || 'robot_agent_app')}/`,
          `用户需求：${requirement || '(未填写)'}`,
          `当前硬件搭建文档：${plan || '(暂无)'}`,
          `可用 API：\n${availableApiText(sdkSummary)}`
        ].join('\n\n')
      }
    ]
  };
}

function componentFunctionType(component) {
  if (component === 'driver_button' || component === 'driver_mpu6050' || component === 'driver_tof2000c_vl53l0x') {
    return '输入';
  }
  if (component === 'driver_led' || component === 'driver_oled' || component === 'driver_servo' || component === 'driver_tw_tts') {
    return '输出';
  }
  if (component === 'service_wifi') {
    return '通信';
  }
  return '待确认';
}

function componentResourceText(component, sdkSummary) {
  const resources = boardResourceLines(sdkSummary).join('；');
  if (component === 'driver_led') return resources.match(/LED:[^；]+/)?.[0] || 'LED 板级资源待确认';
  if (component === 'driver_button') return resources.match(/Button:[^；]+/)?.[0] || 'Button 板级资源待确认';
  if (component === 'driver_servo') return resources.match(/Servo:[^；]+/)?.[0] || 'Servo PWM 资源待确认';
  if (component === 'driver_oled' || component === 'driver_mpu6050' || component === 'driver_tof2000c_vl53l0x') {
    return resources.match(/I2C:[^；]+/)?.[0] || 'I2C 资源待确认';
  }
  if (component === 'driver_tw_tts') return resources.match(/UART:[^；]+/)?.[0] || 'UART 资源待确认';
  if (component === 'service_wifi') return 'WiFi 射频和网络配置';
  return '待确认';
}

function componentPrimaryApi(component, sdkSummary) {
  const item = (sdkSummary.components || []).find((candidate) => candidate.name === component);
  const api = item?.api?.[0];
  if (!api) return '待确认';
  const prototypes = api.prototypes || [];
  return prototypes.slice(0, 3).join('<br>') || (api.functions || []).slice(0, 4).join(', ');
}

function componentCandidate(component) {
  const candidates = {
    driver_led: '板载 LED / 外接单色 LED',
    driver_button: '板载 BOOT 按键 / 独立轻触开关',
    driver_oled: 'SSD1306 0.96 I2C OLED 模块',
    driver_servo: 'SG90 / MG90S PWM 舵机',
    driver_mpu6050: 'MPU6050 I2C IMU 模块',
    driver_tof2000c_vl53l0x: 'VL53L0X ToF 测距模块',
    driver_tw_tts: 'TW TTS UART 语音模块',
    service_wifi: 'ESP32 内置 WiFi',
    driver_motor: '直流减速电机 + 编码器',
    driver_encoder: 'AB 相增量式编码器',
    driver_tb6612: 'TB6612FNG 双路电机驱动模块',
    service_pid: '软件 PID 控制服务'
  };
  return candidates[component] || `${component} 对应模块待确认`;
}

function componentInterface(component) {
  if (component === 'driver_button') return 'GPIO';
  if (component === 'driver_led' || component === 'driver_servo' || component === 'driver_motor' || component === 'driver_tb6612') return 'GPIO/PWM';
  if (component === 'driver_oled' || component === 'driver_mpu6050' || component === 'driver_tof2000c_vl53l0x') return 'I2C';
  if (component === 'driver_tw_tts') return 'UART';
  if (component === 'service_wifi') return 'WiFi';
  if (component === 'driver_encoder') return 'GPIO/PCNT';
  return '待确认';
}

function componentSupply(component) {
  if (component === 'driver_led' || component === 'driver_button') return '3.3V 逻辑，按板级电路';
  if (component === 'driver_oled' || component === 'driver_mpu6050' || component === 'driver_tof2000c_vl53l0x') return '3.3V/5V 模块待确认，I2C 逻辑需兼容 3.3V';
  if (component === 'driver_servo' || component === 'driver_motor' || component === 'driver_tb6612') return '独立电机/舵机电源，电流裕量待确认';
  if (component === 'driver_tw_tts') return '模块供电和喇叭功放电源待确认';
  if (component === 'service_wifi') return '板载 3.3V 供电，关注峰值电流';
  return '待确认';
}

export function fallbackComponentSelection(requirement, analysis, selectionNotes, sdkSummary) {
  const { selected } = hardwareAnalysisRows(`${requirement}\n${analysis}`, sdkSummary);
  const rows = selected.map((component) => ({
    feature: component.replace(/^driver_/, '').replace(/^service_/, ''),
    candidate: componentCandidate(component),
    type: componentFunctionType(component),
    iface: componentInterface(component),
    supply: componentSupply(component),
    resource: componentResourceText(component, sdkSummary),
    api: componentPrimaryApi(component, sdkSummary),
    example: (sdkSummary.examples || []).find((example) => example.includes(component.replace(/^driver_|^service_/, '').split('_')[0])) || '先查找相近 example'
  }));

  return [
    '# 器件选型',
    '',
    '## 器件选型摘要',
    `- 选型目标：基于已接受的功能分析，为 ${rows.map((row) => row.feature).join('、') || '待确认功能'} 选择器件或模块。`,
    '- 目标板卡：优先使用当前选择或默认 ESP32-S3 板级定义。',
    '- 优先约束：当前 SDK 已支持、接口资源可用、资料清晰、适合开发板验证。',
    selectionNotes ? `- 用户调整要求：${selectionNotes}` : '- 用户调整要求：暂无。',
    '- 主要风险：电气参数、供电电流、I2C 地址/PWM/GPIO 资源和结构安装仍需人工确认。',
    '',
    '## 功能到器件映射',
    '| 功能 | 推荐器件/模块 | 类型 | 接口 | 供电/逻辑电平 | SDK 组件 | 板级资源 | 推荐等级 | 备注 |',
    '|---|---|---|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.feature} | ${row.candidate} | ${row.type} | ${row.iface} | ${row.supply} | ${selected.find((name) => name.includes(row.feature)) || row.feature} | ${row.resource} | 推荐 | 优先复用当前 SDK 能力 |`),
    '',
    '## 候选器件比较',
    '| 功能 | 候选项 | 关键参数 | 优点 | 缺点/风险 | 资料来源 | 结论 |',
    '|---|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.feature} | ${row.candidate} | ${row.iface}; ${row.supply} | SDK 适配成本低 | 关键电气/结构参数需查 datasheet | 当前 SDK/examples + 待补 datasheet | 推荐用于首版验证 |`),
    '',
    '## 关键参数清单',
    '| 器件/模块 | 供电 | 接口 | 地址/通道 | 性能参数 | 电流/功耗 | 尺寸/结构 | 待确认 |',
    '|---|---|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.candidate} | ${row.supply} | ${row.iface} | ${row.resource} | 需按 datasheet 确认 | 需按 datasheet 确认 | 需按模块规格确认 | 电气、采购、安装和替代料 |`),
    '',
    '## SDK 适配和资源占用',
    '| 器件/模块 | SDK 支持状态 | 推荐组件/API | 需要资源 | 示例工程 | 适配工作量 | 风险 |',
    '|---|---|---|---|---|---|---|',
    ...rows.map((row, index) => `| ${row.candidate} | 已支持或部分支持 | ${row.api} | ${row.resource} | ${row.example} | ${index === 0 ? '低' : '低/中'} | 需确认资源冲突 |`),
    '',
    '## 不推荐或淘汰项',
    '| 器件/模块 | 淘汰原因 | 可替代方案 |',
    '|---|---|---|',
    '| 资料不完整或电气不兼容模块 | 暂无可靠参数或与 3.3V IO/供电能力冲突 | 使用上表推荐模块 |',
    '',
    '## 待确认问题',
    '- 是否指定具体品牌、封装、尺寸、采购渠道或成本上限？',
    '- 模块供电、电流峰值、逻辑电平和连接器形式是否满足目标板卡？',
    '- 是否需要量产替代料、认证、寿命周期和 BOM 风险评估？',
    '',
    '## 下一步建议',
    '- 如果选型符合预期，请接受器件选型并进入硬件资源规划。',
    '- 如果希望换型号、降成本、提高精度或改变接口，请补充调整要求后再次检索。'
  ].join('\n');
}

function componentResourceMacros(component) {
  const macros = {
    driver_led: 'BOARD_LED_GPIO, BOARD_LED_PWM_CHANNEL, BOARD_LED_PWM_FREQUENCY_HZ',
    driver_button: 'BOARD_BUTTON_GPIO, BOARD_BUTTON_ACTIVE_LEVEL',
    driver_servo: 'BOARD_SERVO_GPIO_0/1, BOARD_SERVO_PWM_CHANNEL_0/1, BOARD_SERVO_PWM_FREQUENCY_HZ',
    driver_oled: 'BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO, BOARD_OLED_I2C_ADDRESS',
    driver_mpu6050: 'BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO, MPU6050 I2C 地址待确认',
    driver_tof2000c_vl53l0x: 'BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO, DRIVER_TOF2000C_VL53L0X_I2C_ADDR_DEFAULT',
    driver_tw_tts: 'BOARD_UART_TX_GPIO, BOARD_UART_RX_GPIO, BOARD_UART_BAUD_RATE',
    service_wifi: 'ESP32 内置 WiFi，无外部 GPIO',
    driver_motor: 'BOARD_MOTOR_* GPIO/PWM 宏',
    driver_encoder: 'BOARD_ENCODER_* GPIO 宏',
    driver_tb6612: 'BOARD_MOTOR_* GPIO/PWM 宏'
  };
  return macros[component] || '待确认';
}

function componentResourcePins(component, sdkSummary) {
  const resource = componentResourceText(component, sdkSummary);
  if (component === 'driver_oled') return `${resource}; I2C address 0x3C`;
  if (component === 'driver_tof2000c_vl53l0x') return `${resource}; I2C address 0x29`;
  if (component === 'driver_mpu6050') return `${resource}; I2C address 0x68/0x69 待确认`;
  return resource;
}

export function fallbackHardwareResourcePlanning(requirement, analysis, componentSelection, resourceNotes, sdkSummary) {
  const { selected } = hardwareAnalysisRows(`${requirement}\n${analysis}\n${componentSelection}`, sdkSummary);
  const rows = selected.map((component) => ({
    component,
    feature: component.replace(/^driver_/, '').replace(/^service_/, ''),
    candidate: componentCandidate(component),
    iface: componentInterface(component),
    macros: componentResourceMacros(component),
    pins: componentResourcePins(component, sdkSummary),
    sharing: ['driver_oled', 'driver_mpu6050', 'driver_tof2000c_vl53l0x'].includes(component) ? '共享' : '独占',
    status: ['driver_oled', 'driver_mpu6050', 'driver_tof2000c_vl53l0x'].includes(component) ? '共享可用' : '已分配'
  }));
  const i2cRows = rows.filter((row) => row.iface === 'I2C');
  const pwmRows = rows.filter((row) => /PWM/.test(row.iface));
  const uartRows = rows.filter((row) => row.iface === 'UART');
  const gpioRows = rows.filter((row) => /GPIO/.test(row.iface) && !/PWM/.test(row.iface));

  return [
    '# 硬件资源规划',
    '',
    '## 硬件资源规划摘要',
    '- 目标板卡：优先使用当前选择或默认 ESP32-S3 板级定义。',
    `- 规划结论：为 ${rows.map((row) => row.candidate).join('、') || '待确认模块'} 生成接口和引脚分配。`,
    resourceNotes ? `- 用户调整要求：${resourceNotes}` : '- 用户调整要求：暂无。',
    '- 主要风险：共享 I2C 地址、PWM/LEDC 通道、UART 端口、供电电流和敏感 GPIO 仍需实物确认。',
    '',
    '## 接口和引脚分配',
    '| 功能/器件 | SDK 组件 | 接口 | 板级资源/宏 | GPIO/通道/地址 | 共享/独占 | 状态 | 备注 |',
    '|---|---|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.candidate} | ${row.component} | ${row.iface} | ${row.macros} | ${row.pins} | ${row.sharing} | ${row.status} | 优先复用 boards/ 中已定义资源 |`),
    '',
    '## 共享总线规划',
    '| 总线 | 引脚/端口 | 挂载器件 | 地址/参数 | 冲突检查 | 备注 |',
    '|---|---|---|---|---|---|',
    i2cRows.length
      ? `| I2C | ${componentResourceText('driver_oled', sdkSummary)} | ${i2cRows.map((row) => row.candidate).join('、')} | ${i2cRows.map((row) => row.pins.split(';').slice(-1)[0].trim()).join('；')} | 待确认地址是否重复 | I2C 可共享 SDA/SCL，但地址必须唯一 |`
      : '| I2C | 未使用 | 无 | 无 | 通过 | 当前选型未命中 I2C 器件 |',
    uartRows.length
      ? `| UART | ${componentResourceText('driver_tw_tts', sdkSummary)} | ${uartRows.map((row) => row.candidate).join('、')} | BOARD_UART_BAUD_RATE | 待确认端口独占 | 避免占用下载/日志串口 |`
      : '| UART | 未使用 | 无 | 无 | 通过 | 当前选型未命中 UART 器件 |',
    '',
    '## 独占资源规划',
    '| 资源类型 | 资源 | 使用者 | 参数 | 状态 | 备注 |',
    '|---|---|---|---|---|---|',
    ...pwmRows.map((row) => `| PWM/LEDC | ${row.macros} | ${row.candidate} | ${row.pins} | 已分配 | 检查 channel/timer 不与其他 PWM 输出冲突 |`),
    ...gpioRows.map((row) => `| GPIO | ${row.macros} | ${row.candidate} | ${row.pins} | 已分配 | 检查输入/输出方向和上下拉 |`),
    pwmRows.length === 0 && gpioRows.length === 0 ? '| GPIO/PWM | 无 | 无 | 无 | 待确认 | 当前选型未命中独占 GPIO/PWM 资源 |' : '',
    '',
    '## 冲突检查',
    '| 检查项 | 结果 | 说明 | 处理建议 |',
    '|---|---|---|---|',
    '| GPIO 独占冲突 | 待确认 | 已按 board 宏分配，仍需检查是否与下载、USB、Flash/PSRAM 或保留引脚冲突 | 实物接线前复核 board.h 和芯片 datasheet |',
    '| I2C 地址冲突 | 待确认 | I2C 总线可共享，但 OLED/ToF/IMU 地址必须唯一 | 上电后先运行 probe/demo 检查地址 |',
    '| PWM/LEDC 通道冲突 | 待确认 | 舵机、电机、LED 可能使用不同 channel/timer | 以 boards/ 宏为准，避免手写新 channel |',
    '| UART 端口冲突 | 待确认 | TTS/外设 UART 不应占用下载和日志串口 | 确认 BOARD_UART_PORT、TX、RX 和波特率 |',
    '| 供电电流 | 待确认 | 舵机、电机、显示和语音模块峰值电流可能超过板载 3.3V | 执行器优先独立供电，共地连接 |',
    '',
    '## 接线和初始化建议',
    '| 顺序 | 模块 | 接线/初始化动作 | 依赖 | 验证方式 |',
    '|---|---|---|---|---|',
    ...rows.map((row, index) => `| ${index + 1} | ${row.candidate} | 按 ${row.macros} 接线并调用 ${row.component} 初始化 | ${row.iface} 资源可用 | 运行对应 example 或 probe/read/set 测试 |`),
    '',
    '## 待确认问题',
    '- 目标板卡是否就是当前选择的 board，是否存在外接扩展板或转接线？',
    '- I2C 器件地址是否可修改，是否与 OLED/ToF/IMU 默认地址冲突？',
    '- 舵机、电机、TTS/功放等模块是否有独立供电和共地方案？',
    '- 是否需要避开启动绑带、USB、Flash/PSRAM 或输入仅用 GPIO？',
    '',
    '## 下一步建议',
    '- 如果资源分配符合预期，请接受硬件资源规划并进入硬件搭建。',
    '- 如果需要换接口、换引脚、增加模块或规避冲突，请补充调整要求后重新规划。'
  ].join('\n');
}

function hardwareAnalysisRows(requirement, sdkSummary) {
  const capabilities = detectCapabilities(requirement, sdkSummary);
  const selected = capabilities.components.length > 0 ? capabilities.components : ['service_device'];
  const knownComponents = componentNames(sdkSummary);
  return {
    capabilities,
    selected,
    hardwareRows: selected.map((component) => [
      component.replace(/^driver_/, '').replace(/^service_/, ''),
      componentFunctionType(component),
      capabilities.notes.find((note) => note.toLowerCase().includes(component.replace(/^driver_|^service_/, '').split('_')[0])) || '根据需求实现对应硬件能力',
      component,
      componentResourceText(component, sdkSummary),
      knownComponents.includes(component) ? '已支持' : '缺口',
      component === 'service_device' ? '基础设备初始化能力' : '需结合需求细化状态机和验收方式'
    ]),
    mappingRows: selected.map((component) => [
      component,
      componentPrimaryApi(component, sdkSummary),
      (sdkSummary.examples || []).find((example) => example.includes(component.replace(/^driver_|^service_/, '').split('_')[0])) || '先查找相近 example',
      knownComponents.includes(component) ? '已支持' : '缺口',
      component === 'service_device' ? '需补充具体硬件需求' : '需确认板级资源和组合冲突'
    ])
  };
}

export function fallbackRequirementAnalysis(requirement, sdkSummary) {
  const { selected, hardwareRows, mappingRows } = hardwareAnalysisRows(requirement, sdkSummary);
  return [
    '# 功能分析',
    '',
    '## 需求摘要',
    requirement ? `- 产品目标：${requirement.split(/\r?\n/).find((line) => line.trim()) || requirement}` : '- 产品目标：尚未填写具体产品需求。',
    '- 产品形态：待确认，当前按开发板/SDK 应用验证处理。',
    `- 初步命中的 SDK 能力：${selected.join(', ')}`,
    '',
    '## 硬件功能清单',
    '| 功能 | 类型 | 用户行为/系统行为 | SDK 组件 | 板级资源 | 状态 | 备注 |',
    '|---|---|---|---|---|---|---|',
    ...hardwareRows.map((row) => `| ${row.join(' | ')} |`),
    '',
    '## SDK 能力映射',
    '| 需求项 | 推荐组件/API | 示例或参考 | 支持状态 | 风险 |',
    '|---|---|---|---|---|',
    ...mappingRows.map((row) => `| ${row.join(' | ')} |`),
    '',
    '## 待确认问题',
    '- 产品形态、供电方式、目标板卡和是否需要量产设计。',
    '- 每个传感器/执行器的具体型号、接口、电压和安装方式。',
    '- 是否需要上位机、网络协议、自动化测试或 physical agent 闭环。',
    '',
    '## 缺口和风险',
    '- 未命中的硬件模块需要补充 SDK driver/service 或调整需求。',
    '- 板级 GPIO、I2C、UART、PWM 等资源需要在资源规划阶段进一步检查冲突。',
    '',
    '## 建议的下一步',
    '- 如果以上功能拆解符合预期，请接受分析并进入器件选型。',
    '- 如果有遗漏或理解不准确，请完善需求后重新进行功能分析。'
  ].join('\n');
}

export function fallbackHardwareBuild(requirement, analysis, componentSelection, resourcePlan, sdkSummary) {
  const { selected } = hardwareAnalysisRows(`${requirement}\n${analysis}\n${componentSelection}\n${resourcePlan}`, sdkSummary);
  const rows = selected.map((component) => ({
    component,
    candidate: componentCandidate(component),
    iface: componentInterface(component),
    macros: componentResourceMacros(component),
    pins: componentResourcePins(component, sdkSummary),
    power: component === 'service_wifi' ? '目标板内置' : '按模块规格，优先 3.3V 逻辑',
    sharing: ['driver_oled', 'driver_mpu6050', 'driver_tof2000c_vl53l0x'].includes(component) ? '共享可用' : '可接线'
  }));
  const edgeRows = rows
    .filter((row) => row.component !== 'service_wifi')
    .map((row, index) => `  MCU -- "${row.iface} / ${row.pins.replaceAll('"', "'")}" --> M${index}[${row.candidate}]`);
  const diagram = [
    'flowchart LR',
    '  MCU[ESP32 / 目标板]',
    ...edgeRows,
    rows.some((row) => row.component !== 'service_wifi') ? '  PWR[电源] --> MCU' : '',
    ...rows
      .filter((row) => row.component !== 'service_wifi')
      .map((row, index) => `  PWR --> M${index}`),
    rows.some((row) => row.component !== 'service_wifi') ? '  GND[共地] --- MCU' : '',
    ...rows
      .filter((row) => row.component !== 'service_wifi')
      .map((row, index) => `  GND --- M${index}`)
  ].filter(Boolean).join('\n');
  const i2cRows = rows.filter((row) => row.iface === 'I2C');
  const uartRows = rows.filter((row) => row.iface === 'UART');

  return [
    '# 硬件搭建',
    '',
    '## 硬件搭建摘要',
    `- 根据已接受的硬件资源规划，为 ${rows.map((row) => row.candidate).join('、') || selected.join('、')} 生成模块连接框图。`,
    '- 接线必须优先采用资源规划中的接口、GPIO、通道和地址；下表只做搭建表达，不重新分配资源。',
    '',
    '## 模块连接框图',
    '```mermaid',
    diagram,
    '```',
    '',
    '## 模块接线清单',
    '| 模块 | 模块接口/管脚 | 目标板接口/宏 | GPIO/通道/地址 | 供电 | 共地 | 状态 | 备注 |',
    '|---|---|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.candidate} | ${row.iface} | ${row.macros} | ${row.pins} | ${row.power} | 必须共地 | ${row.sharing} | 按资源规划接线，模块丝印需实物复核 |`),
    '',
    '## 接口分组',
    '| 接口/总线 | 连接模块 | 引脚/端口 | 地址/参数 | 注意事项 |',
    '|---|---|---|---|---|',
    i2cRows.length ? `| I2C | ${i2cRows.map((row) => row.candidate).join('、')} | ${componentResourceText('driver_oled', sdkSummary)} | ${i2cRows.map((row) => row.pins.split(';').slice(-1)[0].trim()).join('；')} | 地址必须唯一，SDA/SCL 共享 |` : '| I2C | 无 | 无 | 无 | 当前搭建未使用 I2C |',
    uartRows.length ? `| UART | ${uartRows.map((row) => row.candidate).join('、')} | ${componentResourceText('driver_tw_tts', sdkSummary)} | BOARD_UART_BAUD_RATE | 避免占用下载/日志串口 |` : '| UART | 无 | 无 | 无 | 当前搭建未使用 UART |',
    `| GPIO/PWM | ${rows.filter((row) => /GPIO|PWM/.test(row.iface)).map((row) => row.candidate).join('、') || '无'} | 按资源规划宏 | 按资源规划 | 启动绑带和保留引脚需复核 |`,
    '',
    '## 电源和共地',
    '| 模块 | 供电电压 | 峰值电流/功耗 | 供电来源 | 共地要求 | 风险 |',
    '|---|---|---|---|---|---|',
    ...rows.map((row) => `| ${row.candidate} | 待按模块确认 | 待确认 | ${row.power} | 与目标板 GND 共地 | 执行器/语音/电机类模块不建议直接吃板载 3.3V 大电流 |`),
    '',
    '## 上电前检查',
    '- 万用表确认 VCC/GND 未短路，模块供电电压与逻辑电平匹配。',
    '- 复核 SDA/SCL、TX/RX、PWM/GPIO 方向，尤其 UART 需要交叉连接 TX/RX。',
    '- 执行器、电机、舵机和功放模块如需独立供电，先共地再接信号线。',
    '',
    '## 单模块验证顺序',
    ...rows.map((row, index) => `- ${index + 1}. ${row.candidate}：按 ${row.iface} 接线后，先运行对应 demo 或最小读写动作。`),
    '',
    '## 待确认问题',
    '- 模块实物丝印、供电电压、峰值电流和接口方向。',
    '- I2C 地址、UART 波特率、PWM 频率/占空比范围是否与资源规划一致。',
    '- 外设安装位置、线长、连接器规格和抗干扰需求。'
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
    '#include <stdio.h>',
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
  if (capabilities.components.includes('driver_servo')) setup.push('    driver_servo_init();', '    driver_servo_set_angle(DRIVER_SERVO_0, 90);');
  if (capabilities.components.includes('driver_mpu6050')) setup.push('    driver_mpu6050_init_default();');
  if (capabilities.components.includes('driver_tof2000c_vl53l0x')) setup.push('    driver_tof2000c_vl53l0x_init_default();');

  const loop = [];
  if (capabilities.components.includes('driver_servo')) {
    loop.push(
      '        static uint16_t servo_angle = 60;',
      '        static int servo_step = 20;',
      '        driver_servo_set_angle(DRIVER_SERVO_0, servo_angle);',
      '        servo_angle = (uint16_t)((int)servo_angle + servo_step);',
      '        if (servo_angle >= 120 || servo_angle <= 60) {',
      '            servo_step = -servo_step;',
      '        }'
    );
  }
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
    loop.push(...[
      '        driver_tof2000c_vl53l0x_result_t range;',
      '        if (driver_tof2000c_vl53l0x_read_single(&range) == ESP_OK) {',
      '            ESP_LOGI(TAG, "distance=%u mm", range.distance_mm);',
      capabilities.components.includes('driver_oled') ? '            char line[32];' : '',
      capabilities.components.includes('driver_oled') ? '            snprintf(line, sizeof(line), "Dist: %u mm", range.distance_mm);' : '',
      capabilities.components.includes('driver_oled') ? '            driver_oled_clear();' : '',
      capabilities.components.includes('driver_oled') ? '            driver_oled_draw_string(0, 0, "Moce Agent");' : '',
      capabilities.components.includes('driver_oled') ? '            driver_oled_draw_string(0, 16, line);' : '',
      capabilities.components.includes('driver_oled') ? '            driver_oled_flush();' : '',
      '        }'
    ].filter(Boolean));
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

export function validateGeneratedFiles(files) {
  const errors = [];
  const warnings = [];
  const main = (files || []).find((file) => /\/main\/main\.c$/.test(file.path));
  const rootCmake = (files || []).find((file) => /\/CMakeLists\.txt$/.test(file.path) && !/\/main\/CMakeLists\.txt$/.test(file.path));
  const mainCmake = (files || []).find((file) => /\/main\/CMakeLists\.txt$/.test(file.path));
  const content = main?.content || '';

  if (!main) {
    errors.push('missing project/<name>/main/main.c');
  }

  if (!rootCmake || !/EXTRA_COMPONENT_DIRS/.test(rootCmake.content || '') || !/project\.cmake/.test(rootCmake.content || '')) {
    errors.push('root CMakeLists.txt must define EXTRA_COMPONENT_DIRS and include ESP-IDF project.cmake');
  }

  if (!mainCmake || !/idf_component_register\s*\(/.test(mainCmake.content || '')) {
    errors.push('main/CMakeLists.txt must use idf_component_register');
  }

  if (/register_component\s*\(|COMPONENT_SRCS|COMPONENT_REQUIRES|COMPONENT_ADD_INCLUDEDIRS/.test(mainCmake?.content || '')) {
    errors.push('old ESP-IDF component CMake style is not allowed; use idf_component_register');
  }

  if (/esp_err_t\s+\w+\s*=\s*service_device_init\s*\(/.test(content)) {
    errors.push('service_device_init returns void, not esp_err_t');
  }

  if (/driver_oled_init_profile\s*\(\s*\)/.test(content)) {
    errors.push('driver_oled_init_profile requires a driver_oled_profile_t argument');
  }

  if (/\bbutton_event_t\b/.test(content) || /\bBUTTON_(?:PRESSED|RELEASED|LONG_PRESS|SHORT_PRESS)\b/.test(content)) {
    errors.push('button driver uses driver_button_event_t and DRIVER_BUTTON_EVENT_* enum names');
  }

  if (/\bDRIVER_BUTTON_EVENT_(?:PRESSED|RELEASED)\b/.test(content)) {
    errors.push('button enum values are DRIVER_BUTTON_EVENT_PRESS and DRIVER_BUTTON_EVENT_RELEASE');
  }

  if (/\bDRIVER_OLED_PROFILE_128X64\b/.test(content)) {
    errors.push('OLED profile enum must be DRIVER_OLED_PROFILE_096_SSD1306 or DRIVER_OLED_PROFILE_13_SH1106');
  }

  if (/\bDRIVER_SERVO_ID_/.test(content)) {
    errors.push('servo enum values are DRIVER_SERVO_0 and DRIVER_SERVO_1');
  }

  if (/driver_tof2000c_vl53l0x_read_single\s*\(\s*&\s*(?:distance|dist|range_mm)\b/.test(content)) {
    errors.push('driver_tof2000c_vl53l0x_read_single expects driver_tof2000c_vl53l0x_result_t *');
  }

  if (/\btof_result\.status\b|\brange\.status\b/.test(content)) {
    errors.push('ToF result field is range_status, not status');
  }

  for (const file of files || []) {
    if (!String(file.path || '').startsWith('project/')) {
      errors.push(`refusing non-project output path: ${file.path}`);
    }
  }

  if (content.includes('service_wifi') && !content.includes('service_wifi_init_sta')) {
    warnings.push('WiFi appears in code; verify credentials/config flow before flashing');
  }

  return { ok: errors.length === 0, errors, warnings };
}
