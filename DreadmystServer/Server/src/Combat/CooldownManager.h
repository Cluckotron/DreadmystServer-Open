// CooldownManager - Tracks spell cooldowns for players
// Task 5.7: Cooldown System

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <utility>

class Player;
class StlBuffer;

// ============================================================================
// Cooldown Configuration
// ============================================================================

namespace CooldownConfig
{
    // Global Cooldown (GCD) duration in milliseconds
    // Most spells trigger a 1.5 second GCD
    constexpr int32_t GCD_DURATION_MS = 1500;

    // Special category ID for GCD (not a real spell category)
    constexpr int32_t GCD_CATEGORY = -1;

    // Minimum cooldown duration to send to client (ms)
    // Don't send cooldowns shorter than this
    constexpr int32_t MIN_COOLDOWN_TO_SEND = 100;
}

// ============================================================================
// Cooldown Entry - Stored for each active cooldown
// ============================================================================

struct CooldownEntry
{
    int32_t spellId = 0;        // The spell this cooldown is for
    int64_t endTimeMs = 0;      // When the cooldown ends (ms since epoch)
    int32_t categoryId = 0;     // Cooldown category (for shared cooldowns)
};

// ============================================================================
// CooldownManager - Manages cooldowns for a single player
// ============================================================================

class CooldownManager
{
public:
    CooldownManager() = default;
    ~CooldownManager() = default;

    // Start a cooldown for a spell
    // durationMs is the cooldown duration in milliseconds
    void startCooldown(int32_t spellId, int32_t durationMs, int32_t categoryId = 0);

    // Start the Global Cooldown
    void startGCD();

    // Check if a spell is on cooldown
    bool isOnCooldown(int32_t spellId) const;

    // Check if Global Cooldown is active
    bool isOnGCD() const;

    // Check if a category is on cooldown
    bool isCategoryOnCooldown(int32_t categoryId) const;

    // Get remaining cooldown time in milliseconds (0 if not on cooldown)
    int32_t getRemainingCooldown(int32_t spellId) const;

    // Get remaining GCD time in milliseconds
    int32_t getRemainingGCD() const;

    // Get remaining category cooldown time
    int32_t getRemainingCategoryCooldown(int32_t categoryId) const;

    // Clear all cooldowns
    void clearAll();

    // Clear a specific spell cooldown
    void clearCooldown(int32_t spellId);

    // Clear all cooldowns in a category
    void clearCategory(int32_t categoryId);

    // Reduce cooldown by amount (in ms) - for cooldown reduction effects
    void reduceCooldown(int32_t spellId, int32_t amountMs);

    // Reduce all cooldowns by a percentage (0.0 to 1.0)
    void reduceCooldownsByPercent(float percent);

    // Get all active cooldowns (spellId, remainingMs)
    // Used for sending to client on login
    std::vector<std::pair<int32_t, int32_t>> getAllCooldowns() const;

    // Cleanup expired cooldowns (optional - done lazily otherwise)
    void cleanup();

    // Get total number of active cooldowns (for debugging)
    size_t getActiveCooldownCount() const;

private:
    // Get current time in milliseconds
    static int64_t getCurrentTimeMs();

    // Spell ID -> CooldownEntry
    std::unordered_map<int32_t, CooldownEntry> m_cooldowns;

    // Category ID -> End time (for shared cooldowns)
    std::unordered_map<int32_t, int64_t> m_categoryCooldowns;

    // Global Cooldown end time
    int64_t m_gcdEndTime = 0;
};
