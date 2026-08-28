#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

enum class HttpMethod : uint8_t {
    Get = 0,
    Head,
    Post,
    Put,
    Patch,
    Delete,
    Options,
    Any
};

enum class HttpStatus : uint16_t {
    Continue = 100,
    Ok = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    MovedPermanently = 301,
    Found = 302,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    Conflict = 409,
    LengthRequired = 411,
    PayloadTooLarge = 413,
    UnsupportedMediaType = 415,
    TooManyRequests = 429,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503
};

enum class HttpTransportMode : uint8_t {
    Plain = 0,
    Tls
};

enum class HttpResponseState : uint8_t {
    Uncommitted = 0,
    Streaming,
    Completed,
    Aborted
};

struct HttpServerConfiguration final {
    uint16_t Port = 80;
    std::size_t MaximumConnections = 4;
    std::size_t MaximumHeaderBytes = 2048;
    std::size_t MaximumRequestBodyBytes = 64u * 1024u;
    bool KeepAlive = true;
    HttpTransportMode TransportMode = HttpTransportMode::Plain;
};

struct HttpReadResult final {
    WebResult Result;
    std::size_t BytesRead = 0;
    bool EndOfBody = false;

    explicit operator bool() const noexcept { return static_cast<bool>(Result); }
};

class IHttpRequestPlatform {
public:
    virtual ~IHttpRequestPlatform() = default;

    virtual HttpMethod Method() const noexcept = 0;
    virtual std::string_view Path() const noexcept = 0;
    virtual std::string_view QueryString() const noexcept = 0;
    virtual std::optional<std::size_t> ContentLength() const noexcept = 0;

    // Header lookup is deliberately lazy. The platform does not materialize
    // a request-wide header map. HeaderValueLength() returns zero when the
    // header is absent; ReadHeader() copies only the requested value into a
    // caller-owned bounded buffer.
    virtual std::size_t HeaderValueLength(std::string_view name) const noexcept = 0;
    virtual WebResult ReadHeader(
        std::string_view name,
        char* destination,
        std::size_t capacity,
        std::size_t& bytesWritten
    ) const = 0;

    virtual HttpReadResult ReadBody(uint8_t* destination, std::size_t capacity) = 0;
};

class IHttpResponsePlatform {
public:
    virtual ~IHttpResponsePlatform() = default;

    virtual WebResult SetStatus(HttpStatus status) = 0;
    virtual WebResult SetHeader(std::string_view name, std::string_view value) = 0;
    virtual WebResult Begin(std::optional<std::size_t> contentLength) = 0;
    virtual WebResult Write(const uint8_t* data, std::size_t size) = 0;
    virtual WebResult Complete() = 0;
    virtual void Abort() noexcept = 0;
};

class HttpRequest final {
public:
    explicit HttpRequest(IHttpRequestPlatform& platform) noexcept
        : _platform(platform) {}

    HttpRequest(const HttpRequest&) = delete;
    HttpRequest& operator=(const HttpRequest&) = delete;

    HttpMethod Method() const noexcept { return _platform.Method(); }
    std::string_view Path() const noexcept { return _platform.Path(); }
    std::string_view QueryString() const noexcept { return _platform.QueryString(); }
    std::optional<std::size_t> ContentLength() const noexcept { return _platform.ContentLength(); }

    bool HasHeader(std::string_view name) const noexcept {
        return _platform.HeaderValueLength(name) != 0;
    }

    std::size_t HeaderValueLength(std::string_view name) const noexcept {
        return _platform.HeaderValueLength(name);
    }

    WebResult ReadHeader(
        std::string_view name,
        char* destination,
        std::size_t capacity,
        std::size_t& bytesWritten
    ) const {
        bytesWritten = 0;
        if (destination == nullptr || capacity == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        return _platform.ReadHeader(name, destination, capacity, bytesWritten);
    }

    HttpReadResult ReadBody(uint8_t* destination, std::size_t capacity) {
        if (destination == nullptr || capacity == 0) {
            return {WebResult::Failure(WebError::InvalidConfiguration), 0, false};
        }
        return _platform.ReadBody(destination, capacity);
    }

private:
    IHttpRequestPlatform& _platform;
};

class HttpResponse final {
public:
    explicit HttpResponse(IHttpResponsePlatform& platform) noexcept
        : _platform(platform) {}

    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;

    ~HttpResponse() {
        if (_state == HttpResponseState::Streaming) {
            _platform.Abort();
            _state = HttpResponseState::Aborted;
        }
    }

    HttpResponseState State() const noexcept { return _state; }
    bool IsCommitted() const noexcept { return _state != HttpResponseState::Uncommitted; }
    bool IsCompleted() const noexcept { return _state == HttpResponseState::Completed; }

    WebResult Status(HttpStatus status) {
        if (_state != HttpResponseState::Uncommitted) {
            return WebResult::Failure(WebError::InvalidState);
        }
        return _platform.SetStatus(status);
    }

    WebResult Header(std::string_view name, std::string_view value) {
        if (_state != HttpResponseState::Uncommitted || name.empty()) {
            return WebResult::Failure(WebError::InvalidState);
        }
        return _platform.SetHeader(name, value);
    }

    WebResult ContentType(std::string_view value) {
        return Header("Content-Type", value);
    }

    WebResult Begin(std::optional<std::size_t> contentLength = std::nullopt) {
        if (_state != HttpResponseState::Uncommitted) {
            return WebResult::Failure(WebError::InvalidState);
        }
        const auto result = _platform.Begin(contentLength);
        if (result) _state = HttpResponseState::Streaming;
        return result;
    }

    WebResult Write(const uint8_t* data, std::size_t size) {
        if (data == nullptr || size == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        if (_state == HttpResponseState::Uncommitted) {
            const auto beginResult = Begin(std::nullopt);
            if (!beginResult) return beginResult;
        }
        if (_state != HttpResponseState::Streaming) {
            return WebResult::Failure(WebError::InvalidState);
        }
        return _platform.Write(data, size);
    }

    WebResult Write(std::string_view text) {
        return Write(
            reinterpret_cast<const uint8_t*>(text.data()),
            text.size()
        );
    }

    WebResult Send(
        std::string_view body,
        std::string_view contentType = "text/plain; charset=utf-8"
    ) {
        if (_state != HttpResponseState::Uncommitted) {
            return WebResult::Failure(WebError::InvalidState);
        }
        auto result = ContentType(contentType);
        if (!result) return result;
        result = Begin(body.size());
        if (!result) return result;
        if (!body.empty()) {
            result = _platform.Write(
                reinterpret_cast<const uint8_t*>(body.data()),
                body.size()
            );
            if (!result) {
                Abort();
                return result;
            }
        }
        return Complete();
    }

    WebResult Complete() {
        if (_state == HttpResponseState::Completed) return WebResult::Success();
        if (_state == HttpResponseState::Aborted) return WebResult::Failure(WebError::InvalidState);
        if (_state == HttpResponseState::Uncommitted) {
            const auto beginResult = Begin(std::size_t{0});
            if (!beginResult) return beginResult;
        }
        const auto result = _platform.Complete();
        if (result) _state = HttpResponseState::Completed;
        return result;
    }

    void Abort() noexcept {
        if (
            _state == HttpResponseState::Completed ||
            _state == HttpResponseState::Aborted
        ) {
            return;
        }
        _platform.Abort();
        _state = HttpResponseState::Aborted;
    }

private:
    IHttpResponsePlatform& _platform;
    HttpResponseState _state = HttpResponseState::Uncommitted;
};

class WebRequestContext {
public:
    WebRequestContext(
        IHttpRequestPlatform& requestPlatform,
        IHttpResponsePlatform& responsePlatform
    ) noexcept :
        _request(requestPlatform),
        _response(responsePlatform) {}

    WebRequestContext(const WebRequestContext&) = delete;
    WebRequestContext& operator=(const WebRequestContext&) = delete;

    HttpRequest& Request() noexcept { return _request; }
    const HttpRequest& Request() const noexcept { return _request; }
    HttpResponse& Response() noexcept { return _response; }
    const HttpResponse& Response() const noexcept { return _response; }

private:
    HttpRequest _request;
    HttpResponse _response;
};

} // namespace ESPressio::Web
