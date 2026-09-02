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

## WebSocket client transport security and policy

`WebSocketClientConfiguration` separates transport selection, TLS trust/identity, structured handshake headers and connection policy. A provider must never interpret `WebTransportMode::Tls` as permission to establish TLS with server authentication disabled merely because its native client permits that mode.

`WebTlsConfiguration` supports two server-trust modes:

- `PlatformTrust`: the concrete platform may use an architecture-owned trust source only when it actually authenticates the server. If no authenticated trust source is installed/configured, the provider must return `Unsupported` or another explicit failure; it must not fall back to certificate verification being disabled.
- `CertificateAuthority`: the caller supplies CA/certificate material through `WebCredentialView`. The concrete provider maps that material into its native trust facility while preserving any lifetime required by the native stack.

Optional client certificate and private-key views form one client identity and must be supplied together. Credential views are borrowed by the portable `Connect()` call. A concrete provider must copy/retain credential bytes when its native implementation retains pointers beyond initialization. Certificate/key storage and cryptographic ownership do not automatically belong to ESPressio-Web; the Web domain models transport trust/identity without exposing native mbedTLS/ESP-IDF handles.

Application handshake headers are exposed through `IWebClientHeaderSource`. They are validated before the platform receives them, are bounded by `MaximumHandshakeHeaderBytes` and a fixed maximum header count, and may not contain CR/LF injection. Protocol-owned headers (`Host`, `Connection`, `Upgrade`, `Sec-WebSocket-Key`, `Sec-WebSocket-Version`, `Sec-WebSocket-Protocol`) are reserved and cannot be overridden through this surface. Application headers such as `Authorization` remain valid.

`WebSocketClientConnectionPolicy` models portable connection behavior including network timeout, automatic reconnect, reconnect delay, reconnect-after-clean-close, ping/pong timing and optional TCP keepalive. Concrete providers must apply values they can faithfully represent and return `Unsupported` for requested semantics they cannot represent; silently discarding a policy value is not permitted.

A durable client-side `IWebSocketConnection` may span native reconnect cycles. Receive callbacks follow the same synchronous borrowed-payload rule as server WebSockets. Providers may deliver a native complete frame directly without another payload copy, while one native frame split across multiple receive callbacks may be assembled in provider-owned bounded storage.

RFC6455 multi-frame fragmentation is a provider capability constraint. A backend that exposes FIN/continuation state may reassemble fragmented messages before notifying portable observers. A backend that cannot observe enough framing information to distinguish a complete text/binary frame from the first non-final fragment must explicitly document that it supports only non-fragmented inbound messages. Such a provider must not claim full fragmented-message support; continuation frames that cannot be reconstructed safely are not delivered. Applications using that backend must ensure the peer sends non-fragmented messages. The pinned ESP-IDF 4.4 client provider is in this category because its public DATA event/transport APIs expose opcode and payload length/offset but not FIN. Newer ESP-IDF 5+ client APIs expose the framing information needed for a FIN-aware provider.

A concrete native stack may forbid stop/clean-close calls from its own event callback task. In that case the provider must return an explicit state/unsupported result from such a call rather than deadlock or perform unsafe teardown. Provider instances must not be destroyed from inside their own native/portable callback path when the target stack forbids synchronous teardown there; their lifetime must cover callback completion.

Current ESPressio-Security `ITransportSecurityCarrier` protects application payloads and is not a TLS trust-store/client-certificate abstraction. Do not conflate those layers unless a future Security API explicitly defines that responsibility.

## DNS ownership

DNS policy remains in ESPressio-Web. A concrete DNS provider translates target UDP/DNS mechanics into the Web DNS request/response contracts; it must not hard-code captive-portal wildcard matching or application addresses. This keeps `WildcardDnsHandler` and other DNS policies portable and application-composable.
