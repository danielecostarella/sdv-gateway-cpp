cmake_minimum_required(VERSION 3.20)
project(sdv-gateway VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# ---------------------------------------------------------------------------
# Proto: fetch from Eclipse Kuksa if not already present
# ---------------------------------------------------------------------------
set(PROTO_SRC_DIR  ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR  ${CMAKE_BINARY_DIR}/generated)
set(KUKSA_PROTO    ${PROTO_SRC_DIR}/kuksa/val/v2/val.proto)

if(NOT EXISTS ${KUKSA_PROTO})
    message(STATUS "Fetching kuksa.val.v2 proto from eclipse-kuksa/kuksa-databroker...")
    file(MAKE_DIRECTORY ${PROTO_SRC_DIR}/kuksa/val/v2)
    file(DOWNLOAD
        "https://raw.githubusercontent.com/eclipse-kuksa/kuksa-databroker/main/proto/kuksa/val/v2/val.proto"
        ${KUKSA_PROTO}
        STATUS _dl_status
        TLS_VERIFY ON
    )
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        message(FATAL_ERROR
            "Failed to download val.proto (error ${_dl_code}).\n"
            "Run scripts/fetch-proto.sh manually and re-run cmake.")
    endif()
    message(STATUS "Proto file downloaded to ${KUKSA_PROTO}")
endif()

file(MAKE_DIRECTORY ${PROTO_GEN_DIR}/kuksa/val/v2)

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED
    HINTS /usr/bin /usr/local/bin)

add_custom_command(
    OUTPUT
        ${PROTO_GEN_DIR}/kuksa/val/v2/val.pb.cc
        ${PROTO_GEN_DIR}/kuksa/val/v2/val.pb.h
        ${PROTO_GEN_DIR}/kuksa/val/v2/val.grpc.pb.cc
        ${PROTO_GEN_DIR}/kuksa/val/v2/val.grpc.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${PROTO_SRC_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        --grpc_out=${PROTO_GEN_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
        ${KUKSA_PROTO}
    DEPENDS ${KUKSA_PROTO}
    COMMENT "Generating protobuf / gRPC sources for kuksa.val.v2"
    VERBATIM
)

add_library(kuksa-proto STATIC
    ${PROTO_GEN_DIR}/kuksa/val/v2/val.pb.cc
    ${PROTO_GEN_DIR}/kuksa/val/v2/val.grpc.pb.cc
)
target_include_directories(kuksa-proto PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(kuksa-proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(tools)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
