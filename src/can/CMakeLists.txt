if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "sdv-gateway requires Linux (SocketCAN is Linux-only)")
endif()

add_library(sdvgw-can STATIC
    SocketCANReader.cpp
    SimulatedCANReader.cpp
)
target_include_directories(sdvgw-can PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-can PUBLIC spdlog::spdlog)
