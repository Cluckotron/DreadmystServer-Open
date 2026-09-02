#include "stdafx.h"
#include "Map.h"
#include "../Core/Logger.h"

#include <fstream>
#include <cmath>

// Constants matching client's GameMap::Defines
namespace MapDefines
{
    constexpr int NumLayers = 3;
    constexpr int BaseCellWidth = 64;
    constexpr int BaseCellHeight = 32;
}

bool Map::load(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        LOG_ERROR("Map: Failed to open file: {}", filepath);
        return false;
    }

    // Extract map name from filepath (e.g., "maps/fanadin.map" -> "fanadin")
    size_t lastSlash = filepath.find_last_of("/\\");
    size_t lastDot = filepath.find_last_of('.');
    if (lastSlash == std::string::npos)
        lastSlash = 0;
    else
        lastSlash++;

    if (lastDot != std::string::npos && lastDot > lastSlash)
        m_name = filepath.substr(lastSlash, lastDot - lastSlash);
    else
        m_name = filepath.substr(lastSlash);

    // Helper to read little-endian int32
    auto readInt32 = [&file]() -> int32_t {
        int32_t value = 0;
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
    };

    // Helper to read string (length-prefixed with null terminator in file)
    auto readString = [&file]() -> std::string {
        std::string result;
        char c;
        while (file.get(c) && c != '\0')
            result += c;
        return result;
    };

    // 1. Read map width
    m_width = readInt32();
    if (m_width <= 0 || m_width > 10000)
    {
        LOG_ERROR("Map: Invalid map width {} in {}", m_width, filepath);
        return false;
    }

    // Initialize cells array (all cells default to walkable)
    m_cells.clear();
    m_cells.resize(m_width * m_width);

    // 2. Read cell texture count (skip textures, server doesn't need them)
    int32_t numTextures = readInt32();
    for (int i = 0; i < numTextures; ++i)
        readString();  // Skip texture name

    // 3. Read cells with data
    int32_t numCells = readInt32();
    LOG_DEBUG("Map: Loading {} - width={}, cells={}", m_name, m_width, numCells);

    for (int i = 0; i < numCells; ++i)
    {
        int32_t cellId = readInt32();
        uint8_t flags = 0;
        file.read(reinterpret_cast<char*>(&flags), 1);

        // Store flags if valid cell
        if (cellId >= 0 && cellId < static_cast<int32_t>(m_cells.size()))
            m_cells[cellId].flags = flags;

        // Skip layer texture data (3 layers in the released Dreadmyst .map format)
        for (int layer = 0; layer < MapDefines::NumLayers; ++layer)
        {
            bool hasTexture = false;
            file.read(reinterpret_cast<char*>(&hasTexture), 1);

            if (hasTexture)
            {
                int32_t textureIdx = readInt32();  // Skip texture index
                float scale = 0.0f;
                file.read(reinterpret_cast<char*>(&scale), sizeof(float));  // Skip scale
                (void)textureIdx;
                (void)scale;
            }
        }
    }

    // 4. Read terrain data (skip, server doesn't need it)
    int32_t numTerrainTextures = readInt32();

    if (numTerrainTextures > 0)
    {
        // Skip terrain texture names
        for (int i = 0; i < numTerrainTextures; ++i)
            readString();

        // Skip terrain mappings
        int32_t numTerrains = readInt32();
        for (int i = 0; i < numTerrains; ++i)
        {
            readInt32();  // terrainId
            readInt32();  // textureIdx
        }
    }

    // 5. Skip zone data
    int32_t numZones = readInt32();
    for (int i = 0; i < numZones; ++i)
    {
        readInt32();  // terrainId
        readInt32();  // zoneId
    }

    // 6. Skip area data
    int32_t numAreas = readInt32();
    for (int i = 0; i < numAreas; ++i)
    {
        readInt32();  // terrainId
        readInt32();  // areaId
    }

    LOG_INFO("Map: Loaded '%s' (%dx%d, %zu cells with flags)",
             m_name.c_str(), m_width, m_width, numCells);

    return true;
}

const MapCell* Map::getCell(int cellId) const
{
    if (cellId < 0 || cellId >= static_cast<int>(m_cells.size()))
        return nullptr;
    return &m_cells[cellId];
}

const MapCell* Map::getCell(int x, int y) const
{
    return getCell(cellIdFromCoords(x, y));
}

bool Map::isWalkable(int cellId) const
{
    const MapCell* cell = getCell(cellId);
    if (!cell)
        return false;  // Out of bounds = not walkable
    return (cell->flags & CellFlags::Unwalkable) == 0;
}

bool Map::isWalkable(int x, int y) const
{
    return isWalkable(cellIdFromCoords(x, y));
}

bool Map::blocksLineOfSight(int cellId) const
{
    const MapCell* cell = getCell(cellId);
    if (!cell)
        return true;  // Out of bounds = blocks
    return (cell->flags & CellFlags::CollideBlock) != 0;
}

bool Map::blocksLineOfSight(int x, int y) const
{
    return blocksLineOfSight(cellIdFromCoords(x, y));
}

int Map::cellIdFromCoords(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_width)
        return -1;
    return y * m_width + x;
}

void Map::coordsFromCellId(int cellId, int& x, int& y) const
{
    if (cellId < 0 || cellId >= m_width * m_width)
    {
        x = y = -1;
        return;
    }
    x = cellId % m_width;
    y = cellId / m_width;
}

int Map::cellIdFromWorldPos(float worldX, float worldY) const
{
    // Dreadmyst world coordinates are logical map-cell coordinates.
    // BaseCellWidth/BaseCellHeight are *rendering* dimensions used only by the
    // isometric projection on the client; dividing here incorrectly maps e.g.
    // world (100,100) to cell (1,3).
    int cellX = static_cast<int>(std::floor(worldX));
    int cellY = static_cast<int>(std::floor(worldY));
    return cellIdFromCoords(cellX, cellY);
}
