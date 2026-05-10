平台架构
1. 硬件层
这一层是物理载体，包括：主控MCU ，电源管理，传感器接口，调试与烧写接口，电机驱动接口等。设计要点是接口按照功能进行设计，尽可能不留孤立的gpio。
2. BSP层
这层负责对底层外设进行封装。包括：GPIO, UART, SPI, IIC, ADC, PWM, Encoder, CAN, RS485, FLASH, TIMER, DMA, Interrupt wrapper等。避免用户直接接触寄存器细节。
3. 设备层
这一层的职责是把底层资源组合成用户真正想用的对象。包括：
  motor_service
  wifi_service
  ble_service
  sensor_service
  display_service
  storage_service
  network_service
  ota_service
  diagnostics_service
4. 框架层
这一层决定了 AI 生成出来的代码的组织方式。主要包括任务调度，状态机，事件等。
5. App层
这一层是用户Vibe coding的主要区域，主要包括用户的业务逻辑，自定义的控制策略等。核心是要与底层解耦。
6. 工具链
这一层提供开发工具，包括：项目初始化，工程配置，项目编译，打包，烧写，日志，诊断，AI prompt等。
平台的形态
1. 开发板
提供硬件能力和统一接口。
2. SDK
提供驱动、能力服务、框架、示例。
platform-sdk/
├─ boards/
│  ├─ motor_devkit_v1/
│  │  ├─ board.yaml
│  │  ├─ pinmap.h
│  │  └─ board_init.c
├─ bsp/               # 直接面向硬件资源
│  ├─ gpio/
│  ├─ uart/
│  ├─ spi/
│  ├─ i2c/
│  ├─ pwm/
│  ├─ adc/
│  ├─ encoder/
├─ drivers/
│  ├─ motor_driver/
│  ├─ sensor_driver/
│  ├─ display_driver/
├─ services/        # 面向功能的接口
│  ├─ motor_service/
│  ├─ wifi_service/
│  ├─ ble_service/
│  ├─ mqtt_service/
│  ├─ storage_service/
│  ├─ diagnostics_service/
│  ├─ ota_service/
├─ framework/       #程序组织方式
│  ├─ event_bus/
│  ├─ app_lifecycle/
│  ├─ config/
│  ├─ logging/
│  ├─ error/
│  ├─ state_machine/
│  ├─ device_model/
├─ tools/
│  ├─ cli/
│  ├─ flasher/
│  ├─ doctor/
│  ├─ packager/
├─ examples/
├─ docs/
└─ templates/
3. 工具链
提供项目创建、构建、烧写、诊断、打包。
用户工作流
1. 安装开发平台
2. 运行环境检查
  - 是否识别到板卡
  - 工具链是否安装
3. 创建项目（使用工具链）
  
my-app/
├─ app/
│  ├─ src/
│  ├─ include/
│  ├─ modules/
│  ├─ flows/
├─ config/
│  ├─ app.yaml
│  ├─ rules.yaml
├─ tests/
├─ scripts/
├─ project.yaml
└─ sdk.lock
4. 把prompt提给AI，由AI在app/ 中生成代码
5. 编译与烧写。

Promt模板
开发约束类
防止AI幻觉，调用不存在的API或改动不该改的SDK文件