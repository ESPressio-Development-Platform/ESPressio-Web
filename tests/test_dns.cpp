#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ESPressio_Dns.hpp>

using namespace ESPressio::Web;

namespace {

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - NameValue (std::string): 24 bytes [Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * - TypeValue (DnsRecordType): 2 bytes [0 bytes dynamic allocation]
 * - ClassValue (DnsRecordClass): 2 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [NameValue: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Request final : public IDnsRequestPlatform {
public:
    std::string NameValue = "example.test";
    DnsRecordType TypeValue = DnsRecordType::A;
    DnsRecordClass ClassValue = DnsRecordClass::Internet;

    std::string_view Name() const noexcept override { return NameValue; }
    DnsRecordType Type() const noexcept override { return TypeValue; }
    DnsRecordClass Class() const noexcept override { return ClassValue; }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - CodeValue (DnsResponseCode): 1 bytes [0 bytes dynamic allocation]
 * - Addresses (std::vector<DnsAddress>): 12 bytes [Capacity * (17 bytes) element storage]
 * - Ttls (std::vector<uint32_t>): 12 bytes [Capacity * (4 bytes) element storage]
 * - Completed (bool): 1 bytes [0 bytes dynamic allocation]
 * - Aborted (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 36 bytes [Addresses: Capacity * (17 bytes) element storage; Ttls: Capacity * (4 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Response final : public IDnsResponsePlatform {
public:
    DnsResponseCode CodeValue = DnsResponseCode::NoError;
    std::vector<DnsAddress> Addresses;
    std::vector<uint32_t> Ttls;
    bool Completed = false;
    bool Aborted = false;

    WebResult SetResponseCode(DnsResponseCode code) override {
        if (Completed || Aborted) return WebResult::Failure(WebError::InvalidState);
        CodeValue = code;
        return WebResult::Success();
    }
    WebResult AddAddressAnswer(const DnsAddress& address, uint32_t ttlSeconds) override {
        if (Completed || Aborted) return WebResult::Failure(WebError::InvalidState);
        Addresses.push_back(address);
        Ttls.push_back(ttlSeconds);
        return WebResult::Success();
    }
    WebResult Complete() override {
        if (Aborted) return WebResult::Failure(WebError::InvalidState);
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override { Aborted = true; }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - CapabilityValue (WebCapabilities): 4 bytes [0 bytes dynamic allocation]
 * - Dispatcher (IDnsRequestDispatcher*): 4 bytes [0 bytes dynamic allocation]
 * - Configuration (DnsServerConfiguration): 8 bytes [0 bytes dynamic allocation]
 * - Starts (int): 4 bytes [0 bytes dynamic allocation]
 * - Stops (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 28 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Platform final : public IDnsServerPlatform {
public:
    WebCapabilities CapabilityValue = ToCapabilities(WebCapability::Dns);
    IDnsRequestDispatcher* Dispatcher = nullptr;
    DnsServerConfiguration Configuration{};
    int Starts = 0;
    int Stops = 0;

    WebCapabilities Capabilities() const noexcept override { return CapabilityValue; }
    WebResult Initialize(
        const DnsServerConfiguration& configuration,
        IDnsRequestDispatcher& dispatcher
    ) override {
        Configuration = configuration;
        Dispatcher = &dispatcher;
        return WebResult::Success();
    }
    WebResult Start() override { ++Starts; return WebResult::Success(); }
    WebResult Stop() override { ++Stops; return WebResult::Success(); }
    void Reset() noexcept override { Dispatcher = nullptr; }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Server (DnsServer*): 4 bytes [0 bytes dynamic allocation]
 * - Notifications (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Observer final : public IDnsServerObserver {
public:
    DnsServer* Server = nullptr;
    int Notifications = 0;

    void OnDnsServerStateChanged(DnsServerState, DnsServerState newState) override {
        ++Notifications;
        assert(Server != nullptr);
        assert(Server->State() == newState);
    }
};

void TestWildcardAndLifecycle() {
    Platform platform;
    DnsServer server(platform);
    WildcardDnsHandler wildcard(DnsAddress::IPv4(192, 168, 4, 1), 30);
    Observer observer;
    observer.Server = &server;
    auto observerHandle = server.RegisterObserver(&observer);

    assert(server.SetRequestHandler(&wildcard));
    assert(server.Initialize());
    assert(server.State() == DnsServerState::Ready);
    assert(server.Start());
    assert(platform.Dispatcher != nullptr);

    Request request;
    Response response;
    assert(platform.Dispatcher->Dispatch(request, response));
    assert(response.Completed);
    assert(response.CodeValue == DnsResponseCode::NoError);
    assert(response.Addresses.size() == 1);
    assert(response.Addresses[0].Family == DnsAddressFamily::IPv4);
    assert(response.Addresses[0].Bytes[0] == 192);
    assert(response.Addresses[0].Bytes[3] == 1);
    assert(response.Ttls[0] == 30);

    assert(server.Stop());
    assert(platform.Starts == 1 && platform.Stops == 1);
    assert(observer.Notifications == 6);
    observerHandle.reset();
}

void TestUnhandledTypeBecomesNameError() {
    Platform platform;
    DnsServer server(platform);
    WildcardDnsHandler wildcard(DnsAddress::IPv4(10, 0, 0, 1));
    assert(server.SetRequestHandler(&wildcard));
    assert(server.Initialize());
    assert(server.Start());

    Request request;
    request.TypeValue = DnsRecordType::Aaaa;
    Response response;
    assert(platform.Dispatcher->Dispatch(request, response));
    assert(response.Completed);
    assert(response.CodeValue == DnsResponseCode::NameError);
    assert(response.Addresses.empty());
    assert(server.Stop());
}

void TestCapabilityAndConfigurationValidation() {
    Platform platform;
    platform.CapabilityValue = 0;
    DnsServer server(platform);
    assert(server.Initialize().Error == WebError::Unsupported);

    platform.CapabilityValue = ToCapabilities(WebCapability::Dns);
    DnsServerConfiguration invalid;
    invalid.Port = 0;
    assert(server.Initialize(invalid).Error == WebError::InvalidConfiguration);
}

} // namespace

int main() {
    TestWildcardAndLifecycle();
    TestUnhandledTypeBecomesNameError();
    TestCapabilityAndConfigurationValidation();
    return 0;
}
