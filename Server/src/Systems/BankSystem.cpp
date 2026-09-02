// Bank System - Player bank storage management
// Phase 6, Task 6.6: Bank System

#include "stdafx.h"
#include "Systems/BankSystem.h"
#include "Database/DatabaseManager.h"
#include "Database/GameData.h"
#include "Core/Logger.h"
#include <algorithm>

namespace Bank
{

// ============================================================================
// Basic Operations
// ============================================================================

int PlayerBank::addItem(int32_t itemId, int32_t count)
{
    if (itemId <= 0 || count <= 0)
        return INVALID_SLOT;

    // Get item template for stack info
    const ItemTemplate* tmpl = sGameData.getItem(itemId);
    int32_t maxStack = tmpl ? tmpl->stackCount : 1;
    if (maxStack <= 0) maxStack = 1;

    int32_t remaining = count;

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
            }
        }
    }

    // Then, put remainder in empty slots
    while (remaining > 0)
    {
        int emptySlot = getFirstEmptySlot();
        if (emptySlot == INVALID_SLOT)
        {
            LOG_WARN("Bank: Failed to add item %d - no empty slots", itemId);
            return INVALID_SLOT;
        }

        int32_t toAdd = std::min(remaining, maxStack);
        m_slots[emptySlot].itemId = itemId;
        m_slots[emptySlot].stackCount = toAdd;
        m_slots[emptySlot].durability = tmpl ? tmpl->durability : 100;
        m_slots[emptySlot].enchantId = 0;
        remaining -= toAdd;
        m_dirty = true;

        if (remaining <= 0)
            return emptySlot;  // Return last slot used
    }

    return INVALID_SLOT;
}

bool PlayerBank::setItem(int slot, const BankItem& item)
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return false;

    m_slots[slot] = item;
    m_dirty = true;
    return true;
}

bool PlayerBank::removeItem(int slot, int32_t count)
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
    return true;
}

void PlayerBank::clearSlot(int slot)
{
    if (slot >= 0 && slot < MAX_SLOTS)
    {
        m_slots[slot].clear();
        m_dirty = true;
    }
}

const BankItem* PlayerBank::getItem(int slot) const
{
    if (slot < 0 || slot >= MAX_SLOTS)
        return nullptr;

    if (m_slots[slot].isEmpty())
        return nullptr;

    return &m_slots[slot];
}

BankItem* PlayerBank::getItemMutable(int slot)
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

int PlayerBank::findItem(int32_t itemId) const
{
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].itemId == itemId)
            return slot;
    }
    return INVALID_SLOT;
}

bool PlayerBank::hasEmptySlot() const
{
    return getFirstEmptySlot() != INVALID_SLOT;
}

int PlayerBank::getFirstEmptySlot() const
{
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (m_slots[slot].isEmpty())
            return slot;
    }
    return INVALID_SLOT;
}

int PlayerBank::countEmptySlots() const
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

bool PlayerBank::moveItem(int fromSlot, int toSlot)
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

void PlayerBank::sort()
{
    // Collect all non-empty items
    std::vector<BankItem> items;
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        if (!m_slots[slot].isEmpty())
        {
            items.push_back(m_slots[slot]);
        }
    }

    // Sort by itemId to group same items together
    std::sort(items.begin(), items.end(), [](const BankItem& a, const BankItem& b) {
        return a.itemId < b.itemId;
    });

    // Consolidate stacks of same items where possible
    std::vector<BankItem> consolidated;
    for (const auto& item : items)
    {
        const ItemTemplate* tmpl = sGameData.getItem(item.itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (maxStack <= 0) maxStack = 1;

        // Try to add to existing stack of same type
        bool added = false;
        if (maxStack > 1)
        {
            for (auto& existing : consolidated)
            {
                if (existing.itemId == item.itemId && existing.stackCount < maxStack)
                {
                    int32_t canAdd = maxStack - existing.stackCount;
                    int32_t toAdd = std::min(canAdd, item.stackCount);
                    existing.stackCount += toAdd;

                    if (item.stackCount > toAdd)
                    {
                        // Remainder needs new slot
                        BankItem remainder = item;
                        remainder.stackCount = item.stackCount - toAdd;
                        consolidated.push_back(remainder);
                    }
                    added = true;
                    break;
                }
            }
        }

        if (!added)
        {
            consolidated.push_back(item);
        }
    }

    // Clear all slots and repopulate
    for (int slot = 0; slot < MAX_SLOTS; ++slot)
    {
        m_slots[slot].clear();
    }

    for (size_t i = 0; i < consolidated.size() && i < static_cast<size_t>(MAX_SLOTS); ++i)
    {
        m_slots[i] = consolidated[i];
    }

    m_dirty = true;
    LOG_DEBUG("Bank: Sorted - consolidated %zu items into %zu stacks",
              items.size(), consolidated.size());
}

// ============================================================================
// Database Operations
// ============================================================================

void PlayerBank::load(int32_t characterGuid)
{
    // Clear existing
    for (auto& slot : m_slots)
        slot.clear();

    auto stmt = sDatabase.prepare(
        "SELECT slot, item_id, stack_count, durability, enchant_id "
        "FROM character_bank "
        "WHERE character_guid = ? "
        "ORDER BY slot"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("Bank: Failed to prepare load statement");
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
    }

    m_dirty = false;
    LOG_DEBUG("Bank: Loaded %d items for character %d",
              MAX_SLOTS - countEmptySlots(), characterGuid);
}

void PlayerBank::save(int32_t characterGuid) const
{
    // Delete all existing bank entries for this character
    auto deleteStmt = sDatabase.prepare(
        "DELETE FROM character_bank WHERE character_guid = ?"
    );

    if (!deleteStmt.valid())
    {
        LOG_ERROR("Bank: Failed to prepare delete statement");
        return;
    }

    deleteStmt.bind(1, characterGuid);
    deleteStmt.step();

    // Insert current bank contents
    auto insertStmt = sDatabase.prepare(
        "INSERT INTO character_bank "
        "(character_guid, slot, item_id, stack_count, durability, enchant_id) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    );

    if (!insertStmt.valid())
    {
        LOG_ERROR("Bank: Failed to prepare insert statement");
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
            insertStmt.step();
            insertStmt.reset();
            ++savedCount;
        }
    }

    LOG_DEBUG("Bank: Saved %d items for character %d", savedCount, characterGuid);
}

} // namespace Bank
