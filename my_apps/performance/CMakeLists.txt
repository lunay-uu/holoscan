cmake_minimum_required(VERSION 3.20)
project(performance CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(performance
  performance.cpp
)

target_link_libraries(performance
  PRIVATE holoscan::core
)
