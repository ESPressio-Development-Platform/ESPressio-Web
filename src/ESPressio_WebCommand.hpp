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
#include <ESPressio_Memory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

struct HttpCommandIngressConfiguration final {
    std::size_t ReadChunkBytes = 128;
};

class HttpCommandIngress final : public IHttpRouteHandler {
private:
    using CommandBuffer = System::Memory::Vector<
        uint8_t,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

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

        CommandBuffer bytes;
        const auto bodyResult = ReadCommandBody(
            context.Request(),
            declaredLength,
            configuration.ReadChunkBytes,
            bytes
        );
        if (!bodyResult) return HttpHandlerResult::Handled(bodyResult);
        if (bytes.empty()) return HttpHandlerResult::Failure(WebError::ProtocolError);

        Command::CommandRequestEnvelope envelope;
        envelope.RequestId = NextRequestId();
        envelope.ResponseExpectation = Command::CommandResponseExpectation::Acceptance;
        envelope.ResponseMode = Command::CommandResponseMode::Single;
        if (!envelope.SetRaw(
                reinterpret_cast<const char*>(bytes.data()),
                bytes.size())) {
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
    static WebResult ReadCommandBody(
        HttpRequest& request,
        std::optional<std::size_t> declaredLength,
        std::size_t readChunkBytes,
        CommandBuffer& bytes
    ) {
        constexpr std::size_t MaximumBytes = ESPRESSIO_COMMAND_MAX_RAW_LENGTH - 1;

        if (declaredLength.has_value()) {
            bytes.resize(*declaredLength);
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                const auto read = request.ReadBody(
                    bytes.data() + offset,
                    bytes.size() - offset
                );
                if (!read) return read.Result;
                if (read.BytesRead > bytes.size() - offset) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
                offset += read.BytesRead;
                if (read.EndOfBody) {
                    return offset == bytes.size()
                        ? WebResult::Success()
                        : WebResult::Failure(WebError::ProtocolError);
                }
                if (read.BytesRead == 0) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
            }
            return WebResult::Failure(WebError::ProtocolError);
        }

        bytes.reserve(std::min(readChunkBytes, MaximumBytes));

        for (;;) {
            if (bytes.size() >= MaximumBytes) {
                return WebResult::Failure(WebError::RequestTooLarge);
            }

            const auto oldSize = bytes.size();
            const auto capacity = std::min(
                readChunkBytes,
                MaximumBytes - oldSize
            );
            bytes.resize(oldSize + capacity);

            const auto read = request.ReadBody(bytes.data() + oldSize, capacity);
            if (!read) {
                bytes.resize(oldSize);
                return read.Result;
            }
            if (read.BytesRead > capacity) {
                bytes.resize(oldSize);
                return WebResult::Failure(WebError::ProtocolError);
            }

            bytes.resize(oldSize + read.BytesRead);
            if (read.EndOfBody) return WebResult::Success();
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
