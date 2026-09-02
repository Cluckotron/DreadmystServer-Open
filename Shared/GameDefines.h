#pragma once

#include <stdint.h>

namespace GameDefines
{
    // Zone types for PvP rules
    enum class ZoneTypes : uint8_t
    {
        Neutral = 0,    // No special rules
        Friendly = 1,   // Safe zone, no PvP
        Hostile = 2,    // Enemy territory
        Contested = 3,  // Open PvP zone
    };
}
