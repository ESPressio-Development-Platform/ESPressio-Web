#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <ESPressio_Memory.hpp>
#include <ESPressio_PolymorphicMemory.hpp>

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

/// <summary>Provides sequential reads from one already-open Web resource.</summary>
class IWebResourceReadStream {
public:
    virtual ~IWebResourceReadStream() = default;

    /// <summary>Returns the complete resource size in bytes.</summary>
    virtual uint64_t Size() const noexcept = 0;

    /// <summary>Reads the next contiguous resource bytes and advances the stream.</summary>
    virtual WebResourceReadResult Read(
        uint8_t* destination,
        std::size_t capacity
    ) = 0;
};

/// <summary>Policy-aware owner for an implementation-specific Web resource read stream.</summary>
using WebResourceReadStreamPtr =
    System::Memory::PolymorphicUniquePtr<IWebResourceReadStream>;

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

    /// <summary>Opens a resource once for efficient sequential streaming.</summary>
    /// <remarks>Providers without a sequential capability may return <c>Unsupported</c>; callers can use the offset-based <c>Read</c> path instead.</remarks>
    virtual WebResult OpenRead(
        std::string_view path,
        WebResourceReadStreamPtr& stream
    ) const {
        (void)path;
        stream.reset();
        return WebResult::Failure(WebError::Unsupported);
    }
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
        return "application/octet-stream";
    }
};

struct ResourceResponseConfiguration final {
    std::size_t ReadChunkBytes = 1024;
};

inline bool IsSafeWebResourcePath(std::string_view path) noexcept {
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

inline WebResult WriteWebResourceResponse(
    IWebResourceProvider& provider,
    std::string_view path,
    HttpResponse& response,
    bool headersOnly,
    const IHttpContentTypeResolver& contentTypeResolver,
    const ResourceResponseConfiguration& configuration = {}
) {
    if (!IsSafeWebResourcePath(path) || configuration.ReadChunkBytes == 0) {
        return WebResult::Failure(WebError::InvalidConfiguration);
    }

    WebResourceMetadata metadata;
    WebResourceReadStreamPtr stream;

    if (!headersOnly) {
        const auto openResult = provider.OpenRead(path, stream);
        if (openResult) {
            if (!stream) return WebResult::Failure(WebError::PlatformFailure);
            metadata.Size = stream->Size();
            metadata.IsDirectory = false;
        } else if (openResult.Error != WebError::Unsupported) {
            return openResult;
        }
    }

    if (!stream) {
        auto result = provider.Stat(path, metadata);
        if (!result) return result;
        if (metadata.IsDirectory) return WebResult::Failure(WebError::NotFound);
    }

    if (metadata.Size > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return WebResult::Failure(WebError::ResourceExhausted);
    }

    auto result = response.ContentType(contentTypeResolver.Resolve(path));
    if (!result) return result;
    result = response.Begin(static_cast<std::size_t>(metadata.Size));
    if (!result) return result;

    if (headersOnly || metadata.Size == 0) return response.Complete();

    System::Memory::Vector<
        uint8_t,
        System::Memory::MemoryPolicy::ExternalPreferred
    > buffer(configuration.ReadChunkBytes);

    uint64_t offset = 0;
    while (offset < metadata.Size) {
        const uint64_t remaining = metadata.Size - offset;
        const std::size_t requested = remaining < buffer.size()
            ? static_cast<std::size_t>(remaining)
            : buffer.size();

        const auto read = stream
            ? stream->Read(buffer.data(), requested)
            : provider.Read(path, offset, buffer.data(), requested);
        if (!read.Result) {
            response.Abort();
            return read.Result;
        }
        if (read.BytesRead == 0 || read.BytesRead > requested) {
            response.Abort();
            return WebResult::Failure(WebError::ProtocolError);
        }

        result = response.Write(buffer.data(), read.BytesRead);
        if (!result) {
            response.Abort();
            return result;
        }
        offset += read.BytesRead;
    }

    return response.Complete();
}

using StaticResourceConfiguration = ResourceResponseConfiguration;

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
        if (!IsSafeWebResourcePath(path)) {
            return HttpHandlerResult::Failure(WebError::ProtocolError);
        }

        const auto result = WriteWebResourceResponse(
            _provider,
            path,
            context.Response(),
            method == HttpMethod::Head,
            *_contentTypeResolver,
            _configuration
        );
        if (result.Error == WebError::NotFound) return HttpHandlerResult::NotHandled();
        return HttpHandlerResult::Handled(result);
    }

private:
    IWebResourceProvider& _provider;
    DefaultHttpContentTypeResolver _defaultContentTypeResolver;
    const IHttpContentTypeResolver* _contentTypeResolver;
    StaticResourceConfiguration _configuration{};
};

} // namespace ESPressio::Web
