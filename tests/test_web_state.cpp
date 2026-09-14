#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ESPressio_SerializationMacros.hpp>
#include <ESPressio_States.hpp>
#include <ESPressio_WebState.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

struct DynamicValue final {
    std::uint32_t Value = 0;
    constexpr bool operator==(const DynamicValue& other) const noexcept {
        return Value == other.Value;
    }
    ESPRESSIO_SERIALIZABLE_TYPE(DynamicValue)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value", Value))
};

struct DynamicState final : State::SerializableState<DynamicState, DynamicValue> {
    static constexpr State::StateTypeId TypeId{0x7701};
    static constexpr std::string_view CanonicalName = "Test.Web.DynamicState";
};

struct LocalOnlyState final : State::State<LocalOnlyState, std::uint32_t> {
    static constexpr State::StateTypeId TypeId{0x7702};
    static constexpr std::string_view CanonicalName = "Test.Web.LocalOnlyState";
};

Timing::QualifiedTime CaptureTruthTime() {
    return {123456789ULL, Timing::TimeReliability::Holdover};
}

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Get;
    std::string_view Accept;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return "/application-owned-state-route"; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
    bool HasHeader(std::string_view name) const noexcept override {
        return name == HttpHeaderName::Accept && !Accept.empty();
    }
    std::size_t HeaderValueLength(std::string_view name) const noexcept override {
        return HasHeader(name) ? Accept.size() : 0;
    }
    WebResult ReadHeader(
        std::string_view name,
        char* destination,
        std::size_t capacity,
        std::size_t& written
    ) const override {
        written = 0;
        if (!HasHeader(name)) return WebResult::Failure(WebError::NotFound);
        if (capacity < Accept.size()) return WebResult::Failure(WebError::ResourceExhausted);
        std::memcpy(destination, Accept.data(), Accept.size());
        written = Accept.size();
        return WebResult::Success();
    }
    HttpReadResult ReadBody(std::uint8_t*, std::size_t) override {
        return {WebResult::Success(), 0, true};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    HttpStatus Status = HttpStatus::Ok;
    std::unordered_map<std::string, std::string> Headers;
    std::optional<std::size_t> Length;
    std::vector<std::uint8_t> Body;
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
    WebResult Write(const std::uint8_t* data, std::size_t size) override {
        Body.insert(Body.end(), data, data + size);
        return WebResult::Success();
    }
    WebResult Complete() override {
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override {}
};

class Selector final : public IHttpStateTargetSelector {
public:
    std::string_view Name = DynamicState::CanonicalName;
    std::string_view SelectStateType(
        const HttpRequest&,
        const RouteParameters&
    ) const noexcept override {
        return Name;
    }
};

class Authorizer final : public IHttpStateAuthorizer {
public:
    HttpStateAuthorizationDecision Decision = HttpStateAuthorizationDecision::Authorized;
    HttpStateAuthorizationDecision Authorize(
        const HttpRequest&,
        const Primitive::PrimitiveTypeDescriptor&
    ) const noexcept override {
        return Decision;
    }
};

using Handler = HttpStateInspection<128>;
using Runtime = State::Runtime<
    State::TypeConfiguration<DynamicState>,
    State::TypeConfiguration<LocalOnlyState>>;

struct Fixture final {
    Primitive::TypeDirectory<2> Directory;
    Runtime States;
    State::StateOwner<DynamicState> DynamicOwner;
    State::StateOwner<LocalOnlyState> LocalOwner;
    Selector Target;
    Authorizer Authorization;
    Handler Inspection;

    Fixture() {
        assert(Directory.Register<DynamicState>() == Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Register<LocalOnlyState>() == Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);
        DynamicOwner = States.BindOwner<DynamicState>();
        LocalOwner = States.BindOwner<LocalOnlyState>();
        assert(DynamicOwner && LocalOwner);
        assert(States.Initialize(Directory.View(), &CaptureTruthTime) == State::StateRuntimeStatus::Success);
        assert(States.Start() == State::StateRuntimeStatus::Success);

        HttpStateInspectionConfiguration configuration{};
        configuration.Types = Directory.View();
        configuration.TargetSelector = &Target;
        configuration.Authorizer = &Authorization;
        assert(Inspection.Configure(configuration));
    }

    ~Fixture() {
        assert(States.Shutdown() == State::StateRuntimeStatus::Success);
    }
};

HttpHandlerResult Invoke(
    Handler& handler,
    Request& request,
    Response& response
) {
    WebRequestContext context(request, response);
    RouteParameters parameters;
    return handler.Handle(context, parameters);
}

void TestConfigurationRequiresFrozenDiscoveryAndSecurity() {
    Handler handler;
    HttpStateInspectionConfiguration configuration{};
    assert(!handler.Configure(configuration));

    Primitive::TypeDirectory<1> unfrozen;
    Selector selector;
    Authorizer authorizer;
    configuration.Types = unfrozen.View();
    configuration.TargetSelector = &selector;
    configuration.Authorizer = &authorizer;
    assert(!handler.Configure(configuration));
}

void TestAbsentValueAndReadOnlyMethodBoundary(Fixture& fixture) {
    Request request;
    Response absent;
    auto result = Invoke(fixture.Inspection, request, absent);
    assert(result && absent.Status == HttpStatus::NotFound);

    request.MethodValue = HttpMethod::Post;
    Response post;
    result = Invoke(fixture.Inspection, request, post);
    assert(result && result.Disposition == HttpHandlerDisposition::NotHandled);
}

void TestDirectBinaryGetAndHead(Fixture& fixture) {
    assert(fixture.DynamicOwner.Set({42}) == State::StateSetStatus::Changed);

    Request get;
    Response response;
    auto result = Invoke(fixture.Inspection, get, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Status == HttpStatus::Ok && response.Completed);
    assert(response.Headers["Content-Type"] == "application/octet-stream");
    assert(response.Headers["X-ESPressio-State-Type"] == "Test.Web.DynamicState");
    assert(response.Headers["X-ESPressio-State-Type-Id"] == std::to_string(DynamicState::TypeId.Value()));
    assert(response.Headers["X-ESPressio-State-Truth-Nanoseconds"] == "123456789");
    assert(!response.Headers["X-ESPressio-State-Time-Reliability"].empty());
    assert(response.Length.has_value() && *response.Length == response.Body.size());

    DynamicValue decoded{};
    assert(Serializable::DeserializeBoundedDirectBinary(
        response.Body.data(), response.Body.size(), decoded));
    assert(decoded.Value == 42);

    Request head;
    head.MethodValue = HttpMethod::Head;
    Response headResponse;
    result = Invoke(fixture.Inspection, head, headResponse);
    assert(result && headResponse.Status == HttpStatus::Ok && headResponse.Completed);
    assert(headResponse.Length.has_value() && *headResponse.Length > 0);
    assert(headResponse.Body.empty());
}

void TestP3RepresentationSelection(Fixture& fixture) {
    Request json;
    json.Accept = "application/json";
    Response jsonResponse;
    assert(Invoke(fixture.Inspection, json, jsonResponse));
    assert(jsonResponse.Status == HttpStatus::Ok);
    assert(jsonResponse.Headers["Content-Type"] == "application/json");
    DynamicValue jsonDecoded{};
    assert(Serializable::DeserializeBoundedJson(
        jsonResponse.Body.data(), jsonResponse.Body.size(), jsonDecoded));
    assert(jsonDecoded.Value == 42);

    Request cbor;
    cbor.Accept = "application/cbor";
    Response cborResponse;
    assert(Invoke(fixture.Inspection, cbor, cborResponse));
    assert(cborResponse.Status == HttpStatus::Ok);
    assert(cborResponse.Headers["Content-Type"] == "application/cbor");
    DynamicValue cborDecoded{};
    assert(Serializable::DeserializeBoundedCbor(
        cborResponse.Body.data(), cborResponse.Body.size(), cborDecoded));
    assert(cborDecoded.Value == 42);

    Request unsupported;
    unsupported.Accept = "text/plain";
    Response unsupportedResponse;
    assert(Invoke(fixture.Inspection, unsupported, unsupportedResponse));
    assert(unsupportedResponse.Status == HttpStatus::UnsupportedMediaType);
}

void TestDiscoveryAuthorizationAndSerializableBoundary(Fixture& fixture) {
    Request request;

    fixture.Target.Name = "Missing.State";
    Response missing;
    assert(Invoke(fixture.Inspection, request, missing));
    assert(missing.Status == HttpStatus::NotFound);

    fixture.Target.Name = DynamicState::CanonicalName;
    fixture.Authorization.Decision = HttpStateAuthorizationDecision::Unauthorized;
    Response unauthorized;
    assert(Invoke(fixture.Inspection, request, unauthorized));
    assert(unauthorized.Status == HttpStatus::Unauthorized);

    fixture.Authorization.Decision = HttpStateAuthorizationDecision::Forbidden;
    Response forbidden;
    assert(Invoke(fixture.Inspection, request, forbidden));
    assert(forbidden.Status == HttpStatus::Forbidden);

    fixture.Authorization.Decision = HttpStateAuthorizationDecision::Authorized;
    fixture.Target.Name = LocalOnlyState::CanonicalName;
    Response localOnly;
    assert(Invoke(fixture.Inspection, request, localOnly));
    assert(localOnly.Status == HttpStatus::UnprocessableContent);

    fixture.Target.Name = DynamicState::CanonicalName;
}

} // namespace

int main() {
    TestConfigurationRequiresFrozenDiscoveryAndSecurity();
    Fixture fixture;
    TestAbsentValueAndReadOnlyMethodBoundary(fixture);
    TestDirectBinaryGetAndHead(fixture);
    TestP3RepresentationSelection(fixture);
    TestDiscoveryAuthorizationAndSerializableBoundary(fixture);
    return 0;
}
