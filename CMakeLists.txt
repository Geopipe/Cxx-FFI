cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_STANDARD_REQUIRED ON)

project(TestCxxFFI)

find_package(Boost 1.65 COMPONENTS filesystem)

include_directories(${CMAKE_SOURCE_DIR}/include)

find_library(RE2_LIBRARY re2)

add_library(testlib SHARED example/test-lib.cc example/test-lib.hpp include/cxx-ffi/refl_base.hpp include/cxx-ffi/casts_table.hpp)
add_executable(test example/test.cc)
target_link_libraries(testlib Boost::filesystem Boost::headers dl ${RE2_LIBRARY})
target_link_libraries(test testlib)
target_compile_options(testlib PRIVATE -ftemplate-backtrace-limit=0)
target_compile_options(test PRIVATE -ftemplate-backtrace-limit=0)
