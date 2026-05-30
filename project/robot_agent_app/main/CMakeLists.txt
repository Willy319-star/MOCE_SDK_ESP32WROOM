idf_component_register(
    SRCS "main.c"
    REQUIRES
        driver_button
        driver_encoder
        driver_motor
        driver_mpu6050
        driver_oled
        driver_tb6612
        driver_tof2000c_vl53l0x
        driver_tw_tts
        service_device
        service_pid
        bsp_board
)