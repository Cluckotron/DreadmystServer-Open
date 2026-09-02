// LootSystem - Manages loot generation and looting
// Phase 6, Task 6.4: Loot System

#include "stdafx.h"
#include "LootSystem.h"
#include "../World/Player.h"
#include "../World/Npc.h"
#include "../World/Entity.h"
#include "../World/WorldManager.h"
#include "../Database/DatabaseManager.h"
#include "../Database/GameData.h"
#include "../Core/Logger.h"
#include "../Core/Config.h"
#include "GamePacketServer.h"
#include "GamePacketClient.h"
#include "StlBuffer.h"

#include <random>
#include <algorithm>
#include <sqlite3.h>

namespace LootSystem
{

static ItemDefines::ItemDefinition toItemDefinition(const ItemDefines::ItemId& itemId)
{
    ItemDefines::ItemDefinition def;
    def.m_itemId = itemId.m_itemId;
    def.m_affix1 = itemId.m_affix1;
    def.m_affix2 = itemId.m_affix2;
    def.m_gem1 = itemId.m_gem1;
    def.m_gem2 = itemId.m_gem2;
    def.m_gem3 = itemId.m_gem3;
    def.m_gem4 = itemId.m_gem4;
    return def;
}

// ============================================================================
// PendingLoot Implementation
// ============================================================================

bool PendingLoot::isEmpty() const
{
    if (!goldLooted && goldAmount > 0)
        return false;

    for (const auto& item : items)
    {
        if (!item.looted)
            return false;
    }
    return true;
}

int PendingLoot::getRemainingItemCount() const
{
    int count = 0;
    for (const auto& item : items)
    {
        if (!item.looted)
            ++count;
    }
    return count;
}

// ============================================================================
// LootManager Implementation
// ============================================================================

LootManager& LootManager::instance()
{
    static LootManager instance;
    return instance;
}

// ============================================================================
// Loot Generation
// ============================================================================

void LootManager::generateLoot(Npc* npc, Entity* killer)
{
    if (!npc || !killer)
        return;

    uint32_t npcGuid = npc->getGuid();
    uint32_t killerGuid = killer->getGuid();

    // Don't regenerate loot if already exists
    if (m_pendingLoot.find(npcGuid) != m_pendingLoot.end())
    {
        LOG_WARN("LootManager: Loot already exists for NPC {}", npcGuid);
        return;
    }

    // Get loot table ID from NPC template
    int32_t lootTableId = npc->getCustomLootId();
    if (lootTableId <= 0)
    {
        // No custom loot, check if template has a loot reference
        const NpcTemplate* tmpl = npc->getTemplate();
        if (tmpl)
            lootTableId = tmpl->customLoot;
    }

    // Create pending loot entry
    PendingLoot loot;
    loot.targetGuid = npcGuid;
    loot.ownerGuid = killerGuid;
    loot.freeForAllTimer = FREE_FOR_ALL_DELAY;

    // Roll loot from table
    if (lootTableId > 0)
    {
        loot.items = rollLootTable(lootTableId);
    }

    // Roll gold based on NPC level
    int32_t npcLevel = npc->getVariable(ObjDefines::Variable::Level);
    loot.goldAmount = rollGold(npcLevel, npcLevel);

    // Only store if there's something to loot
    if (!loot.items.empty() || loot.goldAmount > 0)
    {
        m_pendingLoot[npcGuid] = std::move(loot);
        LOG_DEBUG("LootManager: Generated loot for NPC {} (items={}, gold={})",
                  npcGuid, m_pendingLoot[npcGuid].items.size(), m_pendingLoot[npcGuid].goldAmount);
    }
}

void LootManager::generateLootForGameObject(uint32_t objGuid, int32_t lootTableId, uint32_t ownerGuid)
{
    if (lootTableId <= 0)
        return;

    // Don't regenerate loot if already exists
    if (m_pendingLoot.find(objGuid) != m_pendingLoot.end())
        return;

    PendingLoot loot;
    loot.targetGuid = objGuid;
    loot.ownerGuid = ownerGuid;
    loot.freeForAllTimer = FREE_FOR_ALL_DELAY;
    loot.items = rollLootTable(lootTableId);

    if (!loot.items.empty())
    {
        m_pendingLoot[objGuid] = std::move(loot);
        LOG_DEBUG("LootManager: Generated loot for GameObject {} (items={})",
                  objGuid, m_pendingLoot[objGuid].items.size());
    }
}

// ============================================================================
// Loot Table Loading and Rolling
// ============================================================================

std::vector<LootTableEntry> LootManager::loadLootTable(int32_t lootTableId)
{
    // Check cache first
    auto it = m_lootTableCache.find(lootTableId);
    if (it != m_lootTableCache.end())
        return it->second;

    std::vector<LootTableEntry> entries;

    // Open game.db directly
    std::string gameDbPath = sConfig.getGameDbPath();
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(gameDbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("LootManager: Failed to open game database: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return entries;
    }

    const char* sql = "SELECT entry, item, lootId, chance, count_min, count_max "
                      "FROM loot WHERE lootId = ?";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("LootManager: Failed to prepare loot query: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return entries;
    }

    sqlite3_bind_int(stmt, 1, lootTableId);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        LootTableEntry entry;
        entry.entry = sqlite3_column_int(stmt, 0);
        entry.itemId = sqlite3_column_int(stmt, 1);
        entry.lootTableId = sqlite3_column_int(stmt, 2);
        entry.chance = sqlite3_column_int(stmt, 3);
        entry.countMin = std::max(1, sqlite3_column_int(stmt, 4));
        entry.countMax = std::max(entry.countMin, sqlite3_column_int(stmt, 5));
        entries.push_back(entry);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // Cache for future use
    m_lootTableCache[lootTableId] = entries;

    LOG_DEBUG("LootManager: Loaded {} entries for loot table {}", entries.size(), lootTableId);
    return entries;
}

std::vector<LootItem> LootManager::rollLootTable(int32_t lootTableId)
{
    std::vector<LootItem> result;

    auto entries = loadLootTable(lootTableId);
    if (entries.empty())
        return result;

    // RNG
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> chanceDist(1, 100);

    for (const auto& entry : entries)
    {
        // Roll for drop
        int roll = chanceDist(rng);
        if (roll > entry.chance)
            continue;  // Didn't drop

        // Determine stack count
        int32_t count = entry.countMin;
        if (entry.countMax > entry.countMin)
        {
            std::uniform_int_distribution<int32_t> countDist(entry.countMin, entry.countMax);
            count = countDist(rng);
        }

        // Create loot item
        LootItem item;
        item.itemId.m_itemId = static_cast<uint16_t>(entry.itemId);
        item.stackCount = count;
        item.looted = false;
        result.push_back(item);
    }

    return result;
}

int32_t LootManager::rollGold(int32_t minLevel, int32_t maxLevel)
{
    // Base gold formula: level * 2 + random 0-5
    static thread_local std::mt19937 rng(std::random_device{}());

    int32_t baseGold = std::max(minLevel, maxLevel) * 2;
    std::uniform_int_distribution<int32_t> bonusDist(0, 5);
    return baseGold + bonusDist(rng);
}

// ============================================================================
// Loot Access
// ============================================================================

void LootManager::openLoot(Player* player, Entity* target)
{
    if (!player || !target)
        return;

    uint32_t targetGuid = target->getGuid();

    // Check if target has loot
    auto* loot = getPendingLoot(targetGuid);
    if (!loot || loot->isEmpty())
    {
        // Send empty loot window to close any open window
        GP_Server_OpenLootWindow packet;
        packet.m_objGuid = targetGuid;
        packet.m_money = 0;
        // m_items is empty

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        player->sendPacket(buf);

        LOG_DEBUG("LootManager: No loot for target {} (player {})", targetGuid, player->getGuid());
        return;
    }

    // Check if player can loot
    if (!canPlayerLoot(player, targetGuid))
    {
        LOG_DEBUG("LootManager: Player {} cannot loot target {}", player->getGuid(), targetGuid);
        return;
    }

    // Send loot window
    sendLootWindow(player, *loot);
}

void LootManager::lootItem(Player* player, uint32_t targetGuid, const ItemDefines::ItemId& itemId)
{
    if (!player)
        return;

    auto* loot = getPendingLoot(targetGuid);
    if (!loot)
    {
        LOG_WARN("LootManager: No loot found for target {}", targetGuid);
        return;
    }

    // Check if player can loot
    if (!canPlayerLoot(player, targetGuid))
    {
        LOG_WARN("LootManager: Player {} cannot loot target {}", player->getGuid(), targetGuid);
        return;
    }

    // Check for gold (special item ID)
    if (itemId.m_itemId == ItemDefines::StaticItems::GoldItem)
    {
        if (!loot->goldLooted && loot->goldAmount > 0)
        {
            // Add gold to player
            int32_t currentGold = player->getVariable(ObjDefines::Variable::Gold);
            player->setVariable(ObjDefines::Variable::Gold, currentGold + loot->goldAmount);
            loot->goldLooted = true;

            LOG_DEBUG("LootManager: Player {} looted {} gold from {}",
                      player->getGuid(), loot->goldAmount, targetGuid);
        }
    }
    else
    {
        // Find and loot specific item
        for (auto& lootItem : loot->items)
        {
            if (lootItem.looted)
                continue;

            if (lootItem.itemId.m_itemId == itemId.m_itemId)
            {
                // Check inventory space
                if (!player->getInventory().hasEmptySlot())
                {
                    LOG_WARN("LootManager: Player {} inventory full", player->getGuid());
                    // Could send error packet here
                    return;
                }

                // Add to inventory
                int slot = player->getInventory().addItem(lootItem.itemId.m_itemId, lootItem.stackCount);
                if (slot >= 0)
                {
                    lootItem.looted = true;

                    // Send item add notification
                    sendItemAddNotification(player, lootItem.itemId, lootItem.stackCount);

                    // Send updated inventory
                    player->sendInventory();

                    LOG_DEBUG("LootManager: Player {} looted item {} x{} from {}",
                              player->getGuid(), lootItem.itemId.m_itemId,
                              lootItem.stackCount, targetGuid);
                }
                break;  // Only loot one item per call
            }
        }
    }

    // Check if all loot is taken
    if (loot->isEmpty())
    {
        markAsLooted(targetGuid, player->getGuid());
        removeLoot(targetGuid);
    }
    else
    {
        // Send updated loot window
        sendLootWindow(player, *loot);
    }
}

void LootManager::lootAll(Player* player, uint32_t targetGuid)
{
    if (!player)
        return;

    auto* loot = getPendingLoot(targetGuid);
    if (!loot)
        return;

    if (!canPlayerLoot(player, targetGuid))
        return;

    // Loot gold first
    if (!loot->goldLooted && loot->goldAmount > 0)
    {
        int32_t currentGold = player->getVariable(ObjDefines::Variable::Gold);
        player->setVariable(ObjDefines::Variable::Gold, currentGold + loot->goldAmount);
        loot->goldLooted = true;
    }

    // Loot all items that fit
    for (auto& lootItem : loot->items)
    {
        if (lootItem.looted)
            continue;

        if (!player->getInventory().hasEmptySlot())
        {
            LOG_DEBUG("LootManager: Player {} inventory full during loot all", player->getGuid());
            break;
        }

        int slot = player->getInventory().addItem(lootItem.itemId.m_itemId, lootItem.stackCount);
        if (slot >= 0)
        {
            lootItem.looted = true;
            sendItemAddNotification(player, lootItem.itemId, lootItem.stackCount);
        }
    }

    // Send updated inventory
    player->sendInventory();

    // Check if all loot is taken
    if (loot->isEmpty())
    {
        markAsLooted(targetGuid, player->getGuid());
        removeLoot(targetGuid);
    }
    else
    {
        // Send updated loot window
        sendLootWindow(player, *loot);
    }
}

// ============================================================================
// Queries
// ============================================================================

bool LootManager::hasLoot(uint32_t targetGuid) const
{
    auto it = m_pendingLoot.find(targetGuid);
    if (it == m_pendingLoot.end())
        return false;
    return !it->second.isEmpty();
}

PendingLoot* LootManager::getPendingLoot(uint32_t targetGuid)
{
    auto it = m_pendingLoot.find(targetGuid);
    if (it == m_pendingLoot.end())
        return nullptr;
    return &it->second;
}

const PendingLoot* LootManager::getPendingLoot(uint32_t targetGuid) const
{
    auto it = m_pendingLoot.find(targetGuid);
    if (it == m_pendingLoot.end())
        return nullptr;
    return &it->second;
}

bool LootManager::canPlayerLoot(Player* player, uint32_t targetGuid) const
{
    if (!player)
        return false;

    auto it = m_pendingLoot.find(targetGuid);
    if (it == m_pendingLoot.end())
        return false;

    const auto& loot = it->second;

    // Owner can always loot
    if (loot.ownerGuid == player->getGuid())
        return true;

    // Free-for-all after timer expires
    if (loot.freeForAllTimer <= 0.0f)
        return true;

    // TODO: Party members could loot with different rules
    return false;
}

// ============================================================================
// Update
// ============================================================================

void LootManager::update(float deltaTime)
{
    // Update free-for-all timers
    for (auto& pair : m_pendingLoot)
    {
        if (pair.second.freeForAllTimer > 0.0f)
        {
            pair.second.freeForAllTimer -= deltaTime;
        }
    }

    // Periodic cleanup could be added here
}

void LootManager::cleanupExpiredLoot()
{
    // Remove loot that's been empty for a while
    // Could add timestamp tracking for this
    std::vector<uint32_t> toRemove;
    for (const auto& pair : m_pendingLoot)
    {
        if (pair.second.isEmpty())
        {
            toRemove.push_back(pair.first);
        }
    }

    for (uint32_t guid : toRemove)
    {
        m_pendingLoot.erase(guid);
    }
}

// ============================================================================
// Private Helpers
// ============================================================================

void LootManager::removeLoot(uint32_t targetGuid)
{
    m_pendingLoot.erase(targetGuid);
    LOG_DEBUG("LootManager: Removed loot for target {}", targetGuid);
}

void LootManager::sendLootWindow(Player* player, const PendingLoot& loot)
{
    GP_Server_OpenLootWindow packet;
    packet.m_objGuid = loot.targetGuid;
    packet.m_money = loot.goldLooted ? 0 : loot.goldAmount;

    for (const auto& item : loot.items)
    {
        if (!item.looted)
        {
            packet.m_items.push_back({toItemDefinition(item.itemId), item.stackCount});
        }
    }

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

void LootManager::sendItemAddNotification(Player* player, const ItemDefines::ItemId& itemId, int32_t count)
{
    GP_Server_NotifyItemAdd packet;
    packet.m_itemId = toItemDefinition(itemId);
    packet.m_amount = count;
    packet.m_looterName.clear();

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

void LootManager::markAsLooted(uint32_t targetGuid, uint32_t looterGuid)
{
    // Broadcast that this object was looted
    GP_Server_OnObjectWasLooted packet;
    packet.m_guid = targetGuid;
    packet.m_looterGuid = looterGuid;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Broadcast to all nearby players
    // Try to get entity from WorldManager (NPC first, then Player)
    Entity* target = sWorldManager.getNpc(targetGuid);
    if (!target)
    {
        target = sWorldManager.getPlayer(targetGuid);
    }

    if (target)
    {
        sWorldManager.broadcastToMap(target->getMapId(), buf);
    }

    LOG_DEBUG("LootManager: Marked object {} as looted", targetGuid);
}

} // namespace LootSystem
