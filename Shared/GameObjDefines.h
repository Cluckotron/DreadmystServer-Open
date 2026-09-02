#pragma once

#include <stdint.h>

namespace GameObjDefines
{
    // Game object types
    enum class Type : uint8_t
    {
        None = 0,
        Chest = 1,
        Door = 2,
        Lever = 3,
        Trap = 4,
        SpellFocus = 5,
        QuestGiver = 6,
        Waypoint = 7,
    };

    // Game object toggle state (for doors, chests, etc.)
    enum ToggleState : uint8_t
    {
        Closed = 0,
        Open = 1,
    };

    // Game object flags
    enum class Flags : uint32_t
    {
        None = 0,
        Interactable = 1 << 0,
        Locked = 1 << 1,
        Consumable = 1 << 2,  // Disappears after use
        NoMouseoverBrighten = 1 << 3,
        ShowName = 1 << 4,
        Lockpicking = 1 << 5,
        RenderOnFloor = 1 << 6,
    };
}

// Alias for client-side code that uses shorter namespace name
namespace GoDefines = GameObjDefines;
