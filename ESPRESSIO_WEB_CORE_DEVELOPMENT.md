# ESPressio Web Core Development Tranche

This is the authoritative continuation record for the current ESPressio-Web tranche. Read this file before modifying Web or its concrete ESP32 providers.

## Operating rules

1. **ESPressio-Web has exactly one Working Branch for this tranche: `work/web-core-tranche`.** Do not create issue-specific or phase-specific Web branches.
2. Historical Web refs `feature/10-web-foundation`, `feature/11-websocket-migration`, `feature/12-web-core`, and `tmp-ignore` are redundant. They contain no unique work and must receive no further commits.
3. ESPressio-ESP32 work for this tranche stays on its existing `feature/1-system-memory-provider` branch.
4. Domain libraries own vocabulary, abstractions, lifecycle/state and behavior. ESPressio-ESP32 owns ESP32/Arduino/ESP-IDF concrete implementation and native type/error translation.
5. Application-visible routes are always application-owned. No Web integration or platform provider may impose a URL.
6. User callbacks/Observers must never execute while an ESPressio-Web internal mutex is held.
7. Prefer borrowed views, bounded streaming, external-preferred memory, and mutation-time snapshots rather than per-request copying.
8. Optional integrations remain optional. ESPressio-ESP32 must not acquire a mandatory dependency on ESPressio-Web merely because it contains a Web provider.
9. Backwards compatibility is not a tranche goal. Update consumers to the current APIs rather than adding compatibility shims.
10. Do not change release version numbers during this development tranche.

## Current Working Branches used by Web CI

- ESPressio-Web: `work/web-core-tranche`
- ESPressio-ESP32: `feature/1-system-memory-provider`
- ESPressio-System: `feature/1-system-memory-policy`
- ESPressio-Observable: `feature/16-rtti-free-observer-registry`
- ESPressio-Persistence: `feature/10-platform-storage-abstractions`
- ESPressio-State: `feature/1-state-foundation`
- ESPressio-Event: `feature/57-rtti-free-memory-efficiency`
- ESPressio-Command: `feature/30-async-command-routing`
- ESPressio-Threads: `optimisation/69-resource-footprint`
- ESPressio-Task: `feature/1-task-execution`
- ESPressio-Timing: `feature/29-platform-clock-abstractions`
- ESPressio-Units: `main` (read-only unless a new working branch is created)
- ESPressio-Serializable: `optimisation/25-psram-buffers`
- ESPressio-WiFi: `feature/20-wifi-off-mode`
- ESPressio-Sockets: `feature/state-transport-major-release`

## Architecture already settled

- Platform-neutral Web API; no ESP-IDF/Arduino/lwIP types in ESPressio-Web public contracts.
- Borrowed synchronous HTTP request contexts; request/response objects cannot be retained for deferred completion.
- Lazy header access; path/query borrowed as views; request and response bodies stream through bounded APIs.
- Long-running operations acknowledge synchronously (normally `202 Accepted`) and continue through owner-library async facilities.
- Runtime route mutation is supported and thread-safe; exact routes outrank named-parameter routes.
- Middleware is ordered, short-circuitable and one-shot; each continuation may be invoked only once.
- Static resources are Web-domain semantics; storage remains Persistence ownership through an optional adapter.
- DNS request/response/lifecycle is Web-owned. Wildcard DNS is Web behavior, not a platform capability.
- HTTP representation is neutral. Providers may inspect the full request (`Content-Type`, `Accept`, etc.) before selecting a representation.
- WebSocket is Web ownership. ESPressio-Sockets no longer owns WebSocket transport APIs.
- Server-side WebSocket paths are explicitly application-owned through `WebSocketEndpointConfiguration`; a platform package may coordinate its own HTTP/WebSocket concrete implementations internally but may not expose native handles through Web.
- SSE, regex routing, multipart streaming, compression, ranges, conditional/cache policy and HTTP/2 remain deferred issues.

## Active Issues

- Web #10 repository foundation
- Web #11 WebSocket ownership migration
- Web #12 umbrella tranche
- Web #13 Sockets WebSocket consumer audit
- Web #14 application-owned routes / MIME-aware composition
- Web #15 single Working Branch consolidation
- Web #16 HTTP vocabulary/request/response/server lifecycle
- Web #17 runtime routing and middleware
- Web #18 static resources and configurable errors
- Web #19 DNS abstractions/lifecycle
- Web #20 State/Command/Event HTTP adapters
- Web #21 request-aware service/provider composition
- Web #22 application-owned WebSocket endpoint binding / upgrade lifecycle
- ESP32 #6 ESP32 HTTP/WebSocket/DNS concrete implementations

## Completed Web core work on `work/web-core-tranche`

### WebSocket ownership (#11/#13)

Web owns endpoint/client abstractions, Event-over-WebSocket and WebSocket clock synchronization. Sockets was cleaned of WebSocket ownership while retaining reusable transport-neutral clock synchronization protocol machinery. Obsolete Links2004 WebSocket Event transports were removed from ESPressio-ESP32.

### HTTP foundation (#16)

Implemented `ESPressio_Http.hpp` and `ESPressio_HttpServer.hpp` with standard method/status vocabulary, borrowed path/query request views, lazy caller-buffer header reads, streamed request bodies, response state, lifecycle/capability validation and explicit handled/not-handled semantics. Observable lifecycle notifications occur outside locks.

### Router and middleware (#17)

Implemented thread-safe exact/named routing with fixed-capacity borrowed parameter views, route handles/removal and deterministic precedence. Route-table locks are released before application handlers execute. Middleware uses immutable mutation-time snapshots and one-shot continuations.

### Resources/errors (#18)

Implemented generic `IWebResourceProvider`, bounded streaming responses, MIME resolution, traversal rejection, GET/HEAD static resource handling and Web-owned application/error composition. Optional `ESPressio_WebPersistence.hpp` adapts Persistence without making it a core Web dependency.

### DNS (#19)

Implemented platform-neutral DNS question/address/response vocabulary, synchronous request context, lifecycle facade and Web-owned `WildcardDnsHandler`. Unhandled questions produce NXDOMAIN.

### Request-aware composition (#21/#14)

Implemented a stable-snapshot provider registry/service surface. Providers inspect the live `HttpRequest` and choose themselves using request metadata without the HTTP core knowing JSON, CBOR, Serializable or other representations.

### State / Command / Event adapters (#20)

Implemented optional HTTP adapters without registering URLs. Event and Command ingress return `202 Accepted`; State GET/HEAD snapshots preserve State metadata and work with State introspection disabled. The live owner-library integration matrix, including successful Command -> Event queuing, is green.

### WebSocket endpoint binding (#22)

The original WebSocket endpoint abstraction did not expose a server-side application-owned upgrade path. This would have forced an ESP32 concrete provider either to hard-code a URI or bypass Web ownership. Issue #22 records and resolves that architectural gap.

`WebSocketEndpointConfiguration` now carries application-selected `Path` and optional `Protocol`. `IWebSocketEndpointPlatform` exposes `Bind`, `Unbind` and `IsBound`; `WebSocketEndpoint` validates absolute paths and owns the portable bind lifecycle. Host fakes/tests verify application path/protocol propagation, bind state, unbind and existing observer/broadcast behavior. Final host WebSocket binding test run `33161878768` succeeded.

`PLATFORM_ABSTRACTIONS.md` now documents durable WebSocket connection lifetime, payload ownership for asynchronous sends, disconnect semantics and pre-start binding requirements for target stacks whose URI precedence is fixed at server start.

## ESP32 HTTP provider status (#16 / ESP32 #6)

Implementation is on ESPressio-ESP32 `feature/1-system-memory-provider` in `src/ESPressio_HttpServerPlatform.hpp`.

Concrete HTTP request/response/server translation uses ESP-IDF `esp_http_server`. Request URI/query are borrowed, headers are lazy, bodies use `httpd_req_recv`, and response metadata is adapter-owned until commit.

Pinned Arduino-ESP32/IDF compatibility was corrected in `be9848f6ac4b84172b17d3f95ea9345b66ebfe87`: `httpd_req_t::method` conversion, absence of runtime `max_req_hdr_len`, absence of `HTTP_ANY`, and empty/non-empty header presence behavior.

Known-length streaming and HEAD semantics were hardened in `0e4efb13d29b542f9c2e9c5fa042d90370e1b0d4`. Unknown-length responses use IDF chunked transfer; known-length responses construct explicit HTTP/1.1 framing and stream through `httpd_send()` with `Content-Length`, avoiding contradictory chunked/content-length headers and avoiding full-body buffering. HEAD consumes/counts the application body path but suppresses wire body bytes. `Content-Length`, `Transfer-Encoding` and `Connection` are provider-owned framing headers. Private pinned-toolchain integration run `33161700071` succeeded.

ESPressio-Web remains optional for ESPressio-ESP32. Real combined validation is owned by Web workflow `.github/workflows/esp32-provider.yml`; public ESP32 package metadata does not mandate the Web repository.

## ESP32 DNS provider status (#19 / ESP32 #6)

`src/ESPressio_DnsServerPlatform.hpp` implements the DNS primitive over Arduino-ESP32 `AsyncUDP`; Arduino `DNSServer` is intentionally not used because it owns domain matching/policy. The provider performs bounded DNS wire parsing/serialization and exposes one-question A/AAAA address response mechanics while Web's `WildcardDnsHandler` retains captive-portal policy ownership. Combined HTTP/DNS pinned-toolchain integration run `33160458979` succeeded.

## ESP32 WebSocket server status (#22 / ESP32 #6)

ESP-IDF 4.4 handler registration is insertion-ordered, and an already-registered wildcard GET handler prevents later exact WebSocket registration. The concrete HTTP provider therefore now owns an internal durable WebSocket binding registry and registers active exact WebSocket handlers **before** its generic `/*` HTTP handlers. ESP32 commit `fef4ce6e0e6f9db06777bd180ac69f26b67d3dba` adds that registry and session-close fan-out.

`src/ESPressio_WebSocketEndpointPlatform.hpp` was added in ESP32 commit `6a927882c483a0a757b81389bdaa572a5a135f98`, and conditional umbrella exposure followed in `696dc2782a9e3b84a240314426097eb990dabe40`.

The concrete endpoint currently provides:

- durable per-socket `IWebSocketConnection` objects rather than retaining borrowed HTTP requests;
- application path and optional subprotocol binding;
- exact-handler registration before generic HTTP routing;
- text/binary receive and broadcast;
- bounded fragmented-message reassembly using external-preferred buffers;
- ping/pong and close control-frame handling;
- close codes/reasons and `httpd_sess_trigger_close()` session teardown;
- `httpd_config_t.close_fn` disconnect detection covering peer/network/server session termination;
- owned outbound payload operations retained until IDF asynchronous-send completion, avoiding caller-buffer lifetime bugs and HTTP-task self-deadlock.

Web compile smoke commit `c3e0a87d6c9323dc7e2972a4f569d20fdd8964f6` instantiated the endpoint conditionally and its provider integration run `33162454811` succeeded. A stronger smoke commit `253502ebec3e517ab0987163ced93a50eb0ee9e1` removes that conditional from the test so the pinned SDK must actually expose WebSocket server support; its provider run `33162559309` is still pending at this checkpoint.

## CI checkpoint

- Current Web host suite is green through the WebSocket binding contract.
- HTTP known-length/HEAD pinned ESP32 build: green (`33161700071`).
- HTTP + DNS pinned ESP32 build: green (`33160458979`).
- Initial concrete WebSocket endpoint compile with conditional SDK exposure: green (`33162454811`).
- Forced pinned-SDK WebSocket support build: pending run `33162559309`.

## Next work

1. Resolve forced WebSocket provider run `33162559309`; fix any pinned Arduino-ESP32/IDF mismatch directly on ESP32 `feature/1-system-memory-provider`.
2. Complete the server-side WebSocket concurrency/lifetime audit, especially unbind/rebind while the HTTP server is running, and add any missing provider-level tests that can be expressed without hardware.
3. Determine the ESP32 WebSocket **client** concrete strategy. ESP-IDF 4.4 does not include a core client API; Espressif's client is a separate `esp_websocket_client` managed component. Do not reintroduce Links2004 or make a package dependency decision without preserving Web ownership and ESP32 optionality.
4. Add `examples/Implementors/` and `examples/ESP32/` examples showing the two distinct extension surfaces.
5. Expand README usage/architecture documentation, perform final memory/threading audit, and only then prepare tranche PR/release work.
