#pragma once

#if !__has_include(<ESPressio_SocketClockSynchronizationProtocol.hpp>)
#error "WebSocket clock synchronization requires ESPressio-Sockets clock protocol support."
#endif
#if !__has_include(<ESPressio_PrecisionThread.hpp>)
#error "WebSocket clock synchronization requires ESPressio-Threads for periodic client scheduling."
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_SocketClockSynchronizationProtocol.hpp>

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"

namespace ESPressio::Web {

struct WebSocketClockSynchronizationClientConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
    bool AutomaticSynchronization = true;
};

struct WebSocketClockSynchronizationServerConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
};

class WebSocketClockSynchronizationClient final :
    private IWebSocketClientObserver {
private:
    class SynchronizationThread final : public Threads::PrecisionThread<> {
    public:
        explicit SynchronizationThread(WebSocketClockSynchronizationClient& owner)
            : _owner(owner) {}

    protected:
        void Iterate(
            IterationTime,
            IterationTime,
            Threads::SkippedIterationCount
        ) override {
            (void)_owner.RequestSynchronization();
        }

    private:
        WebSocketClockSynchronizationClient& _owner;
    };

public:
    explicit WebSocketClockSynchronizationClient(
        Timing::IClockSynchronizationTarget<Timing::ClockTick>* target = nullptr
    ) : _protocol(target), _thread(*this) {}

    ~WebSocketClockSynchronizationClient() { Detach(); }

    WebSocketClockSynchronizationClient(const WebSocketClockSynchronizationClient&) = delete;
    WebSocketClockSynchronizationClient& operator=(const WebSocketClockSynchronizationClient&) = delete;

    WebResult Attach(
        WebSocketClient& client,
        WebSocketClockSynchronizationClientConfiguration configuration = {}
    ) {
        if (_client == &client) return WebResult::Success();
        if (_client != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Client;
        _configuration = configuration;
        _protocol.Configure(_configuration.Protocol);
        _client = &client;
        _observerHandle = client.RegisterObserver(this);

        if (
            _configuration.AutomaticSynchronization &&
            _configuration.Protocol.SynchronizationIntervalMilliseconds > 0
        ) {
            _thread.SetIterationPeriod(
                Units::MilliSeconds<uint64_t>(
                    _configuration.Protocol.SynchronizationIntervalMilliseconds
                )
            );
            const auto status = _thread.Start();
            if (
                status != Threads::ThreadInitializationStatus::Success &&
                status != Threads::ThreadInitializationStatus::AlreadyInitialized
            ) {
                Detach();
                return WebResult::Failure(WebError::ResourceExhausted);
            }
        }

        return WebResult::Success();
    }

    void Detach() {
        _thread.Shutdown();
        _observerHandle.reset();
        _client = nullptr;
        _protocol.CancelPendingRequest();
    }

    bool RequestSynchronization() {
        auto* client = _client;
        if (client == nullptr) return false;
        auto* connection = client->Connection();
        if (connection == nullptr || !connection->IsOpen()) return false;

        Sockets::SocketClockSynchronizationProtocol::RequestMessage request;
        if (!_protocol.BuildRequest(request)) return false;

        const auto result = connection->SendBinary(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request)
        );
        if (!result) _protocol.CancelPendingRequest();
        return static_cast<bool>(result);
    }

    Timing::ClockSynchronizationStatus<Timing::ClockTick>
    GetSynchronizationStatus() const {
        return _protocol.GetSynchronizationStatus();
    }

private:
    WebSocketClient* _client = nullptr;
    WebSocketClockSynchronizationClientConfiguration _configuration;
    Sockets::SocketClockSynchronizationProtocol _protocol;
    SynchronizationThread _thread;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketClientBinary(
        IWebSocketConnection&,
        const uint8_t* data,
        std::size_t size
    ) override {
        if (
            data == nullptr ||
            size < sizeof(Sockets::SocketClockSynchronizationProtocol::MessageHeader)
        ) {
            return;
        }

        Sockets::SocketClockSynchronizationProtocol::MessageHeader header;
        std::memcpy(&header, data, sizeof(header));
        if (
            header.Magic != Sockets::SocketClockSynchronizationProtocol::MessageHeader::MagicValue ||
            header.Version != 1
        ) {
            return;
        }

        const uint64_t receiveTime = _protocol.GetLocalTimestamp();
        const auto type = static_cast<Sockets::SocketClockSynchronizationProtocol::MessageType>(
            header.Type
        );

        if (type == Sockets::SocketClockSynchronizationProtocol::MessageType::Response) {
            (void)_protocol.ProcessResponse(data, size, receiveTime);
        } else if (
            type == Sockets::SocketClockSynchronizationProtocol::MessageType::AuthoritativeBroadcast
        ) {
            (void)_protocol.ProcessAuthoritativeBroadcast(data, size, receiveTime);
        }
    }
};

class WebSocketClockSynchronizationServer final :
    private IWebSocketEndpointObserver {
public:
    explicit WebSocketClockSynchronizationServer(
        Timing::IClockSynchronizationTarget<Timing::ClockTick>* target = nullptr
    ) : _protocol(target) {}

    ~WebSocketClockSynchronizationServer() { Detach(); }

    WebSocketClockSynchronizationServer(const WebSocketClockSynchronizationServer&) = delete;
    WebSocketClockSynchronizationServer& operator=(const WebSocketClockSynchronizationServer&) = delete;

    WebResult Attach(
        WebSocketEndpoint& endpoint,
        WebSocketClockSynchronizationServerConfiguration configuration = {}
    ) {
        if (_endpoint == &endpoint) return WebResult::Success();
        if (_endpoint != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Reference;
        _configuration = configuration;
        _protocol.Configure(_configuration.Protocol);
        _endpoint = &endpoint;
        _observerHandle = endpoint.RegisterObserver(this);
        return WebResult::Success();
    }

    void Detach() {
        _observerHandle.reset();
        _endpoint = nullptr;
    }

    Timing::ClockSynchronizationStatus<Timing::ClockTick>
    GetSynchronizationStatus() const {
        return _protocol.GetSynchronizationStatus();
    }

private:
    WebSocketEndpoint* _endpoint = nullptr;
    WebSocketClockSynchronizationServerConfiguration _configuration;
    Sockets::SocketClockSynchronizationProtocol _protocol;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        const uint64_t receiveTime = _protocol.GetLocalTimestamp();
        (void)_protocol.ProcessRequest(
            data,
            size,
            receiveTime,
            [&](const uint8_t* response, std::size_t responseSize) {
                return static_cast<bool>(
                    connection.SendBinary(response, responseSize)
                );
            }
        );
    }
};

} // namespace ESPressio::Web
