// Server Logging System
// TODO: Implement in Task 1.13

#include "stdafx.h"
#include "Core/Logger.h"
#include <cstdarg>
#include <ctime>

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const char* format, ...)
{
    if (level < m_level)
        return;

    // Get timestamp
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);

    // Level prefix
    const char* levelStr = "";
    switch (level) {
        case LogLevel::Debug:   levelStr = "DEBUG"; break;
        case LogLevel::Info:    levelStr = "INFO "; break;
        case LogLevel::Warning: levelStr = "WARN "; break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
    }

    // Print timestamp and level
    std::printf("[%s] [%s] ", timeStr, levelStr);

    // Print formatted message
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);

    std::printf("\n");
    std::fflush(stdout);
}
