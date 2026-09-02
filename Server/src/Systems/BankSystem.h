// Bank System - Player bank storage management
// Phase 6, Task 6.6: Bank System

#pragma once

#include <array>
#include <cstdint>
#include "PlayerDefines.h"

// Forward declarations
struct ItemTemplate;

namespace Bank
{

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_SLOTS = PlayerDefines::Inventory::NumSlotsBank;
constexpr int INVALID_SLOT = -1;

// ============================================================================
// BankItem - A single item in bank storage
// ============================================================================

struct BankItem
{
    int32_t itemId = 0;         // Item template entry
    int32_t stackCount = 1;     // Number in stack
    int32_t durability = 100;   // Current durability (0-100%)
    int32_t enchantId = 0;      // Applied enchant

    bool isEmpty() const { return itemId == 0; }
    void clear() { itemId = 0; stackCount = 0; durability = 100; enchantId = 0; }
};

// ============================================================================
// PlayerBank - Container for player's banked items
// ============================================================================

class PlayerBank
{
public:
    PlayerBank() = default;
    ~PlayerBank() = default;

    // -------------------------------------------------------------------------
    // Basic Operations
    // -------------------------------------------------------------------------

    // Add item to first empty slot, returns slot index or INVALID_SLOT on failure
    int addItem(int32_t itemId, int32_t count = 1);

    // Add item to specific slot (for loading from DB)
    bool setItem(int slot, const BankItem& item);

    // Remove item from slot, returns true if successful
    bool removeItem(int slot, int32_t count = 1);

    // Clear a slot completely
    void clearSlot(int slot);

    // Get item at slot (returns nullptr if empty)
    const BankItem* getItem(int slot) const;
    BankItem* getItemMutable(int slot);

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    // Find first slot containing item ID, returns INVALID_SLOT if not found
    int findItem(int32_t itemId) const;

    // Check if there's an empty slot
    bool hasEmptySlot() const;

    // Get first empty slot, returns INVALID_SLOT if none
    int getFirstEmptySlot() const;

    // Count empty slots
    int countEmptySlots() const;

    // -------------------------------------------------------------------------
    // Movement Operations
    // -------------------------------------------------------------------------

    // Move item from one bank slot to another (handles stacking/swapping)
    bool moveItem(int fromSlot, int toSlot);

    // Sort bank (group items by type)
    void sort();

    // -------------------------------------------------------------------------
    // Database Operations
    // -------------------------------------------------------------------------

    // Load bank from database
    void load(int32_t characterGuid);

    // Save bank to database
    void save(int32_t characterGuid) const;

    // Mark as dirty (needs save)
    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    int getMaxSlots() const { return MAX_SLOTS; }
    const std::array<BankItem, MAX_SLOTS>& getSlots() const { return m_slots; }

private:
    std::array<BankItem, MAX_SLOTS> m_slots;
    bool m_dirty = false;  // Needs to be saved
};

} // namespace Bank
