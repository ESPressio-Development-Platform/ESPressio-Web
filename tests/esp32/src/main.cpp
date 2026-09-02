#include <Arduino.h>

#include <ESPressio_Web.hpp>
#include <ESPressio_ESP32.hpp>

namespace {

class RootHandler final : public ESPressio::Web::IHttpRouteHandler {
public:
    ESPressio::Web::HttpHandlerResult Handle(
        ESPressio::Web::WebRequestContext& context,
        const ESPressio::Web::RouteParameters&
    ) override {
        return ESPressio::Web::HttpHandlerResult::Handled(
            context.Response().Send("ok")
        );
    }
};

ESPressio::Web::ESP32HttpServerPlatform httpPlatform;
ESPressio::Web::HttpServer httpServer(httpPlatform);
ESPressio::Web::Router router;
ESPressio::Web::HttpApplication application(&router);
RootHandler root;

ESPressio::Web::ESP32WebSocketEndpointPlatform webSocketPlatform(httpPlatform);
ESPressio::Web::WebSocketEndpoint webSocketEndpoint(webSocketPlatform);

// The ESP32 WebSocket client provider is an optional concrete capability. The
// current Arduino/IDF platform bundle used by this integration target does not
// necessarily ship the separately packaged esp_websocket_client component, so
// exercise it only when that native component is actually present.
#if __has_include(<esp_websocket_client.h>)
ESPressio::Web::ESP32WebSocketClientPlatform webSocketClientPlatform;
ESPressio::Web::WebSocketClient webSocketClient(webSocketClientPlatform);
#endif

ESPressio::Web::ESP32DnsServerPlatform dnsPlatform;
ESPressio::Web::DnsServer dnsServer(dnsPlatform);
ESPressio::Web::WildcardDnsHandler wildcardDns(
    ESPressio::Web::DnsAddress::IPv4(192, 168, 4, 1)
);

} // namespace

void setup() {
    ESPressio::Web::RouteHandle getRoute;
    (void)router.RegisterRoute(
        ESPressio::Web::HttpMethod::Get,
        "/",
        root,
        getRoute
    );

    // The same application handler is intentionally registered for HEAD so the
    // ESP32 provider must suppress the body while preserving Content-Length.
    ESPressio::Web::RouteHandle headRoute;
    (void)router.RegisterRoute(
        ESPressio::Web::HttpMethod::Head,
        "/",
        root,
        headRoute
    );

    ESPressio::Web::WebSocketEndpointConfiguration webSocketConfiguration;
    webSocketConfiguration.Path = "/application/socket";
    webSocketConfiguration.Protocol = "espressio.v1";

    // Exercise the provider's retired-state lifecycle before the HTTP server
    // starts. A later bind must use a fresh native binding state rather than
    // reactivating a handler state that has already been released.
    (void)webSocketEndpoint.Bind(webSocketConfiguration);
    (void)webSocketEndpoint.Unbind();
    (void)webSocketEndpoint.Bind(webSocketConfiguration);

    (void)httpServer.SetRequestHandler(&application);

    ESPressio::Web::HttpServerConfiguration httpConfiguration;
    httpConfiguration.Port = 80;
    (void)httpServer.Initialize(httpConfiguration);
    (void)httpServer.Start();

    (void)dnsServer.SetRequestHandler(&wildcardDns);
    ESPressio::Web::DnsServerConfiguration dnsConfiguration;
    dnsConfiguration.Port = 53;
    (void)dnsServer.Initialize(dnsConfiguration);
    (void)dnsServer.Start();

#if __has_include(<esp_websocket_client.h>)
    // millis() keeps this branch runtime-reachable so LTO cannot prove away
    // the optional client's Connect implementation. It is false during boot.
    if (millis() == UINT32_MAX) {
        ESPressio::Web::WebSocketClientConfiguration clientConfiguration;
        clientConfiguration.Host = "127.0.0.1";
        clientConfiguration.Port = 80;
        clientConfiguration.Path = "/compile-only";
        (void)webSocketClient.Connect(clientConfiguration);
    }
#endif
}

void loop() {}
