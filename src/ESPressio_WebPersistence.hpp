#pragma once

#if !__has_include(<ESPressio_IFileStorage.hpp>)
#error "ESPressio Web Persistence integration requires ESPressio-Persistence."
#endif

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <utility>

#include <ESPressio_IFileStorage.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_PolymorphicMemory.hpp>

#include "ESPressio_Resources.hpp"

namespace ESPressio::Web {

class PersistenceWebResourceProvider final : public IWebResourceProvider {
private:
    /// <summary>Adapts one Persistence sequential file stream to the Web resource stream contract.</summary>
    class ResourceReadStream final : public IWebResourceReadStream {
    public:
        explicit ResourceReadStream(Persistence::FileReadStreamPtr stream)
            : _stream(std::move(stream)) {}

        uint64_t Size() const noexcept override {
            return _stream ? _stream->Size() : 0;
        }

        WebResourceReadResult Read(
            uint8_t* destination,
            std::size_t capacity
        ) override {
            if (!_stream) {
                return {WebResult::Failure(WebError::InvalidState), 0};
            }
            std::size_t bytesRead = 0;
            const auto status = _stream->Read(
                destination,
                capacity,
                bytesRead
            );
            return {Translate(status), bytesRead};
        }

    private:
        Persistence::FileReadStreamPtr _stream;
    };

public:
    explicit PersistenceWebResourceProvider(Persistence::IFileStorage& storage)
        : _storage(storage) {}

    WebResult Stat(std::string_view path, WebResourceMetadata& metadata) const override {
        metadata = {};
        const auto nativePath = MakePath(path);
        Persistence::StorageEntry entry;
        const auto status = _storage.Stat(nativePath.c_str(), entry);
        if (status != Persistence::StorageStatus::Success) {
            return Translate(status);
        }
        metadata.Size = entry.size;
        metadata.IsDirectory = entry.isDirectory;
        return WebResult::Success();
    }

    WebResourceReadResult Read(
        std::string_view path,
        uint64_t offset,
        uint8_t* destination,
        std::size_t capacity
    ) const override {
        if (destination == nullptr || capacity == 0) {
            return {WebResult::Failure(WebError::InvalidConfiguration), 0};
        }

        const auto nativePath = MakePath(path);
        std::size_t bytesRead = 0;
        const auto status = _storage.Read(
            nativePath.c_str(),
            offset,
            destination,
            capacity,
            bytesRead
        );
        return {Translate(status), bytesRead};
    }

    /// <summary>Opens the persistence file once and adapts it to a sequential Web resource stream.</summary>
    /// <remarks>The native path and stream wrappers use <c>ExternalPreferred</c> storage. ESP32 backends therefore avoid reopening LittleFS/SPIFFS/SD files for every HTTP response chunk.</remarks>
    WebResult OpenRead(
        std::string_view path,
        WebResourceReadStreamPtr& stream
    ) const override {
        stream.reset();
        const auto nativePath = MakePath(path);
        Persistence::FileReadStreamPtr persistenceStream;
        const auto status = _storage.OpenRead(
            nativePath.c_str(),
            persistenceStream
        );
        if (status != Persistence::StorageStatus::Success) {
            return Translate(status);
        }
        if (!persistenceStream) {
            return WebResult::Failure(WebError::PlatformFailure);
        }

        try {
            stream = System::Memory::MakePolymorphicUnique<
                IWebResourceReadStream,
                ResourceReadStream,
                System::Memory::MemoryPolicy::ExternalPreferred
            >(std::move(persistenceStream));
        } catch (const std::bad_alloc&) {
            return WebResult::Failure(WebError::ResourceExhausted);
        } catch (...) {
            return WebResult::Failure(WebError::PlatformFailure);
        }
        return stream
            ? WebResult::Success()
            : WebResult::Failure(WebError::ResourceExhausted);
    }

private:
    using PathString = System::Memory::String<
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    static PathString MakePath(std::string_view path) {
        return PathString(path.begin(), path.end());
    }

    static WebResult Translate(Persistence::StorageStatus status) noexcept {
        using Persistence::StorageStatus;
        switch (status) {
            case StorageStatus::Success:
                return WebResult::Success();
            case StorageStatus::NotFound:
                return WebResult::Failure(WebError::NotFound);
            case StorageStatus::InvalidArgument:
                return WebResult::Failure(WebError::InvalidConfiguration);
            case StorageStatus::NotInitialized:
                return WebResult::Failure(WebError::InvalidState);
            case StorageStatus::NotSupported:
                return WebResult::Failure(WebError::Unsupported);
            case StorageStatus::Busy:
            case StorageStatus::NoSpace:
                return WebResult::Failure(WebError::ResourceExhausted);
            case StorageStatus::PermissionDenied:
            case StorageStatus::CorruptData:
            case StorageStatus::IoError:
            case StorageStatus::PartialWrite:
            case StorageStatus::AlreadyInitialized:
            case StorageStatus::AlreadyExists:
            case StorageStatus::UnknownError:
            default:
                return WebResult::Failure(WebError::PlatformFailure);
        }
    }

    Persistence::IFileStorage& _storage;
};

} // namespace ESPressio::Web
