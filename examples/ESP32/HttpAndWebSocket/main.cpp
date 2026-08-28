#include <Arduino.h>

#include <ESPressio_Web.hpp>
#include <ESPressio_ESP32.hpp>

using namespace ESPressio::Web;

namespace {

class RootHandler final : public IHttpRouteHandler {
public:
    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters&
    ) override {
        auto result = context.Response().Header(HttpHeaderName::ContentType, "text/plain");
        if (!result) return HttpHandlerResult::Handled(result);
        return HttpHandlerResult::Handled(
            context.Response().Send("Hello from ESPressio-Web on ESP32\n")
        );
    }
};

class SocketObserver final : public IWebSocketEndpointObserver {
public:
    void OnWebSocketConnected(IWebSocketConnection& connection) override {
        Serial.printf("WebSocket connected: %lu\n", static_cast<unsigned long>(connection.Id()));
    }

    void OnWebSocketText(
        IWebSocketConnection& connection,
        std::string_view text
    ) override {
        Serial.printf(
            "WebSocket text from %lu: %.*s\n",
            static_cast<unsigned long>(connection.Id()),
            static_cast<int>(text.size()),
            text.data()
        );

        // Send() is safe here even though this callback originates from the
        // ESP-IDF HTTP task: the ESP32 provider owns the asynchronous payload
        // until IDF has finished transmitting it.
        (void)connection.SendText(text);
    }

    void OnWebSocketDisconnected(
        WebSocketConnectionId id,
        const WebSocketCloseReason& reason
    ) override {
        Serial.printf(
            "WebSocket disconnected: %lu code=%u\n",
            static_cast<unsigned long>(id),
            static_cast<unsigned>(reason.Code)
        );
    }
};

ESP32HttpServerPlatform httpPlatform;
HttpServer httpServer(httpPlatform);
Router router;
HttpApplication application(&router);
RootHandler rootHandler;

ESP32WebSocketEndpointPlatform socketPlatform(httpPlatform);
WebSocketEndpoint socketEndpoint(socketPlatform);
SocketObserver socketObserver;
Observable::ObserverHandlePtr socketObserverHandle;

} // namespace

void setup() {
    Serial.begin(115200);

    // Network connectivity is intentionally not managed here. Bring WiFi up
    // through ESPressio-WiFi (or another application-owned network policy)
    // before starting this server.

    RouteHandle rootRoute;
    (void)router.RegisterRoute(HttpMethod::Get, "/", rootHandler, rootRoute);

    WebSocketEndpointConfiguration socketConfiguration;
    socketConfiguration.Path = "/live";       // chosen by the application
    socketConfiguration.Protocol = "demo.v1";
    (void)socketEndpoint.Bind(socketConfiguration);
    socketObserverHandle = socketEndpoint.RegisterObserver(&socketObserver);

    (void)httpServer.SetRequestHandler(&application);

    HttpServerConfiguration configuration;
    configuration.Port = 80;
    configuration.MaximumConnections = 6;

    const auto initialized = httpServer.Initialize(configuration);
    if (!initialized) {
        Serial.println("HTTP initialization failed");
        return;
    }

    const auto started = httpServer.Start();
    Serial.println(started ? "HTTP/WebSocket server started" : "HTTP server start failed");
}

void loop() {
    // No HTTP/WebSocket polling loop is required. ESP-IDF owns transport I/O;
    // ESPressio-Web dispatches requests and socket events through callbacks.
}
