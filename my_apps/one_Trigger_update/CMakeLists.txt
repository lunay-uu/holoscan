cmake_minimum_required(VERSION 3.20)
project(one_trigger_updated CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(one_trigger_updated
  one_trigger_updated.cpp
)

target_link_libraries(one_trigger_updated
  PRIVATE holoscan::core
)

