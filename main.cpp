#include "logger.hpp"
#include <thread>
#include <vector>

void testSingleThread() {
    Logger logger("test.log");
    
    logger.logInfo("Single thread test started");
    logger.logSuccess("Success message");
    logger.logWarning("Warning message");
    logger.logError("Error message");
    
    // Тест управления
    logger.disable();
    logger.logInfo("This should NOT appear");
    logger.enable();
    logger.logInfo("Logging re-enabled");
}

void testMultiThread(int threadId, Logger& logger) {
    for (int i = 0; i < 5; ++i) {
        logger.logInfo("Thread " + std::to_string(threadId) + " - message " + std::to_string(i));
    }
    logger.logSuccess("Thread " + std::to_string(threadId) + " completed");
}

int main() {
    // Тест 1: Одиночный поток
    testSingleThread();
    
    // Тест 2: Многопоточность
    Logger logger("multithread.log");
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(testMultiThread, i, std::ref(logger));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Тест 3: Смена файла
    logger.setFilePath("new_file.log");
    logger.logInfo("This goes to new file");
    
    return 0;
}