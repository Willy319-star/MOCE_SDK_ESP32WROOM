请基于当前 SDK 生成一个新的按键控制 LED 应用工程。

【工程名】
project/button_brightness_modes/

【功能要求】
实现一个基于单按键的 LED 控制应用，要求同时支持模式切换和亮度档位控制。

【交互要求】
1. 短按一次：
   - 在不同 LED 模式之间切换
2. 长按一次：
   - 调整亮度档位
3. 亮度档位至少支持：
   - 20%
   - 50%
   - 100%

【LED 模式要求】
至少实现以下模式：
1. 常亮
2. 慢闪
3. 快闪
4. 呼吸灯

【行为要求】
- 不同模式下都应尽量应用当前亮度档位
- 呼吸灯模式下可以基于当前亮度档位限制最大亮度
- 上电默认模式为常亮，默认亮度为 50%
- 每次模式变化或亮度变化后都打印日志

【实现要求】
- 基于当前 SDK 已有的 bsp_button 和 bsp_led 接口实现
- 不允许修改 SDK 中任何已有组件
- 只能在 project/button_brightness_modes/ 下创建和修改文件
- 可以参考 examples/ 中已有示例的工程组织方式
- 必须生成完整可编译工程
- 必须包含项目根 CMakeLists.txt 和 main/CMakeLists.txt

【工程组织要求】
建议至少包含：
- project/button_brightness_modes/CMakeLists.txt
- project/button_brightness_modes/main/CMakeLists.txt
- project/button_brightness_modes/main/main.c

如果你认为更清晰，也可以增加：
- project/button_brightness_modes/main/led_app.c
- project/button_brightness_modes/main/led_app.h

【代码要求】
- main.c 尽量简洁
- 模式和亮度档位都使用枚举或清晰的数据结构定义
- 代码应具备良好的扩展性
- 按键逻辑、LED 行为逻辑、应用状态逻辑尽量分离
- 日志输出明确

【输出格式要求】
请直接按“文件路径 + 完整文件内容”的方式输出全部文件。
不要输出解释性大段文字。
不要输出 project/ 以外的文件。