// Npc - Non-Player Character entity
// Task 5.13: NPC Entity
// Task 5.14: Basic NPC AI

#include "stdafx.h"
#include "Npc.h"
#include "Player.h"
#include "WorldManager.h"
#include "../AI/NpcAI.h"
#include "NpcSpawner.h"
#include "../Core/Logger.h"
#include "../Systems/LootSystem.h"
#include "../Systems/QuestManager.h"
#include "../Systems/ExperienceSystem.h"
#include "GamePacketServer.h"
#include "StlBuffer.h"
#include "UnitDefines.h"

#include <cmath>
#include <algorithm>
#include <random>

// ============================================================================
// Constructor / Destructor
// ============================================================================

Npc::Npc(const NpcTemplate& tmpl, int32_t mapId, float x, float y, float orientation)
    : m_entry(tmpl.entry)
    , m_template(&tmpl)
    , m_homeX(x)
    , m_homeY(y)
    , m_homeOrientation(orientation)
{
    // Set entity position
    setPosition(mapId, x, y);
    setOrientation(orientation);

    // Initialize from template
    initFromTemplate(tmpl);

    LOG_DEBUG("Npc: Created '{}' (entry={}) at map {} ({:.1f}, {:.1f})",
              m_name, m_entry, mapId, x, y);
}

Npc::~Npc()
{
    LOG_DEBUG("Npc: Destroyed '{}' (entry={})", m_name, m_entry);
}

// ============================================================================
// Initialization
// ============================================================================

void Npc::initFromTemplate(const NpcTemplate& tmpl)
{
    // Basic info
    m_name = tmpl.name;
    m_subname = tmpl.subname;
    setName(tmpl.name);

    // Faction and flags
    m_faction = tmpl.faction;
    m_npcFlags = tmpl.npcFlags;
    m_isElite = (tmpl.boolElite != 0);
    m_isBoss = (tmpl.boolBoss != 0);

    // Combat properties
    m_leashRange = tmpl.leashRange > 0 ? static_cast<float>(tmpl.leashRange) : DEFAULT_LEASH_RANGE;
    m_meleeSpeed = tmpl.meleeSpeed > 0 ? tmpl.meleeSpeed : 2000;
    m_weaponValue = tmpl.weaponValue > 0 ? tmpl.weaponValue : 10;

    // Determine level (random between min and max)
    int32_t level = tmpl.minLevel;
    if (tmpl.maxLevel > tmpl.minLevel)
    {
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int32_t> dist(tmpl.minLevel, tmpl.maxLevel);
        level = dist(rng);
    }
    setVariable(ObjDefines::Variable::Level, level);

    // Calculate and set health
    int32_t health = tmpl.health;
    if (health <= 0)
    {
        health = calculateHealth(level, m_isElite, m_isBoss);
    }
    setVariable(ObjDefines::Variable::Health, health);
    setVariable(ObjDefines::Variable::MaxHealth, health);

    // Calculate and set mana (if applicable)
    int32_t mana = tmpl.mana;
    if (mana > 0)
    {
        setVariable(ObjDefines::Variable::Mana, mana);
        setVariable(ObjDefines::Variable::MaxMana, mana);
    }
    else if (tmpl.intellect > 0)
    {
        mana = calculateMana(level);
        if (mana > 0)
        {
            setVariable(ObjDefines::Variable::Mana, mana);
            setVariable(ObjDefines::Variable::MaxMana, mana);
        }
    }

    // Content references
    m_gossipMenuId = tmpl.gossipMenuId;
    m_customLootId = tmpl.customLoot;
    m_primarySpellId = tmpl.spellPrimary;

    // Set faction variable
    setVariable(ObjDefines::Variable::Faction, m_faction);

    // Model/visuals
    setVariable(ObjDefines::Variable::ModelId, tmpl.modelId);
    setVariable(ObjDefines::Variable::ModelScale, tmpl.modelScale);

    // Flags
    setVariable(ObjDefines::Variable::Elite, m_isElite ? 1 : 0);
    setVariable(ObjDefines::Variable::Boss, m_isBoss ? 1 : 0);
    setVariable(ObjDefines::Variable::DynGossipStatus, ObjDefines::GossipNone);

    // Movement speed (percent)
    setVariable(ObjDefines::Variable::MoveSpeedPct, 100);
}

int32_t Npc::calculateHealth(int32_t level, bool isElite, bool isBoss) const
{
    // Base health formula: level * 10 + some base
    int32_t baseHealth = 50 + level * 10;

    // Elite multiplier
    if (isElite)
    {
        baseHealth *= 3;
    }

    // Boss multiplier
    if (isBoss)
    {
        baseHealth *= 10;
    }

    return baseHealth;
}

int32_t Npc::calculateMana(int32_t level) const
{
    // Base mana formula
    return 50 + level * 5;
}

// ============================================================================
// Update
// ============================================================================

void Npc::update(float deltaTime)
{
    // Handle respawn timer when dead
    if (m_aiState == NpcAIState::Dead)
    {
        m_deathTimer += deltaTime * 1000.0f;  // Convert to ms
        // Respawn is triggered externally by WorldManager
        return;
    }

    // Update attack cooldown timer
    m_attackTimer += deltaTime;

    // Update spell cooldown
    if (m_spellCooldown > 0.0f)
    {
        m_spellCooldown -= deltaTime;
        if (m_spellCooldown < 0.0f)
            m_spellCooldown = 0.0f;
    }

    // Update auras
    getAuras().update(static_cast<int32_t>(deltaTime * 1000.0f));

    // AI update (Task 5.14)
    NpcAI::update(this, deltaTime);
}

// ============================================================================
// Faction / Hostility
// ============================================================================

bool Npc::isHostileTo(Entity* other) const
{
    if (!other || other == this)
        return false;

    // Dead NPCs are not hostile
    if (isDead())
        return false;

    // Player check
    ::Player* player = dynamic_cast<::Player*>(other);
    if (player)
    {
        // TODO: Implement proper faction system
        // For now: faction 1 = friendly to players, faction 2+ = hostile
        if (m_faction >= 2)
        {
            return true;
        }
    }

    // NPC check
    Npc* otherNpc = dynamic_cast<Npc*>(other);
    if (otherNpc)
    {
        // TODO: Implement faction vs faction hostility
        // For now: NPCs don't attack each other
        return false;
    }

    return false;
}

bool Npc::isFriendlyTo(Entity* other) const
{
    return !isHostileTo(other);
}

// ============================================================================
// NPC Flags
// ============================================================================

bool Npc::isVendor() const
{
    // Check flag bit for vendor
    return (m_npcFlags & static_cast<int32_t>(UnitDefines::NpcFlags::Vendor)) != 0;
}

bool Npc::isQuestGiver() const
{
    return (m_npcFlags & static_cast<int32_t>(UnitDefines::NpcFlags::QuestGiver)) != 0;
}

bool Npc::isTrainer() const
{
    return (m_npcFlags & static_cast<int32_t>(UnitDefines::NpcFlags::Trainer)) != 0;
}

bool Npc::isBanker() const
{
    return (m_npcFlags & static_cast<int32_t>(UnitDefines::NpcFlags::Banker)) != 0;
}

// ============================================================================
// Combat
// ============================================================================

int32_t Npc::getMeleeDamage() const
{
    // Simple damage calculation based on weapon value and level
    int32_t level = getVariable(ObjDefines::Variable::Level);
    return m_weaponValue + (level * 2);
}

void Npc::addThreat(Entity* attacker, int32_t amount)
{
    if (!attacker || amount <= 0)
        return;

    // Add to threat list
    m_threatManager.addThreat(attacker, amount);

    // If idle, enter combat state immediately
    // This prevents chain-reaction call-for-help cascades where each NPC
    // would otherwise stay idle, then enter combat on next tick and call for help again
    if (m_aiState == NpcAIState::Idle)
    {
        m_aiState = NpcAIState::Combat;
        setTarget(attacker);
        // Mark as already having called for help to prevent cascade
        // (this NPC was already recruited via someone else's call for help)
        m_calledForHelp = true;
        LOG_DEBUG("Npc '%s': Aggro on '%s' (threat=%d)",
                  m_name.c_str(), attacker->getName().c_str(), amount);
    }
}

void Npc::setTarget(Entity* target)
{
    if (m_target != target)
    {
        m_target = target;

        if (target)
        {
            m_aiState = NpcAIState::Combat;
            LOG_DEBUG("Npc '{}': Set target to entity {}", m_name, target->getGuid());

            if (::Player* player = dynamic_cast<::Player*>(target))
            {
                GP_Server_AggroMob packet;
                packet.m_guid = static_cast<uint32_t>(getGuid());
                packet.m_aggro = true;

                StlBuffer buf;
                uint16_t opcode = packet.getOpcode();
                buf << opcode;
                packet.pack(buf);
                player->sendPacket(buf);
            }
        }
        else
        {
            m_aiState = NpcAIState::Idle;
            LOG_DEBUG("Npc '{}': Cleared target", m_name);
        }
    }
}

// ============================================================================
// Home Position
// ============================================================================

float Npc::distanceFromHome() const
{
    float dx = getX() - m_homeX;
    float dy = getY() - m_homeY;
    return std::sqrt(dx * dx + dy * dy);
}

bool Npc::isAtHome() const
{
    return distanceFromHome() < 5.0f;  // Within 5 pixels
}

// ============================================================================
// Death / Respawn
// ============================================================================

void Npc::onDeath(Entity* killer)
{
    LOG_INFO("Npc '%s' (entry=%d) died (killer=%lu)",
             m_name.c_str(), m_entry, killer ? killer->getGuid() : 0);

    // Call base class
    Entity::onDeath(killer);

    // Set AI state to dead
    m_aiState = NpcAIState::Dead;
    m_deathTimer = 0.0f;

    // Clear target and threat list
    m_target = nullptr;
    m_threatManager.clear();

    // Generate loot (Phase 6.4)
    sLootManager.generateLoot(this, killer);

    // Quest progress (Phase 7)
    if (::Player* player = dynamic_cast<::Player*>(killer))
    {
        sQuestManager.onNpcKilled(player, m_entry);
        sExperienceSystem.onNpcKilled(player, this);
    }

    // Schedule respawn (Phase 7.1)
    sNpcSpawner.onNpcDeath(this);

    // TODO: Broadcast death animation/corpse state
}

void Npc::respawn()
{
    LOG_INFO("Npc '%s' (entry=%d) respawning at (%.1f, %.1f)",
             m_name.c_str(), m_entry, m_homeX, m_homeY);

    // Restore to home position
    setPosition(getMapId(), m_homeX, m_homeY);
    setOrientation(m_homeOrientation);

    // Restore full health/mana
    int32_t maxHealth = getVariable(ObjDefines::Variable::MaxHealth);
    int32_t maxMana = getVariable(ObjDefines::Variable::MaxMana);

    setDead(false);
    setVariable(ObjDefines::Variable::Health, maxHealth);
    if (maxMana > 0)
    {
        setVariable(ObjDefines::Variable::Mana, maxMana);
    }

    // Reset AI state
    m_aiState = NpcAIState::Idle;
    m_target = nullptr;
    m_deathTimer = 0.0f;
    m_waypointIndex = 0;
    m_waypointWaitTimer = 0.0f;
    m_hasWanderTarget = false;
    m_wanderWaitTimer = 0.0f;
    m_calledForHelp = false;

    // Clear all auras
    getAuras().clearAll(true);

    // Mark as spawned
    setSpawned(true);

    // TODO: Broadcast spawn to nearby players
    // This would require sending GP_Server_Npc to all players who can see this position
}

bool Npc::isReadyToRespawn() const
{
    return m_aiState == NpcAIState::Dead && m_deathTimer >= m_respawnTimeMs;
}
