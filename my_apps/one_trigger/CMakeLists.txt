cmake_minimum_required(VERSION 3.20)
project(one_trigger CXX)


find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(one_trigger one_trigger.cpp)

target_link_libraries(one_trigger
  PRIVATE
  holoscan::core
)

