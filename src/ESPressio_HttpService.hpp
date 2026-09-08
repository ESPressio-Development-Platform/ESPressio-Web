#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include <ESPressio_Memory.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Members:
 * - Id (uint64_t): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct HttpProviderHandle final {
    uint64_t Id = 0;

    constexpr explicit operator bool() const noexcept { return Id != 0; }
    friend constexpr bool operator==(HttpProviderHandle left, HttpProviderHandle right) noexcept {
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
class IHttpServiceProvider {
public:
    virtual ~IHttpServiceProvider() = default;

    // Providers inspect the live borrowed request and may lazily read any
    // metadata they need. They do not consume the body during selection.
    virtual WebResult CanHandle(const HttpRequest& request, bool& canHandle) const = 0;
    virtual HttpHandlerResult Handle(WebRequestContext& context) = 0;
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 8 bytes [0 bytes dynamic allocation]
 * Members:
 * - _mutex (std::mutex): 4 bytes [native synchronization state may allocate platform resources lazily]
 * - _providers (std::shared_ptr<ProviderSet>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 12 bytes; pointee: Entries: Capacity * (12 bytes) element storage]
 * - _nextHandle (std::atomic<uint64_t>): 8 bytes [0 bytes dynamic allocation]
 * Total Memory: 28 bytes [_mutex: native synchronization state may allocate platform resources lazily; _providers: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 12 bytes; _providers: pointee: Entries: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class HttpService final :
    public IHttpRequestHandler,
    public IHttpRouteHandler {
private:
/**
 * ESPressio Memory Audit
 * Members:
 * - Handle (HttpProviderHandle): 8 bytes [0 bytes dynamic allocation]
 * - Provider (IHttpServiceProvider*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct Entry final {
        HttpProviderHandle Handle;
        IHttpServiceProvider* Provider = nullptr;
    };

    using EntryList = System::Memory::Vector<
        Entry,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

/**
 * ESPressio Memory Audit
 * Members:
 * - Entries (EntryList): 12 bytes [Capacity * (12 bytes) element storage]
 * Total Memory: 12 bytes [Entries: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
struct ProviderSet final {
        EntryList Entries;
        ProviderSet() = default;
        ProviderSet(const ProviderSet& other) : Entries(other.Entries) {}
    };

public:
    HttpService()
        : _providers(System::Memory::MakeShared<
              ProviderSet,
              System::Memory::MemoryPolicy::ExternalPreferred
          >()) {}

    HttpService(const HttpService&) = delete;
    HttpService& operator=(const HttpService&) = delete;

    HttpProviderHandle AddProvider(IHttpServiceProvider& provider) {
        const HttpProviderHandle handle{
            _nextHandle.fetch_add(1, std::memory_order_relaxed)
        };
        std::lock_guard<std::mutex> lock(_mutex);
        auto next = CloneLocked();
        next->Entries.push_back({handle, &provider});
        _providers = std::move(next);
        return handle;
    }

    bool RemoveProvider(HttpProviderHandle handle) {
        if (!handle) return false;
        std::lock_guard<std::mutex> lock(_mutex);
        auto next = CloneLocked();
        for (auto it = next->Entries.begin(); it != next->Entries.end(); ++it) {
            if (it->Handle == handle) {
                next->Entries.erase(it);
                _providers = std::move(next);
                return true;
            }
        }
        return false;
    }

    void ClearProviders() {
        std::lock_guard<std::mutex> lock(_mutex);
        _providers = System::Memory::MakeShared<
            ProviderSet,
            System::Memory::MemoryPolicy::ExternalPreferred
        >();
    }

    std::size_t ProviderCount() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _providers->Entries.size();
    }

    HttpHandlerResult Handle(WebRequestContext& context) override {
        std::shared_ptr<const ProviderSet> providers;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            providers = _providers;
        }

        for (const auto& entry : providers->Entries) {
            if (entry.Provider == nullptr) {
                return HttpHandlerResult::Failure(WebError::InvalidState);
            }

            bool canHandle = false;
            const auto selection = entry.Provider->CanHandle(context.Request(), canHandle);
            if (!selection) return HttpHandlerResult::Handled(selection);
            if (!canHandle) continue;

            const auto handling = entry.Provider->Handle(context);
            if (!handling.Result ||
                handling.Disposition == HttpHandlerDisposition::Handled) {
                return handling;
            }
        }

        return HttpHandlerResult::NotHandled();
    }

    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters&
    ) override {
        return Handle(context);
    }

private:
    std::shared_ptr<ProviderSet> CloneLocked() const {
        return System::Memory::MakeShared<
            ProviderSet,
            System::Memory::MemoryPolicy::ExternalPreferred
        >(*_providers);
    }

    mutable std::mutex _mutex;
    std::shared_ptr<ProviderSet> _providers;
    std::atomic<uint64_t> _nextHandle{1};
};

} // namespace ESPressio::Web
