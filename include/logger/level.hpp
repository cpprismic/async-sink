#pragma once

#include <cstdint>
#include <string_view>

namespace logger {

/// @brief Severity levels for log messages, ordered from least to most severe.
///
/// @note kOff is a sentinel value used to disable a sink or logger entirely.
///       It must always remain the highest numeric value.
enum class Level : uint8_t {
    kTrace    = 0, ///< Fine-grained diagnostic events; highest verbosity
    kDebug    = 1, ///< Debugging information useful during development
    kInfo     = 2, ///< General operational messages about normal execution
    kWarning  = 3, ///< Potentially harmful situations that do not block execution
    kError    = 4, ///< Runtime errors that allow the application to continue
    kCritical = 5, ///< Severe errors that may cause premature termination
    kOff      = 6  ///< Sentinel: disables all output; never emitted as a real level
};

/// @brief Policy applied when the logger's internal queue is full.
enum class OverflowPolicy : uint8_t {
    kBlock,         ///< Block the calling thread until space is available (default)
    kDiscardNew,    ///< Silently drop the incoming message and return immediately
    kDiscardOldest  ///< Evict the oldest queued message to make room for the new one
};

/// @brief Returns the human-readable name of @p level.
/// @param level The severity level to convert.
/// @return A non-owning string view of the level name (e.g. @c "WARNING").
///         Returns @c "UNKNOWN" for out-of-range values.
inline std::string_view LevelToString(Level level) noexcept {
    switch (level) {
        case Level::kTrace:    return "TRACE";
        case Level::kDebug:    return "DEBUG";
        case Level::kInfo:     return "INFO";
        case Level::kWarning:  return "WARNING";
        case Level::kError:    return "ERROR";
        case Level::kCritical: return "CRITICAL";
        case Level::kOff:      return "OFF";
    }
    return "UNKNOWN";
}

/// @brief Returns a single uppercase character abbreviation for @p level.
/// @param level The severity level to convert.
/// @return One of @c 'T', @c 'D', @c 'I', @c 'W', @c 'E', @c 'C', @c '-',
///         or @c '?' for out-of-range values.
inline char LevelToChar(Level level) noexcept {
    switch (level) {
        case Level::kTrace:    return 'T';
        case Level::kDebug:    return 'D';
        case Level::kInfo:     return 'I';
        case Level::kWarning:  return 'W';
        case Level::kError:    return 'E';
        case Level::kCritical: return 'C';
        case Level::kOff:      return '-';
    }
    return '?';
}

} // namespace logger
