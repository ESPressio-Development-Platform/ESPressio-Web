#pragma once

#if !__has_include(<ESPressio_EventTransport.hpp>)
#error "ESPressio WebSocket Event transport requires ESPressio-Event."
#endif

#include <cstddef>
#include <cstdint>

#include <ESPressio_EventTransport.hpp>

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"

namespace ESPressio::Web {

struct WebSocketEventTransportConfiguration final {
    std::size_t MaximumPacketBytes = 65536;
};

class WebSocketServerEventTransport final :
    public Event::IEventTransport,
    private IWebSocketEndpointObserver {
public:
    WebSocketServerEventTransport() = default;
    ~WebSocketServerEventTransport() override { Detach(); }

    WebSocketServerEventTransport(const WebSocketServerEventTransport&) = delete;
    WebSocketServerEventTransport& operator=(const WebSocketServerEventTransport&) = delete;

    WebResult Attach(
        WebSocketEndpoint& endpoint,
        const WebSocketEventTransportConfiguration& configuration = {}
    ) {
        if (_endpoint == &endpoint) return WebResult::Success();
        if (_endpoint != nullptr || configuration.MaximumPacketBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        _configuration = configuration;
        _endpoint = &endpoint;
        _observerHandle = endpoint.RegisterObserver(this);
        return WebResult::Success();
    }

    void Detach() {
        _observerHandle.reset();
        _endpoint = nullptr;
        _receiver = nullptr;
    }

    bool Send(const Event::EventTransportPacket& packet) override {
        if (
            _endpoint == nullptr ||
            packet.Data == nullptr ||
            packet.Size == 0 ||
            packet.Size > _configuration.MaximumPacketBytes
        ) {
            return false;
        }
        return static_cast<bool>(_endpoint->BroadcastBinary(packet.Data, packet.Size));
    }

    void SetReceiver(Event::IEventTransportReceiver* receiver) override {
        _receiver = receiver;
    }

private:
    WebSocketEndpoint* _endpoint = nullptr;
    WebSocketEventTransportConfiguration _configuration;
    Event::IEventTransportReceiver* _receiver = nullptr;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketBinary(
        IWebSocketConnection&,
        const uint8_t* data,
        std::size_t size
    ) override {
        auto* receiver = _receiver;
        if (
            receiver == nullptr ||
            data == nullptr ||
            size == 0 ||
            size > _configuration.MaximumPacketBytes
        ) {
            return;
        }
        receiver->ReceiveEventTransportPacket(this, data, size);
    }
};

class WebSocketClientEventTransport final :
    public Event::IEventTransport,
    private IWebSocketClientObserver {
public:
    WebSocketClientEventTransport() = default;
    ~WebSocketClientEventTransport() override { Detach(); }

    WebSocketClientEventTransport(const WebSocketClientEventTransport&) = delete;
    WebSocketClientEventTransport& operator=(const WebSocketClientEventTransport&) = delete;

    WebResult Attach(
        WebSocketClient& client,
        const WebSocketEventTransportConfiguration& configuration = {}
    ) {
        if (_client == &client) return WebResult::Success();
        if (_client != nullptr || configuration.MaximumPacketBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        _configuration = configuration;
        _client = &client;
        _observerHandle = client.RegisterObserver(this);
        return WebResult::Success();
    }

    void Detach() {
        _observerHandle.reset();
        _client = nullptr;
        _receiver = nullptr;
    }

    bool Send(const Event::EventTransportPacket& packet) override {
        if (
            _client == nullptr ||
            packet.Data == nullptr ||
            packet.Size == 0 ||
            packet.Size > _configuration.MaximumPacketBytes
        ) {
            return false;
        }
        auto* connection = _client->Connection();
        return connection != nullptr &&
            static_cast<bool>(connection->SendBinary(packet.Data, packet.Size));
    }

    void SetReceiver(Event::IEventTransportReceiver* receiver) override {
        _receiver = receiver;
    }

private:
    WebSocketClient* _client = nullptr;
    WebSocketEventTransportConfiguration _configuration;
    Event::IEventTransportReceiver* _receiver = nullptr;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketClientBinary(
        IWebSocketConnection&,
        const uint8_t* data,
        std::size_t size
    ) override {
        auto* receiver = _receiver;
        if (
            receiver == nullptr ||
            data == nullptr ||
            size == 0 ||
            size > _configuration.MaximumPacketBytes
        ) {
            return;
        }
        receiver->ReceiveEventTransportPacket(this, data, size);
    }
};

} // namespace ESPressio::Web
