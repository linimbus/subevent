cmake_minimum_required(VERSION 3.14)
project(subevent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Building with coverage instrumentation")
        add_compile_options(-g -O0 -fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage)
    else()
        message(WARNING "Coverage support is enabled, but compiler does not support gcov instrumentation")
    endif()
endif()

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

find_program(LCOV_EXEC lcov)
find_program(GENHTML_EXEC genhtml)
find_program(GCOVR_EXEC gcovr)

set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage_report)

if(LCOV_EXEC AND GENHTML_EXEC)
    set(COVERAGE_INFO ${CMAKE_BINARY_DIR}/coverage.info)

    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV_EXEC} --capture --directory ${CMAKE_BINARY_DIR} --output-file ${COVERAGE_INFO}
        COMMAND ${LCOV_EXEC} --remove ${COVERAGE_INFO} '/usr/*' '*/googletest/*' --output-file ${COVERAGE_INFO}
        COMMAND ${GENHTML_EXEC} --demangle-cpp --legend --output-directory ${COVERAGE_DIR} ${COVERAGE_INFO}
        DEPENDS subevent_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating gcov/lcov coverage report in ${COVERAGE_DIR}"
    )
elseif(GCOVR_EXEC)
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${GCOVR_EXEC} -r ${CMAKE_SOURCE_DIR} --html --html-details --output ${COVERAGE_DIR}/index.html
        DEPENDS subevent_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report using gcovr in ${COVERAGE_DIR}"
    )
else()
    message(STATUS "Coverage reporting tools not found; install lcov/genhtml or gcovr to enable the coverage target")
endif()
