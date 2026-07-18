if(MOCE_BOARD)
    set(MOCE_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../boards/${MOCE_BOARD}")
else()
    set(MOCE_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../boards/my_board_esp32wroom")
endif()

if(NOT EXISTS "${MOCE_BOARD_DIR}/board.h")
    message(FATAL_ERROR "Board not found: ${MOCE_BOARD_DIR}/board.h")
endif()

idf_component_register(
    INCLUDE_DIRS "${MOCE_BOARD_DIR}" "include"
    REQUIRES esp_hal_i2c esp_hal_ledc
)
