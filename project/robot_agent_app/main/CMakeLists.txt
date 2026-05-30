idf_component_register(
    SRCS "main.c"
    REQUIRES
        service_device
        service_pid
        driver_button
        driver_encoder
        driver_mpu6050
        driver_oled
        driver_motor
        driver_tb6612
        driver_tof2000c_vl53l0x
        driver_tw_tts
        bsp_board
)