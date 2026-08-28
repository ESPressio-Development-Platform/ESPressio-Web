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
add_executable(espressio_web_websocket_event_bridge_tests test_websocket_event_bridge.cpp)
add_executable(espressio_web_http_tests test_http.cpp)
add_executable(espressio_web_router_tests test_router.cpp)
add_executable(espressio_web_middleware_tests test_middleware.cpp)
add_executable(espressio_web_resource_tests test_resources.cpp)
add_executable(espressio_web_persistence_resource_tests test_persistence_resources.cpp)
add_executable(espressio_web_dns_tests test_dns.cpp)
add_executable(espressio_web_service_tests test_service.cpp)
add_executable(espressio_web_event_integration_tests test_web_event.cpp)
add_executable(
    espressio_web_command_integration_tests
    test_web_command.cpp
    ../../ESPressio-Threads/src/ESPressio_Thread.cpp
    ../../ESPressio-Threads/src/ESPressio_ThreadTerminationDispatcher.cpp
)
add_executable(espressio_web_state_integration_tests test_web_state.cpp)
add_executable(
    espressio_web_implementor_example
    ../examples/Implementors/MinimalHttpPlatform/main.cpp
)

set(ESPRESSIO_WEB_TEST_TARGETS
    espressio_web_core_include
    espressio_web_websocket_tests
    espressio_web_websocket_policy_tests
    espressio_web_websocket_event_bridge_tests
    espressio_web_http_tests
    espressio_web_router_tests
    espressio_web_middleware_tests
    espressio_web_resource_tests
    espressio_web_persistence_resource_tests
    espressio_web_dns_tests
    espressio_web_service_tests
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

target_include_directories(espressio_web_event_integration_tests PRIVATE
    ../../ESPressio-Event/src
)

target_include_directories(espressio_web_websocket_event_bridge_tests PRIVATE
    ../../ESPressio-Event/src
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

target_include_directories(espressio_web_command_integration_tests PRIVATE
    ../../ESPressio-Timing/tests/stubs
    ../../ESPressio-Command/src
    ../../ESPressio-Event/src
    ../../ESPressio-Threads/src
    ../../ESPressio-Task/src
    ../../ESPressio-Timing/src
    ../../ESPressio-Units/src
)

target_include_directories(espressio_web_state_integration_tests PRIVATE
    ../../ESPressio-State/src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endforeach()
    # ESPressio-Event's current IEventManagerObserver default callback carries
    # named no-op parameters. Suppress only that upstream warning for this
    # header-integration compile target so Web's own warnings remain errors.
    target_compile_options(espressio_web_websocket_event_bridge_tests PRIVATE -Wno-unused-parameter)
elseif(MSVC)
    foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
        target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
endif()

enable_testing()
foreach(target ${ESPRESSIO_WEB_TEST_TARGETS})
    add_test(NAME ${target} COMMAND ${target})
endforeach()
