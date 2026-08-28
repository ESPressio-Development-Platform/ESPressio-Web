#pragma once

#if !__has_include(<ESPressio_Event.hpp>)
#error "ESPressio WebSocket Event bridge requires ESPressio-Event."
#endif

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"
#include "ESPressio_WebSocketEvents.hpp"

namespace ESPressio::Event {

/// <summary>Bridges WebSocket endpoint/client observer notifications into local ESPressio Events.</summary>
class WebSocketEventBridge final :
    public Web::IWebSocketEndpointObserver,
    public Web::IWebSocketClientObserver {
public:
    WebSocketEventBridge() = default;
    ~WebSocketEventBridge() override { Shutdown(); }

    WebSocketEventBridge(const WebSocketEventBridge&) = delete;
    WebSocketEventBridge& operator=(const WebSocketEventBridge&) = delete;

    /// <summary>Begins bridging the supplied server-side WebSocket endpoint.</summary>
    bool Initialize(Web::WebSocketEndpoint& endpoint) {
        if (_endpoint == &endpoint && _endpointObserverHandle) return true;
        if (_endpoint != nullptr) return false;
        _endpoint = &endpoint;
        _endpointObserverHandle = endpoint.RegisterObserver(this);
        if (!_endpointObserverHandle) {
            _endpoint = nullptr;
            return false;
        }
        return true;
    }

    /// <summary>Begins bridging the supplied WebSocket client.</summary>
    bool Initialize(Web::WebSocketClient& client) {
        if (_client == &client && _clientObserverHandle) return true;
        if (_client != nullptr) return false;
        _client = &client;
        _clientObserverHandle = client.RegisterObserver(this);
        if (!_clientObserverHandle) {
            _client = nullptr;
            return false;
        }
        return true;
    }

    /// <summary>Stops all endpoint/client observation.</summary>
    void Shutdown() {
        _endpointObserverHandle.reset();
        _clientObserverHandle.reset();
        _endpoint = nullptr;
        _client = nullptr;
    }

    /// <summary>Reports whether at least one WebSocket object is being observed.</summary>
    bool IsInitialized() const noexcept {
        return static_cast<bool>(_endpointObserverHandle) ||
               static_cast<bool>(_clientObserverHandle);
    }

    void OnWebSocketEndpointStateChanged(
        Web::WebSocketEndpointState previous,
        Web::WebSocketEndpointState current
    ) override {
        (new WebSocketEndpointStateChangedEvent(previous, current))->Queue();
    }

    void OnWebSocketActivity(const Web::WebSocketActivity& activity) override {
        (new WebSocketActivityEvent(false, activity))->Queue();
    }

    void OnWebSocketConnected(Web::IWebSocketConnection& connection) override {
        (new WebSocketConnectedEvent(connection.Id()))->Queue();
    }

    void OnWebSocketBinary(
        Web::IWebSocketConnection& connection,
        const uint8_t*,
        std::size_t size
    ) override {
        (new WebSocketBinaryReceivedEvent(connection.Id(), size))->Queue();
    }

    void OnWebSocketText(
        Web::IWebSocketConnection& connection,
        std::string_view text
    ) override {
        (new WebSocketTextReceivedEvent(connection.Id(), text.size()))->Queue();
    }

    void OnWebSocketDisconnected(
        Web::WebSocketConnectionId id,
        const Web::WebSocketCloseReason& reason
    ) override {
        (new WebSocketDisconnectedEvent(id, reason))->Queue();
    }

    void OnWebSocketClientStateChanged(
        Web::WebSocketClientState previous,
        Web::WebSocketClientState current
    ) override {
        (new WebSocketClientStateChangedEvent(previous, current))->Queue();
    }

    void OnWebSocketClientActivity(const Web::WebSocketActivity& activity) override {
        (new WebSocketActivityEvent(true, activity))->Queue();
    }

    void OnWebSocketClientConnected(Web::IWebSocketConnection& connection) override {
        (new WebSocketClientConnectedEvent(connection.Id()))->Queue();
    }

    void OnWebSocketClientBinary(
        Web::IWebSocketConnection& connection,
        const uint8_t*,
        std::size_t size
    ) override {
        (new WebSocketClientBinaryReceivedEvent(connection.Id(), size))->Queue();
    }

    void OnWebSocketClientText(
        Web::IWebSocketConnection& connection,
        std::string_view text
    ) override {
        (new WebSocketClientTextReceivedEvent(connection.Id(), text.size()))->Queue();
    }

    void OnWebSocketClientDisconnected(const Web::WebSocketCloseReason& reason) override {
        (new WebSocketClientDisconnectedEvent(reason))->Queue();
    }

private:
    Web::WebSocketEndpoint* _endpoint = nullptr;
    Web::WebSocketClient* _client = nullptr;
    Observable::ObserverHandlePtr _endpointObserverHandle;
    Observable::ObserverHandlePtr _clientObserverHandle;
};

} // namespace ESPressio::Event
