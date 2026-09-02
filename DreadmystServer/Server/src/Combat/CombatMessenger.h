// CombatMessenger - Send combat result packets to clients
// Task 5.10: Combat Messages

#pragma once

#include <cstdint>
#include "SpellDefines.h"
#include "Combat/CombatFormulas.h"

// Forward declarations
class Entity;
class Player;
struct SpellTemplate;
struct DamageInfo;
struct HealInfo;

// ============================================================================
// CombatMessenger - Send combat results to relevant clients
// ============================================================================

namespace CombatMessenger
{
    // ========================================================================
    // Main Functions
    // ========================================================================

    // Send damage message to attacker, victim, and nearby players
    // @param attacker - The entity dealing damage
    // @param victim - The entity receiving damage
    // @param spellId - The spell that caused the damage
    // @param damage - Calculated damage information
    // @param effectType - The type of spell effect (Damage, MeleeAtk, etc.)
    void sendDamageMessage(Entity* attacker, Entity* victim, int32_t spellId,
                           const DamageInfo& damage, SpellDefines::Effects effectType);

    // Send heal message to healer, target, and nearby players
    // @param healer - The entity providing healing
    // @param target - The entity being healed
    // @param spellId - The spell that caused the healing
    // @param heal - Calculated heal information
    void sendHealMessage(Entity* healer, Entity* target, int32_t spellId,
                         const HealInfo& heal);

    // Send periodic damage message (DoT tick)
    // @param casterGuid - GUID of the original caster
    // @param victim - The entity receiving damage
    // @param spellId - The spell (aura) causing the damage
    // @param amount - Damage dealt this tick
    void sendPeriodicDamage(uint64_t casterGuid, Entity* victim, int32_t spellId,
                            int32_t amount);

    // Send periodic heal message (HoT tick)
    // @param casterGuid - GUID of the original caster
    // @param target - The entity being healed
    // @param spellId - The spell (aura) causing the healing
    // @param amount - Health restored this tick
    void sendPeriodicHeal(uint64_t casterGuid, Entity* target, int32_t spellId,
                          int32_t amount);

    // Send miss/dodge/parry message (no damage dealt)
    void sendMissMessage(Entity* attacker, Entity* victim, int32_t spellId,
                         HitResult result, SpellDefines::Effects effectType);

    // ========================================================================
    // Utility Functions
    // ========================================================================

    // Convert internal HitResult to packet SpellDefines::HitResult
    SpellDefines::HitResult toPacketHitResult(HitResult result);

    // Send combat message to all relevant players
    // (attacker if player, victim if player, nearby players)
    void broadcastCombatMessage(Entity* attacker, Entity* victim,
                                int32_t spellId, int32_t amount,
                                SpellDefines::Effects effectType,
                                SpellDefines::HitResult hitResult,
                                bool isPeriodic, bool isPositive);
}
