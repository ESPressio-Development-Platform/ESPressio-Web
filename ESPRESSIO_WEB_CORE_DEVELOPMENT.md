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
- ESP32 #6 ESP32 HTTP/WebSocket/DNS concrete implementations

## Completed Web core work on `work/web-core-tranche`

### WebSocket ownership (#11/#13)

Web owns endpoint/client abstractions, Event-over-WebSocket and WebSocket clock synchronization. Sockets was cleaned of WebSocket ownership while retaining reusable transport-neutral clock synchronization protocol machinery. Obsolete Links2004 WebSocket Event transports were removed from ESPressio-ESP32.

### HTTP foundation (#16)

Implemented `ESPressio_Http.hpp` and `ESPressio_HttpServer.hpp` with:

- standard method/status vocabulary;
- borrowed path/query request views;
- lazy caller-buffer header reads including empty-header semantics;
- streamed request bodies;
- response `Begin` / `Write` / `Complete` / `Abort` state machine;
- server configuration/capability validation and declared request-size enforcement;
- lifecycle `Stopped -> Initializing -> Ready -> Starting -> Running -> Stopping`, with `Faulted` failure state;
- Observable lifecycle notifications outside locks;
- explicit `Handled` versus `NotHandled` request result semantics.

### Router and middleware (#17)

Implemented thread-safe exact/named routing with fixed-capacity borrowed parameter views, route handles/removal and deterministic precedence. Route-table locks are released before application handlers execute.

Middleware uses an immutable chain snapshot rebuilt only on mutation. Requests retain one snapshot rather than copying middleware lists. Middleware callbacks execute outside the mutation lock. Continuations are explicitly one-shot and host-tested against double invocation and re-entrant self-removal.

### Resources/errors (#18)

Implemented generic `IWebResourceProvider`, bounded streaming responses, MIME resolution, traversal rejection, GET/HEAD static resource handling and Web-owned application/error composition. Optional `ESPressio_WebPersistence.hpp` adapts current Persistence `IFileStorage` (`Stat` + offset `Read`) without making Persistence a core Web dependency. Configurable resource-backed error pages reuse the same bounded streaming primitive.

### DNS (#19)

Implemented platform-neutral DNS question/address/response vocabulary, synchronous request context, lifecycle facade and Web-owned `WildcardDnsHandler`. Unhandled questions produce NXDOMAIN. Wildcard target/TTL are construction-time state so active requests cannot race configuration mutation.

### Request-aware composition (#21/#14)

Implemented a stable-snapshot provider registry/service surface. Providers inspect the live `HttpRequest` and choose themselves using request metadata without the HTTP core knowing JSON, CBOR, Serializable or other representations. Mutation is synchronized; provider invocation occurs without the registry lock.

### State / Command / Event adapters (#20)

Implemented optional headers only; none registers a URL.

- `ESPressio_WebEvent.hpp`: POST body becomes one bounded Event transport packet delivered to the current `IEventTransportReceiver`; successful handoff returns `202 Accepted`.
- `ESPressio_WebCommand.hpp`: bounded raw command becomes the current `CommandRequestEnvelope`, queues `InboundCommandEvent` through the owner-library Event path and returns `202 Accepted`; it does not synchronously invoke CommandRegistry.
- `ESPressio_WebState.hpp`: GET/HEAD calls `StatePublisher::Snapshot<TDefinition>()`; representation is pluggable and the default binary writer uses State-owned `StateCodec<TDefinition>`, preserving TypeId/Epoch/Revision metadata. It works with State introspection disabled.

Host CI now validates these adapters against the actual current owner-library working branches. GitHub Actions run `33159815566` completed successfully after linking the real Threads implementation and termination dispatcher into the successful Command queue-path test.

## ESP32 HTTP provider status (#16 / ESP32 #6)

Implementation is on ESPressio-ESP32 `feature/1-system-memory-provider` in `src/ESPressio_HttpServerPlatform.hpp`.

Implemented concrete types:

- `ESP32HttpRequestPlatform`
- `ESP32HttpResponsePlatform`
- `ESP32HttpServerPlatform`

The implementation uses ESP-IDF `esp_http_server`; Web routing remains entirely in ESPressio-Web. Request URI/query are borrowed, headers are lazy, bodies use `httpd_req_recv`, response metadata is owned by the adapter until IDF commits it, and response bodies stream using IDF chunked responses.

A first private Web-side PlatformIO integration build reached the pinned Arduino-ESP32/IDF compiler and identified three version-specific differences: `httpd_req_t::method` requires explicit conversion, the pinned IDF does not expose runtime `max_req_hdr_len`, and it has no `HTTP_ANY`. These were corrected in ESP32 commit `be9848f6ac4b84172b17d3f95ea9345b66ebfe87` by explicit method conversion, omitting the unsupported runtime field, and registering the wildcard URI once for each supported HTTP method. The same patch also fixes non-empty/empty header presence detection.

ESPressio-Web remains optional for ESPressio-ESP32: the ESP32 umbrella includes the Web provider only when `ESPressio_HttpServer.hpp` is available, and ESPressio-ESP32 `library.json` does not mandate Web. Public ESP32 smoke CI therefore remains independent of the private Web repository.

Real cross-repository ESP32 Web compilation is instead owned by private Web workflow `.github/workflows/esp32-provider.yml`, which checks out the public ESP32 working branch and builds `tests/esp32` with both libraries locally available.

## CI checkpoint

- Web host suite through #20/#21: **green** at run `33159815566`.
- Private Web-side ESP32 provider integration: first run `33159985994` reached the real pinned IDF compiler and produced the compatibility findings above; a fresh run is triggered by this continuity update and must be checked against ESP32 commit `be9848f6...`.
- Public ESP32 CI has been restored to core-provider coverage and no longer attempts to clone private Web.

## Next work

1. Check the newly triggered private ESP32 provider integration build and fix any remaining pinned-IDF compile/runtime-contract mismatch directly on ESP32 `feature/1-system-memory-provider`.
2. Revisit HTTP known-length/HEAD semantics on `esp_http_server`: IDF chunked send always emits `Transfer-Encoding: chunked`, so do not fake a conflicting `Content-Length`. Preserve bounded streaming and add a correct platform strategy before calling HTTP provider complete.
3. Implement the ESP32 DNS primitive provider without using Arduino `DNSServer` policy logic. The adapter must parse/serialize DNS over the native UDP primitive only; wildcard/captive-portal policy remains Web-owned.
4. Implement WebSocket server/client concrete providers against Web-owned contracts and current IDF facilities.
5. Add `examples/Implementors/` and `examples/ESP32/` examples showing the two distinct extension surfaces.
6. Update README and `PLATFORM_ABSTRACTIONS.md`, perform final memory/threading audit, and only then prepare the tranche PR/release work.
