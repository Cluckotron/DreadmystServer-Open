// NpcSpawner - Loads and spawns NPCs from game.db
// Task 7.1: NPC Spawner

#include "stdafx.h"
#include "NpcSpawner.h"
#include "../Core/Config.h"
#include "../Core/Logger.h"
#include "../Database/GameData.h"
#include "WorldManager.h"

#include <sqlite3.h>

NpcSpawner& NpcSpawner::instance()
{
    static NpcSpawner instance;
    return instance;
}

void NpcSpawner::loadGroups()
{
    if (m_groupsLoaded)
        return;

    m_groupsLoaded = true;

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(sConfig.getGameDbPath().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to open game database for groups: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    const char* sql = "SELECT guid_leader, guid_member, distance, angle, linked_respawn, linked_loot FROM npc_groups";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to prepare npc_groups query: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int32_t leader = sqlite3_column_int(stmt, 0);
        NpcGroupEntry entry;
        entry.memberSpawnId = sqlite3_column_int(stmt, 1);
        entry.distance = static_cast<float>(sqlite3_column_double(stmt, 2));
        entry.angle = static_cast<float>(sqlite3_column_double(stmt, 3));
        entry.linkedRespawn = sqlite3_column_int(stmt, 4) != 0;
        entry.linkedLoot = sqlite3_column_int(stmt, 5) != 0;

        m_groupsByLeader[leader].push_back(entry);
        m_spawnToGroupLeader[entry.memberSpawnId] = leader;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void NpcSpawner::loadWaypoints(int32_t pathId)
{
    if (pathId <= 0 || m_loadedPathIds.count(pathId) > 0)
        return;

    m_loadedPathIds.insert(pathId);

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(sConfig.getGameDbPath().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to open game database for waypoints: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    const char* sql = "SELECT point, position_x, position_y, orientation, run, wait_time "
                      "FROM npc_waypoints WHERE id = ? ORDER BY point ASC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to prepare npc_waypoints query: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_int(stmt, 1, pathId);

    std::vector<NpcWaypoint> waypoints;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        NpcWaypoint wp;
        wp.x = static_cast<float>(sqlite3_column_double(stmt, 1));
        wp.y = static_cast<float>(sqlite3_column_double(stmt, 2));
        wp.orientation = static_cast<float>(sqlite3_column_double(stmt, 3));
        wp.run = sqlite3_column_int(stmt, 4) != 0;
        wp.waitTimeMs = sqlite3_column_int(stmt, 5);
        waypoints.push_back(wp);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (!waypoints.empty())
    {
        m_waypointsByPathId[pathId] = std::move(waypoints);
        LOG_DEBUG("NpcSpawner: Loaded {} waypoints for path {}", m_waypointsByPathId[pathId].size(), pathId);
    }
}

void NpcSpawner::loadSpawnsForMap(int mapId)
{
    if (m_loadedMaps.count(mapId) > 0)
        return;

    m_loadedMaps.insert(mapId);
    loadGroups();

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(sConfig.getGameDbPath().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to open game database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    const char* sql = "SELECT guid, entry, map, position_x, position_y, orientation, path_id, "
                      "respawn_time, movement_type, wander_distance, call_for_help "
                      "FROM npc WHERE map = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("NpcSpawner: Failed to prepare npc spawn query: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_int(stmt, 1, mapId);

    int loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        NpcSpawnInfo info;
        info.spawnId = sqlite3_column_int(stmt, 0);
        info.npcEntry = sqlite3_column_int(stmt, 1);
        info.mapId = sqlite3_column_int(stmt, 2);
        info.x = static_cast<float>(sqlite3_column_double(stmt, 3));
        info.y = static_cast<float>(sqlite3_column_double(stmt, 4));
        info.orientation = static_cast<float>(sqlite3_column_double(stmt, 5));
        info.pathId = sqlite3_column_int(stmt, 6);
        info.respawnSeconds = sqlite3_column_int(stmt, 7);
        info.movementType = sqlite3_column_int(stmt, 8);
        info.wanderDistance = static_cast<float>(sqlite3_column_double(stmt, 9));
        info.callForHelp = sqlite3_column_int(stmt, 10) != 0;

        if (info.respawnSeconds <= 0)
            info.respawnSeconds = 60;

        m_spawns[info.spawnId] = info;
        m_spawnsByMap[mapId].push_back(info.spawnId);
        ++loaded;

        if (info.pathId > 0)
            loadWaypoints(info.pathId);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    LOG_INFO("NpcSpawner: Loaded %d spawns for map %d", loaded, mapId);
    spawnAllForMap(mapId);
}

void NpcSpawner::spawnAllForMap(int mapId)
{
    auto it = m_spawnsByMap.find(mapId);
    if (it == m_spawnsByMap.end())
        return;

    for (int32_t spawnId : it->second)
    {
        if (m_spawnToNpcGuid.count(spawnId) > 0)
            continue;

        auto spawnIt = m_spawns.find(spawnId);
        if (spawnIt == m_spawns.end())
            continue;

        const NpcSpawnInfo& spawn = spawnIt->second;
        const NpcTemplate* tmpl = sGameData.getNpc(spawn.npcEntry);
        if (!tmpl)
        {
            LOG_WARN("NpcSpawner: Missing NPC template entry {}", spawn.npcEntry);
            continue;
        }

        int32_t movementType = spawn.movementType != 0 ? spawn.movementType : tmpl->movementType;
        int32_t pathId = spawn.pathId != 0 ? spawn.pathId : tmpl->pathId;

        if (pathId > 0)
            loadWaypoints(pathId);

        Npc* npc = sWorldManager.spawnNpc(*tmpl, spawn.mapId, spawn.x, spawn.y, spawn.orientation);
        if (!npc)
            continue;

        npc->setSpawnId(spawn.spawnId);
        npc->setRespawnTimeMs(spawn.respawnSeconds * 1000);
        npc->setMovementType(movementType);
        npc->setPathId(pathId);
        npc->setWanderDistance(spawn.wanderDistance);
        npc->setCallForHelp(spawn.callForHelp);
        npc->setCalledForHelp(false);

        npc->getWaypoints().clear();
        auto waypointsIt = m_waypointsByPathId.find(pathId);
        if (waypointsIt != m_waypointsByPathId.end())
            npc->getWaypoints() = waypointsIt->second;

        m_spawnToNpcGuid[spawn.spawnId] = npc->getGuid();
        sWorldManager.broadcastNpcSpawn(npc);
    }
}

void NpcSpawner::onNpcDeath(Npc* npc)
{
    if (!npc)
        return;

    int32_t spawnId = npc->getSpawnId();
    if (spawnId <= 0)
        return;

    auto spawnIt = m_spawns.find(spawnId);
    if (spawnIt == m_spawns.end())
        return;

    int32_t respawnSeconds = spawnIt->second.respawnSeconds;
    if (respawnSeconds <= 0)
        return;

    // Linked respawn group handling
    int32_t leader = spawnId;
    auto groupIt = m_spawnToGroupLeader.find(spawnId);
    if (groupIt != m_spawnToGroupLeader.end())
        leader = groupIt->second;

    auto membersIt = m_groupsByLeader.find(leader);
    bool linkedRespawn = false;
    if (membersIt != m_groupsByLeader.end())
    {
        for (const auto& entry : membersIt->second)
        {
            if (entry.linkedRespawn)
            {
                linkedRespawn = true;
                m_respawnTimers[entry.memberSpawnId] = static_cast<float>(respawnSeconds);
            }
        }
    }

    m_respawnTimers[spawnId] = static_cast<float>(respawnSeconds);

    if (linkedRespawn)
        m_respawnTimers[leader] = static_cast<float>(respawnSeconds);
}

void NpcSpawner::respawnSpawn(int32_t spawnId)
{
    auto spawnIt = m_spawns.find(spawnId);
    if (spawnIt == m_spawns.end())
        return;

    Npc* npc = nullptr;
    auto guidIt = m_spawnToNpcGuid.find(spawnId);
    if (guidIt != m_spawnToNpcGuid.end())
        npc = sWorldManager.getNpc(guidIt->second);

    if (!npc)
    {
        const NpcSpawnInfo& spawn = spawnIt->second;
        const NpcTemplate* tmpl = sGameData.getNpc(spawn.npcEntry);
        if (!tmpl)
            return;

        int32_t movementType = spawn.movementType != 0 ? spawn.movementType : tmpl->movementType;
        int32_t pathId = spawn.pathId != 0 ? spawn.pathId : tmpl->pathId;

        if (pathId > 0)
            loadWaypoints(pathId);

        npc = sWorldManager.spawnNpc(*tmpl, spawn.mapId, spawn.x, spawn.y, spawn.orientation);
        if (!npc)
            return;

        npc->setSpawnId(spawn.spawnId);
        npc->setRespawnTimeMs(spawn.respawnSeconds * 1000);
        npc->setMovementType(movementType);
        npc->setPathId(pathId);
        npc->setWanderDistance(spawn.wanderDistance);
        npc->setCallForHelp(spawn.callForHelp);
        npc->setCalledForHelp(false);

        npc->getWaypoints().clear();
        auto waypointsIt = m_waypointsByPathId.find(pathId);
        if (waypointsIt != m_waypointsByPathId.end())
            npc->getWaypoints() = waypointsIt->second;

        m_spawnToNpcGuid[spawn.spawnId] = npc->getGuid();
    }
    else
    {
        npc->respawn();
        npc->setCalledForHelp(false);
    }

    sWorldManager.broadcastNpcSpawn(npc);
}

void NpcSpawner::update(float deltaTime)
{
    if (m_respawnTimers.empty())
        return;

    std::vector<int32_t> toRespawn;
    toRespawn.reserve(m_respawnTimers.size());

    for (auto& [spawnId, timer] : m_respawnTimers)
    {
        timer -= deltaTime;
        if (timer <= 0.0f)
            toRespawn.push_back(spawnId);
    }

    for (int32_t spawnId : toRespawn)
    {
        respawnSpawn(spawnId);
        m_respawnTimers.erase(spawnId);
    }
}
