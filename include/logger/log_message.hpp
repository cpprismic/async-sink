#pragma once

#include "level.hpp"

#include <chrono>
#include <string>
#include <thread>

#if __cplusplus >= 202002L
#include <source_location>
#endif

namespace logger {

/// @brief Immutable snapshot of a single log event passed from producers to sinks.
///
/// A @c LogMessage is constructed in the calling thread by @c Logger::Log() and
/// moved into the internal queue.  The worker thread then hands it to every
/// registered @c Sink.  All fields are set before the message enters the queue;
/// no field is modified afterwards.
struct LogMessage {
    Level                                 level{Level::kInfo}; ///< Severity of this event
    std::chrono::system_clock::time_point timestamp;           ///< Wall-clock time of the @c Log() call
    std::thread::id                       thread_id;           ///< ID of the thread that called @c Log()
    std::string                           logger_name;         ///< Name of the owning @c Logger instance
    std::string                           message;             ///< Fully composed text body of the message

#if __cplusplus >= 202002L
    std::source_location location; ///< Compile-time source location (C++20); captured automatically
#else
    const char* file     = nullptr; ///< Source file path supplied via macro (__FILE__)
    int         line     = 0;       ///< Source line number supplied via macro (__LINE__)
    const char* function = nullptr; ///< Enclosing function name supplied via macro (__func__)
#endif
};

} // namespace logger
