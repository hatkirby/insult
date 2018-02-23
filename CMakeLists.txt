cmake_minimum_required (VERSION 3.1)
project (insult)

add_subdirectory(vendor/verbly)
add_subdirectory(vendor/libtwittercpp)

find_package(PkgConfig)
pkg_check_modules(yaml-cpp yaml-cpp REQUIRED)

include_directories(
  vendor/verbly/lib
  ${yaml-cpp_INCLUDE_DIRS}
  vendor/libtwittercpp/src)

add_executable(insult insult.cpp patterner.cpp)
set_property(TARGET insult PROPERTY CXX_STANDARD 11)
set_property(TARGET insult PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(insult verbly twitter++ ${yaml-cpp_LIBRARIES})
