#pragma once

#include <stdint.h>

namespace UnitDefines
{
    // Unit faction determines hostility and AI behavior
    enum class Faction : uint8_t
    {
        PlayerDefault = 0,  // Default player faction (friendly to other players)
        Friendly = 1,       // Friendly NPCs (quest givers, vendors)
        Neutral = 2,        // Neutral creatures (attackable but don't aggro)
        Hostile = 3,        // Hostile mobs (will attack on sight)
        PvP = 4             // PvP-flagged (hostile to opposing faction players)
    };

    // 8-direction system for unit facing/movement
    // Ordered clockwise for sprite sheet mapping
    enum Direction : uint8_t
    {
        South = 0,
        SouthWest = 1,
        West = 2,
        NorthWest = 3,
        North = 4,
        NorthEast = 5,
        East = 6,
        SouthEast = 7,
        NumDirections = 8
    };

    // Equipment slots - must match client Equipment::convertInterface mapping
    enum class EquipSlot : uint8_t
    {
        None = 0,
        Helm = 1,
        Weapon1 = 2,     // Main hand weapon
        Offhand = 3,     // Shield or offhand weapon
        Ranged = 4,
        Chest = 5,
        Necklace = 6,
        Hands = 7,       // Gloves
        Ring1 = 8,
        Ring2 = 9,
        Feet = 10,
        Legs = 11,
        Belt = 12,
        MaxSlot
    };

    // Unit stat identifiers (aligns with item_template stat_type values)
    enum class Stat : int32_t
    {
        NullStat = 0,
        Mana = 1,
        Health = 2,
        ArmorValue = 3,
        Strength = 4,
        Agility = 5,
        Willpower = 6,
        Intelligence = 7,
        Courage = 8,
        Regeneration = 9,
        Meditate = 10,
        WeaponValue = 11,
        MeleeValue = WeaponValue,
        MeleeCooldown = 12,
        RangedWeaponValue = 13,
        RangedValue = RangedWeaponValue,
        RangedCooldown = 14,
        MeleeCritical = 15,
        RangedCritical = 16,
        SpellCritical = 17,
        DodgeRating = 18,
        BlockRating = 19,
        ResistFrost = 20,
        ResistFire = 21,
        ResistShadow = 22,
        ResistHoly = 23,
        Bartering = 24,
        Lockpicking = 25,
        Perception = 26,
        StaffSkill = 27,
        MaceSkill = 28,
        AxesSkill = 29,
        SwordSkill = 30,
        RangedSkill = 31,
        DaggerSkill = 32,
        WandSkill = 33,
        ShieldSkill = 34,
        NpcMeleeSkill = 35,
        NpcRangedSkill = 36,
        ParryChanceBonus = 37,
        BlockChanceBonus = 38,
        DodgeChanceBonus = 39,
        ParryRating = 40,
        Fortitude = 41,
        NumStats = 42,
    };

    // NPC flags (from npc_template.npcFlags)
    enum class NpcFlags : int32_t
    {
        None        = 0,
        Gossip      = 0x0001,      // Has gossip menu
        QuestGiver  = 0x0002,      // Can give quests
        Vendor      = 0x0004,      // Can sell items
        Trainer     = 0x0008,      // Can train skills
        FlightPath  = 0x0010,      // Flight master
        InnKeeper   = 0x0020,      // Set hearthstone
        Banker      = 0x0040,      // Access bank
        Repair      = 0x0080,      // Can repair items
        Stable      = 0x0100,      // Stable master
        Tabard      = 0x0200,      // Guild tabard vendor
        AuctionHouse= 0x0400,      // Auctioneer
        MailBox     = 0x0800,      // Access mailbox
    };

    // Animation IDs
    enum class AnimId : uint8_t
    {
        Stance = 0,
        Run = 1,
        Attack = 2,
        Cast = 3,
        Hit = 4,
        Die = 5,
        CastAlt = 6,    // Alternative cast animation
        Spawn = 7,      // Spawn/appear animation
        CritDie = 8,    // Critical death animation
        Swing = 9,      // Melee swing animation
        Block = 10,     // Block animation
        Shoot = 11,     // Ranged attack animation
    };

    // Unit body parts for rendering
    enum class BodyPart : uint8_t
    {
        Feet = 0,
        Legs = 1,
        Torso = 2,
        Hands = 3,
        Head = 4,
        Weapon = 5,
        Offhand = 6,
        WeaponRanged = 7,
        MaxPart
    };

    // Convenience constants for backward compatibility
    // Allows UnitDefines::Stance instead of UnitDefines::AnimId::Stance
    constexpr AnimId Stance = AnimId::Stance;
    constexpr AnimId Run = AnimId::Run;
    constexpr AnimId Attack = AnimId::Attack;
    constexpr AnimId Cast = AnimId::Cast;
    constexpr AnimId Hit = AnimId::Hit;
    constexpr AnimId Die = AnimId::Die;

    // Equipment slot convenience constants
    constexpr EquipSlot Weapon1 = EquipSlot::Weapon1;

    // Faction convenience constants
    constexpr Faction Friendly = Faction::Friendly;
    constexpr Faction Hostile = Faction::Hostile;
    constexpr Faction Neutral = Faction::Neutral;

    // Stat constant for iteration bounds (as integer for comparisons)
    constexpr int NumStatsInt = static_cast<int>(Stat::NumStats);

    // Arithmetic operators for Stat enum to allow indexing operations
    inline constexpr int operator+(int lhs, Stat rhs) { return lhs + static_cast<int>(rhs); }
    inline constexpr int operator+(Stat lhs, int rhs) { return static_cast<int>(lhs) + rhs; }
    inline constexpr int operator-(int lhs, Stat rhs) { return lhs - static_cast<int>(rhs); }
}

// Helper functions for unit stat display
namespace UnitFunctions
{
    // Get full stat name for display
    inline const char* statName(UnitDefines::Stat stat)
    {
        switch (stat) {
            case UnitDefines::Stat::NullStat: return "None";
            case UnitDefines::Stat::Mana: return "Mana";
            case UnitDefines::Stat::Health: return "Health";
            case UnitDefines::Stat::ArmorValue: return "Armor Value";
            case UnitDefines::Stat::Strength: return "Strength";
            case UnitDefines::Stat::Agility: return "Agility";
            case UnitDefines::Stat::Willpower: return "Willpower";
            case UnitDefines::Stat::Intelligence: return "Intelligence";
            case UnitDefines::Stat::Courage: return "Courage";
            case UnitDefines::Stat::Regeneration: return "Regeneration";
            case UnitDefines::Stat::Meditate: return "Meditate";
            case UnitDefines::Stat::WeaponValue: return "Weapon Value";
            case UnitDefines::Stat::MeleeCooldown: return "Melee Cooldown";
            case UnitDefines::Stat::RangedWeaponValue: return "Ranged Weapon Value";
            case UnitDefines::Stat::RangedCooldown: return "Ranged Cooldown";
            case UnitDefines::Stat::MeleeCritical: return "Melee Critical";
            case UnitDefines::Stat::RangedCritical: return "Ranged Critical";
            case UnitDefines::Stat::SpellCritical: return "Spell Critical";
            case UnitDefines::Stat::DodgeRating: return "Dodge Rating";
            case UnitDefines::Stat::BlockRating: return "Block Rating";
            case UnitDefines::Stat::ResistFrost: return "Resist Frost";
            case UnitDefines::Stat::ResistFire: return "Resist Fire";
            case UnitDefines::Stat::ResistShadow: return "Resist Shadow";
            case UnitDefines::Stat::ResistHoly: return "Resist Holy";
            case UnitDefines::Stat::Bartering: return "Bartering";
            case UnitDefines::Stat::Lockpicking: return "Lockpicking";
            case UnitDefines::Stat::Perception: return "Perception";
            case UnitDefines::Stat::StaffSkill: return "Staff Skill";
            case UnitDefines::Stat::MaceSkill: return "Mace Skill";
            case UnitDefines::Stat::AxesSkill: return "Axes Skill";
            case UnitDefines::Stat::SwordSkill: return "Sword Skill";
            case UnitDefines::Stat::RangedSkill: return "Ranged Skill";
            case UnitDefines::Stat::DaggerSkill: return "Dagger Skill";
            case UnitDefines::Stat::WandSkill: return "Wand Skill";
            case UnitDefines::Stat::ShieldSkill: return "Shield Skill";
            case UnitDefines::Stat::NpcMeleeSkill: return "NPC Melee Skill";
            case UnitDefines::Stat::NpcRangedSkill: return "NPC Ranged Skill";
            case UnitDefines::Stat::ParryChanceBonus: return "Parry Chance Bonus";
            case UnitDefines::Stat::BlockChanceBonus: return "Block Chance Bonus";
            case UnitDefines::Stat::DodgeChanceBonus: return "Dodge Chance Bonus";
            case UnitDefines::Stat::ParryRating: return "Parry Rating";
            case UnitDefines::Stat::Fortitude: return "Fortitude";
            default: return "Unknown";
        }
    }

    // Get abbreviated stat name for compact display
    inline const char* statAbbr(UnitDefines::Stat stat)
    {
        switch (stat) {
            case UnitDefines::Stat::NullStat: return "---";
            case UnitDefines::Stat::Mana: return "MNA";
            case UnitDefines::Stat::Health: return "HP";
            case UnitDefines::Stat::ArmorValue: return "ARM";
            case UnitDefines::Stat::Strength: return "STR";
            case UnitDefines::Stat::Agility: return "AGI";
            case UnitDefines::Stat::Willpower: return "WIL";
            case UnitDefines::Stat::Intelligence: return "INT";
            case UnitDefines::Stat::Courage: return "CRG";
            case UnitDefines::Stat::Regeneration: return "RGN";
            case UnitDefines::Stat::Meditate: return "MED";
            case UnitDefines::Stat::WeaponValue: return "WPN";
            case UnitDefines::Stat::MeleeCooldown: return "MCD";
            case UnitDefines::Stat::RangedWeaponValue: return "RWV";
            case UnitDefines::Stat::RangedCooldown: return "RCD";
            case UnitDefines::Stat::MeleeCritical: return "MCR";
            case UnitDefines::Stat::RangedCritical: return "RCR";
            case UnitDefines::Stat::SpellCritical: return "SCR";
            case UnitDefines::Stat::DodgeRating: return "DDG";
            case UnitDefines::Stat::BlockRating: return "BLK";
            case UnitDefines::Stat::ResistFrost: return "RFT";
            case UnitDefines::Stat::ResistFire: return "RFR";
            case UnitDefines::Stat::ResistShadow: return "RSH";
            case UnitDefines::Stat::ResistHoly: return "RHL";
            case UnitDefines::Stat::Bartering: return "BAR";
            case UnitDefines::Stat::Lockpicking: return "LCK";
            case UnitDefines::Stat::Perception: return "PER";
            case UnitDefines::Stat::StaffSkill: return "STF";
            case UnitDefines::Stat::MaceSkill: return "MAC";
            case UnitDefines::Stat::AxesSkill: return "AXE";
            case UnitDefines::Stat::SwordSkill: return "SWD";
            case UnitDefines::Stat::RangedSkill: return "RNG";
            case UnitDefines::Stat::DaggerSkill: return "DGR";
            case UnitDefines::Stat::WandSkill: return "WND";
            case UnitDefines::Stat::ShieldSkill: return "SLD";
            case UnitDefines::Stat::NpcMeleeSkill: return "NML";
            case UnitDefines::Stat::NpcRangedSkill: return "NRG";
            case UnitDefines::Stat::ParryChanceBonus: return "PRY";
            case UnitDefines::Stat::BlockChanceBonus: return "BCB";
            case UnitDefines::Stat::DodgeChanceBonus: return "DCB";
            case UnitDefines::Stat::ParryRating: return "PRT";
            case UnitDefines::Stat::Fortitude: return "FRT";
            default: return "???";
        }
    }
}
