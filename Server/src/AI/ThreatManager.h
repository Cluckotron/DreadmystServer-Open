// ThreatManager - NPC threat/aggro tracking
// Task 7.2: NPC Aggro System

#pragma once

#include <unordered_map>
#include <cstdint>

class Entity;

class ThreatManager
{
public:
    void addThreat(Entity* source, int32_t amount);
    void modifyThreat(Entity* source, float multiplier);
    void removeThreat(Entity* source);
    Entity* getHighestThreat() const;
    int32_t getThreat(Entity* source) const;
    void clear();
    bool hasThreat() const { return !m_threatTable.empty(); }

private:
    std::unordered_map<uint64_t, int32_t> m_threatTable;
};
