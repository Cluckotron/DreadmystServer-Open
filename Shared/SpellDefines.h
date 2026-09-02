#pragma once

#include <stdint.h>

namespace SpellDefines
{
    constexpr int NumEffectIdx = 3;

    // Spell effect types (values aligned to game.db spell_template.effect*)
    enum class Effects : uint8_t
    {
        None = 0,
        SchoolDamage = 1,
        Damage = SchoolDamage,  // Server uses Damage while client uses SchoolDamage
        Teleport = 2,
        ApplyAura = 3,
        ApplyAreaAura = 4,      // Reserved (not currently used in game.db)
        ManaDrain = 5,          // Reserved
        HealthDrain = 8,        // Reserved
        RestoreMana = 11,
        RestoreManaPct = 28,
        ManaBurn = 17,
        Heal = 6,
        HealPct = 27,
        Resurrect = 7,
        CreateItem = 12,        // Reserved
        SummonNpc = 10,
        Dispel = 13,
        WeaponDamage = 14,
        Threat = 18,
        InterruptCast = 22,
        SummonObject = 23,
        TriggerSpell = 24,
        ScriptEffect = 26,      // Reserved
        KnockBack = 25,
        ApplyGemSocket = 36,
        DestroyGems = 44,
        CombineItem = 45,
        ExtractOrb = 46,
        ApplyOrbEnchant = 40,
        ApplyOrbEnchantArcane = 47,  // Arcane orbs use a separate value in game.db
        Duel = 38,
        Charge = 37,
        TeleportForward = 29,   // Reserved
        MeleeAtk = 30,
        RangedAtk = 31,
        LootEffect = 32,
        Kill = 33,              // Reserved
        Gossip = 34,
        Inspect = 35,
        SlideFrom = 39,
        LearnSpell = 41,
        NearestWp = 42,
        PullTo = 43,
    };

    // Hit result types
    enum class HitResult : uint8_t
    {
        Normal = 0,
        Crit = 1,
        Miss = 2,
        Dodge = 3,
        Parry = 4,
        Block = 5,
        Evade = 6,
        Immune = 7,
        Absorb = 8,
        Resist = 9,
    };

    // Aura types for buffs/debuffs (values aligned to game.db apply-aura data1)
    enum class AuraType : uint8_t
    {
        None = 0,
        PeriodicDamage = 1,          // DoT
        PeriodicHeal = 2,            // HoT
        InflictMechanic = 3,
        ModifyStat = 4,
        ModifyStatPct = 5,
        AbsorbDamage = 6,
        PeriodicMeleeDamage = 7,     // Reserved
        PeriodicHealPct = 8,         // Reserved
        PeriodicTriggerSpell = 9,    // Reserved
        PeriodicRestoreMana = 10,
        ModifyMoveSpeedPct = 11,
        MechanicImmunity = 12,
        SchoolImmunity = 13,
        ModifyDamageDealtPct = 14,
        ModifyDamageReceivedPct = 15,
        ModifyMeleeSpeedPct = 16,
        ModifyRangedSpeedPct = 17,   // Reserved
        ModifyResistance = 18,       // Reserved
        Model = 19,
        PeriodicBurnMana = 20,
        Proc = 21,
        ModifyHealingDealtPct = 22,
        ModifyHealingRecvPct = 23,
        PeriodicRestoreManaPct = 24, // Reserved
        RepopOntopOfSelf = 26,

        // Legacy aliases still referenced by server logic
        ModStat = ModifyStat,
        ModDamage = ModifyDamageDealtPct,
        ModSpeed = ModifyMoveSpeedPct,
        Stun = 30,                   // Reserved legacy values (not used in game.db)
        Root = 31,
        Silence = 32,
    };

    // Spell schools/damage types
    enum class School : uint8_t
    {
        Physical = 0,
        Fire = 1,
        Frost = 2,
        Arcane = 3,
        Nature = 4,
        Shadow = 5,
        Holy = 6,
    };

    // Dispel types for buffs/debuffs that can be dispelled
    enum class DispelType : uint8_t
    {
        None = 0,
        Magic = 1,
        Curse = 2,
        Disease = 3,
        Poison = 4,
        Physical = 5, // Unused in game.db, reserved
    };

    // Mechanics (bitmask values derived from game.db mechanic indices)
    enum class Mechanics : uint32_t
    {
        None = 0,
        Confused = 1u << 0,      // index 1
        Pacify = 1u << 1,        // index 2
        Fear = 1u << 2,          // index 3
        Root = 1u << 3,          // index 4
        Silence = 1u << 4,       // index 5
        Sleep = 1u << 5,         // index 6
        Snare = 1u << 6,         // index 7
        Stun = 1u << 7,          // index 8
        Incapacitated = 1u << 8, // index 9
        Disrupt = 1u << 9,       // index 10
        Polymorph = 1u << 10,    // index 11
        Charging = 1u << 11,     // index 12
        Stealth = 1u << 12,      // index 13

        Incapacitate = Incapacitated,
    };

    // Spell attribute flags (bitmask values; some are reserved/unused)
    enum class Attributes : uint64_t
    {
        None = 0,
        AutoApproach = 1ull << 0,
        CantTargetSelf = 1ull << 1,
        CanTargetDead = 1ull << 2,
        CantCrit = 1ull << 3,
        IgnoreArmor = 1ull << 4,
        TargetsGround = 1ull << 33,
        TargetsItem = 1ull << 35,
        IgnoreInvulnerability = 1ull << 5,
        IgnoreLOS = 1ull << 6,
        IgnoreResistances = 1ull << 7,
        ImpossibleBlock = 1ull << 14,
        ImpossibleDodge = 1ull << 15,
        ImpossibleMiss = 1ull << 16,
        ImpossibleParry = 1ull << 17,
        NoHealBonus = 1ull << 10,
        NoSpellBonus = 1ull << 11,
        NoThreat = 1ull << 12,
        NoAggro = 1ull << 13,
        NotInCombat = 1ull << 22,
        OnePerCaster = 1ull << 20,
        OnePerTarget = 1ull << 21,
        Passive = 1ull << 18,
        SameStackForAllCasters = 1ull << 24,
        TargetNotInCombat = 1ull << 27,
        Triggered = 1ull << 25,
        AnimLockStart = 1ull << 30,
        NoGoLock = 1ull << 31,
        DontStopCastingSound = 1ull << 19,
        IgnoreStun = 1ull << 36,
        IgnoreIncapacitated = 1ull << 37,
        IgnoreSleep = 1ull << 38,
        IgnoreConfused = 1ull << 39,
        IgnoreFear = 1ull << 40,
        IgnorePolymorph = 1ull << 41,
        TargetPlayersOnly = 1ull << 29,
        MouseoverTargeting = 1ull << 34,
        PersistsThroughDeath = 1ull << 42,
        NotInArena = 1ull << 8,
        NotInDungeon = 1ull << 9,
    };

    // Targeting types (values aligned to game.db spell_template.effect*_targetType)
    enum class TargetType : int32_t
    {
        None = 0,
        Unit_Caster = 1,
        Unit_Friendly = 2,
        Unit_AreaSrc_Friendly = 3,
        Unit_AreaDst_Friendly = 4,             // Reserved
        Target_GameObject = 13,
        Target_Item = 20,
        Unit_Hostile = 14,
        Unit_AreaSrc_Hostile = 15,
        Unit_AreaDst_Hostile = 19,
        Unit_AreaDst_Friendly_FromDst = 5,     // Reserved
        Unit_AreaDst_Hostile_FromDst = 6,      // Reserved
        Unit_Any = 17,
    };

    // Aura/Spell interruption causes (indices; actual flags are 1 << (index-1))
    enum class InterruptCauses : uint8_t
    {
        Movement = 4,        // 1 << 3 = 8
        StartCasting = 3,    // 1 << 2 = 4
        TakeDamage = 6,      // 1 << 5 = 32
        TakeDamage_Direct = 7, // 1 << 6 = 64
        DisruptMechanic = 5, // 1 << 4 = 16
    };

    enum class ProcFlags : uint32_t
    {
        None = 0,
        ProcFlag_HolderTookDamage = 1u << 0,
        ProcFlag_HolderDealtDamage = 1u << 1,
        ProcFlag_HolderWasImmune = 1u << 2,
        ProcFlag_HolderDodged = 1u << 3,
    };

    enum class ProcType : uint8_t
    {
        None = 0,
        ProcType_RemoveCharge = 1,
    };

    enum class AiTargetType : uint8_t
    {
        Target_T_Victim = 0,
        Target_T_RandomHated = 1,
        Target_T_RandomFriendly = 2,
        Target_T_MissingMostHpFriendly = 3,
    };

    // Miscellaneous spell constants
    namespace Misc
    {
        constexpr int MaxSpellLevel = 10;   // Maximum level a spell can be upgraded to
    }

    // Static/special spell IDs from the released Dreadmyst game.db.
    // These IDs are NOT a synthetic 1..N range: entries 1..11 are real class spells.
    namespace StaticSpells
    {
        constexpr int MeleeSpell = 81;      // Melee Swing / Auto Attack
        constexpr int RangedSpell = 82;     // Ranged Attack / Auto Shot
        constexpr int LootVisual = 84;      // Loot Sparkle visual
        constexpr int LootUnit = 85;        // Loot a dead unit
        constexpr int QuestReady = 86;      // Quest available visual
        constexpr int QuestDone = 87;       // Quest turn-in available visual
        constexpr int NpcGossip = 88;       // Talk to NPC
        constexpr int LootGameObj = 127;    // Interact with/loot a game object
        constexpr int OfferDuel = 128;      // Challenge player to duel
        constexpr int SleepRest = 245;      // Rest / Sleep
        constexpr int Lockpicking = 273;    // Pick Lock
    }

    // Spell visual IDs
    constexpr int CritVisual = 83;          // spell_visual entry for crit impact

    // Convenience aliases for StaticSpells (used by client code)
    // Allows SpellDefines::LootUnit instead of SpellDefines::StaticSpells::LootUnit
    constexpr int MeleeSpell = StaticSpells::MeleeSpell;
    constexpr int RangedSpell = StaticSpells::RangedSpell;
    constexpr int LootUnit = StaticSpells::LootUnit;
    constexpr int LootGameObj = StaticSpells::LootGameObj;
    constexpr int SleepRest = StaticSpells::SleepRest;
    constexpr int Lockpicking = StaticSpells::Lockpicking;
    constexpr int OfferDuel = StaticSpells::OfferDuel;
    constexpr int LootVisual = StaticSpells::LootVisual;
    constexpr int NpcGossip = StaticSpells::NpcGossip;
    constexpr int QuestDone = StaticSpells::QuestDone;
    constexpr int QuestReady = StaticSpells::QuestReady;
}

namespace SpellFunctions
{
    inline bool isStaticSpell(int spellId)
    {
        using namespace SpellDefines::StaticSpells;
        switch (spellId)
        {
            case MeleeSpell:
            case RangedSpell:
            case LootVisual:
            case LootUnit:
            case QuestReady:
            case QuestDone:
            case NpcGossip:
            case LootGameObj:
            case OfferDuel:
            case SleepRest:
            case Lockpicking:
                return true;
            default:
                return false;
        }
    }
}
