#pragma once

#define ESPRESSIO_WEB_VERSION_MAJOR 0
#define ESPRESSIO_WEB_VERSION_MINOR 1
#define ESPRESSIO_WEB_VERSION_PATCH 0

#include "ESPressio_WebTypes.hpp"
#include "ESPressio_Http.hpp"
#include "ESPressio_HttpServer.hpp"
#include "ESPressio_Router.hpp"
#include "ESPressio_Middleware.hpp"
#include "ESPressio_Resources.hpp"
#include "ESPressio_HttpApplication.hpp"
#include "ESPressio_HttpService.hpp"
#include "ESPressio_Dns.hpp"
#include "ESPressio_WebSocket.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"
#include "ESPressio_WebSocketClient.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Members: none (standalone empty object occupies 1 byte; an eligible empty base may be optimized to 0 bytes).
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct Version final {
    static constexpr int Major = ESPRESSIO_WEB_VERSION_MAJOR;
    static constexpr int Minor = ESPRESSIO_WEB_VERSION_MINOR;
    static constexpr int Patch = ESPRESSIO_WEB_VERSION_PATCH;
};

}
