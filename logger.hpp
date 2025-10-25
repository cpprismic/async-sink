#ifndef LOGGER_H
#define LOGGER_H

#include "time_utils.hpp"

#include <fstream>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

/* Пример записей в лог файле
[2024-01-15 14:30:25 +0300] [SUCCESS] Программа успешно запущена
[2024-01-15 14:30:25 +0300] [INFO] Загрузка конфигурации
[2024-01-15 14:30:25 +0300] [WARNING] Не найдены настройки, используются значения по умолчанию
[2024-01-15 14:30:25 +0300] [ERROR] Ошибка подключения к базе данных
[2024-01-15 14:30:25 +0300] [INFO] Обычное сообщение
[2024-01-15 14:30:25 +0300] [SUCCESS] Операция завершена успешно */

/// @brief Многопоточный асинхронный логгер
class Logger {
public:
    /// @brief Типы сообщений для логирования
    enum class MESSAGE_TYPE {
        SUCCESS,
        ERROR, 
        WARNING,
        INFO
    };

    /// @brief Создать логгер с указанным путем к файлу
    explicit Logger(const std::string& file_path = "log.txt");

    // Удаляем копирование и присваивание
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    /// @brief Деструктор - закрывает файл и сбрасывает буфер
    ~Logger();
    
    /// @brief Получить экземпляр логера с путем по умолчанию
    static Logger& getInstance();
    
    /// @brief Получить экземпляр логера с указанным путем к файлу
    static Logger& getInstance(const std::string& file_path);
    
    /// @brief Записать сообщение с указанным типом
    void log(MESSAGE_TYPE message_type, const std::string& message);
    
    /// @brief Записать информационное сообщение
    void logInfo(const std::string& message);
    
    /// @brief Записать предупреждение
    void logWarning(const std::string& message);
    
    /// @brief Записать сообщение об ошибке
    void logError(const std::string& message);
    
    /// @brief Записать сообщение об успехе
    void logSuccess(const std::string& message);
    
    /// @brief Установить новый путь к файлу лога
    void setFilePath(const std::string& new_path);

    /// @brief Включить логирование
    void enable();
    
    /// @brief Выключить логирование
    void disable();
    
    /// @brief Проверить, включено ли логирование
    bool isEnabled() const;
    
    /// @brief Получить текущий путь к файлу лога
    std::string getFilePath() const;

private:
    /// @brief Фоновый поток для записи в файл
    void workerThread();

    /// @brief Преобразовать тип сообщения в строку
    std::string messageTypeToString(MESSAGE_TYPE message_type);

    /// @brief Создать запись лога
    std::string createLogEntry(MESSAGE_TYPE message_type, const std::string& message);

    // Состояние логгера
    std::atomic<bool> enabled_{true};
    std::atomic<bool> shutdown_{false};

    // Механизм многопоточности
    std::queue<std::string> log_queue_;        //< Очередь сообщений для записи
    std::mutex queue_mutex_;                   //< Мьютекс для синхронизации доступа к очереди
    std::condition_variable condition_;        //< Условная переменная для уведомления рабочего потока
    std::thread worker_thread_;                //< Фоновый поток для асинхронной записи

    // Конфигурация логгера
    std::string file_path_;                    //< Путь к файлу лога
    std::ofstream log_file_;                   //< Файловый поток (используется только в worker thread)
};

#endif // LOGGER_H