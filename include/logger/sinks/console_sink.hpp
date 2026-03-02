#pragma once

#include "../sink.hpp"

#include <cstdio>
#include <mutex>

namespace logger {

/// @brief Sink that writes formatted log messages to the console.
///
/// Messages with level @c kWarning and above are directed to @c stderr;
/// all others go to @c stdout.  When ANSI colour mode is enabled each line
/// is wrapped with an escape sequence appropriate for its level, and the
/// colour is reset with @c \\033[0m at the end.
///
/// ANSI colour mapping
/// -------------------
/// | Level     | Colour          |
/// |-----------|-----------------|
/// | kTrace    | White (37)      |
/// | kDebug    | Cyan (36)       |
/// | kInfo     | Green (32)      |
/// | kWarning  | Yellow (33)     |
/// | kError    | Red (31)        |
/// | kCritical | Bold Red (1;31) |
///
/// Thread-safety
/// -------------
/// @c Log() and @c Flush() are guarded by an internal mutex so the sink can be
/// safely shared between multiple @c Logger instances.
class ConsoleSink : public Sink {
public:
    /// @brief Constructs a console sink.
    /// @param use_ansi_colors  When @c true (default), wrap output with ANSI
    ///                         colour escape codes.  Set to @c false when
    ///                         writing to a terminal that does not support ANSI,
    ///                         or when piping output to a file.
    explicit ConsoleSink(bool use_ansi_colors = true)
        : use_colors_(use_ansi_colors) {}

    /// @brief Writes @p msg to stdout or stderr with optional ANSI colour.
    /// @param msg  The log event to output.
    void Log(const LogMessage& msg) override {
        std::string formatted = FormatMessage(msg);
        FILE* target = (msg.level >= Level::kWarning) ? stderr : stdout;

        std::lock_guard<std::mutex> lock(mutex_);
        if (use_colors_) {
            fputs(AnsiColor(msg.level), target);
        }
        fputs(formatted.c_str(), target);
        fputc('\n', target);
        if (use_colors_) {
            fputs("\033[0m", target);
        }
    }

    /// @brief Flushes both @c stdout and @c stderr.
    void Flush() override {
        fflush(stdout);
        fflush(stderr);
    }

private:
    /// @brief Returns the ANSI escape code for the foreground colour of @p level.
    /// @param level  The level whose colour to retrieve.
    /// @return       A null-terminated escape sequence string, or @c "" for unknown levels.
    static const char* AnsiColor(Level level) noexcept {
        switch (level) {
            case Level::kTrace:    return "\033[37m";
            case Level::kDebug:    return "\033[36m";
            case Level::kInfo:     return "\033[32m";
            case Level::kWarning:  return "\033[33m";
            case Level::kError:    return "\033[31m";
            case Level::kCritical: return "\033[1;31m";
            default:               return "";
        }
    }

    bool       use_colors_; ///< When @c true, wrap each line with ANSI colour escape codes
    std::mutex mutex_;      ///< Serialises writes to @c stdout / @c stderr across threads
};

} // namespace logger
