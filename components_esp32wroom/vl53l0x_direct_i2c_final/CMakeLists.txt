idf_component_register(
    SRCS "vl53l0x_direct_i2c_final.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c esp_driver_gpio
)
