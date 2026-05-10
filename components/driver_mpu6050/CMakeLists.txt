idf_component_register(
    SRCS "driver_mpu6050.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c
)
