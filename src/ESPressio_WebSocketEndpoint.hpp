#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <ESPressio_Observable.hpp>

#include "ESPressio_WebSocket.hpp"

namespace ESPressio::Web {

class IWebSocketEndpointObserver : public Observable::IObserver {
public:
    ~IWebSocketEndpointObserver() override = default;
    virtual void OnWebSocketConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketDisconnected(WebSocketConnectionId, const WebSocketCloseReason&) {}
};

class WebSocketEndpoint final :
    public Observable::Observable,
    private IWebSocketEndpointPlatformSink {
public:
    WebSocketEndpoint() = default;
    explicit WebSocketEndpoint(IWebSocketEndpointPlatform& platform) { Attach(platform); }
    ~WebSocketEndpoint() override { Detach(); }

    WebSocketEndpoint(const WebSocketEndpoint&) = delete;
    WebSocketEndpoint& operator=(const WebSocketEndpoint&) = delete;

    WebResult Attach(IWebSocketEndpointPlatform& platform) {
        if (_platform == &platform) return WebResult::Success();
        if (_platform != nullptr) return WebResult::Failure(WebError::InvalidState);
        _platform = &platform;
        _platform->SetSink(this);
        return WebResult::Success();
    }

    void Detach() {
        if (_platform == nullptr) return;
        _platform->SetSink(nullptr);
        _platform = nullptr;
    }

    bool IsAttached() const noexcept { return _platform != nullptr; }

    std::size_t ConnectionCount() const noexcept {
        return _platform == nullptr ? 0 : _platform->ConnectionCount();
    }

    WebResult BroadcastBinary(const uint8_t* data, std::size_t size) {
        if (_platform == nullptr) return WebResult::Failure(WebError::InvalidState);
        if (data == nullptr || size == 0) return WebResult::Failure(WebError::InvalidConfiguration);
        return _platform->BroadcastBinary(data, size);
    }

    WebResult BroadcastText(std::string_view text) {
        if (_platform == nullptr) return WebResult::Failure(WebError::InvalidState);
        return _platform->BroadcastText(text);
    }

    WebResult CloseAll(const WebSocketCloseReason& reason = {}) {
        return _platform == nullptr
            ? WebResult::Failure(WebError::InvalidState)
            : _platform->CloseAll(reason);
    }

private:
    IWebSocketEndpointPlatform* _platform = nullptr;

    void OnPlatformWebSocketConnected(IWebSocketConnection& connection) override {
        ExecuteNotification([&](NotificationContext& context) {
            context.WithObservers<IWebSocketEndpointObserver>(
                [&](IWebSocketEndpointObserver* observer) {
                    observer->OnWebSocketConnected(connection);
                }
            );
        });
    }

    void OnPlatformWebSocketBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        ExecuteNotification([&](NotificationContext& context) {
            context.WithObservers<IWebSocketEndpointObserver>(
                [&](IWebSocketEndpointObserver* observer) {
                    observer->OnWebSocketBinary(connection, data, size);
                }
            );
        });
    }

    void OnPlatformWebSocketText(
        IWebSocketConnection& connection,
        std::string_view text
    ) override {
        ExecuteNotification([&](NotificationContext& context) {
            context.WithObservers<IWebSocketEndpointObserver>(
                [&](IWebSocketEndpointObserver* observer) {
                    observer->OnWebSocketText(connection, text);
                }
            );
        });
    }

    void OnPlatformWebSocketDisconnected(
        WebSocketConnectionId id,
        const WebSocketCloseReason& reason
    ) override {
        ExecuteNotification([&](NotificationContext& context) {
            context.WithObservers<IWebSocketEndpointObserver>(
                [&](IWebSocketEndpointObserver* observer) {
                    observer->OnWebSocketDisconnected(id, reason);
                }
            );
        });
    }
};

} // namespace ESPressio::Web
