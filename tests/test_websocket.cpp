#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ESPressio_Web.hpp>

using namespace ESPressio::Web;

namespace {

class FakeConnection final : public IWebSocketConnection {
public:
    explicit FakeConnection(WebSocketConnectionId id) : _id(id) {}

    WebSocketConnectionId Id() const noexcept override { return _id; }
    bool IsOpen() const noexcept override { return _open; }

    WebResult SendBinary(const uint8_t* data, std::size_t size) override {
        if (!_open || data == nullptr || size == 0) return WebResult::Failure(WebError::Closed);
        LastBinary.assign(data, data + size);
        return WebResult::Success();
    }

    WebResult SendText(std::string_view text) override {
        if (!_open) return WebResult::Failure(WebError::Closed);
        LastText.assign(text.data(), text.size());
        return WebResult::Success();
    }

    WebResult Close(const WebSocketCloseReason& reason = {}) override {
        LastCloseCode = reason.Code;
        LastCloseReason.assign(reason.Reason.data(), reason.Reason.size());
        _open = false;
        return WebResult::Success();
    }

    std::vector<uint8_t> LastBinary;
    std::string LastText;
    uint16_t LastCloseCode = 0;
    std::string LastCloseReason;

private:
    WebSocketConnectionId _id;
    bool _open = true;
};

class FakeEndpointPlatform final : public IWebSocketEndpointPlatform {
public:
    void SetSink(IWebSocketEndpointPlatformSink* sink) override { Sink = sink; }

    WebResult Bind(const WebSocketEndpointConfiguration& configuration) override {
        if (Bound) return WebResult::Failure(WebError::AlreadyRunning);
        if (configuration.Path.empty()) return WebResult::Failure(WebError::InvalidConfiguration);
        LastPath.assign(configuration.Path.data(), configuration.Path.size());
        LastProtocol.assign(configuration.Protocol.data(), configuration.Protocol.size());
        Bound = true;
        ++BindCalls;
        return WebResult::Success();
    }

    WebResult Unbind() override {
        if (Bound) ++UnbindCalls;
        Bound = false;
        return WebResult::Success();
    }

    bool IsBound() const noexcept override { return Bound; }
    std::size_t ConnectionCount() const noexcept override { return Count; }

    WebResult BroadcastBinary(const uint8_t* data, std::size_t size) override {
        ++BinaryBroadcasts;
        if (data == nullptr || size == 0) return WebResult::Failure(WebError::InvalidConfiguration);
        LastBinary.assign(data, data + size);
        return WebResult::Success();
    }

    WebResult BroadcastText(std::string_view text) override {
        ++TextBroadcasts;
        LastText.assign(text.data(), text.size());
        return WebResult::Success();
    }

    WebResult CloseAll(const WebSocketCloseReason& reason = {}) override {
        ++CloseAllCalls;
        LastCloseCode = reason.Code;
        return WebResult::Success();
    }

    void EmitConnected(IWebSocketConnection& connection) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketConnected(connection);
    }

    void EmitBinary(IWebSocketConnection& connection, const uint8_t* data, std::size_t size) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketBinary(connection, data, size);
    }

    void EmitText(IWebSocketConnection& connection, std::string_view text) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketText(connection, text);
    }

    void EmitDisconnected(WebSocketConnectionId id, const WebSocketCloseReason& reason) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketDisconnected(id, reason);
    }

    IWebSocketEndpointPlatformSink* Sink = nullptr;
    bool Bound = false;
    std::size_t Count = 1;
    int BindCalls = 0;
    int UnbindCalls = 0;
    int BinaryBroadcasts = 0;
    int TextBroadcasts = 0;
    int CloseAllCalls = 0;
    uint16_t LastCloseCode = 0;
    std::string LastPath;
    std::string LastProtocol;
    std::vector<uint8_t> LastBinary;
    std::string LastText;
};

class EndpointObserver final : public IWebSocketEndpointObserver {
public:
    void OnWebSocketConnected(IWebSocketConnection& connection) override {
        ++Connected;
        LastConnection = connection.Id();
    }

    void OnWebSocketBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        ++Binary;
        LastConnection = connection.Id();
        LastBinary.assign(data, data + size);
    }

    void OnWebSocketText(IWebSocketConnection& connection, std::string_view text) override {
        ++Text;
        LastConnection = connection.Id();
        LastText.assign(text.data(), text.size());
    }

    void OnWebSocketDisconnected(
        WebSocketConnectionId id,
        const WebSocketCloseReason& reason
    ) override {
        ++Disconnected;
        LastConnection = id;
        LastCloseCode = reason.Code;
    }

    int Connected = 0;
    int Binary = 0;
    int Text = 0;
    int Disconnected = 0;
    WebSocketConnectionId LastConnection = 0;
    uint16_t LastCloseCode = 0;
    std::vector<uint8_t> LastBinary;
    std::string LastText;
};

class FakeHeaderSource final : public IWebClientHeaderSource {
public:
    std::size_t Count() const noexcept override { return Headers.size(); }

    bool Header(std::size_t index, WebClientHeader& header) const noexcept override {
        if (index >= Headers.size()) return false;
        header = Headers[index];
        return true;
    }

    std::vector<WebClientHeader> Headers;
};

class FakeClientPlatform final : public IWebSocketClientPlatform {
public:
    explicit FakeClientPlatform(FakeConnection& connection) : _connection(connection) {}

    void SetSink(IWebSocketClientPlatformSink* sink) override { Sink = sink; }

    WebResult Connect(const WebSocketClientConfiguration& configuration) override {
        ++ConnectCalls;
        LastHost.assign(configuration.Host.data(), configuration.Host.size());
        LastPath.assign(configuration.Path.data(), configuration.Path.size());
        LastPort = configuration.Port;
        LastTransport = configuration.Transport;
        Connected = true;
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientConnected(_connection);
        return WebResult::Success();
    }

    WebResult Disconnect(const WebSocketCloseReason& reason = {}) override {
        Connected = false;
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientDisconnected(reason);
        return WebResult::Success();
    }

    bool IsConnected() const noexcept override { return Connected; }
    IWebSocketConnection* Connection() noexcept override { return Connected ? &_connection : nullptr; }

    void EmitBinary(const uint8_t* data, std::size_t size) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientBinary(_connection, data, size);
    }

    void EmitText(std::string_view text) {
        if (Sink != nullptr) Sink->OnPlatformWebSocketClientText(_connection, text);
    }

    IWebSocketClientPlatformSink* Sink = nullptr;
    bool Connected = false;
    int ConnectCalls = 0;
    uint16_t LastPort = 0;
    WebTransportMode LastTransport = WebTransportMode::Plain;
    std::string LastHost;
    std::string LastPath;

private:
    FakeConnection& _connection;
};

class ClientObserver final : public IWebSocketClientObserver {
public:
    void OnWebSocketClientConnected(IWebSocketConnection& connection) override {
        ++Connected;
        LastConnection = connection.Id();
    }

    void OnWebSocketClientBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        ++Binary;
        LastConnection = connection.Id();
        LastBinary.assign(data, data + size);
    }

    void OnWebSocketClientText(IWebSocketConnection& connection, std::string_view text) override {
        ++Text;
        LastConnection = connection.Id();
        LastText.assign(text.data(), text.size());
    }

    void OnWebSocketClientDisconnected(const WebSocketCloseReason& reason) override {
        ++Disconnected;
        LastCloseCode = reason.Code;
    }

    int Connected = 0;
    int Binary = 0;
    int Text = 0;
    int Disconnected = 0;
    WebSocketConnectionId LastConnection = 0;
    uint16_t LastCloseCode = 0;
    std::vector<uint8_t> LastBinary;
    std::string LastText;
};

void TestEndpointFacade() {
    FakeEndpointPlatform platform;
    FakeConnection connection(42);
    EndpointObserver observer;

    WebSocketEndpoint endpoint;
    assert(endpoint.Attach(platform));
    assert(platform.Sink != nullptr);
    assert(!endpoint.IsBound());
    assert(!endpoint.Bind({"invalid", {}}));
    assert(endpoint.Bind({"/application/socket", "espressio.v1"}));
    assert(endpoint.IsBound());
    assert(platform.BindCalls == 1);
    assert(platform.LastPath == "/application/socket");
    assert(platform.LastProtocol == "espressio.v1");
    assert(endpoint.ConnectionCount() == 1);

    auto observerHandle = endpoint.RegisterObserver(&observer);
    assert(observerHandle != nullptr);

    platform.EmitConnected(connection);
    assert(observer.Connected == 1);
    assert(observer.LastConnection == 42);

    const uint8_t inbound[] = {1, 2, 3};
    platform.EmitBinary(connection, inbound, sizeof(inbound));
    assert(observer.Binary == 1);
    assert(observer.LastBinary == std::vector<uint8_t>({1, 2, 3}));

    platform.EmitText(connection, "hello");
    assert(observer.Text == 1);
    assert(observer.LastText == "hello");

    const uint8_t outbound[] = {9, 8};
    assert(endpoint.BroadcastBinary(outbound, sizeof(outbound)));
    assert(platform.BinaryBroadcasts == 1);
    assert(platform.LastBinary == std::vector<uint8_t>({9, 8}));

    assert(endpoint.BroadcastText("world"));
    assert(platform.TextBroadcasts == 1);
    assert(platform.LastText == "world");

    assert(endpoint.CloseAll({1001, "shutdown"}));
    assert(platform.CloseAllCalls == 1);
    assert(platform.LastCloseCode == 1001);

    platform.EmitDisconnected(42, {1000, "done"});
    assert(observer.Disconnected == 1);
    assert(observer.LastCloseCode == 1000);

    observerHandle.reset();
    platform.EmitText(connection, "not observed");
    assert(observer.Text == 1);

    assert(endpoint.Unbind());
    assert(!endpoint.IsBound());
    assert(platform.UnbindCalls == 1);

    endpoint.Detach();
    assert(platform.Sink == nullptr);
    assert(!endpoint.IsAttached());
}

void TestClientFacade() {
    FakeConnection connection(99);
    FakeClientPlatform platform(connection);
    ClientObserver observer;

    WebSocketClient client(platform);
    auto observerHandle = client.RegisterObserver(&observer);
    assert(observerHandle != nullptr);

    WebSocketClientConfiguration configuration;
    configuration.Host = "device.local";
    configuration.Port = 8080;
    configuration.Path = "/custom/socket";

    assert(client.Connect(configuration));
    assert(client.IsConnected());
    assert(platform.ConnectCalls == 1);
    assert(platform.LastHost == "device.local");
    assert(platform.LastPort == 8080);
    assert(platform.LastPath == "/custom/socket");
    assert(platform.LastTransport == WebTransportMode::Plain);
    assert(observer.Connected == 1);
    assert(observer.LastConnection == 99);

    const uint8_t inbound[] = {4, 5, 6};
    platform.EmitBinary(inbound, sizeof(inbound));
    assert(observer.Binary == 1);
    assert(observer.LastBinary == std::vector<uint8_t>({4, 5, 6}));

    platform.EmitText("client text");
    assert(observer.Text == 1);
    assert(observer.LastText == "client text");

    auto* liveConnection = client.Connection();
    assert(liveConnection == &connection);
    const uint8_t outbound[] = {7};
    assert(liveConnection->SendBinary(outbound, sizeof(outbound)));
    assert(connection.LastBinary == std::vector<uint8_t>({7}));

    assert(client.Disconnect({1000, "done"}));
    assert(!client.IsConnected());
    assert(observer.Disconnected == 1);
    assert(observer.LastCloseCode == 1000);
}

void TestClientConfigurationValidation() {
    FakeConnection connection(100);
    FakeClientPlatform platform(connection);
    WebSocketClient client(platform);

    WebSocketClientConfiguration configuration;
    configuration.Host = "secure.example";
    configuration.Port = 443;
    configuration.Path = "/socket";

    auto invalid = configuration;
    invalid.Host = {};
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = configuration;
    invalid.Path = "relative";
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = configuration;
    invalid.Policy.NetworkTimeoutMilliseconds = 0;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = configuration;
    invalid.Policy.AutomaticReconnect = true;
    invalid.Policy.ReconnectDelayMilliseconds = 0;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    static constexpr uint8_t Ca[] = "-----BEGIN CERTIFICATE-----\nCA\n-----END CERTIFICATE-----\n";
    static constexpr uint8_t ClientCertificate[] = "client certificate";
    static constexpr uint8_t ClientKey[] = "client key";

    invalid = configuration;
    invalid.Transport = WebTransportMode::Tls;
    invalid.Tls.ServerTrust = WebTlsServerTrustMode::CertificateAuthority;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = configuration;
    invalid.Transport = WebTransportMode::Tls;
    invalid.Tls.ClientCertificate = {ClientCertificate, sizeof(ClientCertificate)};
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    invalid = configuration;
    invalid.Tls.ServerCertificateAuthority = {Ca, sizeof(Ca)};
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    FakeHeaderSource invalidHeaders;
    invalidHeaders.Headers.push_back({"X-Test", "value\r\nInjected: true"});
    invalid = configuration;
    invalid.Headers = &invalidHeaders;
    assert(client.Connect(invalid).Error == WebError::InvalidConfiguration);
    assert(platform.ConnectCalls == 0);

    FakeHeaderSource oversizedHeaders;
    oversizedHeaders.Headers.push_back({"X-Large", "1234567890"});
    invalid = configuration;
    invalid.Headers = &oversizedHeaders;
    invalid.Policy.MaximumHandshakeHeaderBytes = 8;
    assert(client.Connect(invalid).Error == WebError::ResourceExhausted);
    assert(platform.ConnectCalls == 0);

    FakeHeaderSource validHeaders;
    validHeaders.Headers.push_back({"Authorization", "Bearer token"});
    validHeaders.Headers.push_back({"X-Application", "ESPressio"});

    auto secure = configuration;
    secure.Transport = WebTransportMode::Tls;
    secure.Tls.ServerTrust = WebTlsServerTrustMode::CertificateAuthority;
    secure.Tls.ServerCertificateAuthority = {Ca, sizeof(Ca)};
    secure.Tls.ClientCertificate = {ClientCertificate, sizeof(ClientCertificate)};
    secure.Tls.ClientPrivateKey = {ClientKey, sizeof(ClientKey)};
    secure.Headers = &validHeaders;
    secure.Policy.AutomaticReconnect = true;
    secure.Policy.ReconnectDelayMilliseconds = 2500;

    assert(client.Connect(secure));
    assert(platform.ConnectCalls == 1);
    assert(platform.LastTransport == WebTransportMode::Tls);
    assert(client.Disconnect());

    auto platformTrust = configuration;
    platformTrust.Transport = WebTransportMode::Tls;
    assert(client.Connect(platformTrust));
    assert(platform.ConnectCalls == 2);
    assert(client.Disconnect());
}

} // namespace

int main() {
    TestEndpointFacade();
    TestClientFacade();
    TestClientConfigurationValidation();
    return 0;
}
