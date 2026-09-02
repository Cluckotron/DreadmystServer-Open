#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

class Map;
struct MapTemplate;  // Forward declaration (defined in GameData.h)

// Manages all maps in the game world
class MapManager
{
public:
    static MapManager& instance();

    // Initialize - sets the maps directory
    bool initialize(const std::string& mapsDirectory);

    // Shutdown - unloads all maps
    void shutdown();

    // Get a map by ID (loads it if not already loaded)
    Map* getMap(int mapId);

    // Get map template (metadata) by ID - from GameData
    const MapTemplate* getMapTemplate(int mapId) const;

    // Check if a map is loaded
    bool isMapLoaded(int mapId) const;

    // Preload specific maps (e.g., starting zones)
    void preloadMaps(const std::vector<int>& mapIds);

    // Unload a map (if no players are on it)
    void unloadMap(int mapId);

    // Get default starting map ID
    int getDefaultStartMapId() const { return m_defaultStartMap; }

    // Get starting position for a map
    bool getStartPosition(int mapId, float& x, float& y, float& orientation) const;

    // Get all loaded map IDs
    std::vector<int> getLoadedMapIds() const;

    // Update all maps (called each tick)
    void update(float deltaTime);

private:
    MapManager() = default;
    ~MapManager() = default;
    MapManager(const MapManager&) = delete;
    MapManager& operator=(const MapManager&) = delete;

    // Load a single map from file
    std::unique_ptr<Map> loadMapFromFile(int mapId);

    std::string m_mapsDirectory;
    int m_defaultStartMap = 1;  // fanadin

    // Loaded maps indexed by ID
    std::unordered_map<int, std::unique_ptr<Map>> m_loadedMaps;

    // Thread safety for map loading
    mutable std::mutex m_mapMutex;
};

#define sMapManager MapManager::instance()
