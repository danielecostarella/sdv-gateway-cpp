add_library(sdvgw-kuksa STATIC placeholder.cpp)
target_include_directories(sdvgw-kuksa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-kuksa PUBLIC
    kuksa-proto
    spdlog::spdlog
)
