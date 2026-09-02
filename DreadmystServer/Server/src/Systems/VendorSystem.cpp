// VendorSystem - Manages NPC vendor buy/sell/buyback
// Phase 6, Task 6.5: Vendor Buy/Sell

#include "stdafx.h"
#include "VendorSystem.h"
#include "Inventory.h"
#include "../World/Player.h"
#include "../World/Npc.h"
#include "../World/WorldManager.h"
#include "../Database/DatabaseManager.h"
#include "../Database/GameData.h"
#include "../Core/Logger.h"
#include "../Core/Config.h"
#include "GamePacketServer.h"
#include "StlBuffer.h"
#include "NpcDefines.h"
#include "PlayerDefines.h"

#include <sqlite3.h>
#include <algorithm>

namespace VendorSystem
{

// ============================================================================
// Helper: Send world error to player
// ============================================================================

static void sendWorldError(Player* player, PlayerDefines::WorldError error)
{
    if (!player)
        return;

    GP_Server_WorldError errorPacket;
    errorPacket.m_code = static_cast<uint8_t>(error);

    StlBuffer buf;
    uint16_t opcode = errorPacket.getOpcode();
    buf << opcode;
    errorPacket.pack(buf);
    player->sendPacket(buf);
}

// ============================================================================
// Static Members
// ============================================================================

const std::deque<BuybackItem> VendorManager::s_emptyBuyback;

// ============================================================================
// VendorManager Implementation
// ============================================================================

VendorManager& VendorManager::instance()
{
    static VendorManager instance;
    return instance;
}

// ============================================================================
// Initialization
// ============================================================================

void VendorManager::loadVendorData()
{
    if (m_loaded)
        return;

    m_vendorInventories.clear();

    // Open game.db directly
    std::string gameDbPath = sConfig.getGameDbPath();
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(gameDbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("VendorManager: Failed to open game database: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // Query all vendor items
    const char* sql = "SELECT npc_entry, item, max_count, restock_cooldown FROM npc_vendor ORDER BY npc_entry";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("VendorManager: Failed to prepare vendor query: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    int itemCount = 0;
    int vendorCount = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int32_t npcEntry = sqlite3_column_int(stmt, 0);
        int32_t itemId = sqlite3_column_int(stmt, 1);
        int32_t maxCount = sqlite3_column_type(stmt, 2) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 2);
        int32_t restockCooldown = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 3);

        // Create vendor inventory if needed
        if (m_vendorInventories.find(npcEntry) == m_vendorInventories.end())
        {
            VendorInventory inv;
            inv.npcEntry = npcEntry;
            m_vendorInventories[npcEntry] = inv;
            vendorCount++;
        }

        // Add item to vendor
        VendorItem item;
        item.itemId = itemId;
        item.maxCount = maxCount;
        item.currentCount = maxCount;  // Start at max stock
        item.restockCooldown = restockCooldown;
        item.restockTimer = 0.0f;

        m_vendorInventories[npcEntry].items.push_back(item);
        itemCount++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    m_loaded = true;
    LOG_INFO("VendorManager: Loaded %zu items for %zu vendors", itemCount, vendorCount);
}

// ============================================================================
// Vendor Access
// ============================================================================

const VendorInventory* VendorManager::getVendorInventory(int32_t npcEntry) const
{
    auto it = m_vendorInventories.find(npcEntry);
    if (it == m_vendorInventories.end())
        return nullptr;
    return &it->second;
}

bool VendorManager::isVendor(int32_t npcEntry) const
{
    return m_vendorInventories.find(npcEntry) != m_vendorInventories.end();
}

// ============================================================================
// Buy/Sell Operations
// ============================================================================

bool VendorManager::buyItem(Player* player, Npc* vendor, int32_t itemIndex, int32_t count)
{
    if (!player || !vendor || count <= 0)
        return false;

    int32_t npcEntry = vendor->getEntry();
    auto it = m_vendorInventories.find(npcEntry);
    if (it == m_vendorInventories.end())
    {
        LOG_WARN("VendorManager: NPC {} is not a vendor", npcEntry);
        return false;
    }

    auto& inventory = it->second;
    if (itemIndex < 0 || itemIndex >= static_cast<int32_t>(inventory.items.size()))
    {
        LOG_WARN("VendorManager: Invalid item index {} for vendor {}", itemIndex, npcEntry);
        return false;
    }

    auto& vendorItem = inventory.items[itemIndex];

    // Check stock
    if (vendorItem.currentCount >= 0 && vendorItem.currentCount < count)
    {
        LOG_DEBUG("VendorManager: Insufficient stock for item {} (have {}, want {})",
                  vendorItem.itemId, vendorItem.currentCount, count);
        sendWorldError(player, PlayerDefines::WorldError::NoInventorySpace);
        return false;
    }

    // Get item price
    int32_t unitPrice = getBuyPrice(vendorItem.itemId);
    int32_t totalPrice = unitPrice * count;

    // Check player has enough gold
    int32_t playerGold = player->getVariable(ObjDefines::Variable::Gold);
    if (playerGold < totalPrice)
    {
        LOG_DEBUG("VendorManager: Player {} doesn't have enough gold (have {}, need {})",
                  player->getGuid(), playerGold, totalPrice);
        sendWorldError(player, PlayerDefines::WorldError::NotEnoughGold);
        return false;
    }

    // Check inventory space
    if (!player->getInventory().hasEmptySlot())
    {
        LOG_DEBUG("VendorManager: Player {} inventory full", player->getGuid());
        sendWorldError(player, PlayerDefines::WorldError::NoInventorySpace);
        return false;
    }

    // Add item to inventory
    int slot = player->getInventory().addItem(vendorItem.itemId, count);
    if (slot < 0)
    {
        sendWorldError(player, PlayerDefines::WorldError::NoInventorySpace);
        return false;
    }

    // Deduct gold
    player->setVariable(ObjDefines::Variable::Gold, playerGold - totalPrice);

    // Reduce vendor stock if limited
    if (vendorItem.currentCount >= 0)
    {
        vendorItem.currentCount -= count;
        if (vendorItem.restockCooldown > 0 && vendorItem.restockTimer <= 0.0f)
        {
            vendorItem.restockTimer = static_cast<float>(vendorItem.restockCooldown);
        }
        sendStockUpdate(vendor, itemIndex, vendorItem.currentCount);
    }

    // Send notifications
    sendSpentGold(player, totalPrice);
    player->sendInventory();

    LOG_DEBUG("VendorManager: Player {} bought item {} x{} for {} gold from vendor {}",
              player->getGuid(), vendorItem.itemId, count, totalPrice, npcEntry);

    return true;
}

bool VendorManager::sellItem(Player* player, Npc* vendor, int32_t inventorySlot)
{
    if (!player || !vendor)
        return false;

    // Get item from inventory
    auto* invItem = player->getInventory().getItem(inventorySlot);
    if (!invItem)
    {
        LOG_WARN("VendorManager: No item in slot {} for player {}", inventorySlot, player->getGuid());
        return false;
    }

    // Check if item is sellable
    const ItemTemplate* itemTmpl = sGameData.getItem(invItem->itemId);
    if (!itemTmpl)
    {
        LOG_WARN("VendorManager: Unknown item {} in slot {}", invItem->itemId, inventorySlot);
        return false;
    }

    // Calculate sell price
    int32_t sellPrice = getSellPrice(invItem->itemId) * invItem->stackCount;
    if (sellPrice <= 0)
    {
        // Item has no value
        sendWorldError(player, PlayerDefines::WorldError::VendorNotWorth);
        return false;
    }

    // Store for buyback before removing
    ItemDefines::ItemId itemId;
    itemId.m_itemId = invItem->itemId;
    int32_t stackCount = invItem->stackCount;

    // Remove from inventory
    player->getInventory().clearSlot(inventorySlot);

    // Add to buyback
    addToBuyback(player->getGuid(), itemId, stackCount, sellPrice);

    // Add gold to player
    int32_t currentGold = player->getVariable(ObjDefines::Variable::Gold);
    player->setVariable(ObjDefines::Variable::Gold, currentGold + sellPrice);

    // Update inventory
    player->sendInventory();

    LOG_DEBUG("VendorManager: Player {} sold item {} x{} for {} gold",
              player->getGuid(), itemId.m_itemId, stackCount, sellPrice);

    return true;
}

bool VendorManager::buyback(Player* player, Npc* vendor, int32_t buybackSlot)
{
    if (!player || !vendor)
        return false;

    // Get buyback list for player
    auto it = m_buybackItems.find(player->getGuid());
    if (it == m_buybackItems.end() || it->second.empty())
    {
        sendWorldError(player, PlayerDefines::WorldError::BuybackEmpty);
        return false;
    }

    auto& buybackList = it->second;
    if (buybackSlot < 0 || buybackSlot >= static_cast<int32_t>(buybackList.size()))
    {
        LOG_WARN("VendorManager: Invalid buyback slot {} for player {}", buybackSlot, player->getGuid());
        return false;
    }

    const auto& buybackItem = buybackList[buybackSlot];

    // Check player has enough gold (same price they sold for)
    int32_t playerGold = player->getVariable(ObjDefines::Variable::Gold);
    if (playerGold < buybackItem.price)
    {
        sendWorldError(player, PlayerDefines::WorldError::NotEnoughGold);
        return false;
    }

    // Check inventory space
    if (!player->getInventory().hasEmptySlot())
    {
        sendWorldError(player, PlayerDefines::WorldError::NoInventorySpace);
        return false;
    }

    // Add item back to inventory (base item ID, affixes not preserved in current implementation)
    int slot = player->getInventory().addItem(buybackItem.itemId.m_itemId, buybackItem.stackCount);
    if (slot < 0)
    {
        sendWorldError(player, PlayerDefines::WorldError::NoInventorySpace);
        return false;
    }

    // Deduct gold
    player->setVariable(ObjDefines::Variable::Gold, playerGold - buybackItem.price);

    // Remove from buyback (need to erase after reading, use iterator)
    buybackList.erase(buybackList.begin() + buybackSlot);

    // Send notifications
    sendSpentGold(player, buybackItem.price);
    player->sendInventory();

    LOG_DEBUG("VendorManager: Player {} bought back item for {} gold", player->getGuid(), buybackItem.price);

    return true;
}

// ============================================================================
// Gossip Integration
// ============================================================================

void VendorManager::populateVendorGossip(Player* player, Npc* vendor,
                                          std::vector<GP_Server_GossipMenu::VendorSlot>& outItems)
{
    (void)player;  // Reserved for reputation-based discounts and player-specific availability

    if (!vendor)
        return;

    int32_t npcEntry = vendor->getEntry();
    auto it = m_vendorInventories.find(npcEntry);
    if (it == m_vendorInventories.end())
        return;

    const auto& inventory = it->second;
    outItems.clear();
    outItems.reserve(inventory.items.size());

    for (const auto& item : inventory.items)
    {
        GP_Server_GossipMenu::VendorSlot slot;
        slot.m_itemId.m_itemId = item.itemId;
        slot.m_cost = getBuyPrice(item.itemId);
        slot.m_supply = item.currentCount;  // -1 for unlimited
        outItems.push_back(slot);
    }
}

// ============================================================================
// Buyback Management
// ============================================================================

const std::deque<BuybackItem>& VendorManager::getBuybackItems(uint32_t playerGuid) const
{
    auto it = m_buybackItems.find(playerGuid);
    if (it == m_buybackItems.end())
        return s_emptyBuyback;
    return it->second;
}

void VendorManager::clearBuybackItems(uint32_t playerGuid)
{
    m_buybackItems.erase(playerGuid);
}

void VendorManager::addToBuyback(uint32_t playerGuid, const ItemDefines::ItemId& itemId,
                                  int32_t stackCount, int32_t price)
{
    auto& buybackList = m_buybackItems[playerGuid];

    BuybackItem item;
    item.itemId = itemId;
    item.stackCount = stackCount;
    item.price = price;

    // Add to front (most recent)
    buybackList.push_front(item);

    // Limit size
    while (buybackList.size() > MAX_BUYBACK_SLOTS)
    {
        buybackList.pop_back();
    }
}

// ============================================================================
// Update
// ============================================================================

void VendorManager::update(float deltaTime)
{
    // Update restock timers for all vendors
    for (auto& pair : m_vendorInventories)
    {
        for (auto& item : pair.second.items)
        {
            if (item.restockTimer > 0.0f && item.maxCount > 0 && item.currentCount < item.maxCount)
            {
                item.restockTimer -= deltaTime;
                if (item.restockTimer <= 0.0f)
                {
                    // Restock one item
                    item.currentCount++;
                    if (item.currentCount < item.maxCount)
                    {
                        item.restockTimer = static_cast<float>(item.restockCooldown);
                    }
                    else
                    {
                        item.restockTimer = 0.0f;
                    }
                }
            }
        }
    }
}

// ============================================================================
// Price Calculation
// ============================================================================

int32_t VendorManager::getBuyPrice(int32_t itemId) const
{
    const ItemTemplate* tmpl = sGameData.getItem(itemId);
    if (!tmpl)
        return 0;
    // buyPrice = sellPrice * buyCostRatio (typical ratio is 3-4x)
    return tmpl->sellPrice * std::max(1, tmpl->buyCostRatio);
}

int32_t VendorManager::getSellPrice(int32_t itemId) const
{
    const ItemTemplate* tmpl = sGameData.getItem(itemId);
    if (!tmpl)
        return 0;

    // Use the item's sell price directly
    return tmpl->sellPrice;
}

// ============================================================================
// Private Helpers
// ============================================================================

void VendorManager::sendSpentGold(Player* player, int32_t amount)
{
    GP_Server_SpentGold packet;
    packet.m_amount = amount;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

void VendorManager::sendStockUpdate(Npc* vendor, int32_t itemIndex, int32_t newStock)
{
    if (!vendor)
        return;

    const VendorInventory* inventory = getVendorInventory(vendor->getEntry());
    if (!inventory || itemIndex < 0 || itemIndex >= static_cast<int32_t>(inventory->items.size()))
        return;

    GP_Server_UpdateVendorStock packet;
    packet.m_vendorGuid = vendor->getGuid();
    packet.m_itemId.m_itemId = inventory->items[itemIndex].itemId;
    packet.m_amount = newStock;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Broadcast to nearby players
    sWorldManager.broadcastToMap(vendor->getMapId(), buf);
}

} // namespace VendorSystem
