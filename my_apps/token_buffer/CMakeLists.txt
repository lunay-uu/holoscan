cmake_minimum_required(VERSION 3.20)
project(token_buffer CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(token_buffer
  token_buffer.cpp
)

target_link_libraries(token_buffer
  PRIVATE holoscan::core
)
