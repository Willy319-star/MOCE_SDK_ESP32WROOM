idf_component_register(
    SRCS "service_device.c"
    INCLUDE_DIRS "include"
    REQUIRES driver_led driver_button
)