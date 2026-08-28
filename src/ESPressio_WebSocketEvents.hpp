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
class WebSocketConnectedEvent final : public TypedEvent<WebSocketConnectedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    explicit WebSocketConnectedEvent(Web::WebSocketConnectionId connectionId)
        : ConnectionId(connectionId) {}
};

/// <summary>Signals receipt of a server-side WebSocket binary message.</summary>
class WebSocketBinaryReceivedEvent final : public TypedEvent<WebSocketBinaryReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketBinaryReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals receipt of a server-side WebSocket text message.</summary>
class WebSocketTextReceivedEvent final : public TypedEvent<WebSocketTextReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketTextReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals that a server-side WebSocket connection closed.</summary>
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
class WebSocketClientConnectedEvent final : public TypedEvent<WebSocketClientConnectedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    explicit WebSocketClientConnectedEvent(Web::WebSocketConnectionId connectionId)
        : ConnectionId(connectionId) {}
};

/// <summary>Signals receipt of a WebSocket client binary message.</summary>
class WebSocketClientBinaryReceivedEvent final : public TypedEvent<WebSocketClientBinaryReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketClientBinaryReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals receipt of a WebSocket client text message.</summary>
class WebSocketClientTextReceivedEvent final : public TypedEvent<WebSocketClientTextReceivedEvent> {
public:
    const Web::WebSocketConnectionId ConnectionId;
    const std::size_t PayloadBytes;
    WebSocketClientTextReceivedEvent(Web::WebSocketConnectionId connectionId, std::size_t payloadBytes)
        : ConnectionId(connectionId), PayloadBytes(payloadBytes) {}
};

/// <summary>Signals that a WebSocket client connection closed.</summary>
class WebSocketClientDisconnectedEvent final : public TypedEvent<WebSocketClientDisconnectedEvent> {
public:
    const uint16_t Code;
    const WebSocketEventString Reason;

    explicit WebSocketClientDisconnectedEvent(const Web::WebSocketCloseReason& reason)
        : Code(reason.Code), Reason(reason.Reason.begin(), reason.Reason.end()) {}
};

} // namespace ESPressio::Event
