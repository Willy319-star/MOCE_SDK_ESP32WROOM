idf_component_register(
    SRCS "driver_motor.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board driver_tb6612 driver_encoder
)
