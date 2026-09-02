#pragma once

#include <stdint.h>
#include <vector>

// Forward declaration for UnitDefines::Stat
namespace UnitDefines { enum class Stat : int32_t; }

namespace PlayerDefines
{
    // Character classes
    enum class Classes : uint8_t
    {
        None = 0,
        Paladin = 1,
        Mage = 2,
        Ranger = 3,
        Bishop = 4,
        MaxClass
    };

    // Character gender
    enum class Gender : uint8_t
    {
        Male = 0,
        Female = 1
    };

    // Unscoped aliases for client code compatibility
    constexpr Classes None = Classes::None;
    constexpr Classes Paladin = Classes::Paladin;
    constexpr Classes Mage = Classes::Mage;
    constexpr Classes Ranger = Classes::Ranger;
    constexpr Classes Bishop = Classes::Bishop;

    constexpr Gender Male = Gender::Male;
    constexpr Gender Female = Gender::Female;

    // Inventory configuration
    namespace Inventory
    {
        constexpr int NumSlots = 49;      // Total inventory slots
        constexpr int NumSlotsBank = 49;  // Bank slots (client UI limit)
    }

    // Trade configuration
    namespace Trade
    {
        constexpr int MaxSlots = 5;              // Max items per player in trade
        constexpr float TradeDistance = 300.0f; // Max distance in pixels
        constexpr int TradeDistance_Cells = 10; // Max distance in cells (for client)
    }

    // Experience configuration
    namespace Experience
    {
        constexpr int MaxLevelDiffExp = 10;  // Max level difference for XP/level display
    }

    // Chat error codes
    enum class ChatError : uint8_t
    {
        None = 0,
        ChatIgnored,
        CantAcceptQuest,
        TargetBusy,
        DuelDeclined,
        DuelCount3,
        DuelCount2,
        DuelCount1,
        DuelBegin,
        PlayerNotFound,
        PlayerGroupedAlready,
        PartyDeclined,
        PartyFull,
        InstanceFull,
        ErrorTransferingMap,
        CantInviteSelf,
        AlreadyInGuild,
        TargetAlreadyInGuild,
        NotInGuild,
        InsufficientGuildRank,
        GuildDisbanded,
        CantLeaveAsLeader,
        TradeInvalid,
        GuildNameTaken,
        GuildFull,
        InstanceInCombat,
    };

    // World error codes
    enum class WorldError : uint8_t
    {
        None = 0,
        SpellRequirementNotMet,
        NotHighEnoughLevel,
        MissingItem,
        BuybackEmpty,
        EnchantLimit,
        NoEmptySockets,
        VendorNotWorth,
        RewardNotChosen,
        MaximumQuestsTracked,
        InventoryFull,
        QuestNotDone,
        Stunned,
        Confused,
        Feared,
        Sleeping,
        Incapacitated,
        Polymorphed,
        Pacified,
        Charging,
        Rooted,
        Silenced,
        CantEquipItem,
        NotWhileInCombat,
        SpellNotReady,
        CantWhileMoving,
        LineOfSight,
        CasterDead,
        EquipItemRequired,
        TargetDead,
        InvalidTarget,
        NotWhileTargetInCombat,
        TargetPlayersOnly,
        OutOfRange,
        TooClose,
        EquipItemBroken,
        NotEnoughMana,
        CastInProgress,
        OutOfStock,
        NotEnoughGold,
        PerceptionFail,
        AlreadyLooted,
        NotEnoughExp,
        MagicSchoolLock,
        LockpickTooLow,
        WrongClass,
        AlreadyKnowSpell,
        ItemIsSoulBound,
        RequiresLockPicking,
        CantInArena,
        CantInDungeon,

        // Server-specific extensions (client may show generic error)
        InvalidItem,
        NoInventorySpace,
        AlreadyInTrade,
        TargetInTrade,
        TradeFull,
        ItemNotUsable,
        ItemOnCooldown,
        TooFarAway,
        QuestNotAvailable,

        // Phase 9 additions
        InvalidSlot,
        ItemNotFound,
        CantDoInCombat,
        WaypointNotDiscovered,
    };
}

// Helper functions for player-related calculations and display
// Used by both client and server for consistent behavior
namespace PlayerFunctions
{
    // Get class name as string
    inline const char* className(PlayerDefines::Classes classId)
    {
        switch (classId) {
            case PlayerDefines::Classes::Paladin: return "Paladin";
            case PlayerDefines::Classes::Mage: return "Mage";
            case PlayerDefines::Classes::Ranger: return "Ranger";
            case PlayerDefines::Classes::Bishop: return "Bishop";
            default: return "Unknown";
        }
    }

    // Get class display color (RGBA)
    inline uint32_t classColor(PlayerDefines::Classes classId)
    {
        switch (classId) {
            case PlayerDefines::Classes::Paladin: return 0xF58231FF;  // Orange
            case PlayerDefines::Classes::Mage:    return 0x69D2E7FF;  // Light blue
            case PlayerDefines::Classes::Ranger:  return 0xA6E22EFF;  // Green
            case PlayerDefines::Classes::Bishop:  return 0xF8F8F2FF;  // White
            default: return 0x888888FF;  // Gray
        }
    }

    // Calculate cost to upgrade a spell (based on total invested points)
    inline int computeSpellUpgradeCost(int totalInvested)
    {
        // Simple formula: base cost increases with each investment
        return 100 + (totalInvested * 50);
    }

    // Calculate cost to upgrade a stat
    inline int computeStatUpgradeCost(int /*statId*/, int currentLevel)
    {
        if (currentLevel >= 100) return -1;  // Max stat level
        return 10 + (currentLevel * 5);
    }

    // Get maximum stat level cap
    inline int getStatUpgradeCap(int /*statId*/)
    {
        return 100;  // Default max stat level
    }

    // Get required stats for equipping an item based on class and item type
    inline int getRequiredStats(PlayerDefines::Classes /*classId*/, int /*armorType*/, int /*equipType*/)
    {
        return 0;  // TODO: implement proper requirement checking
    }

    // Get bartering bonus percentages
    inline void getBarteringPcts(float* buyBonus, float* sellBonus, int barteringStat)
    {
        float bonus = barteringStat * 0.01f;  // 1% per stat point
        if (buyBonus) *buyBonus = -bonus;     // Buy cheaper
        if (sellBonus) *sellBonus = bonus;    // Sell for more
    }

    // Apply item gold value scaling based on level and flags
    inline void applyItemGoldValueScales(int* goldValue, int /*playerLevel*/, int /*itemFlags*/)
    {
        if (!goldValue) return;
        // No scaling by default
    }

    // Apply bartering bonus to prices
    inline void applyBarterting(int* buyPrice, int* sellPrice, int barteringStat)
    {
        float bonus = barteringStat * 0.01f;
        if (buyPrice) *buyPrice = static_cast<int>(*buyPrice * (1.0f - bonus));
        if (sellPrice) *sellPrice = static_cast<int>(*sellPrice * (1.0f + bonus));
    }

    // Get stats that modify a given stat for a class
    // Fills modStats vector with stats that affect the given stat
    inline void getStatModifier(PlayerDefines::Classes classId, UnitDefines::Stat stat, std::vector<UnitDefines::Stat>& modStats)
    {
        modStats.clear();

        // Primary stat to secondary stat modifier mappings
        // These define which base stats affect derived stats per class
        // Note: These are placeholder implementations - actual game may have different rules

        (void)classId;  // Class-specific modifiers can be added later

        // Example: Strength affects melee damage for all classes
        // Intelligence affects spell critical, etc.
        switch (stat) {
            case UnitDefines::Stat(11): // WeaponValue
                modStats.push_back(UnitDefines::Stat(4)); // Strength
                break;
            case UnitDefines::Stat(13): // RangedWeaponValue
                modStats.push_back(UnitDefines::Stat(5)); // Agility
                break;
            case UnitDefines::Stat(17): // SpellCritical
                modStats.push_back(UnitDefines::Stat(7)); // Intelligence
                break;
            case UnitDefines::Stat(15): // MeleeCritical
                modStats.push_back(UnitDefines::Stat(4)); // Strength
                modStats.push_back(UnitDefines::Stat(5)); // Agility
                break;
            case UnitDefines::Stat(16): // RangedCritical
                modStats.push_back(UnitDefines::Stat(5)); // Agility
                break;
            case UnitDefines::Stat(18): // DodgeRating
                modStats.push_back(UnitDefines::Stat(5)); // Agility
                break;
            case UnitDefines::Stat(19): // BlockRating
                modStats.push_back(UnitDefines::Stat(4)); // Strength
                break;
            default:
                break;
        }
    }
}
