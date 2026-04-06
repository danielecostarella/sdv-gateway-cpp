add_subdirectory(can)
add_subdirectory(decoder)
add_subdirectory(vss)
add_subdirectory(kuksa)

add_executable(sdv-gateway main.cpp)
target_link_libraries(sdv-gateway PRIVATE
    sdvgw-can
    sdvgw-decoder
    sdvgw-vss
    sdvgw-kuksa
    spdlog::spdlog
    Threads::Threads
)
