add_library(sdvgw-kuksa STATIC
    KuksaClient.cpp
)
target_include_directories(sdvgw-kuksa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdvgw-kuksa PUBLIC
    kuksa-proto
    sdvgw-decoder
    spdlog::spdlog
    gRPC::grpc++
)
