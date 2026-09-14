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
add_executable(espressio_web_websocket_tests test_websocket.cpp)
add_executable(espressio_web_websocket_policy_tests test_websocket_policy.cpp)
add_executable(espressio_web_websocket_socket_adapter_tests test_websocket_socket_adapter.cpp)
add_executable(espressio_web_websocket_clock_tests test_websocket_clock_synchronization.cpp)
add_executable(espressio_web_http_tests test_http.cpp)
add_executable(espressio_web_router_tests test_router.cpp)
add_executable(espressio_web_middleware_tests test_middleware.cpp)
add_executable(espressio_web_resource_tests test_resources.cpp)
add_executable(espressio_web_persistence_resource_tests test_persistence_resources.cpp)
add_executable(espressio_web_dns_tests test_dns.cpp)
add_executable(espressio_web_service_tests test_service.cpp)
add_executable(espressio_web_primitive_discovery_tests test_primitive_discovery.cpp)
add_executable(espressio_web_event_integration_tests test_web_event.cpp)
add_executable(espressio_web_command_integration_tests test_web_command.cpp)
add_executable(espressio_web_state_integration_tests test_web_state.cpp)
add_executable(
    espressio_web_implementor_example
    ../examples/Implementors/MinimalHttpPlatform/main.cpp
)

set(ESPRESSIO_WEB_TEST_TARGETS
    espressio_web_core_include
    espressio_web_websocket_tests
    espressio_web_websocket_policy_tests
    espressio_web_websocket_socket_adapter_tests
    espressio_web_websocket_clock_tests
    espressio_web_http_tests
    espressio_web_router_tests
    espressio_web_middleware_tests
    espressio_web_resource_tests
    espressio_web_persistence_resource_tests
    espressio_web_dns_tests
    espressio_web_service_tests
    espressio_web_primitive_discovery_tests
    espressio_web_event_integration_tests
    espressio_web_command_integration_tests
    espressio_web_state_integration_tests
    espressio_web_implementor_example
)

foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
    target_include_directories(${target} PRIVATE ${ESPRESSIO_WEB_COMMON_INCLUDES})
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()

target_include_directories(espressio_web_persistence_resource_tests PRIVATE
    ../../ESPressio-Persistence/src
)

target_include_directories(espressio_web_primitive_discovery_tests PRIVATE
    ../../ESPressio-Primitive/src
)

target_include_directories(espressio_web_event_integration_tests PRIVATE
    ../../ESPressio-Event/src
    ../../ESPressio-Primitive/src
    ../../ESPressio-Serializable/src
    ../../ESPressio-Task/src
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

target_include_directories(espressio_web_websocket_socket_adapter_tests PRIVATE
    ../../ESPressio-Sockets/src
    ../../ESPressio-Adapters/src
    ../../ESPressio-Primitive/src
    ../../ESPressio-Task/src
)

target_include_directories(espressio_web_websocket_clock_tests PRIVATE
    ../../ESPressio-Sockets/src
    ../../ESPressio-Sockets/src/timing
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

target_include_directories(espressio_web_command_integration_tests PRIVATE
    ../../ESPressio-Command/src
    ../../ESPressio-Primitive/src
    ../../ESPressio-Serializable/src
    ../../ESPressio-Persistence/src
    ../../ESPressio-Security/src
    ../../ESPressio-Threads/src
    ../../ESPressio-Task/src
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

target_include_directories(espressio_web_state_integration_tests PRIVATE
    ../../ESPressio-State/src
    ../../ESPressio-Primitive/src
    ../../ESPressio-Serializable/src
    ../../ESPressio-Persistence/src
    ../../ESPressio-Security/src
    ../../ESPressio-Threads/src
    ../../ESPressio-Task/src
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endforeach()
elseif(MSVC)
    foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
        target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
endif()

enable_testing()
foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
    add_test(NAME ${target} COMMAND ${target})
endforeach()
