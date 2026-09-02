#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ESPressio_Dns.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IDnsRequestPlatform {
public:
    std::string NameValue = "example.test";
    DnsRecordType TypeValue = DnsRecordType::A;
    DnsRecordClass ClassValue = DnsRecordClass::Internet;

    std::string_view Name() const noexcept override { return NameValue; }
    DnsRecordType Type() const noexcept override { return TypeValue; }
    DnsRecordClass Class() const noexcept override { return ClassValue; }
};

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
