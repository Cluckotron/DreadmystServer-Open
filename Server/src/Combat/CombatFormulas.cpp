// CombatFormulas - Damage, healing, and combat roll calculations
// Task 5.4: Damage Calculation
// Task 5.5: Heal Calculation
// Task 5.6: Hit/Miss/Crit/Dodge System

#include "stdafx.h"
#include "Combat/CombatFormulas.h"
#include "Combat/SpellUtils.h"
#include "Database/GameData.h"
#include "World/Entity.h"
#include "World/Player.h"
#include "ObjDefines.h"
#include "UnitDefines.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace CombatFormulas
{

// ==========================================================================
// Random Number Generation
// ==========================================================================

// Thread-local random engine for combat rolls
static thread_local std::mt19937 s_rng(std::random_device{}());

int roll100()
{
    std::uniform_int_distribution<int> dist(0, 99);
    return dist(s_rng);
}

bool rollChance(int chance)
{
    return roll100() < chance;
}

// ==========================================================================
// Utility Functions
// ==========================================================================

int getLevelDifference(Entity* attacker, Entity* victim)
{
    if (!attacker || !victim)
        return 0;

    int attackerLevel = attacker->getVariable(ObjDefines::Variable::Level);
    int victimLevel = victim->getVariable(ObjDefines::Variable::Level);

    int diff = attackerLevel - victimLevel;

    // Clamp to reasonable range
    return std::clamp(diff, -Config::MAX_LEVEL_DIFF, Config::MAX_LEVEL_DIFF);
}

// Helper to get a stat value from entity's variables
int32_t getStatValue(Entity* entity, UnitDefines::Stat stat)
{
    if (!entity)
        return 0;

    ObjDefines::Variable var = static_cast<ObjDefines::Variable>(
        static_cast<int32_t>(ObjDefines::Variable::StatsStart) + static_cast<int32_t>(stat));
    return entity->getVariable(var);
}

bool isPhysicalSpell(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    return spell->castSchool == static_cast<int32_t>(SpellDefines::School::Physical);
}

bool isMagicalSpell(const SpellTemplate* spell)
{
    return !isPhysicalSpell(spell);
}

// ==========================================================================
// Hit Chance Calculations (Task 5.6)
// ==========================================================================

int getHitChance(Entity* attacker, Entity* victim, const SpellTemplate* spell)
{
    (void)spell;  // May be used for spell-specific hit modifiers

    int baseHit = Config::BASE_HIT_CHANCE;

    // Level difference affects hit chance
    int levelDiff = getLevelDifference(attacker, victim);

    if (levelDiff < 0)
    {
        // Attacker is lower level - harder to hit
        baseHit -= (-levelDiff) * Config::MISS_PER_LEVEL;
    }

    // TODO: Add hit rating from gear/buffs

    return std::clamp(baseHit, 1, 100);  // Always at least 1% chance
}

int getCritChance(Entity* attacker, Entity* victim, const SpellTemplate* spell)
{
    int baseCrit = Config::BASE_CRIT_CHANCE;

    // Add crit rating from gear/buffs based on spell type
    if (attacker)
    {
        if (isPhysicalSpell(spell) || spell == nullptr)
        {
            // Physical attacks use MeleeCritical
            baseCrit += getStatValue(attacker, UnitDefines::Stat::MeleeCritical);
        }
        else
        {
            // Spells use SpellCritical
            baseCrit += getStatValue(attacker, UnitDefines::Stat::SpellCritical);
        }
    }

    // Reduce crit chance against higher level targets
    if (victim)
    {
        int levelDiff = getLevelDifference(attacker, victim);
        if (levelDiff < 0)
        {
            // Target is higher level - reduced crit chance
            baseCrit -= (-levelDiff) * 2;  // 2% reduction per level
        }
    }

    return std::clamp(baseCrit, 0, 100);
}

int getDodgeChance(Entity* victim, const SpellTemplate* spell)
{
    // Can only dodge physical attacks
    if (!isPhysicalSpell(spell) && spell != nullptr)
        return 0;

    int baseDodge = Config::BASE_DODGE_CHANCE;

    if (victim)
    {
        // Add agility-based dodge bonus (1% per 20 agility)
        int agility = getStatValue(victim, UnitDefines::Stat::Agility);
        baseDodge += agility / 20;

        // Add dodge rating from gear
        baseDodge += getStatValue(victim, UnitDefines::Stat::DodgeRating);

        // Add direct dodge bonus from gear/buffs
        baseDodge += getStatValue(victim, UnitDefines::Stat::DodgeChanceBonus);
    }

    return std::clamp(baseDodge, 0, 75);  // Cap at 75%
}

int getParryChance(Entity* victim, const SpellTemplate* spell)
{
    // Can only parry physical melee attacks
    if (!isPhysicalSpell(spell) && spell != nullptr)
        return 0;

    // Check if victim has a weapon equipped (players only)
    if (Player* player = dynamic_cast<Player*>(victim))
    {
        if (!player->hasWeaponEquipped())
            return 0;  // Cannot parry without a weapon
    }

    int baseParry = Config::BASE_PARRY_CHANCE;

    if (victim)
    {
        // Add parry rating from gear
        baseParry += getStatValue(victim, UnitDefines::Stat::ParryRating);

        // Add direct parry bonus from gear/buffs
        baseParry += getStatValue(victim, UnitDefines::Stat::ParryChanceBonus);
    }

    return std::clamp(baseParry, 0, 75);
}

int getBlockChance(Entity* victim, const SpellTemplate* spell)
{
    // Can only block physical attacks
    if (!isPhysicalSpell(spell) && spell != nullptr)
        return 0;

    // Check if victim has a shield equipped (players only)
    if (Player* player = dynamic_cast<Player*>(victim))
    {
        if (!player->hasShieldEquipped())
            return 0;  // Cannot block without a shield
    }
    else
    {
        // NPCs don't have equipment system - check for shield skill as proxy
        if (getStatValue(victim, UnitDefines::Stat::ShieldSkill) == 0)
            return 0;
    }

    int baseBlock = 0;  // No base block - requires shield

    if (victim)
    {
        // Add block rating from gear
        baseBlock += getStatValue(victim, UnitDefines::Stat::BlockRating);

        // Add direct block bonus from gear/buffs
        baseBlock += getStatValue(victim, UnitDefines::Stat::BlockChanceBonus);

        // Shield skill also contributes to block chance
        baseBlock += getStatValue(victim, UnitDefines::Stat::ShieldSkill) / 5;
    }

    return std::clamp(baseBlock, 0, 75);
}

int getResistChance(Entity* attacker, Entity* victim, const SpellTemplate* spell)
{
    // Can only resist magical spells
    if (isPhysicalSpell(spell))
        return 0;

    int baseResist = 0;

    // Level difference affects resist chance
    int levelDiff = getLevelDifference(attacker, victim);
    if (levelDiff < 0)
    {
        // Attacker is lower level - more likely to be resisted
        baseResist += (-levelDiff) * Config::RESIST_PER_LEVEL;
    }

    // Add resistance from gear/buffs based on spell school
    if (victim && spell)
    {
        SpellDefines::School school = static_cast<SpellDefines::School>(spell->castSchool);
        int32_t resistance = 0;

        switch (school)
        {
            case SpellDefines::School::Frost:
                resistance = getStatValue(victim, UnitDefines::Stat::ResistFrost);
                break;
            case SpellDefines::School::Fire:
                resistance = getStatValue(victim, UnitDefines::Stat::ResistFire);
                break;
            case SpellDefines::School::Shadow:
                resistance = getStatValue(victim, UnitDefines::Stat::ResistShadow);
                break;
            case SpellDefines::School::Holy:
                resistance = getStatValue(victim, UnitDefines::Stat::ResistHoly);
                break;
            default:
                break;
        }

        // Convert resistance to resist chance (diminishing returns)
        // Roughly 1% per 10 resistance, capping effectiveness
        baseResist += resistance / 10;
    }

    return std::clamp(baseResist, 0, 75);
}

// ==========================================================================
// Hit Roll (Task 5.6)
// ==========================================================================

HitResult rollToHit(Entity* attacker, Entity* victim, const SpellTemplate* spell)
{
    // Roll order: Miss -> Dodge/Parry/Block -> Resist -> Crit -> Hit

    // 1. Check for miss
    int hitChance = getHitChance(attacker, victim, spell);
    if (!rollChance(hitChance))
    {
        return HitResult::Miss;
    }

    // 2. Physical avoidance (dodge, parry, block)
    if (isPhysicalSpell(spell))
    {
        int dodgeChance = getDodgeChance(victim, spell);
        if (rollChance(dodgeChance))
        {
            return HitResult::Dodge;
        }

        int parryChance = getParryChance(victim, spell);
        if (rollChance(parryChance))
        {
            return HitResult::Parry;
        }

        int blockChance = getBlockChance(victim, spell);
        if (rollChance(blockChance))
        {
            return HitResult::Block;
        }

        // Check for glancing blow (melee vs higher level target)
        int levelDiff = getLevelDifference(attacker, victim);
        if (levelDiff < -2)  // Target is 3+ levels higher
        {
            int glanceChance = (-levelDiff - 2) * 10;  // 10% per level beyond 2
            if (rollChance(glanceChance))
            {
                return HitResult::GlancingBlow;
            }
        }
    }

    // 3. Magic resistance
    if (isMagicalSpell(spell))
    {
        int resistChance = getResistChance(attacker, victim, spell);
        if (rollChance(resistChance))
        {
            return HitResult::Resist;
        }
    }

    // 4. Critical hit check
    int critChance = getCritChance(attacker, victim, spell);
    if (rollChance(critChance))
    {
        return HitResult::Crit;
    }

    // 5. Normal hit
    return HitResult::Hit;
}

HitResult rollMeleeHit(Entity* attacker, Entity* victim)
{
    // Simplified melee hit roll for basic attacks (no spell)
    // Roll order: Miss -> Dodge/Parry/Block -> Crit -> Hit

    // 1. Check for miss
    int hitChance = getHitChance(attacker, victim, nullptr);
    if (!rollChance(hitChance))
    {
        return HitResult::Miss;
    }

    // 2. Physical avoidance (dodge, parry, block)
    int dodgeChance = getDodgeChance(victim, nullptr);
    if (rollChance(dodgeChance))
    {
        return HitResult::Dodge;
    }

    int parryChance = getParryChance(victim, nullptr);
    if (rollChance(parryChance))
    {
        return HitResult::Parry;
    }

    int blockChance = getBlockChance(victim, nullptr);
    if (rollChance(blockChance))
    {
        return HitResult::Block;
    }

    // 3. Check for glancing blow (melee vs higher level target)
    int levelDiff = getLevelDifference(attacker, victim);
    if (levelDiff < -2)  // Target is 3+ levels higher
    {
        int glanceChance = (-levelDiff - 2) * 10;  // 10% per level beyond 2
        if (rollChance(glanceChance))
        {
            return HitResult::GlancingBlow;
        }
    }

    // 4. Critical hit check
    int critChance = getCritChance(attacker, victim, nullptr);
    if (rollChance(critChance))
    {
        return HitResult::Crit;
    }

    // 5. Normal hit
    return HitResult::Hit;
}

// ==========================================================================
// Damage Calculation (Task 5.4)
// ==========================================================================

int32_t getBaseDamage(Entity* attacker, const SpellTemplate* spell, int effectIndex)
{
    if (!attacker || !spell)
        return 0;

    int32_t level = attacker->getVariable(ObjDefines::Variable::Level);

    // Get damage from spell effect (uses SpellUtils for formula evaluation)
    int32_t baseDamage = SpellUtils::calculateEffectValue(spell, effectIndex, level);

    // Add weapon damage for physical attacks
    if (isPhysicalSpell(spell))
    {
        int32_t weaponDamage = getStatValue(attacker, UnitDefines::Stat::WeaponValue);
        baseDamage += weaponDamage;

        // Also add strength bonus for melee (1 damage per 10 strength)
        int32_t strength = getStatValue(attacker, UnitDefines::Stat::Strength);
        baseDamage += strength / 10;
    }
    else
    {
        // Magical spells scale with intelligence (1 damage per 20 intellect)
        int32_t intellect = getStatValue(attacker, UnitDefines::Stat::Intelligence);
        baseDamage += intellect / 20;
    }

    return std::max(baseDamage, Config::MIN_DAMAGE);
}

int32_t applyDamageModifiers(int32_t damage, Entity* attacker, Entity* victim, const SpellTemplate* spell)
{
    (void)attacker;  // Will be used for damage buff checking
    (void)victim;    // Will be used for damage taken reduction
    (void)spell;     // Will be used for spell-specific modifiers

    // TODO: Check attacker for damage increase buffs
    // TODO: Check victim for damage reduction buffs
    // TODO: Check spell school for specific modifiers

    return damage;
}

int32_t calculateArmorReduction(int32_t damage, Entity* victim)
{
    if (!victim)
        return damage;

    // Get armor value from entity stats
    int32_t armor = getStatValue(victim, UnitDefines::Stat::ArmorValue);

    // Apply armor reduction formula
    return applyArmorReduction(damage, armor);
}

int32_t applyArmorReduction(int32_t damage, int32_t armor)
{
    if (armor <= 0)
        return damage;

    // Use a fixed level for armor calculation (20 is mid-range)
    // This gives consistent results regardless of attacker level
    float armorConstant = Config::ARMOR_CONSTANT * 20.0f;

    // Diminishing returns formula: reduction = armor / (armor + constant)
    float reduction = static_cast<float>(armor) / (static_cast<float>(armor) + armorConstant);

    // Apply reduction (cap at 75% reduction)
    reduction = std::min(reduction, 0.75f);

    return static_cast<int32_t>(static_cast<float>(damage) * (1.0f - reduction));
}

int32_t calculateResistReduction(int32_t damage, Entity* victim, SpellDefines::School school)
{
    if (!victim)
        return damage;

    // Get resistance value for specific school
    int32_t resistance = 0;

    switch (school)
    {
        case SpellDefines::School::Frost:
            resistance = getStatValue(victim, UnitDefines::Stat::ResistFrost);
            break;
        case SpellDefines::School::Fire:
            resistance = getStatValue(victim, UnitDefines::Stat::ResistFire);
            break;
        case SpellDefines::School::Shadow:
            resistance = getStatValue(victim, UnitDefines::Stat::ResistShadow);
            break;
        case SpellDefines::School::Holy:
            resistance = getStatValue(victim, UnitDefines::Stat::ResistHoly);
            break;
        default:
            return damage;  // No resistance for physical/unknown schools
    }

    if (resistance <= 0)
        return damage;

    // Apply similar diminishing returns formula as armor
    // resistance / (resistance + constant) gives reduction %
    float resistConstant = 100.0f;  // Lower constant = resistance more effective
    float reduction = static_cast<float>(resistance) / (static_cast<float>(resistance) + resistConstant);

    // Cap at 75% reduction
    reduction = std::min(reduction, 0.75f);

    return static_cast<int32_t>(static_cast<float>(damage) * (1.0f - reduction));
}

int32_t applyDamageVariance(int32_t damage)
{
    // Apply ±DAMAGE_VARIANCE (e.g., ±10%)
    std::uniform_real_distribution<float> dist(1.0f - Config::DAMAGE_VARIANCE, 1.0f + Config::DAMAGE_VARIANCE);
    float multiplier = dist(s_rng);
    return static_cast<int32_t>(static_cast<float>(damage) * multiplier);
}

DamageInfo calculateDamage(Entity* attacker, Entity* victim, const SpellTemplate* spell, int effectIndex)
{
    DamageInfo result;

    if (!attacker || !victim || !spell)
        return result;

    // Determine spell school
    result.school = static_cast<SpellDefines::School>(spell->castSchool);

    // Roll to hit
    result.hitResult = rollToHit(attacker, victim, spell);

    // Handle complete misses
    if (result.hitResult == HitResult::Miss ||
        result.hitResult == HitResult::Dodge ||
        result.hitResult == HitResult::Parry ||
        result.hitResult == HitResult::Immune)
    {
        return result;  // No damage dealt
    }

    // Get base damage
    result.baseDamage = getBaseDamage(attacker, spell, effectIndex);

    // Apply modifiers from buffs/debuffs
    int32_t damage = applyDamageModifiers(result.baseDamage, attacker, victim, spell);

    // Apply armor reduction for physical
    if (isPhysicalSpell(spell))
    {
        damage = calculateArmorReduction(damage, victim);
    }
    else
    {
        // Apply magic resistance reduction
        damage = calculateResistReduction(damage, victim, result.school);
    }

    // Apply hit result modifiers
    switch (result.hitResult)
    {
        case HitResult::Crit:
            damage = static_cast<int32_t>(static_cast<float>(damage) * Config::CRIT_MULTIPLIER);
            break;
        case HitResult::GlancingBlow:
            damage = static_cast<int32_t>(static_cast<float>(damage) * Config::GLANCING_MULTIPLIER);
            break;
        case HitResult::Block:
            // Block reduces damage by 30% (TODO: make configurable based on shield)
            damage = static_cast<int32_t>(static_cast<float>(damage) * 0.7f);
            break;
        case HitResult::Resist:
            // Partial resist - reduce damage by 50%
            result.resisted = damage / 2;
            damage = damage - result.resisted;
            break;
        default:
            break;
    }

    // Apply damage variance
    damage = applyDamageVariance(damage);

    // Ensure minimum damage
    damage = std::max(damage, Config::MIN_DAMAGE);

    // TODO: Check for absorb shields
    // result.absorbed = checkAbsorb(victim, damage);
    // damage -= result.absorbed;

    result.finalDamage = damage;

    // Check for kill
    int32_t victimHealth = victim->getVariable(ObjDefines::Variable::Health);
    if (result.finalDamage >= victimHealth)
    {
        result.overkill = result.finalDamage - victimHealth;
        result.killedTarget = true;
    }

    return result;
}

// ==========================================================================
// Healing Calculation (Task 5.5)
// ==========================================================================

int32_t getBaseHeal(Entity* healer, const SpellTemplate* spell, int effectIndex)
{
    if (!healer || !spell)
        return 0;

    int32_t level = healer->getVariable(ObjDefines::Variable::Level);

    // Get heal amount from spell effect
    int32_t baseHeal = SpellUtils::calculateEffectValue(spell, effectIndex, level);

    // Add stat scaling - healing scales with willpower and intelligence
    int32_t willpower = getStatValue(healer, UnitDefines::Stat::Willpower);
    int32_t intellect = getStatValue(healer, UnitDefines::Stat::Intelligence);

    // 1 healing per 15 willpower + 1 per 30 intellect
    baseHeal += willpower / 15;
    baseHeal += intellect / 30;

    return std::max(baseHeal, 1);
}

int32_t applyHealModifiers(int32_t heal, Entity* healer, Entity* target, const SpellTemplate* spell)
{
    (void)healer;  // Will be used for healing buff checking
    (void)target;  // Will be used for healing received modifiers
    (void)spell;   // Will be used for spell-specific modifiers

    // TODO: Check healer for healing increase buffs
    // TODO: Check target for healing received modifiers

    return heal;
}

HealInfo calculateHeal(Entity* healer, Entity* target, const SpellTemplate* spell, int effectIndex)
{
    HealInfo result;

    if (!healer || !target || !spell)
        return result;

    // Get base heal
    result.baseHeal = getBaseHeal(healer, spell, effectIndex);

    // Apply modifiers from buffs/debuffs
    int32_t heal = applyHealModifiers(result.baseHeal, healer, target, spell);

    // Check for crit (heals can crit too!)
    int critChance = getCritChance(healer, target, spell);
    if (rollChance(critChance))
    {
        result.hitResult = HitResult::Crit;
        heal = static_cast<int32_t>(static_cast<float>(heal) * Config::CRIT_MULTIPLIER);
    }

    // Apply small variance to healing (±5%)
    std::uniform_real_distribution<float> dist(0.95f, 1.05f);
    float multiplier = dist(s_rng);
    heal = static_cast<int32_t>(static_cast<float>(heal) * multiplier);

    result.finalHeal = heal;

    // Calculate overheal
    int32_t currentHealth = target->getVariable(ObjDefines::Variable::Health);
    int32_t maxHealth = target->getVariable(ObjDefines::Variable::MaxHealth);
    int32_t missingHealth = maxHealth - currentHealth;

    if (result.finalHeal > missingHealth)
    {
        result.overheal = result.finalHeal - missingHealth;
        result.finalHeal = missingHealth;
        result.fullHeal = true;
    }

    return result;
}

} // namespace CombatFormulas
