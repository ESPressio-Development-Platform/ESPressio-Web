#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define ESPRESSIO_STATE_ENABLE_INTROSPECTION 0
#include <ESPressio_WebState.hpp>

using namespace ESPressio::Web;

namespace {

/**
 * ESPressio Memory Audit
 * Members: none (standalone empty object occupies 1 byte; an eligible empty base may be optimized to 0 bytes).
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct CounterState {
    using Value = uint32_t;
    static constexpr ESPressio::State::StateTypeId Id = 7;
    static constexpr const char* Name = "counter.value";
};

using Contract = ESPressio::State::StateContract<CounterState>;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - MethodValue (HttpMethod): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Get;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return "/state/counter"; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
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
    HttpReadResult ReadBody(uint8_t*, std::size_t) override {
        return {WebResult::Success(), 0, true};
    }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - Status (HttpStatus): 2 bytes [0 bytes dynamic allocation]
 * - Headers (std::unordered_map<std::string, std::string>): 28 bytes [BucketCount * 4 bytes + N * (hash-node/link overhead + 48 bytes value); key/value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; key/value: Capacity + 1 bytes when capacity exceeds 15-byte SSO]
 * - Length (std::optional<std::size_t>): 8 bytes [0 bytes dynamic allocation]
 * - Body (std::vector<uint8_t>): 12 bytes [Capacity * (1 bytes) element storage]
 * - Completed (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 60 bytes [Headers: BucketCount * 4 bytes + N * (hash-node/link overhead + 48 bytes value); Headers: key/value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; Headers: key/value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; Body: Capacity * (1 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Response final : public IHttpResponsePlatform {
public:
    HttpStatus Status = HttpStatus::Ok;
    std::unordered_map<std::string, std::string> Headers;
    std::optional<std::size_t> Length;
    std::vector<uint8_t> Body;
    bool Completed = false;

    WebResult SetStatus(HttpStatus status) override {
        Status = status;
        return WebResult::Success();
    }
    WebResult SetHeader(std::string_view name, std::string_view value) override {
        Headers[std::string(name)] = std::string(value);
        return WebResult::Success();
    }
    WebResult Begin(std::optional<std::size_t> length) override {
        Length = length;
        return WebResult::Success();
    }
    WebResult Write(const uint8_t* data, std::size_t size) override {
        Body.insert(Body.end(), data, data + size);
        return WebResult::Success();
    }
    WebResult Complete() override {
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override {}
};

HttpHandlerResult Invoke(
    StateSnapshotHttpHandler<Contract, CounterState>& handler,
    Request& request,
    Response& response
) {
    WebRequestContext context(request, response);
    RouteParameters parameters;
    return handler.Handle(context, parameters);
}

void TestGetAndHeadUseStateSnapshotAndCodec() {
    ESPressio::State::StatePublisher<Contract> publisher;
    assert(publisher.RegisterSource<CounterState>([] { return uint32_t{42}; }));
    StateSnapshotHttpHandler<Contract, CounterState> handler(publisher);

    Request getRequest;
    Response getResponse;
    auto result = Invoke(handler, getRequest, getResponse);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(getResponse.Status == HttpStatus::Ok);
    assert(getResponse.Completed);
    assert(getResponse.Length == std::optional<std::size_t>(sizeof(uint32_t)));
    assert(getResponse.Headers["Content-Type"] == "application/octet-stream");
    assert(getResponse.Headers["X-ESPressio-State-Type"] == "counter.value");
    assert(getResponse.Headers["X-ESPressio-State-Type-Id"] == "7");
    assert(getResponse.Headers["X-ESPressio-State-Epoch"] == "1");
    assert(getResponse.Headers["X-ESPressio-State-Revision"] == "1");
    assert(getResponse.Body.size() == sizeof(uint32_t));

    uint32_t decoded = 0;
    std::memcpy(&decoded, getResponse.Body.data(), sizeof(decoded));
    assert(decoded == 42);

    Request headRequest;
    headRequest.MethodValue = HttpMethod::Head;
    Response headResponse;
    result = Invoke(handler, headRequest, headResponse);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::Handled);
    assert(headResponse.Completed);
    assert(headResponse.Length == std::optional<std::size_t>(sizeof(uint32_t)));
    assert(headResponse.Body.empty());
}

void TestMissingSourceAndUnsupportedMethod() {
    ESPressio::State::StatePublisher<Contract> publisher;
    StateSnapshotHttpHandler<Contract, CounterState> handler(publisher);

    Request request;
    Response response;
    auto result = Invoke(handler, request, response);
    assert(!result);
    assert(result.Result.Error == WebError::NotFound);

    request.MethodValue = HttpMethod::Post;
    Response postResponse;
    result = Invoke(handler, request, postResponse);
    assert(result);
    assert(result.Disposition == HttpHandlerDisposition::NotHandled);
}

} // namespace

int main() {
    TestGetAndHeadUseStateSnapshotAndCodec();
    TestMissingSourceAndUnsupportedMethod();
    return 0;
}
