#pragma once

#include "../sink.hpp"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace logger {

/// @brief Sink that appends formatted log messages to a single file.
///
/// The file is opened in the constructor.  If the open fails, a diagnostic is
/// printed to @c stderr and every subsequent @c Log() call becomes a no-op;
/// no exception is thrown (logging should never crash the host application).
///
/// Thread-safety
/// -------------
/// @c Log() and @c Flush() are guarded by an internal mutex so the sink can be
/// safely shared between multiple @c Logger instances.
class FileSink : public Sink {
public:
    /// @brief Opens @p path for writing.
    /// @param path    Path to the log file.
    /// @param append  When @c true (default), new messages are appended to an
    ///                existing file.  When @c false, the file is truncated on open.
    explicit FileSink(const std::string& path, bool append = true) {
        auto mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
        file_.open(path, mode);
        if (!file_.is_open()) {
            fprintf(stderr, "[logger] FileSink: failed to open '%s'\n", path.c_str());
        }
    }

    /// @brief Formats and appends @p msg to the file.
    ///
    /// Does nothing if the file failed to open.
    /// @param msg  The log event to write.
    void Log(const LogMessage& msg) override {
        if (!file_.is_open()) return;
        std::string formatted = FormatMessage(msg);
        std::lock_guard<std::mutex> lock(mutex_);
        file_ << formatted << '\n';
    }

    /// @brief Flushes the file stream's write buffer.
    void Flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }

private:
    std::ofstream file_;  ///< Output file stream; remains in a failed state if open failed
    std::mutex    mutex_; ///< Serialises concurrent @c Log() and @c Flush() calls
};

} // namespace logger
