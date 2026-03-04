#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

/// @brief Thread-safe time conversion and formatting utilities used internally
///        by formatters and sinks.
///
/// All functions are @c inline to comply with the header-only library design.
/// On POSIX systems the reentrant variants @c localtime_r / @c gmtime_r are
/// used; on other platforms the standard @c std::localtime / @c std::gmtime
/// are used as a fallback (which may share a static buffer).

namespace logger::time_utils {

/// @brief Converts @p tp to a @c std::tm in the local (OS) timezone.
///
/// Uses @c localtime_r on POSIX for thread safety.
/// @param tp  A system-clock time point to convert.
/// @return    A @c std::tm structure filled with local-time fields.
inline std::tm LocalTime(std::chrono::system_clock::time_point tp) noexcept {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm result{};
#ifdef _POSIX_VERSION
    localtime_r(&t, &result);
#else
    result = *std::localtime(&t);
#endif
    return result;
}

/// @brief Converts @p tp to a @c std::tm in UTC.
///
/// Uses @c gmtime_r on POSIX for thread safety.
/// @param tp  A system-clock time point to convert.
/// @return    A @c std::tm structure filled with UTC fields.
inline std::tm UtcTime(std::chrono::system_clock::time_point tp) noexcept {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm result{};
#ifdef _POSIX_VERSION
    gmtime_r(&t, &result);
#else
    result = *std::gmtime(&t);
#endif
    return result;
}

/// @brief Formats @p tp as an ISO 8601 UTC timestamp string.
/// @param tp  The time point to format.
/// @return    A string of the form @c "2024-01-15T14:30:25Z".
inline std::string FormatISO8601(std::chrono::system_clock::time_point tp) {
    std::tm utc = UtcTime(tp);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/// @brief Formats @p tp as a local-time string without timezone suffix.
/// @param tp  The time point to format.
/// @return    A string of the form @c "2024-01-15 14:30:25" in the OS timezone.
inline std::string FormatLocal(std::chrono::system_clock::time_point tp) {
    std::tm local = LocalTime(tp);
    std::ostringstream oss;
    oss << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/// @name Individual @c std::tm field accessors
/// Thin wrappers that hide the @c tm_year/tm_mon offset conventions.
/// @{

/// @brief Returns the four-digit year (e.g. 2024) from @p tm.
inline int Year(const std::tm& tm)    noexcept { return tm.tm_year + 1900; }

/// @brief Returns the month in range [1, 12] from @p tm.
inline int Month(const std::tm& tm)   noexcept { return tm.tm_mon + 1; }

/// @brief Returns the day of the month in range [1, 31] from @p tm.
inline int Day(const std::tm& tm)     noexcept { return tm.tm_mday; }

/// @brief Returns the hour in range [0, 23] from @p tm.
inline int Hour(const std::tm& tm)    noexcept { return tm.tm_hour; }

/// @brief Returns the minute in range [0, 59] from @p tm.
inline int Minute(const std::tm& tm)  noexcept { return tm.tm_min; }

/// @brief Returns the second in range [0, 60] from @p tm (60 for leap seconds).
inline int Second(const std::tm& tm)  noexcept { return tm.tm_sec; }

/// @brief Returns the day of the year in range [0, 365] from @p tm.
///        Used by @c DailyFileSink to detect day boundaries.
inline int YearDay(const std::tm& tm) noexcept { return tm.tm_yday; }

/// @}

} // namespace logger::time_utils
