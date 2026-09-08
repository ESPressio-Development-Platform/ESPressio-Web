#pragma once

#if !__has_include(<ESPressio_Event.hpp>)
#error "ESPressio WebSocket Events require ESPressio-Event."
#endif

#include <cstddef>
#include <cstdint>

#include <ESPressio_Event.hpp>
#include <ESPressio_Memory.hpp>

#include "ESPressio_WebSocket.hpp"

namespace ESPressio::Event {

using WebSocketEventString = System::Memory::String<
    System::Memory::MemoryPolicy::ExternalPreferred
>;

/// <summary>Signals a server-side WebSocket endpoint lifecycle state transition.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - Previous (Web::WebSocketEndpointState): 1 bytes [0 bytes dynamic allocation]
 * - Current (Web::WebSocketEndpointState): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 28 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketEndpointStateChangedEvent final :
    public TypedEvent<WebSocketEndpointStateChangedEvent> {
public:
    const Web::WebSocketEndpointState Previous;
    const Web::WebSocketEndpointState Current;

    WebSocketEndpointStateChangedEvent(
        Web::WebSocketEndpointState previous,
        Web::WebSocketEndpointState current
    ) : Previous(previous), Current(current) {}
};

/// <summary>Signals a WebSocket client lifecycle state transition.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - Previous (Web::WebSocketClientState): 1 bytes [0 bytes dynamic allocation]
 * - Current (Web::WebSocketClientState): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 28 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketClientStateChangedEvent final :
    public TypedEvent<WebSocketClientStateChangedEvent> {
public:
    const Web::WebSocketClientState Previous;
    const Web::WebSocketClientState Current;

    WebSocketClientStateChangedEvent(
        Web::WebSocketClientState previous,
        Web::WebSocketClientState current
    ) : Previous(previous), Current(current) {}
};

/// <summary>Signals implementation-level WebSocket activity suitable for diagnostics.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ClientSide (bool): 1 bytes [0 bytes dynamic allocation]
 * - Kind (Web::WebSocketActivityKind): 1 bytes [0 bytes dynamic allocation]
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - FrameType (Web::WebSocketFrameType): 1 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - Error (Web::WebError): 1 bytes [0 bytes dynamic allocation]
 * - PlatformCode (int32_t): 4 bytes [0 bytes dynamic allocation]
 * - CloseCode (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Detail (WebSocketEventString): 24 bytes [_value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Total Memory: 80 bytes [Detail: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketActivityEvent final :
    public TypedEvent<WebSocketActivityEvent> {
public:
    const bool ClientSide;
    const Web::WebSocketActivityKind Kind;
    const Web::WebSocketConnectionId ConnectionId;
    const Web::WebSocketFrameType FrameType;
    const std::size_t PayloadBytes;
    const Web::WebError Error;
    const int32_t PlatformCode;
    const uint16_t CloseCode;
    const WebSocketEventString Detail;

    WebSocketActivityEvent(bool clientSide, const Web::WebSocketActivity& activity)
        : ClientSide(clientSide),
          Kind(activity.Kind),
          ConnectionId(activity.ConnectionId),
          FrameType(activity.FrameType),
          PayloadBytes(activity.PayloadBytes),
          Error(activity.Result.Error),
          PlatformCode(activity.Result.PlatformCode),
          CloseCode(activity.CloseCode),
          Detail(activity.Detail.begin(), activity.Detail.end()) {}
};

/// <summary>Signals that a server-side WebSocket connection became active.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketConnectedEvent final : public TypedEvent<WebSocketConnectedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    explicit WebSocketConnectedEvent(Web::WebSocketConnectionId connectionId)
        : ConnectionId(connectionId) {}
};

/// <summary>Signals receipt of a server-side WebSocket binary message.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 36 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketBinaryReceivedEvent final : public TypedEvent<WebSocketBinaryReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketBinaryReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals receipt of a server-side WebSocket text message.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 36 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketTextReceivedEvent final : public TypedEvent<WebSocketTextReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketTextReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals that a server-side WebSocket connection closed.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - Code (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Reason (WebSocketEventString): 24 bytes [_value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Total Memory: 60 bytes [Reason: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketDisconnectedEvent final : public TypedEvent<WebSocketDisconnectedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const uint16_t Code;
    const WebSocketEventString Reason;

    WebSocketDisconnectedEvent(
        Web::WebSocketConnectionId connectionId,
        const Web::WebSocketCloseReason& reason
    ) : ConnectionId(connectionId),
        Code(reason.Code),
        Reason(reason.Reason.begin(), reason.Reason.end()) {}
};

/// <summary>Signals that a WebSocket client connection became active.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketClientConnectedEvent final : public TypedEvent<WebSocketClientConnectedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    explicit WebSocketClientConnectedEvent(Web::WebSocketConnectionId connectionId)
        : ConnectionId(connectionId) {}
};

/// <summary>Signals receipt of a WebSocket client binary message.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 36 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketClientBinaryReceivedEvent final : public TypedEvent<WebSocketClientBinaryReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketClientBinaryReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals receipt of a WebSocket client text message.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - ConnectionId (Web::WebSocketConnectionId): 8 bytes [0 bytes dynamic allocation]
 * - PayloadBytes (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 36 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class WebSocketClientTextReceivedEvent final : public TypedEvent<WebSocketClientTextReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketClientTextReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals that a WebSocket client connection closed.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 24 bytes [0 bytes dynamic allocation]
 * Members:
 * - Code (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - Reason (WebSocketEventString): 24 bytes [_value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Total Memory: 52 bytes [Reason: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketClientDisconnectedEvent final : public TypedEvent<WebSocketClientDisconnectedEvent> {
public:
    const uint16_t Code;
    const WebSocketEventString Reason;

    explicit WebSocketClientDisconnectedEvent(const Web::WebSocketCloseReason& reason)
        : Code(reason.Code), Reason(reason.Reason.begin(), reason.Reason.end()) {}
};

} // namespace ESPressio::Event
