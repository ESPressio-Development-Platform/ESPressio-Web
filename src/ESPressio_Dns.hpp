#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

#include <ESPressio_Memory.hpp>
#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

enum class DnsRecordType : uint16_t {
    A = 1,
    Ns = 2,
    CName = 5,
    Soa = 6,
    Ptr = 12,
    Mx = 15,
    Txt = 16,
    Aaaa = 28,
    Any = 255
};

enum class DnsRecordClass : uint16_t {
    Internet = 1,
    Any = 255
};

enum class DnsResponseCode : uint8_t {
    NoError = 0,
    FormatError = 1,
    ServerFailure = 2,
    NameError = 3,
    NotImplemented = 4,
    Refused = 5
};

enum class DnsAddressFamily : uint8_t {
    IPv4 = 4,
    IPv6 = 6
};

struct DnsAddress final {
    DnsAddressFamily Family = DnsAddressFamily::IPv4;
    std::array<uint8_t, 16> Bytes{};

    static constexpr DnsAddress IPv4(
        uint8_t a,
        uint8_t b,
        uint8_t c,
        uint8_t d
    ) noexcept {
        DnsAddress result;
        result.Family = DnsAddressFamily::IPv4;
        result.Bytes[0] = a;
        result.Bytes[1] = b;
        result.Bytes[2] = c;
        result.Bytes[3] = d;
        return result;
    }

    static constexpr DnsAddress IPv6(const std::array<uint8_t, 16>& bytes) noexcept {
        DnsAddress result;
        result.Family = DnsAddressFamily::IPv6;
        result.Bytes = bytes;
        return result;
    }
};

struct DnsServerConfiguration final {
    uint16_t Port = 53;
    std::size_t MaximumPendingRequests = 8;
};

enum class DnsResponseState : uint8_t {
    Uncommitted = 0,
    Building,
    Completed,
    Aborted
};

class IDnsRequestPlatform {
public:
    virtual ~IDnsRequestPlatform() = default;
    virtual std::string_view Name() const noexcept = 0;
    virtual DnsRecordType Type() const noexcept = 0;
    virtual DnsRecordClass Class() const noexcept = 0;
};

class IDnsResponsePlatform {
public:
    virtual ~IDnsResponsePlatform() = default;
    virtual WebResult SetResponseCode(DnsResponseCode code) = 0;
    virtual WebResult AddAddressAnswer(const DnsAddress& address, uint32_t ttlSeconds) = 0;
    virtual WebResult Complete() = 0;
    virtual void Abort() noexcept = 0;
};

class DnsRequest final {
public:
    explicit DnsRequest(IDnsRequestPlatform& platform) noexcept : _platform(platform) {}

    std::string_view Name() const noexcept { return _platform.Name(); }
    DnsRecordType Type() const noexcept { return _platform.Type(); }
    DnsRecordClass Class() const noexcept { return _platform.Class(); }

private:
    IDnsRequestPlatform& _platform;
};

class DnsResponse final {
public:
    explicit DnsResponse(IDnsResponsePlatform& platform) noexcept : _platform(platform) {}

    ~DnsResponse() {
        if (_state == DnsResponseState::Building) Abort();
    }

    DnsResponseState State() const noexcept { return _state; }
    bool IsCommitted() const noexcept { return _state != DnsResponseState::Uncommitted; }
    bool IsCompleted() const noexcept { return _state == DnsResponseState::Completed; }

    WebResult Code(DnsResponseCode code) {
        if (_state == DnsResponseState::Completed || _state == DnsResponseState::Aborted) {
            return WebResult::Failure(WebError::InvalidState);
        }
        const auto result = _platform.SetResponseCode(code);
        if (result) _state = DnsResponseState::Building;
        return result;
    }

    WebResult Address(const DnsAddress& address, uint32_t ttlSeconds) {
        if (_state == DnsResponseState::Uncommitted) {
            const auto result = Code(DnsResponseCode::NoError);
            if (!result) return result;
        }
        if (_state != DnsResponseState::Building) {
            return WebResult::Failure(WebError::InvalidState);
        }
        return _platform.AddAddressAnswer(address, ttlSeconds);
    }

    WebResult Complete() {
        if (_state == DnsResponseState::Completed) return WebResult::Success();
        if (_state == DnsResponseState::Aborted) return WebResult::Failure(WebError::InvalidState);
        if (_state == DnsResponseState::Uncommitted) {
            const auto result = Code(DnsResponseCode::NoError);
            if (!result) return result;
        }
        const auto result = _platform.Complete();
        if (result) _state = DnsResponseState::Completed;
        return result;
    }

    void Abort() noexcept {
        if (_state == DnsResponseState::Completed || _state == DnsResponseState::Aborted) return;
        _platform.Abort();
        _state = DnsResponseState::Aborted;
    }

private:
    IDnsResponsePlatform& _platform;
    DnsResponseState _state = DnsResponseState::Uncommitted;
};

class DnsRequestContext final {
public:
    DnsRequestContext(
        IDnsRequestPlatform& requestPlatform,
        IDnsResponsePlatform& responsePlatform
    ) noexcept : _request(requestPlatform), _response(responsePlatform) {}

    DnsRequest& Request() noexcept { return _request; }
    const DnsRequest& Request() const noexcept { return _request; }
    DnsResponse& Response() noexcept { return _response; }
    const DnsResponse& Response() const noexcept { return _response; }

private:
    DnsRequest _request;
    DnsResponse _response;
};

enum class DnsHandlerDisposition : uint8_t {
    NotHandled = 0,
    Handled
};

struct DnsHandlerResult final {
    WebResult Result;
    DnsHandlerDisposition Disposition = DnsHandlerDisposition::NotHandled;

    constexpr explicit operator bool() const noexcept { return static_cast<bool>(Result); }
    static constexpr DnsHandlerResult NotHandled() noexcept {
        return {WebResult::Success(), DnsHandlerDisposition::NotHandled};
    }
    static constexpr DnsHandlerResult Handled(WebResult result = WebResult::Success()) noexcept {
        return {result, DnsHandlerDisposition::Handled};
    }
};

class IDnsRequestHandler {
public:
    virtual ~IDnsRequestHandler() = default;
    virtual DnsHandlerResult Handle(DnsRequestContext& context) = 0;
};

class IDnsRequestDispatcher {
public:
    virtual ~IDnsRequestDispatcher() = default;
    virtual WebResult Dispatch(
        IDnsRequestPlatform& request,
        IDnsResponsePlatform& response
    ) = 0;
};

class IDnsServerPlatform {
public:
    virtual ~IDnsServerPlatform() = default;
    virtual WebCapabilities Capabilities() const noexcept = 0;
    virtual WebResult Initialize(
        const DnsServerConfiguration& configuration,
        IDnsRequestDispatcher& dispatcher
    ) = 0;
    virtual WebResult Start() = 0;
    virtual WebResult Stop() = 0;
    virtual void Reset() noexcept = 0;
};

enum class DnsServerState : uint8_t {
    Stopped = 0,
    Initializing,
    Ready,
    Starting,
    Running,
    Stopping,
    Faulted
};

class IDnsServerObserver : public Observable::IObserver {
public:
    ~IDnsServerObserver() override = default;
    virtual void OnDnsServerStateChanged(DnsServerState, DnsServerState) {}
};

class DnsServer final : public IDnsRequestDispatcher {
private:
    static constexpr auto ExternalPreferred =
        System::Memory::MemoryPolicy::ExternalPreferred;

    class LifecycleObservable final : public Observable::ThreadSafeObservable {
    public:
        void StateChanged(DnsServerState oldState, DnsServerState newState) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IDnsServerObserver>([&](IDnsServerObserver* observer) {
                    observer->OnDnsServerStateChanged(oldState, newState);
                });
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

    void NotifyStateChanged(DnsServerState oldState, DnsServerState newState) {
        auto observable = ObservableSnapshot();
        if (observable) observable->StateChanged(oldState, newState);
    }

public:
    /// <summary>Creates an allocation-free DNS server wrapper around the supplied platform implementation.</summary>
    explicit DnsServer(IDnsServerPlatform& platform)
        : _platform(platform) {}

    ~DnsServer() {
        (void)Stop();
        _platform.Reset();
    }

    DnsServer(const DnsServer&) = delete;
    DnsServer& operator=(const DnsServer&) = delete;

    DnsServerState State() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _state;
    }

    /// <summary>Registers a DNS lifecycle observer, materializing externally preferred observer bookkeeping on first use.</summary>
    Observable::ObserverHandlePtr RegisterObserver(IDnsServerObserver* observer) {
        if (observer == nullptr) return {};
        auto observable = EnsureObservable();
        return observable
            ? observable->template RegisterObserverAs<IDnsServerObserver>(observer)
            : Observable::ObserverHandlePtr{};
    }

    WebResult SetRequestHandler(IDnsRequestHandler* handler) noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state == DnsServerState::Running ||
            _state == DnsServerState::Starting ||
            _state == DnsServerState::Stopping) {
            return WebResult::Failure(WebError::InvalidState);
        }
        _handler = handler;
        return WebResult::Success();
    }

    /// <summary>Initializes DNS service state without allocating observer infrastructure when no lifecycle observers are registered.</summary>
    WebResult Initialize(const DnsServerConfiguration& configuration = {}) {
        if (configuration.Port == 0 || configuration.MaximumPendingRequests == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        if (!HasCapability(_platform.Capabilities(), WebCapability::Dns)) {
            return WebResult::Failure(WebError::Unsupported);
        }

        DnsServerState oldState;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == DnsServerState::Running ||
                _state == DnsServerState::Starting ||
                _state == DnsServerState::Stopping ||
                _state == DnsServerState::Initializing) {
                return WebResult::Failure(WebError::InvalidState);
            }
            oldState = _state;
            _state = DnsServerState::Initializing;
        }
        NotifyStateChanged(oldState, DnsServerState::Initializing);

        _platform.Reset();
        const auto result = _platform.Initialize(configuration, *this);
        const auto finalState = result ? DnsServerState::Ready : DnsServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        NotifyStateChanged(DnsServerState::Initializing, finalState);
        return result;
    }

    WebResult Start() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == DnsServerState::Running) {
                return WebResult::Failure(WebError::AlreadyRunning);
            }
            if (_state != DnsServerState::Ready) {
                return WebResult::Failure(WebError::InvalidState);
            }
            _state = DnsServerState::Starting;
        }
        NotifyStateChanged(DnsServerState::Ready, DnsServerState::Starting);
        const auto result = _platform.Start();
        const auto finalState = result ? DnsServerState::Running : DnsServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        NotifyStateChanged(DnsServerState::Starting, finalState);
        return result;
    }

    WebResult Stop() {
        DnsServerState previous;
        bool resetOnly = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            previous = _state;
            if (_state == DnsServerState::Stopped) return WebResult::Success();
            if (_state == DnsServerState::Ready || _state == DnsServerState::Faulted) {
                _state = DnsServerState::Stopped;
                resetOnly = true;
            } else if (_state == DnsServerState::Running) {
                _state = DnsServerState::Stopping;
            } else {
                return WebResult::Failure(WebError::InvalidState);
            }
        }

        if (resetOnly) {
            _platform.Reset();
            NotifyStateChanged(previous, DnsServerState::Stopped);
            return WebResult::Success();
        }

        NotifyStateChanged(previous, DnsServerState::Stopping);
        const auto result = _platform.Stop();
        const auto finalState = result ? DnsServerState::Stopped : DnsServerState::Faulted;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _state = finalState;
        }
        if (result) _platform.Reset();
        NotifyStateChanged(DnsServerState::Stopping, finalState);
        return result;
    }

    WebResult Dispatch(
        IDnsRequestPlatform& requestPlatform,
        IDnsResponsePlatform& responsePlatform
    ) override {
        IDnsRequestHandler* handler;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state != DnsServerState::Running) {
                return WebResult::Failure(WebError::NotRunning);
            }
            handler = _handler;
        }
        if (handler == nullptr) return WebResult::Failure(WebError::InvalidState);

        DnsRequestContext context(requestPlatform, responsePlatform);
        const auto handling = handler->Handle(context);
        if (!handling.Result) return handling.Result;
        if (handling.Disposition == DnsHandlerDisposition::NotHandled) {
            if (context.Response().IsCommitted()) return WebResult::Failure(WebError::InvalidState);
            auto result = context.Response().Code(DnsResponseCode::NameError);
            if (!result) return result;
            return context.Response().Complete();
        }
        return context.Response().IsCompleted()
            ? WebResult::Success()
            : WebResult::Failure(WebError::InvalidState);
    }

private:
    IDnsServerPlatform& _platform;
    mutable std::mutex _observableMutex;
    std::shared_ptr<LifecycleObservable> _observable;
    mutable std::mutex _mutex;
    DnsServerState _state = DnsServerState::Stopped;
    IDnsRequestHandler* _handler = nullptr;
};

class WildcardDnsHandler final : public IDnsRequestHandler {
public:
    explicit WildcardDnsHandler(
        DnsAddress address,
        uint32_t ttlSeconds = 60
    ) : _address(address), _ttlSeconds(ttlSeconds) {}

    DnsHandlerResult Handle(DnsRequestContext& context) override {
        const auto type = context.Request().Type();
        const bool compatible =
            (_address.Family == DnsAddressFamily::IPv4 && type == DnsRecordType::A) ||
            (_address.Family == DnsAddressFamily::IPv6 && type == DnsRecordType::Aaaa);
        if (!compatible || context.Request().Class() != DnsRecordClass::Internet) {
            return DnsHandlerResult::NotHandled();
        }

        auto result = context.Response().Address(_address, _ttlSeconds);
        if (!result) return DnsHandlerResult::Handled(result);
        return DnsHandlerResult::Handled(context.Response().Complete());
    }

private:
    const DnsAddress _address;
    const uint32_t _ttlSeconds;
};

} // namespace ESPressio::Web