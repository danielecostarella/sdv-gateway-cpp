add_library(sdvgw-decoder STATIC
    MappingConfig.cpp
    SignalDecoder.cpp
)
target_include_directories(sdvgw-decoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-decoder PUBLIC
    sdvgw-can
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
