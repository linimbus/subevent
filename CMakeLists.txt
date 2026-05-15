cmake_minimum_required(VERSION 3.14)
project(subevent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Main executable
add_executable(subevent_app main.cc)

target_include_directories(subevent_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(subevent_app PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-Wpedantic>
)

enable_testing()

# GoogleTest dependency
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
)
# For Windows: prevents overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Unit tests
add_executable(subevent_tests test_SingleEvent.cpp test_MultiEvent.cpp)

target_include_directories(subevent_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(subevent_tests PRIVATE GTest::gtest_main)

target_compile_options(subevent_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-Wpedantic>
)

include(GoogleTest)
gtest_discover_tests(subevent_tests)
