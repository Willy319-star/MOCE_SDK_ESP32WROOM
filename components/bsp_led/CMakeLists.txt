idf_component_register(
    SRCS "bsp_led.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_ledc esp_driver_gpio
)