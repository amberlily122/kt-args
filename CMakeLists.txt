cmake_minimum_required(VERSION 3.16)

project(kt-buildit)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
add_executable(kt-args-demo
  kt-args-demo.cpp
  )

