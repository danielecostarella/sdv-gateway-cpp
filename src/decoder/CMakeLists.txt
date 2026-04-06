add_library(sdvgw-decoder STATIC placeholder.cpp)
target_include_directories(sdvgw-decoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-decoder PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
