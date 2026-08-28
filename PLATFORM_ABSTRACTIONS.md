# ESPressio-Web Platform Abstractions

ESPressio-Web is platform-neutral. It must not include ESP32, Arduino, ESP-IDF, FreeRTOS, lwIP, or target filesystem types in its public domain API.

The portable library owns Web semantics and decomposed platform contracts. Concrete implementations translate target facilities into those contracts in architecture packages such as ESPressio-ESP32.

Current planned capability families:

- HTTP server lifecycle and request dispatch
- HTTP request metadata/body reading
- HTTP response writing/streaming
- WebSocket endpoint/session lifecycle and frame I/O
- DNS server lifecycle and request/response handling
- HTTPS/TLS capability configuration hooks

Native enums, handles, errors, task types, filesystem handles and socket descriptors must not cross the abstraction boundary.
