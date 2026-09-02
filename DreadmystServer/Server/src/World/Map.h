#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Cell flags for collision/pathfinding (matches client MapCellT::Flags)
namespace CellFlags
{
    constexpr uint8_t None         = 0x00;
    constexpr uint8_t Unwalkable   = 0x10;  // Cannot walk through (released .map flag bit)
    constexpr uint8_t CollideBlock = 0x08;  // Blocks collision/line of sight (released .map flag bit)
}

// Server-side map cell - stores only what server needs
struct MapCell
{
    uint8_t flags = CellFlags::None;
};

// Server-side map data loaded from .map files
class Map
{
public:
    Map() = default;
    ~Map() = default;

    // Load map from binary .map file
    bool load(const std::string& filepath);

    // Map dimensions
    int getWidth() const { return m_width; }
    int getHeight() const { return m_width; }  // Maps are square
    int getNumCells() const { return m_width * m_width; }

    // Cell access by index
    const MapCell* getCell(int cellId) const;

    // Cell access by coordinates
    const MapCell* getCell(int x, int y) const;

    // Collision queries
    bool isWalkable(int cellId) const;
    bool isWalkable(int x, int y) const;
    bool blocksLineOfSight(int cellId) const;
    bool blocksLineOfSight(int x, int y) const;

    // Coordinate conversion
    int cellIdFromCoords(int x, int y) const;
    void coordsFromCellId(int cellId, int& x, int& y) const;

    // World position to cell
    int cellIdFromWorldPos(float worldX, float worldY) const;

    // Map name
    const std::string& getName() const { return m_name; }
    int getMapId() const { return m_mapId; }
    void setMapId(int id) { m_mapId = id; }

private:
    std::string m_name;
    int m_mapId = 0;
    int m_width = 0;
    std::vector<MapCell> m_cells;
};
