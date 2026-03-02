#pragma once

#include "log_message.hpp"

#include <memory>
#include <string>

namespace logger {

/// @brief Abstract base class for log message formatters.
///
/// A @c Formatter converts a @c LogMessage into a printable string.
/// Each @c Sink owns exactly one @c Formatter, assigned either explicitly via
/// @c Sink::SetFormatter() or automatically by @c Logger::AddSink() using the
/// default @c PatternFormatter.
///
/// Subclasses must implement both @c Format() and @c Clone().  @c Clone() is
/// required so that a single formatter prototype can be duplicated when the
/// same formatter is shared across multiple sinks.
///
/// @see PatternFormatter, JsonFormatter
class Formatter {
public:
    virtual ~Formatter() = default;

    /// @brief Converts @p msg into a formatted string ready for output.
    /// @param msg  The log event to format.
    /// @return     The formatted text (without a trailing newline).
    virtual std::string Format(const LogMessage& msg) = 0;

    /// @brief Creates a deep copy of this formatter.
    /// @return A heap-allocated clone with identical configuration.
    virtual std::unique_ptr<Formatter> Clone() const = 0;
};

} // namespace logger
