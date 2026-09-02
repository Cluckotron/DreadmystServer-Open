// Inventory System - Player inventory management
// Phase 6, Task 6.1: Inventory System

#pragma once

#include <array>
#include <optional>
#include <cstdint>

// Forward declarations
struct ItemTemplate;
class Player;

namespace Inventory
{

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_SLOTS = 40;
constexpr int INVALID_SLOT = -1;

// ============================================================================
// InventoryItem - A single item in inventory
// ============================================================================

struct InventoryItem
{
    int32_t itemId = 0;         // Item template entry
    int32_t stackCount = 1;     // Number in stack
    int32_t durability = 100;   // Current durability (0-100%)
    int32_t enchantId = 0;      // Applied enchant
    int32_t flags = 0;          // Item flags (soulbound, etc.)
    int32_t gem1 = 0;           // First socketed gem ID
    int32_t gem2 = 0;           // Second socketed gem ID
    int32_t gem3 = 0;           // Third socketed gem ID
    int32_t affix1 = 0;         // Random affix 1
    int32_t affix2 = 0;         // Random affix 2

    bool isEmpty() const { return itemId == 0; }
    void clear() { itemId = 0; stackCount = 0; durability = 100; enchantId = 0; flags = 0; gem1 = 0; gem2 = 0; gem3 = 0; affix1 = 0; affix2 = 0; }
};

// ============================================================================
// PlayerInventory - Container for player's items
// ============================================================================

class PlayerInventory
{
public:
    PlayerInventory() = default;
    ~PlayerInventory() = default;

    // -------------------------------------------------------------------------
    // Basic Operations
    // -------------------------------------------------------------------------

    // Add item to first empty slot, returns slot index or INVALID_SLOT on failure
    int addItem(int32_t itemId, int32_t count = 1);

    // Add item to specific slot (for loading from DB)
    bool setItem(int slot, const InventoryItem& item);

    // Remove item from slot, returns true if successful
    bool removeItem(int slot, int32_t count = 1);

    // Clear a slot completely
    void clearSlot(int slot);

    // Get item at slot (returns nullptr if empty)
    const InventoryItem* getItem(int slot) const;
    InventoryItem* getItemMutable(int slot);

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    // Find first slot containing item ID, returns INVALID_SLOT if not found
    int findItem(int32_t itemId) const;

    // Find all slots containing item ID
    std::vector<int> findAllItems(int32_t itemId) const;

    // Check if player has at least 'count' of item
    bool hasItem(int32_t itemId, int32_t count = 1) const;

    // Get total count of specific item across all slots
    int32_t countItem(int32_t itemId) const;

    // Check if there's an empty slot
    bool hasEmptySlot() const;

    // Get first empty slot, returns INVALID_SLOT if none
    int getFirstEmptySlot() const;

    // Count empty slots
    int countEmptySlots() const;

    // -------------------------------------------------------------------------
    // Movement Operations
    // -------------------------------------------------------------------------

    // Move item from one slot to another (handles stacking/swapping)
    bool moveItem(int fromSlot, int toSlot);

    // Split a stack into another slot
    bool splitStack(int fromSlot, int toSlot, int32_t count);

    // Sort inventory: consolidate stacks and sort by item ID
    void sort();

    // -------------------------------------------------------------------------
    // Database Operations
    // -------------------------------------------------------------------------

    // Load inventory from database
    void load(int32_t characterGuid);

    // Save inventory to database
    void save(int32_t characterGuid) const;

    // Owner (for change notifications)
    void setOwner(Player* owner) { m_owner = owner; }
    void setNotificationsEnabled(bool enabled) { m_notifyEnabled = enabled; }
    bool notificationsEnabled() const { return m_notifyEnabled; }

    // Mark as dirty (needs save)
    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    int getMaxSlots() const { return MAX_SLOTS; }
    const std::array<InventoryItem, MAX_SLOTS>& getSlots() const { return m_slots; }

private:
    void notifyOwner();

    std::array<InventoryItem, MAX_SLOTS> m_slots;
    bool m_dirty = false;  // Needs to be saved
    Player* m_owner = nullptr;
    bool m_notifyEnabled = true;
};

} // namespace Inventory
