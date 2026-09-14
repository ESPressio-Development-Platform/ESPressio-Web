#pragma once

#if !__has_include(<ESPressio_SocketAdapterTransport.hpp>)
#error "ESPressio WebSocket Adapter integration requires the final ESPressio-Sockets neutral A2 transport surface."
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <ESPressio_SocketAdapterTransport.hpp>

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"

namespace ESPressio::Web {

/// Caller-owned admission/mapping policy for dynamic server-side WebSocket connections.
/// A successful selection maps one authenticated/authorized connection to exactly one
/// preconfigured, frozen socket route slot. Discovery or connection existence alone
/// never grants a route or semantic provenance.
class IWebSocketSocketAdapterSessionSelector {
public:
    virtual ~IWebSocketSocketAdapterSessionSelector() = default;
    virtual bool SelectSlot(
        IWebSocketConnection& connection,
        std::size_t slotCount,
        std::size_t& slotIndex
    ) const noexcept = 0;
};

namespace Detail {

inline Sockets::SocketAdapterWriteDisposition MapWebSocketWriteResult(WebResult result) noexcept {
    if (result) return Sockets::SocketAdapterWriteDisposition::Accepted;
    switch (result.Error) {
        case WebError::ResourceExhausted:
            return Sockets::SocketAdapterWriteDisposition::ResourceUnavailable;
        case WebError::ConnectionFailure:
        case WebError::NotRunning:
        case WebError::InvalidState:
        case WebError::Closed:
        case WebError::PlatformFailure:
            return Sockets::SocketAdapterWriteDisposition::TemporarilyUnavailable;
        case WebError::InvalidConfiguration:
        case WebError::AlreadyRunning:
        case WebError::NotFound:
        case WebError::RequestTooLarge:
        case WebError::Unsupported:
        case WebError::ProtocolError:
        case WebError::Count:
            return Sockets::SocketAdapterWriteDisposition::PermanentlyRejected;
        case WebError::None:
            return Sockets::SocketAdapterWriteDisposition::Accepted;
    }
    return Sockets::SocketAdapterWriteDisposition::PermanentlyRejected;
}

inline WebSocketCloseReason CloseReasonForFeed(Sockets::SocketAdapterFeedStatus status) noexcept {
    switch (status) {
        case Sockets::SocketAdapterFeedStatus::Busy:
        case Sockets::SocketAdapterFeedStatus::ResourceUnavailable:
        case Sockets::SocketAdapterFeedStatus::NotRunning:
            return {1013, "Primitive transport temporarily unavailable"};
        case Sockets::SocketAdapterFeedStatus::Unsupported:
        case Sockets::SocketAdapterFeedStatus::Rejected:
            return {1008, "Primitive transport rejected"};
        case Sockets::SocketAdapterFeedStatus::Malformed:
        case Sockets::SocketAdapterFeedStatus::Partial:
            return {1002, "Malformed Primitive transport frame"};
        case Sockets::SocketAdapterFeedStatus::Accepted:
            return {};
    }
    return {1002, "Invalid Primitive transport result"};
}

} // namespace Detail

/// Fixed route-slot bridge from a Web-owned server endpoint to the neutral Sockets/A2
/// datagram transport. Dynamic connections occupy predeclared slots; route topology,
/// writer targets and semantic provenance are bound into SocketAdapterTransport before
/// that transport freezes. This class owns no Primitive family codec, retry queue,
/// admission interpretation, payload copy, worker or connection-to-family registry.
template<class TSocketTransport, std::size_t TMaximumConnections>
class WebSocketSocketAdapterEndpointBinding final : public IWebSocketEndpointObserver {
    static_assert(TMaximumConnections > 0, "WebSocket Adapter endpoint requires finite connection capacity");

    struct Slot final {
        WebSocketSocketAdapterEndpointBinding* Binding = nullptr;
        Adapters::AdapterRouteToken Route{};
        IWebSocketConnection* Connection = nullptr;
        WebSocketConnectionId ConnectionId = 0;
    };

public:
    WebSocketSocketAdapterEndpointBinding() noexcept {
        for (auto& slot : _slots) slot.Binding = this;
    }
    WebSocketSocketAdapterEndpointBinding(const WebSocketSocketAdapterEndpointBinding&) = delete;
    WebSocketSocketAdapterEndpointBinding& operator=(const WebSocketSocketAdapterEndpointBinding&) = delete;
    WebSocketSocketAdapterEndpointBinding(WebSocketSocketAdapterEndpointBinding&&) = delete;
    WebSocketSocketAdapterEndpointBinding& operator=(WebSocketSocketAdapterEndpointBinding&&) = delete;
    ~WebSocketSocketAdapterEndpointBinding() override { Detach(); }

    /// Defines one stable route slot before attaching. The caller subsequently binds
    /// Writer(index) to this exact route in SocketAdapterTransport before Freeze().
    bool ConfigureRoute(std::size_t index, Adapters::AdapterRouteToken route) noexcept {
        if (_endpoint || index >= TMaximumConnections || !route) return false;
        for (std::size_t i = 0; i < TMaximumConnections; ++i) {
            if (i != index && _slots[i].Route.Value == route.Value) return false;
        }
        _slots[index].Route = route;
        return true;
    }

    Sockets::SocketAdapterWriteTarget Writer(std::size_t index) noexcept {
        if (_endpoint || index >= TMaximumConnections || !_slots[index].Route) return {};
        return {&_slots[index], &WebSocketSocketAdapterEndpointBinding::WriteSlot};
    }

    /// Attaches only after all routes are configured and the Sockets transport has
    /// already frozen/started those routes. Existing endpoint connections are rejected
    /// because they could not have passed this binding's selector/admission policy.
    bool Attach(
        WebSocketEndpoint& endpoint,
        TSocketTransport& transport,
        const IWebSocketSocketAdapterSessionSelector& selector
    ) {
        if (_endpoint || endpoint.ConnectionCount() != 0 || !transport.IsActive()) return false;
        for (const auto& slot : _slots) if (!slot.Route) return false;
        _transport = &transport;
        _selector = &selector;
        auto handle = endpoint.RegisterObserver(this);
        if (!handle) {
            _transport = nullptr;
            _selector = nullptr;
            return false;
        }
        _endpoint = &endpoint;
        _observer = std::move(handle);
        return true;
    }

    void Detach() noexcept {
        if (_transport) {
            for (auto& slot : _slots) Deactivate(slot);
        }
        _observer.reset();
        _endpoint = nullptr;
        _transport = nullptr;
        _selector = nullptr;
    }

    bool IsAttached() const noexcept { return _endpoint != nullptr; }
    std::size_t ActiveConnections() const noexcept {
        std::size_t count = 0;
        for (const auto& slot : _slots) if (slot.Connection) ++count;
        return count;
    }

    void OnWebSocketConnected(IWebSocketConnection& connection) override {
        if (!_transport || !_selector || !connection.IsOpen()) return;
        if (Find(connection.Id())) return;
        std::size_t index = TMaximumConnections;
        if (!_selector->SelectSlot(connection, TMaximumConnections, index) ||
            index >= TMaximumConnections || _slots[index].Connection != nullptr) {
            (void)connection.Close({1008, "WebSocket Primitive route not authorized"});
            return;
        }
        auto& slot = _slots[index];
        slot.Connection = &connection;
        slot.ConnectionId = connection.Id();
        if (!_transport->SetSessionAvailable(slot.Route, true)) {
            slot.Connection = nullptr;
            slot.ConnectionId = 0;
            (void)connection.Close({1011, "Primitive transport route unavailable"});
        }
    }

    void OnWebSocketBinary(
        IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) override {
        auto* slot = Find(connection.Id());
        if (!_transport || !slot || slot->Connection != &connection) {
            (void)connection.Close({1008, "WebSocket Primitive route not established"});
            return;
        }
        const auto result = _transport->FeedDatagram(slot->Route, data, size);
        if (result.Status != Sockets::SocketAdapterFeedStatus::Accepted) {
            const auto reason = Detail::CloseReasonForFeed(result.Status);
            Deactivate(*slot);
            (void)connection.Close(reason);
        }
    }

    void OnWebSocketDisconnected(
        WebSocketConnectionId id,
        const WebSocketCloseReason&
    ) override {
        if (auto* slot = Find(id)) Deactivate(*slot);
    }

private:
    static Sockets::SocketAdapterWriteDisposition WriteSlot(
        void* owner,
        const std::uint8_t* data,
        std::size_t size
    ) noexcept {
        auto& slot = *static_cast<Slot*>(owner);
        if (!slot.Binding || !slot.Connection || !slot.Connection->IsOpen()) {
            return Sockets::SocketAdapterWriteDisposition::TemporarilyUnavailable;
        }
        try {
            return Detail::MapWebSocketWriteResult(slot.Connection->SendBinary(data, size));
        } catch (...) {
            return Sockets::SocketAdapterWriteDisposition::ResourceUnavailable;
        }
    }

    Slot* Find(WebSocketConnectionId id) noexcept {
        for (auto& slot : _slots) if (slot.Connection && slot.ConnectionId == id) return &slot;
        return nullptr;
    }

    void Deactivate(Slot& slot) noexcept {
        if (!slot.Connection) return;
        if (_transport) (void)_transport->SetSessionAvailable(slot.Route, false);
        slot.Connection = nullptr;
        slot.ConnectionId = 0;
    }

    std::array<Slot, TMaximumConnections> _slots{};
    WebSocketEndpoint* _endpoint = nullptr;
    TSocketTransport* _transport = nullptr;
    const IWebSocketSocketAdapterSessionSelector* _selector = nullptr;
    Observable::ObserverHandlePtr _observer;
};

/// Fixed single-route bridge from a Web-owned outbound WebSocket client to the neutral
/// Sockets/A2 datagram transport. The caller binds Writer() and semantic provenance to
/// Route() before SocketAdapterTransport::Freeze(); connection/reconnection only toggles
/// that pre-existing session's availability/generation.
template<class TSocketTransport>
class WebSocketSocketAdapterClientBinding final : public IWebSocketClientObserver {
public:
    WebSocketSocketAdapterClientBinding() = default;
    WebSocketSocketAdapterClientBinding(const WebSocketSocketAdapterClientBinding&) = delete;
    WebSocketSocketAdapterClientBinding& operator=(const WebSocketSocketAdapterClientBinding&) = delete;
    WebSocketSocketAdapterClientBinding(WebSocketSocketAdapterClientBinding&&) = delete;
    WebSocketSocketAdapterClientBinding& operator=(WebSocketSocketAdapterClientBinding&&) = delete;
    ~WebSocketSocketAdapterClientBinding() override { Detach(); }

    bool ConfigureRoute(Adapters::AdapterRouteToken route) noexcept {
        if (_client || !route) return false;
        _route = route;
        return true;
    }

    Adapters::AdapterRouteToken Route() const noexcept { return _route; }

    Sockets::SocketAdapterWriteTarget Writer() noexcept {
        if (_client || !_route) return {};
        return {this, &WebSocketSocketAdapterClientBinding::Write};
    }

    bool Attach(WebSocketClient& client, TSocketTransport& transport) {
        if (_client || !_route || !transport.IsActive()) return false;
        _transport = &transport;
        auto handle = client.RegisterObserver(this);
        if (!handle) {
            _transport = nullptr;
            return false;
        }
        _client = &client;
        _observer = std::move(handle);
        if (auto* connection = client.Connection()) Activate(*connection);
        return true;
    }

    void Detach() noexcept {
        Deactivate();
        _observer.reset();
        _client = nullptr;
        _transport = nullptr;
    }

    bool IsAttached() const noexcept { return _client != nullptr; }

    void OnWebSocketClientConnected(IWebSocketConnection& connection) override {
        Activate(connection);
    }

    void OnWebSocketClientBinary(
        IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) override {
        if (!_transport || _connection != &connection) {
            (void)connection.Close({1008, "WebSocket Primitive route not established"});
            return;
        }
        const auto result = _transport->FeedDatagram(_route, data, size);
        if (result.Status != Sockets::SocketAdapterFeedStatus::Accepted) {
            const auto reason = Detail::CloseReasonForFeed(result.Status);
            Deactivate();
            (void)connection.Close(reason);
        }
    }

    void OnWebSocketClientDisconnected(const WebSocketCloseReason&) override {
        Deactivate();
    }

private:
    static Sockets::SocketAdapterWriteDisposition Write(
        void* owner,
        const std::uint8_t* data,
        std::size_t size
    ) noexcept {
        auto& self = *static_cast<WebSocketSocketAdapterClientBinding*>(owner);
        if (!self._connection || !self._connection->IsOpen()) {
            return Sockets::SocketAdapterWriteDisposition::TemporarilyUnavailable;
        }
        try {
            return Detail::MapWebSocketWriteResult(self._connection->SendBinary(data, size));
        } catch (...) {
            return Sockets::SocketAdapterWriteDisposition::ResourceUnavailable;
        }
    }

    void Activate(IWebSocketConnection& connection) noexcept {
        if (!_transport || !connection.IsOpen()) return;
        if (_connection == &connection) return;
        if (_connection) Deactivate();
        _connection = &connection;
        _connectionId = connection.Id();
        if (!_transport->SetSessionAvailable(_route, true)) {
            _connection = nullptr;
            _connectionId = 0;
            (void)connection.Close({1011, "Primitive transport route unavailable"});
        }
    }

    void Deactivate() noexcept {
        if (!_connection) return;
        if (_transport) (void)_transport->SetSessionAvailable(_route, false);
        _connection = nullptr;
        _connectionId = 0;
    }

    Adapters::AdapterRouteToken _route{};
    WebSocketClient* _client = nullptr;
    TSocketTransport* _transport = nullptr;
    IWebSocketConnection* _connection = nullptr;
    WebSocketConnectionId _connectionId = 0;
    Observable::ObserverHandlePtr _observer;
};

} // namespace ESPressio::Web
