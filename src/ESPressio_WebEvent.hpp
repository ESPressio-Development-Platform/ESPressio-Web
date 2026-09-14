#pragma once

#if !__has_include(<ESPressio_EventTypeDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Web Event integration requires the final ESPressio-Event and ESPressio-Primitive redesign surfaces."
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

#include <ESPressio_EventTypeDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

enum class HttpEventAuthorizationDecision : std::uint8_t {
    Authorized,
    Unauthorized,
    Forbidden
};

/// Caller-owned target selection. Web owns no Event registry and imposes no URL shape.
class IHttpEventTargetSelector {
public:
    virtual ~IHttpEventTargetSelector() = default;
    virtual std::string_view SelectEventType(
        const HttpRequest& request,
        const RouteParameters& parameters
    ) const noexcept = 0;
};

/// Caller-owned authorization. TypeDirectory/P3 discovery never grants permission.
class IHttpEventAuthorizer {
public:
    virtual ~IHttpEventAuthorizer() = default;
    virtual HttpEventAuthorizationDecision Authorize(
        const HttpRequest& request,
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

struct HttpEventIngressConfiguration final {
    Primitive::TypeDirectoryView Types{};
    const IHttpEventTargetSelector* TargetSelector = nullptr;
    const IHttpEventAuthorizer* Authorizer = nullptr;
    std::size_t ReadChunkBytes = 128;
};

/// Bounded HTTP -> local Event dispatch through the Event family's final descriptor API.
///
/// This is not an Event transport and owns no listener/subscription registry. Event target
/// topology remains initialization-time/frozen; persistent WebSocket delivery is composed
/// through the neutral transport work in D10-06 rather than recreated here.
template<std::size_t MaximumPayloadBytes>
class HttpEventIngress final : public IHttpRouteHandler {
    static_assert(MaximumPayloadBytes > 0, "HTTP Event ingress requires finite payload capacity");

public:
    WebResult Configure(const HttpEventIngressConfiguration& configuration) {
        if (!configuration.Types.IsFrozen() ||
            configuration.TargetSelector == nullptr ||
            configuration.Authorizer == nullptr ||
            configuration.ReadChunkBytes == 0) {
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
        if (context.Request().Method() != HttpMethod::Post) {
            return HttpHandlerResult::NotHandled();
        }

        HttpEventIngressConfiguration configuration;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_configured) return HttpHandlerResult::Failure(WebError::InvalidState);
            configuration = _configuration;
        }

        const auto targetName = configuration.TargetSelector->SelectEventType(
            context.Request(), parameters);
        if (targetName.empty()) return CompleteStatus(context, HttpStatus::BadRequest);

        const auto* common = configuration.Types.Find(Event::EventFamilyId, targetName);
        if (common == nullptr) return CompleteStatus(context, HttpStatus::NotFound);

        const auto* event = Event::GetEventTypeDescriptor(*common);
        if (event == nullptr || event->Schema == nullptr) {
            return CompleteStatus(context, HttpStatus::UnprocessableContent);
        }

        switch (configuration.Authorizer->Authorize(context.Request(), *common)) {
            case HttpEventAuthorizationDecision::Unauthorized:
                return CompleteStatus(context, HttpStatus::Unauthorized);
            case HttpEventAuthorizationDecision::Forbidden:
                return CompleteStatus(context, HttpStatus::Forbidden);
            case HttpEventAuthorizationDecision::Authorized:
                break;
        }

        if (!event->DynamicallyConstructible || event->DispatchSerialized == nullptr) {
            return CompleteStatus(context, HttpStatus::UnprocessableContent);
        }

        Event::EventPayloadFormat format{};
        if (!ResolvePayloadFormat(context.Request(), format)) {
            return CompleteStatus(context, HttpStatus::UnsupportedMediaType);
        }

        const std::size_t schemaLimit = MaximumSchemaBytes(*event->Schema, format);
        if (schemaLimit == 0) return CompleteStatus(context, HttpStatus::UnprocessableContent);
        const std::size_t ingressLimit = std::min(schemaLimit, MaximumPayloadBytes);

        std::array<std::uint8_t, MaximumPayloadBytes> payload{};
        std::size_t payloadBytes = 0;
        const auto bodyResult = ReadBoundedBody(
            context.Request(), context.Request().ContentLength(), configuration.ReadChunkBytes,
            ingressLimit, payload, payloadBytes);
        if (!bodyResult) {
            if (bodyResult.Error == WebError::RequestTooLarge)
                return CompleteStatus(context, HttpStatus::PayloadTooLarge);
            if (bodyResult.Error == WebError::ProtocolError)
                return CompleteStatus(context, HttpStatus::BadRequest);
            return HttpHandlerResult::Handled(bodyResult);
        }

        const auto dispatched = Event::DispatchDynamicEvent(
            *event, format, payload.data(), payloadBytes);
        return CompleteStatus(context, MapDispatchStatus(dispatched.Status));
    }

private:
    static WebResult ResolvePayloadFormat(
        const HttpRequest& request,
        Event::EventPayloadFormat& format
    ) {
        constexpr std::size_t MaximumContentTypeBytes = 63;
        if (!request.HasHeader(HttpHeaderName::ContentType))
            return WebResult::Failure(WebError::Unsupported);
        const auto length = request.HeaderValueLength(HttpHeaderName::ContentType);
        if (length == 0 || length > MaximumContentTypeBytes)
            return WebResult::Failure(WebError::Unsupported);

        std::array<char, MaximumContentTypeBytes + 1> buffer{};
        std::size_t written = 0;
        const auto read = request.ReadHeader(
            HttpHeaderName::ContentType, buffer.data(), buffer.size(), written);
        if (!read || written == 0 || written > MaximumContentTypeBytes)
            return WebResult::Failure(WebError::Unsupported);

        std::string_view value(buffer.data(), written);
        const auto semicolon = value.find(';');
        if (semicolon != std::string_view::npos) value = value.substr(0, semicolon);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);

        if (value == "application/json") {
            format = Event::EventPayloadFormat::JSON;
            return WebResult::Success();
        }
        if (value == "application/cbor") {
            format = Event::EventPayloadFormat::CBOR;
            return WebResult::Success();
        }
        if (value == "application/octet-stream") {
            format = Event::EventPayloadFormat::DirectBinary;
            return WebResult::Success();
        }
        return WebResult::Failure(WebError::Unsupported);
    }

    static std::size_t MaximumSchemaBytes(
        const Serializable::StaticSchemaDescriptor& schema,
        Event::EventPayloadFormat format
    ) noexcept {
        switch (format) {
            case Event::EventPayloadFormat::DirectBinary: return schema.MaximumDirectBinaryBytes;
            case Event::EventPayloadFormat::CBOR: return schema.MaximumCborBytes;
            case Event::EventPayloadFormat::JSON: return schema.MaximumJsonBytes;
        }
        return 0;
    }

    static WebResult ReadBoundedBody(
        HttpRequest& request,
        std::optional<std::size_t> declaredLength,
        std::size_t readChunkBytes,
        std::size_t limit,
        std::array<std::uint8_t, MaximumPayloadBytes>& destination,
        std::size_t& bytesWritten
    ) {
        bytesWritten = 0;
        if (limit == 0) return WebResult::Failure(WebError::RequestTooLarge);
        if (declaredLength.has_value()) {
            if (*declaredLength == 0) return WebResult::Failure(WebError::ProtocolError);
            if (*declaredLength > limit) return WebResult::Failure(WebError::RequestTooLarge);
            while (bytesWritten < *declaredLength) {
                const auto remaining = *declaredLength - bytesWritten;
                const auto capacity = std::min(readChunkBytes, remaining);
                const auto read = request.ReadBody(destination.data() + bytesWritten, capacity);
                if (!read) return read.Result;
                if (read.BytesRead > capacity) return WebResult::Failure(WebError::ProtocolError);
                bytesWritten += read.BytesRead;
                if (read.EndOfBody) {
                    return bytesWritten == *declaredLength
                        ? WebResult::Success()
                        : WebResult::Failure(WebError::ProtocolError);
                }
                if (read.BytesRead == 0) return WebResult::Failure(WebError::ProtocolError);
            }
            return WebResult::Failure(WebError::ProtocolError);
        }

        for (;;) {
            if (bytesWritten >= limit) return WebResult::Failure(WebError::RequestTooLarge);
            const auto capacity = std::min(readChunkBytes, limit - bytesWritten);
            const auto read = request.ReadBody(destination.data() + bytesWritten, capacity);
            if (!read) return read.Result;
            if (read.BytesRead > capacity) return WebResult::Failure(WebError::ProtocolError);
            bytesWritten += read.BytesRead;
            if (read.EndOfBody) {
                return bytesWritten == 0
                    ? WebResult::Failure(WebError::ProtocolError)
                    : WebResult::Success();
            }
            if (read.BytesRead == 0) return WebResult::Failure(WebError::ProtocolError);
        }
    }

    static HttpStatus MapDispatchStatus(Event::EventDynamicDispatchStatus status) noexcept {
        using S = Event::EventDynamicDispatchStatus;
        switch (status) {
            case S::Accepted: return HttpStatus::Accepted;
            case S::NotConstructible:
            case S::SchemaOrDecodeFailure:
                return HttpStatus::UnprocessableContent;
            case S::UnsupportedFormat: return HttpStatus::UnsupportedMediaType;
            case S::CapacityUnavailable: return HttpStatus::TooManyRequests;
            case S::NotInitialized:
            case S::Stopping:
            case S::IdentityUnavailable:
            case S::IdentifierExhausted:
                return HttpStatus::ServiceUnavailable;
        }
        return HttpStatus::InternalServerError;
    }

    static HttpHandlerResult CompleteStatus(WebRequestContext& context, HttpStatus status) {
        auto result = context.Response().Status(status);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

    mutable std::mutex _mutex;
    HttpEventIngressConfiguration _configuration{};
    bool _configured = false;
};

} // namespace ESPressio::Web
