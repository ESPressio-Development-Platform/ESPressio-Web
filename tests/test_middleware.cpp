#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <ESPressio_Middleware.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod Method() const noexcept override { return HttpMethod::Get; }
    std::string_view Path() const noexcept override { return "/"; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
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

class Terminal final : public IHttpRequestHandler {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext& context) override {
        ++Calls;
        return HttpHandlerResult::Handled(context.Response().Send("terminal"));
    }
};

class RecordingMiddleware final : public IHttpMiddleware {
public:
    std::string* Trace = nullptr;
    char Marker = '?';

    HttpHandlerResult Handle(WebRequestContext& context, IHttpMiddlewareNext& next) override {
        Trace->push_back(Marker);
        const auto result = next.Invoke(context);
        Trace->push_back(static_cast<char>(Marker - 'a' + 'A'));
        return result;
    }
};

class ShortCircuitMiddleware final : public IHttpMiddleware {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext& context, IHttpMiddlewareNext&) override {
        ++Calls;
        return HttpHandlerResult::Handled(context.Response().Send("blocked"));
    }
};

class SelfRemovingMiddleware final : public IHttpMiddleware {
public:
    MiddlewarePipeline* Pipeline = nullptr;
    MiddlewareHandle HandleValue;
    int Calls = 0;

    HttpHandlerResult Handle(WebRequestContext& context, IHttpMiddlewareNext& next) override {
        ++Calls;
        assert(Pipeline != nullptr);
        assert(Pipeline->Remove(HandleValue));
        return next.Invoke(context);
    }
};

HttpHandlerResult Invoke(MiddlewarePipeline& pipeline, Response& response) {
    Request request;
    WebRequestContext context(request, response);
    return pipeline.Handle(context);
}

void TestOrder() {
    MiddlewarePipeline pipeline;
    Terminal terminal;
    pipeline.SetTerminal(&terminal);

    std::string trace;
    RecordingMiddleware first;
    first.Trace = &trace;
    first.Marker = 'a';
    RecordingMiddleware second;
    second.Trace = &trace;
    second.Marker = 'b';
    pipeline.Add(first);
    pipeline.Add(second);

    Response response;
    const auto result = Invoke(pipeline, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(trace == "abBA");
    assert(terminal.Calls == 1);
    assert(response.Body == "terminal");
}

void TestShortCircuit() {
    MiddlewarePipeline pipeline;
    Terminal terminal;
    ShortCircuitMiddleware blocker;
    pipeline.SetTerminal(&terminal);
    pipeline.Add(blocker);

    Response response;
    const auto result = Invoke(pipeline, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(blocker.Calls == 1);
    assert(terminal.Calls == 0);
    assert(response.Body == "blocked");
}

void TestReentrantMutationUsesStableSnapshot() {
    MiddlewarePipeline pipeline;
    Terminal terminal;
    SelfRemovingMiddleware self;
    self.Pipeline = &pipeline;
    self.HandleValue = pipeline.Add(self);
    pipeline.SetTerminal(&terminal);
    assert(pipeline.Count() == 1);

    Response firstResponse;
    auto result = Invoke(pipeline, firstResponse);
    assert(result && terminal.Calls == 1);
    assert(self.Calls == 1);
    assert(pipeline.Count() == 0);

    Response secondResponse;
    result = Invoke(pipeline, secondResponse);
    assert(result && terminal.Calls == 2);
    assert(self.Calls == 1);
}

void TestNoTerminalFallsThrough() {
    MiddlewarePipeline pipeline;
    Response response;
    const auto result = Invoke(pipeline, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::NotHandled);
    assert(!response.Begun);
}

} // namespace

int main() {
    TestOrder();
    TestShortCircuit();
    TestReentrantMutationUsesStableSnapshot();
    TestNoTerminalFallsThrough();
    return 0;
}
