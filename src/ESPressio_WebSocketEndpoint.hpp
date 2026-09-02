#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Synchronization.hpp>
#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_WebSocket.hpp"

namespace ESPressio::Web {

class IWebSocketEndpointObserver : public Observable::IObserver {
public:
    ~IWebSocketEndpointObserver() override = default;
    virtual void OnWebSocketEndpointStateChanged(WebSocketEndpointState, WebSocketEndpointState) {}
    virtual void OnWebSocketActivity(const WebSocketActivity&) {}
    virtual void OnWebSocketConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketDisconnected(WebSocketConnectionId, const WebSocketCloseReason&) {}
};

class WebSocketEndpoint final : private IWebSocketEndpointPlatformSink {
private:
    static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;

    class EndpointObservable final : public Observable::ThreadSafeObservable {
    public:
        void StateChanged(WebSocketEndpointState previous, WebSocketEndpointState current) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketEndpointStateChanged(previous, current);
                    }
                );
            });
        }
        void Activity(const WebSocketActivity& activity) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketActivity(activity);
                    }
                );
            });
        }
        void Connected(IWebSocketConnection& connection) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketConnected(connection);
                    }
                );
            });
        }
        void Binary(IWebSocketConnection& connection, const uint8_t* data, std::size_t size) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketBinary(connection, data, size);
                    }
                );
            });
        }
        void Text(IWebSocketConnection& connection, std::string_view text) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketText(connection, text);
                    }
                );
            });
        }
        void Disconnected(WebSocketConnectionId id, const WebSocketCloseReason& reason) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketEndpointObserver>(
                    [&](IWebSocketEndpointObserver* observer) {
                        observer->OnWebSocketDisconnected(id, reason);
                    }
                );
            });
        }
    };

    std::shared_ptr<EndpointObservable> ObservableSnapshot() const {
        std::lock_guard<System::Synchronization::Mutex> lock(_observableMutex);
        return _observable;
    }

    std::shared_ptr<EndpointObservable> EnsureObservable() noexcept {
        std::lock_guard<System::Synchronization::Mutex> lock(_observableMutex);
        if (_observable) return _observable;
        try {
            _observable = System::Memory::MakeShared<EndpointObservable, ExternalPreferred>();
        } catch (...) {
            return {};
        }
        return _observable;
    }

    void NotifyStateChanged(WebSocketEndpointState previous, WebSocketEndpointState current) {
        auto observable = ObservableSnapshot();
        if (observable) observable->StateChanged(previous, current);
    }

public:
    WebSocketEndpoint() = default;
    explicit WebSocketEndpoint(IWebSocketEndpointPlatform& platform) { Attach(platform); }
    ~WebSocketEndpoint() { (void)Unbind(); Detach(); }

    WebSocketEndpoint(const WebSocketEndpoint&) = delete;
    WebSocketEndpoint& operator=(const WebSocketEndpoint&) = delete;

    WebResult Attach(IWebSocketEndpointPlatform& platform) {
        if (_platform == &platform) return WebResult::Success();
        if (_platform != nullptr) return WebResult::Failure(WebError::InvalidState);
        const auto previous = _state;
        _platform = &platform;
        _platform->SetSink(this);
        _state = platform.IsBound() ? WebSocketEndpointState::Bound : WebSocketEndpointState::Attached;
        NotifyStateChanged(previous, _state);
        return WebResult::Success();
    }

    void Detach() {
        if (_platform == nullptr) return;
        (void)Unbind();
        _platform->SetSink(nullptr);
        _platform = nullptr;
        SetState(WebSocketEndpointState::Detached);
    }

    bool IsAttached() const noexcept { return _platform != nullptr; }
    bool IsBound() const noexcept { return _platform != nullptr && _platform->IsBound(); }
    WebSocketEndpointState State() const noexcept { return _state; }

    WebResult Bind(const WebSocketEndpointConfiguration& configuration) {
        if (_platform == nullptr) return WebResult::Failure(WebError::InvalidState);
        if (configuration.Path.empty() || configuration.Path.front() != '/') {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        if (IsBound()) return WebResult::Failure(WebError::AlreadyRunning);
        SetState(WebSocketEndpointState::Binding);
        const auto result = _platform->Bind(configuration);
        SetState(result ? WebSocketEndpointState::Bound : WebSocketEndpointState::Attached);
        return result;
    }

    WebResult Unbind() {
        if (_platform == nullptr) return WebResult::Success();
        if (!_platform->IsBound()) {
            SetState(WebSocketEndpointState::Attached);
            return WebResult::Success();
        }
        SetState(WebSocketEndpointState::Unbinding);
        const auto result = _platform->Unbind();
        SetState(_platform->IsBound() ? WebSocketEndpointState::Bound : WebSocketEndpointState::Attached);
        return result;
    }

    Observable::ObserverHandlePtr RegisterObserver(IWebSocketEndpointObserver* observer) {
        if (observer == nullptr) return {};
        auto observable = EnsureObservable();
        return observable
            ? observable->template RegisterObserverAs<IWebSocketEndpointObserver>(observer)
            : Observable::ObserverHandlePtr{};
    }

    void UnregisterObserver(IWebSocketEndpointObserver* observer) {
        auto observable = ObservableSnapshot();
        if (observable) observable->UnregisterObserver(observer);
    }

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
    mutable System::Synchronization::Mutex _observableMutex;
    std::shared_ptr<EndpointObservable> _observable;
    WebSocketEndpointState _state = WebSocketEndpointState::Detached;

    void SetState(WebSocketEndpointState state) {
        if (_state == state) return;
        const auto previous = _state;
        _state = state;
        NotifyStateChanged(previous, state);
    }

    void OnPlatformWebSocketConnected(IWebSocketConnection& connection) override {
        auto observable = ObservableSnapshot();
        if (observable) observable->Connected(connection);
    }
    void OnPlatformWebSocketBinary(IWebSocketConnection& connection, const uint8_t* data, std::size_t size) override {
        auto observable = ObservableSnapshot();
        if (observable) observable->Binary(connection, data, size);
    }
    void OnPlatformWebSocketText(IWebSocketConnection& connection, std::string_view text) override {
        auto observable = ObservableSnapshot();
        if (observable) observable->Text(connection, text);
    }
    void OnPlatformWebSocketDisconnected(WebSocketConnectionId id, const WebSocketCloseReason& reason) override {
        auto observable = ObservableSnapshot();
        if (observable) observable->Disconnected(id, reason);
    }
    void OnPlatformWebSocketActivity(const WebSocketActivity& activity) override {
        auto observable = ObservableSnapshot();
        if (observable) observable->Activity(activity);
    }
};

} // namespace ESPressio::Web
