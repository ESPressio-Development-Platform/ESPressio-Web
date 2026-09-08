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

/**
 * ESPressio Memory Audit
 * Underlying storage: 2 bytes
 * Total Memory: 2 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Underlying storage: 2 bytes
 * Total Memory: 2 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsRecordClass : uint16_t {
    Internet = 1,
    Any = 255
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsResponseCode : uint8_t {
    NoError = 0,
    FormatError = 1,
    ServerFailure = 2,
    NameError = 3,
    NotImplemented = 4,
    Refused = 5
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsAddressFamily : uint8_t {
    IPv4 = 4,
    IPv6 = 6
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Family (DnsAddressFamily): 1 bytes [0 bytes dynamic allocation]
 * - Bytes (std::array<uint8_t, 16>): 16 bytes [0 bytes dynamic allocation]
 * Total Memory: 17 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members:
 * - Port (uint16_t): 2 bytes [0 bytes dynamic allocation]
 * - MaximumPendingRequests (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct DnsServerConfiguration final {
    uint16_t Port = 53;
    std::size_t MaximumPendingRequests = 8;
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsResponseState : uint8_t {
    Uncommitted = 0,
    Building,
    Completed,
    Aborted
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IDnsRequestPlatform {
public:
    virtual ~IDnsRequestPlatform() = default;
    virtual std::string_view Name() const noexcept = 0;
    virtual DnsRecordType Type() const noexcept = 0;
    virtual DnsRecordClass Class() const noexcept = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IDnsResponsePlatform {
public:
    virtual ~IDnsResponsePlatform() = default;
    virtual WebResult SetResponseCode(DnsResponseCode code) = 0;
    virtual WebResult AddAddressAnswer(const DnsAddress& address, uint32_t ttlSeconds) = 0;
    virtual WebResult Complete() = 0;
    virtual void Abort() noexcept = 0;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - _platform (IDnsRequestPlatform&): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class DnsRequest final {
public:
    explicit DnsRequest(IDnsRequestPlatform& platform) noexcept : _platform(platform) {}

    std::string_view Name() const noexcept { return _platform.Name(); }
    DnsRecordType Type() const noexcept { return _platform.Type(); }
    DnsRecordClass Class() const noexcept { return _platform.Class(); }

private:
    IDnsRequestPlatform& _platform;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - _platform (IDnsResponsePlatform&): 4 bytes [0 bytes dynamic allocation]
 * - _state (DnsResponseState): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members:
 * - _request (DnsRequest): 4 bytes [0 bytes dynamic allocation]
 * - _response (DnsResponse): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsHandlerDisposition : uint8_t {
    NotHandled = 0,
    Handled
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Result (WebResult): 8 bytes [0 bytes dynamic allocation]
 * - Disposition (DnsHandlerDisposition): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IDnsRequestHandler {
public:
    virtual ~IDnsRequestHandler() = default;
    virtual DnsHandlerResult Handle(DnsRequestContext& context) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IDnsRequestDispatcher {
public:
    virtual ~IDnsRequestDispatcher() = default;
    virtual WebResult Dispatch(
        IDnsRequestPlatform& request,
        IDnsResponsePlatform& response
    ) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class DnsServerState : uint8_t {
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
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IDnsServerObserver : public Observable::IObserver {
public:
    ~IDnsServerObserver() override = default;
    virtual void OnDnsServerStateChanged(DnsServerState, DnsServerState) {}
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _platform (IDnsServerPlatform&): 4 bytes [0 bytes dynamic allocation]
 * - _observableMutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _observable (std::shared_ptr<LifecycleObservable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily]
 * - _mutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _state (DnsServerState): 1 bytes [0 bytes dynamic allocation]
 * - _handler (IDnsRequestHandler*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 32 bytes [_observableMutex: native synchronization state may allocate platform resources lazily; _observable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _observable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _observable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _observable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _observable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _mutex: native synchronization state may allocate platform resources lazily]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class DnsServer final : public IDnsRequestDispatcher {
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

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _address (DnsAddress): 17 bytes [0 bytes dynamic allocation]
 * - _ttlSeconds (uint32_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 28 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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