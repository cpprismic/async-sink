#pragma once

#include "../sink.hpp"

#include <deque>
#include <mutex>
#include <string_view>
#include <vector>

namespace logger {

/// @brief An in-memory sink that retains the most recent @c N log messages.
///
/// Each entry stores both the raw @c LogMessage and the formatted string
/// produced by the attached @c Formatter.  When the buffer is full the oldest
/// entry is evicted (ring-buffer semantics).
///
/// Thread-safety
/// -------------
/// @c Log() is called exclusively by the @c Logger worker thread, but
/// @c GetMessages(), @c Contains(), @c Size(), and @c Clear() may be called
/// from any thread.  All accesses are serialised by an internal mutex.
///
/// Primary use case
/// ----------------
/// Assertion-based testing: attach a @c RingBufferSink, exercise the code
/// under test, then call @c Contains() to verify expected output.
///
/// @code
/// auto sink = std::make_shared<logger::RingBufferSink>(64);
/// logger.AddSink(sink);
/// MyFunctionUnderTest();
/// logger.Flush();
/// assert(sink->Contains("connection established"));
/// @endcode
class RingBufferSink : public Sink {
public:
    /// @brief Constructs a ring buffer with the given maximum @p capacity.
    /// @param capacity  Maximum number of entries retained; oldest are evicted
    ///                  when the limit is reached (default: 128).
    explicit RingBufferSink(std::size_t capacity = 128)
        : capacity_(capacity) {}

    /// @brief Records @p msg, evicting the oldest entry if the buffer is full.
    /// @param msg  The log event to store.
    void Log(const LogMessage& msg) override {
        std::string formatted = FormatMessage(msg);
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.size() >= capacity_) {
            entries_.pop_front();
        }
        entries_.push_back({msg, std::move(formatted)});
    }

    /// @brief No-op; all data is already in memory.
    void Flush() override {}

    /// @brief Returns a snapshot of all currently retained raw @c LogMessage objects.
    /// @return A vector of messages ordered oldest-first.
    std::vector<LogMessage> GetMessages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<LogMessage> out;
        out.reserve(entries_.size());
        for (const auto& e : entries_) out.push_back(e.msg);
        return out;
    }

    /// @brief Returns @c true if any retained entry contains @p substr.
    ///
    /// Both the formatted string and the raw message body are searched.
    /// @param substr  The substring to look for.
    bool Contains(std::string_view substr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : entries_) {
            if (e.formatted.find(substr) != std::string::npos) return true;
            if (e.msg.message.find(substr) != std::string::npos) return true;
        }
        return false;
    }

    /// @brief Returns the number of entries currently in the buffer.
    std::size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    /// @brief Removes all entries from the buffer.
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    /// @brief Combines the raw @c LogMessage with its formatted representation.
    struct Entry {
        LogMessage  msg;          ///< Raw log event (level, timestamp, thread, text, …)
        std::string formatted;    ///< Output of the attached @c Formatter for this event
    };

    std::size_t        capacity_; ///< Maximum number of entries before eviction
    std::deque<Entry>  entries_;  ///< Retained entries, oldest at front
    mutable std::mutex mutex_;    ///< Guards @c entries_; mutable to allow const accessor locking
};

} // namespace logger
