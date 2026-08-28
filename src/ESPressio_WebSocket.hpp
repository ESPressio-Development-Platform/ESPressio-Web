#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

enum class WebSocketFrameType : uint8_t {
    Text = 0,
    Binary,
    Ping,
    Pong,
    Close
};

struct WebSocketCloseReason final {
    uint16_t Code = 1000;
    std::string_view Reason;
};

struct WebSocketClientConfiguration final {
    std::string_view Host;
    uint16_t Port = 0;
    std::string_view Path = "/";
    std::string_view Protocol;
    bool Secure = false;
};

class IWebSocketConnection {
public:
    virtual ~IWebSocketConnection() = default;

    virtual WebSocketConnectionId Id() const noexcept = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual WebResult SendBinary(const uint8_t* data, std::size_t size) = 0;
    virtual WebResult SendText(std::string_view text) = 0;
    virtual WebResult Close(const WebSocketCloseReason& reason = {}) = 0;
};

class IWebSocketEndpointObserver {
public:
    virtual ~IWebSocketEndpointObserver() = default;
    virtual void OnWebSocketConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketDisconnected(WebSocketConnectionId, const WebSocketCloseReason&) {}
};

class IWebSocketEndpoint {
public:
    virtual ~IWebSocketEndpoint() = default;

    virtual void SetObserver(IWebSocketEndpointObserver* observer) = 0;
    virtual std::size_t ConnectionCount() const noexcept = 0;
    virtual WebResult BroadcastBinary(const uint8_t* data, std::size_t size) = 0;
    virtual WebResult BroadcastText(std::string_view text) = 0;
    virtual WebResult CloseAll(const WebSocketCloseReason& reason = {}) = 0;
};

class IWebSocketClientObserver {
public:
    virtual ~IWebSocketClientObserver() = default;
    virtual void OnWebSocketClientConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketClientBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketClientText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketClientDisconnected(const WebSocketCloseReason&) {}
};

class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;

    virtual void SetObserver(IWebSocketClientObserver* observer) = 0;
    virtual WebResult Connect(const WebSocketClientConfiguration& configuration) = 0;
    virtual WebResult Disconnect(const WebSocketCloseReason& reason = {}) = 0;
    virtual bool IsConnected() const noexcept = 0;
    virtual IWebSocketConnection* Connection() noexcept = 0;
};

} // namespace ESPressio::Web
