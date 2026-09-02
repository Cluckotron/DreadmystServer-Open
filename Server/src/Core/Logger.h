// Server Logging System
// TODO: Implement in Task 1.13

#pragma once

#include <string>
#include <cstdio>

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
public:
    static Logger& instance();

    void setLevel(LogLevel level) { m_level = level; }
    void log(LogLevel level, const char* format, ...);

private:
    Logger() = default;
    LogLevel m_level = LogLevel::Info;
};

#define sLogger Logger::instance()

// Convenience macros
#define LOG_DEBUG(fmt, ...) sLogger.log(LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  sLogger.log(LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  sLogger.log(LogLevel::Warning, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) sLogger.log(LogLevel::Error, fmt, ##__VA_ARGS__)
