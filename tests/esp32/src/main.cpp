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

ESPressio::Web::ESP32DnsServerPlatform dnsPlatform;
ESPressio::Web::DnsServer dnsServer(dnsPlatform);
ESPressio::Web::WildcardDnsHandler wildcardDns(
    ESPressio::Web::DnsAddress::IPv4(192, 168, 4, 1)
);

} // namespace

void setup() {
    ESPressio::Web::RouteHandle route;
    (void)router.RegisterRoute(
        ESPressio::Web::HttpMethod::Get,
        "/",
        root,
        route
    );
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
