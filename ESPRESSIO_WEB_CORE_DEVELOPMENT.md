# ESPressio Web Core Development Tranche

This file is the continuity record for the ESPressio-Web core development tranche. A fresh ChatGPT conversation must be able to read this file and continue from the last recorded item without reconstructing hidden context.

## Operating rules

1. Create a distinct GitHub Issue in the appropriate repository for every distinct body of work. Every commit and related work message must reference the relevant Issue number(s).
2. When modifying any ESPressio library other than ESPressio-Web, work solely on that repository's current Working Branch. If only `main` exists, create a new Working Branch first and perform all changes there.
3. Current explicitly supplied Working Branches at tranche start:
   - ESPressio-System: `feature/1-system-memory-policy`
   - ESPressio-ESP32: `feature/1-system-memory-provider`
   - ESPressio-Security: `optimisation/23-memory-policy-transient-buffers`
   - ESPressio-Task: `feature/1-task-execution`
   - ESPressio-Command: `feature/30-async-command-routing`
   - ESPressio-Units: `main` (create a Working Branch before modification)
   - ESPressio-Observable: `feature/16-rtti-free-observer-registry`
   - ESPressio-Serializable: `optimisation/25-psram-buffers`
   - ESPressio-Persistence: `feature/10-platform-storage-abstractions`
   - ESPressio-Timing: `feature/29-platform-clock-abstractions`
   - ESPressio-Threads: `optimisation/69-resource-footprint`
   - ESPressio-Event: `feature/57-rtti-free-memory-efficiency`
   - ESPressio-Serial: `optimisation/39-explicit-thread-lifecycle`
   - ESPressio-State: `feature/1-state-foundation`
   - ESPressio-ESP-Now: `bugfix/39-wifi-coexistence`
   - ESPressio-WiFi: `feature/20-wifi-off-mode`
4. Do not mutate any existing Version Numbers during this tranche. A later Release Restructuring process will handle versions.
5. Backwards compatibility is not a goal. The platform moves forward. When an interface changes, update all known references to the latest API rather than adding compatibility shims.
6. Treat ESPressio libraries as fully mutable for this work. There are no external consumers requiring compatibility preservation.
7. Workarounds are not solutions. Resolve architectural/implementation issues properly; if that cannot be done safely, stop that body of work and report the blocker rather than adding a workaround.
8. Application-visible routes are always chosen by the implementing developer. ESPressio libraries, adapters and providers must never impose fixed application URL paths.
9. Platform/domain ownership rule: each ESPressio domain library owns its vocabulary, abstractions, lifecycle, state and behaviour; ESPressio-ESP32 owns ESP32-specific concrete implementations and native-type/result translation.
10. Maintain this file chronologically after each meaningful tranche item, including repository, branch, Issue, commit, findings, tests and remaining work.

## Agreed ESPressio-Web architecture

- Decomposed platform interfaces rather than one monolithic Web platform interface.
- Borrowed synchronous HTTP request contexts; no retained/deferred HTTP response completion.
- Lazy header/query/route-parameter lookup where practical.
- Streaming/bounded request bodies and responses.
- Async application operations acknowledge over HTTP (typically `202 Accepted`) and use appropriate asynchronous transports for later information flow.
- Optional HTTP adapters for ESPressio primitives; State GET is point-in-time synchronous state retrieval, while inbound async Command/Event operations acknowledge acceptance without pretending completion.
- Every primitive adapter is application-published at a developer-selected route.
- Runtime route registration and unregistration.
- Deterministic exact/named-parameter routing initially; regex routing deferred.
- Unhandled HTTP paths may resolve through static-resource serving; filesystem access remains ESPressio-Persistence's responsibility.
- Developer-configurable error responses may be static, resource-backed, or custom handlers.
- Middleware uses a chain capable of short-circuiting.
- HTTP handlers execute in the underlying platform's chosen request context; Web does not silently marshal requests onto ESPressio Threads. Application route targets must therefore be thread-safe.
- ESPressio-Observable is a core dependency for lifecycle/listener registration; optional bridges/adapters expose Event/State/etc.
- Concrete implementations declare capabilities using ESPressio-Web vocabulary.
- WebSocket is first-version scope; SSE is deferred.
- WebSocket protocol/server/client ownership moves from ESPressio-Sockets into ESPressio-Web. ESPressio-Sockets retains generic TCP/UDP/socket-oriented concerns and transport-neutral protocol machinery.
- DNS is a first-class ESPressio-Web concern sufficient to let applications build captive portals, but ESPressio-Web never imposes captive-portal pages/endpoints/actions.
- Providers/services may inspect complete HTTP metadata, including Content-Type and Accept, before selecting serialization/deserialization behaviour. Core HTTP transport remains representation-agnostic.
- Security semantics may be consumed from ESPressio-Security; target-specific TLS/HTTPS mechanics belong in ESPressio-ESP32.

## Deferred feature Issues created

- ESPressio-Web #2 — regular-expression route matching
- ESPressio-Web #3 — Server-Sent Events
- ESPressio-Web #4 — multipart/form-data streaming
- ESPressio-Web #5 — HTTP response compression
- ESPressio-Web #6 — byte-range requests
- ESPressio-Web #7 — conditional requests/cache validation
- ESPressio-Web #8 — configurable HTTP caching policies
- ESPressio-Web #9 — HTTP/2

## Active tranche Issues

- ESPressio-Web #10 — establish repository foundation and structure
- ESPressio-Web #11 — migrate WebSocket ownership and ESPressio primitive transports into Web
- ESPressio-Web #12 — implement Web core HTTP/routing/middleware/DNS/resources/adapters
- ESPressio-Web #13 — audit all consumers of Sockets WebSocket APIs
- ESPressio-Web #14 — document/test application-owned routes and MIME-aware service/provider composition
- ESPressio-Sockets #33 — remove WebSocket ownership and WebSocket-specific transports from Sockets
- ESPressio-ESP32 #6 — implement ESP32 concrete HTTP/WebSocket/DNS providers

## Chronological work log

### 2026-08-28 — tranche initialization

- Created deferred feature Issues #2 through #9 in ESPressio-Web.
- Created active Issues #10 through #14 in ESPressio-Web, #33 in ESPressio-Sockets, and #6 in ESPressio-ESP32.
- Created ESPressio-Web Working Branch `feature/10-web-foundation` from `main`.
- Identified ESPressio-Sockets current Working Branch as `feature/state-transport-major-release`; it is newer than and incorporates ongoing platform-abstraction work. All Sockets mutations for this tranche must occur only there.
- Confirmed ESPressio-ESP32 Working Branch `feature/1-system-memory-provider` exists and must be used for all ESP32 Web implementation changes.
- Inspected current ESPressio-Sockets WebSocket implementations. They directly own Links2004 `WebSocketsServer`/`WebSocketsClient`, polling workers, heartbeats and frame I/O. WebSocket-specific Event transports and clock synchronization wrappers therefore require architectural relocation rather than file-copy compatibility shims.
- Confirmed generic `Event::IEventTransport` is transport-neutral and suitable for a Web-owned concrete WebSocket Event transport.
- Confirmed `SocketCommandSession` already consumes inbound byte feeds and an outbound writer callback, so transport-neutral Command session/protocol machinery can remain in Sockets while Web owns any WebSocket-specific adapter.
- Began ESPressio-Web foundation on `feature/10-web-foundation`; README created in commit `aaaf07debfbb1536759d097996936b04d1380c58` referencing #10 and #14.
- An empty temporary branch `tmp-ignore` was accidentally created during connector write-path testing. It contains no code changes and is not part of the development flow.

### Next work

1. Complete ESPressio-Web repository foundation commit series (#10).
2. Audit all ESPressio repositories for actual source-level consumers of Sockets WebSocket APIs before removal (#13).
3. Migrate/rebuild WebSocket abstractions and primitive transports in Web (#11) and remove them from Sockets solely on `feature/state-transport-major-release` (#33).
4. Implement ESP32 concrete Web providers solely on `feature/1-system-memory-provider` (#6).
5. Implement remaining Web core (#12), tests, examples and documentation.
