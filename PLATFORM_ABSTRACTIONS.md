# ESPressio-Web Platform Abstractions

ESPressio-Web is platform-neutral. It must not include ESP32, Arduino, ESP-IDF, FreeRTOS, lwIP, or target filesystem types in its public domain API.

The portable library owns Web semantics and decomposed platform contracts. Concrete implementations translate target facilities into those contracts in architecture packages such as ESPressio-ESP32.

Current capability families:

- HTTP server lifecycle and request dispatch
- HTTP request metadata/body reading
- HTTP response writing/streaming
- WebSocket endpoint/session lifecycle and frame I/O
- WebSocket client lifecycle and frame I/O
- DNS server lifecycle and request/response handling
- HTTPS/TLS capability configuration hooks

Native enums, handles, errors, task types, filesystem handles and socket descriptors must not cross the abstraction boundary.

## HTTP request/response lifetime

HTTP request and response objects are borrowed synchronous views over the platform request-processing context. They may not be retained after a handler returns. Paths and query strings are borrowed where the target permits; header values are retrieved lazily into bounded caller-owned storage; request and response bodies use streaming APIs.

A concrete platform may invoke more than one request concurrently. Routing and middleware therefore must be safe for concurrent reads/mutations, and no application callback may execute while a Web internal mutex is held.

Known-length responses and unknown-length streamed responses are distinct platform cases. A provider may use fixed `Content-Length` framing for the former and chunked/native streaming for the latter. The portable layer must not require complete-body buffering merely to satisfy HTTP framing.

Handler objects, middleware objects, service providers and custom responders are application-owned unless an API explicitly says otherwise. Their lifetime must cover every registration/publication that refers to them. Mutation-oriented containers such as Router, MiddlewarePipeline and HttpService provide synchronization/snapshot semantics; configuration methods on individual leaf handlers/responders are setup-time operations and should be completed before those objects are published to concurrently executing request paths unless that type explicitly documents runtime mutation safety.

## WebSocket endpoint binding

A server-side WebSocket endpoint is explicitly bound by the application through `WebSocketEndpointConfiguration`. `Path` is application-owned and must be an absolute path beginning with `/`; ESPressio-Web and concrete platform packages must never invent a fixed WebSocket URI. `Protocol` optionally selects the WebSocket subprotocol exposed by the concrete platform.

`IWebSocketEndpointPlatform` owns the concrete bind/unbind mechanics while `WebSocketEndpoint` owns the portable lifecycle façade and observer surface. A bound endpoint produces durable `IWebSocketConnection` objects whose lifetime is independent of the HTTP upgrade request object.

The portable contract intentionally does not expose an HTTP server handle, socket descriptor, task handle, or native upgrade request. Concrete packages may coordinate their own HTTP and WebSocket providers internally where a target WebSocket implementation is layered on its native HTTP server.

For target stacks whose URI-handler ordering is fixed at HTTP-server start, a new endpoint may require binding before the HTTP server starts. The concrete implementation must reject an unsafe late bind rather than silently changing routing precedence. Unbinding must prevent new endpoint use and must not leave dangling platform callbacks even when native handler removal cannot safely occur concurrently.

Inbound frame payloads supplied to Web observers are borrowed for the duration of the synchronous callback. Concrete implementations may therefore move an owned native receive buffer directly into callback storage rather than making a second copy. Fragment accumulation may retain/move provider-owned buffers until a complete message is available.

Outbound `IWebSocketConnection` sends may be invoked outside the native HTTP request task. Concrete implementations must therefore preserve payload lifetime until asynchronous transmission completes and must not assume that a caller-owned buffer remains valid after `SendBinary` or `SendText` returns. A copy into provider-owned storage is valid and necessary when the native asynchronous API does not retain/copy the caller payload itself. Disconnect notification must also cover peer/network/session teardown rather than relying only on an explicit WebSocket close frame.

## WebSocket client security

`WebSocketClientConfiguration::Secure` is not sufficient by itself to define trustworthy `wss` semantics. A platform provider must not interpret `Secure = true` as permission to establish TLS with server authentication disabled merely because its native client permits that mode.

Portable server-trust policy, optional client credentials, handshake headers, reconnect/timeout and keepalive policy are tracked by Web issue #23. Until that contract is complete, concrete WebSocket-client implementations that cannot guarantee authenticated secure transport must reject underspecified secure configurations rather than silently weakening them.

TLS certificate/key storage and cryptographic ownership do not automatically belong to ESPressio-Web. Existing ESPressio-Security transport protection concerns application-payload security and must not be repurposed as a TLS trust-store abstraction unless a future Security API explicitly defines that responsibility. Web should reference neutral credential/trust abstractions rather than native mbedTLS/ESP-IDF handles.

## DNS ownership

DNS policy remains in ESPressio-Web. A concrete DNS provider translates target UDP/DNS mechanics into the Web DNS request/response contracts; it must not hard-code captive-portal wildcard matching or application addresses. This keeps `WildcardDnsHandler` and other DNS policies portable and application-composable.
