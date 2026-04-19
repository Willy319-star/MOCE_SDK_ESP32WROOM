请基于当前 SDK 生成一个新的 LED 应用工程。

【工程名】
project/led_effects_demo/

【功能要求】
实现一个 LED 灯效应用，要求包含以下效果：
1. 常亮
2. 常灭
3. 固定频率闪烁
4. 呼吸灯

【闪烁要求】
- 至少支持 1Hz、2Hz、5Hz 三种闪烁频率
- 闪烁逻辑清晰，便于后续扩展更多频率

【呼吸灯要求】
- 使用平滑亮度变化
- 周期明显、视觉自然
- 尽量复用已有 BSP LED PWM 亮度控制能力

【实现要求】
- 基于当前 SDK 已有的 bsp_led 和相关现有接口实现
- 不允许修改 SDK 中任何已有组件
- 只能在 project/led_effects_demo/ 下创建和修改文件
- 可以参考 examples/ 中已有示例的工程组织方式
- 必须生成完整可编译工程
- 必须包含项目根 CMakeLists.txt 和 main/CMakeLists.txt

【工程组织要求】
建议至少包含：
- project/led_effects_demo/CMakeLists.txt
- project/led_effects_demo/main/CMakeLists.txt
- project/led_effects_demo/main/main.c

如果你认为需要更清晰的模块划分，也可以增加：
- project/led_effects_demo/main/led_effects.c
- project/led_effects_demo/main/led_effects.h

【代码要求】
- main.c 尽量简洁
- 灯效逻辑尽量封装
- 使用枚举定义效果模式
- 使用清晰的状态机或独立函数实现不同效果
- 日志输出简洁明确

【输出格式要求】
请直接按“文件路径 + 完整文件内容”的方式输出全部文件。
不要输出解释性大段文字。
不要输出 project/ 以外的文件。