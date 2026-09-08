#pragma once

#include <memory>
#include <mutex>

#include <ESPressio_Memory.hpp>
#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_Http.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class HttpServerState : uint8_t {
    Stopped = 0,
    Initializing,
    Ready,
    Starting,
    Running,
    Stopping,
    Faulted
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class HttpHandlerDisposition : uint8_t {
    NotHandled = 0,
    Handled
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Result (WebResult): 8 bytes [0 bytes dynamic allocation]
 * - Disposition (HttpHandlerDisposition): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct HttpHandlerResult final {
    WebResult Result;
    HttpHandlerDisposition Disposition = HttpHandlerDisposition::NotHandled;

    constexpr explicit operator bool() const noexcept {
        return static_cast<bool>(Result);
    }

    static constexpr HttpHandlerResult NotHandled() noexcept {
        return {WebResult::Success(), HttpHandlerDisposition::NotHandled};
    }

    static constexpr HttpHandlerResult Handled(WebResult result = WebResult::Success()) noexcept {
        return {result, HttpHandlerDisposition::Handled};
    }

    static constexpr HttpHandlerResult Failure(WebError error, int32_t platformCode = 0) noexcept {
        return {WebResult::Failure(error, platformCode), HttpHandlerDisposition::Handled};
    }
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpRequestHandler {
public:
    virtual ~IHttpRequestHandler() = default;
    virtual HttpHandlerResult Handle(WebRequestContext& context) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpRequestDispatcher {
public:
    virtual ~IHttpRequestDispatcher() = default;
    virtual WebResult Dispatch(
        IHttpRequestPlatform& request,
        IHttpResponsePlatform& response
    ) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpServerPlatform {
public:
    virtual ~IHttpServerPlatform() = default;

    virtual WebCapabilities Capabilities() const noexcept = 0;
    virtual WebResult Initialize(
        const HttpServerConfiguration& configuration,
        IHttpRequestDispatcher& dispatcher
    ) = 0;
    virtual WebResult Start() = 0;
    virtual WebResult Stop() = 0;
    virtual void Reset() noexcept = 0;
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpServerObserver : public Observable::IObserver {
public:
    ~IHttpServerObserver() override = default;
    virtual void OnHttpServerStateChanged(HttpServerState, HttpServerState) {}
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _platform (IHttpServerPlatform&): 4 bytes [0 bytes dynamic allocation]
 * - _observableMutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _observable (std::shared_ptr<LifecycleObservable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * - _mutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _configuration (HttpServerConfiguration): 20 bytes [0 bytes dynamic allocation]
 * - _state (HttpServerState): 1 bytes [0 bytes dynamic allocation]
 * - _handler (IHttpRequestHandler*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 52 bytes [_observableMutex: native synchronization state may allocate platform resources lazily; _observable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _observable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class HttpServer final : public IHttpRequestDispatcher {
private:
    static constexpr auto ExternalPreferred =
        System::Memory::MemoryPolicy::ExternalPreferred;

        /**
     * ESPressio Memory Audit
     * Inherited Memory Total: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
     * Members: none (standalone empty object occupies 1 byte; an eligible empty base may be optimized to 0 bytes).
     * Total Memory: 96 bytes [ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
     * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
     * End ESPressio Memory Audit
     */
class LifecycleObservable final : public Observable::ThreadSafeObservable {
    public:
        void StateChanged(HttpServerState oldState, HttpServerState newState) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IHttpServerObserver>(
                    [&](IHttpServerObserver* observer) {
                        observer->OnHttpServerStateChanged(oldState, newState);
                    }
                );
            });
        }
    };

    std::shared_ptr<LifecycleObservable> ObservableSnapshot() const {
        std::lock_guard<std::mutex> lock(_observableMutex);
        return _observable;
    }

    std::shared_ptr<LifecycleObservable> EnsureObservable() noexcept {
        std::lock_guard<std::mutex> lock(_observableMutex);
        if (_observable) return _observable;
        try {
            _observable = System::Memory::MakeShared<
                LifecycleObservable,
                ExternalPreferred
            >();
        } catch (...) {
            return {};
        }
        return _observable;
    }

    void NotifyStateChanged(HttpServerState oldState, HttpServerState newState) {
        auto observable = ObservableSnapshot();
        if (observable) observable->StateChanged(oldState, newState);
    }

public:
    /// <summary>Creates an allocation-free HTTP server wrapper around the supplied platform implementation.</summary>
    explicit HttpServer(IHttpServerPlatform& platform)
        : _platform(platform) {}

    ~HttpServer() {
        (void)Stop();
        _platform.Reset();
    }

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /// <summary>Registers a lifecycle observer, materializing externally preferred observer bookkeeping on first use.</summary>
    Observable::ObserverHandlePtr RegisterObserver(IHttpServerObserver* observer) {
        if (observer == nullptr) return {};
        auto observable = EnsureObservable();
        return observable
            ? observable->template RegisterObserverAs<IHttpServerObserver>(observer)
            : Observable::ObserverHandlePtr{};
    }

    void UnregisterObserver(IHttpServerObserver* observer) {
        auto observable = ObservableSnapshot();
        if (observable) observable->UnregisterObserver(observer);
    }

    HttpServerState State() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _state;
    }

    WebCapabilities Capabilities() const noexcept {
        return _platform.Capabilities();
    }

    bool Supports(WebCapability capability) const noexcept {
        return HasCapability(Capabilities(), capability);
    }

    HttpServerConfiguration Configuration() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _configuration;
    }

    /// <summary>Initializes the HTTP service without allocating observer infrastructure unless observers were registered.</summary>
    WebResult Initialize(const HttpServerConfiguration& configuration) {
        if (configuration.Port == 0 ||
            configuration.MaximumConnections == 0 ||
            configuration.MaximumHeaderBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        const auto capabilities = Capabilities();
        if (!HasCapability(capabilities, WebCapability::Http)) {
            return WebResult::Failure(WebError::Unsupported);
        }
        if (configuration.TransportMode == HttpTransportMode::Tls &&
            !HasCapability(capabilities, WebCapability::Tls)) {
            return WebResult::Failure(WebError::Unsupported);
        }
        if (configuration.KeepAlive &&
            !HasCapability(capabilities, WebCapability::PersistentConnections)) {
            return WebResult::Failure(WebError::Unsupported);
        }

        HttpServerState oldState;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == HttpServerState::Running ||
                _state == HttpServerState::Starting ||
                _state == HttpServerState::Stopping ||
                _state == HttpServerState::Initializing) {
                return WebResult::Failure(WebError::InvalidState);
            }
            oldState = _state;
            _state = HttpServerState::Initializing;
        }
        NotifyStateChanged(oldState, HttpServerState::Initializing);

        _platform.Reset();
        const auto result = _platform.Initialize(configuration, *this);
        const HttpServerState finalState = result
            ? HttpServerState::Ready
            : HttpServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (result) _configuration = configuration;
            _state = finalState;
        }
        NotifyStateChanged(HttpServerState::Initializing, finalState);
        return result;
    }

    WebResult Start() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == HttpServerState::Running) {
                return WebResult::Failure(WebError::AlreadyRunning);
            }
            if (_state != HttpServerState::Ready) {
                return WebResult::Failure(WebError::InvalidState);
            }
            _state = HttpServerState::Starting;
        }
        NotifyStateChanged(HttpServerState::Ready, HttpServerState::Starting);

        const auto result = _platform.Start();
        const auto finalState = result
            ? HttpServerState::Running
            : HttpServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        NotifyStateChanged(HttpServerState::Starting, finalState);
        return result;
    }

    WebResult Stop() {
        HttpServerState previous;
        bool resetOnly = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            previous = _state;
            if (_state == HttpServerState::Stopped) return WebResult::Success();
            if (_state == HttpServerState::Ready || _state == HttpServerState::Faulted) {
                _state = HttpServerState::Stopped;
                resetOnly = true;
            } else if (_state == HttpServerState::Running) {
                _state = HttpServerState::Stopping;
            } else {
                return WebResult::Failure(WebError::InvalidState);
            }
        }

        if (resetOnly) {
            _platform.Reset();
            NotifyStateChanged(previous, HttpServerState::Stopped);
            return WebResult::Success();
        }

        NotifyStateChanged(previous, HttpServerState::Stopping);
        const auto result = _platform.Stop();
        const auto finalState = result
            ? HttpServerState::Stopped
            : HttpServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        if (result) _platform.Reset();
        NotifyStateChanged(HttpServerState::Stopping, finalState);
        return result;
    }

    WebResult SetRequestHandler(IHttpRequestHandler* handler) noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state == HttpServerState::Running ||
            _state == HttpServerState::Starting ||
            _state == HttpServerState::Stopping) {
            return WebResult::Failure(WebError::InvalidState);
        }
        _handler = handler;
        return WebResult::Success();
    }

    IHttpRequestHandler* RequestHandler() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _handler;
    }

    WebResult Dispatch(
        IHttpRequestPlatform& requestPlatform,
        IHttpResponsePlatform& responsePlatform
    ) override {
        IHttpRequestHandler* handler;
        std::size_t maximumRequestBodyBytes;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state != HttpServerState::Running) {
                return WebResult::Failure(WebError::NotRunning);
            }
            handler = _handler;
            maximumRequestBodyBytes = _configuration.MaximumRequestBodyBytes;
        }

        if (handler == nullptr) {
            return WebResult::Failure(WebError::InvalidState);
        }

        const auto contentLength = requestPlatform.ContentLength();
        if (contentLength.has_value() && *contentLength > maximumRequestBodyBytes) {
            return WebResult::Failure(WebError::RequestTooLarge);
        }

        WebRequestContext context(requestPlatform, responsePlatform);
        const auto handling = handler->Handle(context);
        if (!handling.Result) return handling.Result;

        if (handling.Disposition == HttpHandlerDisposition::NotHandled) {
            if (context.Response().IsCommitted()) {
                return WebResult::Failure(WebError::InvalidState);
            }
            return WebResult::Failure(WebError::NotFound);
        }

        if (!context.Response().IsCompleted()) {
            return WebResult::Failure(WebError::InvalidState);
        }
        return WebResult::Success();
    }

private:
    IHttpServerPlatform& _platform;
    mutable std::mutex _observableMutex;
    std::shared_ptr<LifecycleObservable> _observable;
    mutable std::mutex _mutex;
    HttpServerConfiguration _configuration{};
    HttpServerState _state = HttpServerState::Stopped;
    IHttpRequestHandler* _handler = nullptr;
};

} // namespace ESPressio::Web