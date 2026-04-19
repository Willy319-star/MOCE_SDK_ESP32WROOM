idf_component_register(
    SRCS "service_device.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_led bsp_button
)