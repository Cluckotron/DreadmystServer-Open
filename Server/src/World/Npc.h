// Npc - Non-Player Character entity
// Task 5.13: NPC Entity
// Task 5.14: Basic NPC AI

#pragma once

#include "Entity.h"
#include "../Database/GameData.h"
#include "../AI/NpcAI.h"
#include "../AI/ThreatManager.h"
#include <string>

class Player;

// ============================================================================
// NPC AI State (for Task 5.14)
// ============================================================================

enum class NpcAIState
{
    Idle,       // Standing, not engaged
    Combat,     // In combat with a target
    Evading,    // Returning to home, resetting
    Dead,       // Waiting for respawn
};

// ============================================================================
// NPC Waypoint
// ============================================================================

struct NpcWaypoint
{
    float x = 0.0f;
    float y = 0.0f;
    float orientation = 0.0f;
    int32_t waitTimeMs = 0;
    bool run = false;
};

// ============================================================================
// NPC Entity
// ============================================================================

class Npc : public Entity
{
public:
    // Create NPC from template at spawn position
    Npc(const NpcTemplate& tmpl, int32_t mapId, float x, float y, float orientation = 0.0f);
    ~Npc() override;

    // Entity interface
    void update(float deltaTime) override;
    MutualObject::Type getType() const override { return MutualObject::Type::Npc; }
    const std::string& getName() const override { return m_name; }

    // Template info
    int32_t getEntry() const { return m_entry; }
    const NpcTemplate* getTemplate() const { return m_template; }

    // Faction/hostility
    int32_t getFaction() const { return m_faction; }
    bool isHostileTo(Entity* other) const;
    bool isFriendlyTo(Entity* other) const;

    // NPC flags
    bool isVendor() const;
    bool isQuestGiver() const;
    bool isTrainer() const;
    bool isBanker() const;
    bool isElite() const { return m_isElite; }
    bool isBoss() const { return m_isBoss; }

    // Combat
    float getAggroRange() const { return m_aggroRange; }
    float getLeashRange() const { return m_leashRange; }
    float getMeleeRange() const { return m_meleeRange; }
    int32_t getMeleeSpeed() const { return m_meleeSpeed; }
    int32_t getMeleeDamage() const;

    // AI state
    NpcAIState getAIState() const { return m_aiState; }
    void setAIState(NpcAIState state) { m_aiState = state; }

    // Combat target
    Entity* getTarget() const { return m_target; }
    void setTarget(Entity* target);
    bool hasTarget() const { return m_target != nullptr; }

    // Home position (where NPC spawned)
    float getHomeX() const { return m_homeX; }
    float getHomeY() const { return m_homeY; }
    float distanceFromHome() const;
    bool isAtHome() const;

    // Death/respawn (Task 5.11/5.12 for NPCs)
    void onDeath(Entity* killer) override;
    void respawn();
    bool isReadyToRespawn() const;
    int32_t getRespawnTimeMs() const { return m_respawnTimeMs; }
    void setRespawnTimeMs(int32_t ms) { m_respawnTimeMs = ms; }

    // Loot (stub for Phase 7)
    int32_t getCustomLootId() const { return m_customLootId; }

    // Gossip menu (stub for Phase 7)
    int32_t getGossipMenuId() const { return m_gossipMenuId; }

    // Spells (for AI combat)
    int32_t getPrimarySpellId() const { return m_primarySpellId; }

    // Threat management (Task 5.14)
    ThreatManager& getThreatManager() { return m_threatManager; }
    const ThreatManager& getThreatManager() const { return m_threatManager; }
    void addThreat(Entity* attacker, int32_t amount);

    // Attack timing (Task 5.14)
    float getTimeSinceLastAttack() const { return m_attackTimer; }
    void resetAttackTimer() { m_attackTimer = 0.0f; }

    // Spell cooldown (for NPC spell casting)
    float getSpellCooldown() const { return m_spellCooldown; }
    void setSpellCooldown(float cooldown) { m_spellCooldown = cooldown; }
    bool isSpellOnCooldown() const { return m_spellCooldown > 0.0f; }

    // Configuration
    static constexpr float DEFAULT_AGGRO_RANGE = 5.0f;
    static constexpr float DEFAULT_LEASH_RANGE = 50.0f;
    static constexpr float DEFAULT_MELEE_RANGE = 3.0f;
    static constexpr int32_t DEFAULT_RESPAWN_TIME_MS = 60000;  // 1 minute

    // Spawn info (Task 7.1)
    int32_t getSpawnId() const { return m_spawnId; }
    void setSpawnId(int32_t spawnId) { m_spawnId = spawnId; }

    int32_t getMovementType() const { return m_movementType; }
    void setMovementType(int32_t movementType) { m_movementType = movementType; }

    int32_t getPathId() const { return m_pathId; }
    void setPathId(int32_t pathId) { m_pathId = pathId; }

    float getWanderDistance() const { return m_wanderDistance; }
    void setWanderDistance(float distance) { m_wanderDistance = distance; }

    bool shouldCallForHelp() const { return m_callForHelp; }
    void setCallForHelp(bool value) { m_callForHelp = value; }

    std::vector<NpcWaypoint>& getWaypoints() { return m_waypoints; }
    const std::vector<NpcWaypoint>& getWaypoints() const { return m_waypoints; }

    int32_t getCurrentWaypointIndex() const { return m_waypointIndex; }
    void setCurrentWaypointIndex(int32_t index) { m_waypointIndex = index; }

    float getWaypointWaitTimer() const { return m_waypointWaitTimer; }
    void setWaypointWaitTimer(float timer) { m_waypointWaitTimer = timer; }

    bool hasWanderTarget() const { return m_hasWanderTarget; }
    void setWanderTarget(float x, float y)
    {
        m_wanderTargetX = x;
        m_wanderTargetY = y;
        m_hasWanderTarget = true;
    }
    void clearWanderTarget() { m_hasWanderTarget = false; }
    float getWanderTargetX() const { return m_wanderTargetX; }
    float getWanderTargetY() const { return m_wanderTargetY; }

    float getWanderWaitTimer() const { return m_wanderWaitTimer; }
    void setWanderWaitTimer(float timer) { m_wanderWaitTimer = timer; }

    bool hasCalledForHelp() const { return m_calledForHelp; }
    void setCalledForHelp(bool called) { m_calledForHelp = called; }

private:
    // Initialize stats from template
    void initFromTemplate(const NpcTemplate& tmpl);
    int32_t calculateHealth(int32_t level, bool isElite, bool isBoss) const;
    int32_t calculateMana(int32_t level) const;

    // Template reference
    int32_t m_entry = 0;
    const NpcTemplate* m_template = nullptr;
    std::string m_name;
    std::string m_subname;

    // Faction
    int32_t m_faction = 1;

    // Flags
    int32_t m_npcFlags = 0;
    bool m_isElite = false;
    bool m_isBoss = false;

    // Combat properties
    float m_aggroRange = DEFAULT_AGGRO_RANGE;
    float m_leashRange = DEFAULT_LEASH_RANGE;
    float m_meleeRange = DEFAULT_MELEE_RANGE;
    int32_t m_meleeSpeed = 2000;
    int32_t m_weaponValue = 10;

    // Home position
    float m_homeX = 0.0f;
    float m_homeY = 0.0f;
    float m_homeOrientation = 0.0f;

    // AI state
    NpcAIState m_aiState = NpcAIState::Idle;
    Entity* m_target = nullptr;
    ThreatManager m_threatManager;
    float m_attackTimer = 0.0f;  // Time since last melee attack
    float m_spellCooldown = 0.0f;  // Time until spell can be cast again

    // Death/respawn
    int32_t m_respawnTimeMs = DEFAULT_RESPAWN_TIME_MS;
    float m_deathTimer = 0.0f;

    // Content references
    int32_t m_gossipMenuId = 0;
    int32_t m_customLootId = 0;
    int32_t m_primarySpellId = 0;

    // Spawn info (Task 7.1)
    int32_t m_spawnId = 0;
    int32_t m_movementType = 0;
    int32_t m_pathId = 0;
    float m_wanderDistance = 0.0f;
    bool m_callForHelp = true;

    // Movement state (Task 7.4)
    std::vector<NpcWaypoint> m_waypoints;
    int32_t m_waypointIndex = 0;
    float m_waypointWaitTimer = 0.0f;
    float m_wanderTargetX = 0.0f;
    float m_wanderTargetY = 0.0f;
    bool m_hasWanderTarget = false;
    float m_wanderWaitTimer = 0.0f;

    // Combat coordination
    bool m_calledForHelp = false;
};
