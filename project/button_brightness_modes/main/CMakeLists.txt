idf_component_register(
    SRCS "main.c" "led_app.c"
    INCLUDE_DIRS "."
    REQUIRES service_device bsp_led bsp_button
)
