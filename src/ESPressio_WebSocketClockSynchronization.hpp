#pragma once

#if !__has_include(<ESPressio_SocketClockSynchronization.hpp>)
#error "WebSocket clock synchronization requires ESPressio-Sockets clock synchronization support."
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include <ESPressio_SocketClockSynchronization.hpp>

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"

namespace ESPressio::Web {

/// <summary>
/// Optional provider/composition hook for a receive capture that was taken closer
/// to the concrete WebSocket receive boundary than the portable observer callback.
/// </summary>
/// <remarks>
/// The returned quality and uncertainty are Timing-domain evidence and must be
/// truthful. A provider must never label a callback-time software capture as a
/// hardware or bounded capture merely to improve synchronization qualification.
/// </remarks>
using WebSocketClockReceiveCaptureThunk = Timing::ClockTimestampCapture<>(*)(
    void*,
    const IWebSocketConnection&,
    const std::uint8_t*,
    std::size_t
) noexcept;

struct WebSocketClockReceiveCaptureSource final {
    void* Owner = nullptr;
    WebSocketClockReceiveCaptureThunk Capture = nullptr;

    constexpr explicit operator bool() const noexcept {
        return Owner != nullptr && Capture != nullptr;
    }

    Timing::ClockTimestampCapture<> Read(
        const IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) const noexcept {
        return *this ? Capture(Owner, connection, data, size)
                     : Timing::ClockTimestampCapture<>{};
    }
};

struct WebSocketClockSynchronizationClientConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
    WebSocketClockReceiveCaptureSource ReceiveCapture;
};

struct WebSocketClockSynchronizationServerConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
    WebSocketClockReceiveCaptureSource ReceiveCapture;
};

/// <summary>Outcome of one externally driven bounded clock-service quantum.</summary>
enum class WebSocketClockServiceResult : std::uint8_t {
    Idle = 0,
    Busy,
    TimeoutServiced,
    RequestSent,
    SendUnavailable
};

/// <summary>
/// WebSocket client composition for the final Sockets K1/K2 wire and Timing-owned
/// synchronization model.
/// </summary>
/// <remarks>
/// This type owns no worker and no synchronization cadence. The embedding service
/// context calls Service() with the current raw monotonic coordinate. Timing's
/// adaptive evidence deadline decides when a request is due. The nonblocking gate
/// drops a competing service/receive quantum rather than serializing WebSocket
/// callbacks behind an internal blocking lock.
/// </remarks>
class WebSocketClockSynchronizationClient final :
    private IWebSocketClientObserver {
private:
    class ProtocolGuard final {
    public:
        explicit ProtocolGuard(std::atomic_flag& gate) noexcept
            : _gate(&gate),
              _acquired(!gate.test_and_set(std::memory_order_acquire)) {}

        ~ProtocolGuard() {
            if (_acquired) _gate->clear(std::memory_order_release);
        }

        ProtocolGuard(const ProtocolGuard&) = delete;
        ProtocolGuard& operator=(const ProtocolGuard&) = delete;

        explicit operator bool() const noexcept { return _acquired; }

    private:
        std::atomic_flag* _gate;
        bool _acquired;
    };

public:
    explicit WebSocketClockSynchronizationClient(
        Timing::IClockSynchronizationTarget* target = nullptr
    ) noexcept : _protocol(target) {}

    ~WebSocketClockSynchronizationClient() { Detach(); }

    WebSocketClockSynchronizationClient(const WebSocketClockSynchronizationClient&) = delete;
    WebSocketClockSynchronizationClient& operator=(const WebSocketClockSynchronizationClient&) = delete;

    /// <summary>
    /// Attaches the portable WebSocket client and configures the final Sockets V2
    /// protocol. Registration does not create a worker or periodic timer.
    /// </summary>
    WebResult Attach(
        WebSocketClient& client,
        WebSocketClockSynchronizationClientConfiguration configuration = {}
    ) {
        if (_client == &client && _observerHandle) return WebResult::Success();
        if (_client != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Client;
        if (!_protocol.Configure(configuration.Protocol)) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        _configuration = configuration;
        _client = &client;
        _observerHandle = client.RegisterObserver(this);
        if (!_observerHandle) {
            _client = nullptr;
            _protocol.SetReferenceAvailable(false);
            _protocol.CancelPendingRequest();
            return WebResult::Failure(WebError::ResourceExhausted);
        }

        _protocol.SetReferenceAvailable(client.IsConnected());
        return WebResult::Success();
    }

    /// <summary>
    /// Detaches after the embedding context has quiesced concurrent Service calls.
    /// A live reference session loses continuity rather than being silently reused.
    /// </summary>
    void Detach() noexcept {
        const bool hadReference = _client != nullptr && _client->IsConnected();
        _observerHandle.reset();
        _client = nullptr;
        _protocol.SetReferenceAvailable(false);
        _protocol.CancelPendingRequest();
        if (hadReference) _protocol.NotifyReferenceContinuityLost();
    }

    /// <summary>
    /// Executes at most one synchronization service quantum. Timing's adaptive
    /// deadline, not a Web-owned interval, decides whether evidence is due.
    /// </summary>
    WebSocketClockServiceResult Service(std::uint64_t nowMonotonicNanoseconds) noexcept {
        auto* client = _client;
        if (client == nullptr || nowMonotonicNanoseconds == 0) {
            return WebSocketClockServiceResult::Idle;
        }

        ProtocolGuard guard(_protocolGate);
        if (!guard) return WebSocketClockServiceResult::Busy;

        if (_protocol.ServiceTimeout(nowMonotonicNanoseconds)) {
            return WebSocketClockServiceResult::TimeoutServiced;
        }
        if (!_protocol.EvidenceDue(nowMonotonicNanoseconds)) {
            return WebSocketClockServiceResult::Idle;
        }

        auto* connection = client->Connection();
        if (connection == nullptr || !connection->IsOpen()) {
            _protocol.SetReferenceAvailable(false);
            return WebSocketClockServiceResult::SendUnavailable;
        }

        std::array<std::uint8_t, Sockets::SocketClockWireV2::RequestBytes> request{};
        std::size_t requestBytes = 0;
        if (!_protocol.BuildRequest(request.data(), request.size(), requestBytes)) {
            return WebSocketClockServiceResult::Idle;
        }

        const auto sent = connection->SendBinary(request.data(), requestBytes);
        if (!sent) {
            _protocol.CancelPendingRequest();
            return WebSocketClockServiceResult::SendUnavailable;
        }
        return WebSocketClockServiceResult::RequestSent;
    }

    Timing::ClockSynchronizationStatus GetSynchronizationStatus() const noexcept {
        return _protocol.GetSynchronizationStatus();
    }

private:
    Timing::ClockTimestampCapture<> CaptureReceive(
        const IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) const noexcept {
        if (_configuration.ReceiveCapture) {
            return _configuration.ReceiveCapture.Read(connection, data, size);
        }
        // The portable fallback is deliberately truthful: this is a callback-time
        // software capture with unknown latency, never reconstructed historical time.
        return _protocol.CaptureServiceReceive();
    }

    WebSocketClient* _client = nullptr;
    WebSocketClockSynchronizationClientConfiguration _configuration{};
    Sockets::SocketClockSynchronizationProtocol _protocol;
    Observable::ObserverHandlePtr _observerHandle;
    std::atomic_flag _protocolGate = ATOMIC_FLAG_INIT;

    void OnWebSocketClientConnected(IWebSocketConnection& connection) override {
        ProtocolGuard guard(_protocolGate);
        if (guard) _protocol.SetReferenceAvailable(connection.IsOpen());
    }

    void OnWebSocketClientDisconnected(const WebSocketCloseReason&) override {
        ProtocolGuard guard(_protocolGate);
        if (!guard) return;
        _protocol.SetReferenceAvailable(false);
        _protocol.NotifyReferenceContinuityLost();
    }

    void OnWebSocketClientBinary(
        IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) override {
        Sockets::SocketClockWireV2::Header header{};
        if (!Sockets::SocketClockWireV2::DecodeHeader(data, size, header) ||
            header.Type != Sockets::SocketClockWireV2::Kind::Response) {
            return;
        }

        const auto t4 = CaptureReceive(connection, data, size);
        ProtocolGuard guard(_protocolGate);
        if (!guard) return;
        (void)_protocol.ProcessResponse(data, size, t4);
    }
};

/// <summary>
/// WebSocket reference-side composition for the final bounded Sockets V2 K1/K2
/// exchange. Timing owns reference status, capture semantics and qualification.
/// </summary>
class WebSocketClockSynchronizationServer final :
    private IWebSocketEndpointObserver {
private:
    class ProtocolGuard final {
    public:
        explicit ProtocolGuard(std::atomic_flag& gate) noexcept
            : _gate(&gate),
              _acquired(!gate.test_and_set(std::memory_order_acquire)) {}

        ~ProtocolGuard() {
            if (_acquired) _gate->clear(std::memory_order_release);
        }

        ProtocolGuard(const ProtocolGuard&) = delete;
        ProtocolGuard& operator=(const ProtocolGuard&) = delete;
        explicit operator bool() const noexcept { return _acquired; }

    private:
        std::atomic_flag* _gate;
        bool _acquired;
    };

    static bool SendResponse(
        void* owner,
        const std::uint8_t* data,
        std::size_t size
    ) noexcept {
        auto* connection = static_cast<IWebSocketConnection*>(owner);
        return connection != nullptr && connection->IsOpen() &&
               static_cast<bool>(connection->SendBinary(data, size));
    }

public:
    explicit WebSocketClockSynchronizationServer(
        Timing::IClockSynchronizationTarget* target = nullptr
    ) noexcept : _protocol(target) {}

    ~WebSocketClockSynchronizationServer() { Detach(); }

    WebSocketClockSynchronizationServer(const WebSocketClockSynchronizationServer&) = delete;
    WebSocketClockSynchronizationServer& operator=(const WebSocketClockSynchronizationServer&) = delete;

    WebResult Attach(
        WebSocketEndpoint& endpoint,
        WebSocketClockSynchronizationServerConfiguration configuration = {}
    ) {
        if (_endpoint == &endpoint && _observerHandle) return WebResult::Success();
        if (_endpoint != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Reference;
        if (!_protocol.Configure(configuration.Protocol)) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        _configuration = configuration;
        _endpoint = &endpoint;
        _observerHandle = endpoint.RegisterObserver(this);
        if (!_observerHandle) {
            _endpoint = nullptr;
            return WebResult::Failure(WebError::ResourceExhausted);
        }
        return WebResult::Success();
    }

    void Detach() noexcept {
        _observerHandle.reset();
        _endpoint = nullptr;
    }

    Timing::ClockSynchronizationStatus GetSynchronizationStatus() const noexcept {
        return _protocol.GetSynchronizationStatus();
    }

private:
    Timing::ClockTimestampCapture<> CaptureReceive(
        const IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) const noexcept {
        if (_configuration.ReceiveCapture) {
            return _configuration.ReceiveCapture.Read(connection, data, size);
        }
        return _protocol.CaptureServiceReceive();
    }

    WebSocketEndpoint* _endpoint = nullptr;
    WebSocketClockSynchronizationServerConfiguration _configuration{};
    Sockets::SocketClockSynchronizationProtocol _protocol;
    Observable::ObserverHandlePtr _observerHandle;
    std::atomic_flag _protocolGate = ATOMIC_FLAG_INIT;

    void OnWebSocketBinary(
        IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) override {
        Sockets::SocketClockWireV2::Header header{};
        if (!Sockets::SocketClockWireV2::DecodeHeader(data, size, header) ||
            header.Type != Sockets::SocketClockWireV2::Kind::Request) {
            return;
        }

        const auto t2 = CaptureReceive(connection, data, size);
        ProtocolGuard guard(_protocolGate);
        if (!guard) return;
        (void)_protocol.ProcessRequest(
            data,
            size,
            t2,
            {&connection, &WebSocketClockSynchronizationServer::SendResponse}
        );
    }
};

} // namespace ESPressio::Web
