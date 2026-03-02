#pragma once

#include "logger.hpp"

#ifndef LOGGER_DISABLE

#if __cplusplus >= 202002L
// C++20: source_location is captured automatically by Logger::Log default parameter.
#  define LOGGER_TRACE(msg)    ::logger::Logger::GetDefault().Trace(msg)
#  define LOGGER_DEBUG(msg)    ::logger::Logger::GetDefault().Debug(msg)
#  define LOGGER_INFO(msg)     ::logger::Logger::GetDefault().Info(msg)
#  define LOGGER_WARNING(msg)  ::logger::Logger::GetDefault().Warning(msg)
#  define LOGGER_ERROR(msg)    ::logger::Logger::GetDefault().Error(msg)
#  define LOGGER_CRITICAL(msg) ::logger::Logger::GetDefault().Critical(msg)
#else
// C++17: pass source location via macros.
#  define LOGGER_TRACE(msg)    ::logger::Logger::GetDefault().Log(::logger::Level::kTrace,    (msg), __FILE__, __LINE__, __func__)
#  define LOGGER_DEBUG(msg)    ::logger::Logger::GetDefault().Log(::logger::Level::kDebug,    (msg), __FILE__, __LINE__, __func__)
#  define LOGGER_INFO(msg)     ::logger::Logger::GetDefault().Log(::logger::Level::kInfo,     (msg), __FILE__, __LINE__, __func__)
#  define LOGGER_WARNING(msg)  ::logger::Logger::GetDefault().Log(::logger::Level::kWarning,  (msg), __FILE__, __LINE__, __func__)
#  define LOGGER_ERROR(msg)    ::logger::Logger::GetDefault().Log(::logger::Level::kError,    (msg), __FILE__, __LINE__, __func__)
#  define LOGGER_CRITICAL(msg) ::logger::Logger::GetDefault().Log(::logger::Level::kCritical, (msg), __FILE__, __LINE__, __func__)
#endif

#else // LOGGER_DISABLE

#  define LOGGER_TRACE(msg)    do {} while(0)
#  define LOGGER_DEBUG(msg)    do {} while(0)
#  define LOGGER_INFO(msg)     do {} while(0)
#  define LOGGER_WARNING(msg)  do {} while(0)
#  define LOGGER_ERROR(msg)    do {} while(0)
#  define LOGGER_CRITICAL(msg) do {} while(0)

#endif // LOGGER_DISABLE
