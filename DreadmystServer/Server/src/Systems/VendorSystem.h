// VendorSystem - Manages NPC vendor buy/sell/buyback
// Phase 6, Task 6.5: Vendor Buy/Sell

#pragma once

#include <vector>
#include <unordered_map>
#include <deque>
#include <cstdint>
#include "ItemDefines.h"

// Forward declarations
class Player;
class Npc;

namespace VendorSystem
{

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_BUYBACK_SLOTS = 12;
constexpr float SELL_PRICE_MULTIPLIER = 0.25f;  // Sell price = 25% of buy price

// ============================================================================
// VendorItem - An item in a vendor's inventory
// ============================================================================

struct VendorItem
{
    int32_t itemId = 0;
    int32_t maxCount = -1;      // -1 = unlimited
    int32_t currentCount = -1;  // Current stock (-1 = unlimited)
    int32_t restockCooldown = 0; // Seconds to restock (0 = instant)
    float restockTimer = 0.0f;   // Time until next restock
};

// ============================================================================
// BuybackItem - An item sold to vendor (can be bought back)
// ============================================================================

struct BuybackItem
{
    ItemDefines::ItemId itemId;
    int32_t stackCount = 1;
    int32_t price = 0;  // Price player sold it for
};

// ============================================================================
// VendorInventory - A single vendor's inventory
// ============================================================================

struct VendorInventory
{
    int32_t npcEntry = 0;
    std::vector<VendorItem> items;
};

// ============================================================================
// VendorManager - Singleton managing all vendor interactions
// ============================================================================

class VendorManager
{
public:
    static VendorManager& instance();

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    // Load all vendor inventories from game.db
    void loadVendorData();

    // -------------------------------------------------------------------------
    // Vendor Access
    // -------------------------------------------------------------------------

    // Get vendor items for an NPC (by entry ID)
    const VendorInventory* getVendorInventory(int32_t npcEntry) const;

    // Check if NPC is a vendor
    bool isVendor(int32_t npcEntry) const;

    // -------------------------------------------------------------------------
    // Buy/Sell Operations
    // -------------------------------------------------------------------------

    // Player buys item from vendor
    // Returns true on success
    bool buyItem(Player* player, Npc* vendor, int32_t itemIndex, int32_t count);

    // Player sells item to vendor
    // Returns true on success
    bool sellItem(Player* player, Npc* vendor, int32_t inventorySlot);

    // Player buys back previously sold item
    // Returns true on success
    bool buyback(Player* player, Npc* vendor, int32_t buybackSlot);

    // -------------------------------------------------------------------------
    // Gossip Integration
    // -------------------------------------------------------------------------

    // Populate gossip menu with vendor items for a player
    void populateVendorGossip(Player* player, Npc* vendor,
                              std::vector<GP_Server_GossipMenu::VendorSlot>& outItems);

    // -------------------------------------------------------------------------
    // Buyback Management
    // -------------------------------------------------------------------------

    // Get buyback items for a player
    const std::deque<BuybackItem>& getBuybackItems(uint32_t playerGuid) const;

    // Clear buyback items for a player (on logout, etc.)
    void clearBuybackItems(uint32_t playerGuid);

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    // Update restock timers (call from game loop)
    void update(float deltaTime);

    // -------------------------------------------------------------------------
    // Price Calculation
    // -------------------------------------------------------------------------

    // Get buy price for an item
    int32_t getBuyPrice(int32_t itemId) const;

    // Get sell price for an item (what vendor pays player)
    int32_t getSellPrice(int32_t itemId) const;

private:
    VendorManager() = default;
    ~VendorManager() = default;
    VendorManager(const VendorManager&) = delete;
    VendorManager& operator=(const VendorManager&) = delete;

    // Load vendor inventory from database
    void loadVendorInventory(int32_t npcEntry);

    // Add item to player's buyback list
    void addToBuyback(uint32_t playerGuid, const ItemDefines::ItemId& itemId,
                      int32_t stackCount, int32_t price);

    // Send gold spent notification
    void sendSpentGold(Player* player, int32_t amount);

    // Send stock update to nearby players
    void sendStockUpdate(Npc* vendor, int32_t itemIndex, int32_t newStock);

    // Vendor inventories keyed by NPC entry ID
    std::unordered_map<int32_t, VendorInventory> m_vendorInventories;

    // Buyback items keyed by player GUID
    std::unordered_map<uint32_t, std::deque<BuybackItem>> m_buybackItems;

    // Empty buyback list for returning const reference
    static const std::deque<BuybackItem> s_emptyBuyback;

    bool m_loaded = false;
};

// Convenience macro
#define sVendorManager VendorSystem::VendorManager::instance()

} // namespace VendorSystem
