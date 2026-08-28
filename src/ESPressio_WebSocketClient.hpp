#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_WebSocket.hpp"

namespace ESPressio::Web {

class IWebSocketClientObserver : public Observable::IObserver {
public:
    ~IWebSocketClientObserver() override = default;
    virtual void OnWebSocketClientConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketClientBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketClientText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketClientDisconnected(const WebSocketCloseReason&) {}
};

class WebSocketClient final : private IWebSocketClientPlatformSink {
private:
    class ClientObservable final : public Observable::ThreadSafeObservable {
    public:
        void Connected(IWebSocketConnection& connection) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientConnected(connection);
                    }
                );
            });
        }

        void Binary(IWebSocketConnection& connection, const uint8_t* data, std::size_t size) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientBinary(connection, data, size);
                    }
                );
            });
        }

        void Text(IWebSocketConnection& connection, std::string_view text) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientText(connection, text);
                    }
                );
            });
        }

        void Disconnected(const WebSocketCloseReason& reason) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientDisconnected(reason);
                    }
                );
            });
        }
    };

public:
    WebSocketClient()
        : _observable(std::make_shared<ClientObservable>()) {}

    explicit WebSocketClient(IWebSocketClientPlatform& platform)
        : WebSocketClient() {
        Attach(platform);
    }

    ~WebSocketClient() { Detach(); }

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    WebResult Attach(IWebSocketClientPlatform& platform) {
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

    Observable::ObserverHandlePtr RegisterObserver(IWebSocketClientObserver* observer) {
        return _observable->template RegisterObserverAs<IWebSocketClientObserver>(observer);
    }

    void UnregisterObserver(IWebSocketClientObserver* observer) {
        _observable->UnregisterObserver(observer);
    }

    WebResult Connect(const WebSocketClientConfiguration& configuration) {
        return _platform == nullptr
            ? WebResult::Failure(WebError::InvalidState)
            : _platform->Connect(configuration);
    }

    WebResult Disconnect(const WebSocketCloseReason& reason = {}) {
        return _platform == nullptr
            ? WebResult::Failure(WebError::InvalidState)
            : _platform->Disconnect(reason);
    }

    bool IsConnected() const noexcept {
        return _platform != nullptr && _platform->IsConnected();
    }

    IWebSocketConnection* Connection() noexcept {
        return _platform == nullptr ? nullptr : _platform->Connection();
    }

private:
    IWebSocketClientPlatform* _platform = nullptr;
    std::shared_ptr<ClientObservable> _observable;

    void OnPlatformWebSocketClientConnected(IWebSocketConnection& connection) override {
        _observable->Connected(connection);
    }

    void OnPlatformWebSocketClientBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        _observable->Binary(connection, data, size);
    }

    void OnPlatformWebSocketClientText(
        IWebSocketConnection& connection,
        std::string_view text
    ) override {
        _observable->Text(connection, text);
    }

    void OnPlatformWebSocketClientDisconnected(const WebSocketCloseReason& reason) override {
        _observable->Disconnected(reason);
    }
};

} // namespace ESPressio::Web
