#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ESPressio_WebTransportSecurity.hpp"
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

struct WebSocketEndpointConfiguration final {
    std::string_view Path;
    std::string_view Protocol;
};

struct WebClientHeader final {
    std::string_view Name;
    std::string_view Value;
};

class IWebClientHeaderSource {
public:
    virtual ~IWebClientHeaderSource() = default;
    virtual std::size_t Count() const noexcept = 0;
    virtual bool Header(std::size_t index, WebClientHeader& header) const noexcept = 0;
};

struct WebSocketClientConnectionPolicy final {
    uint32_t NetworkTimeoutMilliseconds = 10000;

    // AutomaticReconnect applies to unexpected transport/network failures.
    bool AutomaticReconnect = false;
    uint32_t ReconnectDelayMilliseconds = 10000;

    // Some platforms can separately reconnect after a clean RFC6455 close.
    // A provider that cannot represent this must return Unsupported when true.
    bool ReconnectAfterCleanClose = false;

    // Zero may be used to request the concrete platform's default heartbeat
    // behavior. Non-zero values describe explicit portable policy.
    uint32_t PingIntervalMilliseconds = 10000;
    uint32_t PongTimeoutMilliseconds = 10000;

    bool TcpKeepAlive = false;
    uint32_t TcpKeepAliveIdleSeconds = 5;
    uint32_t TcpKeepAliveIntervalSeconds = 5;
    uint32_t TcpKeepAliveProbeCount = 3;

    std::size_t MaximumHandshakeHeaderBytes = 4096;
};

struct WebSocketClientConfiguration final {
    std::string_view Host;
    uint16_t Port = 0;
    std::string_view Path = "/";
    std::string_view Protocol;
    WebTransportMode Transport = WebTransportMode::Plain;
    WebTlsConfiguration Tls;
    const IWebClientHeaderSource* Headers = nullptr;
    WebSocketClientConnectionPolicy Policy;
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

class IWebSocketEndpointPlatformSink {
public:
    virtual ~IWebSocketEndpointPlatformSink() = default;
    virtual void OnPlatformWebSocketConnected(IWebSocketConnection&) = 0;
    virtual void OnPlatformWebSocketBinary(IWebSocketConnection&, const uint8_t*, std::size_t) = 0;
    virtual void OnPlatformWebSocketText(IWebSocketConnection&, std::string_view) = 0;
    virtual void OnPlatformWebSocketDisconnected(WebSocketConnectionId, const WebSocketCloseReason&) = 0;
};

class IWebSocketEndpointPlatform {
public:
    virtual ~IWebSocketEndpointPlatform() = default;
    virtual void SetSink(IWebSocketEndpointPlatformSink* sink) = 0;
    virtual WebResult Bind(const WebSocketEndpointConfiguration& configuration) = 0;
    virtual WebResult Unbind() = 0;
    virtual bool IsBound() const noexcept = 0;
    virtual std::size_t ConnectionCount() const noexcept = 0;
    virtual WebResult BroadcastBinary(const uint8_t* data, std::size_t size) = 0;
    virtual WebResult BroadcastText(std::string_view text) = 0;
    virtual WebResult CloseAll(const WebSocketCloseReason& reason = {}) = 0;
};

class IWebSocketClientPlatformSink {
public:
    virtual ~IWebSocketClientPlatformSink() = default;
    virtual void OnPlatformWebSocketClientConnected(IWebSocketConnection&) = 0;
    virtual void OnPlatformWebSocketClientBinary(IWebSocketConnection&, const uint8_t*, std::size_t) = 0;
    virtual void OnPlatformWebSocketClientText(IWebSocketConnection&, std::string_view) = 0;
    virtual void OnPlatformWebSocketClientDisconnected(const WebSocketCloseReason&) = 0;
};

class IWebSocketClientPlatform {
public:
    virtual ~IWebSocketClientPlatform() = default;
    virtual void SetSink(IWebSocketClientPlatformSink* sink) = 0;
    virtual WebResult Connect(const WebSocketClientConfiguration& configuration) = 0;
    virtual WebResult Disconnect(const WebSocketCloseReason& reason = {}) = 0;
    virtual bool IsConnected() const noexcept = 0;
    virtual IWebSocketConnection* Connection() noexcept = 0;
};

} // namespace ESPressio::Web
