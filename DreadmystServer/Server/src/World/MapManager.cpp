#include "stdafx.h"
#include "MapManager.h"
#include "Map.h"
#include "../Core/Logger.h"
#include "../Database/GameData.h"
#include "NpcSpawner.h"

MapManager& MapManager::instance()
{
    static MapManager instance;
    return instance;
}

bool MapManager::initialize(const std::string& mapsDirectory)
{
    m_mapsDirectory = mapsDirectory;

    // Ensure trailing slash
    if (!m_mapsDirectory.empty() && m_mapsDirectory.back() != '/' && m_mapsDirectory.back() != '\\')
        m_mapsDirectory += '/';

    LOG_INFO("MapManager: Initialized with maps directory: %s", m_mapsDirectory.c_str());
    LOG_INFO("MapManager: %zu map templates available from GameData", sGameData.getMapCount());

    return true;
}

void MapManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    LOG_INFO("MapManager: Shutting down, unloading %zu maps", m_loadedMaps.size());
    m_loadedMaps.clear();
}

Map* MapManager::getMap(int mapId)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    // Check if already loaded
    auto it = m_loadedMaps.find(mapId);
    if (it != m_loadedMaps.end())
        return it->second.get();

    // Load the map
    auto map = loadMapFromFile(mapId);
    if (!map)
        return nullptr;

    Map* result = map.get();
    m_loadedMaps[mapId] = std::move(map);

    sNpcSpawner.loadSpawnsForMap(mapId);

    return result;
}

const MapTemplate* MapManager::getMapTemplate(int mapId) const
{
    return sGameData.getMap(mapId);
}

bool MapManager::isMapLoaded(int mapId) const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    return m_loadedMaps.find(mapId) != m_loadedMaps.end();
}

void MapManager::preloadMaps(const std::vector<int>& mapIds)
{
    LOG_INFO("MapManager: Preloading %zu maps...", mapIds.size());

    for (int mapId : mapIds)
    {
        if (!isMapLoaded(mapId))
        {
            Map* map = getMap(mapId);
            if (map)
                LOG_INFO("MapManager: Preloaded map %d (%s)", mapId, map->getName().c_str());
            else
                LOG_WARN("MapManager: Failed to preload map %d", mapId);
        }
    }
}

void MapManager::unloadMap(int mapId)
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    auto it = m_loadedMaps.find(mapId);
    if (it != m_loadedMaps.end())
    {
        LOG_INFO("MapManager: Unloading map %d (%s)", mapId, it->second->getName().c_str());
        m_loadedMaps.erase(it);
    }
}

bool MapManager::getStartPosition(int mapId, float& x, float& y, float& orientation) const
{
    const MapTemplate* tmpl = getMapTemplate(mapId);
    if (!tmpl)
        return false;

    x = static_cast<float>(tmpl->startX);
    y = static_cast<float>(tmpl->startY);
    orientation = static_cast<float>(tmpl->startO);
    return true;
}

std::vector<int> MapManager::getLoadedMapIds() const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);

    std::vector<int> ids;
    ids.reserve(m_loadedMaps.size());
    for (const auto& pair : m_loadedMaps)
        ids.push_back(pair.first);
    return ids;
}

void MapManager::update(float deltaTime)
{
    (void)deltaTime;
    // Future: update map-specific logic like respawns, events, etc.
}

std::unique_ptr<Map> MapManager::loadMapFromFile(int mapId)
{
    const MapTemplate* tmpl = getMapTemplate(mapId);
    if (!tmpl)
    {
        LOG_ERROR("MapManager: No template found for map ID {}", mapId);
        return nullptr;
    }

    std::string filepath = m_mapsDirectory + tmpl->name + ".map";

    auto map = std::make_unique<Map>();
    map->setMapId(mapId);

    if (!map->load(filepath))
    {
        LOG_ERROR("MapManager: Failed to load map file: {}", filepath);
        return nullptr;
    }

    return map;
}
