#include "time_utils.hpp"
#include <ctime>
#include <chrono>

std::string time_utils::getCurrentTimeISO() {
    auto now = std::chrono::system_clock::now();
    return timeToISO(now);
}

std::string time_utils::timeToISO(std::chrono::system_clock::time_point time_point) {
    auto time = std::chrono::system_clock::to_time_t(time_point);
    auto tm = *std::gmtime(&time); // UTC time

    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string time_utils::getCurrentTimeWithTimezone(int hours_offset, int minutes_offset) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm_utc = *std::gmtime(&time); // UTC time
    
    // Добавляем смещение часового пояса
    tm_utc.tm_hour += hours_offset;
    tm_utc.tm_min  += minutes_offset;

    // Нормализация времени (вдруг tm_hour >= 24 или tm_min >= 60)
    std::mktime(&tm_utc);

    // Форматируем в строку ISO 8601 с указанием смещения
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S");

    // Добавляем смещение вручную
    oss << (hours_offset >= 0 ? "+" : "-")
        << std::setfill('0') << std::setw(2) << std::abs(hours_offset)
        << ":"
        << std::setw(2) << std::abs(minutes_offset);

    return oss.str();
}

bool time_utils::isValidISO8601(const std::string& time_string) {
    // Простая проверка формата
    if (time_string.length() < 19) return false;
    if (time_string[4] != '-' || time_string[7] != '-') return false;
    if (time_string[10] != 'T') return false;
    if (time_string[13] != ':' || time_string[16] != ':') return false;
    
    return true;
}