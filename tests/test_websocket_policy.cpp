#include <cassert>
#include <cstddef>
#include <string_view>
#include <vector>

#include <ESPressio_Web.hpp>

using namespace ESPressio::Web;

namespace {

class FakeConnection final : public IWebSocketConnection {
public:
    WebSocketConnectionId Id() const noexcept override { return 1; }
    bool IsOpen() const noexcept override { return true; }
    WebResult SendBinary(const uint8_t*, std::size_t) override { return WebResult::Success(); }
    WebResult SendText(std::string_view) override { return WebResult::Success(); }
    WebResult Close(const WebSocketCloseReason& = {}) override { return WebResult::Success(); }
};

class FakeClientPlatform final : public IWebSocketClientPlatform {
public:
    void SetSink(IWebSocketClientPlatformSink* sink) override { Sink = sink; }

    WebResult Connect(const WebSocketClientConfiguration&) override {
        ++ConnectCalls;
        return WebResult::Success();
    }

    WebResult Disconnect(const WebSocketCloseReason& = {}) override {
        return WebResult::Success();
    }

    bool IsConnected() const noexcept override { return false; }
    IWebSocketConnection* Connection() noexcept override { return &ConnectionValue; }

    IWebSocketClientPlatformSink* Sink = nullptr;
    int ConnectCalls = 0;
    FakeConnection ConnectionValue;
};

class HeaderSource final : public IWebClientHeaderSource {
public:
    std::size_t Count() const noexcept override { return Headers.size(); }

    bool Header(std::size_t index, WebClientHeader& header) const noexcept override {
        if (index >= Headers.size()) return false;
        header = Headers[index];
        return true;
    }

    std::vector<WebClientHeader> Headers;
};

WebSocketClientConfiguration BaseConfiguration() {
    WebSocketClientConfiguration configuration;
    configuration.Host = "device.local";
    configuration.Port = 80;
    configuration.Path = "/socket";
    return configuration;
}

void TestReservedHandshakeHeadersAreRejected() {
    FakeClientPlatform platform;
    WebSocketClient client(platform);

    static constexpr std::string_view reserved[] = {
        "Host",
        "Connection",
        "Upgrade",
        "Sec-WebSocket-Key",
        "Sec-WebSocket-Version",
        "Sec-WebSocket-Protocol",
        "sec-websocket-protocol"
    };

    for (const auto name : reserved) {
        HeaderSource headers;
        headers.Headers.push_back({name, "application-value"});
        auto configuration = BaseConfiguration();
        configuration.Headers = &headers;
        const auto result = client.Connect(configuration);
        assert(result.Error == WebError::InvalidConfiguration);
        assert(platform.ConnectCalls == 0);
    }

    HeaderSource allowed;
    allowed.Headers.push_back({"Authorization", "Bearer token"});
    allowed.Headers.push_back({"X-Application", "ESPressio"});
    auto configuration = BaseConfiguration();
    configuration.Headers = &allowed;
    assert(client.Connect(configuration));
    assert(platform.ConnectCalls == 1);
}

void TestTcpKeepAliveValidation() {
    FakeClientPlatform platform;
    WebSocketClient client(platform);

    auto invalid = BaseConfiguration();
    invalid.Policy.TcpKeepAlive = true;
    invalid.Policy.TcpKeepAliveIdleSeconds = 0;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = BaseConfiguration();
    invalid.Policy.TcpKeepAlive = true;
    invalid.Policy.TcpKeepAliveIntervalSeconds = 0;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = BaseConfiguration();
    invalid.Policy.TcpKeepAlive = true;
    invalid.Policy.TcpKeepAliveProbeCount = 0;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    auto valid = BaseConfiguration();
    valid.Policy.TcpKeepAlive = true;
    valid.Policy.TcpKeepAliveIdleSeconds = 30;
    valid.Policy.TcpKeepAliveIntervalSeconds = 10;
    valid.Policy.TcpKeepAliveProbeCount = 3;
    assert(client.Connect(valid));
    assert(platform.ConnectCalls == 1);
}

void TestCleanCloseReconnectPolicyIsPortable() {
    FakeClientPlatform platform;
    WebSocketClient client(platform);

    auto configuration = BaseConfiguration();
    configuration.Policy.AutomaticReconnect = true;
    configuration.Policy.ReconnectAfterCleanClose = true;
    configuration.Policy.ReconnectDelayMilliseconds = 10000;

    // The portable facade accepts this semantic. A concrete platform that
    // cannot represent it (such as the pinned ESP-IDF 4.4 provider) must
    // return Unsupported rather than silently discard the policy.
    assert(client.Connect(configuration));
    assert(platform.ConnectCalls == 1);
}

} // namespace

int main() {
    TestReservedHandshakeHeadersAreRejected();
    TestTcpKeepAliveValidation();
    TestCleanCloseReconnectPolicyIsPortable();
    return 0;
}
