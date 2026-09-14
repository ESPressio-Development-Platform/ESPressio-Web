#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include <ESPressio_WebSocketSocketAdapter.hpp>

using namespace ESPressio;

namespace {

struct FakeAdapter final {
    Adapters::AdapterInboundCompletionTarget InboundCompletion{};
    std::uint64_t InboundCorrelation = 0;
    std::size_t AdmitCount = 0;
    std::size_t CompleteCount = 0;
    Adapters::LowerTransportCompletion LastCompletion{};

    Adapters::AdapterSubmissionDisposition AdmitTrustedInbound(
        Primitive::PrimitiveFamilyId,
        Adapters::AdapterServiceClass,
        Primitive::PrimitiveProtocolVersion,
        Adapters::AdapterByteView,
        const Adapters::AdapterSemanticProvenance&,
        Adapters::AdapterRouteToken,
        Primitive::PrimitivePolicyDescriptor,
        std::uint64_t correlation,
        Adapters::AdapterInboundCompletionTarget completion
    ) noexcept {
        ++AdmitCount;
        InboundCorrelation = correlation;
        InboundCompletion = completion;
        return Adapters::AdapterSubmissionDisposition::Accepted;
    }

    Adapters::AdapterSubmissionDisposition CompleteTransport(
        const Adapters::LowerTransportCompletion& completion
    ) noexcept {
        ++CompleteCount;
        LastCompletion = completion;
        return Adapters::AdapterSubmissionDisposition::Accepted;
    }
};

Sockets::SocketAdapterPolicyResolutionStatus ResolvePolicy(
    void*,
    Primitive::PrimitiveFamilyId family,
    Primitive::PrimitiveProtocolVersion protocol,
    Adapters::AdapterServiceClass,
    Adapters::AdapterByteView bytes,
    Primitive::PrimitivePolicyDescriptor& policy
) noexcept {
    if (family != 1 || protocol != 1 || bytes.Data == nullptr || bytes.Size == 0) {
        return Sockets::SocketAdapterPolicyResolutionStatus::Unsupported;
    }
    policy = {};
    policy.Category = 1;
    policy.Evidence = bytes.Data[0] == 0 ? 0 : 1;
    policy.MaximumAttempts = 1;
    return Sockets::SocketAdapterPolicyResolutionStatus::Success;
}

void Wake(void* owner) noexcept { ++*static_cast<std::size_t*>(owner); }

Adapters::AdapterRecordIdentity Record(std::uint64_t generation) {
    Adapters::AdapterRecordIdentity result{};
    result.Direction = Adapters::AdapterDirection::Outbound;
    result.Generation = generation;
    return result;
}

std::vector<std::uint8_t> PrimitiveFrame(std::uint8_t marker = 0) {
    const std::uint8_t payload[]{marker, 0x5A};
    std::vector<std::uint8_t> frame(Sockets::SocketAdapterWire::HeaderBytes + sizeof(payload));
    Sockets::SocketAdapterWire::Header header{};
    header.Kind = Sockets::SocketAdapterFrameKind::Primitive;
    header.Service = Adapters::AdapterServiceClass::BestEffort;
    header.Family = 1;
    header.Protocol = 1;
    header.PayloadBytes = sizeof(payload);
    assert(Sockets::SocketAdapterWire::EncodeHeader(header, frame.data(), frame.size()));
    std::memcpy(frame.data() + Sockets::SocketAdapterWire::HeaderBytes, payload, sizeof(payload));
    return frame;
}

class FakeConnection final : public Web::IWebSocketConnection {
public:
    explicit FakeConnection(Web::WebSocketConnectionId id) : _id(id) {}

    Web::WebSocketConnectionId Id() const noexcept override { return _id; }
    bool IsOpen() const noexcept override { return Open; }

    Web::WebResult SendBinary(const std::uint8_t* data, std::size_t size) override {
        ++SendCalls;
        if (!Open) return Web::WebResult::Failure(Web::WebError::Closed);
        if (NextSendError != Web::WebError::None) {
            const auto error = NextSendError;
            NextSendError = Web::WebError::None;
            return Web::WebResult::Failure(error);
        }
        LastBinary.assign(data, data + size);
        return Web::WebResult::Success();
    }

    Web::WebResult SendText(std::string_view) override {
        return Open ? Web::WebResult::Success() : Web::WebResult::Failure(Web::WebError::Closed);
    }

    Web::WebResult Close(const Web::WebSocketCloseReason& reason = {}) override {
        ++CloseCalls;
        LastCloseCode = reason.Code;
        Open = false;
        return Web::WebResult::Success();
    }

    bool Open = true;
    Web::WebError NextSendError = Web::WebError::None;
    std::size_t SendCalls = 0;
    std::size_t CloseCalls = 0;
    std::uint16_t LastCloseCode = 0;
    std::vector<std::uint8_t> LastBinary;

private:
    Web::WebSocketConnectionId _id;
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
    std::size_t ConnectionCount() const noexcept override { return Count; }
    Web::WebResult BroadcastBinary(const std::uint8_t*, std::size_t) override {
        return Web::WebResult::Success();
    }
    Web::WebResult BroadcastText(std::string_view) override { return Web::WebResult::Success(); }
    Web::WebResult CloseAll(const Web::WebSocketCloseReason& = {}) override {
        return Web::WebResult::Success();
    }

    void EmitConnected(Web::IWebSocketConnection& connection) {
        ++Count;
        if (Sink) Sink->OnPlatformWebSocketConnected(connection);
    }
    void EmitBinary(Web::IWebSocketConnection& connection, const std::uint8_t* data, std::size_t size) {
        if (Sink) Sink->OnPlatformWebSocketBinary(connection, data, size);
    }
    void EmitText(Web::IWebSocketConnection& connection, std::string_view text) {
        if (Sink) Sink->OnPlatformWebSocketText(connection, text);
    }
    void EmitDisconnected(Web::WebSocketConnectionId id) {
        if (Count) --Count;
        if (Sink) Sink->OnPlatformWebSocketDisconnected(id, {1000, "test"});
    }

    Web::IWebSocketEndpointPlatformSink* Sink = nullptr;
    bool Bound = false;
    std::size_t Count = 0;
};

class Selector final : public Web::IWebSocketSocketAdapterSessionSelector {
public:
    bool SelectSlot(
        Web::IWebSocketConnection& connection,
        std::size_t slotCount,
        std::size_t& slotIndex
    ) const noexcept override {
        if (slotCount != 2) return false;
        if (connection.Id() == 101 || connection.Id() == 103) {
            slotIndex = 0;
            return true;
        }
        if (connection.Id() == 102) {
            slotIndex = 1;
            return true;
        }
        return false;
    }
};

class FakeClientPlatform final : public Web::IWebSocketClientPlatform {
public:
    explicit FakeClientPlatform(FakeConnection& connection) : ConnectionValue(connection) {}

    void SetSink(Web::IWebSocketClientPlatformSink* sink) override { Sink = sink; }
    Web::WebResult Connect(const Web::WebSocketClientConfiguration&) override {
        Connected = true;
        if (Sink) Sink->OnPlatformWebSocketClientConnected(ConnectionValue);
        return Web::WebResult::Success();
    }
    Web::WebResult Disconnect(const Web::WebSocketCloseReason& reason = {}) override {
        Connected = false;
        if (Sink) Sink->OnPlatformWebSocketClientDisconnected(reason);
        return Web::WebResult::Success();
    }
    bool IsConnected() const noexcept override { return Connected; }
    Web::IWebSocketConnection* Connection() noexcept override {
        return Connected ? &ConnectionValue : nullptr;
    }
    void EmitBinary(const std::uint8_t* data, std::size_t size) {
        if (Sink) Sink->OnPlatformWebSocketClientBinary(ConnectionValue, data, size);
    }
    void EmitText(std::string_view text) {
        if (Sink) Sink->OnPlatformWebSocketClientText(ConnectionValue, text);
    }

    FakeConnection& ConnectionValue;
    Web::IWebSocketClientPlatformSink* Sink = nullptr;
    bool Connected = false;
};

using EndpointTransport = Sockets::SocketAdapterTransport<FakeAdapter, 2, 2, 2, 256>;
using ClientTransport = Sockets::SocketAdapterTransport<FakeAdapter, 1, 1, 1, 256>;

void TestEndpointUsesFrozenSocketRoutes() {
    FakeAdapter adapter;
    std::size_t wakes = 0;
    int resolverOwner = 0;
    EndpointTransport transport(adapter, {&resolverOwner, &ResolvePolicy}, {&wakes, &Wake});
    Web::WebSocketSocketAdapterEndpointBinding<EndpointTransport, 2> binding;
    assert(binding.ConfigureRoute(0, {11}));
    assert(binding.ConfigureRoute(1, {12}));
    assert(!binding.ConfigureRoute(1, {11}));

    Adapters::AdapterSemanticProvenance provenance{};
    provenance.ImmediatePeer.Token = 77;
    assert(transport.BindSession(
        {11}, Sockets::SocketAdapterSessionMode::Datagram, binding.Writer(0), provenance, false));
    assert(transport.BindSession(
        {12}, Sockets::SocketAdapterSessionMode::Datagram, binding.Writer(1), provenance, false));
    assert(transport.Freeze());
    assert(transport.Start());

    FakeEndpointPlatform platform;
    Web::WebSocketEndpoint endpoint(platform);
    assert(endpoint.Bind({"/primitive", "espressio-a2"}));
    Selector selector;
    assert(binding.Attach(endpoint, transport, selector));

    FakeConnection first(101);
    platform.EmitConnected(first);
    assert(binding.ActiveConnections() == 1);

    Primitive::PrimitivePolicyDescriptor policy{};
    policy.Category = 1;
    policy.MaximumAttempts = 1;
    const std::uint8_t payload[]{0, 0xA5};
    const auto outbound = transport.Submit(
        Record(1), 1, 1, policy, Adapters::AdapterServiceClass::BestEffort,
        {payload, sizeof(payload)}, {11});
    assert(outbound.Disposition == Adapters::LowerTransportDisposition::Accepted);
    assert(!outbound.DeferredCompletion);
    assert(!first.LastBinary.empty());
    assert(Sockets::SocketAdapterWire::Read32(first.LastBinary.data()) == Sockets::SocketAdapterWire::Magic);

    const auto inbound = PrimitiveFrame();
    platform.EmitBinary(first, inbound.data(), inbound.size());
    assert(adapter.AdmitCount == 1);

    platform.EmitText(first, "not-a-Primitive-transport-frame");
    assert(adapter.AdmitCount == 1);

    FakeConnection second(102);
    platform.EmitConnected(second);
    assert(binding.ActiveConnections() == 2);

    FakeConnection occupied(103);
    platform.EmitConnected(occupied);
    assert(!occupied.IsOpen());
    assert(occupied.LastCloseCode == 1008);
    assert(binding.ActiveConnections() == 2);

    const std::uint8_t malformed[]{0x01, 0x02};
    platform.EmitBinary(second, malformed, sizeof(malformed));
    assert(!second.IsOpen());
    assert(second.LastCloseCode == 1002);
    assert(binding.ActiveConnections() == 1);
    const auto unavailable = transport.Submit(
        Record(2), 1, 1, policy, Adapters::AdapterServiceClass::BestEffort,
        {payload, sizeof(payload)}, {12});
    assert(unavailable.Disposition == Adapters::LowerTransportDisposition::TemporarilyUnavailable);

    platform.EmitDisconnected(first.Id());
    assert(binding.ActiveConnections() == 0);
    const auto disconnected = transport.Submit(
        Record(3), 1, 1, policy, Adapters::AdapterServiceClass::BestEffort,
        {payload, sizeof(payload)}, {11});
    assert(disconnected.Disposition == Adapters::LowerTransportDisposition::TemporarilyUnavailable);
}

void TestClientUsesOneFrozenSocketRoute() {
    FakeAdapter adapter;
    std::size_t wakes = 0;
    int resolverOwner = 0;
    ClientTransport transport(adapter, {&resolverOwner, &ResolvePolicy}, {&wakes, &Wake});
    Web::WebSocketSocketAdapterClientBinding<ClientTransport> binding;
    assert(binding.ConfigureRoute({21}));

    Adapters::AdapterSemanticProvenance provenance{};
    provenance.ImmediatePeer.Token = 88;
    assert(transport.BindSession(
        {21}, Sockets::SocketAdapterSessionMode::Datagram, binding.Writer(), provenance, false));
    assert(transport.Freeze());
    assert(transport.Start());

    FakeConnection connection(201);
    FakeClientPlatform platform(connection);
    Web::WebSocketClient client(platform);
    assert(binding.Attach(client, transport));

    Web::WebSocketClientConfiguration configuration{};
    configuration.Host = "example.test";
    configuration.Port = 80;
    configuration.Path = "/primitive";
    assert(client.Connect(configuration));

    Primitive::PrimitivePolicyDescriptor policy{};
    policy.Category = 1;
    policy.MaximumAttempts = 1;
    const std::uint8_t payload[]{0, 0x33};
    const auto outbound = transport.Submit(
        Record(4), 1, 1, policy, Adapters::AdapterServiceClass::BestEffort,
        {payload, sizeof(payload)}, {21});
    assert(outbound.Disposition == Adapters::LowerTransportDisposition::Accepted);
    assert(!connection.LastBinary.empty());

    const auto inbound = PrimitiveFrame();
    platform.EmitBinary(inbound.data(), inbound.size());
    assert(adapter.AdmitCount == 1);
    platform.EmitText("diagnostic-only");
    assert(adapter.AdmitCount == 1);

    assert(client.Disconnect());
    const auto disconnected = transport.Submit(
        Record(5), 1, 1, policy, Adapters::AdapterServiceClass::BestEffort,
        {payload, sizeof(payload)}, {21});
    assert(disconnected.Disposition == Adapters::LowerTransportDisposition::TemporarilyUnavailable);
}

} // namespace

int main() {
    TestEndpointUsesFrozenSocketRoutes();
    TestClientUsesOneFrozenSocketRoute();
    return 0;
}
