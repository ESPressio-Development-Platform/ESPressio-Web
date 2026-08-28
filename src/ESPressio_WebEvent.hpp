#pragma once

#if !__has_include(<ESPressio_IEventTransport.hpp>)
#error "ESPressio Web Event integration requires ESPressio-Event."
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

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
private:
    using PacketBuffer = System::Memory::Vector<
        uint8_t,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

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

        PacketBuffer packet;
        const auto bodyResult = ReadPacketBody(
            context.Request(),
            declaredLength,
            configuration,
            packet
        );
        if (!bodyResult) return HttpHandlerResult::Handled(bodyResult);
        if (packet.empty()) return HttpHandlerResult::Failure(WebError::ProtocolError);

        receiver->ReceiveEventTransportPacket(this, packet.data(), packet.size());

        auto result = context.Response().Status(HttpStatus::Accepted);
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(context.Response().Complete());
    }

private:
    static WebResult ReadPacketBody(
        HttpRequest& request,
        std::optional<std::size_t> declaredLength,
        const HttpEventIngressConfiguration& configuration,
        PacketBuffer& packet
    ) {
        if (declaredLength.has_value()) {
            packet.resize(*declaredLength);
            std::size_t offset = 0;
            while (offset < packet.size()) {
                const auto read = request.ReadBody(
                    packet.data() + offset,
                    packet.size() - offset
                );
                if (!read) return read.Result;
                if (read.BytesRead > packet.size() - offset) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
                offset += read.BytesRead;
                if (read.EndOfBody) {
                    return offset == packet.size()
                        ? WebResult::Success()
                        : WebResult::Failure(WebError::ProtocolError);
                }
                if (read.BytesRead == 0) {
                    return WebResult::Failure(WebError::ProtocolError);
                }
            }
            return WebResult::Failure(WebError::ProtocolError);
        }

        packet.reserve(std::min(
            configuration.ReadChunkBytes,
            configuration.MaximumPacketBytes
        ));

        for (;;) {
            if (packet.size() >= configuration.MaximumPacketBytes) {
                return WebResult::Failure(WebError::RequestTooLarge);
            }

            const auto oldSize = packet.size();
            const auto capacity = std::min(
                configuration.ReadChunkBytes,
                configuration.MaximumPacketBytes - oldSize
            );
            packet.resize(oldSize + capacity);

            const auto read = request.ReadBody(packet.data() + oldSize, capacity);
            if (!read) {
                packet.resize(oldSize);
                return read.Result;
            }
            if (read.BytesRead > capacity) {
                packet.resize(oldSize);
                return WebResult::Failure(WebError::ProtocolError);
            }

            packet.resize(oldSize + read.BytesRead);
            if (read.EndOfBody) return WebResult::Success();
            if (read.BytesRead == 0) {
                return WebResult::Failure(WebError::ProtocolError);
            }
        }
    }

    mutable std::mutex _mutex;
    HttpEventIngressConfiguration _configuration{};
    Event::IEventTransportReceiver* _receiver = nullptr;
};

} // namespace ESPressio::Web
