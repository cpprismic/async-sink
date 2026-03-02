# Asynchronous C++ Logger

Header-only, thread-safe asynchronous logging library for C++17/20.
One include — no build step required.

---

## Features

- **Header-only** — add `include/` to your include path and you are done
- **Asynchronous** — producer threads never wait for I/O; a single worker thread drains the queue
- **Pluggable sinks** — write to a file, console, rotating files, daily files, or keep the last N messages in memory
- **Pluggable formatters** — pattern-based (`[%Y-%m-%d %H:%M:%S] [%l] %v`) or JSON
- **Level filtering** — per-logger and per-sink thresholds
- **Contextual logging** — attach key-value pairs to a scope with `WithContext()`
- **Duration tracing** — measure and log elapsed time with `TraceDuration()`
- **Convenience macros** — `LOGGER_INFO(...)`, `LOGGER_ERROR(...)`, etc.
- **C++20 `std::source_location`** — file, line, and function captured automatically when available

## Requirements

| Item | Minimum |
|---|---|
| C++ standard | C++17 (C++20 recommended) |
| CMake | 3.14+ (optional) |
| Platforms | Linux, macOS, Windows |

## Build & Install

### Direct compilation

```bash
# Your project only needs -Iinclude; no extra .cpp files to compile
g++ -std=c++17 -pthread -O2 -Iinclude main.cpp -o my_app
```

### CMake (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(logger GIT_REPOSITORY https://github.com/cpprismic/async-sink.git)
FetchContent_MakeAvailable(logger)

target_link_libraries(my_app PRIVATE logger::logger)
```

### CMake (subdirectory)

```cmake
add_subdirectory(multithreaded-logger)
target_link_libraries(my_app PRIVATE logger::logger)
```

---

## Quick Start

```cpp
#include <logger.hpp>

int main() {
    auto& log = logger::Logger::GetDefault();
    log.AddSink(std::make_shared<logger::ConsoleSink>());
    log.SetLevel(logger::Level::kTrace);

    log.Info("Application started");
    log.Warning("Low disk space: {} MB free", 128);
    log.Error("Connection refused");
}
```

Output:

```
[2024-01-15 14:30:25] [INFO]    [default] [1402567] Application started
[2024-01-15 14:30:25] [WARNING] [default] [1402567] Low disk space: 128 MB free
[2024-01-15 14:30:25] [ERROR]   [default] [1402567] Connection refused
```

---

## Log Levels

```cpp
namespace logger {
enum class Level : uint8_t {
    kTrace    = 0,   // verbose diagnostics
    kDebug    = 1,   // development info
    kInfo     = 2,   // normal operations  (default)
    kWarning  = 3,   // non-critical issues
    kError    = 4,   // recoverable errors
    kCritical = 5,   // severe failures
    kOff      = 6    // disable all output
};
}
```

Set the minimum level on the logger (primary gate, applied before the message enters the queue) and optionally on each sink (secondary gate):

```cpp
log.SetLevel(logger::Level::kDebug);           // logger filter
my_file_sink->SetLevel(logger::Level::kError); // sink filter — only errors to file
```

---

## Sinks

All sinks are in `include/logger/sinks/`. Attach them with `Logger::AddSink()`.
If no formatter is set on the sink, `PatternFormatter` with the default pattern is assigned automatically.

### `FileSink` — write to a file

```cpp
// Append mode (default)
auto sink = std::make_shared<logger::FileSink>("app.log");

// Truncate on open
auto sink = std::make_shared<logger::FileSink>("app.log", /*append=*/false);

log.AddSink(sink);
```

### `ConsoleSink` — colour output to stdout / stderr

Messages at `kWarning` and above go to `stderr`; the rest go to `stdout`.

```cpp
auto sink = std::make_shared<logger::ConsoleSink>();          // ANSI colours on
auto sink = std::make_shared<logger::ConsoleSink>(false);     // plain text

log.AddSink(sink);
```

### `RotatingFileSink` — size-based log rotation

```cpp
// Rotate "app.log" every 10 MB, keep up to 5 archived copies
auto sink = std::make_shared<logger::RotatingFileSink>("app.log", 10*1024*1024, 5);
log.AddSink(sink);
```

Rotation renames `app.log → app.1.log → … → app.5.log`; the oldest file is deleted.

### `DailyFileSink` — one file per calendar day

```cpp
auto sink = std::make_shared<logger::DailyFileSink>("app.log");
log.AddSink(sink);
// Day 1: app_2024-01-15.log  |  Day 2: app_2024-01-16.log
```

### `RingBufferSink` — keep the last N messages in memory

Primarily used in tests to assert that specific messages were logged.

```cpp
auto sink = std::make_shared<logger::RingBufferSink>(64); // retain last 64 messages
log.AddSink(sink);

// ... run code ...
log.Flush();

assert(sink->Contains("connection established"));
auto messages = sink->GetMessages(); // vector<LogMessage>
```

### `NullSink` — discard all output

```cpp
log.AddSink(std::make_shared<logger::NullSink>()); // useful for benchmarks
```

### Multiple sinks simultaneously

```cpp
log.AddSink(std::make_shared<logger::ConsoleSink>());
log.AddSink(std::make_shared<logger::FileSink>("app.log"));
log.AddSink(std::make_shared<logger::DailyFileSink>("archive.log"));
// All three receive every message above the logger's level.
```

---

## Formatters

Attach a formatter to a sink before (or instead of) calling `AddSink()`:

```cpp
auto sink = std::make_shared<logger::FileSink>("app.log");
sink->SetFormatter(std::make_unique<logger::JsonFormatter>());
log.AddSink(sink); // AddSink will not override the formatter you set
```

### `PatternFormatter` — human-readable text

```cpp
// Custom pattern
sink->SetFormatter(std::make_unique<logger::PatternFormatter>(
    "[%Y-%m-%d %H:%M:%S.%S] [%L] %v  (%n/%t)"));
```

**Pattern tokens:**

| Token | Output |
|---|---|
| `%Y` | Four-digit year |
| `%m` | Month (01–12) |
| `%d` | Day (01–31) |
| `%H` | Hour (00–23) |
| `%M` | Minute (00–59) |
| `%S` | Second (00–60) |
| `%t` | Thread ID |
| `%l` | Level name (`WARNING`) |
| `%L` | Level initial (`W`) |
| `%v` | Message text |
| `%n` | Logger name |
| `%@` | `file.cpp:42` (source location) |
| `%!` | Function name |
| `%%` | Literal `%` |

**Default pattern:** `[%Y-%m-%d %H:%M:%S] [%l] [%n] [%t] %v`

### `JsonFormatter` — structured output for log aggregators

```cpp
sink->SetFormatter(std::make_unique<logger::JsonFormatter>());
```

```json
{"timestamp":"2024-01-15T11:30:25Z","level":"ERROR","logger":"auth","thread":"14023","message":"token expired"}
```

---

## Multiple Logger Instances

The global default logger is available via `GetDefault()`. Named instances let different subsystems log independently:

```cpp
// Global logger (console output)
auto& glog = logger::Logger::GetDefault();
glog.AddSink(std::make_shared<logger::ConsoleSink>());

// Per-subsystem logger (file output)
logger::Logger db_log("database");
db_log.AddSink(std::make_shared<logger::FileSink>("db.log"));
db_log.SetLevel(logger::Level::kWarning);

db_log.Info("ignored — below threshold");
db_log.Error("connection pool exhausted"); // written to db.log
```

Replace the default logger (useful in tests):

```cpp
auto test_logger = std::make_shared<logger::Logger>("test");
auto sink = std::make_shared<logger::RingBufferSink>(256);
test_logger->AddSink(sink);

logger::Logger::SetDefault(test_logger);
// Now LOGGER_INFO() etc. go to the ring buffer
```

---

## Contextual Logging

`WithContext()` returns a `ScopeLogger` that prepends `[key=value]` pairs to every message:

```cpp
void HandleRequest(logger::Logger& log, int req_id, const std::string& user) {
    auto scope = logger::WithContext(log, {{"req", std::to_string(req_id)},
                                          {"user", user}});
    scope.Info("processing started");
    scope.Error("permission denied");
}
// [req=42] [user=alice] processing started
// [req=42] [user=alice] permission denied
```

---

## Duration Tracing

`TraceDuration()` returns a RAII guard that logs elapsed milliseconds when it leaves scope:

```cpp
{
    auto t = logger::TraceDuration(log, "database query");
    // ... query code ...
} // INFO: "database query took 5ms"
```

---

## Overflow Policy

When the internal queue is full (default capacity 8192), one of three policies applies:

```cpp
log.SetOverflowPolicy(logger::OverflowPolicy::kBlock);        // wait for space (default)
log.SetOverflowPolicy(logger::OverflowPolicy::kDiscardNew);   // drop the incoming message
log.SetOverflowPolicy(logger::OverflowPolicy::kDiscardOldest);// evict oldest, push new
```

---

## Convenience Macros

Include `<logger/macros.hpp>` (already included by the umbrella `<logger.hpp>`):

```cpp
LOGGER_TRACE("entering loop");
LOGGER_DEBUG("value = " + std::to_string(x));
LOGGER_INFO("server listening on port 8080");
LOGGER_WARNING("retry attempt " + std::to_string(n));
LOGGER_ERROR("file not found: " + path);
LOGGER_CRITICAL("out of memory — aborting");
```

All macros forward to `Logger::GetDefault()`. Disable all logging at compile time:

```cpp
#define LOGGER_DISABLE
#include <logger.hpp>  // all macros become no-ops
```

On C++20, source location (`file`, `line`, `function`) is captured automatically.
On C++17, pass it explicitly via `Log()` or use the macros (they inject `__FILE__`/`__LINE__`/`__func__`).

---

## Multithreaded Example

```cpp
#include <logger.hpp>
#include <thread>
#include <vector>

void worker(int id, logger::Logger& log) {
    for (int i = 0; i < 5; ++i) {
        log.Info("thread=" + std::to_string(id) + " step=" + std::to_string(i));
    }
}

int main() {
    logger::Logger log("main");
    log.AddSink(std::make_shared<logger::FileSink>("multithread.log"));
    log.SetLevel(logger::Level::kTrace);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i, std::ref(log));
    }
    for (auto& t : threads) t.join();

    log.Flush(); // wait for all messages to reach the file
}
```

---

## Running Tests

```bash
# CMake
cmake -S . -B build -DLOGGER_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Direct
g++ -std=c++17 -pthread -O2 -Iinclude tests/test_logger.cpp -o test_logger
./test_logger
```

The test suite covers single-thread, multi-thread, level filtering, multiple sinks, overflow policies, rotating files, formatter output, contextual logging, and duration tracing — no external framework required.
