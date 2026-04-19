idf_component_register(
    SRCS "bsp_button.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board
    PRIV_REQUIRES esp_driver_gpio esp_timer
)