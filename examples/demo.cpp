#include <logger.hpp>

#include <thread>
#include <vector>

void testSingleThread() {
    logger::Logger log("single");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(std::make_shared<logger::FileSink>("test.log"));

    log.Info("Single thread test started");
    log.Info("Success message");
    log.Warning("Warning message");
    log.Error("Error message");

    // Test enable/disable
    log.SetEnabled(false);
    log.Info("This should NOT appear");
    log.SetEnabled(true);
    log.Info("Logging re-enabled");
}

void testMultiThread(int threadId, logger::Logger& log) {
    for (int i = 0; i < 5; ++i) {
        log.Info("Thread " + std::to_string(threadId) + " - message " + std::to_string(i));
    }
    log.Info("Thread " + std::to_string(threadId) + " completed");
}

int main() {
    // Test 1: Single thread
    testSingleThread();

    // Test 2: Multi-thread
    logger::Logger log("multi");
    log.SetLevel(logger::Level::kTrace);
    log.AddSink(std::make_shared<logger::FileSink>("multithread.log"));

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(testMultiThread, i, std::ref(log));
    }
    for (auto& t : threads) t.join();

    // Test 3: Switch output file
    log.ClearSinks();
    log.AddSink(std::make_shared<logger::FileSink>("new_file.log"));
    log.Info("This goes to new file");

    return 0;
}
