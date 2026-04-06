add_subdirectory(can)
add_subdirectory(decoder)
add_subdirectory(vss)
add_subdirectory(kuksa)

add_executable(sdv-gateway main.cpp)
# src/ itself needed for #include "SpscQueue.hpp"
target_include_directories(sdv-gateway PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdv-gateway PRIVATE
    sdvgw-can
    sdvgw-decoder
    sdvgw-vss
    sdvgw-kuksa
    spdlog::spdlog
    Threads::Threads
)
