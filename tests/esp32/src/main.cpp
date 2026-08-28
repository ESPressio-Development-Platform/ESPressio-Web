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

ESPressio::Web::ESP32HttpServerPlatform platform;
ESPressio::Web::HttpServer server(platform);
ESPressio::Web::Router router;
ESPressio::Web::HttpApplication application(&router);
RootHandler root;

} // namespace

void setup() {
    ESPressio::Web::RouteHandle route;
    (void)router.RegisterRoute(
        ESPressio::Web::HttpMethod::Get,
        "/",
        root,
        route
    );
    (void)server.SetRequestHandler(&application);

    ESPressio::Web::HttpServerConfiguration configuration;
    configuration.Port = 80;
    (void)server.Initialize(configuration);
}

void loop() {}
