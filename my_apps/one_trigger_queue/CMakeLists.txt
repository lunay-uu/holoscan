cmake_minimum_required(VERSION 3.20)
project(one_trigger_queue CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(one_trigger_queue
  one_trigger_queue.cpp
)

target_link_libraries(one_trigger_queue
  PRIVATE holoscan::core
)
