idf_component_register(
    SRCS "main.c"
    REQUIRES driver_oled driver_mpu6050 driver_tof2000c_vl53l0x driver_tw_tts bsp_board
)