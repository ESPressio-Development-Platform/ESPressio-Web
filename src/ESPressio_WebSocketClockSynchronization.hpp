#pragma once

#if !__has_include(<ESPressio_SocketClockSynchronizationProtocol.hpp>)
#error "WebSocket clock synchronization requires ESPressio-Sockets clock protocol support."
#endif
#if !__has_include(<ESPressio_PrecisionThread.hpp>)
#error "WebSocket clock synchronization requires ESPressio-Threads for periodic client scheduling."
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_SocketClockSynchronizationProtocol.hpp>

#include "ESPressio_WebSocketClient.hpp"
#include "ESPressio_WebSocketEndpoint.hpp"

namespace ESPressio::Web {

/**
 * ESPressio Memory Audit
 * Members:
 * - Protocol (Sockets::SocketClockSynchronizationConfig): 16 bytes [0 bytes dynamic allocation]
 * - AutomaticSynchronization (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 20 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebSocketClockSynchronizationClientConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
    bool AutomaticSynchronization = true;
};

/**
 * ESPressio Memory Audit
 * Members:
 * - Protocol (Sockets::SocketClockSynchronizationConfig): 16 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct WebSocketClockSynchronizationServerConfiguration final {
    Sockets::SocketClockSynchronizationConfig Protocol;
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _client (WebSocketClient*): 4 bytes [0 bytes dynamic allocation]
 * - _configuration (WebSocketClockSynchronizationClientConfiguration): 20 bytes [0 bytes dynamic allocation]
 * - _protocol (Sockets::SocketClockSynchronizationProtocol): 28 bytes [0 bytes dynamic allocation]
 * - _thread (SynchronizationThread): 360 bytes [PrecisionThread: Thread: _taskExited: owned object: 4 bytes; PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _scheduleSignal: owned object: 4 bytes; PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers]
 * - _observerHandle (Observable::ObserverHandlePtr): 12 bytes [owned object: 4 bytes]
 * Total Memory: 428 bytes [_thread: PrecisionThread: Thread: _taskExited: owned object: 4 bytes; _thread: PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; _thread: PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; _thread: PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: _scheduleSignal: owned object: 4 bytes; _thread: PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; _thread: PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; _thread: PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers; _observerHandle: owned object: 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketClockSynchronizationClient final :
    private IWebSocketClientObserver {
private:
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 356 bytes [PrecisionThread: Thread: _taskExited: owned object: 4 bytes; PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _scheduleSignal: owned object: 4 bytes; PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers]
 * Requires Stack/Heap Preallocation
 * Members:
 * - _owner (WebSocketClockSynchronizationClient&): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 360 bytes [PrecisionThread: Thread: _taskExited: owned object: 4 bytes; PrecisionThread: Thread: _taskStartGate: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _taskConfigurationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _stateTransitionMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _stateTransitionMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _lifecycleObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _callbackMutex: _owned: owned object: 4 bytes; PrecisionThread: Thread: _callbackMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: Thread: _onDestroy: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitialize: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStart: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onPause: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminate: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onTerminated: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onInitializationFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onExecutionFailed: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: Thread: _onStateChange: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; PrecisionThread: _iterationObservable: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 96 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _registrations: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: Observable: _bindings: Capacity * (12 bytes) element storage; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _mutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _owned: owned object: 4 bytes; PrecisionThread: _iterationObservable: pointee: ThreadSafeObservable: _notificationMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _scheduleSignal: owned object: 4 bytes; PrecisionThread: _timingMutex: _owned: owned object: 4 bytes; PrecisionThread: _timingMutex: _fallback: _mutex: native synchronization state may allocate platform resources lazily; PrecisionThread: _iterationSamples: implementation blocks containing N * (8 bytes) plus block-map pointers]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class SynchronizationThread final : public Threads::PrecisionThread<> {
    public:
        explicit SynchronizationThread(WebSocketClockSynchronizationClient& owner)
            : _owner(owner) {}

    protected:
        void Iterate(
            IterationTime,
            IterationTime,
            Threads::SkippedIterationCount
        ) override {
            (void)_owner.RequestSynchronization();
        }

    private:
        WebSocketClockSynchronizationClient& _owner;
    };

public:
    explicit WebSocketClockSynchronizationClient(
        Timing::IClockSynchronizationTarget<Timing::ClockTick>* target = nullptr
    ) : _protocol(target), _thread(*this) {}

    ~WebSocketClockSynchronizationClient() { Detach(); }

    WebSocketClockSynchronizationClient(const WebSocketClockSynchronizationClient&) = delete;
    WebSocketClockSynchronizationClient& operator=(const WebSocketClockSynchronizationClient&) = delete;

    WebResult Attach(
        WebSocketClient& client,
        WebSocketClockSynchronizationClientConfiguration configuration = {}
    ) {
        if (_client == &client) return WebResult::Success();
        if (_client != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Client;
        _configuration = configuration;
        _protocol.Configure(_configuration.Protocol);
        _client = &client;
        _observerHandle = client.RegisterObserver(this);

        if (
            _configuration.AutomaticSynchronization &&
            _configuration.Protocol.SynchronizationIntervalMilliseconds > 0
        ) {
            _thread.SetIterationPeriod(
                Units::MilliSeconds<uint64_t>(
                    _configuration.Protocol.SynchronizationIntervalMilliseconds
                )
            );
            const auto status = _thread.Start();
            if (
                status != Threads::ThreadInitializationStatus::Success &&
                status != Threads::ThreadInitializationStatus::AlreadyInitialized
            ) {
                Detach();
                return WebResult::Failure(WebError::ResourceExhausted);
            }
        }

        return WebResult::Success();
    }

    void Detach() {
        _thread.Shutdown();
        _observerHandle.reset();
        _client = nullptr;
        _protocol.CancelPendingRequest();
    }

    bool RequestSynchronization() {
        auto* client = _client;
        if (client == nullptr) return false;
        auto* connection = client->Connection();
        if (connection == nullptr || !connection->IsOpen()) return false;

        Sockets::SocketClockSynchronizationProtocol::RequestMessage request;
        if (!_protocol.BuildRequest(request)) return false;

        const auto result = connection->SendBinary(
            reinterpret_cast<const uint8_t*>(&request),
            sizeof(request)
        );
        if (!result) _protocol.CancelPendingRequest();
        return static_cast<bool>(result);
    }

    Timing::ClockSynchronizationStatus<Timing::ClockTick>
    GetSynchronizationStatus() const {
        return _protocol.GetSynchronizationStatus();
    }

private:
    WebSocketClient* _client = nullptr;
    WebSocketClockSynchronizationClientConfiguration _configuration;
    Sockets::SocketClockSynchronizationProtocol _protocol;
    SynchronizationThread _thread;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketClientBinary(
        IWebSocketConnection&,
        const uint8_t* data,
        std::size_t size
    ) override {
        if (
            data == nullptr ||
            size < sizeof(Sockets::SocketClockSynchronizationProtocol::MessageHeader)
        ) {
            return;
        }

        Sockets::SocketClockSynchronizationProtocol::MessageHeader header;
        std::memcpy(&header, data, sizeof(header));
        if (
            header.Magic != Sockets::SocketClockSynchronizationProtocol::MessageHeader::MagicValue ||
            header.Version != 1
        ) {
            return;
        }

        const uint64_t receiveTime = _protocol.GetLocalTimestamp();
        const auto type = static_cast<Sockets::SocketClockSynchronizationProtocol::MessageType>(
            header.Type
        );

        if (type == Sockets::SocketClockSynchronizationProtocol::MessageType::Response) {
            (void)_protocol.ProcessResponse(data, size, receiveTime);
        } else if (
            type == Sockets::SocketClockSynchronizationProtocol::MessageType::AuthoritativeBroadcast
        ) {
            (void)_protocol.ProcessAuthoritativeBroadcast(data, size, receiveTime);
        }
    }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members:
 * - _endpoint (WebSocketEndpoint*): 4 bytes [0 bytes dynamic allocation]
 * - _configuration (WebSocketClockSynchronizationServerConfiguration): 16 bytes [0 bytes dynamic allocation]
 * - _protocol (Sockets::SocketClockSynchronizationProtocol): 28 bytes [0 bytes dynamic allocation]
 * - _observerHandle (Observable::ObserverHandlePtr): 12 bytes [owned object: 4 bytes]
 * Total Memory: 64 bytes [_observerHandle: owned object: 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class WebSocketClockSynchronizationServer final :
    private IWebSocketEndpointObserver {
public:
    explicit WebSocketClockSynchronizationServer(
        Timing::IClockSynchronizationTarget<Timing::ClockTick>* target = nullptr
    ) : _protocol(target) {}

    ~WebSocketClockSynchronizationServer() { Detach(); }

    WebSocketClockSynchronizationServer(const WebSocketClockSynchronizationServer&) = delete;
    WebSocketClockSynchronizationServer& operator=(const WebSocketClockSynchronizationServer&) = delete;

    WebResult Attach(
        WebSocketEndpoint& endpoint,
        WebSocketClockSynchronizationServerConfiguration configuration = {}
    ) {
        if (_endpoint == &endpoint) return WebResult::Success();
        if (_endpoint != nullptr) return WebResult::Failure(WebError::InvalidState);

        configuration.Protocol.Mode = Sockets::SocketClockSynchronizationMode::Reference;
        _configuration = configuration;
        _protocol.Configure(_configuration.Protocol);
        _endpoint = &endpoint;
        _observerHandle = endpoint.RegisterObserver(this);
        return WebResult::Success();
    }

    void Detach() {
        _observerHandle.reset();
        _endpoint = nullptr;
    }

    Timing::ClockSynchronizationStatus<Timing::ClockTick>
    GetSynchronizationStatus() const {
        return _protocol.GetSynchronizationStatus();
    }

private:
    WebSocketEndpoint* _endpoint = nullptr;
    WebSocketClockSynchronizationServerConfiguration _configuration;
    Sockets::SocketClockSynchronizationProtocol _protocol;
    Observable::ObserverHandlePtr _observerHandle;

    void OnWebSocketBinary(
        IWebSocketConnection& connection,
        const uint8_t* data,
        std::size_t size
    ) override {
        const uint64_t receiveTime = _protocol.GetLocalTimestamp();
        (void)_protocol.ProcessRequest(
            data,
            size,
            receiveTime,
            [&](const uint8_t* response, std::size_t responseSize) {
                return static_cast<bool>(
                    connection.SendBinary(response, responseSize)
                );
            }
        );
    }
};

} // namespace ESPressio::Web
