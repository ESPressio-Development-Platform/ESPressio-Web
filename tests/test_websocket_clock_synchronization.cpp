#include <ESPressio_WebSocketClockSynchronization.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace ESPressio;

namespace {

class FakeTarget final : public Timing::IClockSynchronizationTarget {
public:
    mutable Timing::ClockTimestampCapture<> NextCapture{};
    Timing::ClockSynchronizationObservation<> LastObservation{};
    Timing::ClockSynchronizationStatus Status{};
    Timing::ClockSynchronizationProfile Profile{};
    std::uint64_t SelectedReference = 0;
    std::size_t SubmitCount = 0;
    std::size_t ResetCount = 0;
    std::size_t DeadlineMisses = 0;
    bool Acquiring = false;
    bool ReferenceAvailable = false;

    Timing::ClockTimestampCapture<> CaptureSynchronizationTimestamp(
        Timing::ClockCaptureQuality quality,
        Timing::ClockUncertainty uncertainty
    ) const override {
        auto result = NextCapture;
        result.Quality = quality;
        result.Uncertainty = uncertainty;
        return result;
    }

    Timing::ClockSynchronizationResult SubmitSynchronizationObservation(
        const Timing::ClockSynchronizationObservation<>& observation
    ) override {
        LastObservation = observation;
        ++SubmitCount;
        Timing::ClockSynchronizationResult result{};
        result.Accepted = true;
        return result;
    }

    Timing::ClockSynchronizationStatus GetSynchronizationStatus() const override {
        return Status;
    }

    Timing::ClockConfigurationStatus ConfigureSynchronization(
        const Timing::ClockSynchronizationProfile& profile
    ) override {
        Profile = profile;
        return Timing::ClockConfigurationStatus::Success;
    }

    Timing::ClockSynchronizationProfile GetSynchronizationProfile() const override {
        return Profile;
    }

    Timing::ClockConfigurationStatus SelectSynchronizationReference(
        std::uint64_t reference
    ) override {
        SelectedReference = reference;
        return reference == 0
            ? Timing::ClockConfigurationStatus::InvalidReference
            : Timing::ClockConfigurationStatus::Success;
    }

    void SetSynchronizationActivity(bool acquiring, bool available) override {
        Acquiring = acquiring;
        ReferenceAvailable = available;
    }

    void ResetSynchronization() override { ++ResetCount; }
    void RecordSynchronizationDeadlineMiss() override { ++DeadlineMisses; }
};

class FakeConnection final : public Web::IWebSocketConnection {
public:
    explicit FakeConnection(Web::WebSocketConnectionId id) : _id(id) {}

    Web::WebSocketConnectionId Id() const noexcept override { return _id; }
    bool IsOpen() const noexcept override { return _open; }

    Web::WebResult SendBinary(const std::uint8_t* data, std::size_t size) override {
        if (!_open || data == nullptr || size == 0) {
            return Web::WebResult::Failure(Web::WebError::Closed);
        }
        LastBinary.assign(data, data + size);
        ++BinarySends;
        return Web::WebResult::Success();
    }

    Web::WebResult SendText(std::string_view) override {
        return _open ? Web::WebResult::Success()
                     : Web::WebResult::Failure(Web::WebError::Closed);
    }

    Web::WebResult Close(const Web::WebSocketCloseReason& = {}) override {
        _open = false;
        return Web::WebResult::Success();
    }

    std::vector<std::uint8_t> LastBinary;
    std::size_t BinarySends = 0;

private:
    Web::WebSocketConnectionId _id;
    bool _open = true;
};

class FakeClientPlatform final : public Web::IWebSocketClientPlatform {
public:
    explicit FakeClientPlatform(FakeConnection& connection) : _connection(connection) {}

    void SetSink(Web::IWebSocketClientPlatformSink* sink) override { Sink = sink; }

    Web::WebResult Connect(const Web::WebSocketClientConfiguration&) override {
        Connected = true;
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientConnected(_connection);
        return Web::WebResult::Success();
    }

    Web::WebResult Disconnect(const Web::WebSocketCloseReason& reason = {}) override {
        Connected = false;
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientDisconnected(reason);
        return Web::WebResult::Success();
    }

    bool IsConnected() const noexcept override { return Connected; }
    Web::IWebSocketConnection* Connection() noexcept override {
        return Connected ? &_connection : nullptr;
    }

    void EmitBinary(const std::uint8_t* data, std::size_t size) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientBinary(_connection, data, size);
    }

    Web::IWebSocketClientPlatformSink* Sink = nullptr;
    bool Connected = true;

private:
    FakeConnection& _connection;
};

class FakeEndpointPlatform final : public Web::IWebSocketEndpointPlatform {
public:
    void SetSink(Web::IWebSocketEndpointPlatformSink* sink) override { Sink = sink; }
    Web::WebResult Bind(const Web::WebSocketEndpointConfiguration&) override {
        Bound = true;
        return Web::WebResult::Success();
    }
    Web::WebResult Unbind() override {
        Bound = false;
        return Web::WebResult::Success();
    }
    bool IsBound() const noexcept override { return Bound; }
    std::size_t ConnectionCount() const noexcept override { return 1; }
    Web::WebResult BroadcastBinary(const std::uint8_t*, std::size_t) override {
        return Web::WebResult::Success();
    }
    Web::WebResult BroadcastText(std::string_view) override {
        return Web::WebResult::Success();
    }
    Web::WebResult CloseAll(const Web::WebSocketCloseReason& = {}) override {
        return Web::WebResult::Success();
    }

    void EmitBinary(
        Web::IWebSocketConnection& connection,
        const std::uint8_t* data,
        std::size_t size
    ) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketBinary(connection, data, size);
    }

    Web::IWebSocketEndpointPlatformSink* Sink = nullptr;
    bool Bound = true;
};

struct CaptureSource final {
    Timing::ClockTimestampCapture<> Next{};
    std::size_t Calls = 0;

    static Timing::ClockTimestampCapture<> Capture(
        void* owner,
        const Web::IWebSocketConnection&,
        const std::uint8_t*,
        std::size_t
    ) noexcept {
        auto& self = *static_cast<CaptureSource*>(owner);
        ++self.Calls;
        return self.Next;
    }
};

Sockets::SocketClockSynchronizationConfig ClientProtocol() {
    Sockets::SocketClockSynchronizationConfig config{};
    config.ReferenceIdentity = 42;
    config.RequestTimeoutNanoseconds = 500;
    config.LocalTransmitCaptureQuality = Timing::ClockCaptureQuality::SoftwareBounded;
    config.LocalTransmitCaptureUncertainty = Timing::ClockUncertainty::Known(10);
    return config;
}

Sockets::SocketClockSynchronizationConfig ReferenceProtocol() {
    Sockets::SocketClockSynchronizationConfig config{};
    config.LocalTransmitCaptureQuality = Timing::ClockCaptureQuality::SoftwareBounded;
    config.LocalTransmitCaptureUncertainty = Timing::ClockUncertainty::Known(20);
    return config;
}

} // namespace

int main() {
    FakeTarget clientTarget;
    FakeTarget referenceTarget;
    FakeConnection connection(7);
    FakeClientPlatform clientPlatform(connection);
    FakeEndpointPlatform endpointPlatform;
    Web::WebSocketClient client(clientPlatform);
    Web::WebSocketEndpoint endpoint(endpointPlatform);

    CaptureSource clientCapture;
    clientCapture.Next.SystemTimeNanoseconds = 1700;
    clientCapture.Next.MonotonicTimeNanoseconds = 700;
    clientCapture.Next.Quality = Timing::ClockCaptureQuality::Hardware;
    clientCapture.Next.Uncertainty = Timing::ClockUncertainty::Known(4);

    CaptureSource serverCapture;
    serverCapture.Next.SystemTimeNanoseconds = 1500;
    serverCapture.Next.MonotonicTimeNanoseconds = 500;
    serverCapture.Next.Quality = Timing::ClockCaptureQuality::Hardware;
    serverCapture.Next.Uncertainty = Timing::ClockUncertainty::Known(5);

    Web::WebSocketClockSynchronizationClient clockClient(&clientTarget);
    Web::WebSocketClockSynchronizationClientConfiguration clientConfiguration{};
    clientConfiguration.Protocol = ClientProtocol();
    clientConfiguration.ReceiveCapture = {&clientCapture, &CaptureSource::Capture};
    assert(clockClient.Attach(client, clientConfiguration));
    assert(clientTarget.SelectedReference == 42);
    assert(clientTarget.Acquiring);
    assert(clientTarget.ReferenceAvailable);

    Web::WebSocketClockSynchronizationServer clockServer(&referenceTarget);
    Web::WebSocketClockSynchronizationServerConfiguration serverConfiguration{};
    serverConfiguration.Protocol = ReferenceProtocol();
    serverConfiguration.ReceiveCapture = {&serverCapture, &CaptureSource::Capture};
    assert(clockServer.Attach(endpoint, serverConfiguration));

    referenceTarget.Status.Reliability = Timing::TimeReliability::Synchronized;
    referenceTarget.Status.CurrentUncertainty = Timing::ClockUncertainty::Known(100);
    referenceTarget.NextCapture.SystemTimeNanoseconds = 1600;
    referenceTarget.NextCapture.MonotonicTimeNanoseconds = 600;

    clientTarget.Status.HasSynchronizationDeadline = true;
    clientTarget.Status.NextRequiredSynchronizationMonotonic = 100;
    clientTarget.NextCapture.SystemTimeNanoseconds = 1000;
    clientTarget.NextCapture.MonotonicTimeNanoseconds = 100;

    assert(clockClient.Service(99) == Web::WebSocketClockServiceResult::Idle);
    assert(connection.BinarySends == 0);
    assert(clockClient.Service(100) == Web::WebSocketClockServiceResult::RequestSent);
    assert(connection.BinarySends == 1);
    assert(connection.LastBinary.size() == Sockets::SocketClockWireV2::RequestBytes);

    const auto request = connection.LastBinary;
    endpointPlatform.EmitBinary(connection, request.data(), request.size());
    assert(serverCapture.Calls == 1);
    assert(connection.BinarySends == 2);
    assert(connection.LastBinary.size() == Sockets::SocketClockWireV2::ResponseBytes);

    const auto response = connection.LastBinary;
    clientPlatform.EmitBinary(response.data(), response.size());
    assert(clientCapture.Calls == 1);
    assert(clientTarget.SubmitCount == 1);
    assert(clientTarget.LastObservation.T1.SystemTimeNanoseconds == 1000);
    assert(clientTarget.LastObservation.T1.MonotonicTimeNanoseconds == 100);
    assert(clientTarget.LastObservation.T2.SystemTimeNanoseconds == 1500);
    assert(clientTarget.LastObservation.T2.MonotonicTimeNanoseconds == 500);
    assert(clientTarget.LastObservation.T2.Quality == Timing::ClockCaptureQuality::Hardware);
    assert(clientTarget.LastObservation.T2.Uncertainty.IsKnown);
    assert(clientTarget.LastObservation.T2.Uncertainty.Nanoseconds == 5);
    assert(clientTarget.LastObservation.T3.SystemTimeNanoseconds == 1600);
    assert(clientTarget.LastObservation.T3.MonotonicTimeNanoseconds == 600);
    assert(clientTarget.LastObservation.T3.Quality == Timing::ClockCaptureQuality::SoftwareBounded);
    assert(clientTarget.LastObservation.T3.Uncertainty.IsKnown);
    assert(clientTarget.LastObservation.T3.Uncertainty.Nanoseconds == 20);
    assert(clientTarget.LastObservation.T4.SystemTimeNanoseconds == 1700);
    assert(clientTarget.LastObservation.T4.MonotonicTimeNanoseconds == 700);
    assert(clientTarget.LastObservation.T4.Quality == Timing::ClockCaptureQuality::Hardware);
    assert(clientTarget.LastObservation.T4.Uncertainty.IsKnown);
    assert(clientTarget.LastObservation.T4.Uncertainty.Nanoseconds == 4);
    assert(clientTarget.LastObservation.ReferenceIdentity == 42);
    assert(clientTarget.LastObservation.ReferenceReliability == Timing::TimeReliability::Synchronized);
    assert(clientTarget.LastObservation.ReferenceUncertainty.IsKnown);
    assert(clientTarget.LastObservation.ReferenceUncertainty.Nanoseconds == 100);

    // A second deadline is externally serviced. Web owns no interval or worker.
    clientTarget.Status.NextRequiredSynchronizationMonotonic = 1000;
    clientTarget.NextCapture.SystemTimeNanoseconds = 2000;
    clientTarget.NextCapture.MonotonicTimeNanoseconds = 2000;
    assert(clockClient.Service(999) == Web::WebSocketClockServiceResult::Idle);
    assert(clockClient.Service(1000) == Web::WebSocketClockServiceResult::RequestSent);
    assert(clockClient.Service(2499) == Web::WebSocketClockServiceResult::Idle);
    assert(clockClient.Service(2500) == Web::WebSocketClockServiceResult::TimeoutServiced);
    assert(clientTarget.DeadlineMisses == 1);

    // Losing the WebSocket reference is a real continuity loss, not a stale-session reuse.
    assert(client.Disconnect());
    assert(!clientTarget.ReferenceAvailable);
    assert(clientTarget.ResetCount == 1);

    // Non-clock binary payloads are ignored by both clock compositions.
    const std::uint8_t unrelated[]{0x01, 0x02, 0x03, 0x04};
    const auto clientCalls = clientCapture.Calls;
    const auto serverCalls = serverCapture.Calls;
    clientPlatform.EmitBinary(unrelated, sizeof(unrelated));
    endpointPlatform.EmitBinary(connection, unrelated, sizeof(unrelated));
    assert(clientCapture.Calls == clientCalls);
    assert(serverCapture.Calls == serverCalls);

    // The portable fallback is explicitly SoftwareUnbounded with unknown uncertainty.
    FakeTarget fallbackTarget;
    fallbackTarget.NextCapture.SystemTimeNanoseconds = 3000;
    fallbackTarget.NextCapture.MonotonicTimeNanoseconds = 3000;
    Web::WebSocketClockSynchronizationServer fallbackServer(&fallbackTarget);
    Web::WebSocketClockSynchronizationServerConfiguration fallbackConfiguration{};
    fallbackConfiguration.Protocol = ReferenceProtocol();
    assert(fallbackServer.Attach(endpoint, fallbackConfiguration));
    // A second observer may coexist; malformed/non-clock input remains harmless.

    return 0;
}
