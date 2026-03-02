#pragma once

// Single include to pull in the entire logger library.

#include "logger/level.hpp"
#include "logger/log_message.hpp"
#include "logger/time_utils.hpp"
#include "logger/formatter.hpp"
#include "logger/sink.hpp"
#include "logger/formatters/pattern_formatter.hpp"
#include "logger/formatters/json_formatter.hpp"
#include "logger/sinks/null_sink.hpp"
#include "logger/sinks/ring_buffer_sink.hpp"
#include "logger/sinks/console_sink.hpp"
#include "logger/sinks/file_sink.hpp"
#include "logger/sinks/rotating_file_sink.hpp"
#include "logger/sinks/daily_file_sink.hpp"
#include "logger/logger.hpp"
#include "logger/macros.hpp"
