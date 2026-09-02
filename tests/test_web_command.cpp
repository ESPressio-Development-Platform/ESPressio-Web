#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

#include <ESPressio_WebCommand.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Post;
    std::vector<uint8_t> Body;
    std::size_t Offset = 0;
    std::optional<std::size_t> DeclaredLength;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return "/command"; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override {
        return DeclaredLength;
    }
    bool HasHeader(std::string_view) const noexcept override { return false; }
    std::size_t HeaderValueLength(std::string_view) const noexcept override { return 0; }
    WebResult ReadHeader(
        std::string_view,
        char*,
        std::size_t,
        std::size_t& written
    ) const override {
        written = 0;
        return WebResult::Failure(WebError::NotFound);
    }
    HttpReadResult ReadBody(uint8_t* destination, std::size_t capacity) override {
        const auto remaining = Body.size() - Offset;
        const auto count = remaining < capacity ? remaining : capacity;
        if (count != 0) {
            std::memcpy(destination, Body.data() + Offset, count);
            Offset += count;
        }
        return {WebResult::Success(), count, Offset == Body.size()};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    HttpStatus Status = HttpStatus::Ok;
    bool Begun = false;
    bool Completed = false;

    WebResult SetStatus(HttpStatus status) override {
        Status = status;
        return WebResult::Success();
    }
    WebResult SetHeader(std::string_view, std::string_view) override {
        return WebResult::Success();
    }
    WebResult Begin(std::optional<std::size_t>) override {
        Begun = true;
        return WebResult::Success();
    }
    WebResult Write(const uint8_t*, std::size_t) override {
        return WebResult::Success();
    }
    WebResult Complete() override {
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override {}
};

HttpHandlerResult Invoke(
    HttpCommandIngress& ingress,
    Request& request,
    Response& response
) {
    WebRequestContext context(request, response);
    RouteParameters parameters;
    return ingress.Handle(context, parameters);
}

void TestAcceptedCommandQueuesOwnerLibraryEvent() {
    HttpCommandIngress ingress;
    assert(ingress.Configure({3}));

    Request request;
    constexpr std::string_view command = "system status";
    request.Body.assign(command.begin(), command.end());
    request.DeclaredLength = request.Body.size();
    Response response;

    const auto result = Invoke(ingress, request, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Status == HttpStatus::Accepted);
    assert(response.Begun && response.Completed);
}

void TestUnknownLengthCommandStreamsDirectlyIntoEnvelope() {
    HttpCommandIngress ingress;
    assert(ingress.Configure({3}));

    Request request;
    constexpr std::string_view command = "led toggle";
    request.Body.assign(command.begin(), command.end());
    request.DeclaredLength.reset();
    Response response;

    const auto result = Invoke(ingress, request, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(request.Offset == command.size());
    assert(response.Status == HttpStatus::Accepted);
    assert(response.Begun && response.Completed);
}

void TestValidationAndMethodFallthrough() {
    HttpCommandIngress ingress;
    assert(!ingress.Configure({0}));
    assert(ingress.Configure({8}));

    Request get;
    get.MethodValue = HttpMethod::Get;
    Response getResponse;
    auto result = Invoke(ingress, get, getResponse);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::NotHandled);

    Request empty;
    empty.DeclaredLength = 0;
    Response emptyResponse;
    result = Invoke(ingress, empty, emptyResponse);
    assert(!result);
    assert(result.Result.Error == WebError::ProtocolError);

    Request oversized;
    oversized.DeclaredLength = ESPRESSIO_COMMAND_MAX_RAW_LENGTH;
    Response oversizedResponse;
    result = Invoke(ingress, oversized, oversizedResponse);
    assert(!result);
    assert(result.Result.Error == WebError::RequestTooLarge);

    Request unknownOversized;
    unknownOversized.Body.resize(ESPRESSIO_COMMAND_MAX_RAW_LENGTH, 'x');
    unknownOversized.DeclaredLength.reset();
    Response unknownOversizedResponse;
    result = Invoke(ingress, unknownOversized, unknownOversizedResponse);
    assert(!result);
    assert(result.Result.Error == WebError::RequestTooLarge);
}

} // namespace

int main() {
    TestAcceptedCommandQueuesOwnerLibraryEvent();
    TestUnknownLengthCommandStreamsDirectlyIntoEnvelope();
    TestValidationAndMethodFallthrough();
    return 0;
}
