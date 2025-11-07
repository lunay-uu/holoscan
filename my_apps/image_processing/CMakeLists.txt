cmake_minimum_required(VERSION 3.20)
project(holoscan_image_processing CXX)

find_package(holoscan REQUIRED CONFIG
  PATHS "/opt/nvidia/holoscan" "/workspace/holoscan-sdk/install"
)

add_executable(image_processing
  image_processing.cpp
)

target_link_libraries(image_processing
  PRIVATE
  holoscan::core
)

