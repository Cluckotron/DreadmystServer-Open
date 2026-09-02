// NpcAI - Basic NPC artificial intelligence system
// Task 5.14: Basic NPC AI

#pragma once

#include <cstdint>
#include <vector>

class Npc;
class Entity;
class Player;

// ============================================================================
// NpcAI Namespace - AI behavior functions
// ============================================================================

namespace NpcAI
{
    // Main AI update - called each tick from Npc::update()
    void update(Npc* npc, float deltaTime);

    // State-specific updates
    void updateIdle(Npc* npc, float deltaTime);
    void updateCombat(Npc* npc, float deltaTime);
    void updateEvading(Npc* npc, float deltaTime);

    // Aggro detection - finds hostile targets in range
    Player* findAggroTarget(Npc* npc);

    // Combat logic
    void performMeleeAttack(Npc* npc, Entity* target);
    void performSpellCast(Npc* npc, Entity* target, int32_t spellId);
    bool isInMeleeRange(Npc* npc, Entity* target);
    bool canCastSpell(Npc* npc, int32_t spellId, Entity* target);
    int32_t selectSpellToCast(Npc* npc, Entity* target);

    // Idle movement
    void updatePatrol(Npc* npc, float deltaTime);
    void updateWander(Npc* npc, float deltaTime);

    // Combat helpers
    void callForHelp(Npc* npc, Entity* target);

    // Movement
    void moveTowards(Npc* npc, float targetX, float targetY, float deltaTime);
    void moveTowardsEntity(Npc* npc, Entity* target, float deltaTime);
    void returnHome(Npc* npc, float deltaTime);

    // Leash check - returns true if NPC should evade
    bool shouldLeash(Npc* npc);

    // Target validation
    bool isValidTarget(Npc* npc, Entity* target);

    // Calculate orientation from NPC to target
    float calculateOrientation(float fromX, float fromY, float toX, float toY);

    // Configuration
    constexpr float NPC_MOVE_SPEED = 4.0f;          // Logical world cells per second
    constexpr float MELEE_ATTACK_COOLDOWN = 2.0f;   // Seconds between melee attacks
    constexpr float EVADE_SPEED_MULTIPLIER = 2.0f;  // NPCs move faster when evading
    constexpr float HOME_ARRIVAL_DISTANCE = 0.35f;  // Logical cells; close enough to reset at home
}
