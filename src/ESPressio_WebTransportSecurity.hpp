#pragma once

#include <cstddef>
#include <cstdint>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebTransportMode : uint8_t {
    Plain = 0,
    Tls
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebCredentialEncoding : uint8_t {
    Pem = 0,
    Der
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Data (uint8_t*): 4 bytes [0 bytes dynamic allocation]
 * - Size (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - Encoding (WebCredentialEncoding): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebCredentialView final {
    const uint8_t* Data = nullptr;
    std::size_t Size = 0;
    WebCredentialEncoding Encoding = WebCredentialEncoding::Pem;

    constexpr bool Empty() const noexcept {
        return Data == nullptr || Size == 0;
    }

    constexpr bool Valid() const noexcept {
        return (Data == nullptr) == (Size == 0);
    }
};

/**
 * ESPressio Memory Audit
 * Underlying storage: 1 bytes
 * Total Memory: 1 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
enum class WebTlsServerTrustMode : uint8_t {
    // The concrete platform may use this only when its platform trust source
    // authenticates the peer. A native "TLS without verification" default is
    // not an implementation of PlatformTrust.
    PlatformTrust = 0,

    // Authenticate the server against the caller-provided CA/certificate
    // material in ServerCertificateAuthority.
    CertificateAuthority
};

/**
 * ESPressio Memory Audit
 * Members:
 * - ServerTrust (WebTlsServerTrustMode): 1 bytes [0 bytes dynamic allocation]
 * - ServerCertificateAuthority (WebCredentialView): 12 bytes [0 bytes dynamic allocation]
 * - ClientCertificate (WebCredentialView): 12 bytes [0 bytes dynamic allocation]
 * - ClientPrivateKey (WebCredentialView): 12 bytes [0 bytes dynamic allocation]
 * Total Memory: 40 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebTlsConfiguration final {
    WebTlsServerTrustMode ServerTrust = WebTlsServerTrustMode::PlatformTrust;
    WebCredentialView ServerCertificateAuthority;
    WebCredentialView ClientCertificate;
    WebCredentialView ClientPrivateKey;

    constexpr bool HasClientIdentity() const noexcept {
        return !ClientCertificate.Empty() && !ClientPrivateKey.Empty();
    }

    constexpr WebResult Validate() const noexcept {
        if (!ServerCertificateAuthority.Valid() ||
            !ClientCertificate.Valid() ||
            !ClientPrivateKey.Valid()) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        if (ServerTrust == WebTlsServerTrustMode::CertificateAuthority) {
            if (ServerCertificateAuthority.Empty()) {
                return WebResult::Failure(WebError::InvalidConfiguration);
            }
        } else if (!ServerCertificateAuthority.Empty()) {
            // A caller must choose exactly one server-authentication source.
            // Never accept CA bytes that a PlatformTrust implementation would
            // then silently ignore.
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        const bool hasCertificate = !ClientCertificate.Empty();
        const bool hasPrivateKey = !ClientPrivateKey.Empty();
        if (hasCertificate != hasPrivateKey) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        return WebResult::Success();
    }
};

} // namespace ESPressio::Web
