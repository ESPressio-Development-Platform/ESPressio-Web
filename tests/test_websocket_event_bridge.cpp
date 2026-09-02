#include <cassert>

#include <ESPressio_WebSocketEventBridge.hpp>

int main() {
    ESPressio::Event::WebSocketEventBridge bridge;
    assert(!bridge.IsInitialized());

    ESPressio::Web::WebSocketActivity activity;
    activity.Kind = ESPressio::Web::WebSocketActivityKind::UpgradeRequested;
    activity.ConnectionId = 42;
    activity.Detail = "compile-test";

    ESPressio::Event::WebSocketActivityEvent event(false, activity);
    assert(!event.ClientSide);
    assert(event.ConnectionId == 42);
    assert(event.Detail == "compile-test");
    return 0;
}
