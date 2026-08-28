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

    (void)dnsServer.SetRequestHandler(&wildcardDns);
    ESPressio::Web::DnsServerConfiguration dnsConfiguration;
    dnsConfiguration.Port = 53;
    (void)dnsServer.Initialize(dnsConfiguration);
}

void loop() {}
