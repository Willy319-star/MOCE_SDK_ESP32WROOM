idf_component_register(
    SRCS "bsp_i2c.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_i2c
)
