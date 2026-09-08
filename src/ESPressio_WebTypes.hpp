#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebError : uint8_t {
    None = 0,
    InvalidConfiguration,
    AlreadyRunning,
    NotRunning,
    NotFound,
    ResourceExhausted,
    ConnectionFailure,
    RequestTooLarge,
    Unsupported,
    InvalidState,
    ProtocolError,
    Closed,
    PlatformFailure,
    Count
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Error (WebError): 1 bytes [0 bytes dynamic allocation]
 * - PlatformCode (int32_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebResult final {
    WebError Error = WebError::None;
    int32_t PlatformCode = 0;

    constexpr bool Succeeded() const noexcept { return Error == WebError::None; }
    constexpr explicit operator bool() const noexcept { return Succeeded(); }

    static constexpr WebResult Success() noexcept { return {}; }
    static constexpr WebResult Failure(WebError error, int32_t platformCode = 0) noexcept {
        return {error, platformCode};
    }
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 4 bytes
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebCapability : uint32_t {
    None = 0,
    Http = 1u << 0,
    ChunkedResponses = 1u << 1,
    PersistentConnections = 1u << 2,
    WebSocketServer = 1u << 3,
    WebSocketClient = 1u << 4,
    Tls = 1u << 5,
    Dns = 1u << 6
};

using WebCapabilities = uint32_t;

constexpr WebCapabilities ToCapabilities(WebCapability capability) noexcept {
    return static_cast<WebCapabilities>(capability);
}

constexpr WebCapabilities operator|(WebCapability left, WebCapability right) noexcept {
    return ToCapabilities(left) | ToCapabilities(right);
}

constexpr bool HasCapability(WebCapabilities capabilities, WebCapability capability) noexcept {
    return (capabilities & ToCapabilities(capability)) != 0;
}

using WebSocketConnectionId = uint64_t;

} // namespace ESPressio::Web
