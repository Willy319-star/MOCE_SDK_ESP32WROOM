# Moce SDK

面向嵌入式产品开发的SDK，支持基于 ESP32 平台快速构建应用，提供统一的 BSP、驱动、组件、中间件与项目模板。
---

## 仓库结构

```text
.
├── bsp/                     # 板级与 GPIO/I2C/PWM 等硬件资源封装
├── components/              # SDK 公共组件
│   ├── driver_*             # LED/Button/Servo/OLED 等外设驱动
│   └── service_*            # 面向应用的能力服务
├── boards/                  # 板级支持包
├── examples/                # 官方示例工程
├── project/                 # 用户应用工程
├── third_party/             # 第三方依赖
│   └── esp-idf/             # ESP-IDF submodule
├── tools/                   # 构建、烧写、辅助脚本
├── docs/                    # 文档
├── .gitmodules
├── .gitignore
└── README.md
```
## 使用方法

### 克隆仓库
```
git clone --recurse-submodules git@github.com:wuyang9266/moce_sdk.git
cd moce_sdk
```
如果已经完成普通 clone，但 submodule 没有拉取，可以执行：
```
git submodule update --init --recursive
```

### 更新submodule
```
git submodule update --init --recursive
```
如果 submodule 地址发生变化，可以执行：
```
git submodule sync --recursive
git submodule update --init --recursive
```

### 初始化开发环境
```
./env/install.sh
source ./env/export.sh #每次都要执行export
```

## 用户工作流

1. 完成开发环境的配置。
2. 在prompt/中新建文本文档，用自然语言描述待开发功能，可以参照prompt目录下的其他模板。其中prompt0.md为开发约束，请勿改动。
3. 将prompt0.md以及用户的功能描述prompt提交给大模型（建议用codex）
4. 生成的工程位于/project目录下；
5. 编译与烧录
    ```
    ./tools/build.sh /project/<your_project_name>
    ./tools/flash.sh /project/<your_project_name>
    ```
## 常见问题

1. 串口没有权限

    将当前用户加入 dialout 用户组：
    ```
    sudo usermod -aG dialout $USER
    newgrp dialout
    ```
