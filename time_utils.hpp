#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace time_utils {

/// @brief Текущее время в ISO 8601 format (UTC)
std::string getCurrentTimeISO();

/// @brief Текущее время с указанием timezone
std::string getCurrentTimeWithTimezone(int hours_offset, int minutes_offset = 0);

/// @brief Проверка валидности ISO строки
bool isValidISO8601(const std::string& time_string);

/// @brief Конвертация из system time в ISO
std::string timeToISO(std::chrono::system_clock::time_point time_point);

}; //namespace time_utils

#endif // TIME_UTILS_H