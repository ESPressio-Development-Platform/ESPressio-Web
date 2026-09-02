#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ESPressio_WebEvent.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Post;
    std::vector<uint8_t> Body;
    std::size_t Offset = 0;
    std::optional<std::size_t> DeclaredLength;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return "/event"; }
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

class Receiver final : public ESPressio::Event::IEventTransportReceiver {
public:
    ESPressio::Event::IEventTransport* Transport = nullptr;
    std::vector<uint8_t> Packet;
    int Calls = 0;

    void ReceiveEventTransportPacket(
        ESPressio::Event::IEventTransport* transport,
        ESPressio::Event::EventTransportPacket packet
    ) override {
        ++Calls;
        Transport = transport;
        Packet.assign(packet.Data(), packet.Data() + packet.Size());
    }
};

HttpHandlerResult Invoke(
    HttpEventIngress& ingress,
    Request& request,
    Response& response
) {
    WebRequestContext context(request, response);
    RouteParameters parameters;
    return ingress.Handle(context, parameters);
}

void TestAcceptedPacketIsHandedToEventTransportReceiver() {
    HttpEventIngress ingress;
    Receiver receiver;
    ingress.SetReceiver(&receiver);
    assert(ingress.Configure({64, 2}));

    Request request;
    request.Body = {1, 2, 3, 4, 5};
    request.DeclaredLength = request.Body.size();
    Response response;

    const auto result = Invoke(ingress, request, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Status == HttpStatus::Accepted);
    assert(response.Begun && response.Completed);
    assert(receiver.Calls == 1);
    assert(receiver.Transport == &ingress);
    assert(receiver.Packet == request.Body);
    assert(!ingress.Send({}));
}

void TestValidationAndMethodFallthrough() {
    HttpEventIngress ingress;
    Receiver receiver;
    ingress.SetReceiver(&receiver);
    assert(ingress.Configure({4, 2}));

    Request request;
    request.MethodValue = HttpMethod::Get;
    Response response;
    auto result = Invoke(ingress, request, response);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::NotHandled);
    assert(receiver.Calls == 0);

    request.MethodValue = HttpMethod::Post;
    request.Body = {1, 2, 3, 4, 5};
    request.DeclaredLength = request.Body.size();
    Response oversizedResponse;
    result = Invoke(ingress, request, oversizedResponse);
    assert(!result);
    assert(result.Result.Error == WebError::RequestTooLarge);
    assert(receiver.Calls == 0);

    HttpEventIngress unbound;
    Request valid;
    valid.Body = {1};
    valid.DeclaredLength = 1;
    Response unboundResponse;
    result = Invoke(unbound, valid, unboundResponse);
    assert(!result);
    assert(result.Result.Error == WebError::NotRunning);
}

} // namespace

int main() {
    TestAcceptedPacketIsHandedToEventTransportReceiver();
    TestValidationAndMethodFallthrough();
    return 0;
}
