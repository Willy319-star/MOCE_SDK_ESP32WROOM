idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp_board vc02_direct_uart_final
)
