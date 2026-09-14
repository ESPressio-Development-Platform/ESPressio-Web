#pragma once

#if !__has_include(<ESPressio_StateDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Web State integration requires the final ESPressio-State and ESPressio-Primitive redesign surfaces."
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

#include <ESPressio_StateDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

namespace StateHttpHeaderName {
inline constexpr std::string_view Type = "X-ESPressio-State-Type";
inline constexpr std::string_view TypeId = "X-ESPressio-State-Type-Id";
inline constexpr std::string_view TruthNanoseconds = "X-ESPressio-State-Truth-Nanoseconds";
inline constexpr std::string_view TimeReliability = "X-ESPressio-State-Time-Reliability";
} // namespace StateHttpHeaderName

enum class HttpStateAuthorizationDecision : std::uint8_t {
    Authorized,
    Unauthorized,
    Forbidden
};

/// Caller-owned target selection policy. Applications retain ownership of URL shape;
/// the selector returns only the canonical State type name borrowed from the request/route.
class IHttpStateTargetSelector {
public:
    virtual ~IHttpStateTargetSelector() = default;
    virtual std::string_view SelectStateType(
        const HttpRequest& request,
        const RouteParameters& parameters
    ) const noexcept = 0;
};

/// Caller-owned security decision. Frozen State metadata/read capability never implies permission.
class IHttpStateAuthorizer {
public:
    virtual ~IHttpStateAuthorizer() = default;
    virtual HttpStateAuthorizationDecision Authorize(
        const HttpRequest& request,
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

struct HttpStateInspectionConfiguration final {
    Primitive::TypeDirectoryView Types{};
    const IHttpStateTargetSelector* TargetSelector = nullptr;
    const IHttpStateAuthorizer* Authorizer = nullptr;
};

/// Bounded generic HTTP read/inspect surface for current owner-authoritative State.
///
/// MaximumPayloadBytes is caller-chosen fixed response storage. Web discovers only
/// frozen P1/P3 metadata and invokes the State family's erased read thunk; it never
/// obtains StateOwner authority, mutates canonical State, registers a parallel State
/// registry, or creates a runtime observer callback. Long-lived presentation/session
/// streaming belongs to the WebSocket migration rather than dynamic State topology.
template<std::size_t MaximumPayloadBytes>
class HttpStateInspection final : public IHttpRouteHandler {
    static_assert(MaximumPayloadBytes > 0, "HTTP State inspection requires finite payload capacity");

public:
    WebResult Configure(const HttpStateInspectionConfiguration& configuration) {
        if (!configuration.Types.IsFrozen() ||
            configuration.TargetSelector == nullptr ||
            configuration.Authorizer == nullptr) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        std::lock_guard<std::mutex> lock(_mutex);
        _configuration = configuration;
        _configured = true;
        return WebResult::Success();
    }

    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters& parameters
    ) override {
        const auto method = context.Request().Method();
        if (method != HttpMethod::Get && method != HttpMethod::Head) {
            return HttpHandlerResult::NotHandled();
        }

        HttpStateInspectionConfiguration configuration;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_configured) return HttpHandlerResult::Failure(WebError::InvalidState);
            configuration = _configuration;
        }

        const auto targetName = configuration.TargetSelector->SelectStateType(
            context.Request(), parameters);
        if (targetName.empty()) return CompleteStatus(context, HttpStatus::BadRequest);

        const auto* common = configuration.Types.Find(State::StateFamilyId, targetName);
        if (common == nullptr) return CompleteStatus(context, HttpStatus::NotFound);

        const auto* state = State::GetStateTypeDescriptor(*common);
        if (state == nullptr || state->ValueSchema == nullptr) {
            return CompleteStatus(context, HttpStatus::UnprocessableContent);
        }

        switch (configuration.Authorizer->Authorize(context.Request(), *common)) {
            case HttpStateAuthorizationDecision::Unauthorized:
                return CompleteStatus(context, HttpStatus::Unauthorized);
            case HttpStateAuthorizationDecision::Forbidden:
                return CompleteStatus(context, HttpStatus::Forbidden);
            case HttpStateAuthorizationDecision::Authorized:
                break;
        }

        State::StatePayloadFormat format = State::StatePayloadFormat::DirectBinary;
        std::string_view contentType = "application/octet-stream";
        const auto representation = ResolveRepresentation(context.Request(), format, contentType);
        if (!representation) return CompleteStatus(context, HttpStatus::UnsupportedMediaType);

        const auto maximum = MaximumSerializedBytes(*state, format);
        if (maximum == 0) return CompleteStatus(context, HttpStatus::UnprocessableContent);
        if (maximum > MaximumPayloadBytes) return CompleteStatus(context, HttpStatus::ServiceUnavailable);

        std::array<std::uint8_t, MaximumPayloadBytes> payload{};
        const auto read = State::ReadDynamicState(
            *state, format, payload.data(), MaximumPayloadBytes);
        if (!read) return CompleteStatus(context, MapReadStatus(read.Status));

        auto& response = context.Response();
        auto result = response.Status(HttpStatus::Ok);
        if (!result) return HttpHandlerResult::Handled(result);
        result = response.ContentType(contentType);
        if (!result) return HttpHandlerResult::Handled(result);
        result = response.Header(StateHttpHeaderName::Type, common->CanonicalName);
        if (!result) return HttpHandlerResult::Handled(result);
        result = NumericHeader(response, StateHttpHeaderName::TypeId, state->TypeId.Value());
        if (!result) return HttpHandlerResult::Handled(result);
        result = NumericHeader(response, StateHttpHeaderName::TruthNanoseconds,
                               read.TruthTime.Nanoseconds);
        if (!result) return HttpHandlerResult::Handled(result);
        result = NumericHeader(response, StateHttpHeaderName::TimeReliability,
                               static_cast<std::uint8_t>(read.TruthTime.Reliability));
        if (!result) return HttpHandlerResult::Handled(result);
        result = response.Begin(read.Bytes);
        if (!result) return HttpHandlerResult::Handled(result);
        if (method != HttpMethod::Head && read.Bytes != 0) {
            result = response.Write(payload.data(), read.Bytes);
            if (!result) {
                response.Abort();
                return HttpHandlerResult::Handled(result);
            }
        }
        return HttpHandlerResult::Handled(response.Complete());
    }

private:
    static WebResult ResolveRepresentation(
        const HttpRequest& request,
        State::StatePayloadFormat& format,
        std::string_view& contentType
    ) {
        if (!request.HasHeader(HttpHeaderName::Accept)) {
            format = State::StatePayloadFormat::DirectBinary;
            contentType = "application/octet-stream";
            return WebResult::Success();
        }

        constexpr std::size_t MaximumAcceptBytes = 63;
        const auto length = request.HeaderValueLength(HttpHeaderName::Accept);
        if (length == 0 || length > MaximumAcceptBytes) return WebResult::Failure(WebError::Unsupported);

        std::array<char, MaximumAcceptBytes + 1> buffer{};
        std::size_t written = 0;
        const auto read = request.ReadHeader(HttpHeaderName::Accept, buffer.data(), buffer.size(), written);
        if (!read || written == 0 || written > MaximumAcceptBytes) return WebResult::Failure(WebError::Unsupported);

        std::string_view value(buffer.data(), written);
        const auto comma = value.find(',');
        if (comma != std::string_view::npos) value = value.substr(0, comma);
        const auto semicolon = value.find(';');
        if (semicolon != std::string_view::npos) value = value.substr(0, semicolon);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);

        if (value == "application/octet-stream" || value == "*/*") {
            format = State::StatePayloadFormat::DirectBinary;
            contentType = "application/octet-stream";
            return WebResult::Success();
        }
        if (value == "application/cbor") {
            format = State::StatePayloadFormat::CBOR;
            contentType = "application/cbor";
            return WebResult::Success();
        }
        if (value == "application/json") {
            format = State::StatePayloadFormat::JSON;
            contentType = "application/json";
            return WebResult::Success();
        }
        return WebResult::Failure(WebError::Unsupported);
    }

    static std::size_t MaximumSerializedBytes(
        const State::StateTypeDescriptor& descriptor,
        State::StatePayloadFormat format
    ) noexcept {
        switch (format) {
            case State::StatePayloadFormat::DirectBinary:
                return descriptor.MaximumSerializedValueBytes[0];
            case State::StatePayloadFormat::CBOR:
                return descriptor.MaximumSerializedValueBytes[1];
            case State::StatePayloadFormat::JSON:
                return descriptor.MaximumSerializedValueBytes[2];
        }
        return 0;
    }

    static HttpStatus MapReadStatus(State::StateDynamicReadStatus status) noexcept {
        using S = State::StateDynamicReadStatus;
        switch (status) {
            case S::Success: return HttpStatus::Ok;
            case S::NoValue: return HttpStatus::NotFound;
            case S::InsufficientOutput: return HttpStatus::ServiceUnavailable;
            case S::SerializationFailure: return HttpStatus::InternalServerError;
            case S::UnsupportedFormat: return HttpStatus::UnsupportedMediaType;
        }
        return HttpStatus::InternalServerError;
    }

    template<class TValue>
    static WebResult NumericHeader(
        HttpResponse& response,
        std::string_view name,
        TValue value
    ) {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{}) return WebResult::Failure(WebError::ProtocolError);
        return response.Header(name, std::string_view(
            buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data())));
    }

    static HttpHandlerResult CompleteStatus(WebRequestContext& context, HttpStatus status) {
        auto result = context.Response().Status(status);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

    mutable std::mutex _mutex;
    HttpStateInspectionConfiguration _configuration{};
    bool _configured = false;
};

} // namespace ESPressio::Web
