// Equipment System - Player equipment management
// Phase 6, Task 6.3: Equipment System

#include "stdafx.h"
#include "Systems/Equipment.h"
#include "Database/DatabaseManager.h"
#include "Database/GameData.h"
#include "Core/Logger.h"

namespace Equipment
{

// ============================================================================
// Equipment Operations
// ============================================================================

const EquippedItem* PlayerEquipment::getItem(UnitDefines::EquipSlot slot) const
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= NUM_SLOTS)
        return nullptr;

    if (m_slots[idx].isEmpty())
        return nullptr;

    return &m_slots[idx];
}

EquippedItem* PlayerEquipment::getItemMutable(UnitDefines::EquipSlot slot)
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= NUM_SLOTS)
        return nullptr;

    if (m_slots[idx].isEmpty())
        return nullptr;

    return &m_slots[idx];
}

bool PlayerEquipment::equipItem(UnitDefines::EquipSlot slot, const EquippedItem& item)
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= NUM_SLOTS)
        return false;

    // Validate the item can go in this slot
    const ItemTemplate* tmpl = sGameData.getItem(item.itemId);
    if (tmpl && !canEquipInSlot(tmpl->equipType, slot))
    {
        LOG_DEBUG("Equipment: Item %d (equipType=%d) cannot go in slot %d",
                  item.itemId, tmpl->equipType, idx);
        return false;
    }

    m_slots[idx] = item;
    m_dirty = true;
    return true;
}

EquippedItem PlayerEquipment::unequipItem(UnitDefines::EquipSlot slot)
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= NUM_SLOTS)
        return EquippedItem{};

    EquippedItem item = m_slots[idx];
    m_slots[idx].clear();
    m_dirty = true;
    return item;
}

bool PlayerEquipment::isSlotEmpty(UnitDefines::EquipSlot slot) const
{
    int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= NUM_SLOTS)
        return true;

    return m_slots[idx].isEmpty();
}

// ============================================================================
// Validation
// ============================================================================

bool PlayerEquipment::canEquipInSlot(int32_t equipType, UnitDefines::EquipSlot slot)
{
    switch (equipType)
    {
        case EquipType::Helm:
            return slot == UnitDefines::EquipSlot::Helm;

        case EquipType::Necklace:
            return slot == UnitDefines::EquipSlot::Necklace;

        case EquipType::Chest:
            return slot == UnitDefines::EquipSlot::Chest;

        case EquipType::Belt:
            return slot == UnitDefines::EquipSlot::Belt;

        case EquipType::Legs:
            return slot == UnitDefines::EquipSlot::Legs;

        case EquipType::Feet:
            return slot == UnitDefines::EquipSlot::Feet;

        case EquipType::Hands:
            return slot == UnitDefines::EquipSlot::Hands;

        case EquipType::Ring:
            // Rings can go in either ring slot
            return slot == UnitDefines::EquipSlot::Ring1 ||
                   slot == UnitDefines::EquipSlot::Ring2;

        case EquipType::Weapon:
            return slot == UnitDefines::EquipSlot::Weapon1;

        case EquipType::Shield:
            return slot == UnitDefines::EquipSlot::Offhand;

        case EquipType::Ranged:
            return slot == UnitDefines::EquipSlot::Ranged;

        default:
            return false;
    }
}

UnitDefines::EquipSlot PlayerEquipment::getSlotForEquipType(int32_t equipType)
{
    switch (equipType)
    {
        case EquipType::Helm:     return UnitDefines::EquipSlot::Helm;
        case EquipType::Necklace: return UnitDefines::EquipSlot::Necklace;
        case EquipType::Chest:    return UnitDefines::EquipSlot::Chest;
        case EquipType::Belt:     return UnitDefines::EquipSlot::Belt;
        case EquipType::Legs:     return UnitDefines::EquipSlot::Legs;
        case EquipType::Feet:     return UnitDefines::EquipSlot::Feet;
        case EquipType::Hands:    return UnitDefines::EquipSlot::Hands;
        case EquipType::Ring:     return UnitDefines::EquipSlot::Ring1;  // Default to Ring1
        case EquipType::Weapon:   return UnitDefines::EquipSlot::Weapon1;
        case EquipType::Shield:   return UnitDefines::EquipSlot::Offhand;
        case EquipType::Ranged:   return UnitDefines::EquipSlot::Ranged;
        default:                  return UnitDefines::EquipSlot::None;
    }
}

UnitDefines::EquipSlot PlayerEquipment::getPrimarySlotForEquipType(int32_t equipType)
{
    // Same as getSlotForEquipType - returns the primary/default slot
    return getSlotForEquipType(equipType);
}

// ============================================================================
// Stats
// ============================================================================

std::unordered_map<int32_t, int32_t> PlayerEquipment::calculateStatBonuses() const
{
    std::unordered_map<int32_t, int32_t> bonuses;

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        // Skip items with 0 durability (broken items don't give stats)
        if (m_slots[i].durability <= 0)
            continue;

        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        if (!tmpl)
            continue;

        // Add all stat bonuses from this item
        for (int s = 0; s < 10; ++s)
        {
            if (tmpl->statType[s] != 0 && tmpl->statValue[s] != 0)
            {
                bonuses[tmpl->statType[s]] += tmpl->statValue[s];
            }
        }
    }

    return bonuses;
}

// ============================================================================
// Database Operations
// ============================================================================

void PlayerEquipment::load(int32_t characterGuid)
{
    // Clear existing
    for (auto& slot : m_slots)
        slot.clear();

    auto stmt = sDatabase.prepare(
        "SELECT slot, item_id, durability, enchant_id, affix1, affix2, gem1, gem2, gem3 "
        "FROM character_equipment "
        "WHERE character_guid = ? "
        "ORDER BY slot"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("Equipment: Failed to prepare load statement");
        return;
    }

    stmt.bind(1, characterGuid);

    while (stmt.step())
    {
        int slot = stmt.getInt(0);
        if (slot < 0 || slot >= NUM_SLOTS)
            continue;

        m_slots[slot].itemId = stmt.getInt(1);
        m_slots[slot].durability = stmt.getInt(2);
        m_slots[slot].enchantId = stmt.getInt(3);
        m_slots[slot].affix1 = stmt.getInt(4);
        m_slots[slot].affix2 = stmt.getInt(5);
        m_slots[slot].gem1 = stmt.getInt(6);
        m_slots[slot].gem2 = stmt.getInt(7);
        m_slots[slot].gem3 = stmt.getInt(8);
    }

    m_dirty = false;

    int equippedCount = 0;
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (!m_slots[i].isEmpty())
            ++equippedCount;
    }

    LOG_DEBUG("Equipment: Loaded %d equipped items for character %d", equippedCount, characterGuid);
}

void PlayerEquipment::save(int32_t characterGuid) const
{
    // Delete all existing equipment for this character
    auto deleteStmt = sDatabase.prepare(
        "DELETE FROM character_equipment WHERE character_guid = ?"
    );

    if (!deleteStmt.valid())
    {
        LOG_ERROR("Equipment: Failed to prepare delete statement");
        return;
    }

    deleteStmt.bind(1, characterGuid);
    deleteStmt.step();

    // Insert current equipment
    auto insertStmt = sDatabase.prepare(
        "INSERT INTO character_equipment "
        "(character_guid, slot, item_id, durability, enchant_id, affix1, affix2, gem1, gem2, gem3) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );

    if (!insertStmt.valid())
    {
        LOG_ERROR("Equipment: Failed to prepare insert statement");
        return;
    }

    int savedCount = 0;
    for (int slot = 0; slot < NUM_SLOTS; ++slot)
    {
        if (!m_slots[slot].isEmpty())
        {
            insertStmt.bind(1, characterGuid);
            insertStmt.bind(2, slot);
            insertStmt.bind(3, m_slots[slot].itemId);
            insertStmt.bind(4, m_slots[slot].durability);
            insertStmt.bind(5, m_slots[slot].enchantId);
            insertStmt.bind(6, m_slots[slot].affix1);
            insertStmt.bind(7, m_slots[slot].affix2);
            insertStmt.bind(8, m_slots[slot].gem1);
            insertStmt.bind(9, m_slots[slot].gem2);
            insertStmt.bind(10, m_slots[slot].gem3);
            insertStmt.step();
            insertStmt.reset();
            ++savedCount;
        }
    }

    LOG_DEBUG("Equipment: Saved %d equipped items for character %d", savedCount, characterGuid);
}

// ============================================================================
// Durability & Repair
// ============================================================================

bool PlayerEquipment::reduceDurabilityOnDeath()
{
    bool anyReduced = false;
    constexpr int DURABILITY_LOSS_ON_DEATH = 10;  // Lose 10 durability points on death

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        // Get item template to check if it has durability
        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        if (!tmpl || tmpl->durability <= 0)
            continue;  // Item has no durability

        // Reduce durability (cannot go below 0)
        if (m_slots[i].durability > 0)
        {
            m_slots[i].durability = std::max(0, m_slots[i].durability - DURABILITY_LOSS_ON_DEATH);
            anyReduced = true;
            m_dirty = true;
        }
    }

    if (anyReduced)
    {
        LOG_DEBUG("Equipment: Durability reduced on death");
    }

    return anyReduced;
}

int32_t PlayerEquipment::calculateRepairCost() const
{
    int32_t totalCost = 0;

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        if (!tmpl || tmpl->durability <= 0)
            continue;  // Item has no durability

        // Calculate damage (max durability - current durability)
        int32_t damage = tmpl->durability - m_slots[i].durability;
        if (damage <= 0)
            continue;  // No damage to repair

        // Repair cost formula: (item_level * quality_modifier * damage) / 10
        // Quality modifiers: common=1, uncommon=2, rare=3, epic=4, legendary=5
        int32_t qualityMod = std::max(1, tmpl->quality + 1);
        int32_t itemLevel = std::max(1, tmpl->itemLevel);

        int32_t itemCost = (itemLevel * qualityMod * damage) / 10;
        itemCost = std::max(1, itemCost);  // Minimum 1 gold per damaged item

        totalCost += itemCost;
    }

    return totalCost;
}

bool PlayerEquipment::repairAll()
{
    bool anyRepaired = false;

    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        if (!tmpl || tmpl->durability <= 0)
            continue;  // Item has no durability

        if (m_slots[i].durability < tmpl->durability)
        {
            m_slots[i].durability = tmpl->durability;
            anyRepaired = true;
            m_dirty = true;
        }
    }

    if (anyRepaired)
    {
        LOG_DEBUG("Equipment: All items repaired to full durability");
    }

    return anyRepaired;
}

bool PlayerEquipment::needsRepair() const
{
    for (int i = 0; i < NUM_SLOTS; ++i)
    {
        if (m_slots[i].isEmpty())
            continue;

        const ItemTemplate* tmpl = sGameData.getItem(m_slots[i].itemId);
        if (!tmpl || tmpl->durability <= 0)
            continue;  // Item has no durability

        if (m_slots[i].durability < tmpl->durability)
            return true;
    }

    return false;
}

} // namespace Equipment
