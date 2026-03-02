#include <logger.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

// Minimal test harness
static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr) do { \
    if (expr) { \
        ++g_passed; \
    } else { \
        ++g_failed; \
        fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
    } \
} while(0)

#define SECTION(name) fprintf(stdout, "\n[%s]\n", name)

// Helper: wait for logger to drain, then flush
static void Drain(logger::Logger& log) {
    log.Flush();
}

// ── Test 1: Single-thread, RingBufferSink ─────────────────────────────────────
static void TestSingleThread() {
    SECTION("single-thread");

    auto sink = std::make_shared<logger::RingBufferSink>(32);
    logger::Logger log("t1");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    log.Info("hello world");
    log.Debug("debug message");
    log.Warning("warn");
    Drain(log);

    CHECK(sink->Contains("hello world"));
    CHECK(sink->Contains("debug message"));
    CHECK(sink->Contains("warn"));
    CHECK(sink->Size() == 3);
}

// ── Test 2: Level filtering ────────────────────────────────────────────────────
static void TestLevelFilter() {
    SECTION("level-filter");

    auto sink = std::make_shared<logger::RingBufferSink>(16);
    logger::Logger log("t2");
    log.SetLevel(logger::Level::kError);
    log.AddSink(sink);

    log.Info("should be dropped");
    log.Warning("also dropped");
    log.Error("should appear");
    log.Critical("critical appears");
    Drain(log);

    CHECK(!sink->Contains("should be dropped"));
    CHECK(!sink->Contains("also dropped"));
    CHECK(sink->Contains("should appear"));
    CHECK(sink->Contains("critical appears"));
    CHECK(sink->Size() == 2);
}

// ── Test 3: Multi-thread correctness ──────────────────────────────────────────
static void TestMultiThread() {
    SECTION("multi-thread");

    constexpr int kThreads   = 8;
    constexpr int kPerThread = 100;
    auto sink = std::make_shared<logger::RingBufferSink>(kThreads * kPerThread + 10);

    logger::Logger log("t3");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&log, t] {
            for (int i = 0; i < kPerThread; ++i) {
                log.Info("thread=" + std::to_string(t) + " msg=" + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) th.join();
    Drain(log);

    CHECK(sink->Size() == static_cast<std::size_t>(kThreads * kPerThread));
}

// ── Test 4: Multiple sinks ────────────────────────────────────────────────────
static void TestMultipleSinks() {
    SECTION("multiple-sinks");

    auto s1 = std::make_shared<logger::RingBufferSink>(8);
    auto s2 = std::make_shared<logger::RingBufferSink>(8);
    logger::Logger log("t4");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(s1);
    log.AddSink(s2);

    log.Info("broadcast");
    Drain(log);

    CHECK(s1->Contains("broadcast"));
    CHECK(s2->Contains("broadcast"));
}

// ── Test 5: Enable / disable ──────────────────────────────────────────────────
static void TestEnableDisable() {
    SECTION("enable-disable");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    logger::Logger log("t5");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    log.SetEnabled(false);
    log.Info("invisible");
    log.SetEnabled(true);
    log.Info("visible");
    Drain(log);

    CHECK(!sink->Contains("invisible"));
    CHECK(sink->Contains("visible"));
}

// ── Test 6: Overflow kDiscardNew ─────────────────────────────────────────────
static void TestOverflowDiscardNew() {
    SECTION("overflow-discard-new");

    // Use a null sink so the worker drains slowly only when we flush
    auto sink = std::make_shared<logger::NullSink>();
    logger::Logger log("t6");
    log.SetLevel(logger::Level::kTrace);
    log.SetOverflowPolicy(logger::OverflowPolicy::kDiscardNew);
    log.AddSink(sink);

    // Send many messages quickly — should not block or crash
    for (int i = 0; i < 1000; ++i) {
        log.Info("msg " + std::to_string(i));
    }
    Drain(log);
    CHECK(true);  // no crash = pass
}

// ── Test 7: RotatingFileSink ──────────────────────────────────────────────────
static void TestRotatingFileSink() {
    SECTION("rotating-file-sink");

    const std::string base = "/tmp/test_rotate.log";
    // Clean up any previous run
    for (int i = 0; i <= 3; ++i) {
        std::string n = (i == 0) ? base : base.substr(0, base.rfind('.'))
                                          + '.' + std::to_string(i)
                                          + base.substr(base.rfind('.'));
        std::remove(n.c_str());
    }

    {
        auto sink = std::make_shared<logger::RotatingFileSink>(base, 200, 3);
        logger::Logger log("t7");
        log.SetLevel(logger::Level::kTrace);
        log.AddSink(sink);

        // Each message is ~20 chars; 200-byte limit forces rotation after ~10 msgs
        for (int i = 0; i < 50; ++i) {
            log.Info("rotation-test-msg-" + std::to_string(i));
        }
        Drain(log);
    }

    // At least the base and one rotated file should exist
    CHECK(std::filesystem::exists(base));
    // At least one rotated file
    std::string rot1 = base.substr(0, base.rfind('.')) + ".1" + base.substr(base.rfind('.'));
    CHECK(std::filesystem::exists(rot1));
}

// ── Test 8: NullSink smoke test ───────────────────────────────────────────────
static void TestNullSink() {
    SECTION("null-sink-smoke");

    logger::Logger log("t8");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(std::make_shared<logger::NullSink>());

    for (int i = 0; i < 1000; ++i) log.Info("message " + std::to_string(i));
    Drain(log);
    CHECK(true);  // no crash = pass
}

// ── Test 9: PatternFormatter tokens ──────────────────────────────────────────
static void TestPatternFormatter() {
    SECTION("pattern-formatter");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    sink->SetFormatter(
        std::make_unique<logger::PatternFormatter>("[%l] %v"));
    logger::Logger log("t9");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    log.Error("something bad");
    Drain(log);

    CHECK(sink->Contains("[ERROR] something bad"));
}

// ── Test 10: JsonFormatter ────────────────────────────────────────────────────
static void TestJsonFormatter() {
    SECTION("json-formatter");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    sink->SetFormatter(std::make_unique<logger::JsonFormatter>());
    logger::Logger log("t10");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    log.Info("json test");
    Drain(log);

    CHECK(sink->Contains("\"level\":\"INFO\""));
    CHECK(sink->Contains("\"message\":\"json test\""));
}

// ── Test 11: ScopeLogger (contextual logging) ─────────────────────────────────
static void TestScopeLogger() {
    SECTION("scope-logger");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    logger::Logger log("t11");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    auto scope = logger::WithContext(log, {{"request_id", "42"}, {"user", "alice"}});
    scope.Info("processing");
    Drain(log);

    CHECK(sink->Contains("request_id=42"));
    CHECK(sink->Contains("user=alice"));
    CHECK(sink->Contains("processing"));
}

// ── Test 12: DurationTracer ───────────────────────────────────────────────────
static void TestDurationTracer() {
    SECTION("duration-tracer");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    logger::Logger log("t12");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(sink);

    {
        auto tracer = logger::TraceDuration(log, "my operation");
        // tracer destructor logs on exit
    }
    Drain(log);

    CHECK(sink->Contains("my operation took"));
    CHECK(sink->Contains("ms"));
}

// ── Test 13: Sink-level filtering ─────────────────────────────────────────────
static void TestSinkLevelFilter() {
    SECTION("sink-level-filter");

    auto error_only = std::make_shared<logger::RingBufferSink>(8);
    error_only->SetLevel(logger::Level::kError);

    auto all = std::make_shared<logger::RingBufferSink>(8);
    all->SetLevel(logger::Level::kTrace);

    logger::Logger log("t13");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(error_only);
    log.AddSink(all);

    log.Info("info msg");
    log.Error("error msg");
    Drain(log);

    CHECK(!error_only->Contains("info msg"));
    CHECK(error_only->Contains("error msg"));
    CHECK(all->Contains("info msg"));
    CHECK(all->Contains("error msg"));
}

// ── Test 14: LOGGER_ macros ───────────────────────────────────────────────────
static void TestMacros() {
    SECTION("macros");

    auto sink = std::make_shared<logger::RingBufferSink>(8);
    auto logger = std::make_shared<logger::Logger>("macro-test");
    logger->SetLevel(logger::Level::kTrace);
    logger->AddSink(sink);
    logger::Logger::SetDefault(logger);

    LOGGER_INFO("macro info");
    LOGGER_ERROR("macro error");
    logger->Flush();

    CHECK(sink->Contains("macro info"));
    CHECK(sink->Contains("macro error"));

    // Restore default
    logger::Logger::SetDefault(std::make_shared<logger::Logger>("default"));
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    TestSingleThread();
    TestLevelFilter();
    TestMultiThread();
    TestMultipleSinks();
    TestEnableDisable();
    TestOverflowDiscardNew();
    TestRotatingFileSink();
    TestNullSink();
    TestPatternFormatter();
    TestJsonFormatter();
    TestScopeLogger();
    TestDurationTracer();
    TestSinkLevelFilter();
    TestMacros();

    fprintf(stdout, "\n%d passed, %d failed\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
