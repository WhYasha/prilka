find_package(GTest CONFIG REQUIRED)

# Compile JwtService as a library for testing (excludes main.cpp)
add_library(messenger_lib STATIC
    ../src/services/JwtService.cpp
    ../src/services/MetricsService.cpp
)
target_include_directories(messenger_lib PUBLIC ../src)
target_link_libraries(messenger_lib
    PUBLIC Drogon::Drogon OpenSSL::SSL OpenSSL::Crypto
)

add_executable(messenger_tests test_auth.cpp test_metrics.cpp)
target_link_libraries(messenger_tests
    PRIVATE messenger_lib GTest::gtest GTest::gtest_main
)

add_test(NAME messenger_tests COMMAND messenger_tests)
