// CombatMessenger - Send combat result packets to clients
// Task 5.10: Combat Messages

#include "stdafx.h"
#include "Combat/CombatMessenger.h"
#include "Combat/CombatFormulas.h"
#include "World/Entity.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Core/Logger.h"
#include "GamePacketServer.h"

namespace CombatMessenger
{

// ============================================================================
// HitResult Conversion
// ============================================================================

SpellDefines::HitResult toPacketHitResult(HitResult result)
{
    // Convert our internal HitResult to the packet's SpellDefines::HitResult
    // These enums have slightly different values
    switch (result)
    {
        case HitResult::Hit:          return SpellDefines::HitResult::Normal;
        case HitResult::Crit:         return SpellDefines::HitResult::Crit;
        case HitResult::Miss:         return SpellDefines::HitResult::Miss;
        case HitResult::Dodge:        return SpellDefines::HitResult::Dodge;
        case HitResult::Parry:        return SpellDefines::HitResult::Parry;
        case HitResult::Block:        return SpellDefines::HitResult::Block;
        case HitResult::Resist:       return SpellDefines::HitResult::Resist;
        case HitResult::Immune:       return SpellDefines::HitResult::Immune;
        case HitResult::Absorb:       return SpellDefines::HitResult::Absorb;
        case HitResult::GlancingBlow: return SpellDefines::HitResult::Normal;  // No glancing in packet
        default:                      return SpellDefines::HitResult::Normal;
    }
}

// ============================================================================
// Core Broadcast Function
// ============================================================================

void broadcastCombatMessage(Entity* attacker, Entity* victim,
                            int32_t spellId, int32_t amount,
                            SpellDefines::Effects effectType,
                            SpellDefines::HitResult hitResult,
                            bool isPeriodic, bool isPositive)
{
    // Build the combat message packet
    GP_Server_CombatMsg packet;
    packet.m_casterGuid = attacker ? static_cast<uint32_t>(attacker->getGuid()) : 0;
    packet.m_targetGuid = victim ? static_cast<uint32_t>(victim->getGuid()) : 0;
    packet.m_spellId = spellId;
    packet.m_amount = amount;
    packet.m_spellEffect = static_cast<uint8_t>(effectType);
    packet.m_spellResult = static_cast<uint8_t>(hitResult);
    packet.m_periodic = isPeriodic;
    packet.m_positive = isPositive;

    // Serialize the packet
    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Track who we've already sent to (avoid duplicates)
    Player* attackerPlayer = attacker ? dynamic_cast<Player*>(attacker) : nullptr;
    Player* victimPlayer = victim ? dynamic_cast<Player*>(victim) : nullptr;

    // 1. Send to attacker if they're a player
    if (attackerPlayer)
    {
        attackerPlayer->sendPacket(buf);
    }

    // 2. Send to victim if they're a different player
    if (victimPlayer && victimPlayer != attackerPlayer)
    {
        victimPlayer->sendPacket(buf);
    }

    // 3. Broadcast to nearby players (excluding attacker and victim)
    // Use the victim's position for the broadcast center (combat happens at victim)
    Entity* broadcastCenter = victim ? victim : attacker;
    if (broadcastCenter)
    {
        Player* centerPlayer = dynamic_cast<Player*>(broadcastCenter);
        if (centerPlayer)
        {
            // broadcastToVisible excludes the source player
            // We need to exclude both attacker and victim
            auto nearbyPlayers = sWorldManager.getPlayersOnMap(centerPlayer->getMapId());

            for (Player* nearby : nearbyPlayers)
            {
                // Skip attacker and victim (already sent)
                if (nearby == attackerPlayer || nearby == victimPlayer)
                    continue;

                // Check visibility distance
                float dx = nearby->getX() - broadcastCenter->getX();
                float dy = nearby->getY() - broadcastCenter->getY();
                float distSq = dx * dx + dy * dy;

                // Use visibility range (assume ~1000 pixels, squared = 1000000)
                constexpr float COMBAT_MSG_RANGE_SQ = 1000.0f * 1000.0f;
                if (distSq <= COMBAT_MSG_RANGE_SQ)
                {
                    nearby->sendPacket(buf);
                }
            }
        }
    }

    LOG_DEBUG("CombatMessenger: Sent combat msg - spell={}, amount={}, hit={}, periodic={}, positive={}",
              spellId, amount, static_cast<int>(hitResult), isPeriodic, isPositive);
}

// ============================================================================
// Damage Messages
// ============================================================================

void sendDamageMessage(Entity* attacker, Entity* victim, int32_t spellId,
                       const DamageInfo& damage, SpellDefines::Effects effectType)
{
    if (!attacker || !victim)
        return;

    // Convert our hit result to packet format
    SpellDefines::HitResult packetResult = toPacketHitResult(damage.hitResult);

    // Send the message
    broadcastCombatMessage(
        attacker,
        victim,
        spellId,
        damage.finalDamage,
        effectType,
        packetResult,
        false,  // Not periodic (direct damage)
        false   // Not positive (damage is negative for target)
    );

    // Log the combat event
    LOG_INFO("Combat: %lu dealt %d damage to %lu (spell=%d, result=%d)",
             attacker->getGuid(), damage.finalDamage, victim->getGuid(),
             spellId, static_cast<int>(damage.hitResult));
}

void sendMissMessage(Entity* attacker, Entity* victim, int32_t spellId,
                     HitResult result, SpellDefines::Effects effectType)
{
    if (!attacker || !victim)
        return;

    SpellDefines::HitResult packetResult = toPacketHitResult(result);

    // Send with 0 damage
    broadcastCombatMessage(
        attacker,
        victim,
        spellId,
        0,  // No damage for miss/dodge/etc.
        effectType,
        packetResult,
        false,  // Not periodic
        false   // Not positive
    );

    LOG_INFO("Combat: %lu attacked %lu but %s (spell=%d)",
             attacker->getGuid(), victim->getGuid(),
             result == HitResult::Miss ? "missed" :
             result == HitResult::Dodge ? "was dodged" :
             result == HitResult::Parry ? "was parried" :
             result == HitResult::Block ? "was blocked" :
             result == HitResult::Resist ? "was resisted" : "failed",
             spellId);
}

// ============================================================================
// Heal Messages
// ============================================================================

void sendHealMessage(Entity* healer, Entity* target, int32_t spellId,
                     const HealInfo& heal)
{
    if (!healer || !target)
        return;

    // Heals can crit too
    SpellDefines::HitResult packetResult = toPacketHitResult(heal.hitResult);

    broadcastCombatMessage(
        healer,
        target,
        spellId,
        heal.finalHeal,
        SpellDefines::Effects::Heal,
        packetResult,
        false,  // Not periodic (direct heal)
        true    // Positive effect
    );

    LOG_INFO("Combat: %lu healed %lu for %d (spell=%d, crit=%d)",
             healer->getGuid(), target->getGuid(), heal.finalHeal,
             spellId, heal.hitResult == HitResult::Crit ? 1 : 0);
}

// ============================================================================
// Periodic Messages (DoT/HoT)
// ============================================================================

void sendPeriodicDamage(uint64_t casterGuid, Entity* victim, int32_t spellId,
                        int32_t amount)
{
    if (!victim)
        return;

    // Build packet manually since we only have the caster's GUID, not entity
    GP_Server_CombatMsg packet;
    packet.m_casterGuid = static_cast<uint32_t>(casterGuid);
    packet.m_targetGuid = static_cast<uint32_t>(victim->getGuid());
    packet.m_spellId = spellId;
    packet.m_amount = amount;
    packet.m_spellEffect = static_cast<uint8_t>(SpellDefines::Effects::Damage);
    packet.m_spellResult = static_cast<uint8_t>(SpellDefines::HitResult::Normal);
    packet.m_periodic = true;   // This is periodic damage
    packet.m_positive = false;  // Damage is negative

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Send to victim if player
    Player* victimPlayer = dynamic_cast<Player*>(victim);
    if (victimPlayer)
    {
        victimPlayer->sendPacket(buf);

        // Broadcast to nearby players
        sWorldManager.broadcastToVisible(victimPlayer, buf, false);
    }
    // TODO: For NPCs, broadcast to all nearby players

    LOG_DEBUG("CombatMessenger: DoT spell={} dealt {} periodic damage to {}",
              spellId, amount, victim->getGuid());
}

void sendPeriodicHeal(uint64_t casterGuid, Entity* target, int32_t spellId,
                      int32_t amount)
{
    if (!target)
        return;

    GP_Server_CombatMsg packet;
    packet.m_casterGuid = static_cast<uint32_t>(casterGuid);
    packet.m_targetGuid = static_cast<uint32_t>(target->getGuid());
    packet.m_spellId = spellId;
    packet.m_amount = amount;
    packet.m_spellEffect = static_cast<uint8_t>(SpellDefines::Effects::Heal);
    packet.m_spellResult = static_cast<uint8_t>(SpellDefines::HitResult::Normal);
    packet.m_periodic = true;   // This is periodic healing
    packet.m_positive = true;   // Healing is positive

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Send to target if player
    Player* targetPlayer = dynamic_cast<Player*>(target);
    if (targetPlayer)
    {
        targetPlayer->sendPacket(buf);

        // Broadcast to nearby players
        sWorldManager.broadcastToVisible(targetPlayer, buf, false);
    }

    LOG_DEBUG("CombatMessenger: HoT spell={} healed {} for {} periodic",
              spellId, target->getGuid(), amount);
}

} // namespace CombatMessenger
