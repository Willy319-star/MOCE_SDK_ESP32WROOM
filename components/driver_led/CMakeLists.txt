idf_component_register(
    SRCS "driver_led.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_pwm
)
