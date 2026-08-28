cmake_minimum_required(VERSION 3.16)
project(ESPressioWebHostTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(espressio_web_core_include test_core_include.cpp)
target_include_directories(espressio_web_core_include PRIVATE ../src)
