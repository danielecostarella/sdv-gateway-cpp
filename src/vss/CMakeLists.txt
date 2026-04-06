add_library(sdvgw-vss STATIC placeholder.cpp)
target_include_directories(sdvgw-vss PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-vss PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
