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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _platform (IWebSocketEndpointPlatform*): 4 bytes [0 bytes dynamic allocation]
 * - _observableMutex (System::Synchronization::Mutex): 20 bytes [_owned: owned object: 4 bytes; _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * - _observable (std::shared_ptr<EndpointObservable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * - _state (WebSocketEndpointState): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 40 bytes [_observableMutex: _owned: owned object: 4 bytes; _observableMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _observable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketEndpoint final : private IWebSocketEndpointPlatformSink {
private:
    static constexpr auto ExternalPreferred = System::Memory::MemoryPolicy::ExternalPreferred;

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Members: none (standalone empty object occupies 1 byte; an eligible empty base may be optimized to 0 bytes).
 * Total Memory: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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
