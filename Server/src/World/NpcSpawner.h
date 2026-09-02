// NpcSpawner - Loads and spawns NPCs from game.db
// Task 7.1: NPC Spawner

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

#include "Npc.h"

class Npc;

struct NpcSpawnInfo
{
    int32_t spawnId = 0;
    int32_t npcEntry = 0;
    int32_t mapId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float orientation = 0.0f;
    int32_t respawnSeconds = 60;
    int32_t movementType = 0;
    int32_t pathId = 0;
    float wanderDistance = 0.0f;
    bool callForHelp = true;
};

class NpcSpawner
{
public:
    static NpcSpawner& instance();

    void loadSpawnsForMap(int mapId);
    void spawnAllForMap(int mapId);
    void onNpcDeath(Npc* npc);
    void update(float deltaTime);

private:
    NpcSpawner() = default;

    struct NpcGroupEntry
    {
        int32_t memberSpawnId = 0;
        float distance = 0.0f;
        float angle = 0.0f;
        bool linkedRespawn = false;
        bool linkedLoot = false;
    };

    void loadGroups();
    void loadWaypoints(int32_t pathId);
    void respawnSpawn(int32_t spawnId);

    std::unordered_map<int32_t, NpcSpawnInfo> m_spawns;
    std::unordered_map<int32_t, std::vector<int32_t>> m_spawnsByMap;
    std::unordered_map<int32_t, uint32_t> m_spawnToNpcGuid;
    std::unordered_map<int32_t, float> m_respawnTimers;
    std::unordered_set<int32_t> m_loadedMaps;

    std::unordered_map<int32_t, std::vector<NpcGroupEntry>> m_groupsByLeader;
    std::unordered_map<int32_t, int32_t> m_spawnToGroupLeader;
    std::unordered_set<int32_t> m_loadedPathIds;
    std::unordered_map<int32_t, std::vector<NpcWaypoint>> m_waypointsByPathId;
    bool m_groupsLoaded = false;
};

#define sNpcSpawner NpcSpawner::instance()
