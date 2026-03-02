#pragma once

#include "level.hpp"
#include "log_message.hpp"
#include "sink.hpp"
#include "formatters/pattern_formatter.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#if __cplusplus >= 202002L
#include <source_location>
#endif

namespace logger {

/// @brief Thread-safe asynchronous logger.
///
/// Architecture overview
/// ---------------------
/// Producer threads (any thread calling @c Log()) push @c LogMessage objects
/// into an internal @c std::queue.  A single background @b worker thread drains
/// the queue in batches and dispatches each message to every registered @c Sink.
/// The batch-swap pattern minimises time the @c queue_mutex_ is held: the worker
/// atomically swaps the entire queue into a local variable, releases the lock,
/// then processes the batch without competing with producers.
///
/// Flush correctness
/// -----------------
/// @c Flush() spins until @c processed_ ≥ the value of @c enqueued_ captured at
/// the start of the call.  This counter-based approach avoids the race where the
/// worker has already swapped the queue (so @c queue_.empty() == true) but has
/// not yet dispatched all messages to sinks.
///
/// Singleton
/// ---------
/// @c GetDefault() returns a reference to the process-wide default @c Logger,
/// created lazily on first access.  @c SetDefault() replaces it (useful in
/// tests to capture output via a @c RingBufferSink).
///
/// @code
/// auto& log = logger::Logger::GetDefault();
/// log.AddSink(std::make_shared<logger::ConsoleSink>());
/// log.Info("application started");
/// @endcode
class Logger {
public:
    /// @brief Constructs a logger and starts the background worker thread.
    /// @param name  Human-readable name embedded in every @c LogMessage produced
    ///              by this instance (shown by formatters as @c %n).
    explicit Logger(std::string name = "default")
        : name_(std::move(name)) {
        worker_thread_ = std::thread(&Logger::WorkerThread, this);
    }

    /// @brief Signals the worker thread to stop, drains remaining messages, and joins.
    ~Logger() {
        shutdown_.store(true, std::memory_order_release);
        condition_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // ── Logging ───────────────────────────────────────────────────────────────

    /// @brief Enqueues a log message at the given @p level.
    ///
    /// The call returns immediately after placing the message in the queue
    /// (or after applying the @c OverflowPolicy if the queue is full).
    /// Messages below the logger's own level are discarded before entering
    /// the queue.
    ///
    /// @param level    Severity of the message.
    /// @param message  The text body (moved into the @c LogMessage).
    /// @param loc      Automatically captured source location (C++20 only).
#if __cplusplus >= 202002L
    void Log(Level level, std::string message,
             std::source_location loc = std::source_location::current()) {
        LogMessage msg;
        msg.level       = level;
        msg.timestamp   = std::chrono::system_clock::now();
        msg.thread_id   = std::this_thread::get_id();
        msg.logger_name = name_;
        msg.message     = std::move(message);
        msg.location    = loc;
        EnqueueMessage(std::move(msg));
    }
#else
    /// @param level    Severity of the message.
    /// @param message  The text body (moved into the @c LogMessage).
    /// @param file     Source file path (supply @c __FILE__ or @c nullptr).
    /// @param line     Source line number (supply @c __LINE__ or @c 0).
    /// @param func     Enclosing function name (supply @c __func__ or @c nullptr).
    void Log(Level level, std::string message,
             const char* file = nullptr, int line = 0, const char* func = nullptr) {
        LogMessage msg;
        msg.level       = level;
        msg.timestamp   = std::chrono::system_clock::now();
        msg.thread_id   = std::this_thread::get_id();
        msg.logger_name = name_;
        msg.message     = std::move(message);
        msg.file        = file;
        msg.line        = line;
        msg.function    = func;
        EnqueueMessage(std::move(msg));
    }
#endif

    /// @brief Enqueues a @c kTrace message.  @param msg  The message text.
    void Trace(std::string msg)    { Log(Level::kTrace,    std::move(msg)); }

    /// @brief Enqueues a @c kDebug message.  @param msg  The message text.
    void Debug(std::string msg)    { Log(Level::kDebug,    std::move(msg)); }

    /// @brief Enqueues a @c kInfo message.   @param msg  The message text.
    void Info(std::string msg)     { Log(Level::kInfo,     std::move(msg)); }

    /// @brief Enqueues a @c kWarning message. @param msg  The message text.
    void Warning(std::string msg)  { Log(Level::kWarning,  std::move(msg)); }

    /// @brief Enqueues a @c kError message.  @param msg  The message text.
    void Error(std::string msg)    { Log(Level::kError,    std::move(msg)); }

    /// @brief Enqueues a @c kCritical message. @param msg  The message text.
    void Critical(std::string msg) { Log(Level::kCritical, std::move(msg)); }

    // ── Sink management ───────────────────────────────────────────────────────

    /// @brief Registers @p sink as an output destination.
    ///
    /// If @p sink has no formatter attached, a default @c PatternFormatter is
    /// assigned.  This allows sinks to be constructed without specifying a
    /// formatter explicitly.
    /// @param sink  The sink to register; must not be @c nullptr.
    void AddSink(std::shared_ptr<Sink> sink) {
        if (!sink->HasFormatter()) {
            sink->SetFormatter(std::make_unique<PatternFormatter>());
        }
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.push_back(std::move(sink));
    }

    /// @brief Removes all registered sinks.
    ///
    /// Messages already in the queue will be discarded when dispatched (no
    /// sinks to receive them).  Call @c Flush() before @c ClearSinks() if
    /// in-flight messages must be delivered first.
    void ClearSinks() {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.clear();
    }

    /// @brief Blocks until all messages enqueued before this call have been
    ///        dispatched, then flushes every registered sink.
    ///
    /// Uses @c enqueued_ / @c processed_ counters to avoid the race where the
    /// worker has swapped the queue but not yet called @c Sink::Log().
    void Flush() {
        uint64_t target = enqueued_.load(std::memory_order_acquire);
        while (processed_.load(std::memory_order_acquire) < target) {
            std::this_thread::yield();
        }
        std::vector<std::shared_ptr<Sink>> sinks_copy;
        {
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            sinks_copy = sinks_;
        }
        for (auto& s : sinks_copy) s->Flush();
    }

    // ── Configuration ─────────────────────────────────────────────────────────

    /// @brief Sets the minimum level for messages to be enqueued.
    ///
    /// Messages below @p level are dropped in @c EnqueueMessage() before they
    /// enter the queue, saving allocation and locking overhead.
    /// @param level  The new minimum level.
    void SetLevel(Level level) noexcept {
        level_.store(level, std::memory_order_relaxed);
    }

    /// @brief Returns the current minimum level filter.
    Level GetLevel() const noexcept {
        return level_.load(std::memory_order_relaxed);
    }

    /// @brief Enables or disables all logging on this instance.
    /// @param enabled  When @c false, @c EnqueueMessage() is a no-op for every
    ///                 message regardless of level.
    void SetEnabled(bool enabled) noexcept {
        enabled_.store(enabled, std::memory_order_relaxed);
    }

    /// @brief Returns @c true if logging is currently enabled.
    bool IsEnabled() const noexcept {
        return enabled_.load(std::memory_order_relaxed);
    }

    /// @brief Sets the policy applied when the internal queue is full.
    /// @param policy  One of @c kBlock, @c kDiscardNew, or @c kDiscardOldest.
    void SetOverflowPolicy(OverflowPolicy policy) noexcept {
        overflow_policy_ = policy;
    }

    /// @brief Returns the logger's name as supplied to the constructor.
    const std::string& GetName() const noexcept { return name_; }

    // ── Singleton ─────────────────────────────────────────────────────────────

    /// @brief Returns a reference to the process-wide default @c Logger.
    ///
    /// The default logger is created lazily on first access with the name
    /// @c "default".  It has no sinks by default; add at least one before
    /// calling logging methods.
    static Logger& GetDefault() {
        std::lock_guard<std::mutex> lock(DefaultMutex());
        return *DefaultPtr();
    }

    /// @brief Replaces the process-wide default @c Logger.
    ///
    /// @param logger  The new default; must not be @c nullptr.
    ///                The old default is destroyed when its reference count drops
    ///                to zero (it may still be held via other @c shared_ptr copies).
    static void SetDefault(std::shared_ptr<Logger> logger) {
        std::lock_guard<std::mutex> lock(DefaultMutex());
        DefaultPtr() = std::move(logger);
    }

private:
    /// @brief Holds the sole instance of the default @c Logger (lazy init).
    static std::shared_ptr<Logger>& DefaultPtr() {
        static std::shared_ptr<Logger> instance = std::make_shared<Logger>("default");
        return instance;
    }

    /// @brief Guards @c DefaultPtr() for thread-safe replacement via @c SetDefault().
    static std::mutex& DefaultMutex() {
        static std::mutex mtx;
        return mtx;
    }

    // ── Internal ──────────────────────────────────────────────────────────────

    /// @brief Applies level/enable filters, increments @c enqueued_, and pushes
    ///        @p msg into the queue according to the active @c OverflowPolicy.
    /// @param msg  The fully populated message to enqueue.
    void EnqueueMessage(LogMessage msg) {
        if (!enabled_.load(std::memory_order_relaxed)) return;
        if (msg.level < level_.load(std::memory_order_relaxed)) return;

        enqueued_.fetch_add(1, std::memory_order_relaxed);
        bool enqueued = true;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            switch (overflow_policy_) {
                case OverflowPolicy::kBlock:
                    condition_.wait(lock, [this] {
                        return queue_.size() < kMaxQueueSize ||
                               shutdown_.load(std::memory_order_relaxed);
                    });
                    queue_.push(std::move(msg));
                    break;
                case OverflowPolicy::kDiscardNew:
                    if (queue_.size() < kMaxQueueSize) {
                        queue_.push(std::move(msg));
                    } else {
                        enqueued = false;
                    }
                    break;
                case OverflowPolicy::kDiscardOldest:
                    if (queue_.size() >= kMaxQueueSize) {
                        queue_.pop();
                        processed_.fetch_add(1, std::memory_order_relaxed);
                    }
                    queue_.push(std::move(msg));
                    break;
            }
        }
        if (!enqueued) {
            processed_.fetch_add(1, std::memory_order_relaxed);
        }
        condition_.notify_one();
    }

    /// @brief Delivers @p msg to every sink whose level threshold is satisfied,
    ///        then increments @c processed_.
    /// @param msg  The message to dispatch.
    void DispatchMessage(const LogMessage& msg) {
        {
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            for (auto& sink : sinks_) {
                if (sink->ShouldLog(msg.level)) {
                    sink->Log(msg);
                }
            }
        }
        processed_.fetch_add(1, std::memory_order_release);
    }

    /// @brief Calls @c Flush() on every registered sink (used at worker shutdown).
    void FlushAllSinks() {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        for (auto& sink : sinks_) sink->Flush();
    }

    /// @brief The background worker loop: waits for messages, batch-swaps the
    ///        queue, dispatches all messages without holding @c queue_mutex_,
    ///        then performs a final drain on shutdown.
    void WorkerThread() {
        while (true) {
            std::queue<LogMessage> local;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return !queue_.empty() ||
                           shutdown_.load(std::memory_order_acquire);
                });
                std::swap(local, queue_);
            }

            while (!local.empty()) {
                DispatchMessage(local.front());
                local.pop();
            }

            if (shutdown_.load(std::memory_order_acquire)) {
                // Final drain: pick up messages pushed between the swap and now.
                std::queue<LogMessage> remaining;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    std::swap(remaining, queue_);
                }
                while (!remaining.empty()) {
                    DispatchMessage(remaining.front());
                    remaining.pop();
                }
                break;
            }
        }
        FlushAllSinks();
    }

    static constexpr std::size_t kMaxQueueSize = 8192; ///< Queue capacity before overflow policy kicks in

    std::string           name_;                                   ///< Logger instance name (appears as %n in patterns)
    std::atomic<Level>    level_{Level::kInfo};                    ///< Minimum level accepted by EnqueueMessage()
    std::atomic<bool>     enabled_{true};                          ///< Master switch; when false all messages are dropped
    std::atomic<bool>     shutdown_{false};                        ///< Set by destructor to stop WorkerThread()

    std::queue<LogMessage>    queue_;                              ///< Pending messages awaiting dispatch
    std::mutex                queue_mutex_;                        ///< Guards queue_; also used by condition_
    std::condition_variable   condition_;                          ///< Wakes WorkerThread() when queue_ is non-empty or shutdown_
    OverflowPolicy            overflow_policy_{OverflowPolicy::kBlock}; ///< Action taken when queue_ reaches kMaxQueueSize
    std::atomic<uint64_t>     enqueued_{0};                        ///< Monotonic count of messages accepted by EnqueueMessage()
    std::atomic<uint64_t>     processed_{0};                       ///< Monotonic count of messages dispatched by DispatchMessage()

    std::vector<std::shared_ptr<Sink>> sinks_;                     ///< Registered output destinations
    std::mutex                         sinks_mutex_;               ///< Guards sinks_ for AddSink() / ClearSinks() / DispatchMessage()

    std::thread               worker_thread_;                      ///< Background thread that drains queue_ to sinks
};

// ── Contextual logging ────────────────────────────────────────────────────────

/// @brief A view of a @c Logger that prepends a fixed context prefix to every message.
///
/// @c ScopeLogger does not own the @c Logger; it holds a reference.  The caller
/// must ensure the referenced @c Logger outlives the @c ScopeLogger.
///
/// Typically obtained via @c WithContext():
/// @code
/// auto scope = logger::WithContext(log, {{"request_id", "42"}, {"user", "alice"}});
/// scope.Info("processing request");
/// // Logged as: "[request_id=42] [user=alice] processing request"
/// @endcode
class ScopeLogger {
public:
    /// @brief Constructs a scoped logger with a set of key-value context pairs.
    /// @param logger  The underlying logger to forward messages to.
    /// @param ctx     Key-value pairs that form the prefix (e.g. @c {{"k","v"}}).
    ScopeLogger(Logger& logger,
                std::initializer_list<std::pair<std::string, std::string>> ctx)
        : logger_(logger) {
        for (const auto& [k, v] : ctx) {
            prefix_ += '[' + k + '=' + v + "] ";
        }
    }

    /// @brief Logs @p msg at @c kTrace with the context prefix.
    void Trace(const std::string& msg)    { logger_.Trace(prefix_ + msg); }

    /// @brief Logs @p msg at @c kDebug with the context prefix.
    void Debug(const std::string& msg)    { logger_.Debug(prefix_ + msg); }

    /// @brief Logs @p msg at @c kInfo with the context prefix.
    void Info(const std::string& msg)     { logger_.Info(prefix_ + msg); }

    /// @brief Logs @p msg at @c kWarning with the context prefix.
    void Warning(const std::string& msg)  { logger_.Warning(prefix_ + msg); }

    /// @brief Logs @p msg at @c kError with the context prefix.
    void Error(const std::string& msg)    { logger_.Error(prefix_ + msg); }

    /// @brief Logs @p msg at @c kCritical with the context prefix.
    void Critical(const std::string& msg) { logger_.Critical(prefix_ + msg); }

private:
    Logger&     logger_; ///< Reference to the parent Logger
    std::string prefix_; ///< Precomputed "[key=value] " context string
};

/// @brief Creates a @c ScopeLogger that prepends @p ctx to every message.
/// @param logger  The logger to forward messages to.
/// @param ctx     Key-value pairs forming the context prefix.
/// @return        A @c ScopeLogger by value (NRVO-eligible).
inline ScopeLogger WithContext(
    Logger& logger,
    std::initializer_list<std::pair<std::string, std::string>> ctx) {
    return ScopeLogger(logger, ctx);
}

// ── Duration tracing ──────────────────────────────────────────────────────────

/// @brief RAII guard that logs the elapsed time of a code section on destruction.
///
/// The start time is captured at construction using @c std::chrono::steady_clock.
/// When the @c DurationTracer is destroyed (scope exit), it emits a @c kInfo
/// message of the form @c "{label} took {N}ms".
///
/// @code
/// {
///     auto t = logger::TraceDuration(log, "database query");
///     // ... query code ...
/// }  // Logs: "database query took 42ms"
/// @endcode
///
/// @note Non-copyable and non-movable; always use in the same scope it was created.
class DurationTracer {
public:
    /// @brief Starts timing and records @p label.
    /// @param logger  Logger used to emit the elapsed-time message.
    /// @param label   Human-readable label prepended to the log output.
    DurationTracer(Logger& logger, std::string label)
        : logger_(logger)
        , label_(std::move(label))
        , start_(std::chrono::steady_clock::now()) {}

    /// @brief Computes elapsed milliseconds and logs @c "{label} took {N}ms".
    ~DurationTracer() {
        auto elapsed = std::chrono::steady_clock::now() - start_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        logger_.Info(label_ + " took " + std::to_string(ms) + "ms");
    }

    DurationTracer(const DurationTracer&)            = delete;
    DurationTracer& operator=(const DurationTracer&) = delete;

private:
    Logger&                               logger_; ///< Logger used to emit the elapsed-time message
    std::string                           label_;  ///< Human-readable label prepended to the output
    std::chrono::steady_clock::time_point start_;  ///< Monotonic start time captured at construction
};

/// @brief Creates a @c DurationTracer that logs elapsed time when it goes out of scope.
/// @param logger  Logger to emit the timing message to.
/// @param label   Description of the timed section.
/// @return        A @c DurationTracer by value.
inline DurationTracer TraceDuration(Logger& logger, std::string label) {
    return DurationTracer(logger, std::move(label));
}

} // namespace logger
