if(CONFIG_IDF_TARGET_ESP32S3)
    set(MOCE_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../boards/my_board_esp32s3")
else()
    set(MOCE_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../boards/my_board_esp32")
endif()

if(NOT EXISTS "${MOCE_BOARD_DIR}/board.h")
    message(FATAL_ERROR "Board not found: ${MOCE_BOARD_DIR}/board.h")
endif()

idf_component_register(
    INCLUDE_DIRS "${MOCE_BOARD_DIR}" "include"
    REQUIRES esp_hal_i2c esp_hal_ledc
)
