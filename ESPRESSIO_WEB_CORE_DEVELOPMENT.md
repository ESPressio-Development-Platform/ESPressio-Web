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

## Tracked issues

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
- #7 optional framework-backed WebSocket client provider

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
- WebSocket client security is explicit: `WebTransportMode::Tls` must use authenticated platform trust or explicit CA/certificate material. A native TLS mode with verification disabled is never an implementation of portable secure transport.
- TLS trust/client identity is neutral Web transport vocabulary, not a repurposing of ESPressio-Security's application-payload transport protection.
- WebSocket client handshake headers are structured/bounded; protocol-owned WebSocket handshake headers cannot be overridden by the application extra-header surface.
- Portable client policy includes network timeout, reconnect behavior, ping/pong and TCP keepalive. Concrete providers must reject unsupported requested semantics rather than silently ignore them.
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

Memory-pass results:

- Event declared-length bodies read directly into their final external-preferred packet buffer; unknown-length bodies grow/read directly into the final buffer. The old chunk-to-packet staging copy is gone (`2df6fcbf...`, include follow-up `ffea8db5...`).
- Command ingress uses the same direct-read strategy (`79df5c4f...`, include follow-up `50dbb237...`). Host and ESP32 integration runs `33163816790` / `33163816789` succeeded.
- State codec scratch storage moved from a `MaximumEncodedSize` request-stack array to an external-preferred buffer (`8661a5bc...`) with a defensive encoded-size check.

### WebSocket endpoint binding (#22)

`WebSocketEndpointConfiguration` carries application-selected `Path` and optional `Protocol`; `IWebSocketEndpointPlatform` exposes Bind/Unbind/IsBound. Host tests validate binding lifecycle, application path/protocol propagation, observer behavior and broadcasts.

`PLATFORM_ABSTRACTIONS.md` defines durable connection lifetime, synchronous borrowed inbound payloads, async outbound ownership, pre-start binding requirements and safe unbind behavior for native stacks with fixed handler precedence.

### WebSocket client security/policy (#23)

The old host/port/path/subprotocol/secure-boolean client surface was replaced by explicit transport/security/policy vocabulary:

- `WebTransportMode` selects plain vs TLS.
- `WebTlsConfiguration` supports authenticated `PlatformTrust` or caller-provided `CertificateAuthority` material.
- `WebCredentialView` represents PEM/DER CA/client-certificate/private-key bytes without native handles.
- client certificate/private-key material is an optional paired identity.
- `IWebClientHeaderSource` provides structured application headers with syntax, CR/LF, count and aggregate-byte validation.
- `Host`, `Connection`, `Upgrade`, `Sec-WebSocket-Key`, `Sec-WebSocket-Version` and `Sec-WebSocket-Protocol` are reserved to the WebSocket handshake/provider.
- `WebSocketClientConnectionPolicy` covers network timeout, automatic reconnect, reconnect delay, reconnect-after-clean-close, ping/pong and optional TCP keepalive.
- the portable façade validates invalid/insecure combinations before calling a platform provider.

Implementation sequence includes `4a7df016...`, `80184d5c...`, `f0833e14...`, `cdc9c2c7...`, `5bfb50e3...`, `cd9df7ff...`, `11617bf7...` and `adb75df5...`. Host security/policy validation run `33164325249` succeeded. Additional regression coverage for reserved handshake headers, TCP keepalive invariants and portable clean-close-reconnect semantics was added in `test_websocket_policy.cpp` (`ef9827fb...`, registered by `fb1db9cd...`).

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

`ESP32WebSocketEndpointPlatform` provides:

- durable per-socket `IWebSocketConnection` objects;
- application path and optional subprotocol binding;
- text/binary receive and broadcast;
- bounded fragmented-message reassembly in external-preferred memory;
- ping/pong and close control handling;
- `httpd_sess_trigger_close()` teardown and `close_fn` disconnect detection;
- async outbound send operations owning payload bytes until IDF completion;
- fresh binding-state rotation on unbind, so a retained stale native `user_ctx` can never be reactivated (`dcd9d75d...`);
- receive-buffer moves for complete frames and first fragments, removing the common-case second allocation/copy; only continuation-fragment append remains (`da0555b7374ff79deaa9dd5581b8919b7cbf6e30`).

Important validation checkpoints include `33162454811`, `33162559309` and `33163078689`.

## ESP32 WebSocket client (#23 / ESP32 #7)

The dependency decision was corrected during implementation. Arduino-ESP32 **2.0.17 already bundles the ESP-IDF 4.4 `esp_websocket_client` header/component/static library**. Managed component releases 1.5.x-1.8.x require ESP-IDF >=5.0, so they are not dependencies of the pinned lane and must not be added to core ESPressio-ESP32 `library.json`.

`src/ESPressio_WebSocketClientPlatform.hpp` on ESP32 `feature/1-system-memory-provider` implements optional `IWebSocketClientPlatform` when both ESPressio-Web and `esp_websocket_client.h` are available. `ESPressio_ESP32.hpp` exposes it conditionally; ESPressio-Web remains optional for general ESP32 consumers.

Provider behavior:

- plain WS and authenticated WSS mapping;
- explicit CA/certificate trust mapping;
- `PlatformTrust` accepted only when `esp_tls_get_global_ca_store()` confirms an installed authenticated global CA store; otherwise `Unsupported`;
- retained external-preferred TLS credential buffers because the IDF 4.4 transport retains their pointers after client initialization;
- paired client certificate/private-key mTLS identity;
- bounded translation of structured application handshake headers to native CRLF storage;
- automatic reconnect, whole-second ping/pong and TCP keepalive mapping;
- explicit `Unsupported` for portable values IDF 4.4 cannot configure faithfully (custom network timeout, custom reconnect delay, reconnect-after-clean-close, non-whole-second ping/pong);
- direct borrowed callback delivery for one-DATA-event text/binary frames;
- bounded external-preferred reassembly when one native frame is split across multiple DATA callbacks;
- clean close/stop rejected from the native WebSocket event task because IDF 4.4 explicitly forbids that operation there;
- no external managed-component dependency and no Links2004 client ownership.

Implementation landed initially as `4b25557f...`, umbrella exposure `62e8650e...`, with GCC-8.4 nested-type/method name collision fixed in `b18416734b0f733445eb1fa2b00b36eb16ff1791`.

Pinned compile/link validation succeeded on rerun `33164971085`: both `Build ESP32 provider integration` and `Build ESP32 consumer example` passed against Arduino-ESP32 2.0.17, proving the bundled IDF 4.4 header and static library satisfy the concrete provider.

### IDF 4.4 inbound fragmentation limitation

IDF 4.4 public `esp_websocket_event_data_t` exposes `op_code`, `payload_len` and `payload_offset`, but no RFC6455 FIN flag. Its public `esp_transport_ws` API likewise exposes only the last opcode and payload length. Therefore the pinned client provider can faithfully reassemble one native frame split by its receive buffer, but **cannot distinguish a complete initial text/binary frame from the first non-final fragment of a multi-frame RFC6455 message**.

The pinned ESP32 client backend therefore supports **non-fragmented inbound WebSocket messages**. Continuation frames are not delivered. Applications using this backend must ensure the peer sends non-fragmented messages. FIN-aware fragmented-message support belongs to a future/newer-toolchain ESP-IDF 5+ client provider, whose event API exposes the required framing information.

Provider instances must not be destroyed from inside their own callback path when the native stack forbids synchronous stop/destroy from that event task; lifetime must cover callback completion.

## Examples and documentation

Examples remain split by audience:

- `examples/Implementors/MinimalHttpPlatform/main.cpp` demonstrates a pedagogical native request/response/server implementation and the synchronous Web dispatcher boundary. It is a host-CI executable under `-Wall -Wextra -Wpedantic -Werror`; run `33163484385` succeeded.
- `examples/ESP32/HttpAndWebSocket/main.cpp` demonstrates application consumption of the concrete ESP32 HTTP/WebSocket server providers with application-selected routes. Provider CI compiles the real example.

README and `PLATFORM_ABSTRACTIONS.md` now document the two extension surfaces, provider ownership, response framing, WebSocket lifetime/move semantics, setup-time leaf configuration, explicit client TLS trust/identity/policy semantics, and the IDF-4.4 non-fragmented-inbound limitation.

## Memory/threading audit status

Audited portable components:

- Router: external-preferred route entries/vector; no per-request route-list copy; callbacks outside shared lock.
- Middleware: mutation-time external-preferred immutable snapshots; one shared_ptr acquisition per request; callbacks outside lock.
- HttpService: mutation-time provider snapshots; callbacks outside lock.
- Resources: one reusable external-preferred stream buffer; no full-resource body copy.
- Event/Command ingress: direct final-buffer reads as described above.
- State representation: codec scratch moved off request stack.
- HTTP/DNS lifecycle: state snapshot under locks, platform/user callbacks outside locks.
- WebSocket client portable validation: no native handles/credential ownership in Web; structured headers are enumerated without a mandatory prebuilt raw blob.

Audited ESP32 WebSocket providers:

- server inbound receive/frame buffers external-preferred;
- server complete-frame/first-fragment copy removed via moves;
- server outbound payload copy retained intentionally because IDF asynchronous send requires provider-owned lifetime;
- client TLS credentials retained only where IDF keeps pointers;
- client host/path/protocol/header native initialization strings are temporary because the IDF client duplicates them during init;
- client single-DATA-event payloads are borrowed directly for synchronous callbacks;
- client split-native-frame accumulation uses external-preferred storage;
- callbacks occur after provider locks are released.

## CI checkpoint

Known-green checkpoints:

- host example/core suite including Implementor example: `33163484385`;
- WebSocket portable client security/policy validation: `33164325249`;
- Event/Command direct-read integration: host `33163816790`, ESP32 `33163816789`;
- ESP32 WebSocket client compile/link + ESP32 consumer example after the GCC naming fix: rerun `33164971085` green;
- hardened server start path: `33163078689`;
- forced pinned WebSocket server support: `33162559309`;
- known-length/HEAD HTTP: `33161700071`;
- HTTP+DNS: `33160458979`.

At this checkpoint, the latest documentation/policy-regression commits have queued fresh host validation. Confirm the latest host run before closing Web #23 / ESP32 #7 or opening the tranche PR. The concrete ESP32 client code itself is already proven in the pinned compile/link lane above.

## Next work

1. Confirm the latest current-head host suite is green, including `test_websocket_policy.cpp`.
2. Once green, close Web #23 and ESP32 #7 as completed, preserving the documented IDF-4.4 non-fragmented-inbound limitation.
3. Perform the final lightweight branch diff/API/lifetime review for the tranche; do not add compatibility shims or speculative features.
4. Prepare the tranche PR/release documentation and issue closure sequence when explicitly moving into release preparation. Do not change version numbers until that release step is started.
