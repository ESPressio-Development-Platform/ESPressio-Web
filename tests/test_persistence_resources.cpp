#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include <ESPressio_WebPersistence.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

class FakeFileStorage final : public Persistence::IFileStorage {
public:
    std::string Path = "/index.html";
    std::string Data = "hello";

    Persistence::StorageStatus Initialize() override { return Persistence::StorageStatus::Success; }
    void Shutdown() override {}
    bool IsReady() const override { return true; }
    const char* GetBackendName() const override { return "fake"; }
    Persistence::StorageCapability GetCapabilities() const override {
        return Persistence::StorageCapability::Hierarchical;
    }
    Persistence::StorageStatistics GetStatistics() const override { return {}; }

    Persistence::StorageStatus Exists(const char* path, bool& exists) const override {
        exists = path != nullptr && Path == path;
        return Persistence::StorageStatus::Success;
    }

    Persistence::StorageStatus Stat(
        const char* path,
        Persistence::StorageEntry& entry
    ) const override {
        if (path == nullptr || Path != path) return Persistence::StorageStatus::NotFound;
        entry.size = Data.size();
        entry.isDirectory = false;
        std::strncpy(entry.path, path, sizeof(entry.path) - 1);
        return Persistence::StorageStatus::Success;
    }

    Persistence::StorageStatus Read(
        const char* path,
        uint64_t offset,
        uint8_t* buffer,
        std::size_t capacity,
        std::size_t& bytesRead
    ) const override {
        bytesRead = 0;
        if (path == nullptr || Path != path) return Persistence::StorageStatus::NotFound;
        if (buffer == nullptr && capacity != 0) return Persistence::StorageStatus::InvalidArgument;
        const auto start = static_cast<std::size_t>(offset);
        if (start >= Data.size()) return Persistence::StorageStatus::Success;
        const auto remaining = Data.size() - start;
        bytesRead = remaining < capacity ? remaining : capacity;
        std::memcpy(buffer, Data.data() + start, bytesRead);
        return Persistence::StorageStatus::Success;
    }

    Persistence::StorageStatus Write(const char*, const uint8_t*, std::size_t, Persistence::WriteMode) override {
        return Persistence::StorageStatus::NotSupported;
    }
    Persistence::StorageStatus Remove(const char*) override { return Persistence::StorageStatus::NotSupported; }
    Persistence::StorageStatus Rename(const char*, const char*) override { return Persistence::StorageStatus::NotSupported; }
    Persistence::StorageStatus CreateDirectory(const char*) override { return Persistence::StorageStatus::NotSupported; }
    Persistence::StorageStatus RemoveDirectory(const char*) override { return Persistence::StorageStatus::NotSupported; }
    Persistence::StorageStatus List(const char*, Persistence::StorageListCallback, void*) const override {
        return Persistence::StorageStatus::NotSupported;
    }
};

void TestAdapter() {
    FakeFileStorage storage;
    PersistenceWebResourceProvider provider(storage);

    WebResourceMetadata metadata;
    assert(provider.Stat("/index.html", metadata));
    assert(metadata.Size == 5);
    assert(!metadata.IsDirectory);

    uint8_t buffer[3]{};
    auto read = provider.Read("/index.html", 1, buffer, sizeof(buffer));
    assert(read);
    assert(read.BytesRead == 3);
    assert(std::string(reinterpret_cast<char*>(buffer), read.BytesRead) == "ell");

    assert(provider.Stat("/missing", metadata).Error == WebError::NotFound);
}

} // namespace

int main() {
    TestAdapter();
    return 0;
}
