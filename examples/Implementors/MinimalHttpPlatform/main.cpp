#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

#include <ESPressio_Web.hpp>

using namespace ESPressio::Web;

// This example deliberately uses tiny made-up native types. Replace these with
// the request/server primitives offered by the architecture being onboarded.
struct NativeRequest final {
    HttpMethod Method = HttpMethod::Get;
    std::string_view Path = "/";
    std::string_view Query;
    const uint8_t* Body = nullptr;
    std::size_t BodyBytes = 0;
};

class ExampleRequestPlatform final : public IHttpRequestPlatform {
public:
    explicit ExampleRequestPlatform(NativeRequest& request) : _request(request) {}

    HttpMethod Method() const noexcept override { return _request.Method; }
    std::string_view Path() const noexcept override { return _request.Path; }
    std::string_view QueryString() const noexcept override { return _request.Query; }

    std::optional<std::size_t> ContentLength() const noexcept override {
        return _request.BodyBytes == 0
            ? std::nullopt
            : std::optional<std::size_t>(_request.BodyBytes);
    }

    bool HasHeader(std::string_view) const noexcept override {
        // A real adapter should ask the native request only for the requested
        // header; do not eagerly construct a per-request map.
        return false;
    }

    std::size_t HeaderValueLength(std::string_view) const noexcept override {
        return 0;
    }

    WebResult ReadHeader(
        std::string_view,
        char*,
        std::size_t,
        std::size_t& bytesWritten
    ) const override {
        bytesWritten = 0;
        return WebResult::Failure(WebError::NotFound);
    }

    HttpReadResult ReadBody(uint8_t* destination, std::size_t capacity) override {
        if (destination == nullptr || capacity == 0) {
            return {WebResult::Failure(WebError::InvalidConfiguration), 0, false};
        }
        if (_offset >= _request.BodyBytes) {
            return {WebResult::Success(), 0, true};
        }

        const auto count = std::min(capacity, _request.BodyBytes - _offset);
        std::memcpy(destination, _request.Body + _offset, count);
        _offset += count;
        return {WebResult::Success(), count, _offset == _request.BodyBytes};
    }

private:
    NativeRequest& _request;
    std::size_t _offset = 0;
};

class ExampleResponsePlatform final : public IHttpResponsePlatform {
public:
    WebResult SetStatus(HttpStatus status) override {
        if (_begun) return WebResult::Failure(WebError::InvalidState);
        _status = status;
        return WebResult::Success();
    }

    WebResult SetHeader(std::string_view, std::string_view) override {
        if (_begun) return WebResult::Failure(WebError::InvalidState);
        // Translate/store native response headers here. Keep any borrowed Web
        // string_view alive only if the native stack copies it immediately.
        return WebResult::Success();
    }

    WebResult Begin(std::optional<std::size_t> contentLength) override {
        if (_begun) return WebResult::Failure(WebError::InvalidState);
        _expected = contentLength;
        _begun = true;
        // Commit status/headers to the native response here.
        return WebResult::Success();
    }

    WebResult Write(const uint8_t*, std::size_t size) override {
        if (!_begun || _completed || size == 0) {
            return WebResult::Failure(WebError::InvalidState);
        }
        // Stream these bytes to the native connection rather than buffering the
        // whole response merely to satisfy ESPressio-Web.
        _written += size;
        return WebResult::Success();
    }

    WebResult Complete() override {
        if (!_begun || _completed) return WebResult::Failure(WebError::InvalidState);
        if (_expected.has_value() && _written != *_expected) {
            return WebResult::Failure(WebError::ProtocolError);
        }
        _completed = true;
        return WebResult::Success();
    }

    void Abort() noexcept override {
        // Close/abort the native response if the Web response dies mid-stream.
        _completed = true;
    }

private:
    HttpStatus _status = HttpStatus::Ok;
    bool _begun = false;
    bool _completed = false;
    std::optional<std::size_t> _expected;
    std::size_t _written = 0;
};

class ExampleHttpServerPlatform final : public IHttpServerPlatform {
public:
    WebCapabilities Capabilities() const noexcept override {
        return ToCapabilities(WebCapability::Http) |
               ToCapabilities(WebCapability::PersistentConnections);
    }

    WebResult Initialize(
        const HttpServerConfiguration& configuration,
        IHttpRequestDispatcher& dispatcher
    ) override {
        if (_running) return WebResult::Failure(WebError::InvalidState);
        _configuration = configuration;
        _dispatcher = &dispatcher;
        return WebResult::Success();
    }

    WebResult Start() override {
        if (_dispatcher == nullptr) return WebResult::Failure(WebError::InvalidState);
        _running = true;
        // Start/listen using the architecture's native HTTP facility here.
        return WebResult::Success();
    }

    WebResult Stop() override {
        _running = false;
        return WebResult::Success();
    }

    void Reset() noexcept override {
        _running = false;
        _dispatcher = nullptr;
        _configuration = {};
    }

    // This is the important translation boundary. A real native callback should
    // construct short-lived request/response adapters and dispatch synchronously.
    WebResult OnNativeRequest(NativeRequest& request) {
        if (!_running || _dispatcher == nullptr) {
            return WebResult::Failure(WebError::NotRunning);
        }

        ExampleRequestPlatform requestPlatform(request);
        ExampleResponsePlatform responsePlatform;
        return _dispatcher->Dispatch(requestPlatform, responsePlatform);
    }

private:
    HttpServerConfiguration _configuration{};
    IHttpRequestDispatcher* _dispatcher = nullptr;
    bool _running = false;
};

class HelloHandler final : public IHttpRequestHandler {
public:
    HttpHandlerResult Handle(WebRequestContext& context) override {
        if (context.Request().Method() != HttpMethod::Get ||
            context.Request().Path() != "/") {
            return HttpHandlerResult::NotHandled();
        }
        return HttpHandlerResult::Handled(
            context.Response().Send("hello from a porting example")
        );
    }
};

int main() {
    ExampleHttpServerPlatform platform;
    HttpServer server(platform);
    HelloHandler handler;

    if (!server.SetRequestHandler(&handler)) return 1;
    if (!server.Initialize({})) return 2;
    if (!server.Start()) return 3;

    NativeRequest request;
    request.Method = HttpMethod::Get;
    request.Path = "/";

    // In a real port, this call is made by the architecture's native HTTP
    // callback rather than by main().
    if (!platform.OnNativeRequest(request)) return 4;
    return 0;
}
