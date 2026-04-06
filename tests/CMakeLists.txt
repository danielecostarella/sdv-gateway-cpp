find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(test-decoder
    test_mapping_config.cpp
    test_signal_decoder.cpp
)
target_link_libraries(test-decoder PRIVATE
    sdvgw-decoder
    GTest::GTest
    GTest::Main
)
gtest_discover_tests(test-decoder)
