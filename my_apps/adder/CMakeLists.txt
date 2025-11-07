cmake_minimum_required(VERSION 3.20)
project(holoscan_add_example CXX)

 find_package(holoscan REQUIRED CONFIG PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install")
add_executable(add_example
  adder.cpp
)

target_link_libraries(add_example
  PRIVATE
  holoscan::core
)

