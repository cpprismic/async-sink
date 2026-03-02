#pragma once

#include "../formatter.hpp"
#include "../time_utils.hpp"

#include <sstream>
#include <string_view>

namespace logger {

/// @brief Formatter that serialises a @c LogMessage as a single-line JSON object.
///
/// The output is suitable for ingestion by log aggregators such as ELK Stack or
/// Loki.  No external JSON library is required; the object is built by manual
/// string concatenation with proper escaping of the message field.
///
/// @par Output schema
/// @code{.json}
/// {
///   "timestamp": "2024-01-15T14:30:25Z",
///   "level":     "INFO",
///   "logger":    "my-service",
///   "thread":    "140234567890",
///   "message":   "User logged in",
///   "file":      "auth.cpp",      // C++20 / macro-provided only
///   "line":      42               // C++20 / macro-provided only
/// }
/// @endcode
///
/// @note The @c file and @c line fields are emitted only when source-location
///       information is available (C++20 @c std::source_location or explicit
///       @c __FILE__ / @c __LINE__ passed via the logging macros).
///
/// @see PatternFormatter
class JsonFormatter : public Formatter {
public:
    /// @brief Converts @p msg to a single-line JSON string.
    /// @param msg  The log event to serialise.
    /// @return     A JSON object string without a trailing newline.
    std::string Format(const LogMessage& msg) override {
        std::string out;
        out.reserve(256);
        out += "{\"timestamp\":\"";
        out += time_utils::FormatISO8601(msg.timestamp);
        out += "\",\"level\":\"";
        out += LevelToString(msg.level);
        out += "\",\"logger\":\"";
        out += Escape(msg.logger_name);
        out += "\",\"thread\":\"";
        {
            std::ostringstream oss;
            oss << msg.thread_id;
            out += oss.str();
        }
        out += "\",\"message\":\"";
        out += Escape(msg.message);
        out += "\"";
#if __cplusplus >= 202002L
        if (msg.location.file_name() && msg.location.file_name()[0] != '\0') {
            out += ",\"file\":\"";
            out += Escape(msg.location.file_name());
            out += "\",\"line\":";
            out += std::to_string(msg.location.line());
        }
#else
        if (msg.file) {
            out += ",\"file\":\"";
            out += Escape(msg.file);
            out += "\",\"line\":";
            out += std::to_string(msg.line);
        }
#endif
        out += '}';
        return out;
    }

    /// @brief Returns a deep copy of this formatter.
    std::unique_ptr<Formatter> Clone() const override {
        return std::make_unique<JsonFormatter>();
    }

private:
    /// @brief Escapes JSON special characters in @p s.
    ///
    /// Handles @c ", @c \\, @c \\n, @c \\r, and @c \\t.
    /// @param s  The raw string to escape.
    /// @return   A copy of @p s with special characters replaced by escape sequences.
    static std::string Escape(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    }
};

} // namespace logger
