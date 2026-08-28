# ESPressio Web

Platform-neutral Web services for the ESPressio Development Platform.

ESPressio-Web owns Web-domain vocabulary and behaviour: HTTP request/response semantics, routing, middleware, static-resource resolution, WebSocket lifecycle and sessions, DNS service abstractions, capability reporting, and optional adapters for ESPressio primitives. Target-specific implementations belong in ESPressio-ESP32 or equivalent architecture packages.

## Architectural rules

- Applications always choose every published HTTP/WebSocket route. ESPressio never imposes application URL conventions.
- HTTP request contexts are borrowed and synchronous; a handler completes its response before returning.
- Request metadata uses lazy lookup where practical to avoid unnecessary heap pressure.
- Request bodies and responses are streaming-oriented and bounded.
- Known-length and unknown-length responses are distinct platform concerns; Web does not require full-body buffering merely to determine framing.
- Route registration and removal are supported at runtime. User handlers and middleware are never invoked while Web mutation locks are held.
- Unmatched HTTP routes may fall through to a configured static-resource provider.
- Filesystem access is owned by ESPressio-Persistence; Web owns only URI/resource semantics.
- Observable supplies synchronous lifecycle/state observation; optional bridges/adapters layer Event, State, Command and other primitives above Web.
- Providers/services may inspect the full request, including `Content-Type` and `Accept`, before consuming or producing representations.
- WebSocket protocol ownership resides in ESPressio-Web; ESPressio-Sockets remains responsible for non-Web generic socket technologies and transport-neutral protocol machinery.
- WebSocket endpoint paths and optional subprotocols are application-owned. Concrete providers translate those bindings into their native HTTP-upgrade facilities.
- Durable WebSocket connections may outlive the HTTP handshake callback. Concrete providers must therefore own any native connection state and asynchronous-send payloads for as long as the target API requires them.
- A secure Web transport must not silently degrade into unauthenticated TLS. Portable trust/client-credential semantics are tracked separately before the WebSocket client or HTTPS providers are considered complete.

## ESP32 concrete providers

The current ESPressio-ESP32 working branch provides optional concrete implementations for the Web abstractions when ESPressio-Web is present:

- HTTP server/request/response translation using ESP-IDF `esp_http_server`;
- bounded DNS parsing/serialization over the native UDP primitive while wildcard/captive-portal policy remains Web-owned;
- server-side WebSocket endpoints backed by `esp_http_server`, including application-selected path/subprotocol binding, fragmented-message reassembly, ping/pong, close handling and durable asynchronous send ownership.

ESPressio-Web is not a mandatory dependency of ESPressio-ESP32. The combined provider validation lane lives in this repository so the general ESP32 package remains usable without Web.

The ESP32 WebSocket client is intentionally separate from the server provider. Espressif supplies the maintained client as the optional managed `esp_websocket_client` component rather than an ESP-IDF 4.4 core facility. ESPressio-ESP32 therefore does not add it to its core package dependency set. Portable transport-security/client-policy work is tracked before that concrete client is enabled.

## Examples

Examples are intentionally split by audience.

### Application developers

`examples/ESP32/` shows how an application consumes an already-onboarded architecture. `HttpAndWebSocket/main.cpp` uses ESPressio-Web plus ESPressio-ESP32, publishes an application-owned HTTP route and WebSocket endpoint, and requires no ESP-IDF request/response plumbing in application code.

The ESP32 consumer example is compiled by the pinned PlatformIO provider-integration workflow so it cannot silently drift behind the concrete APIs.

### Platform implementors

`examples/Implementors/` shows how to satisfy ESPressio-Web platform contracts for a new architecture. `MinimalHttpPlatform/main.cpp` demonstrates the intended translation boundary:

1. the native server receives a request;
2. short-lived native request/response adapters satisfy `IHttpRequestPlatform` and `IHttpResponsePlatform`;
3. the architecture provider synchronously calls the Web-owned dispatcher;
4. the Web application routes/handles the request without seeing native target types.

The example deliberately keeps header lookup lazy and body/response I/O streaming. It is a pedagogical porting example, not a production backend, and is compiled/executed by host CI under the same warning-as-error policy as the core suite.

## Development

The active tranche is documented chronologically in `ESPRESSIO_WEB_CORE_DEVELOPMENT.md`. The canonical Web working branch for this tranche is `work/web-core-tranche`; target implementation work remains on the corresponding architecture-package working branch.
