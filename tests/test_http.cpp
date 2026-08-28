#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ESPressio_Http.hpp>
#include <ESPressio_HttpServer.hpp>

using namespace ESPressio::Web;

namespace {

class FakeRequest final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Get;
    std::string PathValue = "/status";
    std::string QueryValue = "verbose=1&empty=&flag";
    std::optional<std::size_t> Length;
    std::unordered_map<std::string, std::string> Headers;
    std::vector<uint8_t> Body;
    std::size_t BodyOffset = 0;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return PathValue; }
    std::string_view QueryString() const noexcept override { return QueryValue; }
    std::optional<std::size_t> ContentLength() const noexcept override { return Length; }
    bool HasHeader(std::string_view name) const noexcept override {
        return Headers.find(std::string(name)) != Headers.end();
    }
    std::size_t HeaderValueLength(std::string_view name) const noexcept override {
        const auto it = Headers.find(std::string(name));
        return it == Headers.end() ? 0 : it->second.size();
    }
    WebResult ReadHeader(
        std::string_view name,
        char* destination,
        std::size_t capacity,
        std::size_t& bytesWritten
    ) const override {
        const auto it = Headers.find(std::string(name));
        if (it == Headers.end()) return WebResult::Failure(WebError::NotFound);
        if (capacity < it->second.size()) return WebResult::Failure(WebError::ResourceExhausted);
        std::memcpy(destination, it->second.data(), it->second.size());
        bytesWritten = it->second.size();
        return WebResult::Success();
    }
    HttpReadResult ReadBody(uint8_t* destination, std::size_t capacity) override {
        const std::size_t remaining = Body.size() - BodyOffset;
        const std::size_t count = remaining < capacity ? remaining : capacity;
        if (count > 0) {
            std::memcpy(destination, Body.data() + BodyOffset, count);
            BodyOffset += count;
        }
        return {WebResult::Success(), count, BodyOffset == Body.size()};
    }
};

class FakeResponse final : public IHttpResponsePlatform {
public:
    HttpStatus StatusValue = HttpStatus::Ok;
    std::unordered_map<std::string, std::string> Headers;
    std::optional<std::size_t> BegunLength;
    std::vector<uint8_t> Bytes;
    bool Begun = false;
    bool Completed = false;
    bool Aborted = false;

    WebResult SetStatus(HttpStatus status) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        StatusValue = status;
        return WebResult::Success();
    }
    WebResult SetHeader(std::string_view name, std::string_view value) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        Headers[std::string(name)] = std::string(value);
        return WebResult::Success();
    }
    WebResult Begin(std::optional<std::size_t> length) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        Begun = true;
        BegunLength = length;
        return WebResult::Success();
    }
    WebResult Write(const uint8_t* data, std::size_t size) override {
        if (!Begun || Completed || Aborted) return WebResult::Failure(WebError::InvalidState);
        Bytes.insert(Bytes.end(), data, data + size);
        return WebResult::Success();
    }
    WebResult Complete() override {
        if (!Begun || Aborted) return WebResult::Failure(WebError::InvalidState);
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override { Aborted = true; }
};

class FakeServerPlatform final : public IHttpServerPlatform {
public:
    IHttpRequestDispatcher* Dispatcher = nullptr;
    HttpServerConfiguration Config{};
    int Starts = 0;
    int Stops = 0;
    WebCapabilities CapabilityValue =
        ToCapabilities(WebCapability::Http) |
        ToCapabilities(WebCapability::ChunkedResponses) |
        ToCapabilities(WebCapability::PersistentConnections);

    WebCapabilities Capabilities() const noexcept override { return CapabilityValue; }
    WebResult Initialize(const HttpServerConfiguration& config, IHttpRequestDispatcher& dispatcher) override {
        Config = config;
        Dispatcher = &dispatcher;
        return WebResult::Success();
    }
    WebResult Start() override { ++Starts; return WebResult::Success(); }
    WebResult Stop() override { ++Stops; return WebResult::Success(); }
    void Reset() noexcept override { Dispatcher = nullptr; }
};

class EchoHandler final : public IHttpRequestHandler {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext& context) override {
        ++Calls;
        if (context.Request().Path() != "/status") return HttpHandlerResult::NotHandled();
        (void)context.Response().Status(HttpStatus::Accepted);
        return HttpHandlerResult::Handled(context.Response().Send("accepted", "text/plain"));
    }
};

class Observer final : public IHttpServerObserver {
public:
    HttpServer* Server = nullptr;
    int Notifications = 0;
    void OnHttpServerStateChanged(HttpServerState, HttpServerState state) override {
        ++Notifications;
        assert(Server != nullptr);
        assert(Server->State() == state);
    }
};

void TestRequestAndResponse() {
    FakeRequest request;
    request.Headers["Content-Type"] = "application/json";
    request.Headers["X-Empty"] = "";
    request.Body = {1, 2, 3, 4};
    request.Length = request.Body.size();
    FakeResponse response;
    WebRequestContext context(request, response);

    assert(context.Request().Method() == HttpMethod::Get);
    assert(context.Request().Path() == "/status");
    assert(context.Request().HasHeader("Content-Type"));
    assert(context.Request().HasHeader("X-Empty"));
    assert(context.Request().HeaderValueLength("X-Empty") == 0);
    assert(context.Request().QueryValue("verbose") == std::optional<std::string_view>("1"));
    assert(context.Request().QueryValue("empty") == std::optional<std::string_view>(""));
    assert(context.Request().QueryValue("flag") == std::optional<std::string_view>(""));
    assert(!context.Request().QueryValue("missing").has_value());

    char header[32]{};
    std::size_t written = 0;
    assert(context.Request().ReadHeader("Content-Type", header, sizeof(header), written));
    assert(std::string_view(header, written) == "application/json");

    uint8_t body[3]{};
    auto read = context.Request().ReadBody(body, sizeof(body));
    assert(read && read.BytesRead == 3 && !read.EndOfBody);
    read = context.Request().ReadBody(body, sizeof(body));
    assert(read && read.BytesRead == 1 && read.EndOfBody);

    assert(context.Response().Status(HttpStatus::Created));
    assert(context.Response().Send("hello", "text/plain"));
    assert(context.Response().State() == HttpResponseState::Completed);
    assert(response.StatusValue == HttpStatus::Created);
    assert(response.Headers["Content-Type"] == "text/plain");
    assert(response.BegunLength == std::optional<std::size_t>(5));
}

void TestServerLifecycleAndDispatch() {
    FakeServerPlatform platform;
    HttpServer server(platform);
    EchoHandler handler;
    Observer observer;
    observer.Server = &server;
    auto observerHandle = server.RegisterObserver(&observer);

    assert(server.SetRequestHandler(&handler));
    HttpServerConfiguration config;
    config.Port = 8080;
    assert(server.Initialize(config));
    assert(server.State() == HttpServerState::Ready);
    assert(server.Supports(WebCapability::Http));
    assert(server.Start());
    assert(!server.SetRequestHandler(nullptr));

    FakeRequest request;
    FakeResponse response;
    assert(platform.Dispatcher != nullptr);
    assert(platform.Dispatcher->Dispatch(request, response));
    assert(handler.Calls == 1);
    assert(response.StatusValue == HttpStatus::Accepted);
    assert(response.Completed);

    assert(server.Stop());
    assert(server.State() == HttpServerState::Stopped);
    assert(platform.Starts == 1 && platform.Stops == 1);
    assert(observer.Notifications == 6);
    observerHandle.reset();
}

void TestNotHandledBecomesNotFound() {
    FakeServerPlatform platform;
    HttpServer server(platform);
    EchoHandler handler;
    assert(server.SetRequestHandler(&handler));
    assert(server.Initialize({}));
    assert(server.Start());

    FakeRequest request;
    request.PathValue = "/missing";
    FakeResponse response;
    const auto result = platform.Dispatcher->Dispatch(request, response);
    assert(result.Error == WebError::NotFound);
    assert(!response.Begun);
    assert(server.Stop());
}

void TestConfigurationCapabilities() {
    FakeServerPlatform platform;
    HttpServer server(platform);

    HttpServerConfiguration invalid;
    invalid.Port = 0;
    assert(server.Initialize(invalid).Error == WebError::InvalidConfiguration);

    HttpServerConfiguration tls;
    tls.TransportMode = HttpTransportMode::Tls;
    assert(server.Initialize(tls).Error == WebError::Unsupported);

    platform.CapabilityValue = ToCapabilities(WebCapability::Http);
    HttpServerConfiguration keepAlive;
    assert(server.Initialize(keepAlive).Error == WebError::Unsupported);
}

void TestDeclaredBodyLimitRejectsBeforeHandler() {
    FakeServerPlatform platform;
    HttpServer server(platform);
    EchoHandler handler;
    assert(server.SetRequestHandler(&handler));

    HttpServerConfiguration config;
    config.MaximumRequestBodyBytes = 4;
    assert(server.Initialize(config));
    assert(server.Start());

    FakeRequest request;
    request.Length = 5;
    FakeResponse response;
    const auto result = platform.Dispatcher->Dispatch(request, response);
    assert(result.Error == WebError::RequestTooLarge);
    assert(handler.Calls == 0);
    assert(!response.Begun);
    assert(server.Stop());
}

} // namespace

int main() {
    TestRequestAndResponse();
    TestServerLifecycleAndDispatch();
    TestNotHandledBecomesNotFound();
    TestConfigurationCapabilities();
    TestDeclaredBodyLimitRejectsBeforeHandler();
    return 0;
}
