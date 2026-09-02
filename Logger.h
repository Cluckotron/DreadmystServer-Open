// Logger.h - Simple logging for Dreadmyst client
#pragma once

#include <cstdio>
#include <cstdarg>

// Logger class with level constants matching code expectations
class Logger {
public:
    static constexpr int LOG_DEBUG = 0;
    static constexpr int LOG_INFO = 1;
    static constexpr int LOG_WARNING = 2;
    static constexpr int LOG_ERROR = 3;
};

// blog function - the primary logging interface used by the codebase
inline void blog(int level, const char* fmt, ...)
{
    const char* prefix = "";
    switch (level)
    {
        case Logger::LOG_DEBUG:   prefix = "[DEBUG] "; break;
        case Logger::LOG_INFO:    prefix = "[INFO]  "; break;
        case Logger::LOG_WARNING: prefix = "[WARN]  "; break;
        case Logger::LOG_ERROR:   prefix = "[ERROR] "; break;
    }

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s", prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
