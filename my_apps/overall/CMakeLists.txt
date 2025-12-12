cmake_minimum_required(VERSION 3.20)
project(overall CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(overall
  overall.cpp
)

target_link_libraries(overall
  PRIVATE holoscan::core
)
