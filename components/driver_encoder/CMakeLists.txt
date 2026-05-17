idf_component_register(
    SRCS "driver_encoder.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board esp_driver_pcnt
)
