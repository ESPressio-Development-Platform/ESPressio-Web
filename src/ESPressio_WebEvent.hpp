#pragma once

#if !__has_include(<ESPressio_IEventTransport.hpp>)
#error "ESPressio Web Event integration requires ESPressio-Event."
#endif

#include <cstddef>
#include <cstdint>
#include <mutex>

#include <ESPressio_IEventTransport.hpp>
#include <ESPressio_Memory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

struct HttpEventIngressConfiguration final {
    std::size_t MaximumPacketBytes = 64u * 1024u;
    std::size_t ReadChunkBytes = 1024;
};

class HttpEventIngress final :
    public Event::IEventTransport,
    public IHttpRouteHandler {
public:
    WebResult Configure(const HttpEventIngressConfiguration& configuration) {
        if (configuration.MaximumPacketBytes == 0 ||
            configuration.ReadChunkBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        std::lock_guard<std::mutex> lock(_mutex);
        _configuration = configuration;
        return WebResult::Success();
    }

    // HTTP server ingress is intentionally receive-only. Event transport
    // egress requires a persistent/peer-addressable transport such as
    // WebSocket, TCP, UDP, ESP-NOW, etc.
    bool Send(const Event::EventTransportPacket&) override { return false; }

    void SetReceiver(Event::IEventTransportReceiver* receiver) override {
        std::lock_guard<std::mutex> lock(_mutex);
        _receiver = receiver;
    }

    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters&
    ) override {
        if (context.Request().Method() != HttpMethod::Post) {
            return HttpHandlerResult::NotHandled();
        }

        Event::IEventTransportReceiver* receiver;
        HttpEventIngressConfiguration configuration;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            receiver = _receiver;
            configuration = _configuration;
        }
        if (receiver == nullptr) {
            return HttpHandlerResult::Failure(WebError::NotRunning);
        }

        const auto declaredLength = context.Request().ContentLength();
        if (declaredLength.has_value() &&
            (*declaredLength == 0 || *declaredLength > configuration.MaximumPacketBytes)) {
            return HttpHandlerResult::Failure(
                *declaredLength > configuration.MaximumPacketBytes
                    ? WebError::RequestTooLarge
                    : WebError::ProtocolError
            );
        }

        System::Memory::Vector<
            uint8_t,
            System::Memory::MemoryPolicy::ExternalPreferred
        > packet;
        if (declaredLength.has_value()) packet.reserve(*declaredLength);

        System::Memory::Vector<
            uint8_t,
            System::Memory::MemoryPolicy::ExternalPreferred
        > chunk(configuration.ReadChunkBytes);

        bool end = false;
        while (!end) {
            const auto read = context.Request().ReadBody(chunk.data(), chunk.size());
            if (!read) return HttpHandlerResult::Handled(read.Result);
            if (read.BytesRead > chunk.size()) {
                return HttpHandlerResult::Failure(WebError::ProtocolError);
            }
            if (packet.size() + read.BytesRead > configuration.MaximumPacketBytes) {
                return HttpHandlerResult::Failure(WebError::RequestTooLarge);
            }
            packet.insert(
                packet.end(),
                chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(read.BytesRead)
            );
            end = read.EndOfBody;
            if (!end && read.BytesRead == 0) {
                return HttpHandlerResult::Failure(WebError::ProtocolError);
            }
        }

        if (packet.empty()) return HttpHandlerResult::Failure(WebError::ProtocolError);
        if (declaredLength.has_value() && packet.size() != *declaredLength) {
            return HttpHandlerResult::Failure(WebError::ProtocolError);
        }

        receiver->ReceiveEventTransportPacket(this, packet.data(), packet.size());

        auto result = context.Response().Status(HttpStatus::Accepted);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

private:
    mutable std::mutex _mutex;
    HttpEventIngressConfiguration _configuration{};
    Event::IEventTransportReceiver* _receiver = nullptr;
};

} // namespace ESPressio::Web
