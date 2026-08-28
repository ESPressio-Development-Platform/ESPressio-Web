# ESPressio Web Core Development Tranche

This is the authoritative continuity record for the ESPressio-Web core development tranche. A fresh ChatGPT conversation must be able to read this file and continue from the exact point at which the preceding conversation ended.

## Operating rules

1. Every distinct body of work requires a distinct GitHub Issue in the appropriate repository. Commits and related work messages must reference the relevant Issue(s).
2. **ESPressio-Web has exactly one Working Branch for this entire tranche: `work/web-core-tranche`.** All Web work, regardless of Issue, must be committed only to this branch. Do not create Issue-specific or phase-specific Web branches.
3. Earlier Web branches `feature/10-web-foundation`, `feature/11-websocket-migration`, `feature/12-web-core`, and accidental `tmp-ignore` are redundant historical refs. The canonical branch already contains their latest work. They must receive no further commits and may be pruned.
4. When modifying any repository other than ESPressio-Web, work solely on that repository's current Working Branch. If only `main` exists, create one Working Branch before modification.
5. Explicit non-Web Working Branches for this tranche:
   - ESPressio-System: `feature/1-system-memory-policy`
   - ESPressio-ESP32: `feature/1-system-memory-provider`
   - ESPressio-Security: `optimisation/23-memory-policy-transient-buffers`
   - ESPressio-Task: `feature/1-task-execution`
   - ESPressio-Command: `feature/30-async-command-routing`
   - ESPressio-Units: `main` (create a Working Branch before any modification)
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
   - ESPressio-Sockets: `feature/state-transport-major-release`
6. Do not mutate any existing Version Number during this tranche. Release restructuring happens later.
7. Backwards compatibility is not a goal. Update all consumers to the newest API; do not create compatibility shims.
8. Treat ESPressio libraries as fully mutable. There are no external consumers requiring compatibility preservation.
9. Workarounds are not solutions. Resolve issues properly or stop and report a genuine blocker.
10. Application-visible routes are always chosen by the application developer. ESPressio libraries/providers/adapters must never impose fixed URLs.
11. Domain ownership rule: an ESPressio domain library owns vocabulary, abstractions, lifecycle, state and behavior. ESPressio-ESP32 owns ESP32-specific concrete implementations and translation from Arduino/ESP-IDF/native types and results.
12. User callbacks/Observers must never execute while an ESPressio-Web internal mutex is held.
13. Maintain this file chronologically after every meaningful body of work, including repositories, branches, Issues, commits, test status, findings and remaining work.

## Agreed Web architecture

- Platform-neutral Web API; no ESP32/Arduino/ESP-IDF/lwIP types in ESPressio-Web public interfaces.
- Decomposed platform contracts rather than one monolithic platform interface.
- Borrowed synchronous HTTP request contexts. HTTP requests/responses may not be retained for deferred completion.
- Low-copy metadata: path/query are borrowed views; headers are lazily retrieved into caller-owned bounded storage rather than materialized into maps.
- Request bodies and responses use streaming/bounded APIs.
- Long-running application operations acknowledge synchronously over HTTP (normally `202 Accepted`) and deliver later results through appropriate async facilities.
- Runtime route registration/removal; exact and named-parameter routes initially. Regex is deferred.
- Deterministic route precedence. No application URL convention is imposed.
- Middleware supports ordered chaining and short-circuiting.
- Unhandled routes may fall through to static resource resolution. Storage/filesystem access remains owned by ESPressio-Persistence.
- Error responses are configurable and may be static, Persistence-backed, or handler-generated.
- HTTP handlers execute in the platform request-processing context. Web does not silently marshal requests to ESPressio Threads.
- Observable is a core lifecycle/listener dependency; Event/State/Command/etc. integrations are opt-in.
- WebSocket is Web-domain ownership and first-version scope. SSE is deferred.
- ESPressio-Sockets retains generic TCP/UDP/socket framing and reusable transport-neutral protocol machinery; it does not own WebSocket.
- DNS is a first-class Web mechanism sufficient for application-built captive portals, but Web never imposes captive portal pages/routes/actions.
- HTTP core is representation agnostic. Optional providers/adapters may inspect complete request metadata including Content-Type and Accept before selecting serialization behavior.
- Web authentication/authorization semantics may consume ESPressio-Security; cryptography/key ownership remains Security. ESP32 TLS mechanics belong in ESPressio-ESP32.

## Deferred Feature Issues

- Web #2 — regex route matching
- Web #3 — Server-Sent Events
- Web #4 — multipart/form-data streaming
- Web #5 — response compression
- Web #6 — byte-range requests
- Web #7 — conditional requests/cache validation
- Web #8 — configurable HTTP cache policies
- Web #9 — HTTP/2

## Active tranche Issues

- Web #10 — repository foundation
- Web #11 — WebSocket ownership/transport migration
- Web #12 — umbrella Web core tranche
- Web #13 — Sockets WebSocket consumer audit
- Web #14 — application-owned routes and MIME-aware composition
- Web #15 — consolidate tranche to one Web Working Branch / remove redundant refs
- Web #16 — HTTP vocabulary, request/response contracts, server lifecycle
- Web #17 — runtime routing and middleware
- Web #18 — Persistence-backed static resources and configurable errors
- Web #19 — DNS abstractions/lifecycle
- Web #20 — State/Command/Event HTTP adapters
- Web #21 — request-aware service/provider composition
- Sockets #33 — remove WebSocket ownership from Sockets
- ESP32 #6 — ESP32 HTTP/WebSocket/DNS implementations

## Chronological work log

### 2026-08-28 — tranche initialization

- Created deferred Web Issues #2-#9 and initial active Issues #10-#14; later decomposed remaining core into #16-#21.
- Initialized Web repository structure based on mature ESPressio libraries: metadata, ESP-IDF component files, host-test scaffolding, CI, `PLATFORM_ABSTRACTIONS.md`, `examples/Implementors`, `examples/ESP32`, README, and this continuity file.
- Original work was incorrectly split across `feature/10-web-foundation`, `feature/11-websocket-migration`, and `feature/12-web-core`; an empty `tmp-ignore` ref was also created while testing connector writes. This branch model was later corrected under #15.

### 2026-08-28 — WebSocket ownership migration (#11)

- Added platform-neutral Web result/capability vocabulary.
- Added WebSocket connection, endpoint platform and client platform contracts.
- Added Web-owned `WebSocketEndpoint` and `WebSocketClient` facades.
- Corrected Observable lifetime/thread-safety by using internally shared `ThreadSafeObservable` notification objects, following the current WiFi pattern. Relevant fixes included `86647ddc646226bcc1aaa6f3f0932704d7b2fca9` and `a18ad90aa4b5304766537a1cd56767e7c810630e`.
- Migrated Event-over-WebSocket into Web. It attaches to an application-owned endpoint/client and owns no server, port, route, worker or external platform library. Initial migration `1186e8f0718aa3d0aff59baebf82713945e672a9`; receiver synchronization fix `5e0763a7b8d8ad1836aa137d9f82bc9750ebbbcf`.
- Migrated WebSocket clock synchronization into Web in `284e3493e4b45c794442a839b3a8387c180dad2c`. It reuses Sockets' transport-neutral synchronization wire protocol. A `PrecisionThread` is used only for optional periodic client requests; WebSocket I/O itself stays platform-owned.
- Added host WebSocket fakes/tests and corrected CI multi-repository workspace layout. Canonical migration baseline CI succeeded in GitHub Actions run `33155806592`.

### 2026-08-28 — Sockets cleanup (#33, branch `feature/state-transport-major-release` only)

- Found portable clock synchronization types incorrectly mixed with UDP `IPAddress` configuration.
- Split portable synchronization config from UDP-only config rather than duplicating protocol logic:
  - `16b910068792f9c7a07d61fdfd30467d19fd73a4`
  - `cc1b75e7a7c8b589cf76b6603172491a1e31f8e0`
  - `742f0eacfe4dad7221b349d53cc56673501a5b49`
- Removed Sockets WebSocket clock wrapper, stale WebSocket examples, compatibility transport forwarder, WebSocket clock umbrella inclusion, and WebSocket package/docs ownership claims.
- Kept Sockets version `0.7.3` unchanged.
- Recursive tree verification after cleanup found no WebSocket-named source/example path remaining in the active Sockets branch.

### 2026-08-28 — ESP32 obsolete WebSocket transport removal (#6, branch `feature/1-system-memory-provider` only)

- Removed obsolete Links2004 `ESPressio_WebSocketServerEventTransport.hpp` and `ESPressio_WebSocketClientEventTransport.hpp`.
- Removed their conditional exposure from `ESPressio_ESP32SocketTransports.hpp`.
- ESP32 version remained unchanged.
- Replacement ESP32 implementations against Web-owned contracts are still pending under ESP32 #6.

### 2026-08-28 — WebSocket consumer audit (#13)

- GitHub code search produced known false negatives and was rejected as authoritative.
- Audited dependency metadata on every supplied current Working Branch.
- Event, Command, State, Timing, WiFi, Serial, ESP-Now, Security, Task, System, Persistence, Observable, Serializable, Threads and Units do not declare Sockets as a dependency on those working branches.
- Recursive tree checks also found no WebSocket-named source/example surfaces in Event, Command, State, Timing, WiFi, Threads, Serial, ESP-Now or Security.
- ESPressio-ESP32 was the only live non-Sockets location exposing obsolete WebSocket transports; those were removed as above.
- Result: no additional ESPressio library consumer migrations identified.

### 2026-08-28 — single Web Working Branch consolidation (#15)

- User explicitly rejected concurrent Issue/phase branches for Web.
- Canonical branch selected: `work/web-core-tranche`.
- Fast-forwarded `work/web-core-tranche` to `0ff9dd1d0cdf1cb1a4d0ea756967820e6a259f50`, which was the exact latest head of the prior `feature/12-web-core` line and already contained all foundation and WebSocket migration history.
- No work is stranded on `feature/10-web-foundation`, `feature/11-websocket-migration`, or `feature/12-web-core`.
- `tmp-ignore` contains no work.
- The connected GitHub interface does not expose branch-ref deletion, so these four redundant refs cannot be pruned by ChatGPT. They are safe for the user to delete. **Do not write to them again.**
- Updated Web #15 to document the canonical branch and redundant refs.
- From this point forward every Web Issue is implemented sequentially on `work/web-core-tranche` only.

### 2026-08-28 — HTTP foundation in progress (#16/#12, canonical branch only)

- Commit `0ff9dd1d0cdf1cb1a4d0ea756967820e6a259f50`: added `ESPressio_Http.hpp` containing HTTP method/status/transport/response-state vocabulary, server configuration, lazy bounded header access, borrowed path/query views, streamed body reads, streamed/chunk-compatible response writes, and borrowed `WebRequestContext`.
- HTTP core deliberately avoids per-request maps/vectors for metadata and avoids automatically buffering request/response bodies.
- Commit `c1fd19088cab261a78bb700fd6bc41751c9e2038`: added decomposed `IHttpServerPlatform`, dispatcher/handler contracts and Web-owned HTTP lifecycle facade.
- Immediately identified that the first Ready/Faulted Stop path notified Observers under the server mutex. Corrected in `51909e13ea6d81ffa39d247852259d6a1caab142`; all lifecycle notifications now occur outside the server lock.
- Handler replacement is disallowed while Starting/Running/Stopping so a request dispatch cannot race against replacement of its handler object. Runtime route mutation will instead be implemented inside the long-lived router under #17.
- Added HTTP host tests in `187d026836283baf214592a530689abd0213aba5`, covering lazy header access, streamed body reads, response state, server lifecycle, platform dispatch, capability reporting, and Observer re-entry into `HttpServer::State()`.
- Exposed HTTP core/server from the normal Web umbrella in `f69a54613f92403abd5c94b1eba7dcb9ddd660de`.
- Added HTTP tests to CMake/ctest in `d461f5465ec644ea1cdc3bafea42652fe29b6559`.
- GitHub Actions run `33156913241` was started for this HTTP baseline. Its final result must be checked before #17 routing implementation is considered validated.

## Current next work

1. Check/fix canonical branch CI for HTTP baseline run `33156913241` (#16/#12).
2. Finish #16 validation and documentation on `work/web-core-tranche` only.
3. Implement runtime router and middleware under #17 on the same branch. Route-table reads/mutations must be thread-safe, user handlers must execute outside locks, and routes remain application-owned.
4. Implement Persistence-backed static resources/configurable errors (#18).
5. Implement DNS abstractions/lifecycle (#19).
6. Implement request-aware provider/service composition (#21) and optional State/Command/Event HTTP adapters (#20).
7. Implement ESP32 HTTP/WebSocket/DNS concrete providers solely on ESP32 `feature/1-system-memory-provider` (#6).
8. Add Implementor and ESP32 consumer examples, expand host/integration tests, update README/PLATFORM_ABSTRACTIONS, and keep this file current.
