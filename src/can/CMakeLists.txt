# Header-only interface library.
# Implementations (SocketCANReader, SimulatedCANReader) are added in feat(can).
add_library(sdvgw-can INTERFACE)
target_include_directories(sdvgw-can INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
