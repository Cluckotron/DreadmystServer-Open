// ItemDefiner.h - Item definition and stat calculation singleton
#pragma once

#include <map>
#include <memory>
#include <vector>
#include <string>
#include "UnitDefines.h"

class SqlConnector;

// Item affix data (prefix/suffix modifiers)
class ItemAffix
{
public:
    ItemAffix() = default;
    ~ItemAffix() = default;

    // Get stats in their display order
    const std::vector<UnitDefines::Stat>& statOrder() const { return m_statOrder; }

    // Stat values for this affix
    std::map<UnitDefines::Stat, int> stats;
    std::string name;
    int id = 0;

    void addStat(UnitDefines::Stat stat, int value)
    {
        stats[stat] = value;
        m_statOrder.push_back(stat);
    }

private:
    std::vector<UnitDefines::Stat> m_statOrder;
};

// Item template data loaded from database
struct ItemTemplate
{
    int entry = 0;
    std::string name;
    int requiredLevel = 0;
    int quality = 0;
    int armorType = 0;
    int weaponType = 0;
    int equipSlot = 0;
    std::map<UnitDefines::Stat, int> baseStats;
};

// Singleton for managing item definitions and stat calculations
class ItemDefiner
{
public:
    static ItemDefiner* instance()
    {
        static ItemDefiner s_instance;
        return &s_instance;
    }

    // Load item templates from database
    void loadItemTemplates(std::shared_ptr<SqlConnector> database)
    {
        // Load item_template table
        if (!database) return;

        // Stub - actual implementation would query database
        (void)database;
    }

    // Load item affixes from database
    void loadAffixes(std::shared_ptr<SqlConnector> database)
    {
        // Load item_affix table
        if (!database) return;

        // Stub - actual implementation would query database
        (void)database;
    }

    // Reload a single item entry
    void reloadItemEntry(std::shared_ptr<SqlConnector> database, int entry)
    {
        if (!database) return;
        (void)entry;
    }

    // Get base item stats (before affixes)
    std::map<UnitDefines::Stat, int> baseItemStats(int entry, int requiredLevel, int enchantLevel)
    {
        std::map<UnitDefines::Stat, int> result;

        auto it = m_itemTemplates.find(entry);
        if (it != m_itemTemplates.end())
        {
            result = it->second.baseStats;
        }

        // Apply enchant scaling
        (void)requiredLevel;
        (void)enchantLevel;

        return result;
    }

    // Get final item stats (with affixes applied)
    std::map<UnitDefines::Stat, int> crunchItemStats(int entry, int affixId, int affixScore, int requiredLevel, int enchantLevel)
    {
        std::map<UnitDefines::Stat, int> result = baseItemStats(entry, requiredLevel, enchantLevel);

        // Apply affix stats
        auto affix = getAffix(affixId);
        if (affix)
        {
            for (auto& [stat, value] : affix->stats)
            {
                // Scale by affix score (0-100 typically)
                int scaledValue = (value * affixScore) / 100;
                result[stat] += scaledValue;
            }
        }

        return result;
    }

    // Get affix data by ID
    std::shared_ptr<ItemAffix> getAffix(int affixId)
    {
        auto it = m_affixes.find(affixId);
        if (it != m_affixes.end())
            return it->second;
        return nullptr;
    }

    // Get item template by entry
    const ItemTemplate* getItemTemplate(int entry) const
    {
        auto it = m_itemTemplates.find(entry);
        if (it != m_itemTemplates.end())
            return &it->second;
        return nullptr;
    }

private:
    ItemDefiner() = default;
    ~ItemDefiner() = default;

    std::map<int, ItemTemplate> m_itemTemplates;
    std::map<int, std::shared_ptr<ItemAffix>> m_affixes;
};

#define sItemDefiner ItemDefiner::instance()
