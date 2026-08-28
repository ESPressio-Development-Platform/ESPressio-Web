# ESPressio Web Core Development Tranche

This is the authoritative continuation record for the current ESPressio-Web tranche. Read this file before modifying Web or its concrete ESP32 providers.

## Operating rules

1. **ESPressio-Web has exactly one Working Branch for this tranche: `work/web-core-tranche`.** Do not create issue-specific or phase-specific Web branches.
2. Historical Web refs `feature/10-web-foundation`, `feature/11-websocket-migration`, `feature/12-web-core`, and `tmp-ignore` are redundant. They contain no unique work and must receive no further commits.
3. ESPressio-ESP32 work for this tranche stays on its existing `feature/1-system-memory-provider` branch.
4. Domain libraries own vocabulary, abstractions, lifecycle/state and behavior. ESPressio-ESP32 owns ESP32/Arduino/ESP-IDF concrete implementation and native type/error translation.
5. Application-visible HTTP/WebSocket routes are always application-owned. No integration or platform provider may impose a URL.
6. User callbacks/Observers must never execute while an ESPressio-Web internal mutex is held.
7. Prefer borrowed views, bounded streaming, external-preferred memory, mutation-time snapshots and moves rather than per-request copying.
8. Optional integrations remain optional. ESPressio-ESP32 must not acquire a mandatory dependency on ESPressio-Web merely because it contains Web providers.
9. Backwards compatibility is not a tranche goal. Update consumers to current APIs rather than adding compatibility shims.
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
- ESPressio-Units: `main`
- ESPressio-Serializable: `optimisation/25-psram-buffers`
- ESPressio-WiFi: `feature/20-wifi-off-mode`
- ESPressio-Sockets: `feature/state-transport-major-release`

## Active issues

### ESPressio-Web

- #10 repository foundation
- #11 WebSocket ownership migration
- #12 umbrella tranche
- #13 Sockets WebSocket consumer audit
- #14 application-owned routes / MIME-aware composition
- #15 single Working Branch consolidation
- #16 HTTP vocabulary/request/response/server lifecycle
- #17 runtime routing and middleware
- #18 static resources and configurable errors
- #19 DNS abstractions/lifecycle
- #20 State/Command/Event HTTP adapters
- #21 request-aware service/provider composition
- #22 application-owned WebSocket endpoint binding / upgrade lifecycle
- #23 WebSocket client transport-security and connection-policy configuration

### ESPressio-ESP32

- #6 ESP32 HTTP/WebSocket/DNS concrete providers
- #7 optional maintained Espressif WebSocket client provider

## Architecture settled in this tranche

- ESPressio-Web public contracts are platform-neutral; ESP-IDF/Arduino/lwIP/native handles do not cross the boundary.
- HTTP request/response contexts are borrowed and synchronous. Deferred work uses owner-library async primitives and normally acknowledges with `202 Accepted`.
- Paths/query strings are borrowed where possible; headers are lazy caller-buffer lookups; bodies are bounded streaming APIs.
- Known-length responses may use fixed framing without whole-body buffering; unknown-length responses remain streaming/chunked provider concerns.
- Runtime route mutation is supported and synchronized. Exact routes outrank named-parameter routes. Selected entries retain stable lifetime while user callbacks execute outside locks.
- Middleware and HTTP service providers use mutation-time immutable snapshots rather than copying their lists per request.
- Static-resource semantics are Web-owned while filesystem/storage access remains Persistence-owned.
- DNS vocabulary/lifecycle/policy is Web-owned. ESP32 supplies only DNS/UDP mechanics; wildcard captive-portal policy remains `WildcardDnsHandler` behavior.
- WebSocket protocol ownership is ESPressio-Web. ESPressio-Sockets no longer owns WebSocket transports.
- Server WebSocket endpoint path/subprotocol binding is application-owned. Native HTTP/WebSocket provider coordination may stay private inside an architecture package.
- Durable WebSocket connections outlive the HTTP upgrade request; inbound callback payloads are synchronous borrowed views over provider-owned buffers; async outbound payloads must remain provider-owned until native completion.
- Leaf handler/responder configuration is setup-time unless a type explicitly documents runtime mutation. Router/middleware/service mutation are the synchronized runtime composition surfaces.
- `Secure=true` must never silently map to unauthenticated TLS. Full portable WebSocket-client trust/credential/policy semantics are deliberately blocked on Web #23.
- SSE, regex routing, multipart streaming, compression, ranges, conditional/cache policy and HTTP/2 remain deferred work.

## Completed portable Web core

### HTTP foundation (#16)

`ESPressio_Http.hpp` / `ESPressio_HttpServer.hpp` provide standard method/status/header vocabulary, borrowed request metadata, lazy headers, streamed bodies, response state/framing contracts, lifecycle/capability validation and explicit handled/not-handled semantics. Observable lifecycle notifications execute outside server locks.

### Router and middleware (#17)

Router uses external-preferred stable route entries, fixed-capacity borrowed route parameters, handles/removal and deterministic precedence. A shared lock is held only while selecting an entry; the handler executes after release.

Middleware uses an immutable `Chain` snapshot rebuilt on mutation. Requests retain one shared snapshot; middleware/terminal callbacks execute without the mutation mutex. Continuations are one-shot and host-tested against double invocation and self-removal.

### Resources/errors (#18)

Generic `IWebResourceProvider`, MIME resolution, traversal rejection and GET/HEAD static-resource serving are implemented. Resource bodies stream through one reusable external-preferred chunk buffer. Optional `ESPressio_WebPersistence.hpp` adapts Persistence without making it a core dependency.

Custom leaf resource/error responder configuration is setup-time. Runtime request paths do not add locks/copies around immutable mappings.

### DNS (#19)

Platform-neutral DNS question/address/response vocabulary, lifecycle facade and `WildcardDnsHandler` are implemented. Unhandled requests become NXDOMAIN. Wildcard address/TTL are construction-time immutable state.

### Request-aware service/provider composition (#21/#14)

`HttpService` uses a stable provider-set snapshot. Providers inspect the live request (`Content-Type`, `Accept`, etc.) without the core learning JSON/CBOR/Serializable representations. Provider selection/handling runs outside the mutation mutex.

### State / Command / Event adapters (#20)

Optional adapters register no URLs themselves.

- Event POST ingress delivers one bounded Event transport packet and returns `202 Accepted` after receiver handoff.
- Command POST ingress builds the current Command envelope, queues `InboundCommandEvent` through the owner-library Event path and returns `202 Accepted`; it does not synchronously invoke CommandRegistry.
- State GET/HEAD snapshots preserve State TypeId/Epoch/Revision metadata and use State-owned `StateCodec<TDefinition>`.

Final memory pass changes:

- Event declared-length bodies now read directly into their final external-preferred packet buffer; unknown-length bodies grow/read directly into the final buffer. The old chunk-to-packet staging copy is gone (`2df6fcbf...`, explicit include follow-up `ffea8db5...`).
- Command ingress uses the same direct-read strategy and removes its staging copy (`79df5c4f...`, explicit include follow-up `50dbb237...`). Host and ESP32 integration runs for `50dbb237...` both succeeded (`33163816790`, `33163816789`).
- State codec scratch storage moved from a `MaximumEncodedSize` request-stack array to an external-preferred buffer (`8661a5bc...`) with a defensive encoded-size check.

### WebSocket endpoint binding (#22)

`WebSocketEndpointConfiguration` carries application-selected `Path` and optional `Protocol`; `IWebSocketEndpointPlatform` exposes Bind/Unbind/IsBound. Host tests validate binding lifecycle, application path/protocol propagation, observer behavior and broadcasts. Earlier binding contract run `33161878768` is green.

`PLATFORM_ABSTRACTIONS.md` defines durable connection lifetime, synchronous borrowed inbound payloads, async outbound ownership, pre-start binding requirements and safe unbind behavior for native stacks with fixed handler precedence.

## ESP32 HTTP provider (#16 / ESP32 #6)

Implementation is `src/ESPressio_HttpServerPlatform.hpp` on ESP32 `feature/1-system-memory-provider`.

- Uses ESP-IDF `esp_http_server`; Web routing remains portable.
- Request URI/query are borrowed; headers are lazy; bodies use `httpd_req_recv`.
- Pinned-IDF compatibility fixes are in `be9848f6ac4b84172b17d3f95ea9345b66ebfe87`.
- Known-length responses construct explicit HTTP/1.1 framing and stream through `httpd_send()`; unknown-length responses use native chunked send. HEAD counts/validates application body bytes but suppresses them on the wire (`0e4efb13d29b542f9c2e9c5fa042d90370e1b0d4`).
- Framing headers (`Content-Length`, `Transfer-Encoding`, `Connection`) are provider-owned to prevent contradictory wire state.
- HTTP native start/register/stop work is no longer executed while the provider mutex is held; `_starting` blocks concurrent lifecycle/binding mutation (`0e572b19824856ae57042894e61c2af330e2a605`).

## ESP32 DNS provider (#19 / ESP32 #6)

`src/ESPressio_DnsServerPlatform.hpp` uses Arduino-ESP32 `AsyncUDP` only as the UDP primitive. Arduino `DNSServer` is intentionally not used because its wildcard/policy behavior belongs to Web. Bounded DNS parse/serialize supports the required A/AAAA path. HTTP+DNS pinned integration run `33160458979` is green.

## ESP32 WebSocket server (#22 / ESP32 #6)

ESP-IDF 4.4 URI lookup is insertion ordered and an existing wildcard GET handler blocks later exact WebSocket registration. The HTTP provider therefore retains active WebSocket bindings and installs exact upgrade handlers before generic `/*` handlers (`fef4ce6e...`).

`ESP32WebSocketEndpointPlatform` (`6a927882...`, umbrella exposure `696dc278...`) provides:

- durable per-socket `IWebSocketConnection` objects;
- application path and optional subprotocol binding;
- text/binary receive and broadcast;
- bounded fragmented-message reassembly in external-preferred memory;
- ping/pong and close control handling;
- `httpd_sess_trigger_close()` teardown and `close_fn` disconnect detection;
- async outbound send operations owning payload bytes until IDF completion;
- fresh binding-state rotation on unbind, so a retained stale native `user_ctx` can never be reactivated (`dcd9d75d...`);
- receive-buffer moves for complete frames and first fragments, removing the common-case second allocation/copy; only continuation-fragment append remains (`da0555b7374ff79deaa9dd5581b8919b7cbf6e30`).

Validation checkpoints:

- initial concrete WS provider: `33162454811` green;
- forced pinned-SDK WebSocket support (no preprocessor skip): `33162559309` green;
- hardened HTTP/DNS/WS start/bind/unbind/rebind paths: `33163078689` green.

## WebSocket client decision (#23 / ESP32 #7)

The pinned Arduino-ESP32 / ESP-IDF 4.4 stack does not provide the maintained client as a core facility. Espressif now distributes `esp_websocket_client` separately as a managed component. ESP32 #7 therefore keeps the client optional and explicitly forbids adding it to the core ESPressio-ESP32 `library.json` dependency set.

The current portable `WebSocketClientConfiguration` is not yet sufficient for a trustworthy concrete implementation: it lacks server trust policy/material, client credentials, structured handshake headers and connection/reconnect/keepalive policy. Native `wss` must not be enabled in a mode that skips server authentication merely because `Secure=true` was supplied. Web #23 owns the portable contract correction before the ESP32 client is considered complete.

Current ESPressio-Security `ITransportSecurityCarrier` protects application payloads and is not a TLS trust-store/client-certificate abstraction. Do not conflate those layers.

## Examples and documentation

Examples are split by audience as required:

- `examples/Implementors/MinimalHttpPlatform/main.cpp` demonstrates a pedagogical native request/response/server implementation and the synchronous Web dispatcher boundary. It is now a host-CI executable under `-Wall -Wextra -Wpedantic -Werror`; run `33163484385` succeeded.
- `examples/ESP32/HttpAndWebSocket/main.cpp` demonstrates application consumption of the concrete ESP32 HTTP/WebSocket providers with application-selected routes. The provider workflow compiles the actual example; run `33163268936` succeeded.

README and `PLATFORM_ABSTRACTIONS.md` now document the two extension surfaces, provider ownership, response framing, WebSocket lifetime/move semantics, setup-time leaf configuration and the secure-client blocker.

## Memory/threading audit status

Audited portable components:

- Router: external-preferred route entries/vector; no per-request route-list copy; callbacks outside shared lock.
- Middleware: mutation-time external-preferred immutable snapshots; one shared_ptr acquisition per request; callbacks outside lock.
- HttpService: mutation-time provider snapshots; callbacks outside lock.
- Resources: one reusable external-preferred stream buffer; no full-resource body copy.
- Event/Command ingress: direct final-buffer reads as described above.
- State representation: codec scratch moved off request stack.
- HTTP/DNS lifecycle: state snapshot under locks, platform/user callbacks outside locks.

Audited ESP32 WebSocket provider:

- inbound receive/frame buffers external-preferred;
- common complete-frame and first-fragment copy removed via moves;
- continuation fragments append only as required to create one completed contiguous payload;
- outbound payload copy retained intentionally because IDF asynchronous send requires provider-owned lifetime;
- connection-list snapshots are mutation/lifecycle safety snapshots for broadcasts/close operations, not per-frame payload copies.

## CI checkpoint

Known-green checkpoints:

- host example/core suite including Implementor example: `33163484385`;
- Event/Command direct-read integration state: host `33163816790`, ESP32 `33163816789`;
- ESP32 consumer example: `33163268936`;
- hardened server start path: `33163078689`;
- forced pinned WebSocket support: `33162559309`;
- known-length/HEAD HTTP: `33161700071`;
- HTTP+DNS: `33160458979`.

The latest State-buffer/doc commits and ESP32 `da0555b7...` receive optimization trigger fresh current-head host/provider validation; check those before opening the tranche PR.

## Next work

1. Confirm the current-head host suite and combined pinned ESP32 provider lane are green after `8661a5bc...`, `e39ad5af...`, this checkpoint commit, and ESP32 `da0555b7...`.
2. Finish the remaining lightweight API/lifetime audit; do not add compatibility shims or speculative features.
3. Keep ESP32 WebSocket client implementation blocked on the portable Web #23 security/policy design; do not weaken TLS semantics simply to mark #7 complete.
4. Once current-head validation is green, prepare the tranche PR/release documentation and issue closure sequence. Do not change version numbers until the release step is explicitly started.
