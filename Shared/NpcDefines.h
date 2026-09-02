#pragma once

#include <stdint.h>
#include <string>

namespace NpcDefines
{
    // NPC creature types for tooltips and AI
    enum class Type : uint8_t
    {
        None = 0,
        Beast = 1,
        Humanoid = 2,
        Undead = 3,
        Demon = 4,
        Elemental = 5,
        Dragon = 6,
        Giant = 7,
        Mechanical = 8,
        Aberration = 9,
        MaxType
    };

    // NPC default movement behavior types
    enum class DefaultMovement : uint8_t
    {
        None = 0,    // Stay in place
        Random = 1,  // Wander randomly within range
        Patrol = 2   // Follow waypoint path
    };
}

namespace NpcFunctions
{
    inline std::string typeName(NpcDefines::Type type)
    {
        switch (type)
        {
            case NpcDefines::Type::Beast:      return "Beast";
            case NpcDefines::Type::Humanoid:   return "Humanoid";
            case NpcDefines::Type::Undead:     return "Undead";
            case NpcDefines::Type::Demon:      return "Demon";
            case NpcDefines::Type::Elemental:  return "Elemental";
            case NpcDefines::Type::Dragon:     return "Dragon";
            case NpcDefines::Type::Giant:      return "Giant";
            case NpcDefines::Type::Mechanical: return "Mechanical";
            case NpcDefines::Type::Aberration: return "Aberration";
            default:                           return "";
        }
    }
}
