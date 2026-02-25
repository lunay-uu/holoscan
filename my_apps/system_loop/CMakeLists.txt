cmake_minimum_required(VERSION 3.20)
project(system_loop CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(system_loop
  system_loop.cpp
)

target_link_libraries(system_loop
  PRIVATE holoscan::core
)
