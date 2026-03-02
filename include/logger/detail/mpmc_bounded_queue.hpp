#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace logger::detail {

/// @brief Multiple-producer, single-consumer (MPSC) bounded ring queue.
///
/// @tparam T         Element type.  Must be move-constructible and move-assignable.
/// @tparam Capacity  Maximum number of elements.  **Must be a power of 2.**
///
/// Design notes
/// ------------
/// - **MPSC**: multiple producer threads may call @c TryPush() concurrently;
///   only one consumer thread may call @c TryPop() at a time.
/// - **Lock-free push**: producers compete for a slot with a CAS loop on
///   @c tail_.  After winning a slot the value is written and the per-slot
///   @c published_ flag is set with a release store, making the write visible
///   to the consumer.
/// - **Published flags**: because a producer may be preempted between the CAS
///   and the value write, @c TryPop() checks the @c published_ flag before
///   consuming a slot.  If the flag is not yet set, @c TryPop() returns @c false
///   (try again later) rather than returning a half-written element.
/// - **Cache-line isolation**: @c head_ and @c tail_ are placed on separate
///   64-byte cache lines with @c alignas(64) to eliminate false sharing between
///   producer threads (updating @c tail_) and the consumer thread (updating
///   @c head_).
///
/// Memory ordering
/// ---------------
/// | Operation               | Ordering                     |
/// |-------------------------|------------------------------|
/// | @c tail_ CAS (success)  | relaxed/relaxed (slot reservation only) |
/// | @c published_ store     | release (publishes the value write) |
/// | @c published_ load      | acquire (sees the value write) |
/// | @c head_ store          | release (publishes slot reclaim) |
/// | @c head_ load in push   | acquire (sees consumer's reclaim) |
template<typename T, std::size_t Capacity>
class MpscBoundedQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "MpscBoundedQueue: Capacity must be a power of 2");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "MpscBoundedQueue: T must be movable");

public:
    MpscBoundedQueue() = default;

    /// @brief Attempts to enqueue @p item.
    ///
    /// Safe to call concurrently from multiple threads.
    /// @param item  The value to enqueue (moved into the buffer).
    /// @return      @c true if @p item was enqueued; @c false if the queue is full.
    bool TryPush(T item) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        while (true) {
            std::size_t next_tail = (tail + 1) & mask_;
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;  // full
            }
            if (tail_.compare_exchange_weak(tail, next_tail,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
                break;
            }
            // Another producer claimed this slot; retry with the updated tail.
        }
        buffer_[tail] = std::move(item);
        published_[tail].store(true, std::memory_order_release);
        return true;
    }

    /// @brief Attempts to dequeue one item into @p out.
    ///
    /// Must be called from **at most one** thread at a time.
    /// @param out  Receives the dequeued value on success.
    /// @return     @c true if an item was dequeued; @c false if the queue is
    ///             empty or the head slot's publication flag is not yet set.
    bool TryPop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        // Guard against a producer that reserved the slot but hasn't written yet.
        if (!published_[head].load(std::memory_order_acquire)) {
            return false;
        }
        out = std::move(buffer_[head]);
        published_[head].store(false, std::memory_order_relaxed);
        head_.store((head + 1) & mask_, std::memory_order_release);
        return true;
    }

    /// @brief Returns @c true if the queue contains no items.
    bool Empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /// @brief Returns the approximate number of items currently in the queue.
    ///
    /// @note The result is a snapshot; it may be stale by the time the caller
    ///       uses it in a multi-threaded context.
    std::size_t Size() const noexcept {
        std::size_t t = tail_.load(std::memory_order_acquire);
        std::size_t h = head_.load(std::memory_order_acquire);
        return (t - h) & mask_;
    }

    static constexpr std::size_t kCapacity = Capacity; ///< Compile-time capacity of the queue

private:
    static constexpr std::size_t mask_ = Capacity - 1; ///< Bitmask for fast modulo (power-of-2)

    alignas(64) std::atomic<std::size_t> head_{0}; ///< Consumer read index; cache-line isolated
    alignas(64) std::atomic<std::size_t> tail_{0}; ///< Producer write index; cache-line isolated

    std::array<T, Capacity>                 buffer_{};    ///< Fixed-size element storage
    std::array<std::atomic<bool>, Capacity> published_{}; ///< Per-slot flags: true = slot is ready to consume
};

} // namespace logger::detail
