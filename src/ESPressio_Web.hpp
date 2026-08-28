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

#if __has_include(<ESPressio_Event.hpp>)
#include "ESPressio_WebSocketEvents.hpp"
#include "ESPressio_WebSocketEventBridge.hpp"
#endif

namespace ESPressio::Web {

struct Version final {
    static constexpr int Major = ESPRESSIO_WEB_VERSION_MAJOR;
    static constexpr int Minor = ESPRESSIO_WEB_VERSION_MINOR;
    static constexpr int Patch = ESPRESSIO_WEB_VERSION_PATCH;
};

}
