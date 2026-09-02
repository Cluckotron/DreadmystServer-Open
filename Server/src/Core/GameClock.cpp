// Game Clock - Tick-based timing system
// Task 2.9: Tick-Based Update System

#include "stdafx.h"
#include "Core/GameClock.h"
#include "Core/Logger.h"
#include <algorithm>

GameClock& GameClock::instance()
{
    static GameClock instance;
    return instance;
}

void GameClock::start()
{
    m_startTime = Clock::now();
    m_lastTickTime = m_startTime;
    m_currentTime = m_startTime;
    m_tickCount = 0;
    m_accumulator = 0.0f;
    m_started = true;

    LOG_INFO("Game clock started (tick rate: %d/sec, interval: %.0fms)",
             m_tickRate, m_tickInterval * 1000.0f);
}

bool GameClock::tick()
{
    if (!m_started)
        start();

    m_currentTime = Clock::now();
    const float elapsedSinceTick =
        std::chrono::duration<float>(m_currentTime - m_lastTickTime).count();

    // Do not accumulate the same frame time repeatedly. The original emulator
    // added (now-lastTick) on every 1 ms main-loop poll without advancing
    // lastTick until a tick fired, causing the nominal 20 Hz clock to run far
    // faster than real time.
    if (elapsedSinceTick < m_tickInterval)
        return false;

    // Use real elapsed time so movement/timers stay wall-clock correct if the
    // host stalls briefly, but cap pathological pauses to avoid a huge jump.
    constexpr float MAX_DELTA = 0.25f;
    m_deltaTime = std::min(elapsedSinceTick, MAX_DELTA);
    m_wasLagging = elapsedSinceTick > MAX_DELTA;
    m_lagAmount = m_wasLagging ? (elapsedSinceTick - MAX_DELTA) : 0.0f;
    if (m_wasLagging)
        LOG_WARN("Game clock lagging by %.1fms", m_lagAmount * 1000.0f);

    m_lastTickTime = m_currentTime;
    m_tickCount++;
    return true;
}

double GameClock::getElapsedTime() const
{
    auto elapsed = std::chrono::duration<double>(Clock::now() - m_startTime);
    return elapsed.count();
}

std::string GameClock::getUptimeString() const
{
    double seconds = getElapsedTime();

    int days = static_cast<int>(seconds / 86400);
    seconds -= days * 86400;

    int hours = static_cast<int>(seconds / 3600);
    seconds -= hours * 3600;

    int minutes = static_cast<int>(seconds / 60);
    int secs = static_cast<int>(seconds) % 60;

    char buf[64];
    if (days > 0) {
        std::snprintf(buf, sizeof(buf), "%dd %02d:%02d:%02d", days, hours, minutes, secs);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, secs);
    }
    return buf;
}

void GameClock::setTickRate(int ticksPerSecond)
{
    if (ticksPerSecond <= 0) {
        ticksPerSecond = DEFAULT_TICK_RATE;
    }
    m_tickRate = ticksPerSecond;
    m_tickInterval = 1.0f / static_cast<float>(ticksPerSecond);
}
