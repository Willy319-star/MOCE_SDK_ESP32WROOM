idf_component_register(
    SRCS "driver_button.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_gpio
    PRIV_REQUIRES esp_timer
)
