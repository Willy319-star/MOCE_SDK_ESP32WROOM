set(MOCE_BOARD "my_board_esp32" CACHE STRING "Moce board name")
set(MOCE_BOARD_DIR "$ENV{MY_SDK_ROOT}/boards/${MOCE_BOARD}")

if(NOT EXISTS "${MOCE_BOARD_DIR}/board.h")
    message(FATAL_ERROR "Board '${MOCE_BOARD}' not found: ${MOCE_BOARD_DIR}/board.h")
endif()

idf_component_register(
    INCLUDE_DIRS
        "${MOCE_BOARD_DIR}"
        "include"
    REQUIRES esp_driver_ledc
)
