// SpellUtils - Spell calculation utilities
// Task 5.1: Spell Template Loader (formula evaluation extension)

#pragma once

#include <string>
#include <cstdint>
#include "Database/GameData.h"
#include "SpellDefines.h"

// ============================================================================
// Spell Utility Functions
// ============================================================================

namespace SpellUtils
{
    // Formula evaluation - evaluates expressions like "2+((clvl*20)/20)"
    // Supports: +, -, *, /, (), and the variable 'clvl' (character level)
    int32_t evaluateFormula(const std::string& formula, int32_t clvl);

    // Calculate mana cost for a spell at given caster level
    // Returns actual mana cost (evaluates formula if present)
    int32_t calculateManaCost(const SpellTemplate* spell, int32_t casterLevel, int32_t maxMana);

    // Calculate duration for a spell at given caster level
    int32_t calculateDuration(const SpellTemplate* spell, int32_t casterLevel);

    // Calculate effect value (damage/heal amount) for an effect
    int32_t calculateEffectValue(const SpellTemplate* spell, int effectIndex, int32_t casterLevel);

    // Get the primary effect type of a spell
    SpellDefines::Effects getPrimaryEffect(const SpellTemplate* spell);

    // Check if spell is a damage spell
    bool isDamageSpell(const SpellTemplate* spell);

    // Check if spell is a heal spell
    bool isHealSpell(const SpellTemplate* spell);

    // Check if spell applies an aura (buff/debuff)
    bool isAuraSpell(const SpellTemplate* spell);

    // Check if spell is instant cast
    bool isInstant(const SpellTemplate* spell);

    // Check if spell requires a target
    bool requiresTarget(const SpellTemplate* spell);

    // Check if spell can target friendly units
    bool canTargetFriendly(const SpellTemplate* spell);

    // Check if spell can target hostile units
    bool canTargetHostile(const SpellTemplate* spell);

    // Check if spell is self-only
    bool isSelfOnly(const SpellTemplate* spell);

    // Check if spell is AoE
    bool isAoE(const SpellTemplate* spell);

    // Get the school/damage type of a spell
    SpellDefines::School getSpellSchool(const SpellTemplate* spell);
}
