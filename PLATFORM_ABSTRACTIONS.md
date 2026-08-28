# ESPressio-Web Platform Abstractions

ESPressio-Web is platform-neutral. It must not include ESP32, Arduino, ESP-IDF, FreeRTOS, lwIP, or target filesystem types in its public domain API.

The portable library owns Web semantics and decomposed platform contracts. Concrete implementations translate target facilities into those contracts in architecture packages such as ESPressio-ESP32.

Current capability families:

- HTTP server lifecycle and request dispatch
- HTTP request metadata/body reading
- HTTP response writing/streaming
- WebSocket endpoint/session lifecycle and frame I/O
- DNS server lifecycle and request/response handling
- HTTPS/TLS capability configuration hooks

Native enums, handles, errors, task types, filesystem handles and socket descriptors must not cross the abstraction boundary.

## HTTP request/response lifetime

HTTP request and response objects are borrowed synchronous views over the platform request-processing context. They may not be retained after a handler returns. Paths and query strings are borrowed where the target permits; header values are retrieved lazily into bounded caller-owned storage; request and response bodies use streaming APIs.

A concrete platform may invoke more than one request concurrently. Routing and middleware therefore must be safe for concurrent reads/mutations, and no application callback may execute while a Web internal mutex is held.

## WebSocket endpoint binding

A server-side WebSocket endpoint is explicitly bound by the application through `WebSocketEndpointConfiguration`. `Path` is application-owned and must be an absolute path beginning with `/`; ESPressio-Web and concrete platform packages must never invent a fixed WebSocket URI. `Protocol` optionally selects the WebSocket subprotocol exposed by the concrete platform.

`IWebSocketEndpointPlatform` owns the concrete bind/unbind mechanics while `WebSocketEndpoint` owns the portable lifecycle façade and observer surface. A bound endpoint produces durable `IWebSocketConnection` objects whose lifetime is independent of the HTTP upgrade request object.

The portable contract intentionally does not expose an HTTP server handle, socket descriptor, task handle, or native upgrade request. Concrete packages may coordinate their own HTTP and WebSocket providers internally where a target WebSocket implementation is layered on its native HTTP server.

For target stacks whose URI-handler ordering is fixed at HTTP-server start, a new endpoint may require binding before the HTTP server starts. The concrete implementation must reject an unsafe late bind rather than silently changing routing precedence. Unbinding must prevent new endpoint use and must not leave dangling platform callbacks even when native handler removal cannot safely occur concurrently.

Outbound `IWebSocketConnection` sends may be invoked outside the native HTTP request task. Concrete implementations must therefore preserve payload lifetime until asynchronous transmission completes and must not assume that a caller-owned buffer remains valid after `SendBinary` or `SendText` returns. Disconnect notification must also cover peer/network/session teardown rather than relying only on an explicit WebSocket close frame.

## DNS ownership

DNS policy remains in ESPressio-Web. A concrete DNS provider translates target UDP/DNS mechanics into the Web DNS request/response contracts; it must not hard-code captive-portal wildcard matching or application addresses. This keeps `WildcardDnsHandler` and other DNS policies portable and application-composable.
