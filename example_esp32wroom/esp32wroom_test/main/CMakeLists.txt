idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES bsp_board esp_driver_gpio esp_timer spi_flash
    INCLUDE_DIRS "."
)
