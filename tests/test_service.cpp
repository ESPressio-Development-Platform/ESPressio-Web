#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <ESPressio_HttpService.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Post;
    std::string PathValue = "/service";
    std::unordered_map<std::string, std::string> Headers;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return PathValue; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
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
        std::size_t& written
    ) const override {
        const auto it = Headers.find(std::string(name));
        if (it == Headers.end()) return WebResult::Failure(WebError::NotFound);
        if (it->second.size() > capacity) return WebResult::Failure(WebError::ResourceExhausted);
        std::memcpy(destination, it->second.data(), it->second.size());
        written = it->second.size();
        return WebResult::Success();
    }
    HttpReadResult ReadBody(uint8_t*, std::size_t) override {
        return {WebResult::Success(), 0, true};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    std::string Body;
    bool Begun = false;
    bool Completed = false;

    WebResult SetStatus(HttpStatus) override { return WebResult::Success(); }
    WebResult SetHeader(std::string_view, std::string_view) override { return WebResult::Success(); }
    WebResult Begin(std::optional<std::size_t>) override { Begun = true; return WebResult::Success(); }
    WebResult Write(const uint8_t* data, std::size_t size) override {
        Body.append(reinterpret_cast<const char*>(data), size);
        return WebResult::Success();
    }
    WebResult Complete() override { Completed = true; return WebResult::Success(); }
    void Abort() noexcept override {}
};

class MimeProvider final : public IHttpServiceProvider {
public:
    std::string Mime;
    std::string Reply;
    mutable int SelectionCalls = 0;
    int HandleCalls = 0;

    MimeProvider(std::string mime, std::string reply)
        : Mime(std::move(mime)), Reply(std::move(reply)) {}

    WebResult CanHandle(const HttpRequest& request, bool& canHandle) const override {
        ++SelectionCalls;
        canHandle = false;
        if (!request.HasHeader(HttpHeaderName::ContentType)) return WebResult::Success();
        char value[64]{};
        std::size_t written = 0;
        const auto result = request.ReadHeader(
            HttpHeaderName::ContentType,
            value,
            sizeof(value),
            written
        );
        if (!result) return result;
        canHandle = std::string_view(value, written) == Mime;
        return WebResult::Success();
    }

    HttpHandlerResult Handle(WebRequestContext& context) override {
        ++HandleCalls;
        return HttpHandlerResult::Handled(context.Response().Send(Reply));
    }
};

HttpHandlerResult Invoke(IHttpRequestHandler& handler, Request& request, Response& response) {
    WebRequestContext context(request, response);
    return handler.Handle(context);
}

void TestProviderSelection() {
    HttpService service;
    MimeProvider json("application/json", "json");
    MimeProvider cbor("application/cbor", "cbor");
    service.AddProvider(json);
    service.AddProvider(cbor);

    Request request;
    request.Headers["Content-Type"] = "application/cbor";
    Response response;
    const auto result = Invoke(service, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Body == "cbor");
    assert(json.SelectionCalls == 1 && json.HandleCalls == 0);
    assert(cbor.SelectionCalls == 1 && cbor.HandleCalls == 1);
}

void TestNoProviderFallsThroughAndRemovalWorks() {
    HttpService service;
    MimeProvider json("application/json", "json");
    const auto handle = service.AddProvider(json);
    assert(service.ProviderCount() == 1);
    assert(service.RemoveProvider(handle));
    assert(service.ProviderCount() == 0);

    Request request;
    request.Headers["Content-Type"] = "text/plain";
    Response response;
    const auto result = Invoke(service, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::NotHandled);
    assert(!response.Begun);
}

void TestServiceRegistersDirectlyAsRoute() {
    Router router;
    HttpService service;
    MimeProvider json("application/json", "routed-json");
    service.AddProvider(json);

    RouteHandle route;
    assert(router.RegisterRoute(HttpMethod::Post, "/service", service, route));

    Request request;
    request.Headers["Content-Type"] = "application/json";
    Response response;
    WebRequestContext context(request, response);
    const auto result = router.Handle(context);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Body == "routed-json");
}

} // namespace

int main() {
    TestProviderSelection();
    TestNoProviderFallsThroughAndRemovalWorks();
    TestServiceRegistersDirectlyAsRoute();
    return 0;
}
