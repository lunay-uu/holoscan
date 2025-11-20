cmake_minimum_required(VERSION 3.20)
project(two_out_new CXX)
find_package(holoscan REQUIRED CONFIG PATHS
  "/opt/nvidia/holoscan"
  "/workspace/holoscan"
)

add_executable(two_out_new
  two_out_new.cpp
)

target_link_libraries(two_out_new
  PRIVATE holoscan::core
)




