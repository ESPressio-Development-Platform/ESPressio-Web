#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include <ESPressio_Memory.hpp>

#include "ESPressio_HttpServer.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Members:
 * - Id (uint64_t): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct MiddlewareHandle final {
    uint64_t Id = 0;

    constexpr explicit operator bool() const noexcept { return Id != 0; }
    friend constexpr bool operator==(MiddlewareHandle left, MiddlewareHandle right) noexcept {
        return left.Id == right.Id;
    }
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpMiddlewareNext {
public:
    virtual ~IHttpMiddlewareNext() = default;
    virtual HttpHandlerResult Invoke(WebRequestContext& context) = 0;
};

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IHttpMiddleware {
public:
    virtual ~IHttpMiddleware() = default;
    virtual HttpHandlerResult Handle(
        WebRequestContext& context,
        IHttpMiddlewareNext& next
    ) = 0;
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _mutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _chain (std::shared_ptr<Chain>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 16 bytes; pointee: Entries: Capacity * (12 bytes) element storage]
 * - _nextHandle (std::atomic<uint64_t>): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 24 bytes [_mutex: native synchronization state may allocate platform resources lazily; _chain: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 16 bytes; _chain: pointee: Entries: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class MiddlewarePipeline final : public IHttpRequestHandler {
private:
/**
 * ESPressio Memory Audit
 * Members:
 * - Handle (MiddlewareHandle): 8 bytes [0 bytes dynamic allocation]
 * - Middleware (IHttpMiddleware*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct Entry final {
        MiddlewareHandle Handle;
        IHttpMiddleware* Middleware = nullptr;
    };

    using EntryList = System::Memory::Vector<
        Entry,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

/**
 * ESPressio Memory Audit
 * Members:
 * - Entries (EntryList): 12 bytes [Capacity * (12 bytes) element storage]
 * - Terminal (IHttpRequestHandler*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [Entries: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
struct Chain final {
        EntryList Entries;
        IHttpRequestHandler* Terminal = nullptr;

        Chain() = default;
        Chain(const Chain& other) : Entries(other.Entries), Terminal(other.Terminal) {}
    };

/**
 * ESPressio Memory Audit
 * Members:
 * - _chain (std::shared_ptr<Chain>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 16 bytes; pointee: Entries: Capacity * (12 bytes) element storage]
 * Total Memory: 12 bytes [_chain: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 16 bytes; _chain: pointee: Entries: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Invocation final {
    private:
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _invocation (Invocation&): 4 bytes [0 bytes dynamic allocation]
 * - _nextIndex (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - _invoked (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class Next final : public IHttpMiddlewareNext {
        public:
            Next(Invocation& invocation, std::size_t nextIndex)
                : _invocation(invocation), _nextIndex(nextIndex) {}

            HttpHandlerResult Invoke(WebRequestContext& context) override {
                if (_invoked) return HttpHandlerResult::Failure(WebError::InvalidState);
                _invoked = true;
                return _invocation.InvokeAt(context, _nextIndex);
            }

        private:
            Invocation& _invocation;
            std::size_t _nextIndex;
            bool _invoked = false;
        };

    public:
        explicit Invocation(std::shared_ptr<const Chain> chain)
            : _chain(std::move(chain)) {}

        HttpHandlerResult Invoke(WebRequestContext& context) {
            return InvokeAt(context, 0);
        }

    private:
        HttpHandlerResult InvokeAt(WebRequestContext& context, std::size_t index) {
            if (index < _chain->Entries.size()) {
                auto* middleware = _chain->Entries[index].Middleware;
                if (middleware == nullptr) {
                    return HttpHandlerResult::Failure(WebError::InvalidState);
                }
                Next next(*this, index + 1);
                return middleware->Handle(context, next);
            }

            if (_chain->Terminal == nullptr) {
                return HttpHandlerResult::NotHandled();
            }
            return _chain->Terminal->Handle(context);
        }

        std::shared_ptr<const Chain> _chain;
    };

public:
    MiddlewarePipeline()
        : _chain(System::Memory::MakeShared<
              Chain,
              System::Memory::MemoryPolicy::ExternalPreferred
          >()) {}

    MiddlewarePipeline(const MiddlewarePipeline&) = delete;
    MiddlewarePipeline& operator=(const MiddlewarePipeline&) = delete;

    MiddlewareHandle Add(IHttpMiddleware& middleware) {
        const MiddlewareHandle handle{
            _nextHandle.fetch_add(1, std::memory_order_relaxed)
        };

        std::lock_guard<std::mutex> lock(_mutex);
        auto next = CloneLocked();
        next->Entries.push_back({handle, &middleware});
        _chain = std::move(next);
        return handle;
    }

    bool Remove(MiddlewareHandle handle) {
        if (!handle) return false;

        std::lock_guard<std::mutex> lock(_mutex);
        auto next = CloneLocked();
        for (auto it = next->Entries.begin(); it != next->Entries.end(); ++it) {
            if (it->Handle == handle) {
                next->Entries.erase(it);
                _chain = std::move(next);
                return true;
            }
        }
        return false;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        auto next = System::Memory::MakeShared<
            Chain,
            System::Memory::MemoryPolicy::ExternalPreferred
        >();
        next->Terminal = _chain->Terminal;
        _chain = std::move(next);
    }

    void SetTerminal(IHttpRequestHandler* terminal) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto next = CloneLocked();
        next->Terminal = terminal;
        _chain = std::move(next);
    }

    IHttpRequestHandler* Terminal() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _chain->Terminal;
    }

    std::size_t Count() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _chain->Entries.size();
    }

    HttpHandlerResult Handle(WebRequestContext& context) override {
        std::shared_ptr<const Chain> chain;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            chain = _chain;
        }
        Invocation invocation(std::move(chain));
        return invocation.Invoke(context);
    }

private:
    std::shared_ptr<Chain> CloneLocked() const {
        return System::Memory::MakeShared<
            Chain,
            System::Memory::MemoryPolicy::ExternalPreferred
        >(*_chain);
    }

    mutable std::mutex _mutex;
    std::shared_ptr<Chain> _chain;
    std::atomic<uint64_t> _nextHandle{1};
};

} // namespace ESPressio::Web
