cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")

find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD 14)

add_executable(poor-perf src/main.cpp)
target_link_libraries(poor-perf Catch2::Catch2 boost_system boost_filesystem Threads::Threads)

add_subdirectory(3rd-party/Catch2)
add_executable(poor-perf-tests tests/main.cpp tests/proc_maps_tests.cpp)
target_link_libraries(poor-perf-tests Catch2::Catch2 boost_system boost_filesystem)
target_include_directories(poor-perf-tests PRIVATE src/)
