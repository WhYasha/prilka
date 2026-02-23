cmake_minimum_required(VERSION 3.25)
project(messenger_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(Drogon  CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# libuuid: Linux has it in libuuid-dev; Windows/macOS: use a compat header
if(UNIX AND NOT APPLE)
    find_library(UUID_LIB uuid REQUIRED)
    set(UUID_LIBS ${UUID_LIB})
else()
    # macOS: uuid is part of the system
    set(UUID_LIBS "")
endif()

# ── Source files ──────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/**/*.cpp"
)
# Exclude test files
list(FILTER SOURCES EXCLUDE REGEX ".*/tests/.*")

add_executable(messenger_api ${SOURCES})

target_include_directories(messenger_api PRIVATE src)

target_link_libraries(messenger_api
    PRIVATE
        Drogon::Drogon
        OpenSSL::SSL
        OpenSSL::Crypto
        ${UUID_LIBS}
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(messenger_api PRIVATE /W4 /WX-)
else()
    target_compile_options(messenger_api PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS messenger_api DESTINATION bin)
