idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp_board esp_driver_twai ch32_motor_gateway_final
)
