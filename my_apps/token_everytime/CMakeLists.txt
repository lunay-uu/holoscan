cmake_minimum_required(VERSION 3.20)
project(token_everytime CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(token_everytime
  token_everytime.cpp
)

target_link_libraries(token_everytime
  PRIVATE holoscan::core
)
