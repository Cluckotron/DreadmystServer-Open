// CombatFormulas - Damage, healing, and combat roll calculations
// Task 5.4: Damage Calculation
// Task 5.5: Heal Calculation
// Task 5.6: Hit/Miss/Crit/Dodge System

#pragma once

#include <cstdint>
#include "SpellDefines.h"

// Forward declarations
class Entity;
struct SpellTemplate;

// ============================================================================
// Combat Result Structures
// ============================================================================

// Result of a hit roll
enum class HitResult : uint8_t
{
    Hit = 0,            // Normal hit
    Crit = 1,           // Critical hit (extra damage/heal)
    Miss = 2,           // Attack missed
    Dodge = 3,          // Target dodged (physical)
    Parry = 4,          // Target parried (physical, with weapon)
    Block = 5,          // Target blocked (physical, with shield)
    Resist = 6,         // Spell resisted (magic)
    Immune = 7,         // Target is immune
    Absorb = 8,         // Damage absorbed by shield
    GlancingBlow = 9,   // Reduced damage hit (melee, high level target)
};

// Damage calculation result
struct DamageInfo
{
    int32_t baseDamage = 0;         // Before modifiers
    int32_t finalDamage = 0;        // After all modifiers
    int32_t absorbed = 0;           // Amount absorbed by shields
    int32_t resisted = 0;           // Amount resisted (partial resist)
    SpellDefines::School school = SpellDefines::School::Physical;
    HitResult hitResult = HitResult::Hit;
    int32_t overkill = 0;           // Damage beyond killing blow
    bool killedTarget = false;       // Did this kill the target?
};

// Healing calculation result
struct HealInfo
{
    int32_t baseHeal = 0;           // Before modifiers
    int32_t finalHeal = 0;          // After all modifiers
    int32_t overheal = 0;           // Healing beyond max health
    HitResult hitResult = HitResult::Hit;  // Can crit
    bool fullHeal = false;          // Did this bring target to full?
};

// ============================================================================
// Combat Formulas
// ============================================================================

namespace CombatFormulas
{
    // ==========================================================================
    // Configuration Constants
    // ==========================================================================

    namespace Config
    {
        // Damage variance (±10%)
        constexpr float DAMAGE_VARIANCE = 0.10f;

        // Critical hit multiplier (2x damage)
        constexpr float CRIT_MULTIPLIER = 2.0f;

        // Glancing blow multiplier (reduced damage)
        constexpr float GLANCING_MULTIPLIER = 0.70f;

        // Armor constant for diminishing returns formula
        // damage_reduction = armor / (armor + ARMOR_CONSTANT * target_level)
        constexpr float ARMOR_CONSTANT = 400.0f;

        // Base hit chance (%)
        constexpr int BASE_HIT_CHANCE = 95;

        // Miss chance per level difference (when attacker is lower level)
        constexpr int MISS_PER_LEVEL = 5;

        // Base crit chance (%)
        constexpr int BASE_CRIT_CHANCE = 5;

        // Base dodge chance (%)
        constexpr int BASE_DODGE_CHANCE = 5;

        // Base parry chance (%) - requires weapon
        constexpr int BASE_PARRY_CHANCE = 5;

        // Base block chance (%) - requires shield
        constexpr int BASE_BLOCK_CHANCE = 10;

        // Resist chance per level difference for magic (%)
        constexpr int RESIST_PER_LEVEL = 3;

        // Max level difference for combat (beyond this, always miss/resist)
        constexpr int MAX_LEVEL_DIFF = 10;

        // Minimum damage floor (always deal at least this much)
        constexpr int MIN_DAMAGE = 1;
    }

    // Expose constants for external use (Task 5.14)
    constexpr float CRIT_MULTIPLIER = Config::CRIT_MULTIPLIER;
    constexpr float GLANCING_MULTIPLIER = Config::GLANCING_MULTIPLIER;
    constexpr int MIN_DAMAGE = Config::MIN_DAMAGE;

    // ==========================================================================
    // Combat Roll Functions (Task 5.6)
    // ==========================================================================

    // Roll to hit - determines hit result before damage calculation
    HitResult rollToHit(Entity* attacker, Entity* victim, const SpellTemplate* spell);

    // Roll for melee hit (basic attack, no spell) - Task 5.14
    HitResult rollMeleeHit(Entity* attacker, Entity* victim);

    // Get hit chance percentage
    int getHitChance(Entity* attacker, Entity* victim, const SpellTemplate* spell);

    // Get crit chance percentage
    int getCritChance(Entity* attacker, Entity* victim, const SpellTemplate* spell);

    // Get dodge chance percentage (physical only)
    int getDodgeChance(Entity* victim, const SpellTemplate* spell);

    // Get parry chance percentage (physical only, requires weapon)
    int getParryChance(Entity* victim, const SpellTemplate* spell);

    // Get block chance percentage (physical only, requires shield)
    int getBlockChance(Entity* victim, const SpellTemplate* spell);

    // Get resist chance percentage (magic only)
    int getResistChance(Entity* attacker, Entity* victim, const SpellTemplate* spell);

    // ==========================================================================
    // Damage Calculation (Task 5.4)
    // ==========================================================================

    // Calculate damage for a spell effect
    DamageInfo calculateDamage(Entity* attacker, Entity* victim, const SpellTemplate* spell, int effectIndex);

    // Calculate base damage from spell and caster stats
    int32_t getBaseDamage(Entity* attacker, const SpellTemplate* spell, int effectIndex);

    // Apply damage modifiers (buffs, debuffs)
    int32_t applyDamageModifiers(int32_t damage, Entity* attacker, Entity* victim, const SpellTemplate* spell);

    // Calculate armor damage reduction (physical only)
    int32_t calculateArmorReduction(int32_t damage, Entity* victim);

    // Apply armor reduction - convenience wrapper (Task 5.14)
    int32_t applyArmorReduction(int32_t damage, int32_t armor);

    // Calculate magic resistance reduction
    int32_t calculateResistReduction(int32_t damage, Entity* victim, SpellDefines::School school);

    // Apply damage variance (±10%)
    int32_t applyDamageVariance(int32_t damage);

    // ==========================================================================
    // Healing Calculation (Task 5.5)
    // ==========================================================================

    // Calculate healing for a spell effect
    HealInfo calculateHeal(Entity* healer, Entity* target, const SpellTemplate* spell, int effectIndex);

    // Calculate base heal from spell and caster stats
    int32_t getBaseHeal(Entity* healer, const SpellTemplate* spell, int effectIndex);

    // Apply healing modifiers (buffs, debuffs)
    int32_t applyHealModifiers(int32_t heal, Entity* healer, Entity* target, const SpellTemplate* spell);

    // ==========================================================================
    // Utility Functions
    // ==========================================================================

    // Roll a random number 0-99
    int roll100();

    // Check if a percentage roll succeeds
    bool rollChance(int chance);

    // Get level difference (attacker - victim), clamped to ±MAX_LEVEL_DIFF
    int getLevelDifference(Entity* attacker, Entity* victim);

    // Check if spell is physical
    bool isPhysicalSpell(const SpellTemplate* spell);

    // Check if spell is magical
    bool isMagicalSpell(const SpellTemplate* spell);
}
