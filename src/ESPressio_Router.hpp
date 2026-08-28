#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string_view>

#include <ESPressio_Memory.hpp>

#include "ESPressio_HttpServer.hpp"

#ifndef ESPRESSIO_WEB_MAX_ROUTE_PARAMETERS
#define ESPRESSIO_WEB_MAX_ROUTE_PARAMETERS 8
#endif

namespace ESPressio::Web {

struct RouteHandle final {
    uint64_t Id = 0;

    constexpr explicit operator bool() const noexcept { return Id != 0; }
    friend constexpr bool operator==(RouteHandle left, RouteHandle right) noexcept {
        return left.Id == right.Id;
    }
};

struct RouteParameterView final {
    std::string_view Name;
    std::string_view Value;
};

class RouteParameters final {
public:
    std::size_t Count() const noexcept { return _count; }

    const RouteParameterView* Get(std::size_t index) const noexcept {
        return index < _count ? &_values[index] : nullptr;
    }

    std::string_view Find(std::string_view name) const noexcept {
        for (std::size_t index = 0; index < _count; ++index) {
            if (_values[index].Name == name) return _values[index].Value;
        }
        return {};
    }

private:
    friend class Router;

    bool Add(std::string_view name, std::string_view value) noexcept {
        if (_count >= _values.size()) return false;
        _values[_count++] = {name, value};
        return true;
    }

    std::array<RouteParameterView, ESPRESSIO_WEB_MAX_ROUTE_PARAMETERS> _values{};
    std::size_t _count = 0;
};

class IHttpRouteHandler {
public:
    virtual ~IHttpRouteHandler() = default;
    virtual WebResult Handle(
        WebRequestContext& context,
        const RouteParameters& parameters
    ) = 0;
};

class Router final : public IHttpRequestHandler {
private:
    struct RouteEntry final {
        RouteHandle Handle;
        HttpMethod Method = HttpMethod::Any;
        System::Memory::String<System::Memory::MemoryPolicy::ExternalPreferred> Pattern;
        IHttpRouteHandler* Handler = nullptr;
        std::size_t NamedParameterCount = 0;
        std::size_t LiteralCharacterCount = 0;

        RouteEntry(
            RouteHandle handle,
            HttpMethod method,
            std::string_view pattern,
            IHttpRouteHandler& handler,
            std::size_t namedParameterCount,
            std::size_t literalCharacterCount
        ) : Handle(handle),
            Method(method),
            Pattern(pattern.begin(), pattern.end()),
            Handler(&handler),
            NamedParameterCount(namedParameterCount),
            LiteralCharacterCount(literalCharacterCount) {}
    };

    struct PatternAnalysis final {
        bool Valid = false;
        std::size_t NamedParameterCount = 0;
        std::size_t LiteralCharacterCount = 0;
    };

public:
    Router() = default;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    WebResult RegisterRoute(
        HttpMethod method,
        std::string_view pattern,
        IHttpRouteHandler& handler,
        RouteHandle& handle
    ) {
        handle = {};
        const auto analysis = AnalyzePattern(pattern);
        if (!analysis.Valid) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        const RouteHandle newHandle{_nextHandle.fetch_add(1, std::memory_order_relaxed)};
        auto entry = System::Memory::MakeShared<
            RouteEntry,
            System::Memory::MemoryPolicy::ExternalPreferred
        >(
            newHandle,
            method,
            pattern,
            handler,
            analysis.NamedParameterCount,
            analysis.LiteralCharacterCount
        );

        {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            _routes.push_back(std::move(entry));
        }
        handle = newHandle;
        return WebResult::Success();
    }

    bool RemoveRoute(RouteHandle handle) {
        if (!handle) return false;
        std::unique_lock<std::shared_mutex> lock(_mutex);
        for (auto it = _routes.begin(); it != _routes.end(); ++it) {
            if ((*it)->Handle == handle) {
                _routes.erase(it);
                return true;
            }
        }
        return false;
    }

    void Clear() {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _routes.clear();
    }

    std::size_t RouteCount() const noexcept {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _routes.size();
    }

    WebResult Handle(WebRequestContext& context) override {
        const auto method = context.Request().Method();
        const auto path = context.Request().Path();

        std::shared_ptr<RouteEntry> selected;
        RouteParameters selectedParameters;
        std::size_t selectedNamedCount = static_cast<std::size_t>(-1);
        std::size_t selectedLiteralCount = 0;
        bool selectedMethodExact = false;

        {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            for (const auto& route : _routes) {
                const bool methodExact = route->Method == method;
                if (!methodExact && route->Method != HttpMethod::Any) continue;

                RouteParameters candidateParameters;
                if (!Match(route->Pattern, path, candidateParameters)) continue;

                const bool better =
                    selected == nullptr ||
                    route->NamedParameterCount < selectedNamedCount ||
                    (route->NamedParameterCount == selectedNamedCount &&
                     route->LiteralCharacterCount > selectedLiteralCount) ||
                    (route->NamedParameterCount == selectedNamedCount &&
                     route->LiteralCharacterCount == selectedLiteralCount &&
                     methodExact && !selectedMethodExact);

                if (!better) continue;

                selected = route;
                selectedParameters = candidateParameters;
                selectedNamedCount = route->NamedParameterCount;
                selectedLiteralCount = route->LiteralCharacterCount;
                selectedMethodExact = methodExact;
            }
        }

        if (selected == nullptr || selected->Handler == nullptr) {
            return WebResult::Failure(WebError::ProtocolError);
        }

        return selected->Handler->Handle(context, selectedParameters);
    }

private:
    using RouteList = System::Memory::Vector<
        std::shared_ptr<RouteEntry>,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    static PatternAnalysis AnalyzePattern(std::string_view pattern) noexcept {
        PatternAnalysis result;
        if (pattern.empty() || pattern.front() != '/') return result;
        if (pattern.size() > 1 && pattern.back() == '/') return result;

        std::size_t position = 1;
        while (position <= pattern.size()) {
            const std::size_t separator = pattern.find('/', position);
            const std::size_t end = separator == std::string_view::npos
                ? pattern.size()
                : separator;
            const auto segment = pattern.substr(position, end - position);
            if (segment.empty() && position < pattern.size()) return result;

            if (!segment.empty() && segment.front() == ':') {
                if (segment.size() == 1) return result;
                ++result.NamedParameterCount;
                if (result.NamedParameterCount > ESPRESSIO_WEB_MAX_ROUTE_PARAMETERS) {
                    return PatternAnalysis{};
                }
            } else {
                result.LiteralCharacterCount += segment.size();
            }

            if (separator == std::string_view::npos) break;
            position = separator + 1;
        }

        result.Valid = true;
        return result;
    }

    static bool Match(
        std::string_view pattern,
        std::string_view path,
        RouteParameters& parameters
    ) noexcept {
        if (path.empty() || path.front() != '/') return false;
        if (path.size() > 1 && path.back() == '/') return false;

        std::size_t patternPosition = 1;
        std::size_t pathPosition = 1;

        for (;;) {
            const std::size_t patternSeparator = pattern.find('/', patternPosition);
            const std::size_t pathSeparator = path.find('/', pathPosition);
            const std::size_t patternEnd = patternSeparator == std::string_view::npos
                ? pattern.size()
                : patternSeparator;
            const std::size_t pathEnd = pathSeparator == std::string_view::npos
                ? path.size()
                : pathSeparator;

            const auto patternSegment = pattern.substr(
                patternPosition,
                patternEnd - patternPosition
            );
            const auto pathSegment = path.substr(pathPosition, pathEnd - pathPosition);

            if (patternSegment.empty() != pathSegment.empty()) return false;

            if (!patternSegment.empty() && patternSegment.front() == ':') {
                if (pathSegment.empty() ||
                    !parameters.Add(patternSegment.substr(1), pathSegment)) {
                    return false;
                }
            } else if (patternSegment != pathSegment) {
                return false;
            }

            const bool patternDone = patternSeparator == std::string_view::npos;
            const bool pathDone = pathSeparator == std::string_view::npos;
            if (patternDone || pathDone) return patternDone && pathDone;

            patternPosition = patternSeparator + 1;
            pathPosition = pathSeparator + 1;
        }
    }

    mutable std::shared_mutex _mutex;
    RouteList _routes;
    std::atomic<uint64_t> _nextHandle{1};
};

} // namespace ESPressio::Web
