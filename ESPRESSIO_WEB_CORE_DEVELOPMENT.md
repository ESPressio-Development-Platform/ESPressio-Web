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
- Identified ESPressio-Sockets current Working Branch as `feature/state-transport-major-release`; all Sockets mutations for this tranche are restricted to that branch.
- Confirmed ESPressio-ESP32 Working Branch `feature/1-system-memory-provider`; all ESP32 mutations for this tranche are restricted to that branch.
- Inspected current/legacy Sockets WebSocket implementations and confirmed that their server/client ownership, worker loops, heartbeat handling and frame I/O are Web-domain responsibilities.
- Confirmed generic `Event::IEventTransport` is transport-neutral.
- Confirmed `SocketCommandSession` already accepts an inbound byte feed plus outbound writer callback, allowing it to remain reusable protocol/session machinery rather than WebSocket ownership.
- Initialized ESPressio-Web repository structure on `feature/10-web-foundation`, including README, build metadata, host-test scaffolding, platform-abstraction documentation, split `examples/Implementors` and `examples/ESP32` areas, and this continuity file.
- An empty temporary branch `tmp-ignore` was accidentally created during connector write-path testing. It contains no code changes and is not part of the development flow; ESPressio-Web #15 records its eventual cleanup.

### 2026-08-28 — WebSocket abstraction and transport migration

ESPressio-Web branch: `feature/11-websocket-migration`.

- Fast-forwarded the WebSocket migration branch to the completed foundation baseline before migration work.
- Added platform-neutral Web result/capability vocabulary (`ESPressio_WebTypes.hpp`) in commit `63cfe783b50bab4763b8a6ce0d65e12845ebaeea` (#11 #12).
- Added decomposed WebSocket contracts (`IWebSocketConnection`, endpoint/client platform interfaces and platform sinks) and then refined them so the concrete platform has one sink into Web while Web owns listener fan-out.
- Added Web-owned `WebSocketEndpoint` and `WebSocketClient` facades.
- Found and corrected an Observable lifetime defect before further layering: current Observable notification objects require shared ownership. Reworked endpoint/client listener dispatch to use internally shared `ThreadSafeObservable` notification objects, matching the current WiFi architecture. Fix commits: `86647ddc646226bcc1aaa6f3f0932704d7b2fca9` and `a18ad90aa4b5304766537a1cd56767e7c810630e` (#11).
- Migrated WebSocket Event transport into ESPressio-Web as an adapter over `WebSocketEndpoint`/`WebSocketClient`; it no longer owns a WebSocket library, server, worker, port or route. Initial commit `1186e8f0718aa3d0aff59baebf82713945e672a9`, receiver-synchronization correction `5e0763a7b8d8ad1836aa137d9f82bc9750ebbbcf` (#11).
- Preserved callback thread safety by copying the Event receiver pointer under its mutex and invoking the receiver after releasing that mutex.
- Migrated WebSocket clock synchronization into ESPressio-Web as an endpoint/client adapter in commit `284e3493e4b45c794442a839b3a8387c180dad2c` (#11 #33). It reuses the existing transport-neutral Sockets clock wire protocol and uses `PrecisionThread` only for optional periodic synchronization scheduling; WebSocket I/O itself remains platform-owned.

### 2026-08-28 — Sockets ownership cleanup

ESPressio-Sockets branch: `feature/state-transport-major-release` only.

- Discovered that current Sockets clock protocol types mixed portable synchronization configuration with UDP `IPAddress`, which would have leaked an Arduino type into ESPressio-Web.
- Split portable clock synchronization types from UDP-specific configuration rather than duplicating the protocol in Web:
  - `16b910068792f9c7a07d61fdfd30467d19fd73a4` — portable clock types (#32 #33).
  - `cc1b75e7a7c8b589cf76b6603172491a1e31f8e0` — new UDP-specific clock configuration header (#32 #33).
  - `742f0eacfe4dad7221b349d53cc56673501a5b49` — explicit UDP include update (#32 #33).
- Removed the WebSocket clock synchronization wrapper from Sockets (`250bcb3a4b88cf630a71e4af5805c38441bfa0ce`, #33).
- Removed the historical concrete Event transport compatibility forwarder rather than preserving a compatibility shim (`dc2f35b764e85f09d969c91a5359ff7d6ff98396`, #33).
- Removed stale WebSocket Event and clock synchronization examples from Sockets (#33).
- Removed WebSocket from the Sockets clock umbrella (`724f31b043f45542f30d732595916e07ceedeef6`, #33).
- Removed WebSocket ownership/dependency claims from Sockets package metadata while leaving version `0.7.3` unchanged (`880cab3bdaabaddfab1c213a118808d888bba7dc`, `5767c636cdc1262bf55eee2eb651eaa6fe043904`, #33).
- Rewrote the active-branch README to make Sockets ownership explicitly TCP/UDP/generic-socket oriented and ESPressio-Web ownership explicitly WebSocket-oriented (`f170f1c0c67586f4223773f25cc3ea77d6e9f66d`, #33).

### 2026-08-28 — ESP32 obsolete WebSocket transport removal

ESPressio-ESP32 branch: `feature/1-system-memory-provider` only.

- The previous platform-abstraction tranche had already moved the Links2004 WebSocket Event transports out of Sockets into ESPressio-ESP32. They were therefore the live obsolete concrete transports requiring removal during this migration.
- Removed `ESPressio_WebSocketServerEventTransport.hpp` (`6d5de311fd3cac88f645de7acb6fb4f8d6d7774b`, #6 / Web #11).
- Removed `ESPressio_WebSocketClientEventTransport.hpp` (`606988970f1536c2b510bef7c9b573d696bdad72`, #6 / Web #11).
- Removed their conditional inclusion from `ESPressio_ESP32SocketTransports.hpp` (`1d7ceb3120ee14707f7da65c82c5fbeab8758637`, #6 / Web #11).
- No ESP32 version number was changed.
- New ESP32 WebSocket/HTTP/DNS concrete implementations still need to be built against the new ESPressio-Web contracts under ESPressio-ESP32 #6.

### 2026-08-28 — WebSocket consumer audit (#13)

- GitHub's code-search index returned false negatives even for known Sockets WebSocket source, so it was explicitly rejected as an authoritative audit source.
- Performed a dependency-metadata audit on every user-supplied current working branch: System, ESP32, Security, Task, Command, Units, Observable, Serializable, Persistence, Timing, Threads, Event, Serial, State, ESP-Now and WiFi.
- None of Event, Command, State, Timing, WiFi, Serial, ESP-Now, Security, Task, System, Persistence, Observable, Serializable, Threads or Units declares Sockets as a dependency on its current working branch.
- ESPressio-ESP32 was the only live platform-level location containing the obsolete WebSocket Event transport implementation; those files and their umbrella exposure have now been removed.
- A second recursive-tree pass found no WebSocket-named source/example surfaces in Event, Command, State, Timing, WiFi, Threads, Serial, ESP-Now or Security on the mandated current branches.
- Result: no additional ESPressio library code migrations have been identified beyond Sockets and ESP32. Continue to treat any later-discovered stale include as an API migration defect to update directly, never as a reason to add compatibility aliases.

## Current next work

1. Finish WebSocket migration validation: Web umbrella/test coverage and any remaining current-branch Sockets/ESP32 documentation references (#11/#33/#6).
2. Advance `feature/12-web-core` to the latest `feature/11-websocket-migration` head before any #12 work.
3. Implement HTTP vocabulary, decomposed HTTP platform contracts, synchronous request context, lazy metadata access, streaming body/response contracts, response state, routing, middleware, static-resource fallback, configurable errors and lifecycle (#12/#14).
4. Implement DNS abstractions and lifecycle sufficient for application-defined captive portals (#12).
5. Implement optional State/Command/Event HTTP adapters and representation-aware provider/service composition, with application-owned routes (#12/#14).
6. Implement ESP32 HTTP/WebSocket/DNS concrete providers solely on `feature/1-system-memory-provider` (#6).
7. Add host tests, platform-implementor examples and ESP32 consumer examples; keep this file updated after each meaningful tranche item.
