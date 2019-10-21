cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_STANDARD_REQUIRED ON)

project(TestCxxFFI)

find_package(Boost 1.67 COMPONENTS Filesystem)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(testlib SHARED test-lib.cc test-lib.hpp include/refl_base.hpp)
add_executable(test test.cc)
target_link_libraries(test testlib Boost::Filesystem)
target_compile_options(testlib PRIVATE -ftemplate-backtrace-limit=0)
target_compile_options(test PRIVATE -ftemplate-backtrace-limit=0)
