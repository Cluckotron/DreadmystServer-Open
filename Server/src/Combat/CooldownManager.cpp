// CooldownManager - Tracks spell cooldowns for players
// Task 5.7: Cooldown System

#include "stdafx.h"
#include "Combat/CooldownManager.h"
#include "Core/Logger.h"

#include <chrono>
#include <algorithm>

// ============================================================================
// Time Utilities
// ============================================================================

int64_t CooldownManager::getCurrentTimeMs()
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now.time_since_epoch()).count();
}

// ============================================================================
// Cooldown Management
// ============================================================================

void CooldownManager::startCooldown(int32_t spellId, int32_t durationMs, int32_t categoryId)
{
    if (durationMs <= 0)
        return;

    int64_t now = getCurrentTimeMs();
    int64_t endTime = now + durationMs;

    // Create cooldown entry
    CooldownEntry entry;
    entry.spellId = spellId;
    entry.endTimeMs = endTime;
    entry.categoryId = categoryId;

    m_cooldowns[spellId] = entry;

    // If this spell has a category, also set the category cooldown
    if (categoryId > 0)
    {
        // Use the longer of existing category cooldown or new one
        auto it = m_categoryCooldowns.find(categoryId);
        if (it == m_categoryCooldowns.end() || it->second < endTime)
        {
            m_categoryCooldowns[categoryId] = endTime;
        }
    }
}

void CooldownManager::startGCD()
{
    int64_t now = getCurrentTimeMs();
    m_gcdEndTime = now + CooldownConfig::GCD_DURATION_MS;
}

bool CooldownManager::isOnCooldown(int32_t spellId) const
{
    auto it = m_cooldowns.find(spellId);
    if (it == m_cooldowns.end())
        return false;

    return getCurrentTimeMs() < it->second.endTimeMs;
}

bool CooldownManager::isOnGCD() const
{
    return getCurrentTimeMs() < m_gcdEndTime;
}

bool CooldownManager::isCategoryOnCooldown(int32_t categoryId) const
{
    if (categoryId <= 0)
        return false;

    auto it = m_categoryCooldowns.find(categoryId);
    if (it == m_categoryCooldowns.end())
        return false;

    return getCurrentTimeMs() < it->second;
}

int32_t CooldownManager::getRemainingCooldown(int32_t spellId) const
{
    auto it = m_cooldowns.find(spellId);
    if (it == m_cooldowns.end())
        return 0;

    int64_t now = getCurrentTimeMs();
    if (now >= it->second.endTimeMs)
        return 0;

    return static_cast<int32_t>(it->second.endTimeMs - now);
}

int32_t CooldownManager::getRemainingGCD() const
{
    int64_t now = getCurrentTimeMs();
    if (now >= m_gcdEndTime)
        return 0;

    return static_cast<int32_t>(m_gcdEndTime - now);
}

int32_t CooldownManager::getRemainingCategoryCooldown(int32_t categoryId) const
{
    if (categoryId <= 0)
        return 0;

    auto it = m_categoryCooldowns.find(categoryId);
    if (it == m_categoryCooldowns.end())
        return 0;

    int64_t now = getCurrentTimeMs();
    if (now >= it->second)
        return 0;

    return static_cast<int32_t>(it->second - now);
}

// ============================================================================
// Cooldown Clearing
// ============================================================================

void CooldownManager::clearAll()
{
    m_cooldowns.clear();
    m_categoryCooldowns.clear();
    m_gcdEndTime = 0;
}

void CooldownManager::clearCooldown(int32_t spellId)
{
    auto it = m_cooldowns.find(spellId);
    if (it != m_cooldowns.end())
    {
        // Also need to recalculate category cooldown if this was contributing
        int32_t categoryId = it->second.categoryId;
        m_cooldowns.erase(it);

        // Recalculate category cooldown from remaining spells in that category
        if (categoryId > 0)
        {
            int64_t maxCategoryEnd = 0;
            for (const auto& [id, entry] : m_cooldowns)
            {
                if (entry.categoryId == categoryId && entry.endTimeMs > maxCategoryEnd)
                {
                    maxCategoryEnd = entry.endTimeMs;
                }
            }

            if (maxCategoryEnd > 0)
                m_categoryCooldowns[categoryId] = maxCategoryEnd;
            else
                m_categoryCooldowns.erase(categoryId);
        }
    }
}

void CooldownManager::clearCategory(int32_t categoryId)
{
    if (categoryId <= 0)
        return;

    // Remove all cooldowns in this category
    for (auto it = m_cooldowns.begin(); it != m_cooldowns.end(); )
    {
        if (it->second.categoryId == categoryId)
            it = m_cooldowns.erase(it);
        else
            ++it;
    }

    m_categoryCooldowns.erase(categoryId);
}

// ============================================================================
// Cooldown Modification
// ============================================================================

void CooldownManager::reduceCooldown(int32_t spellId, int32_t amountMs)
{
    auto it = m_cooldowns.find(spellId);
    if (it == m_cooldowns.end())
        return;

    it->second.endTimeMs -= amountMs;

    // If cooldown expired, remove it
    if (it->second.endTimeMs <= getCurrentTimeMs())
    {
        int32_t categoryId = it->second.categoryId;
        m_cooldowns.erase(it);

        // Recalculate category cooldown
        if (categoryId > 0)
        {
            int64_t maxCategoryEnd = 0;
            for (const auto& [id, entry] : m_cooldowns)
            {
                if (entry.categoryId == categoryId && entry.endTimeMs > maxCategoryEnd)
                {
                    maxCategoryEnd = entry.endTimeMs;
                }
            }

            if (maxCategoryEnd > 0)
                m_categoryCooldowns[categoryId] = maxCategoryEnd;
            else
                m_categoryCooldowns.erase(categoryId);
        }
    }
}

void CooldownManager::reduceCooldownsByPercent(float percent)
{
    if (percent <= 0.0f || percent > 1.0f)
        return;

    int64_t now = getCurrentTimeMs();

    // Reduce all spell cooldowns
    for (auto& [spellId, entry] : m_cooldowns)
    {
        int64_t remaining = entry.endTimeMs - now;
        if (remaining > 0)
        {
            int64_t reduction = static_cast<int64_t>(static_cast<float>(remaining) * percent);
            entry.endTimeMs -= reduction;
        }
    }

    // Reduce all category cooldowns
    for (auto& [categoryId, endTime] : m_categoryCooldowns)
    {
        int64_t remaining = endTime - now;
        if (remaining > 0)
        {
            int64_t reduction = static_cast<int64_t>(static_cast<float>(remaining) * percent);
            endTime -= reduction;
        }
    }

    // Reduce GCD
    if (m_gcdEndTime > now)
    {
        int64_t remaining = m_gcdEndTime - now;
        int64_t reduction = static_cast<int64_t>(static_cast<float>(remaining) * percent);
        m_gcdEndTime -= reduction;
    }

    // Cleanup expired entries
    cleanup();
}

// ============================================================================
// Query Functions
// ============================================================================

std::vector<std::pair<int32_t, int32_t>> CooldownManager::getAllCooldowns() const
{
    std::vector<std::pair<int32_t, int32_t>> result;
    int64_t now = getCurrentTimeMs();

    for (const auto& [spellId, entry] : m_cooldowns)
    {
        if (entry.endTimeMs > now)
        {
            int32_t remaining = static_cast<int32_t>(entry.endTimeMs - now);
            if (remaining >= CooldownConfig::MIN_COOLDOWN_TO_SEND)
            {
                result.emplace_back(spellId, remaining);
            }
        }
    }

    return result;
}

void CooldownManager::cleanup()
{
    int64_t now = getCurrentTimeMs();

    // Remove expired spell cooldowns
    for (auto it = m_cooldowns.begin(); it != m_cooldowns.end(); )
    {
        if (it->second.endTimeMs <= now)
            it = m_cooldowns.erase(it);
        else
            ++it;
    }

    // Remove expired category cooldowns
    for (auto it = m_categoryCooldowns.begin(); it != m_categoryCooldowns.end(); )
    {
        if (it->second <= now)
            it = m_categoryCooldowns.erase(it);
        else
            ++it;
    }

    // Clear GCD if expired
    if (m_gcdEndTime <= now)
        m_gcdEndTime = 0;
}

size_t CooldownManager::getActiveCooldownCount() const
{
    int64_t now = getCurrentTimeMs();
    size_t count = 0;

    for (const auto& [spellId, entry] : m_cooldowns)
    {
        if (entry.endTimeMs > now)
            ++count;
    }

    return count;
}
