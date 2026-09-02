#pragma once

#include <stdint.h>

namespace ObjDefines
{
    // Object variable IDs - used for dynamic unit state
    // Server sends updates via Server_ObjectVariable packet
    enum class Variable : uint16_t
    {
        None = 0,

        // Core stats
        Health = 1,
        MaxHealth = 2,
        Mana = 3,
        MaxMana = 4,
        Energy = 5,
        MaxEnergy = 6,

        // Character info
        Level = 10,
        Experience = 11,
        Gold = 12,
        Money = Gold,         // Client uses Money
        Progression = 13,     // XP progress towards next level

        // Combat state
        InCombat = 20,
        IsDead = 21,
        IsStunned = 22,
        IsSilenced = 23,
        IsRooted = 24,

        // Faction/PvP
        Faction = 30,
        FactionId = Faction,  // Client uses FactionId
        PvPFlag = 31,

        // Movement
        Speed = 40,
        MoveSpeedPct = Speed, // Client uses MoveSpeedPct

        // Dynamic flags
        DynLootable = 50,
        DynSparkle = 51,
        DynGossipStatus = 52,
        DynInteractable = 53,
        DynGreyTagged = 54,

        // Model/animation
        ModelId = 60,
        ModelScale = 61,
        IsAnimating = 62,
        IsSliding = 63,

        // Mechanics/reactions
        MechanicMask = 70,
        ReactionMask_In = 71,
        ReactionMask_Out = 72,

        // Game object variables
        GoFlags = 80,
        LockpickingLevel = 81,
        ToggleState = 82,

        // Player meta
        MailLoot = 90,
        GameMaster = 91,
        GmInvisible = 92,
        ZoneType = 93,
        Moderator = 94,

        // Ratings/counters
        CombatRating = 100,
        CombatRatingMax = 101,
        NumInvestedSpells = 102,
        PkCount = 103,
        ArenaRating = 104,
        InArenaQueue = 105,
        Elite = 106,
        Boss = 107,

        // Stat variable range (StatsStart + UnitDefines::Stat::*)
        StatsStart = 1000,
        StatsEnd = 1100,

        MaxVariable
    };

    enum class GossipStatus : uint8_t
    {
        None = 0,
        GossipAvailable = 1,
        QuestAvailable = 2,
        QuestComplete = 3,
    };

    // Legacy/constants used directly by client code
    constexpr int DynLootable = static_cast<int>(Variable::DynLootable);
    constexpr int DynSparkle = static_cast<int>(Variable::DynSparkle);
    constexpr int DynGossipStatus = static_cast<int>(Variable::DynGossipStatus);
    constexpr int DynInteractable = static_cast<int>(Variable::DynInteractable);
    constexpr int DynGreyTagged = static_cast<int>(Variable::DynGreyTagged);
    constexpr int PvP = static_cast<int>(Variable::PvPFlag);
    constexpr int StatsStart = static_cast<int>(Variable::StatsStart);
    constexpr int StatsEnd = static_cast<int>(Variable::StatsEnd);
    constexpr int PkCount = static_cast<int>(Variable::PkCount);
    constexpr int ArenaRating = static_cast<int>(Variable::ArenaRating);

    // Gossip status convenience constants
    constexpr int GossipNone = static_cast<int>(GossipStatus::None);
    constexpr int GossipAvailable = static_cast<int>(GossipStatus::GossipAvailable);
    constexpr int QuestAvailable = static_cast<int>(GossipStatus::QuestAvailable);
    constexpr int QuestComplete = static_cast<int>(GossipStatus::QuestComplete);
}
