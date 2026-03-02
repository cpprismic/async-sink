#pragma once

#include "../sink.hpp"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace logger {

/// @brief Sink that writes to a file and rotates it once it exceeds a size limit.
///
/// When the active log file would exceed @c max_bytes after the next write,
/// the rotation algorithm runs:
/// 1. Close the current file.
/// 2. Delete @c basename.{max_files}.ext (the oldest rotated file, if present).
/// 3. Rename @c basename.{i}.ext → @c basename.{i+1}.ext for i = max_files−1 … 1.
/// 4. Rename @c basename.ext → @c basename.1.ext.
/// 5. Open a fresh @c basename.ext for writing.
///
/// The active file size is tracked in @c current_bytes_ (incremented on every
/// write) rather than calling @c tellp() repeatedly, which avoids a syscall per
/// message.  The initial size is measured once at construction via @c seekp().
///
/// @par Example
/// @code
/// // Rotate "app.log" every 10 MB, keep at most 5 rotated copies.
/// auto sink = std::make_shared<logger::RotatingFileSink>("app.log", 10*1024*1024, 5);
/// @endcode
class RotatingFileSink : public Sink {
public:
    /// @brief Opens @p basename and prepares for size-based rotation.
    /// @param basename   Path to the active log file (e.g. @c "app.log").
    /// @param max_bytes  Maximum file size in bytes before a rotation is triggered.
    /// @param max_files  Number of rotated files to retain
    ///                   (e.g. 3 keeps @c app.1.log, @c app.2.log, @c app.3.log).
    RotatingFileSink(std::string basename, std::size_t max_bytes, std::size_t max_files)
        : basename_(std::move(basename))
        , max_bytes_(max_bytes)
        , max_files_(max_files) {
        OpenFile();
    }

    /// @brief Formats and writes @p msg, rotating the file beforehand if needed.
    /// @param msg  The log event to write.
    void Log(const LogMessage& msg) override {
        if (!file_.is_open()) return;
        std::string formatted = FormatMessage(msg);
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_bytes_ + formatted.size() + 1 > max_bytes_) {
            Rotate();
        }
        file_ << formatted << '\n';
        current_bytes_ += formatted.size() + 1;
    }

    /// @brief Flushes the active file's write buffer.
    void Flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }

private:
    /// @brief Opens @c basename_ in append mode and records its current size.
    void OpenFile() {
        file_.open(basename_, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            fprintf(stderr, "[logger] RotatingFileSink: failed to open '%s'\n",
                    basename_.c_str());
            return;
        }
        file_.seekp(0, std::ios::end);
        current_bytes_ = static_cast<std::size_t>(file_.tellp());
        if (file_.fail()) current_bytes_ = 0;
    }

    /// @brief Builds the rotated filename for index @p index.
    ///
    /// For @c "app.log" with index 1 → @c "app.1.log".
    /// For files without an extension → @c "app.1".
    /// @param index  The rotation index (1 = most recent, N = oldest).
    /// @return       The computed filename.
    std::string RotatedName(std::size_t index) const {
        auto dot = basename_.rfind('.');
        if (dot == std::string::npos) {
            return basename_ + '.' + std::to_string(index);
        }
        return basename_.substr(0, dot) + '.' + std::to_string(index)
               + basename_.substr(dot);
    }

    /// @brief Performs the full rotation sequence and reopens the base file.
    void Rotate() {
        file_.close();

        std::string oldest = RotatedName(max_files_);
        std::remove(oldest.c_str());

        for (std::size_t i = max_files_ - 1; i >= 1; --i) {
            std::string from = RotatedName(i);
            std::string to   = RotatedName(i + 1);
            std::rename(from.c_str(), to.c_str());
        }

        std::rename(basename_.c_str(), RotatedName(1).c_str());
        OpenFile();
    }

    std::string   basename_;          ///< Active file path used as base for rotated names
    std::size_t   max_bytes_;         ///< Size threshold (bytes) that triggers a rotation
    std::size_t   max_files_;         ///< Maximum number of rotated (archived) files to keep
    std::ofstream file_;              ///< Active output file stream
    std::size_t   current_bytes_{0};  ///< Bytes written to the current file (tracked manually)
    std::mutex    mutex_;             ///< Serialises writes and rotation
};

} // namespace logger
