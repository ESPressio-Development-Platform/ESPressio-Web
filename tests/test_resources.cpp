#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <ESPressio_HttpApplication.hpp>

using namespace ESPressio::Web;

namespace {

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue = HttpMethod::Get;
    std::string PathValue = "/index.html";

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return PathValue; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return std::nullopt; }
    bool HasHeader(std::string_view) const noexcept override { return false; }
    std::size_t HeaderValueLength(std::string_view) const noexcept override { return 0; }
    WebResult ReadHeader(std::string_view, char*, std::size_t, std::size_t& written) const override {
        written = 0;
        return WebResult::Failure(WebError::NotFound);
    }
    HttpReadResult ReadBody(uint8_t*, std::size_t) override {
        return {WebResult::Success(), 0, true};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    HttpStatus StatusValue = HttpStatus::Ok;
    std::unordered_map<std::string, std::string> Headers;
    std::optional<std::size_t> Length;
    std::string Body;
    bool Begun = false;
    bool Completed = false;
    bool Aborted = false;

    WebResult SetStatus(HttpStatus status) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        StatusValue = status;
        return WebResult::Success();
    }
    WebResult SetHeader(std::string_view name, std::string_view value) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        Headers[std::string(name)] = std::string(value);
        return WebResult::Success();
    }
    WebResult Begin(std::optional<std::size_t> length) override {
        if (Begun) return WebResult::Failure(WebError::InvalidState);
        Begun = true;
        Length = length;
        return WebResult::Success();
    }
    WebResult Write(const uint8_t* data, std::size_t size) override {
        Body.append(reinterpret_cast<const char*>(data), size);
        return WebResult::Success();
    }
    WebResult Complete() override {
        Completed = true;
        return WebResult::Success();
    }
    void Abort() noexcept override { Aborted = true; }
};

class MemoryResources final : public IWebResourceProvider {
public:
    std::unordered_map<std::string, std::string> Files;
    mutable int ReadCalls = 0;

    WebResult Stat(std::string_view path, WebResourceMetadata& metadata) const override {
        const auto it = Files.find(std::string(path));
        if (it == Files.end()) return WebResult::Failure(WebError::NotFound);
        metadata.Size = it->second.size();
        metadata.IsDirectory = false;
        return WebResult::Success();
    }

    WebResourceReadResult Read(
        std::string_view path,
        uint64_t offset,
        uint8_t* destination,
        std::size_t capacity
    ) const override {
        const auto it = Files.find(std::string(path));
        if (it == Files.end()) return {WebResult::Failure(WebError::NotFound), 0};
        ++ReadCalls;
        const auto start = static_cast<std::size_t>(offset);
        if (start >= it->second.size()) return {WebResult::Success(), 0};
        const auto remaining = it->second.size() - start;
        const auto count = remaining < capacity ? remaining : capacity;
        std::memcpy(destination, it->second.data() + start, count);
        return {WebResult::Success(), count};
    }
};

class SequentialMemoryResources final : public IWebResourceProvider {
private:
    class ReadStream final : public IWebResourceReadStream {
    public:
        explicit ReadStream(std::string data)
            : _data(std::move(data)) {}

        uint64_t Size() const noexcept override {
            return static_cast<uint64_t>(_data.size());
        }

        WebResourceReadResult Read(
            uint8_t* destination,
            std::size_t capacity
        ) override {
            if (destination == nullptr && capacity != 0) {
                return {WebResult::Failure(WebError::InvalidConfiguration), 0};
            }
            if (_offset >= _data.size()) return {WebResult::Success(), 0};
            const auto remaining = _data.size() - _offset;
            const auto count = remaining < capacity ? remaining : capacity;
            std::memcpy(destination, _data.data() + _offset, count);
            _offset += count;
            return {WebResult::Success(), count};
        }

    private:
        std::string _data;
        std::size_t _offset = 0;
    };

public:
    std::unordered_map<std::string, std::string> Files;
    mutable int StatCalls = 0;
    mutable int RandomReadCalls = 0;
    mutable int OpenReadCalls = 0;

    WebResult Stat(std::string_view path, WebResourceMetadata& metadata) const override {
        ++StatCalls;
        const auto it = Files.find(std::string(path));
        if (it == Files.end()) return WebResult::Failure(WebError::NotFound);
        metadata.Size = it->second.size();
        metadata.IsDirectory = false;
        return WebResult::Success();
    }

    WebResourceReadResult Read(
        std::string_view,
        uint64_t,
        uint8_t*,
        std::size_t
    ) const override {
        ++RandomReadCalls;
        return {WebResult::Failure(WebError::PlatformFailure), 0};
    }

    WebResult OpenRead(
        std::string_view path,
        WebResourceReadStreamPtr& stream
    ) const override {
        ++OpenReadCalls;
        const auto it = Files.find(std::string(path));
        if (it == Files.end()) return WebResult::Failure(WebError::NotFound);
        stream = ESPressio::System::Memory::MakePolymorphicUnique<
            IWebResourceReadStream,
            ReadStream,
            ESPressio::System::Memory::MemoryPolicy::ExternalPreferred
        >(it->second);
        return WebResult::Success();
    }
};

class AlwaysNotHandled final : public IHttpRequestHandler {
public:
    int Calls = 0;
    HttpHandlerResult Handle(WebRequestContext&) override {
        ++Calls;
        return HttpHandlerResult::NotHandled();
    }
};

HttpHandlerResult Invoke(IHttpRequestHandler& handler, Request& request, Response& response) {
    WebRequestContext context(request, response);
    return handler.Handle(context);
}

void TestStaticResourceStreamingFallback() {
    MemoryResources resources;
    resources.Files["/index.html"] = "abcdefghij";
    StaticResourceHandler staticHandler(resources);
    StaticResourceConfiguration config;
    config.ReadChunkBytes = 4;
    assert(staticHandler.Configure(config));

    Request request;
    Response response;
    const auto result = Invoke(staticHandler, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Completed && !response.Aborted);
    assert(response.Body == "abcdefghij");
    assert(response.Length == std::optional<std::size_t>(10));
    assert(response.Headers["Content-Type"] == "text/html; charset=utf-8");
    assert(resources.ReadCalls == 3);
}

void TestSequentialResourceStreamingUsesOneOpen() {
    SequentialMemoryResources resources;
    resources.Files["/index.html"] = "abcdefghij";
    StaticResourceHandler staticHandler(resources);
    StaticResourceConfiguration config;
    config.ReadChunkBytes = 4;
    assert(staticHandler.Configure(config));

    Request request;
    Response response;
    const auto result = Invoke(staticHandler, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::Handled);
    assert(response.Completed && !response.Aborted);
    assert(response.Body == "abcdefghij");
    assert(response.Length == std::optional<std::size_t>(10));
    assert(resources.OpenReadCalls == 1);
    assert(resources.StatCalls == 0);
    assert(resources.RandomReadCalls == 0);
}

void TestHeadDoesNotReadBody() {
    MemoryResources resources;
    resources.Files["/app.js"] = "console.log(1);";
    StaticResourceHandler staticHandler(resources);

    Request request;
    request.MethodValue = HttpMethod::Head;
    request.PathValue = "/app.js";
    Response response;
    const auto result = Invoke(staticHandler, request, response);
    assert(result && response.Completed);
    assert(response.Body.empty());
    assert(response.Length == std::optional<std::size_t>(15));
    assert(resources.ReadCalls == 0);
}

void TestMissingFallsThroughAndTraversalFails() {
    MemoryResources resources;
    StaticResourceHandler staticHandler(resources);

    Request request;
    request.PathValue = "/missing.txt";
    Response response;
    auto result = Invoke(staticHandler, request, response);
    assert(result && result.Disposition == HttpHandlerDisposition::NotHandled);

    request.PathValue = "/assets/../secret.txt";
    Response traversalResponse;
    result = Invoke(staticHandler, request, traversalResponse);
    assert(!result.Result);
    assert(result.Result.Error == WebError::ProtocolError);
    assert(!traversalResponse.Begun);
}

void TestApplicationFallbackAndDefaultError() {
    MemoryResources resources;
    resources.Files["/app.js"] = "asset";
    StaticResourceHandler staticHandler(resources);
    AlwaysNotHandled primary;
    HttpApplication application(&primary, &staticHandler);

    Request request;
    request.PathValue = "/app.js";
    Response response;
    auto result = Invoke(application, request, response);
    assert(result && response.Body == "asset");
    assert(primary.Calls == 1);

    request.PathValue = "/missing";
    Response missingResponse;
    result = Invoke(application, request, missingResponse);
    assert(result && missingResponse.Completed);
    assert(missingResponse.StatusValue == HttpStatus::NotFound);
    assert(missingResponse.Body == "Not Found");
}

void TestResourceBackedErrorPage() {
    MemoryResources resources;
    resources.Files["/errors/not-found.html"] = "<h1>gone</h1>";
    ResourceHttpErrorResponder errors(resources);
    assert(errors.ConfigureResource(
        WebError::NotFound,
        "/errors/not-found.html",
        HttpStatus::NotFound
    ));

    HttpApplication application(nullptr, nullptr, &errors);
    Request request;
    request.PathValue = "/nothing";
    Response response;
    const auto result = Invoke(application, request, response);
    assert(result && response.Completed);
    assert(response.StatusValue == HttpStatus::NotFound);
    assert(response.Body == "<h1>gone</h1>");
    assert(response.Headers["Content-Type"] == "text/html; charset=utf-8");
}

} // namespace

int main() {
    TestStaticResourceStreamingFallback();
    TestSequentialResourceStreamingUsesOneOpen();
    TestHeadDoesNotReadBody();
    TestMissingFallsThroughAndTraversalFails();
    TestApplicationFallbackAndDefaultError();
    TestResourceBackedErrorPage();
    return 0;
}
