#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <ESPressio_Middleware.hpp>

using namespace ESPressio::Web;

namespace {

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Request final : public IHttpRequestPlatform {
public:
    HttpMethod Method() const noexcept override { return HttpMethod::Get; }
    std::string_view Path() const noexcept override { return "/"; }
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Body (std::string): 24 bytes [Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * - Begun (bool): 1 bytes [0 bytes dynamic allocation]
 * - Completed (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [Body: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Calls (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Terminal final : public IHttpRequestHandler {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext& context) override {
        ++Calls;
        return HttpHandlerResult::Handled(context.Response().Send("terminal"));
    }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Trace (std::string*): 4 bytes [0 bytes dynamic allocation]
 * - Marker (char): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Calls (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class ShortCircuitMiddleware final : public IHttpMiddleware {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext& context, IHttpMiddlewareNext&) override {
        ++Calls;
        return HttpHandlerResult::Handled(context.Response().Send("blocked"));
    }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Pipeline (MiddlewarePipeline*): 4 bytes [0 bytes dynamic allocation]
 * - HandleValue (MiddlewareHandle): 8 bytes [0 bytes dynamic allocation]
 * - Calls (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 20 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - SecondResult (HttpHandlerResult): 12 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class DoubleNextMiddleware final : public IHttpMiddleware {
public:
    HttpHandlerResult SecondResult = HttpHandlerResult::NotHandled();

    HttpHandlerResult Handle(WebRequestContext& context, IHttpMiddlewareNext& next) override {
        const auto first = next.Invoke(context);
        SecondResult = next.Invoke(context);
        return first;
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

void TestNextCanOnlyBeInvokedOnce() {
    MiddlewarePipeline pipeline;
    Terminal terminal;
    DoubleNextMiddleware middleware;
    pipeline.SetTerminal(&terminal);
    pipeline.Add(middleware);

    Response response;
    const auto result = Invoke(pipeline, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(terminal.Calls == 1);
    assert(!middleware.SecondResult);
    assert(middleware.SecondResult.Result.Error == WebError::InvalidState);
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
    TestNextCanOnlyBeInvokedOnce();
    TestNoTerminalFallsThrough();
    return 0;
}
