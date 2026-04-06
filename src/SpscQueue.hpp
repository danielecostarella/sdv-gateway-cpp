#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace sdvgw {

/// Lock-free single-producer single-consumer ring buffer.
///
/// Capacity must be a power of two. Each end of the queue owns exactly
/// one atomic index — no mutex, no CAS loop, no heap allocation.
///
/// Thread-safety contract:
///   push() — called exclusively by the producer thread
///   pop()  — called exclusively by the consumer thread
///
/// When the queue is full, push() returns false and the caller is
/// responsible for incrementing a dropped-frame counter (see main.cpp).
///
/// Reference: Dmitry Vyukov's SPSC queue pattern.
template<typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

    static constexpr std::size_t kMask = Capacity - 1;

public:
    /// Enqueue an item. Returns false (non-blocking) if the queue is full.
    bool push(const T& item) noexcept
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & kMask;

        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }

        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Dequeue an item into out. Returns false if the queue is empty.
    bool pop(T& out) noexcept
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }

        out = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept
    {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    std::array<T, Capacity> buffer_;

    // Placed on separate cache lines to prevent false sharing between
    // the producer and consumer threads.
    alignas(64) std::atomic<std::size_t> head_{0};  ///< written by producer
    alignas(64) std::atomic<std::size_t> tail_{0};  ///< written by consumer
};

} // namespace sdvgw
