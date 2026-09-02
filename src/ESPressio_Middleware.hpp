#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include <ESPressio_Memory.hpp>

#include "ESPressio_HttpServer.hpp"

namespace ESPressio::Web {

struct MiddlewareHandle final {
    uint64_t Id = 0;

    constexpr explicit operator bool() const noexcept { return Id != 0; }
    friend constexpr bool operator==(MiddlewareHandle left, MiddlewareHandle right) noexcept {
        return left.Id == right.Id;
    }
};

class IHttpMiddlewareNext {
public:
    virtual ~IHttpMiddlewareNext() = default;
    virtual HttpHandlerResult Invoke(WebRequestContext& context) = 0;
};

class IHttpMiddleware {
public:
    virtual ~IHttpMiddleware() = default;
    virtual HttpHandlerResult Handle(
        WebRequestContext& context,
        IHttpMiddlewareNext& next
    ) = 0;
};

class MiddlewarePipeline final : public IHttpRequestHandler {
private:
    struct Entry final {
        MiddlewareHandle Handle;
        IHttpMiddleware* Middleware = nullptr;
    };

    using EntryList = System::Memory::Vector<
        Entry,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    struct Chain final {
        EntryList Entries;
        IHttpRequestHandler* Terminal = nullptr;

        Chain() = default;
        Chain(const Chain& other) : Entries(other.Entries), Terminal(other.Terminal) {}
    };

    class Invocation final {
    private:
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
