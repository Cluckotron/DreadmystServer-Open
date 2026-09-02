// SpellCaster - Spell casting validation and execution
// Task 5.2: Cast Validation
// Task 5.3: Spell Targeting

#pragma once

#include <cstdint>
#include <vector>
#include <string>

// Forward declarations
class Entity;
class Player;
struct SpellTemplate;

// ============================================================================
// Cast Result Enum - Specific error codes for cast failures
// ============================================================================

enum class CastResult : uint8_t
{
    Success = 0,            // Cast can proceed

    // Caster state errors
    CasterDead = 1,         // Caster is dead
    CasterStunned = 2,      // Caster is stunned
    CasterSilenced = 3,     // Caster is silenced (for spells)
    CasterMoving = 4,       // Caster is moving (for channeled spells)
    CasterCasting = 5,      // Already casting another spell

    // Spell errors
    UnknownSpell = 10,      // Spell doesn't exist
    SpellNotLearned = 11,   // Caster doesn't know this spell
    SpellDisabled = 12,     // Spell is currently disabled

    // Resource errors
    NotEnoughMana = 20,     // Insufficient mana
    NotEnoughHealth = 21,   // Insufficient health (for health-cost spells)
    OnCooldown = 22,        // Spell is on cooldown
    OnGlobalCooldown = 23,  // Global cooldown active
    MissingReagent = 24,    // Required reagent not in inventory

    // Target errors
    NoTarget = 30,          // No target selected
    InvalidTarget = 31,     // Target type doesn't match spell
    TargetDead = 32,        // Target is dead (for non-resurrection spells)
    TargetImmune = 33,      // Target is immune to this spell
    TargetFriendly = 34,    // Cannot cast hostile spell on friendly
    TargetHostile = 35,     // Cannot cast friendly spell on hostile
    TargetSelf = 36,        // Cannot target self with this spell

    // Range errors
    OutOfRange = 40,        // Target too far away
    TooClose = 41,          // Target too close (minimum range)
    LineOfSight = 42,       // No line of sight to target

    // Equipment errors
    WrongEquipment = 50,    // Required weapon not equipped

    // Other errors
    AreaUnavailable = 60,   // Cannot cast in this area
    CombatRequired = 61,    // Must be in combat
    OutOfCombat = 62,       // Must be out of combat

    // Internal errors
    InternalError = 255,    // Something went wrong
};

// Convert CastResult to user-friendly string
const char* getCastResultString(CastResult result);

// ============================================================================
// SpellCaster - Spell validation and execution manager
// ============================================================================

class SpellCaster
{
public:
    // Validate if a cast can proceed
    // Returns Success if valid, or specific error code if not
    static CastResult validateCast(Entity* caster, int32_t spellId, Entity* target);

    // Validate with spell template already looked up
    static CastResult validateCast(Entity* caster, const SpellTemplate* spell, Entity* target);

    // Get list of targets for a spell (handles AoE, self-cast, etc.)
    // Task 5.3: Spell Targeting
    static std::vector<Entity*> getTargets(Entity* caster, const SpellTemplate* spell, Entity* target);

    // Calculate if caster has line of sight to target
    static bool hasLineOfSight(Entity* caster, Entity* target);

    // Calculate distance between two entities
    static float getDistance(Entity* a, Entity* b);

    // Check if two entities are hostile to each other
    static bool areHostile(Entity* a, Entity* b);

    // Check if two entities are friendly to each other
    static bool areFriendly(Entity* a, Entity* b);

private:
    // Individual validation checks
    static CastResult checkCasterState(Entity* caster, const SpellTemplate* spell);
    static CastResult checkResources(Entity* caster, const SpellTemplate* spell);
    static CastResult checkTarget(Entity* caster, const SpellTemplate* spell, Entity* target);
    static CastResult checkRange(Entity* caster, const SpellTemplate* spell, Entity* target);
    static CastResult checkEquipment(Entity* caster, const SpellTemplate* spell);

    // Helper: Check if entity has an aura/mechanic
    static bool hasAura(Entity* entity, int32_t auraId);
    static bool hasMechanic(Entity* entity, int32_t mechanic);
};
