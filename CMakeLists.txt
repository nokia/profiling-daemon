cmake_minimum_required(VERSION 3.5)

option(CXXOPTS_BUILD_TESTS "" OFF)
option(CXXOPTS_BUILD_EXAMPLES "" OFF)
add_subdirectory(3rd-party/cxxopts)

option(BUILD_TEST "build the unit tests" OFF)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")
set(CMAKE_CXX_STANDARD 14)

find_package(Threads REQUIRED)

add_executable(poor-perf src/main.cpp)
target_link_libraries(poor-perf cxxopts::cxxopts boost_program_options boost_system boost_filesystem Threads::Threads)

if(BUILD_TEST)
    add_subdirectory(3rd-party/Catch2)
    add_executable(poor-perf-tests tests/main.cpp tests/proc_maps_tests.cpp)
    target_link_libraries(poor-perf-tests Catch2::Catch2 boost_system boost_filesystem)
    target_include_directories(poor-perf-tests PRIVATE src/)
endif()
