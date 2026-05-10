idf_component_register(
    SRCS "bsp_pwm.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_ledc
)
