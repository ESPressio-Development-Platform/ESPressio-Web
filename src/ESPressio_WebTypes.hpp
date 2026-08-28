#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ESPressio::Web {

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
    PlatformFailure
};

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

enum class WebCapability : uint32_t {
    None = 0,
    Http = 1u << 0,
    ChunkedResponses = 1u << 1,
    PersistentConnections = 1u << 2,
    WebSocketServer = 1u << 3,
    WebSocketClient = 1u << 4,
    Tls = 1u << 5,
    Dns = 1u << 6,
    WildcardDns = 1u << 7
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
