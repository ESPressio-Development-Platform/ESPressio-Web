#pragma once

#if !__has_include(<ESPressio_CommandEnvelope.hpp>) || !__has_include(<ESPressio_CommandEvents.hpp>)
#error "ESPressio Web Command integration requires ESPressio-Command and ESPressio-Event."
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include <ESPressio_CommandEnvelope.hpp>
#include <ESPressio_CommandEvents.hpp>
#include <ESPressio_Memory.hpp>

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

        System::Memory::Vector<
            uint8_t,
            System::Memory::MemoryPolicy::ExternalPreferred
        > bytes;
        if (declaredLength.has_value()) bytes.reserve(*declaredLength);

        System::Memory::Vector<
            uint8_t,
            System::Memory::MemoryPolicy::ExternalPreferred
        > chunk(_configuration.ReadChunkBytes);

        bool end = false;
        while (!end) {
            const auto read = context.Request().ReadBody(chunk.data(), chunk.size());
            if (!read) return HttpHandlerResult::Handled(read.Result);
            if (read.BytesRead > chunk.size()) {
                return HttpHandlerResult::Failure(WebError::ProtocolError);
            }
            if (bytes.size() + read.BytesRead >= ESPRESSIO_COMMAND_MAX_RAW_LENGTH) {
                return HttpHandlerResult::Failure(WebError::RequestTooLarge);
            }
            bytes.insert(
                bytes.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(read.BytesRead)
            );
            end = read.EndOfBody;
            if (!end && read.BytesRead == 0) {
                return HttpHandlerResult::Failure(WebError::ProtocolError);
            }
        }

        if (bytes.empty()) return HttpHandlerResult::Failure(WebError::ProtocolError);
        if (declaredLength.has_value() && bytes.size() != *declaredLength) {
            return HttpHandlerResult::Failure(WebError::ProtocolError);
        }

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
    static Command::CommandRequestId NextRequestId() noexcept {
        auto value = _nextRequestId.fetch_add(1, std::memory_order_relaxed);
        if (value == 0) value = _nextRequestId.fetch_add(1, std::memory_order_relaxed);
        return value;
    }

    inline static std::atomic<Command::CommandRequestId> _nextRequestId{1};
    HttpCommandIngressConfiguration _configuration{};
};

} // namespace ESPressio::Web
