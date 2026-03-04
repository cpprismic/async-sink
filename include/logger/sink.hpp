#pragma once

#include "log_message.hpp"
#include "formatter.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace logger {

/// @brief Abstract base class for log output destinations.
///
/// A @c Sink receives formatted @c LogMessage objects from the @c Logger worker
/// thread and writes them to some output medium (file, console, ring buffer, …).
///
/// Thread-safety contract
/// ----------------------
/// The @c Logger worker thread is the **sole** caller of @c Log().  Subclasses
/// do not need to serialise @c Log() calls against each other.  However, if a
/// sink exposes additional reader APIs callable from other threads (e.g.
/// @c RingBufferSink::GetMessages()), those must use their own synchronisation.
///
/// Level filtering
/// ---------------
/// Every sink has an independent level threshold (default: @c kTrace, i.e.
/// accept everything).  The @c Logger's own level acts as a primary gate before
/// messages reach the queue; per-sink level is a secondary refinement applied
/// in @c Logger::DispatchMessage().
///
/// @see FileSink, ConsoleSink, RingBufferSink, NullSink
class Sink {
public:
    virtual ~Sink() = default;

    /// @brief Writes @p msg to the underlying output medium.
    /// @param msg  The log event to record.
    virtual void Log(const LogMessage& msg) = 0;

    /// @brief Flushes any internally buffered data to the output medium.
    virtual void Flush() = 0;

    /// @brief Returns the current minimum level accepted by this sink.
    Level GetLevel() const noexcept {
        return level_.load(std::memory_order_relaxed);
    }

    /// @brief Sets the minimum level accepted by this sink.
    /// @param level  Messages below this level are silently dropped in @c Log().
    void SetLevel(Level level) noexcept {
        level_.store(level, std::memory_order_relaxed);
    }

    /// @brief Returns @c true if @p level meets or exceeds this sink's threshold.
    /// @param level  The level of an incoming message.
    bool ShouldLog(Level level) const noexcept {
        return level >= level_.load(std::memory_order_relaxed);
    }

    /// @brief Attaches a @c Formatter to this sink.
    /// @param fmt  The formatter to use; ownership is transferred.
    void SetFormatter(std::unique_ptr<Formatter> fmt) {
        formatter_ = std::move(fmt);
    }

    /// @brief Returns @c true if a formatter has been attached to this sink.
    bool HasFormatter() const noexcept { return formatter_ != nullptr; }

protected:
    /// @brief Formats @p msg using the attached formatter.
    ///
    /// If no formatter has been attached, returns the raw @c msg.message string.
    /// @param msg  The log event to format.
    /// @return     The formatted string produced by the formatter.
    std::string FormatMessage(const LogMessage& msg) {
        if (formatter_) return formatter_->Format(msg);
        return msg.message;
    }

    std::atomic<Level>         level_{Level::kTrace};  ///< Minimum accepted level; kTrace = pass all
    std::unique_ptr<Formatter> formatter_;             ///< Attached formatter; set by Logger::AddSink()
};

} // namespace logger
