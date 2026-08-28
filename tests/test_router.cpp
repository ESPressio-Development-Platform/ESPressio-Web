#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <ESPressio_Router.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Get;
    std::string PathValue = "/";

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return PathValue; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
    bool HasHeader(std::string_view) const noexcept override { return false; }
    std::size_t HeaderValueLength(std::string_view) const noexcept override { return 0; }
    WebResult ReadHeader(std::string_view, char*, std::size_t, std::size_t& written) const override {
        written = 0;
        return WebResult::Failure(WebError::NotFound);
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

    WebResult SetStatus(HttpStatus) override { return Begun ? WebResult::Failure(WebError::InvalidState) : WebResult::Success(); }
    WebResult SetHeader(std::string_view, std::string_view) override { return Begun ? WebResult::Failure(WebError::InvalidState) : WebResult::Success(); }
    WebResult Begin(std::optional<std::size_t>) override { Begun = true; return WebResult::Success(); }
    WebResult Write(const uint8_t* data, std::size_t size) override {
        Body.append(reinterpret_cast<const char*>(data), size);
        return WebResult::Success();
    }
    WebResult Complete() override { Completed = true; return WebResult::Success(); }
    void Abort() noexcept override {}
};

class Handler final : public IHttpRouteHandler {
public:
    std::string Name;
    std::string LastId;
    int Calls = 0;

    explicit Handler(std::string name) : Name(std::move(name)) {}

    HttpHandlerResult Handle(WebRequestContext& context, const RouteParameters& parameters) override {
        ++Calls;
        LastId = std::string(parameters.Find("id"));
        return HttpHandlerResult::Handled(context.Response().Send(Name));
    }
};

HttpHandlerResult Invoke(Router& router, Request& request, Response& response) {
    WebRequestContext context(request, response);
    return router.Handle(context);
}

void TestExactBeatsNamedAndParametersAreBorrowed() {
    Router router;
    Handler named("named");
    Handler exact("exact");
    RouteHandle namedHandle;
    RouteHandle exactHandle;

    assert(router.RegisterRoute(HttpMethod::Get, "/device/:id", named, namedHandle));
    assert(router.RegisterRoute(HttpMethod::Get, "/device/status", exact, exactHandle));
    assert(router.RouteCount() == 2);

    Request request;
    request.PathValue = "/device/status";
    Response response;
    auto result = Invoke(router, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Body == "exact");
    assert(exact.Calls == 1 && named.Calls == 0);

    request.PathValue = "/device/42";
    Response parameterResponse;
    result = Invoke(router, request, parameterResponse);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(parameterResponse.Body == "named");
    assert(named.LastId == "42");
}

void TestMethodSpecificBeatsAny() {
    Router router;
    Handler any("any");
    Handler post("post");
    RouteHandle anyHandle;
    RouteHandle postHandle;
    assert(router.RegisterRoute(HttpMethod::Any, "/command", any, anyHandle));
    assert(router.RegisterRoute(HttpMethod::Post, "/command", post, postHandle));

    Request request;
    request.MethodValue = HttpMethod::Post;
    request.PathValue = "/command";
    Response response;
    auto result = Invoke(router, request, response);
    assert(result && response.Body == "post");
}

void TestRemovalAndNotHandled() {
    Router router;
    Handler handler("value");
    RouteHandle handle;
    assert(router.RegisterRoute(HttpMethod::Get, "/value/:id", handler, handle));
    assert(handle);
    assert(router.RemoveRoute(handle));
    assert(!router.RemoveRoute(handle));
    assert(router.RouteCount() == 0);

    Request request;
    request.PathValue = "/value/1";
    Response response;
    const auto result = Invoke(router, request, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::NotHandled);
    assert(!response.Begun);
}

void TestInvalidPatternsRejected() {
    Router router;
    Handler handler("invalid");
    RouteHandle handle;
    assert(!router.RegisterRoute(HttpMethod::Get, "missing-leading-slash", handler, handle));
    assert(!router.RegisterRoute(HttpMethod::Get, "/bad/", handler, handle));
    assert(!router.RegisterRoute(HttpMethod::Get, "/bad/:", handler, handle));
}

} // namespace

int main() {
    TestExactBeatsNamedAndParametersAreBorrowed();
    TestMethodSpecificBeatsAny();
    TestRemovalAndNotHandled();
    TestInvalidPatternsRejected();
    return 0;
}
