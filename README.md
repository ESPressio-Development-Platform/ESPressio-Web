# ESPressio Web

Platform-neutral Web services for the ESPressio Development Platform.

ESPressio-Web owns Web-domain vocabulary and behaviour: HTTP request/response semantics, routing, middleware, static-resource resolution, WebSocket lifecycle and sessions, DNS service abstractions, capability reporting, and optional adapters for ESPressio primitives. Target-specific implementations belong in ESPressio-ESP32 or equivalent architecture packages.

## Architectural rules

- Applications always choose every published HTTP/WebSocket route. ESPressio never imposes application URL conventions.
- HTTP request contexts are borrowed and synchronous; a handler completes its response before returning.
- Request metadata uses lazy lookup where practical to avoid unnecessary heap pressure.
- Request bodies and responses are streaming-oriented and bounded.
- Route registration and removal are supported at runtime.
- Unmatched HTTP routes may fall through to a configured static-resource provider.
- Filesystem access is owned by ESPressio-Persistence; Web owns only URI/resource semantics.
- Observable supplies synchronous lifecycle/state observation; optional bridges/adapters layer Event, State, Command and other primitives above Web.
- Providers/services may inspect the full request, including Content-Type and Accept, before consuming or producing representations.
- WebSocket protocol ownership resides in ESPressio-Web; ESPressio-Sockets remains responsible for non-Web generic socket technologies and transport-neutral protocol machinery.

## Examples

Examples are intentionally split by audience:

- `examples/Implementors/` — how to satisfy ESPressio-Web platform contracts when onboarding a new architecture.
- `examples/ESP32/` — how an application consumes the supported ESP32 concrete implementation.

## Development

The active tranche is documented chronologically in `ESPRESSIO_WEB_CORE_DEVELOPMENT.md`.
