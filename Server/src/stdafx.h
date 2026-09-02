// stdafx.h : Precompiled header for Dreadmyst Server
// Include frequently used headers here for faster compilation

#pragma once

// Standard library
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// SQLite3
#include <sqlite3.h>

// Shared code
#include "StlBuffer.h"
#include "GamePacketBase.h"
#include "GamePacketServer.h"
#include "GamePacketClient.h"

// Server-specific assertion macro
#ifdef NDEBUG
    #define ASSERT(expr) ((void)0)
#else
    #define ASSERT(expr) \
        do { \
            if (!(expr)) { \
                std::fprintf(stderr, "\n%s:%d in %s ASSERTION FAILED:\n  %s\n", \
                    __FILE__, __LINE__, __FUNCTION__, #expr); \
                std::abort(); \
            } \
        } while (0)
#endif

// Convenience type aliases
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

// Forward declarations for common server types
class Session;
class SessionManager;
class PacketRouter;
class DatabaseManager;
