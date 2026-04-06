add_executable(can-simulator main.cpp)
target_link_libraries(can-simulator PRIVATE Threads::Threads)
