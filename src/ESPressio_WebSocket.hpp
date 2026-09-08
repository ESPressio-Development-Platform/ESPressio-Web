#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ESPressio_WebTransportSecurity.hpp"
#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebSocketFrameType : uint8_t {
    Text = 0,
    Binary,
    Ping,
    Pong,
    Close
};

/// <summary>Describes the lifecycle state of a server-side WebSocket endpoint.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebSocketEndpointState : uint8_t {
    Detached = 0,
    Attached,
    Binding,
    Bound,
    Unbinding
};

/// <summary>Describes the lifecycle state of a WebSocket client.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebSocketClientState : uint8_t {
    Detached = 0,
    Attached,
    Connecting,
    Connected,
    Disconnecting,
    Disconnected
};

/// <summary>Identifies a diagnostic activity emitted by a WebSocket platform implementation.</summary>
/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebSocketActivityKind : uint8_t {
    BindRequested = 0,
    Bound,
    Unbound,
    UpgradeRequested,
    ConnectionCreated,
    FrameHeaderReceived,
    FramePayloadReceived,
    FragmentStarted,
    FragmentCompleted,
    PingReceived,
    PongReceived,
    PeerCloseReceived,
    CloseRequested,
    SendQueued,
    SendFailed,
    ReceiveFailed,
    SessionClosed,
    ConnectRequested,
    NativeConnected,
    NativeDisconnected,
    NativeClosed,
    NativeError,
    ProtocolError
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Code (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Reason (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebSocketCloseReason final {
    uint16_t Code = 1000;
    std::string_view Reason;
};

/// <summary>Portable metadata describing an implementation-level WebSocket activity.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - Kind (WebSocketActivityKind): 1 bytes [0 bytes dynamic allocation]
 * - ConnectionId (WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - FrameType (WebSocketFrameType): 1 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - Result (WebResult): 8 bytes [0 bytes dynamic allocation]
 * - CloseCode (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Detail (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 40 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebSocketActivity final {
    WebSocketActivityKind Kind = WebSocketActivityKind::ProtocolError;
    WebSocketConnectionId ConnectionId = 0;
    WebSocketFrameType FrameType = WebSocketFrameType::Binary;
    std::size_t PayloadBytes = 0;
    WebResult Result = WebResult::Success();
    uint16_t CloseCode = 0;
    std::string_view Detail;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Path (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * - Protocol (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebSocketEndpointConfiguration final {
    std::string_view Path;
    std::string_view Protocol;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Name (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * - Value (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebClientHeader final {
    std::string_view Name;
    std::string_view Value;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IWebClientHeaderSource {
public:
    virtual ~IWebClientHeaderSource() = default;
    virtual std::size_t Count() const noexcept = 0;
    virtual bool Header(std::size_t index, WebClientHeader& header) const noexcept = 0;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - NetworkTimeoutMilliseconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - AutomaticReconnect (bool): 1 bytes [0 bytes dynamic allocation]
 * - ReconnectDelayMilliseconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - ReconnectAfterCleanClose (bool): 1 bytes [0 bytes dynamic allocation]
 * - PingIntervalMilliseconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - PongTimeoutMilliseconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - TcpKeepAlive (bool): 1 bytes [0 bytes dynamic allocation]
 * - TcpKeepAliveIdleSeconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - TcpKeepAliveIntervalSeconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - TcpKeepAliveProbeCount (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * - MaximumHandshakeHeaderBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 44 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members:
 * - Host (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * - Port (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Path (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * - Protocol (std::string_view): 8 bytes [0 bytes dynamic allocation]
 * - Transport (WebTransportMode): 1 bytes [0 bytes dynamic allocation]
 * - Tls (WebTlsConfiguration): 40 bytes [0 bytes dynamic allocation]
 * - Headers (IWebClientHeaderSource*): 4 bytes [0 bytes dynamic allocation]
 * - Policy (WebSocketClientConnectionPolicy): 44 bytes [0 bytes dynamic allocation]
 * Total Memory: 120 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IWebSocketConnection {
public:
    virtual ~IWebSocketConnection() = default;
    virtual WebSocketConnectionId Id() const noexcept = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual WebResult SendBinary(const uint8_t* data, std::size_t size) = 0;
    virtual WebResult SendText(std::string_view text) = 0;
    virtual WebResult Close(const WebSocketCloseReason& reason = {}) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IWebSocketEndpointPlatformSink {
public:
    virtual ~IWebSocketEndpointPlatformSink() = default;
    virtual void OnPlatformWebSocketConnected(IWebSocketConnection&) = 0;
    virtual void OnPlatformWebSocketBinary(IWebSocketConnection&, const uint8_t*, std::size_t) = 0;
    virtual void OnPlatformWebSocketText(IWebSocketConnection&, std::string_view) = 0;
    virtual void OnPlatformWebSocketDisconnected(WebSocketConnectionId, const WebSocketCloseReason&) = 0;
    /// <summary>Receives implementation-level WebSocket diagnostics that are safe to expose portably.</summary>
    virtual void OnPlatformWebSocketActivity(const WebSocketActivity&) {}
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IWebSocketClientPlatformSink {
public:
    virtual ~IWebSocketClientPlatformSink() = default;
    virtual void OnPlatformWebSocketClientConnected(IWebSocketConnection&) = 0;
    virtual void OnPlatformWebSocketClientBinary(IWebSocketConnection&, const uint8_t*, std::size_t) = 0;
    virtual void OnPlatformWebSocketClientText(IWebSocketConnection&, std::string_view) = 0;
    virtual void OnPlatformWebSocketClientDisconnected(const WebSocketCloseReason&) = 0;
    /// <summary>Receives implementation-level WebSocket client diagnostics that are safe to expose portably.</summary>
    virtual void OnPlatformWebSocketClientActivity(const WebSocketActivity&) {}
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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
