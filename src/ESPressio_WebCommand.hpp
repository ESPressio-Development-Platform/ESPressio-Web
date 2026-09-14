#pragma once

#if !__has_include(<ESPressio_CommandDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Web Command integration requires the final ESPressio-Command and ESPressio-Primitive redesign surfaces."
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

#include <ESPressio_CommandDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

enum class HttpCommandAuthorizationDecision : std::uint8_t {
    Authorized,
    Unauthorized,
    Forbidden
};

/// Caller-owned target selection policy. Applications retain ownership of URL shape;
/// the selector returns only the canonical Command type name borrowed from the request/route.
class IHttpCommandTargetSelector {
public:
    virtual ~IHttpCommandTargetSelector() = default;
    virtual std::string_view SelectCommandType(
        const HttpRequest& request,
        const RouteParameters& parameters
    ) const noexcept = 0;
};

/// Caller-owned security decision. TypeDirectory/schema discovery never implies permission.
class IHttpCommandAuthorizer {
public:
    virtual ~IHttpCommandAuthorizer() = default;
    virtual HttpCommandAuthorizationDecision Authorize(
        const HttpRequest& request,
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

struct HttpCommandIngressConfiguration final {
    Primitive::TypeDirectoryView Types{};
    const IHttpCommandTargetSelector* TargetSelector = nullptr;
    const IHttpCommandAuthorizer* Authorizer = nullptr;
    std::size_t ReadChunkBytes = 128;
};

/// Bounded generic HTTP -> typed Command ingress.
///
/// MaximumPayloadBytes is caller-chosen fixed storage. Every request is additionally
/// constrained by the selected Command's P3 schema for the requested representation.
/// The adapter owns no Command registry, factory, retry queue, Event broker or heap fallback.
template<std::size_t MaximumPayloadBytes>
class HttpCommandIngress final : public IHttpRouteHandler {
    static_assert(MaximumPayloadBytes > 0, "HTTP Command ingress requires finite payload capacity");

public:
    WebResult Configure(const HttpCommandIngressConfiguration& configuration) {
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

        HttpCommandIngressConfiguration configuration;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_configured) return HttpHandlerResult::Failure(WebError::InvalidState);
            configuration = _configuration;
        }

        const auto targetName = configuration.TargetSelector->SelectCommandType(
            context.Request(), parameters);
        if (targetName.empty()) return CompleteStatus(context, HttpStatus::BadRequest);

        const auto* common = configuration.Types.Find(Command::CommandFamilyId, targetName);
        if (common == nullptr) return CompleteStatus(context, HttpStatus::NotFound);

        const auto* command = Command::GetCommandTypeDescriptor(*common);
        if (command == nullptr || command->RequestSchema == nullptr) {
            return CompleteStatus(context, HttpStatus::UnprocessableContent);
        }

        switch (configuration.Authorizer->Authorize(context.Request(), *common)) {
            case HttpCommandAuthorizationDecision::Unauthorized:
                return CompleteStatus(context, HttpStatus::Unauthorized);
            case HttpCommandAuthorizationDecision::Forbidden:
                return CompleteStatus(context, HttpStatus::Forbidden);
            case HttpCommandAuthorizationDecision::Authorized:
                break;
        }

        if (command->DynamicConstruction == Command::CommandDynamicConstructionMode::RequesterRequired) {
            return CompleteStatus(context, HttpStatus::Conflict);
        }
        if (command->DynamicConstruction != Command::CommandDynamicConstructionMode::FireAndForget ||
            command->SubmitSerialized == nullptr) {
            return CompleteStatus(context, HttpStatus::UnprocessableContent);
        }

        Command::CommandPayloadFormat format{};
        const auto formatResult = ResolvePayloadFormat(context.Request(), format);
        if (!formatResult) return CompleteStatus(context, HttpStatus::UnsupportedMediaType);

        const std::size_t schemaLimit = MaximumSchemaBytes(*command->RequestSchema, format);
        if (schemaLimit == 0) return CompleteStatus(context, HttpStatus::UnprocessableContent);
        const std::size_t ingressLimit = std::min(schemaLimit, MaximumPayloadBytes);

        std::array<std::uint8_t, MaximumPayloadBytes> payload{};
        std::size_t payloadBytes = 0;
        const auto bodyResult = ReadBoundedBody(
            context.Request(),
            context.Request().ContentLength(),
            configuration.ReadChunkBytes,
            ingressLimit,
            payload,
            payloadBytes);
        if (!bodyResult) {
            if (bodyResult.Error == WebError::RequestTooLarge) {
                return CompleteStatus(context, HttpStatus::PayloadTooLarge);
            }
            if (bodyResult.Error == WebError::ProtocolError) {
                return CompleteStatus(context, HttpStatus::BadRequest);
            }
            return HttpHandlerResult::Handled(bodyResult);
        }

        const auto submitted = Command::SubmitDynamicCommand(
            *command, format, payload.data(), payloadBytes);
        return CompleteStatus(context, MapSubmissionStatus(submitted.Status));
    }

private:
    static WebResult ResolvePayloadFormat(
        const HttpRequest& request,
        Command::CommandPayloadFormat& format
    ) {
        constexpr std::size_t MaximumContentTypeBytes = 63;
        if (!request.HasHeader(HttpHeaderName::ContentType)) {
            return WebResult::Failure(WebError::Unsupported);
        }
        const auto length = request.HeaderValueLength(HttpHeaderName::ContentType);
        if (length == 0 || length > MaximumContentTypeBytes) {
            return WebResult::Failure(WebError::Unsupported);
        }

        std::array<char, MaximumContentTypeBytes + 1> buffer{};
        std::size_t written = 0;
        const auto read = request.ReadHeader(
            HttpHeaderName::ContentType, buffer.data(), buffer.size(), written);
        if (!read || written == 0 || written > MaximumContentTypeBytes) {
            return WebResult::Failure(WebError::Unsupported);
        }

        std::string_view value(buffer.data(), written);
        const auto semicolon = value.find(';');
        if (semicolon != std::string_view::npos) value = value.substr(0, semicolon);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);

        if (value == "application/json") {
            format = Command::CommandPayloadFormat::JSON;
            return WebResult::Success();
        }
        if (value == "application/cbor") {
            format = Command::CommandPayloadFormat::CBOR;
            return WebResult::Success();
        }
        if (value == "application/octet-stream") {
            format = Command::CommandPayloadFormat::DirectBinary;
            return WebResult::Success();
        }
        return WebResult::Failure(WebError::Unsupported);
    }

    static std::size_t MaximumSchemaBytes(
        const Serializable::StaticSchemaDescriptor& schema,
        Command::CommandPayloadFormat format
    ) noexcept {
        switch (format) {
            case Command::CommandPayloadFormat::DirectBinary: return schema.MaximumDirectBinaryBytes;
            case Command::CommandPayloadFormat::CBOR: return schema.MaximumCborBytes;
            case Command::CommandPayloadFormat::JSON: return schema.MaximumJsonBytes;
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

    static HttpStatus MapSubmissionStatus(Command::CommandSubmissionStatus status) noexcept {
        using S = Command::CommandSubmissionStatus;
        switch (status) {
            case S::Accepted: return HttpStatus::Accepted;
            case S::SchemaOrDecodeFailure: return HttpStatus::UnprocessableContent;
            case S::InvalidRequest: return HttpStatus::BadRequest;
            case S::InvalidTarget: return HttpStatus::NotFound;
            case S::CapacityUnavailable:
            case S::ResponseCapacityUnavailable:
            case S::LedgerCapacityUnavailable:
            case S::Discarded:
                return HttpStatus::TooManyRequests;
            case S::NotInitialized:
            case S::Stopping:
            case S::IdentityUnavailable:
            case S::IdentifierExhausted:
            case S::HandlerUnavailable:
            case S::PersistenceUnavailable:
            case S::TransportUnavailable:
                return HttpStatus::ServiceUnavailable;
        }
        return HttpStatus::InternalServerError;
    }

    static HttpHandlerResult CompleteStatus(
        WebRequestContext& context,
        HttpStatus status
    ) {
        auto result = context.Response().Status(status);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

    mutable std::mutex _mutex;
    HttpCommandIngressConfiguration _configuration{};
    bool _configured = false;
};

} // namespace ESPressio::Web
