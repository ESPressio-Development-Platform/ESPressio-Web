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
    std::string QueryValue = "verbose=1";
    std::optional<std::size_t> Length;
    std::unordered_map<std::string, std::string> Headers;
    std::vector<uint8_t> Body;
    std::size_t BodyOffset = 0;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return PathValue; }
    std::string_view QueryString() const noexcept override { return QueryValue; }
    std::optional<std::size_t> ContentLength() const noexcept override { return Length; }

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
        if (it == Headers.end()) {
            bytesWritten = 0;
            return WebResult::Failure(WebError::ProtocolError);
        }
        if (capacity < it->second.size()) {
            bytesWritten = 0;
            return WebResult::Failure(WebError::ResourceExhausted);
        }
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

    WebResult Begin(std::optional<std::size_t> contentLength) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        Begun = true;
        BegunLength = contentLength;
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
    WebCapabilities CapabilityValue =
        ToCapabilities(WebCapability::Http) |
        ToCapabilities(WebCapability::ChunkedResponses);
    IHttpRequestDispatcher* Dispatcher = nullptr;
    HttpServerConfiguration Configuration{};
    int InitializeCalls = 0;
    int StartCalls = 0;
    int StopCalls = 0;
    int ResetCalls = 0;

    WebCapabilities Capabilities() const noexcept override { return CapabilityValue; }

    WebResult Initialize(
        const HttpServerConfiguration& configuration,
        IHttpRequestDispatcher& dispatcher
    ) override {
        ++InitializeCalls;
        Configuration = configuration;
        Dispatcher = &dispatcher;
        return WebResult::Success();
    }

    WebResult Start() override {
        ++StartCalls;
        return WebResult::Success();
    }

    WebResult Stop() override {
        ++StopCalls;
        return WebResult::Success();
    }

    void Reset() noexcept override {
        ++ResetCalls;
        Dispatcher = nullptr;
    }
};

class EchoHandler final : public IHttpRequestHandler {
public:
    int Calls = 0;

    WebResult Handle(WebRequestContext& context) override {
        ++Calls;
        if (context.Request().Path() != "/status") {
            return WebResult::Failure(WebError::ProtocolError);
        }
        (void)context.Response().Status(HttpStatus::Accepted);
        return context.Response().Send("accepted", "text/plain");
    }
};

class ServerObserver final : public IHttpServerObserver {
public:
    HttpServer* Server = nullptr;
    int Notifications = 0;

    void OnHttpServerStateChanged(HttpServerState, HttpServerState newState) override {
        ++Notifications;
        assert(Server != nullptr);
        assert(Server->State() == newState);
    }
};

void TestRequestAndResponse() {
    FakeRequest requestPlatform;
    requestPlatform.Headers["Content-Type"] = "application/json";
    requestPlatform.Body = {1, 2, 3, 4};
    requestPlatform.Length = requestPlatform.Body.size();

    FakeResponse responsePlatform;
    WebRequestContext context(requestPlatform, responsePlatform);

    assert(context.Request().Method() == HttpMethod::Get);
    assert(context.Request().Path() == "/status");
    assert(context.Request().QueryString() == "verbose=1");
    assert(context.Request().HasHeader("Content-Type"));
    assert(context.Request().HeaderValueLength("Content-Type") == 16);

    char header[32]{};
    std::size_t written = 0;
    assert(context.Request().ReadHeader("Content-Type", header, sizeof(header), written));
    assert(std::string_view(header, written) == "application/json");

    uint8_t body[3]{};
    auto read = context.Request().ReadBody(body, sizeof(body));
    assert(read);
    assert(read.BytesRead == 3);
    assert(!read.EndOfBody);
    read = context.Request().ReadBody(body, sizeof(body));
    assert(read);
    assert(read.BytesRead == 1);
    assert(read.EndOfBody);

    assert(context.Response().Status(HttpStatus::Created));
    assert(context.Response().Send("hello", "text/plain"));
    assert(context.Response().State() == HttpResponseState::Completed);
    assert(responsePlatform.StatusValue == HttpStatus::Created);
    assert(responsePlatform.Headers["Content-Type"] == "text/plain");
    assert(responsePlatform.BegunLength == std::optional<std::size_t>(5));
    assert(std::string(responsePlatform.Bytes.begin(), responsePlatform.Bytes.end()) == "hello");
    assert(responsePlatform.Completed);
}

void TestServerLifecycleAndDispatch() {
    FakeServerPlatform platform;
    HttpServer server(platform);
    EchoHandler handler;
    ServerObserver observer;
    observer.Server = &server;
    auto observerHandle = server.RegisterObserver(&observer);

    assert(server.SetRequestHandler(&handler));

    HttpServerConfiguration configuration;
    configuration.Port = 8080;
    assert(server.Initialize(configuration));
    assert(server.State() == HttpServerState::Ready);
    assert(platform.InitializeCalls == 1);
    assert(platform.Configuration.Port == 8080);
    assert(server.Supports(WebCapability::Http));
    assert(server.Supports(WebCapability::ChunkedResponses));

    assert(server.Start());
    assert(server.State() == HttpServerState::Running);
    assert(platform.StartCalls == 1);
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
    assert(platform.StopCalls == 1);
    assert(observer.Notifications == 6);

    observerHandle.reset();
}

} // namespace

int main() {
    TestRequestAndResponse();
    TestServerLifecycleAndDispatch();
    return 0;
}
