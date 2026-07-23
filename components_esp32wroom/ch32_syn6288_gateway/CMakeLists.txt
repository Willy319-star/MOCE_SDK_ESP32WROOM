idf_component_register(
    SRCS
        "ch32_syn6288_gateway.c"
        "ch32_syn6288_protocol.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_driver_twai freertos
)

