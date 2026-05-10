cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../bsp"
    "${CMAKE_CURRENT_LIST_DIR}/../../components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wifi_tcp_udp_demo)
