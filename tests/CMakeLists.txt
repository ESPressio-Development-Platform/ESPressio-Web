cmake_minimum_required(VERSION 3.16)
project(ESPressioWebHostTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(ESPRESSIO_WEB_COMMON_INCLUDES
    ../src
    ../../ESPressio-System/src
    ../../ESPressio-Observable/src
)

add_executable(espressio_web_core_include test_core_include.cpp)
target_include_directories(espressio_web_core_include PRIVATE ${ESPRESSIO_WEB_COMMON_INCLUDES})
target_link_libraries(espressio_web_core_include PRIVATE Threads::Threads)

add_executable(espressio_web_websocket_tests test_websocket.cpp)
target_include_directories(espressio_web_websocket_tests PRIVATE ${ESPRESSIO_WEB_COMMON_INCLUDES})
target_link_libraries(espressio_web_websocket_tests PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target espressio_web_core_include espressio_web_websocket_tests)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endforeach()
elseif(MSVC)
    foreach(target espressio_web_core_include espressio_web_websocket_tests)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
endif()

enable_testing()
add_test(NAME espressio_web_core_include COMMAND espressio_web_core_include)
add_test(NAME espressio_web_websocket_tests COMMAND espressio_web_websocket_tests)
