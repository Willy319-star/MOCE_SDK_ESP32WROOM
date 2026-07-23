cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_board"
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp/bsp_i2c"
    "${CMAKE_CURRENT_LIST_DIR}/../../components_esp32wroom/vl53l0x_direct_i2c_final"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tof_direct_test0_final)