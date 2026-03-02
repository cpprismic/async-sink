#pragma once

#include "../sink.hpp"
#include "../time_utils.hpp"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace logger {

/// @brief Sink that writes to a new file each calendar day (local timezone).
///
/// The active file is named @c "{base_name}_{YYYY-MM-DD}{.ext}" where the date
/// is derived from the local OS timezone.  On each @c Log() call the message's
/// timestamp is compared against @c current_day_.  When the day changes the
/// current file is closed and a new one is opened automatically; no separate
/// timer thread is required.
///
/// @par Example
/// For @c base_name = @c "app.log":
/// - Day 1 → @c app_2024-01-15.log
/// - Day 2 → @c app_2024-01-16.log
///
/// @par Example (no extension)
/// For @c base_name = @c "app":
/// - Day 1 → @c app_2024-01-15
class DailyFileSink : public Sink {
public:
    /// @brief Opens the log file for the current day.
    /// @param base_name  Base file path, with or without an extension.
    explicit DailyFileSink(std::string base_name)
        : base_name_(std::move(base_name)) {
        auto tp = std::chrono::system_clock::now();
        OpenFileForDay(tp);
    }

    /// @brief Writes @p msg to the active daily file, rolling over if the day changed.
    /// @param msg  The log event to write.
    void Log(const LogMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::tm local = time_utils::LocalTime(msg.timestamp);
        int today = time_utils::YearDay(local);
        if (today != current_day_) {
            OpenFileForDay(msg.timestamp);
        }
        if (!file_.is_open()) return;
        file_ << FormatMessage(msg) << '\n';
    }

    /// @brief Flushes the active file's write buffer.
    void Flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.flush();
    }

private:
    /// @brief Builds the dated filename for the day containing @p tp.
    /// @param tp  A time point within the target day.
    /// @return    A path of the form @c "{base}_YYYY-MM-DD{.ext}".
    std::string BuildFilename(std::chrono::system_clock::time_point tp) const {
        std::tm local = time_utils::LocalTime(tp);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "_%04d-%02d-%02d",
            time_utils::Year(local), time_utils::Month(local), time_utils::Day(local));
        auto dot = base_name_.rfind('.');
        if (dot == std::string::npos) {
            return base_name_ + buf;
        }
        return base_name_.substr(0, dot) + buf + base_name_.substr(dot);
    }

    /// @brief Closes the current file and opens a new one for the day of @p tp.
    /// @param tp  A time point within the day to open.
    void OpenFileForDay(std::chrono::system_clock::time_point tp) {
        file_.close();
        std::tm local = time_utils::LocalTime(tp);
        current_day_ = time_utils::YearDay(local);
        std::string path = BuildFilename(tp);
        file_.open(path, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            fprintf(stderr, "[logger] DailyFileSink: failed to open '%s'\n", path.c_str());
        }
    }

    std::string   base_name_;       ///< Base file path used to construct dated filenames
    std::ofstream file_;            ///< Active output file stream for the current day
    int           current_day_{-1}; ///< Day-of-year of the open file; -1 = uninitialised
    std::mutex    mutex_;           ///< Serialises writes, day checks, and file rollovers
};

} // namespace logger
