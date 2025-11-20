cmake_minimum_required(VERSION 3.20)
project(two_out CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(two_out
  two_out.cpp
)

target_link_libraries(two_out
  PRIVATE holoscan::core
)
