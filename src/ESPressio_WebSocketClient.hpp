#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>

#include <ESPressio_Memory.hpp>
#include <ESPressio_ThreadSafeObservable.hpp>

#include "ESPressio_WebSocket.hpp"

namespace ESPressio::Web {

class IWebSocketClientObserver : public Observable::IObserver {
public:
    ~IWebSocketClientObserver() override = default;
    virtual void OnWebSocketClientConnected(IWebSocketConnection&) {}
    virtual void OnWebSocketClientBinary(IWebSocketConnection&, const uint8_t*, std::size_t) {}
    virtual void OnWebSocketClientText(IWebSocketConnection&, std::string_view) {}
    virtual void OnWebSocketClientDisconnected(const WebSocketCloseReason&) {}
};

class WebSocketClient final : private IWebSocketClientPlatformSink {
private:
    class ClientObservable final : public Observable::ThreadSafeObservable {
    public:
        void Connected(IWebSocketConnection& connection) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientConnected(connection);
                    }
                );
            });
        }

        void Binary(IWebSocketConnection& connection, const uint8_t* data, std::size_t size) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientBinary(connection, data, size);
                    }
                );
            });
        }

        void Text(IWebSocketConnection& connection, std::string_view text) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientText(connection, text);
                    }
                );
            });
        }

        void Disconnected(const WebSocketCloseReason& reason) {
            ExecuteNotification([&](NotificationContext& context) {
                context.WithObservers<IWebSocketClientObserver>(
                    [&](IWebSocketClientObserver* observer) {
                        observer->OnWebSocketClientDisconnected(reason);
                    }
                );
            });
        }
    };

public:
    WebSocketClient()
        : _observable(
            System::Memory::MakeShared<
                ClientObservable,
                System::Memory::MemoryPolicy::ExternalPreferred
            >()
          ) {}

    explicit WebSocketClient(IWebSocketClientPlatform& platform)
        : WebSocketClient() {
        Attach(platform);
    }

    ~WebSocketClient() { Detach(); }

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    WebResult Attach(IWebSocketClientPlatform& platform) {
        if (_platform == &platform) return WebResult::Success();
        if (_platform != nullptr) return WebResult::Failure(WebError::InvalidState);
        _platform = &platform;
        _platform->SetSink(this);
        return WebResult::Success();
    }

    void Detach() {
        if (_platform == nullptr) return;
        _platform->SetSink(nullptr);
        _platform = nullptr;
    }

    Observable::ObserverHandlePtr RegisterObserver(IWebSocketClientObserver* observer) {
        return _observable->template RegisterObserverAs<IWebSocketClientObserver>(observer);
    }

    void UnregisterObserver(IWebSocketClientObserver* observer) {
        _observable->UnregisterObserver(observer);
    }

    WebResult Connect(const WebSocketClientConfiguration& configuration) {
        if (_platform == nullptr) return WebResult::Failure(WebError::InvalidState);
        const auto validation = ValidateConfiguration(configuration);
        return validation ? _platform->Connect(configuration) : validation;
    }

    WebResult Disconnect(const WebSocketCloseReason& reason = {}) {
        return _platform == nullptr
            ? WebResult::Failure(WebError::InvalidState)
            : _platform->Disconnect(reason);
    }

    bool IsConnected() const noexcept {
        return _platform != nullptr && _platform->IsConnected();
    }

    IWebSocketConnection* Connection() noexcept {
        return _platform == nullptr ? nullptr : _platform->Connection();
    }

private:
    static constexpr std::size_t MaximumHandshakeHeaderCount = 64;

    static char LowerAscii(char value) noexcept {
        return value >= 'A' && value <= 'Z'
            ? static_cast<char>(value + ('a' - 'A'))
            : value;
    }

    static bool HeaderNameEquals(std::string_view left, std::string_view right) noexcept {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (LowerAscii(left[index]) != LowerAscii(right[index])) return false;
        }
        return true;
    }

    static bool IsReservedHandshakeHeader(std::string_view name) noexcept {
        return HeaderNameEquals(name, "Host") ||
               HeaderNameEquals(name, "Connection") ||
               HeaderNameEquals(name, "Upgrade") ||
               HeaderNameEquals(name, "Sec-WebSocket-Key") ||
               HeaderNameEquals(name, "Sec-WebSocket-Version") ||
               HeaderNameEquals(name, "Sec-WebSocket-Protocol");
    }

    static bool ContainsInvalidHeaderNameCharacter(std::string_view name) noexcept {
        if (name.empty()) return true;
        for (const unsigned char character : name) {
            if (character <= 0x20 || character >= 0x7f || character == ':') {
                return true;
            }
        }
        return false;
    }

    static bool ContainsHeaderLineBreak(std::string_view value) noexcept {
        return value.find('\r') != std::string_view::npos ||
               value.find('\n') != std::string_view::npos;
    }

    static WebResult ValidateHeaders(
        const IWebClientHeaderSource* headers,
        std::size_t maximumBytes
    ) noexcept {
        if (headers == nullptr) return WebResult::Success();
        if (maximumBytes == 0) return WebResult::Failure(WebError::InvalidConfiguration);

        const std::size_t count = headers->Count();
        if (count > MaximumHandshakeHeaderCount) {
            return WebResult::Failure(WebError::ResourceExhausted);
        }

        std::size_t totalBytes = 0;
        for (std::size_t index = 0; index < count; ++index) {
            WebClientHeader header;
            if (!headers->Header(index, header) ||
                ContainsInvalidHeaderNameCharacter(header.Name) ||
                IsReservedHandshakeHeader(header.Name) ||
                ContainsHeaderLineBreak(header.Value)) {
                return WebResult::Failure(WebError::InvalidConfiguration);
            }

            constexpr std::size_t FramingBytes = 4; // ": " + CRLF
            if (header.Name.size() > std::numeric_limits<std::size_t>::max() - header.Value.size() ||
                header.Name.size() + header.Value.size() >
                    std::numeric_limits<std::size_t>::max() - FramingBytes) {
                return WebResult::Failure(WebError::ResourceExhausted);
            }
            const std::size_t lineBytes = header.Name.size() + header.Value.size() + FramingBytes;
            if (lineBytes > maximumBytes || totalBytes > maximumBytes - lineBytes) {
                return WebResult::Failure(WebError::ResourceExhausted);
            }
            totalBytes += lineBytes;
        }
        return WebResult::Success();
    }

    static WebResult ValidateConfiguration(
        const WebSocketClientConfiguration& configuration
    ) noexcept {
        if (configuration.Host.empty() ||
            configuration.Port == 0 ||
            configuration.Path.empty() ||
            configuration.Path.front() != '/' ||
            configuration.Policy.NetworkTimeoutMilliseconds == 0 ||
            configuration.Policy.MaximumHandshakeHeaderBytes == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        if (configuration.Policy.AutomaticReconnect &&
            configuration.Policy.ReconnectDelayMilliseconds == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        if (configuration.Policy.PingIntervalMilliseconds != 0 &&
            configuration.Policy.PongTimeoutMilliseconds == 0) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        if (configuration.Policy.TcpKeepAlive &&
            (configuration.Policy.TcpKeepAliveIdleSeconds == 0 ||
             configuration.Policy.TcpKeepAliveIntervalSeconds == 0 ||
             configuration.Policy.TcpKeepAliveProbeCount == 0)) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }

        const auto headerValidation = ValidateHeaders(
            configuration.Headers,
            configuration.Policy.MaximumHandshakeHeaderBytes
        );
        if (!headerValidation) return headerValidation;

        if (configuration.Transport == WebTransportMode::Tls) {
            return configuration.Tls.Validate();
        }

        if (configuration.Tls.ServerTrust != WebTlsServerTrustMode::PlatformTrust ||
            !configuration.Tls.ServerCertificateAuthority.Empty() ||
            !configuration.Tls.ClientCertificate.Empty() ||
            !configuration.Tls.ClientPrivateKey.Empty()) {
            return WebResult::Failure(WebError::InvalidConfiguration);
        }
        return WebResult::Success();
    }

    IWebSocketClientPlatform* _platform = nullptr;
    std::shared_ptr<ClientObservable> _observable;

    void OnPlatformWebSocketClientConnected(IWebSocketConnection& connection) override {
        _observable->Connected(connection);
    }

    void OnPlatformWebSocketClientBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        _observable->Binary(connection, data, size);
    }

    void OnPlatformWebSocketClientText(
        IWebSocketConnection& connection,
        std::string_view text
    ) override {
        _observable->Text(connection, text);
    }

    void OnPlatformWebSocketClientDisconnected(const WebSocketCloseReason& reason) override {
        _observable->Disconnected(reason);
    }
};

} // namespace ESPressio::Web
