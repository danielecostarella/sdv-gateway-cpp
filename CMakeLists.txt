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
# Proto: fetch val.proto + types.proto from Eclipse Kuksa if not present.
# val.proto imports types.proto — both files must be present for protoc.
# ---------------------------------------------------------------------------
set(PROTO_SRC_DIR  ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR  ${CMAKE_BINARY_DIR}/generated)
set(KUKSA_PROTO_BASE_URL
    "https://raw.githubusercontent.com/eclipse-kuksa/kuksa-databroker/main/proto")

set(KUKSA_PROTOS
    kuksa/val/v2/types.proto
    kuksa/val/v2/val.proto
)

file(MAKE_DIRECTORY ${PROTO_SRC_DIR}/kuksa/val/v2)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR}/kuksa/val/v2)

foreach(_proto ${KUKSA_PROTOS})
    set(_proto_path "${PROTO_SRC_DIR}/${_proto}")
    if(NOT EXISTS "${_proto_path}")
        message(STATUS "Fetching ${_proto} from eclipse-kuksa/kuksa-databroker...")
        file(DOWNLOAD
            "${KUKSA_PROTO_BASE_URL}/${_proto}"
            "${_proto_path}"
            STATUS _dl_status
            TLS_VERIFY ON
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            message(FATAL_ERROR
                "Failed to download ${_proto} (error ${_dl_code}).\n"
                "Run scripts/fetch-proto.sh manually and re-run cmake.")
        endif()
    endif()
endforeach()

set(KUKSA_VAL_PROTO   ${PROTO_SRC_DIR}/kuksa/val/v2/val.proto)
set(KUKSA_TYPES_PROTO ${PROTO_SRC_DIR}/kuksa/val/v2/types.proto)

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED
    HINTS /usr/bin /usr/local/bin)

# types.proto has no service — generate only C++ messages (no gRPC stubs)
add_custom_command(
    OUTPUT
        ${PROTO_GEN_DIR}/kuksa/val/v2/types.pb.cc
        ${PROTO_GEN_DIR}/kuksa/val/v2/types.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${PROTO_SRC_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        ${KUKSA_TYPES_PROTO}
    DEPENDS ${KUKSA_TYPES_PROTO}
    COMMENT "Generating protobuf sources for kuksa.val.v2 types"
    VERBATIM
)

# val.proto defines the VAL service — generate both C++ messages and gRPC stubs
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
        ${KUKSA_VAL_PROTO}
    DEPENDS ${KUKSA_VAL_PROTO} ${KUKSA_TYPES_PROTO}
        ${PROTO_GEN_DIR}/kuksa/val/v2/types.pb.h
    COMMENT "Generating protobuf / gRPC sources for kuksa.val.v2"
    VERBATIM
)

add_library(kuksa-proto STATIC
    ${PROTO_GEN_DIR}/kuksa/val/v2/types.pb.cc
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
