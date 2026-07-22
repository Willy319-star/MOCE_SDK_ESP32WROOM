cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "../../bsp"
    "../../components_esp32wroom/ch32_vl53l0x_gateway_final"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tof_test0_final)
