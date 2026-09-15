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
- A secure Web transport must not silently degrade into unauthenticated TLS. `WebTransportMode::Tls` requires an authenticated platform trust source or explicit caller-supplied CA/certificate material; optional client certificate/private-key identity is represented separately.
- WebSocket client handshake headers are structured and bounded. Protocol-owned headers (`Host`, `Connection`, `Upgrade`, and `Sec-WebSocket-*`) cannot be overridden through the application extra-header surface.

## Primitive redesign integration

The `primitives_redesign` branch consumes the final Primitive-family contracts through optional adapters. Web remains a tooling/protocol surface; it does not become an owner of Primitive, Command, Event, State, Timing or transport semantics.

Dynamic tooling follows these rules:

- discovery begins from a frozen `Primitive::TypeDirectoryView`; discovery does not create a second mutable registry;
- schema and constructibility information come from the final family descriptors and Serializable metadata rather than ad-hoc JSON or text registries;
- a dynamically requested Command is constructed through its typed descriptor/factory and enters the normal Command admission/runtime path; Web never fabricates an Event as an execution shortcut;
- Event discovery/submission uses the final Event descriptor/runtime contracts, including the Event family admission and delivery policy;
- generic State tooling is read/inspect only. Owner-authoritative State can only be changed by its bound `StateOwner` path, never by a Web request merely because a descriptor is discoverable;
- TypeDirectory/schema availability is not authorization. Applications must supply the applicable authorization/policy decision before a dynamic operation is admitted;
- request bodies, type names, representations and constructed payloads are bounded before parsing/construction. Invalid schema/factory input produces an explicit protocol/tool failure; there is no unbounded heap fallback, raw reinterpretation or exception-driven retry loop;
- WebSocket Primitive traffic uses the Sockets/A2/family binding boundaries. Web owns WebSocket framing/session behaviour, not Primitive transport semantics;
- clock information exposed by Web uses the final Timing quality/evidence vocabulary; a synchronized-looking scalar timestamp is not treated as sufficient clock evidence by itself.

The focused `primitives-redesign-*` workflows compile and exercise discovery, schema-driven construction, Command, Event, State, Sockets/WebSocket and Timing integration against the corresponding `primitives_redesign` provider branches.

## ESP32 concrete providers

The current ESPressio-ESP32 working branch provides optional concrete implementations for the Web abstractions when ESPressio-Web is present:

- HTTP server/request/response translation using ESP-IDF `esp_http_server`;
- bounded DNS parsing/serialization over the native UDP primitive while wildcard/captive-portal policy remains Web-owned;
- server-side WebSocket endpoints backed by `esp_http_server`, including application-selected path/subprotocol binding, fragmented-message reassembly, ping/pong, close handling and durable asynchronous send ownership;
- WebSocket client support using the `esp_websocket_client` component bundled by the pinned Arduino-ESP32 2.0.17 / ESP-IDF 4.4 framework, including authenticated CA/global-trust TLS, optional mTLS client identity, bounded application handshake headers, reconnect/ping/pong/TCP-keepalive policy mapping, and bounded reassembly when one native frame is split across multiple DATA callbacks.

ESPressio-Web is not a mandatory dependency of ESPressio-ESP32. The combined provider validation lane lives in this repository so the general ESP32 package remains usable without Web.

The ESP32 WebSocket client remains capability-driven and adds no external managed-component dependency to core ESPressio-ESP32. Arduino-ESP32 2.0.17 already supplies the compatible IDF-4.4 client header and static library. Newer managed `esp_websocket_client` releases target ESP-IDF 5+ and are therefore a future/newer-toolchain compatibility path rather than a dependency of the pinned lane.

The pinned IDF 4.4 provider rejects portable policy values it cannot faithfully express instead of silently discarding them. In particular, custom network timeout/reconnect-delay values and reconnect-after-clean-close are unsupported on that native client; ping/pong values must be representable in whole seconds. `PlatformTrust` is accepted only when an authenticated global CA store has actually been installed; otherwise the provider returns `Unsupported` rather than using IDF's unauthenticated TLS default.

IDF 4.4's public WebSocket-client event API does not expose the RFC6455 FIN bit. Consequently this pinned client backend supports **non-fragmented inbound WebSocket messages**. It can reassemble one frame split by the native receive buffer across DATA callbacks, but it cannot faithfully reconstruct a message fragmented across multiple RFC6455 frames; continuation frames are not delivered. FIN-aware fragmented-message support belongs to the newer ESP-IDF 5+ compatibility path.

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

The legacy Web-core development history remains recorded in `ESPRESSIO_WEB_CORE_DEVELOPMENT.md`. The canonical branch for the Primitive Platform Redesign work described above is `primitives_redesign`; its focused workflows are the executable integration evidence for this tranche.
