# Datasheet Key Information Extraction

目标：从 datasheet 中提取支撑 AI 原理图设计和基础配置代码生成的关键信息。

关键信息：

- Part number、厂商、封装
- 供电范围和典型电流
- 绝对最大额定值
- 接口类型和默认地址
- 引脚定义
- 上电时序
- 参考电路
- 寄存器初始化流程
- PCB layout 注意事项
- 替代料和生命周期风险

首版实现建议：

- 先作为 workflow prompt 和数据结构预留。
- 后续接入 PDF 解析、表格抽取和人工确认队列。
