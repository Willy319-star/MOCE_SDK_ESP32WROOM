idf_component_register(
    SRCS "bsp_uart.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_uart
)
