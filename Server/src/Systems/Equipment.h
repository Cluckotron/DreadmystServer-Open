// Equipment System - Player equipment management
// Phase 6, Task 6.3: Equipment System

#pragma once

#include "UnitDefines.h"
#include "ItemDefines.h"
#include "Inventory.h"
#include <array>
#include <cstdint>

// Forward declarations
struct ItemTemplate;
class Player;

namespace Equipment
{

// ============================================================================
// Constants
// ============================================================================

// Number of equipment slots (matches UnitDefines::EquipSlot::MaxSlot)
constexpr int NUM_SLOTS = static_cast<int>(UnitDefines::EquipSlot::MaxSlot);

// ============================================================================
// EquippedItem - A single equipped item
// ============================================================================

struct EquippedItem
{
    int32_t itemId = 0;
    int32_t durability = 100;
    int32_t enchantId = 0;
    // Affixes and gems from ItemDefines::ItemId
    int32_t affix1 = 0;
    int32_t affix2 = 0;
    int32_t gem1 = 0;
    int32_t gem2 = 0;
    int32_t gem3 = 0;

    bool isEmpty() const { return itemId == 0; }
    void clear() { itemId = 0; durability = 100; enchantId = 0; affix1 = affix2 = gem1 = gem2 = gem3 = 0; }

    // Convert to ItemDefines::ItemId for packet sending
    ItemDefines::ItemId toItemId() const
    {
        ItemDefines::ItemId id;
        id.m_itemId = itemId;
        id.m_affix1 = affix1;
        id.m_affix2 = affix2;
        id.m_gem1 = gem1;
        id.m_gem2 = gem2;
        id.m_gem3 = gem3;
        return id;
    }

    // Set from ItemDefines::ItemId
    void fromItemId(const ItemDefines::ItemId& id)
    {
        itemId = id.m_itemId;
        affix1 = id.m_affix1;
        affix2 = id.m_affix2;
        gem1 = id.m_gem1;
        gem2 = id.m_gem2;
        gem3 = id.m_gem3;
    }
};

// ============================================================================
// PlayerEquipment - Container for player's equipped items
// ============================================================================

class PlayerEquipment
{
public:
    PlayerEquipment() = default;
    ~PlayerEquipment() = default;

    // -------------------------------------------------------------------------
    // Equipment Operations
    // -------------------------------------------------------------------------

    // Get item in slot (returns nullptr if empty)
    const EquippedItem* getItem(UnitDefines::EquipSlot slot) const;
    EquippedItem* getItemMutable(UnitDefines::EquipSlot slot);

    // Equip an item to a slot (validates slot compatibility)
    // Returns true if successful, false if slot is incompatible
    bool equipItem(UnitDefines::EquipSlot slot, const EquippedItem& item);

    // Unequip item from slot, returns the item that was there
    EquippedItem unequipItem(UnitDefines::EquipSlot slot);

    // Check if slot is empty
    bool isSlotEmpty(UnitDefines::EquipSlot slot) const;

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    // Check if an item can be equipped in the given slot based on its equip_type
    static bool canEquipInSlot(int32_t equipType, UnitDefines::EquipSlot slot);

    // Get the appropriate EquipSlot for an item's equip_type
    // Returns EquipSlot::None if item is not equippable
    static UnitDefines::EquipSlot getSlotForEquipType(int32_t equipType);

    // Get the primary EquipSlot for an item (used when equip_type maps to multiple slots like Ring)
    static UnitDefines::EquipSlot getPrimarySlotForEquipType(int32_t equipType);

    // -------------------------------------------------------------------------
    // Stats
    // -------------------------------------------------------------------------

    // Calculate total stat bonuses from all equipped items
    // Returns a map of stat_type -> stat_value
    std::unordered_map<int32_t, int32_t> calculateStatBonuses() const;

    // -------------------------------------------------------------------------
    // Durability & Repair
    // -------------------------------------------------------------------------

    // Reduce durability on all equipped items (called on player death)
    // Returns true if any item durability was reduced
    bool reduceDurabilityOnDeath();

    // Calculate total repair cost for all equipped items
    // Cost is based on item level, quality, and damage amount
    int32_t calculateRepairCost() const;

    // Repair all equipped items to full durability
    // Returns true if any item was repaired
    bool repairAll();

    // Check if any equipped item needs repair
    bool needsRepair() const;

    // -------------------------------------------------------------------------
    // Database Operations
    // -------------------------------------------------------------------------

    // Load equipment from database
    void load(int32_t characterGuid);

    // Save equipment to database
    void save(int32_t characterGuid) const;

    // Dirty tracking
    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    const std::array<EquippedItem, NUM_SLOTS>& getSlots() const { return m_slots; }

private:
    std::array<EquippedItem, NUM_SLOTS> m_slots;
    bool m_dirty = false;
};

// ============================================================================
// EquipType to EquipSlot mapping (from game.db equip_type values)
// ============================================================================

// game.db equip_type values (from item_template)
namespace EquipType
{
    constexpr int32_t None = 0;
    constexpr int32_t Helm = 1;
    constexpr int32_t Necklace = 2;
    constexpr int32_t Chest = 3;
    constexpr int32_t Belt = 4;
    constexpr int32_t Legs = 5;
    constexpr int32_t Feet = 6;
    constexpr int32_t Hands = 7;
    constexpr int32_t Ring = 8;      // Can go in Ring1 or Ring2
    constexpr int32_t Weapon = 9;
    constexpr int32_t Shield = 10;   // Offhand
    constexpr int32_t Ranged = 11;
}

} // namespace Equipment
