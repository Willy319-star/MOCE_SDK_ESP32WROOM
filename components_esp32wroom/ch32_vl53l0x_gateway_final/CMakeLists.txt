idf_component_register(
    SRCS "ch32_vl53l0x_gateway_final.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_twai esp_driver_gpio esp_timer bsp_board
)
