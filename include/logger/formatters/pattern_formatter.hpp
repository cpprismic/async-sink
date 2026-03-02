#pragma once

#include "../formatter.hpp"
#include "../time_utils.hpp"

#include <sstream>
#include <vector>
#include <iomanip>

namespace logger {

/// @brief Formatter that converts a @c LogMessage to a string using a
///        @c printf-style pattern of @c % escape sequences.
///
/// The pattern is compiled into a token sequence **once** in the constructor,
/// so @c Format() performs zero string-scanning at runtime.
///
/// Supported escape sequences
/// --------------------------
/// | Escape | Output                                   |
/// |--------|------------------------------------------|
/// | @c %Y  | Four-digit year (e.g. @c 2024)           |
/// | @c %m  | Two-digit month (01–12)                  |
/// | @c %d  | Two-digit day of month (01–31)           |
/// | @c %H  | Two-digit hour in 24-h format (00–23)    |
/// | @c %M  | Two-digit minute (00–59)                 |
/// | @c %S  | Two-digit second (00–60)                 |
/// | @c %t  | Thread ID                                |
/// | @c %l  | Level name (e.g. @c WARNING)             |
/// | @c %L  | Level initial character (e.g. @c W)      |
/// | @c %v  | Message text                             |
/// | @c %n  | Logger name                              |
/// | @c %@  | Source file and line (e.g. @c main.cpp:42) |
/// | @c %!  | Enclosing function name                  |
/// | @c %%  | Literal @c % character                   |
///
/// @note Time components are taken from the **local** (OS) timezone via
///       @c time_utils::LocalTime().  No timezone offset is hardcoded.
///
/// @par Default pattern
/// @code
/// "[%Y-%m-%d %H:%M:%S] [%l] [%n] [%t] %v"
/// @endcode
///
/// @see JsonFormatter
class PatternFormatter : public Formatter {
public:
    /// @brief Constructs a formatter from @p pattern.
    /// @param pattern  The format string; may contain any combination of literal
    ///                 text and @c % escape sequences listed above.
    explicit PatternFormatter(
        std::string pattern = "[%Y-%m-%d %H:%M:%S] [%l] [%n] [%t] %v")
        : pattern_(std::move(pattern)) {
        Compile(pattern_);
    }

    /// @brief Formats @p msg according to the compiled token sequence.
    /// @param msg  The log event to format.
    /// @return     The formatted string (no trailing newline).
    std::string Format(const LogMessage& msg) override {
        std::tm local = time_utils::LocalTime(msg.timestamp);
        std::ostringstream oss;

        for (const auto& tok : tokens_) {
            switch (tok.type) {
                case TokenType::kLiteral:  oss << tok.literal; break;
                case TokenType::kYear:     oss << std::setfill('0') << std::setw(4) << time_utils::Year(local); break;
                case TokenType::kMonth:    oss << std::setfill('0') << std::setw(2) << time_utils::Month(local); break;
                case TokenType::kDay:      oss << std::setfill('0') << std::setw(2) << time_utils::Day(local); break;
                case TokenType::kHour:     oss << std::setfill('0') << std::setw(2) << time_utils::Hour(local); break;
                case TokenType::kMinute:   oss << std::setfill('0') << std::setw(2) << time_utils::Minute(local); break;
                case TokenType::kSecond:   oss << std::setfill('0') << std::setw(2) << time_utils::Second(local); break;
                case TokenType::kThreadId: oss << msg.thread_id; break;
                case TokenType::kLevel:    oss << LevelToString(msg.level); break;
                case TokenType::kLevelChar:oss << LevelToChar(msg.level); break;
                case TokenType::kMessage:  oss << msg.message; break;
                case TokenType::kName:     oss << msg.logger_name; break;
                case TokenType::kSourceLoc:
#if __cplusplus >= 202002L
                    oss << msg.location.file_name() << ':' << msg.location.line();
#else
                    if (msg.file) oss << msg.file << ':' << msg.line;
#endif
                    break;
                case TokenType::kFunction:
#if __cplusplus >= 202002L
                    oss << msg.location.function_name();
#else
                    if (msg.function) oss << msg.function;
#endif
                    break;
            }
        }
        return oss.str();
    }

    /// @brief Returns a deep copy of this formatter with the same pattern.
    std::unique_ptr<Formatter> Clone() const override {
        return std::make_unique<PatternFormatter>(pattern_);
    }

private:
    /// @brief Identifies the kind of a compiled token.
    enum class TokenType {
        kLiteral,   ///< Raw literal text segment
        kYear,      ///< %Y — four-digit year
        kMonth,     ///< %m — two-digit month
        kDay,       ///< %d — two-digit day
        kHour,      ///< %H — two-digit hour
        kMinute,    ///< %M — two-digit minute
        kSecond,    ///< %S — two-digit second
        kThreadId,  ///< %t — thread ID
        kLevel,     ///< %l — full level name
        kLevelChar, ///< %L — single-character level abbreviation
        kMessage,   ///< %v — message body
        kName,      ///< %n — logger name
        kSourceLoc, ///< %@ — source file and line number
        kFunction   ///< %! — enclosing function name
    };

    /// @brief A single unit in the compiled token sequence.
    struct Token {
        TokenType   type;    ///< Kind of this token
        std::string literal; ///< Literal text; populated only when @c type == @c kLiteral
    };

    /// @brief Parses @p pattern into @c tokens_ in O(pattern.size()) time.
    /// @param pattern  The raw format string to compile.
    void Compile(const std::string& pattern) {
        tokens_.clear();
        std::string lit;

        for (std::size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] != '%' || i + 1 >= pattern.size()) {
                lit += pattern[i];
                continue;
            }
            // Flush accumulated literal
            if (!lit.empty()) {
                tokens_.push_back({TokenType::kLiteral, std::move(lit)});
                lit.clear();
            }
            ++i;
            switch (pattern[i]) {
                case 'Y': tokens_.push_back({TokenType::kYear,      {}}); break;
                case 'm': tokens_.push_back({TokenType::kMonth,     {}}); break;
                case 'd': tokens_.push_back({TokenType::kDay,       {}}); break;
                case 'H': tokens_.push_back({TokenType::kHour,      {}}); break;
                case 'M': tokens_.push_back({TokenType::kMinute,    {}}); break;
                case 'S': tokens_.push_back({TokenType::kSecond,    {}}); break;
                case 't': tokens_.push_back({TokenType::kThreadId,  {}}); break;
                case 'l': tokens_.push_back({TokenType::kLevel,     {}}); break;
                case 'L': tokens_.push_back({TokenType::kLevelChar, {}}); break;
                case 'v': tokens_.push_back({TokenType::kMessage,   {}}); break;
                case 'n': tokens_.push_back({TokenType::kName,      {}}); break;
                case '@': tokens_.push_back({TokenType::kSourceLoc, {}}); break;
                case '!': tokens_.push_back({TokenType::kFunction,  {}}); break;
                case '%': lit += '%'; break;
                default:  lit += '%'; lit += pattern[i]; break;
            }
        }
        if (!lit.empty()) {
            tokens_.push_back({TokenType::kLiteral, std::move(lit)});
        }
    }

    std::string        pattern_; ///< Original pattern string passed to the constructor
    std::vector<Token> tokens_;  ///< Pre-compiled token sequence produced by Compile()
};

} // namespace logger
