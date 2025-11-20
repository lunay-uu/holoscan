cmake_minimum_required(VERSION 3.20)
project(two_in CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(two_in
  two_in.cpp
)

target_link_libraries(two_in
  PRIVATE holoscan::core
)


