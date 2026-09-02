// Inventory System - Player inventory management
// Phase 6, Task 6.1: Inventory System

#include "stdafx.h"
#include "Systems/Inventory.h"
#include "Database/DatabaseManager.h"
#include "Database/GameData.h"
#include "Core/Logger.h"
#include "World/Player.h"

namespace Inventory
{

// ============================================================================
// Basic Operations
// ============================================================================

int PlayerInventory::addItem(int32_t itemId, int32_t count)
{
    if (itemId <= 0 || count <= 0)
        return INVALID_SLOT;

    // Get item template for stack info
    const ItemTemplate* tmpl = sGameData.getItem(itemId);
    int32_t maxStack = tmpl ? tmpl->stackCount : 1;
    if (maxStack <= 0) maxStack = 1;

    int32_t remaining = count;

    bool changed = false;
    int lastSlot = INVALID_SLOT;

    // First, try to stack with existing items of same type
    if (maxStack > 1)
    {
        for (int slot = 0; slot < MAX_SLOTS && remaining > 0; ++slot)
        {
            if (m_slots[slot].itemId == itemId && m_slots[slot].stackCount < maxStack)
            {
                int32_t canAdd = maxStack - m_slots[slot].stackCount;
                int32_t toAdd = std::min(canAdd, remaining);
                m_slots[slot].stackCount += toAdd;
                remaining -= toAdd;
                m_dirty = true;
                changed = true;
            }
        }
    }

    // Then, put remainder in empty slots
    while (remaining > 0)
    {
        int emptySlot = getFirstEmptySlot();
        if (emptySlot == INVALID_SLOT)
        {
            LOG_WARN("Inventory: Failed to add item %d - no empty slots", itemId);
            if (changed)
                notifyOwner();
            return INVALID_SLOT;
        }

        int32_t toAdd = std::min(remaining, maxStack);
        m_slots[emptySlot].itemId = itemId;
        m_slots[emptySlot].stackCount = toAdd;
        m_slots[emptySlot].durability = tmpl ? tmpl->durability : 100;
        m_slots[emptySlot].enchantId = 0;
        m_slots[emptySlot].flags = 0;
        remaining -= toAdd;
        m_dirty = true;
        changed = true;
        lastSlot = emptySlot;

        if (remaining <= 0)
            break;
    }

    if (changed)
        notifyOwner();

    return lastSlot;
}

bool PlayerInventory::setItem(int slot, const InventoryItem& item)
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return false;

    m_slots[slot] = item;
    m_dirty = true;
    return true;
}

bool PlayerInventory::removeItem(int slot, int32_t count)
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return false;

    if (m_slots[slot].isEmpty())
        return false;

    if (count <= 0)
        return false;

    if (m_slots[slot].stackCount <= count)
    {
        // Remove entire stack
        m_slots[slot].clear();
    }
    else
    {
        // Reduce stack
        m_slots[slot].stackCount -= count;
    }

    m_dirty = true;
    notifyOwner();
    return true;
}

void PlayerInventory::clearSlot(int slot)
{
    if (slot >= 0 && slot < MAX_SLOTS)
    {
        m_slots[slot].clear();
        m_dirty = true;
        notifyOwner();
    }
}

const InventoryItem* PlayerInventory::getItem(int slot) const
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return nullptr;

    if (m_slots[slot].isEmpty())
        return nullptr;

    return &m_slots[slot];
}

InventoryItem* PlayerInventory::getItemMutable(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return nullptr;

    if (m_slots[slot].isEmpty())
        return nullptr;

    return &m_slots[slot];
}

// ============================================================================
// Search Operations
// ============================================================================

int PlayerInventory::findItem(int32_t itemId) const
{
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].itemId == itemId)
            return slot;
    }
    return INVALID_SLOT;
}

std::vector<int> PlayerInventory::findAllItems(int32_t itemId) const
{
    std::vector<int> slots;
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].itemId == itemId)
            slots.push_back(slot);
    }
    return slots;
}

bool PlayerInventory::hasItem(int32_t itemId, int32_t count) const
{
    return countItem(itemId) >= count;
}

int32_t PlayerInventory::countItem(int32_t itemId) const
{
    int32_t total = 0;
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].itemId == itemId)
            total += m_slots[slot].stackCount;
    }
    return total;
}

bool PlayerInventory::hasEmptySlot() const
{
    return getFirstEmptySlot() != INVALID_SLOT;
}

int PlayerInventory::getFirstEmptySlot() const
{
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].isEmpty())
            return slot;
    }
    return INVALID_SLOT;
}

int PlayerInventory::countEmptySlots() const
{
    int count = 0;
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].isEmpty())
            ++count;
    }
    return count;
}

// ============================================================================
// Movement Operations
// ============================================================================

bool PlayerInventory::moveItem(int fromSlot, int toSlot)
{
    if (fromSlot < 0 || fromSlot >= MAX_SLOTS ||
        toSlot < 0 || toSlot >= MAX_SLOTS)
        return false;

    if (fromSlot == toSlot)
        return true;

    auto& from = m_slots[fromSlot];
    auto& to = m_slots[toSlot];

    if (from.isEmpty())
        return false;  // Nothing to move

    if (to.isEmpty())
    {
        // Simple move to empty slot
        to = from;
        from.clear();
    }
    else if (from.itemId == to.itemId)
    {
        // Try to stack same item types
        const ItemTemplate* tmpl = sGameData.getItem(from.itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (maxStack <= 0) maxStack = 1;

        int32_t canAdd = maxStack - to.stackCount;
        int32_t toAdd = std::min(canAdd, from.stackCount);

        if (toAdd > 0)
        {
            to.stackCount += toAdd;
            from.stackCount -= toAdd;
            if (from.stackCount <= 0)
                from.clear();
        }
        else
        {
            // Target is full, swap instead
            std::swap(from, to);
        }
    }
    else
    {
        // Different items, swap
        std::swap(from, to);
    }

    m_dirty = true;
    return true;
}

bool PlayerInventory::splitStack(int fromSlot, int toSlot, int32_t count)
{
    if (fromSlot < 0 || fromSlot >= MAX_SLOTS ||
        toSlot < 0 || toSlot >= MAX_SLOTS)
        return false;

    if (fromSlot == toSlot)
        return false;

    auto& from = m_slots[fromSlot];
    auto& to = m_slots[toSlot];

    if (from.isEmpty())
        return false;

    if (count <= 0 || count >= from.stackCount)
        return false;  // Can't split 0 or entire stack

    if (!to.isEmpty())
    {
        // Target must be empty or same item with room
        if (to.itemId != from.itemId)
            return false;

        const ItemTemplate* tmpl = sGameData.getItem(from.itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (to.stackCount + count > maxStack)
            return false;  // Would exceed max stack
    }

    // Perform split
    if (to.isEmpty())
    {
        to.itemId = from.itemId;
        to.stackCount = count;
        to.durability = from.durability;
        to.enchantId = from.enchantId;
        to.flags = from.flags;
    }
    else
    {
        to.stackCount += count;
    }

    from.stackCount -= count;
    m_dirty = true;
    return true;
}

void PlayerInventory::sort()
{
    // Step 1: Consolidate stacks of same item
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (maxStack <= 0) maxStack = 1;

        // Try to merge with later slots of same item
        for (int j = i + 1; j < MAX_SLOTS && m_slots[i].stackCount < maxStack; ++j)
        {
            if (m_slots[j].itemId == m_slots[i].itemId)
            {
                int32_t canAdd = maxStack - m_slots[i].stackCount;
                int32_t toAdd = std::min(canAdd, m_slots[j].stackCount);

                m_slots[i].stackCount += toAdd;
                m_slots[j].stackCount -= toAdd;

                if (m_slots[j].stackCount <= 0)
                    m_slots[j].clear();
            }
        }
    }

    // Step 2: Collect non-empty items into a vector
    std::vector<InventoryItem> items;
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (!m_slots[i].isEmpty())
        {
            items.push_back(m_slots[i]);
            m_slots[i].clear();
        }
    }

    // Step 3: Sort by item ID
    std::sort(items.begin(), items.end(), [](const InventoryItem& a, const InventoryItem& b) {
        return a.itemId < b.itemId;
    });

    // Step 4: Place items back into inventory starting from slot 0
    for (size_t i = 0; i < items.size() && i < MAX_SLOTS; ++i)
    {
        m_slots[i] = items[i];
    }

    m_dirty = true;
    notifyOwner();
}

// ============================================================================
// Database Operations
// ============================================================================

void PlayerInventory::load(int32_t characterGuid)
{
    // Clear existing
    for (auto& slot : m_slots)
        slot.clear();

    auto stmt = sDatabase.prepare(
        "SELECT slot, item_id, stack_count, durability, enchant_id, flags "
        "FROM character_inventory "
        "WHERE character_guid = ? "
        "ORDER BY slot"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("Inventory: Failed to prepare load statement");
        return;
    }

    stmt.bind(1, characterGuid);

    while (stmt.step())
    {
        int slot = stmt.getInt(0);
        if (slot < 0 || slot >= MAX_SLOTS)
            continue;

        m_slots[slot].itemId = stmt.getInt(1);
        m_slots[slot].stackCount = stmt.getInt(2);
        m_slots[slot].durability = stmt.getInt(3);
        m_slots[slot].enchantId = stmt.getInt(4);
        m_slots[slot].flags = stmt.getInt(5);
    }

    m_dirty = false;
    LOG_DEBUG("Inventory: Loaded %d items for character %d",
              MAX_SLOTS - countEmptySlots(), characterGuid);
}

void PlayerInventory::save(int32_t characterGuid) const
{
    // Delete all existing inventory for this character
    auto deleteStmt = sDatabase.prepare(
        "DELETE FROM character_inventory WHERE character_guid = ?"
    );

    if (!deleteStmt.valid())
    {
        LOG_ERROR("Inventory: Failed to prepare delete statement");
        return;
    }

    deleteStmt.bind(1, characterGuid);
    deleteStmt.step();

    // Insert current inventory
    auto insertStmt = sDatabase.prepare(
        "INSERT INTO character_inventory "
        "(character_guid, slot, item_id, stack_count, durability, enchant_id, flags) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );

    if (!insertStmt.valid())
    {
        LOG_ERROR("Inventory: Failed to prepare insert statement");
        return;
    }

    int savedCount = 0;
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (!m_slots[slot].isEmpty())
        {
            insertStmt.bind(1, characterGuid);
            insertStmt.bind(2, slot);
            insertStmt.bind(3, m_slots[slot].itemId);
            insertStmt.bind(4, m_slots[slot].stackCount);
            insertStmt.bind(5, m_slots[slot].durability);
            insertStmt.bind(6, m_slots[slot].enchantId);
            insertStmt.bind(7, m_slots[slot].flags);
            insertStmt.step();
            insertStmt.reset();
            ++savedCount;
        }
    }

    LOG_DEBUG("Inventory: Saved %d items for character %d", savedCount, characterGuid);
}

void PlayerInventory::notifyOwner()
{
    if (m_owner && m_notifyEnabled)
        m_owner->onInventoryChanged();
}

} // namespace Inventory
