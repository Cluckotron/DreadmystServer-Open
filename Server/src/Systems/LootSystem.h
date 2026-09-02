// LootSystem - Manages loot generation and looting
// Phase 6, Task 6.4: Loot System

#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "ItemDefines.h"

// Forward declarations
class Player;
class Entity;
class Npc;

namespace LootSystem
{

// ============================================================================
// Constants
// ============================================================================

constexpr float FREE_FOR_ALL_DELAY = 60.0f;  // Seconds before anyone can loot

// ============================================================================
// LootItem - A single item in loot
// ============================================================================

struct LootItem
{
    ItemDefines::ItemId itemId;
    int32_t stackCount = 1;
    bool looted = false;  // Has this item been taken?
};

// ============================================================================
// PendingLoot - Loot waiting to be picked up
// ============================================================================

struct PendingLoot
{
    uint32_t targetGuid = 0;      // GUID of the lootable object
    uint32_t ownerGuid = 0;       // Player who has loot rights
    int32_t goldAmount = 0;       // Gold to loot
    std::vector<LootItem> items;  // Items to loot
    float freeForAllTimer = 0.0f; // When anyone can loot
    bool goldLooted = false;      // Has gold been taken?

    // Check if all loot has been taken
    bool isEmpty() const;

    // Get remaining item count
    int getRemainingItemCount() const;
};

// ============================================================================
// LootTableEntry - Entry from game.db loot table
// ============================================================================

struct LootTableEntry
{
    int32_t entry = 0;
    int32_t itemId = 0;
    int32_t lootTableId = 0;
    int32_t chance = 100;      // Percentage chance (0-100)
    int32_t countMin = 1;
    int32_t countMax = 1;
    // Conditions could be added here
};

// ============================================================================
// LootManager - Singleton managing all pending loot
// ============================================================================

class LootManager
{
public:
    static LootManager& instance();

    // -------------------------------------------------------------------------
    // Loot Generation
    // -------------------------------------------------------------------------

    // Generate loot when NPC dies
    void generateLoot(Npc* npc, Entity* killer);

    // Generate loot for a game object (chests, etc.)
    void generateLootForGameObject(uint32_t objGuid, int32_t lootTableId, uint32_t ownerGuid);

    // -------------------------------------------------------------------------
    // Loot Access
    // -------------------------------------------------------------------------

    // Player opens loot window on target
    void openLoot(Player* player, Entity* target);

    // Player takes specific item from loot
    void lootItem(Player* player, uint32_t targetGuid, const ItemDefines::ItemId& itemId);

    // Player takes all items from loot
    void lootAll(Player* player, uint32_t targetGuid);

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    // Check if entity has loot
    bool hasLoot(uint32_t targetGuid) const;

    // Get pending loot for entity
    PendingLoot* getPendingLoot(uint32_t targetGuid);
    const PendingLoot* getPendingLoot(uint32_t targetGuid) const;

    // Check if player can loot target
    bool canPlayerLoot(Player* player, uint32_t targetGuid) const;

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    // Update timers (call from game loop)
    void update(float deltaTime);

    // Clean up old loot
    void cleanupExpiredLoot();

private:
    LootManager() = default;
    ~LootManager() = default;
    LootManager(const LootManager&) = delete;
    LootManager& operator=(const LootManager&) = delete;

    // Load loot table entries from game.db
    std::vector<LootTableEntry> loadLootTable(int32_t lootTableId);

    // Roll items from loot table
    std::vector<LootItem> rollLootTable(int32_t lootTableId);

    // Calculate gold drop
    int32_t rollGold(int32_t minLevel, int32_t maxLevel);

    // Remove loot entry
    void removeLoot(uint32_t targetGuid);

    // Send loot window packet to player
    void sendLootWindow(Player* player, const PendingLoot& loot);

    // Send item added notification
    void sendItemAddNotification(Player* player, const ItemDefines::ItemId& itemId, int32_t count);

    // Mark object as fully looted
    void markAsLooted(uint32_t targetGuid, uint32_t looterGuid);

    // All pending loot keyed by target GUID
    std::unordered_map<uint32_t, PendingLoot> m_pendingLoot;

    // Cache of loaded loot tables
    std::unordered_map<int32_t, std::vector<LootTableEntry>> m_lootTableCache;
};

// Convenience macro
#define sLootManager LootSystem::LootManager::instance()

} // namespace LootSystem
