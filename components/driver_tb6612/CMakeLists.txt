idf_component_register(
    SRCS "driver_tb6612.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_pwm bsp_gpio
)
