idf_component_register(
    SRCS "main.c" "led_effects.c"
    INCLUDE_DIRS "."
    REQUIRES service_device driver_led
)
