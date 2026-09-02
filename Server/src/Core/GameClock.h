// Game Clock - Tick-based timing system
// Task 2.9: Tick-Based Update System

#pragma once

#include <chrono>
#include <cstdint>

class GameClock
{
public:
    static GameClock& instance();

    // Start the clock (call once at server start)
    void start();

    // Update the clock and return true if a tick has passed
    bool tick();

    // Get delta time since last tick (in seconds)
    float getDeltaTime() const { return m_deltaTime; }

    // Get total ticks since start
    uint64_t getTickCount() const { return m_tickCount; }

    // Get time since start (in seconds)
    double getElapsedTime() const;

    // Get server uptime as formatted string
    std::string getUptimeString() const;

    // Configuration
    void setTickRate(int ticksPerSecond);
    int getTickRate() const { return m_tickRate; }
    float getTickInterval() const { return m_tickInterval; }

    // Check for lag (tick took longer than expected)
    bool wasLagging() const { return m_wasLagging; }
    float getLagAmount() const { return m_lagAmount; }

    // Default tick rate
    static constexpr int DEFAULT_TICK_RATE = 20;  // 20 ticks per second (50ms)

private:
    GameClock() = default;

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_startTime;
    TimePoint m_lastTickTime;
    TimePoint m_currentTime;

    float m_deltaTime = 0.0f;       // Time since last tick (seconds)
    float m_tickInterval = 0.05f;   // Target interval (seconds)
    float m_accumulator = 0.0f;     // Time accumulated for next tick

    uint64_t m_tickCount = 0;
    int m_tickRate = DEFAULT_TICK_RATE;

    bool m_wasLagging = false;
    float m_lagAmount = 0.0f;

    bool m_started = false;
};

#define sGameClock GameClock::instance()
