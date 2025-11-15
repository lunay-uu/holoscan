cmake_minimum_required(VERSION 3.20)
project(async_kpop CXX)

find_package(holoscan REQUIRED CONFIG PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk")

add_executable(async_kpop async_kpop.cpp)
target_link_libraries(async_kpop PRIVATE holoscan::core)

