#pragma once

#if !__has_include(<ESPressio_CommandEnvelope.hpp>) || !__has_include(<ESPressio_CommandEvents.hpp>)
#error "ESPressio Web Command integration requires ESPressio-Command and ESPressio-Event."
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <optional>

#include <ESPressio_CommandEnvelope.hpp>
#include <ESPressio_CommandEvents.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

struct HttpCommandIngressConfiguration final {
    std::size_t ReadChunkBytes = 128;
};

class HttpCommandIngress final : public IHttpRouteHandler {
public:
    WebResult Configure(const HttpCommandIngressConfiguration& configuration) {
        if (configuration.ReadChunkBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        std::lock_guard<std::mutex> lock(_mutex);
        _configuration = configuration;
        return WebResult::Success();
    }

    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters&
    ) override {
        if (context.Request().Method() != HttpMethod::Post) {
            return HttpHandlerResult::NotHandled();
        }

        HttpCommandIngressConfiguration configuration;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            configuration = _configuration;
        }

        const auto declaredLength = context.Request().ContentLength();
        if (declaredLength.has_value() &&
            (*declaredLength == 0 ||
             *declaredLength >= ESPRESSIO_COMMAND_MAX_RAW_LENGTH)) {
            return HttpHandlerResult::Failure(
                *declaredLength >= ESPRESSIO_COMMAND_MAX_RAW_LENGTH
                    ? WebError::RequestTooLarge
                    : WebError::ProtocolError
            );
        }

        Command::CommandRequestEnvelope envelope;
        envelope.RequestId = NextRequestId();
        envelope.ResponseExpectation = Command::CommandResponseExpectation::Acceptance;
        envelope.ResponseMode = Command::CommandResponseMode::Single;

        const auto bodyResult = ReadCommandBody(
            context.Request(),
            declaredLength,
            configuration.ReadChunkBytes,
            envelope
        );
        if (!bodyResult) return HttpHandlerResult::Handled(bodyResult);
        if (envelope.RawLength == 0) {
            return HttpHandlerResult::Failure(WebError::ProtocolError);
        }

        try {
            (new Event::InboundCommandEvent(envelope))->Queue();
        } catch (const std::bad_alloc&) {
            return HttpHandlerResult::Failure(WebError::ResourceExhausted);
        } catch (...) {
            return HttpHandlerResult::Failure(WebError::PlatformFailure);
        }

        auto result = context.Response().Status(HttpStatus::Accepted);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

private:
    /// <summary>Streams one bounded HTTP command body directly into its destination envelope.</summary>
    /// <remarks>This avoids a transient request-body allocation and the subsequent copy into <c>CommandRequestEnvelope::Raw</c>.</remarks>
    static WebResult ReadCommandBody(
        HttpRequest& request,
        std::optional<std::size_t> declaredLength,
        std::size_t readChunkBytes,
        Command::CommandRequestEnvelope& envelope
    ) {
        constexpr std::size_t MaximumBytes = ESPRESSIO_COMMAND_MAX_RAW_LENGTH - 1;
        auto* destination = reinterpret_cast<uint8_t*>(envelope.Raw.data());
        std::size_t offset = 0;

        const auto finish = [&]() {
            envelope.Raw[offset] = '\0';
            envelope.RawLength = static_cast<uint16_t>(offset);
            return WebResult::Success();
        };

        if (declaredLength.has_value()) {
            while (offset < *declaredLength) {
                const auto read = request.ReadBody(
                    destination + offset,
                    *declaredLength - offset
                );
                if (!read) return read.Result;
                if (read.BytesRead > *declaredLength - offset) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
                offset += read.BytesRead;
                if (read.EndOfBody) {
                    return offset == *declaredLength
                        ? finish()
                        : WebResult::Failure(WebError::ProtocolError);
                }
                if (read.BytesRead == 0) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
            }
            return WebResult::Failure(WebError::ProtocolError);
        }

        for (;;) {
            if (offset >= MaximumBytes) {
                return WebResult::Failure(WebError::RequestTooLarge);
            }

            const std::size_t capacity = std::min(
                readChunkBytes,
                MaximumBytes - offset
            );
            const auto read = request.ReadBody(destination + offset, capacity);
            if (!read) return read.Result;
            if (read.BytesRead > capacity) {
                return WebResult::Failure(WebError::ProtocolError);
            }

            offset += read.BytesRead;
            if (read.EndOfBody) return finish();
            if (read.BytesRead == 0) {
                return WebResult::Failure(WebError::ProtocolError);
            }
        }
    }

    static Command::CommandRequestId NextRequestId() noexcept {
        auto value = _nextRequestId.fetch_add(1, std::memory_order_relaxed);
        if (value == 0) value = _nextRequestId.fetch_add(1, std::memory_order_relaxed);
        return value;
    }

    inline static std::atomic<Command::CommandRequestId> _nextRequestId{1};
    mutable std::mutex _mutex;
    HttpCommandIngressConfiguration _configuration{};
};

} // namespace ESPressio::Web
