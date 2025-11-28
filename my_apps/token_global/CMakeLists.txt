cmake_minimum_required(VERSION 3.20)
project(token_global CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(token_global
  token_global.cpp
)

target_link_libraries(token_global
  PRIVATE holoscan::core
)
