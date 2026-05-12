# Skillset 2: Hardware Module Family Development And Debug

目标：支持硬件模块家族的搭建、嵌入式开发、测试调试和整机联调。

覆盖流程：

- 硬件搭建
- 嵌入式驱动/应用开发
- 单模块测试
- 模块组合测试
- 接入 physical agent 做上位机测试
- 结构设计装配
- 整机联调
- 在 physical 框架下继续 vibe code 功能

首版状态：

- 当前仓库暂无 physical agent 实现。
- `agent/server/lib/physical.js` 提供占位能力描述。
- 后续可接 serial/TCP/UDP 协议，把 build/flash/monitor 日志和测试命令纳入闭环。
