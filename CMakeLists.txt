cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_STANDARD_REQUIRED ON)

project(TestCxxFFI)

find_package(Boost 1.67)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(test test.cc include/refl_base.hpp)
