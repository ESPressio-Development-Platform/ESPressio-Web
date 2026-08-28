#pragma once

#if !__has_include(<ESPressio_IFileStorage.hpp>)
#error "ESPressio Web Persistence integration requires ESPressio-Persistence."
#endif

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

#include <ESPressio_IFileStorage.hpp>
#include <ESPressio_Memory.hpp>

#include "ESPressio_Resources.hpp"

namespace ESPressio::Web {

class PersistenceWebResourceProvider final : public IWebResourceProvider {
public:
    explicit PersistenceWebResourceProvider(Persistence::IFileStorage& storage)
        : _storage(storage) {}

    WebResult Stat(std::string_view path, WebResourceMetadata& metadata) const override {
        metadata = {};
        std::lock_guard<std::mutex> lock(_pathMutex);
        AssignPath(path);

        Persistence::StorageEntry entry;
        const auto status = _storage.Stat(_nativePath.c_str(), entry);
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

        std::lock_guard<std::mutex> lock(_pathMutex);
        AssignPath(path);

        std::size_t bytesRead = 0;
        const auto status = _storage.Read(
            _nativePath.c_str(),
            offset,
            destination,
            capacity,
            bytesRead
        );
        return {Translate(status), bytesRead};
    }

private:
    using PathString = System::Memory::String<
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    void AssignPath(std::string_view path) const {
        _nativePath.assign(path.begin(), path.end());
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
    mutable std::mutex _pathMutex;
    mutable PathString _nativePath;
};

} // namespace ESPressio::Web
