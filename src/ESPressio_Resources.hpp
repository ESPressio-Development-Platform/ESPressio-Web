#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <ESPressio_Memory.hpp>

#include "ESPressio_HttpServer.hpp"

namespace ESPressio::Web {

struct WebResourceMetadata final {
    uint64_t Size = 0;
    bool IsDirectory = false;
};

struct WebResourceReadResult final {
    WebResult Result;
    std::size_t BytesRead = 0;

    explicit operator bool() const noexcept { return static_cast<bool>(Result); }
};

class IWebResourceProvider {
public:
    virtual ~IWebResourceProvider() = default;
    virtual WebResult Stat(std::string_view path, WebResourceMetadata& metadata) const = 0;
    virtual WebResourceReadResult Read(
        std::string_view path,
        uint64_t offset,
        uint8_t* destination,
        std::size_t capacity
    ) const = 0;
};

class IHttpContentTypeResolver {
public:
    virtual ~IHttpContentTypeResolver() = default;
    virtual std::string_view Resolve(std::string_view path) const noexcept = 0;
};

class DefaultHttpContentTypeResolver final : public IHttpContentTypeResolver {
public:
    std::string_view Resolve(std::string_view path) const noexcept override {
        const auto dot = path.find_last_of('.');
        if (dot == std::string_view::npos) return "application/octet-stream";
        const auto extension = path.substr(dot + 1);

        if (extension == "html" || extension == "htm") return "text/html; charset=utf-8";
        if (extension == "css") return "text/css; charset=utf-8";
        if (extension == "js" || extension == "mjs") return "text/javascript; charset=utf-8";
        if (extension == "json") return "application/json";
        if (extension == "xml") return "application/xml";
        if (extension == "txt") return "text/plain; charset=utf-8";
        if (extension == "svg") return "image/svg+xml";
        if (extension == "png") return "image/png";
        if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
        if (extension == "gif") return "image/gif";
        if (extension == "webp") return "image/webp";
        if (extension == "ico") return "image/x-icon";
        if (extension == "wasm") return "application/wasm";
        if (extension == "pdf") return "application/pdf";
        if (extension == "cbor") return "application/cbor";
        if (extension == "bin") return "application/octet-stream";
        return "application/octet-stream";
    }
};

struct StaticResourceConfiguration final {
    std::size_t ReadChunkBytes = 1024;
};

class StaticResourceHandler final : public IHttpRequestHandler {
public:
    explicit StaticResourceHandler(
        IWebResourceProvider& provider,
        const IHttpContentTypeResolver* contentTypeResolver = nullptr
    ) : _provider(provider),
        _contentTypeResolver(
            contentTypeResolver == nullptr
                ? static_cast<const IHttpContentTypeResolver*>(&_defaultContentTypeResolver)
                : contentTypeResolver
        ) {}

    WebResult Configure(const StaticResourceConfiguration& configuration) {
        if (configuration.ReadChunkBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        _configuration = configuration;
        return WebResult::Success();
    }

    HttpHandlerResult Handle(WebRequestContext& context) override {
        const auto method = context.Request().Method();
        if (method != HttpMethod::Get && method != HttpMethod::Head) {
            return HttpHandlerResult::NotHandled();
        }

        const auto path = context.Request().Path();
        if (!IsSafeResourcePath(path)) {
            return HttpHandlerResult::Failure(WebError::ProtocolError);
        }

        WebResourceMetadata metadata;
        const auto stat = _provider.Stat(path, metadata);
        if (!stat) {
            if (stat.Error == WebError::NotFound) return HttpHandlerResult::NotHandled();
            return HttpHandlerResult::Handled(stat);
        }
        if (metadata.IsDirectory) return HttpHandlerResult::NotHandled();
        if (metadata.Size > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max())) {
            return HttpHandlerResult::Failure(WebError::ResourceExhausted);
        }

        auto& response = context.Response();
        auto result = response.ContentType(_contentTypeResolver->Resolve(path));
        if (!result) return HttpHandlerResult::Handled(result);
        result = response.Begin(static_cast<std::size_t>(metadata.Size));
        if (!result) return HttpHandlerResult::Handled(result);

        if (method == HttpMethod::Head || metadata.Size == 0) {
            return HttpHandlerResult::Handled(response.Complete());
        }

        System::Memory::Vector<
            uint8_t,
            System::Memory::MemoryPolicy::ExternalPreferred
        > buffer(_configuration.ReadChunkBytes);

        uint64_t offset = 0;
        while (offset < metadata.Size) {
            const uint64_t remaining = metadata.Size - offset;
            const std::size_t requested = remaining < buffer.size()
                ? static_cast<std::size_t>(remaining)
                : buffer.size();

            const auto read = _provider.Read(
                path,
                offset,
                buffer.data(),
                requested
            );
            if (!read.Result) {
                response.Abort();
                return HttpHandlerResult::Handled(read.Result);
            }
            if (read.BytesRead == 0 || read.BytesRead > requested) {
                response.Abort();
                return HttpHandlerResult::Failure(WebError::ProtocolError);
            }

            result = response.Write(buffer.data(), read.BytesRead);
            if (!result) {
                response.Abort();
                return HttpHandlerResult::Handled(result);
            }
            offset += read.BytesRead;
        }

        return HttpHandlerResult::Handled(response.Complete());
    }

private:
    static bool IsSafeResourcePath(std::string_view path) noexcept {
        if (path.empty() || path.front() != '/') return false;
        if (path.find('\\') != std::string_view::npos ||
            path.find('\0') != std::string_view::npos) {
            return false;
        }

        std::size_t position = 1;
        while (position <= path.size()) {
            const auto separator = path.find('/', position);
            const auto end = separator == std::string_view::npos ? path.size() : separator;
            const auto segment = path.substr(position, end - position);
            if (segment == "..") return false;
            if (separator == std::string_view::npos) break;
            position = separator + 1;
        }
        return true;
    }

    IWebResourceProvider& _provider;
    DefaultHttpContentTypeResolver _defaultContentTypeResolver;
    const IHttpContentTypeResolver* _contentTypeResolver;
    StaticResourceConfiguration _configuration{};
};

} // namespace ESPressio::Web
