#pragma once

#include <stdint.h>

namespace QuestDefines
{
    // Quest objective tally types
    enum TallyType : uint8_t
    {
        TallyNpc = 0,       // Kill NPC
        TallyItem = 1,      // Collect item
        TallyGameObj = 2,   // Interact with object
        TallySpell = 3,     // Cast spell
    };

    // Quest flags
    enum class Flags : uint32_t
    {
        QuestFlagRepeatable = 1 << 0,
    };

    // Quest status
    enum class Status : uint8_t
    {
        NotStarted = 0,
        InProgress = 1,
        Complete = 2,
        Rewarded = 3,
    };
}
