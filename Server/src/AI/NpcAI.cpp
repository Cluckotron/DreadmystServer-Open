// NpcAI - Basic NPC artificial intelligence system
// Task 5.14: Basic NPC AI

#include "stdafx.h"
#include "NpcAI.h"
#include "../World/Npc.h"
#include "../World/Player.h"
#include "../World/WorldManager.h"
#include "../Combat/CombatFormulas.h"
#include "../Combat/CombatMessenger.h"
#include "../Combat/SpellCaster.h"
#include "../Combat/SpellUtils.h"
#include "../Database/GameData.h"
#include "../Core/Logger.h"
#include "GamePacketServer.h"
#include "StlBuffer.h"
#include "ObjDefines.h"
#include "NpcDefines.h"

#include <cmath>
#include <algorithm>
#include <random>

// Forward declarations for helper functions
static void broadcastNpcMovement(Npc* npc, float targetX, float targetY);

// ============================================================================
// NpcAI Namespace Implementation
// ============================================================================

void NpcAI::update(Npc* npc, float deltaTime)
{
    if (!npc)
        return;

    // Don't update AI if dead (handled by Npc::update for respawn timer)
    if (npc->getAIState() == NpcAIState::Dead)
        return;


    // State machine
    switch (npc->getAIState())
    {
        case NpcAIState::Idle:
            updateIdle(npc, deltaTime);
            break;

        case NpcAIState::Combat:
            updateCombat(npc, deltaTime);
            break;

        case NpcAIState::Evading:
            updateEvading(npc, deltaTime);
            break;

        default:
            break;
    }
}

void NpcAI::updateIdle(Npc* npc, float deltaTime)
{
    // Check for aggro targets
    Player* target = findAggroTarget(npc);
    if (target)
    {
        // Enter combat
        npc->setTarget(target);
        npc->setAIState(NpcAIState::Combat);
        npc->getThreatManager().addThreat(target, 1);  // Initial aggro

        LOG_DEBUG("NpcAI: '{}' entering combat with '{}'",
                  npc->getName(), target->getName());
        callForHelp(npc, target);
        return;
    }

    // Idle movement
    int32_t movementType = npc->getMovementType();
    if (movementType == static_cast<int32_t>(NpcDefines::DefaultMovement::Patrol))
    {
        updatePatrol(npc, deltaTime);
    }
    else if (movementType == static_cast<int32_t>(NpcDefines::DefaultMovement::Random))
    {
        updateWander(npc, deltaTime);
    }
}

void NpcAI::updateCombat(Npc* npc, float deltaTime)
{
    // Check leash distance
    if (shouldLeash(npc))
    {
        npc->setAIState(NpcAIState::Evading);
        npc->setTarget(nullptr);
        npc->getThreatManager().clear();

        LOG_DEBUG("NpcAI: '{}' leashing - returning home", npc->getName());
        return;
    }

    // Get current target (highest threat)
    Entity* target = npc->getThreatManager().getHighestThreat();

    // Validate target
    if (!isValidTarget(npc, target))
    {
        // Try to find new target from threat list
        npc->getThreatManager().removeThreat(target);
        target = npc->getThreatManager().getHighestThreat();

        if (!target || !isValidTarget(npc, target))
        {
            // No valid targets - evade
            npc->setAIState(NpcAIState::Evading);
            npc->setTarget(nullptr);
            npc->getThreatManager().clear();

            LOG_DEBUG("NpcAI: '{}' no valid targets - evading", npc->getName());
            return;
        }
    }

    // Update target reference
    if (npc->getTarget() != target)
    {
        npc->setTarget(target);
    }

    if (!npc->hasCalledForHelp())
    {
        callForHelp(npc, target);
    }

    // Check if in melee range
    if (isInMeleeRange(npc, target))
    {
        // Perform melee attack
        performMeleeAttack(npc, target);
    }
    else
    {
        // Move towards target
        moveTowardsEntity(npc, target, deltaTime);
    }

    // Try to cast spells if available
    int32_t spellId = selectSpellToCast(npc, target);
    if (spellId > 0)
    {
        performSpellCast(npc, target, spellId);
    }
}

void NpcAI::updateEvading(Npc* npc, float deltaTime)
{
    // Move towards home position
    returnHome(npc, deltaTime);

    // Check if we've arrived home
    if (npc->distanceFromHome() < HOME_ARRIVAL_DISTANCE)
    {
        // Reset NPC
        npc->setAIState(NpcAIState::Idle);

        // Restore full health
        int32_t maxHealth = npc->getVariable(ObjDefines::Variable::MaxHealth);
        npc->setVariable(ObjDefines::Variable::Health, maxHealth);
        npc->broadcastVariable(ObjDefines::Variable::Health, maxHealth);

        // Restore mana if applicable
        int32_t maxMana = npc->getVariable(ObjDefines::Variable::MaxMana);
        if (maxMana > 0)
        {
            npc->setVariable(ObjDefines::Variable::Mana, maxMana);
            npc->broadcastVariable(ObjDefines::Variable::Mana, maxMana);
        }

        // Clear all auras
        npc->getAuras().clearAll(true);
        npc->setCalledForHelp(false);

        LOG_DEBUG("NpcAI: '{}' arrived home - reset complete", npc->getName());
    }
}

Player* NpcAI::findAggroTarget(Npc* npc)
{
    if (!npc)
        return nullptr;

    float aggroRange = npc->getAggroRange();
    int mapId = npc->getMapId();

    // Get all players on the same map
    std::vector<Player*> players = sWorldManager.getPlayersOnMap(mapId);

    Player* closestTarget = nullptr;
    float closestDistSq = aggroRange * aggroRange;

    for (Player* player : players)
    {
        // Skip invalid targets
        if (!player || player->isDead())
            continue;

        // Check if hostile
        if (!npc->isHostileTo(player))
            continue;

        // Check distance
        float dx = player->getX() - npc->getX();
        float dy = player->getY() - npc->getY();
        float distSq = dx * dx + dy * dy;

        if (distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestTarget = player;
        }
    }

    return closestTarget;
}

void NpcAI::performMeleeAttack(Npc* npc, Entity* target)
{
    if (!npc || !target)
        return;

    // Check attack cooldown
    float timeSinceLastAttack = npc->getTimeSinceLastAttack();
    float attackSpeed = npc->getMeleeSpeed() / 1000.0f;  // Convert ms to seconds

    if (timeSinceLastAttack < attackSpeed)
        return;  // On cooldown

    // Reset attack timer
    npc->resetAttackTimer();

    // Calculate damage
    int32_t baseDamage = npc->getMeleeDamage();

    // Roll for hit/miss/crit
    HitResult hitResult = CombatFormulas::rollMeleeHit(npc, target);

    if (hitResult == HitResult::Miss || hitResult == HitResult::Dodge ||
        hitResult == HitResult::Parry)
    {
        // Send miss message
        CombatMessenger::sendMissMessage(npc, target, 0, hitResult, SpellDefines::Effects::MeleeAtk);
        return;
    }

    // Apply damage modifiers
    int32_t finalDamage = baseDamage;

    if (hitResult == HitResult::Crit)
    {
        finalDamage = static_cast<int32_t>(finalDamage * CombatFormulas::CRIT_MULTIPLIER);
    }
    else if (hitResult == HitResult::GlancingBlow)
    {
        finalDamage = static_cast<int32_t>(finalDamage * CombatFormulas::GLANCING_MULTIPLIER);
    }

    // Apply armor reduction (TODO: Add Armor variable when implemented)
    // For now, skip armor reduction for simplicity
    // int32_t targetArmor = target->getVariable(ObjDefines::Variable::Armor);
    // finalDamage = CombatFormulas::applyArmorReduction(finalDamage, targetArmor);

    // Apply damage variance
    finalDamage = CombatFormulas::applyDamageVariance(finalDamage);

    // Ensure minimum damage
    finalDamage = std::max(finalDamage, CombatFormulas::MIN_DAMAGE);

    // Deal damage
    target->takeDamage(finalDamage, npc);

    // Note: When NPC damages player, no threat is added (threat is for when
    // players attack NPCs). If player attacks back, that adds threat instead.

    // Send combat message
    DamageInfo damageInfo;
    damageInfo.baseDamage = baseDamage;
    damageInfo.finalDamage = finalDamage;
    damageInfo.school = SpellDefines::School::Physical;
    damageInfo.hitResult = hitResult;
    damageInfo.killedTarget = target->isDead();

    CombatMessenger::sendDamageMessage(npc, target, 0, damageInfo, SpellDefines::Effects::MeleeAtk);

    LOG_DEBUG("NpcAI: '{}' melee attack on '{}' for {} damage ({})",
              npc->getName(), target->getName(), finalDamage,
              hitResult == HitResult::Crit ? "crit" : "hit");
}

void NpcAI::performSpellCast(Npc* npc, Entity* target, int32_t spellId)
{
    if (!npc || !target)
        return;

    if (spellId <= 0)
        return;

    // Get spell template
    const SpellTemplate* spell = sGameData.getSpell(spellId);
    if (!spell)
        return;

    // Validate cast
    CastResult result = SpellCaster::validateCast(npc, spell, target);
    if (result != CastResult::Success)
    {
        LOG_DEBUG("NpcAI: '{}' spell cast validation failed - %s",
                  npc->getName(), getCastResultString(result));
        return;
    }

    // Get targets for the spell
    std::vector<Entity*> targets = SpellCaster::getTargets(npc, spell, target);
    if (targets.empty())
        return;

    // Process spell effects (instant cast for NPCs - no cast time handling)
    std::vector<std::pair<uint32_t, uint8_t>> hitTargets;

    for (Entity* effectTarget : targets)
    {
        SpellDefines::Effects effectType = static_cast<SpellDefines::Effects>(spell->effect[0]);

        if (effectType == SpellDefines::Effects::Damage ||
            effectType == SpellDefines::Effects::MeleeAtk ||
            effectType == SpellDefines::Effects::RangedAtk)
        {
            // Damage spell
            DamageInfo damage = CombatFormulas::calculateDamage(npc, effectTarget, spell, 0);

            hitTargets.push_back({static_cast<uint32_t>(effectTarget->getGuid()),
                static_cast<uint8_t>(CombatMessenger::toPacketHitResult(damage.hitResult))});

            if (damage.hitResult != HitResult::Miss &&
                damage.hitResult != HitResult::Dodge &&
                damage.hitResult != HitResult::Parry)
            {
                effectTarget->takeDamage(damage.finalDamage, npc);
                CombatMessenger::sendDamageMessage(npc, effectTarget, spellId, damage, effectType);
            }
            else
            {
                CombatMessenger::sendMissMessage(npc, effectTarget, spellId, damage.hitResult, effectType);
            }
        }
        else if (effectType == SpellDefines::Effects::Heal)
        {
            // Healing spell
            HealInfo heal = CombatFormulas::calculateHeal(npc, effectTarget, spell, 0);

            hitTargets.push_back({static_cast<uint32_t>(effectTarget->getGuid()),
                static_cast<uint8_t>(CombatMessenger::toPacketHitResult(heal.hitResult))});

            effectTarget->heal(heal.finalHeal, npc);
            CombatMessenger::sendHealMessage(npc, effectTarget, spellId, heal);
        }
        else if (effectType == SpellDefines::Effects::ApplyAura)
        {
            // Buff/debuff spell
            effectTarget->getAuras().applyAura(npc, spell, 0);
            hitTargets.push_back({static_cast<uint32_t>(effectTarget->getGuid()),
                static_cast<uint8_t>(SpellDefines::HitResult::Normal)});
        }
    }

    // Send spell execution notification
    GP_Server_SpellGo spellGo;
    spellGo.m_casterGuid = static_cast<uint32_t>(npc->getGuid());
    spellGo.m_spellId = spellId;
    for (const auto& [guid, hitResult] : hitTargets)
    {
        spellGo.m_targets[guid] = hitResult;
    }

    StlBuffer buf;
    uint16_t opcode = spellGo.getOpcode();
    buf << opcode;
    spellGo.pack(buf);

    // Send to all players on the map
    std::vector<Player*> players = sWorldManager.getPlayersOnMap(npc->getMapId());
    for (Player* player : players)
    {
        player->sendPacket(buf);
    }

    // Consume mana
    int32_t npcLevel = npc->getVariable(ObjDefines::Variable::Level);
    int32_t maxMana = npc->getVariable(ObjDefines::Variable::MaxMana);
    int32_t manaCost = SpellUtils::calculateManaCost(spell, npcLevel, maxMana);
    if (manaCost > 0)
    {
        int32_t currentMana = npc->getVariable(ObjDefines::Variable::Mana);
        npc->setVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
        npc->broadcastVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
    }

    // Start cooldown using NPC's spell cooldown timer
    npc->setSpellCooldown(static_cast<float>(spell->cooldown) / 1000.0f);

    LOG_DEBUG("NpcAI: '{}' cast spell '{}' on {} targets",
              npc->getName(), spell->name, targets.size());
}

bool NpcAI::isInMeleeRange(Npc* npc, Entity* target)
{
    if (!npc || !target)
        return false;

    return npc->isInRange(target, npc->getMeleeRange());
}

bool NpcAI::canCastSpell(Npc* npc, int32_t spellId, Entity* target)
{
    if (!npc)
        return false;

    if (spellId <= 0)
        return false;

    // Check if spell is on cooldown
    if (npc->isSpellOnCooldown())
        return false;

    // Get spell template
    const SpellTemplate* spell = sGameData.getSpell(spellId);
    if (!spell)
        return false;

    // Check mana cost
    int32_t npcLevel = npc->getVariable(ObjDefines::Variable::Level);
    int32_t maxMana = npc->getVariable(ObjDefines::Variable::MaxMana);
    int32_t manaCost = SpellUtils::calculateManaCost(spell, npcLevel, maxMana);
    int32_t currentMana = npc->getVariable(ObjDefines::Variable::Mana);
    if (manaCost > 0 && currentMana < manaCost)
        return false;

    // Check range to target
    if (target && spell->range > 0)
    {
        constexpr float GAME_DB_RANGE_UNITS_PER_CELL = 20.0f;
        const float rangeCells = static_cast<float>(spell->range) / GAME_DB_RANGE_UNITS_PER_CELL;
        if (!npc->isInRange(target, rangeCells))
            return false;
    }

    return true;
}

int32_t NpcAI::selectSpellToCast(Npc* npc, Entity* target)
{
    if (!npc || !target)
        return 0;

    if (npc->isSpellOnCooldown())
        return 0;

    const NpcTemplate* tmpl = npc->getTemplate();
    if (tmpl)
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> chanceDist(1, 100);

        int32_t primary = npc->getPrimarySpellId();
        int32_t maxHealth = npc->getVariable(ObjDefines::Variable::MaxHealth);
        int32_t health = npc->getVariable(ObjDefines::Variable::Health);
        if (primary > 0 && maxHealth > 0)
        {
            int32_t healthPct = (health * 100) / maxHealth;
            if (healthPct <= 30 && canCastSpell(npc, primary, target))
                return primary;
        }

        for (const auto& slot : tmpl->spells)
        {
            if (slot.id <= 0)
                continue;

            int roll = chanceDist(rng);
            if (roll > slot.chance)
                continue;

            if (canCastSpell(npc, slot.id, target))
                return slot.id;
        }
    }

    int32_t primary = npc->getPrimarySpellId();
    if (canCastSpell(npc, primary, target))
        return primary;

    return 0;
}

void NpcAI::updatePatrol(Npc* npc, float deltaTime)
{
    if (!npc)
        return;

    const auto& waypoints = npc->getWaypoints();
    if (waypoints.empty())
        return;

    float waitTimer = npc->getWaypointWaitTimer();
    if (waitTimer > 0.0f)
    {
        npc->setWaypointWaitTimer(waitTimer - deltaTime);
        return;
    }

    int32_t index = npc->getCurrentWaypointIndex();
    if (index < 0 || static_cast<size_t>(index) >= waypoints.size())
        index = 0;

    const NpcWaypoint& wp = waypoints[index];
    float distance = npc->distanceTo(wp.x, wp.y);

    if (distance <= 2.0f)
    {
        npc->setOrientation(wp.orientation);
        npc->setCurrentWaypointIndex((index + 1) % static_cast<int32_t>(waypoints.size()));
        if (wp.waitTimeMs > 0)
            npc->setWaypointWaitTimer(static_cast<float>(wp.waitTimeMs) / 1000.0f);
        return;
    }

    moveTowards(npc, wp.x, wp.y, deltaTime);
}

void NpcAI::updateWander(Npc* npc, float deltaTime)
{
    if (!npc)
        return;

    float radius = npc->getWanderDistance();
    if (radius <= 0.0f)
        return;

    float waitTimer = npc->getWanderWaitTimer();
    if (waitTimer > 0.0f)
    {
        npc->setWanderWaitTimer(waitTimer - deltaTime);
        return;
    }

    if (!npc->hasWanderTarget())
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
        std::uniform_real_distribution<float> radiusDist(0.0f, radius);

        float angle = angleDist(rng);
        float dist = radiusDist(rng);

        float targetX = npc->getHomeX() + std::cos(angle) * dist;
        float targetY = npc->getHomeY() + std::sin(angle) * dist;
        npc->setWanderTarget(targetX, targetY);
    }

    float distance = npc->distanceTo(npc->getWanderTargetX(), npc->getWanderTargetY());
    if (distance <= 2.0f)
    {
        npc->clearWanderTarget();
        npc->setWanderWaitTimer(1.5f);
        return;
    }

    moveTowards(npc, npc->getWanderTargetX(), npc->getWanderTargetY(), deltaTime);
}

void NpcAI::callForHelp(Npc* npc, Entity* target)
{
    if (!npc || !target)
        return;

    if (!npc->shouldCallForHelp())
        return;

    npc->setCalledForHelp(true);

    constexpr float helpRange = 3.0f;
    std::vector<Npc*> npcs = sWorldManager.getNpcsOnMap(npc->getMapId());
    for (Npc* ally : npcs)
    {
        if (!ally || ally == npc || ally->isDead())
            continue;

        if (ally->getAIState() != NpcAIState::Idle)
            continue;

        if (ally->getFaction() != npc->getFaction())
            continue;

        if (!npc->isInRange(ally, helpRange))
            continue;

        ally->addThreat(target, 1);
    }
}

void NpcAI::moveTowards(Npc* npc, float targetX, float targetY, float deltaTime)
{
    if (!npc)
        return;

    float dx = targetX - npc->getX();
    float dy = targetY - npc->getY();
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance < 1.0f)
        return;  // Already at target

    // Normalize direction
    float dirX = dx / distance;
    float dirY = dy / distance;

    // Calculate movement amount
    float moveSpeedPct = static_cast<float>(
        std::max(1, npc->getVariable(ObjDefines::Variable::MoveSpeedPct)));
    float moveSpeed = NPC_MOVE_SPEED * (moveSpeedPct / 100.0f);
    if (npc->getAIState() == NpcAIState::Evading)
    {
        moveSpeed *= EVADE_SPEED_MULTIPLIER;
    }

    float moveAmount = moveSpeed * deltaTime;
    moveAmount = std::min(moveAmount, distance);  // Don't overshoot

    // Update position
    float newX = npc->getX() + dirX * moveAmount;
    float newY = npc->getY() + dirY * moveAmount;
    npc->setPosition(newX, newY);

    // Update orientation
    float orientation = calculateOrientation(npc->getX(), npc->getY(), targetX, targetY);
    npc->setOrientation(orientation);

    // Broadcast movement to nearby players
    broadcastNpcMovement(npc, targetX, targetY);
}

void NpcAI::moveTowardsEntity(Npc* npc, Entity* target, float deltaTime)
{
    if (!npc || !target)
        return;

    // Stop at melee range, not exactly at target
    float stopDistance = std::max(0.25f, npc->getMeleeRange() * 0.85f);
    float dx = target->getX() - npc->getX();
    float dy = target->getY() - npc->getY();
    float distance = std::sqrt(dx * dx + dy * dy);

    if (distance <= stopDistance)
        return;  // Close enough

    moveTowards(npc, target->getX(), target->getY(), deltaTime);
}

void NpcAI::returnHome(Npc* npc, float deltaTime)
{
    if (!npc)
        return;

    moveTowards(npc, npc->getHomeX(), npc->getHomeY(), deltaTime);
}

bool NpcAI::shouldLeash(Npc* npc)
{
    if (!npc)
        return false;

    return npc->distanceFromHome() > npc->getLeashRange();
}

bool NpcAI::isValidTarget(Npc* npc, Entity* target)
{
    if (!npc || !target)
        return false;

    // Target must be alive
    if (target->isDead())
        return false;

    // Target must be on same map
    if (target->getMapId() != npc->getMapId())
        return false;

    // Target must still be hostile
    if (!npc->isHostileTo(target))
        return false;

    return true;
}

float NpcAI::calculateOrientation(float fromX, float fromY, float toX, float toY)
{
    float dx = toX - fromX;
    float dy = toY - fromY;
    return std::atan2(dy, dx);
}

// Helper function to broadcast NPC movement
static void broadcastNpcMovement(Npc* npc, float targetX, float targetY)
{
    if (!npc)
        return;

    // Use GP_Server_UnitSpline for movement
    // Spline format: start position + list of waypoints
    GP_Server_UnitSpline packet;
    packet.m_guid = static_cast<uint32_t>(npc->getGuid());
    packet.m_startX = npc->getX();
    packet.m_startY = npc->getY();
    packet.m_spline.push_back({targetX, targetY});
    packet.m_slide = false;
    packet.m_silent = false;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Send to all players on the map
    std::vector<Player*> players = sWorldManager.getPlayersOnMap(npc->getMapId());
    for (Player* player : players)
    {
        player->sendPacket(buf);
    }
}
