#pragma once

#include <memory>
#include <mutex>

#include <ESPressio_Memory.hpp>
#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_Http.hpp"

namespace ESPressio::Web {

enum class HttpServerState : uint8_t {
    Stopped = 0,
    Initializing,
    Ready,
    Starting,
    Running,
    Stopping,
    Faulted
};

class IHttpRequestHandler {
public:
    virtual ~IHttpRequestHandler() = default;
    virtual WebResult Handle(WebRequestContext& context) = 0;
};

class IHttpRequestDispatcher {
public:
    virtual ~IHttpRequestDispatcher() = default;
    virtual WebResult Dispatch(
        IHttpRequestPlatform& request,
        IHttpResponsePlatform& response
    ) = 0;
};

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

class IHttpServerObserver : public Observable::IObserver {
public:
    ~IHttpServerObserver() override = default;
    virtual void OnHttpServerStateChanged(
        HttpServerState,
        HttpServerState
    ) {}
};

class HttpServer final : public IHttpRequestDispatcher {
private:
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

public:
    explicit HttpServer(IHttpServerPlatform& platform)
        : _platform(platform),
          _observable(System::Memory::MakeShared<
              LifecycleObservable,
              System::Memory::MemoryPolicy::ExternalPreferred
          >()) {}

    ~HttpServer() {
        Stop();
        _platform.Reset();
    }

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    Observable::ObserverHandlePtr RegisterObserver(IHttpServerObserver* observer) {
        return _observable->template RegisterObserverAs<IHttpServerObserver>(observer);
    }

    void UnregisterObserver(IHttpServerObserver* observer) {
        _observable->UnregisterObserver(observer);
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

    WebResult Initialize(const HttpServerConfiguration& configuration) {
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
        _observable->StateChanged(oldState, HttpServerState::Initializing);

        _platform.Reset();
        const auto result = _platform.Initialize(configuration, *this);

        HttpServerState finalState;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (result) {
                _configuration = configuration;
                finalState = HttpServerState::Ready;
            } else {
                finalState = HttpServerState::Faulted;
            }
            _state = finalState;
        }
        _observable->StateChanged(HttpServerState::Initializing, finalState);
        return result;
    }

    WebResult Start() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == HttpServerState::Running) {
                return WebResult::Failure(WebError::AlreadyRunning);
            }
            if (_state != HttpServerState::Ready && _state != HttpServerState::Stopped) {
                return WebResult::Failure(WebError::InvalidState);
            }
            if (_state == HttpServerState::Stopped) {
                return WebResult::Failure(WebError::InvalidState);
            }
            _state = HttpServerState::Starting;
        }
        _observable->StateChanged(HttpServerState::Ready, HttpServerState::Starting);

        const auto result = _platform.Start();
        const auto finalState = result ? HttpServerState::Running : HttpServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        _observable->StateChanged(HttpServerState::Starting, finalState);
        return result;
    }

    WebResult Stop() {
        HttpServerState previous;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            previous = _state;
            if (_state == HttpServerState::Stopped) return WebResult::Success();
            if (_state == HttpServerState::Ready || _state == HttpServerState::Faulted) {
                _platform.Reset();
                _state = HttpServerState::Stopped;
                _observable->StateChanged(previous, HttpServerState::Stopped);
                return WebResult::Success();
            }
            if (_state != HttpServerState::Running) {
                return WebResult::Failure(WebError::InvalidState);
            }
            _state = HttpServerState::Stopping;
        }
        _observable->StateChanged(previous, HttpServerState::Stopping);

        const auto result = _platform.Stop();
        const auto finalState = result ? HttpServerState::Stopped : HttpServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        if (result) _platform.Reset();
        _observable->StateChanged(HttpServerState::Stopping, finalState);
        return result;
    }

    void SetRequestHandler(IHttpRequestHandler* handler) noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        _handler = handler;
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
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state != HttpServerState::Running) {
                return WebResult::Failure(WebError::NotRunning);
            }
            handler = _handler;
        }

        if (handler == nullptr) {
            return WebResult::Failure(WebError::InvalidState);
        }

        WebRequestContext context(requestPlatform, responsePlatform);
        return handler->Handle(context);
    }

private:
    IHttpServerPlatform& _platform;
    std::shared_ptr<LifecycleObservable> _observable;
    mutable std::mutex _mutex;
    HttpServerConfiguration _configuration{};
    HttpServerState _state = HttpServerState::Stopped;
    IHttpRequestHandler* _handler = nullptr;
};

} // namespace ESPressio::Web
