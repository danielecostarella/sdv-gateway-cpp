# ADR-002: Lock-Free SPSC Queue Between Transport and Signal Layers

**Status:** Accepted  
**Date:** 2026-04-06

## Context

The gateway pipeline has two distinct execution contexts:

- **Transport thread** — reads raw CAN frames from the kernel via SocketCAN.
  This is a tight, time-sensitive loop; any blocking here causes frame drops.
- **Signal thread** — decodes frames, maps them to VSS paths, and enqueues
  values for the gRPC client. This involves JSON config lookups and value
  transformations.

A handoff mechanism is needed between the two threads.

Candidates:

1. **Mutex-protected `std::queue`** — simple, but introduces lock contention
   and unpredictable latency spikes on the transport thread.
2. **`std::condition_variable` + queue** — adds wake-up latency and still
   requires a mutex.
3. **Lock-free SPSC (Single-Producer Single-Consumer) ring buffer** — no
   mutex, no heap allocation on the hot path, bounded memory, deterministic
   latency.
4. **Direct synchronous call** — couples layers, blocks the CAN reader on
   gRPC back-pressure.

## Decision

Use a **lock-free SPSC ring buffer** (`src/SpscQueue.hpp`) between the
Transport and Signal layers.

The queue is sized at compile time (power-of-two capacity). If full, the
transport thread drops the frame and increments a `frames_dropped` counter
(observable via metrics). No blocking, no allocation.

The Signal→Service handoff (decoded VSS values → gRPC client) uses a
separate `std::queue` protected by a mutex, since gRPC back-pressure
already introduces non-deterministic latency at that boundary — the
complexity of a second lock-free queue is not justified there.

## Rationale

- **Determinism on the hot path:** the CAN reader must never block on
  downstream back-pressure. Lock-free ensures the transport thread remains
  at a predictable, bounded execution time per frame.
- **No heap allocation:** a fixed-capacity ring buffer avoids `malloc` on
  the hot path, which is a hard requirement in safety-relevant automotive
  software (aligned with MISRA C++ and AUTOSAR Adaptive guidelines).
- **Single producer, single consumer:** the SPSC constraint (one writer
  thread, one reader thread) removes the need for atomic CAS loops, making
  the implementation simpler and faster than an MPMC queue.

## Consequences

- Queue capacity is a configuration constant; sizing must account for burst
  traffic (e.g., 100 ms of frames at maximum CAN bus utilization).
- Dropped frames are counted and exposed as a metric — operators can detect
  undersizing in production without log spam.
- The SPSC assumption must be upheld: only one thread may call `push()` and
  only one thread may call `pop()` at any time.
