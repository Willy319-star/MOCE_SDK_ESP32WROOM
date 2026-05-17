请基于当前 SDK 生成一个多传感器融合的桌面伴侣应用工程。

【工程名】 project/510demo/

【功能概述】上电后 OLED 显示笑脸，TW-TTS 播报"你好，莫测上线了"。之后同时监控 MPU6050 姿态和 VL53L0X 距离——任一触发条件满足就变哭脸 + 显示 WARNING + 语音报警，条件解除后恢复笑脸。

【启动行为】

    初始化 OLED
    初始化 TW-TTS set_volume(1)
    在 OLED 上画笑脸，不需要显示任何距离数值
    TW-TTS 播报"你好，莫测上线了"
    说完后初始化 MPU6050和 VL53L0X

【MPU6050 倾斜检测】

    触发条件：|倾斜角度| > 30 度

    报警语音：仅进入 ALERT 时播报一次"我要翻了"

【VL53L0X 距离检测】

    该传感器有约 30mm 的物理光程偏移（VCSEL → 封装玻璃 → 接收器的内反射）
    校准距离 = 原始读数 - 30mm。若原始读数 ≤ 30mm，视为无目标（安全）
    触发条件：校准距离 > 0 且 < 30mm（即原始读数在 31~59mm 之间）
    报警语音：仅进入 ALERT 时播报一次"我要撞了"

【状态机逻辑】

    两个传感器各自独立判断是否触发
    只要任一传感器触发 → 进入 ALERT 态
    所有传感器都未触发 → 进入 SAFE 态
    仅在 SAFE ↔ ALERT 切换时更新 OLED 画面和播报语音（避免重复刷新）
    ALERT 态：哭脸（嘴巴是倒弧线，即上半圆）+ 底部显示"WARNING"文字
    SAFE 态：笑脸（嘴巴是正弧线，即下半圆）
    不显示任何距离/角度数值在 OLED 上

【实现约束】

    不允许修改 SDK 中任何已有组件（components/、bsp/、boards/ 等）
    只能在 project/510demo/ 下创建和修改文件
    必须生成完整可编译工程
    必须包含项目根 CMakeLists.txt 和 main/CMakeLists.txt
    EXTRA_COMPONENT_DIRS 中必须包含 ../../bsp 和 ../../components（参考现有 project 的 CMakeLists.txt）
    main/CMakeLists.txt 中 REQUIRES: driver_oled driver_mpu6050 driver_tof2000c_vl53l0x driver_tw_tts

【工程文件要求】至少包含：

    project/510demo/CMakeLists.txt
    project/510demo/main/CMakeLists.txt
    project/510demo/main/main.c
