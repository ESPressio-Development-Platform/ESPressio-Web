#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <string_view>

#include <ESPressio_Memory.hpp>

#include "ESPressio_Resources.hpp"

namespace ESPressio::Web {

class IHttpErrorResponder {
public:
    virtual ~IHttpErrorResponder() = default;
    virtual HttpHandlerResult Respond(WebRequestContext& context, WebError error) = 0;
};

inline HttpStatus DefaultHttpStatusForWebError(WebError error) noexcept {
    switch (error) {
        case WebError::NotFound: return HttpStatus::NotFound;
        case WebError::RequestTooLarge: return HttpStatus::PayloadTooLarge;
        case WebError::Unsupported: return HttpStatus::NotImplemented;
        case WebError::ProtocolError:
        case WebError::InvalidConfiguration: return HttpStatus::BadRequest;
        case WebError::ResourceExhausted:
        case WebError::NotRunning: return HttpStatus::ServiceUnavailable;
        case WebError::None: return HttpStatus::Ok;
        default: return HttpStatus::InternalServerError;
    }
}

inline std::string_view DefaultHttpErrorBody(WebError error) noexcept {
    switch (error) {
        case WebError::NotFound: return "Not Found";
        case WebError::RequestTooLarge: return "Payload Too Large";
        case WebError::Unsupported: return "Not Implemented";
        case WebError::ProtocolError:
        case WebError::InvalidConfiguration: return "Bad Request";
        case WebError::ResourceExhausted:
        case WebError::NotRunning: return "Service Unavailable";
        default: return "Internal Server Error";
    }
}

class DefaultHttpErrorResponder final : public IHttpErrorResponder {
public:
    HttpHandlerResult Respond(WebRequestContext& context, WebError error) override {
        auto& response = context.Response();
        auto result = response.Status(DefaultHttpStatusForWebError(error));
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(
            response.Send(DefaultHttpErrorBody(error), "text/plain; charset=utf-8")
        );
    }
};

class ResourceHttpErrorResponder final : public IHttpErrorResponder {
private:
    struct Mapping final {
        bool Configured = false;
        HttpStatus Status = HttpStatus::InternalServerError;
        System::Memory::String<System::Memory::MemoryPolicy::ExternalPreferred> Path;
    };

public:
    explicit ResourceHttpErrorResponder(
        IWebResourceProvider& provider,
        IHttpErrorResponder* fallback = nullptr,
        const IHttpContentTypeResolver* contentTypeResolver = nullptr
    ) : _provider(provider),
        _fallback(fallback == nullptr
            ? static_cast<IHttpErrorResponder*>(&_defaultFallback)
            : fallback),
        _contentTypeResolver(contentTypeResolver == nullptr
            ? static_cast<const IHttpContentTypeResolver*>(&_defaultContentTypeResolver)
            : contentTypeResolver) {}

    WebResult ConfigureResource(WebError error, std::string_view path, HttpStatus status) {
        const auto index = static_cast<std::size_t>(error);
        if (error == WebError::None || error == WebError::Count ||
            index >= _mappings.size() || !IsSafeWebResourcePath(path)) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        auto& mapping = _mappings[index];
        mapping.Configured = true;
        mapping.Status = status;
        mapping.Path.assign(path.begin(), path.end());
        return WebResult::Success();
    }

    void ClearResource(WebError error) {
        const auto index = static_cast<std::size_t>(error);
        if (index >= _mappings.size()) return;
        auto& mapping = _mappings[index];
        mapping.Configured = false;
        mapping.Path.clear();
    }

    WebResult ConfigureStreaming(const ResourceResponseConfiguration& configuration) {
        if (configuration.ReadChunkBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        _resourceConfiguration = configuration;
        return WebResult::Success();
    }

    HttpHandlerResult Respond(WebRequestContext& context, WebError error) override {
        const auto index = static_cast<std::size_t>(error);
        if (index >= _mappings.size() || !_mappings[index].Configured) {
            return _fallback->Respond(context, error);
        }

        const auto& mapping = _mappings[index];
        auto& response = context.Response();
        auto result = response.Status(mapping.Status);
        if (!result) return HttpHandlerResult::Handled(result);

        result = WriteWebResourceResponse(
            _provider,
            std::string_view(mapping.Path.data(), mapping.Path.size()),
            response,
            context.Request().Method() == HttpMethod::Head,
            *_contentTypeResolver,
            _resourceConfiguration
        );
        if (result.Error == WebError::NotFound && !response.IsCommitted()) {
            return _fallback->Respond(context, error);
        }
        return HttpHandlerResult::Handled(result);
    }

private:
    static constexpr std::size_t ErrorCount = static_cast<std::size_t>(WebError::Count);
    IWebResourceProvider& _provider;
    DefaultHttpErrorResponder _defaultFallback;
    IHttpErrorResponder* _fallback;
    DefaultHttpContentTypeResolver _defaultContentTypeResolver;
    const IHttpContentTypeResolver* _contentTypeResolver;
    ResourceResponseConfiguration _resourceConfiguration{};
    std::array<Mapping, ErrorCount> _mappings{};
};

class HttpApplication final : public IHttpRequestHandler {
public:
    HttpApplication(
        IHttpRequestHandler* primary = nullptr,
        IHttpRequestHandler* fallback = nullptr,
        IHttpErrorResponder* errorResponder = nullptr
    ) : _primary(primary),
        _fallback(fallback),
        _errorResponder(errorResponder == nullptr
            ? static_cast<IHttpErrorResponder*>(&_defaultErrorResponder)
            : errorResponder) {}

    void SetPrimary(IHttpRequestHandler* primary) noexcept {
        _primary.store(primary, std::memory_order_release);
    }
    void SetFallback(IHttpRequestHandler* fallback) noexcept {
        _fallback.store(fallback, std::memory_order_release);
    }
    void SetErrorResponder(IHttpErrorResponder* responder) noexcept {
        _errorResponder.store(
            responder == nullptr
                ? static_cast<IHttpErrorResponder*>(&_defaultErrorResponder)
                : responder,
            std::memory_order_release
        );
    }

    HttpHandlerResult Handle(WebRequestContext& context) override {
        auto result = Invoke(_primary.load(std::memory_order_acquire), context);
        if (!result.Result) return ResolveError(context, result.Result.Error);
        if (result.Disposition == HttpHandlerDisposition::Handled) return result;

        result = Invoke(_fallback.load(std::memory_order_acquire), context);
        if (!result.Result) return ResolveError(context, result.Result.Error);
        if (result.Disposition == HttpHandlerDisposition::Handled) return result;

        return ResolveError(context, WebError::NotFound);
    }

private:
    static HttpHandlerResult Invoke(IHttpRequestHandler* handler, WebRequestContext& context) {
        return handler == nullptr
            ? HttpHandlerResult::NotHandled()
            : handler->Handle(context);
    }

    HttpHandlerResult ResolveError(WebRequestContext& context, WebError error) {
        if (context.Response().IsCommitted()) {
            return HttpHandlerResult::Failure(error);
        }
        return _errorResponder.load(std::memory_order_acquire)->Respond(context, error);
    }

    std::atomic<IHttpRequestHandler*> _primary;
    std::atomic<IHttpRequestHandler*> _fallback;
    DefaultHttpErrorResponder _defaultErrorResponder;
    std::atomic<IHttpErrorResponder*> _errorResponder;
};

} // namespace ESPressio::Web
