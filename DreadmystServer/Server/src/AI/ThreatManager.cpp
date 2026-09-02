// ThreatManager - NPC threat/aggro tracking
// Task 7.2: NPC Aggro System

#include "stdafx.h"
#include "ThreatManager.h"
#include "../World/WorldManager.h"
#include "../World/Entity.h"
#include "../World/Player.h"
#include "../World/Npc.h"

void ThreatManager::addThreat(Entity* source, int32_t amount)
{
    if (!source || amount <= 0)
        return;

    uint64_t guid = source->getGuid();
    m_threatTable[guid] += amount;
}

void ThreatManager::modifyThreat(Entity* source, float multiplier)
{
    if (!source)
        return;

    uint64_t guid = source->getGuid();
    auto it = m_threatTable.find(guid);
    if (it == m_threatTable.end())
        return;

    it->second = static_cast<int32_t>(it->second * multiplier);
    if (it->second <= 0)
        m_threatTable.erase(it);
}

void ThreatManager::removeThreat(Entity* source)
{
    if (!source)
        return;

    m_threatTable.erase(source->getGuid());
}

Entity* ThreatManager::getHighestThreat() const
{
    Entity* best = nullptr;
    int32_t highest = -1;

    for (const auto& [guid, threat] : m_threatTable)
    {
        if (threat <= 0)
            continue;

        Entity* candidate = sWorldManager.getPlayer(static_cast<uint32_t>(guid));
        if (!candidate)
            candidate = sWorldManager.getNpc(static_cast<uint32_t>(guid));

        if (!candidate || candidate->isDead())
            continue;

        if (threat > highest)
        {
            highest = threat;
            best = candidate;
        }
    }

    return best;
}

int32_t ThreatManager::getThreat(Entity* source) const
{
    if (!source)
        return 0;

    auto it = m_threatTable.find(source->getGuid());
    return it != m_threatTable.end() ? it->second : 0;
}

void ThreatManager::clear()
{
    m_threatTable.clear();
}
