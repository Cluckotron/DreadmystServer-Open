#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "../Geo2d.h"
#include "MapCellT.h"

// Reconstructed shared GameMap API required by the released r1189 client.
// The on-disk parser is bounds-checked and follows the shipped Dreadmyst .map
// format: 3 render layers and NUL-terminated texture names.
class GameMap
{
public:
    enum Defines
    {
        Layer1 = 0,
        Layer2 = 1,
        Layer3 = 2,
        NumLayers = 3,
        BaseCellWidth = 64,
        BaseCellHeight = 32,
        TerrainToCellRatio = 13,
    };

    explicit GameMap(const int id = 0) : m_id(id) {}
    virtual ~GameMap() = default;

    int getId() const { return m_id; }
    void setId(const int id) { m_id = id; }
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    int getMapWidth() const { return m_mapWidth; }
    int getMapHeight() const { return m_mapHeight; }
    int getTerrainWidth() const { return m_terrainWidth; }
    int getTerrainHeight() const { return m_terrainHeight; }
    std::size_t getNumCells() const { return m_cells.size(); }

    MapCellT* getCell(const int cellId) const
    {
        if (cellId < 0 || static_cast<std::size_t>(cellId) >= m_cells.size())
            return nullptr;
        return m_cells[static_cast<std::size_t>(cellId)].get();
    }

    std::shared_ptr<MapCellT> getCellRef(const int cellId) const
    {
        if (cellId < 0 || static_cast<std::size_t>(cellId) >= m_cells.size())
            return nullptr;
        return m_cells[static_cast<std::size_t>(cellId)];
    }

    static Geo2d::Vector2 computeRawScreenPosition(const Geo2d::Vector2& worldPos)
    {
        const float halfW = static_cast<float>(BaseCellWidth) / 2.0f;
        const float halfH = static_cast<float>(BaseCellHeight) / 2.0f;
        return Geo2d::Vector2((worldPos.x - worldPos.y) * halfW,
                              (worldPos.x + worldPos.y) * halfH);
    }

    Geo2d::Vector2 getCellRenderPos(const int cellId,
                                    const int* customIsleWidth = nullptr,
                                    const int* customCellW = nullptr,
                                    const int* customCellH = nullptr) const
    {
        const int w = customIsleWidth ? *customIsleWidth : std::max(m_mapWidth, 1);
        const int x = cellId % w;
        const int y = cellId / w;
        return getCellRenderPos(x, y, customCellW, customCellH);
    }

    Geo2d::Vector2 getCellRenderPos(const int cellX, const int cellY,
                                    const int* customCellW = nullptr,
                                    const int* customCellH = nullptr) const
    {
        const float halfW = static_cast<float>(customCellW ? *customCellW : BaseCellWidth) / 2.0f;
        const float halfH = static_cast<float>(customCellH ? *customCellH : BaseCellHeight) / 2.0f;
        return Geo2d::Vector2((cellX - cellY) * halfW, (cellX + cellY) * halfH);
    }

    Geo2d::Vector2 getCellRenderPosF(const float worldX, const float worldY) const
    {
        return computeRawScreenPosition(Geo2d::Vector2(worldX, worldY));
    }

    int getCellId(const Geo2d::Vector2& renderPos,
                  const int* customIsleWidth = nullptr,
                  const int* customCellW = nullptr,
                  const int* customCellH = nullptr) const
    {
        int x = 0, y = 0;
        getCellPosition(x, y, renderPos, customCellW, customCellH);
        const int w = customIsleWidth ? *customIsleWidth : std::max(m_mapWidth, 1);
        return y * w + x;
    }

    void getCellPosition(int& x, int& y, const Geo2d::Vector2& renderPos,
                         const int* customCellW = nullptr,
                         const int* customCellH = nullptr) const
    {
        const float halfW = static_cast<float>(customCellW ? *customCellW : BaseCellWidth) / 2.0f;
        const float halfH = static_cast<float>(customCellH ? *customCellH : BaseCellHeight) / 2.0f;
        const float fx = (renderPos.x / halfW + renderPos.y / halfH) / 2.0f;
        const float fy = (renderPos.y / halfH - renderPos.x / halfW) / 2.0f;
        // floor is important for negative screen coordinates near map edges.
        x = static_cast<int>(std::floor(fx));
        y = static_cast<int>(std::floor(fy));
    }

    Geo2d::Vector2 renderPosToWorldPos(const Geo2d::Vector2& renderPos,
                                       const int* customCellW = nullptr,
                                       const int* customCellH = nullptr) const
    {
        const float halfW = static_cast<float>(customCellW ? *customCellW : BaseCellWidth) / 2.0f;
        const float halfH = static_cast<float>(customCellH ? *customCellH : BaseCellHeight) / 2.0f;
        return Geo2d::Vector2((renderPos.x / halfW + renderPos.y / halfH) / 2.0f,
                              (renderPos.y / halfH - renderPos.x / halfW) / 2.0f);
    }

    int worldPosToTerrainId(const Geo2d::Vector2& worldPos) const
    {
        if (m_terrainWidth <= 0 || m_mapWidth <= 0)
            return 0;
        const int tx = static_cast<int>(worldPos.x * static_cast<float>(m_terrainWidth) / static_cast<float>(m_mapWidth));
        const int ty = static_cast<int>(worldPos.y * static_cast<float>(m_terrainHeight) / static_cast<float>(m_mapHeight));
        if (tx < 0 || ty < 0 || tx >= m_terrainWidth || ty >= m_terrainHeight)
            return 0;
        return ty * m_terrainWidth + tx;
    }

    virtual bool loadFromDisk(const std::string& name)
    {
#ifdef _WIN32
        const std::string path = "maps\\" + name + ".map";
#else
        const std::string path = "maps/" + name + ".map";
#endif
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            return false;

        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),
                                              std::istreambuf_iterator<char>());
        std::size_t offset = 0;
        bool ok = true;

        auto readU8 = [&]() -> std::uint8_t {
            if (offset + 1 > data.size()) { ok = false; return 0; }
            return data[offset++];
        };
        auto readU32 = [&]() -> std::uint32_t {
            if (offset + 4 > data.size()) { ok = false; return 0; }
            const std::uint32_t v = std::uint32_t(data[offset]) |
                (std::uint32_t(data[offset + 1]) << 8) |
                (std::uint32_t(data[offset + 2]) << 16) |
                (std::uint32_t(data[offset + 3]) << 24);
            offset += 4;
            return v;
        };
        auto readF32 = [&]() -> float {
            const std::uint32_t bits = readU32();
            float v = 0.0f;
            if (ok) std::memcpy(&v, &bits, sizeof(v));
            return v;
        };
        auto readString = [&]() -> std::string {
            const std::size_t begin = offset;
            while (offset < data.size() && data[offset] != 0) ++offset;
            if (offset >= data.size()) { ok = false; return {}; }
            std::string out(reinterpret_cast<const char*>(data.data() + begin), offset - begin);
            ++offset;
            return out;
        };

        const std::uint32_t width = readU32();
        if (!ok || width == 0 || width > 2000)
            return false;
        const std::uint32_t textureCount = readU32();
        if (!ok || textureCount > 1000000u)
            return false;

        std::vector<std::string> textures;
        textures.reserve(textureCount);
        for (std::uint32_t i = 0; i < textureCount && ok; ++i)
            textures.push_back(readString());
        if (!ok)
            return false;

        setName(name);
        startedLoading();
        m_mapWidth = static_cast<int>(width);
        m_mapHeight = static_cast<int>(width);
        m_terrainWidth = m_mapWidth / TerrainToCellRatio;
        m_terrainHeight = m_terrainWidth;
        m_cells.clear();
        m_cells.resize(static_cast<std::size_t>(m_mapWidth) * static_cast<std::size_t>(m_mapHeight));
        onResize();

        const std::uint32_t populatedCells = readU32();
        if (!ok || populatedCells > m_cells.size()) { finishedLoading(); return false; }
        for (std::uint32_t i = 0; i < populatedCells && ok; ++i)
        {
            const std::uint32_t cellId = readU32();
            const std::uint8_t flags = readU8();
            if (!ok || cellId >= m_cells.size()) { ok = false; break; }

            std::vector<std::shared_ptr<std::string>> layerTextures(NumLayers);
            std::vector<float> layerScale(NumLayers, 1.0f);
            for (int layer = 0; layer < NumLayers && ok; ++layer)
            {
                const bool present = readU8() != 0;
                if (present)
                {
                    const std::uint32_t textureIndex = readU32();
                    const float scale = readF32();
                    if (!ok || textureIndex >= textures.size()) { ok = false; break; }
                    layerTextures[static_cast<std::size_t>(layer)] =
                        std::make_shared<std::string>(textures[textureIndex]);
                    layerScale[static_cast<std::size_t>(layer)] = scale;
                }
            }
            if (ok)
                onCellDataLoaded(static_cast<int>(cellId), static_cast<int>(flags), layerTextures, layerScale);
        }
        if (!ok) { finishedLoading(); return false; }

        onFinishedLoadingCells();

        const std::uint32_t terrainTextureCount = readU32();
        if (!ok || terrainTextureCount > 100000u) { finishedLoading(); return false; }
        std::vector<std::string> terrainTextures;
        terrainTextures.reserve(terrainTextureCount);
        for (std::uint32_t i = 0; i < terrainTextureCount && ok; ++i)
            terrainTextures.push_back(readString());
        if (!ok) { finishedLoading(); return false; }

        if (terrainTextureCount > 0)
        {
            const std::uint32_t terrainCellCount = readU32();
            if (!ok) { finishedLoading(); return false; }
            const std::size_t terrainGridCount = static_cast<std::size_t>(m_terrainWidth) * static_cast<std::size_t>(m_terrainHeight);

            // Shipped maps store sparse overrides; palette[0] is the base ground.
            if (!terrainTextures[0].empty())
            {
                for (std::size_t i = 0; i < terrainGridCount; ++i)
                    onTerrainTextureLoaded(static_cast<int>(i), terrainTextures[0]);
            }

            for (std::uint32_t i = 0; i < terrainCellCount && ok; ++i)
            {
                const std::uint32_t terrainId = readU32();
                const std::uint32_t textureIndex = readU32();
                if (!ok || textureIndex >= terrainTextures.size()) { ok = false; break; }
                if (terrainId < terrainGridCount)
                    onTerrainTextureLoaded(static_cast<int>(terrainId), terrainTextures[textureIndex]);
            }
        }
        if (!ok) { finishedLoading(); return false; }

        const std::uint32_t zoneCount = readU32();
        for (std::uint32_t i = 0; i < zoneCount && ok; ++i)
        {
            const std::uint32_t terrainId = readU32();
            const std::uint32_t zoneId = readU32();
            if (ok) onTerrainZoneLoaded(static_cast<int>(terrainId), static_cast<int>(zoneId));
        }
        const std::uint32_t areaCount = readU32();
        for (std::uint32_t i = 0; i < areaCount && ok; ++i)
        {
            const std::uint32_t terrainId = readU32();
            const std::uint32_t areaId = readU32();
            if (ok) onTerrainAreaLoaded(static_cast<int>(terrainId), static_cast<int>(areaId));
        }

        finishedLoading();
        return ok && offset == data.size();
    }

    virtual void resize(const int size)
    {
        if (size <= 0)
            return;
        m_mapWidth = size;
        m_mapHeight = size;
        m_terrainWidth = size / TerrainToCellRatio;
        m_terrainHeight = m_terrainWidth;
        m_cells.clear();
        m_cells.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size));
        onResize();
    }

protected:
    virtual void startedLoading() {}
    virtual void finishedLoading() {}
    virtual void onFinishedLoadingCells() {}
    virtual void onResize() {}
    virtual void onTerrainTextureLoaded(const int, const std::string&) {}
    virtual void onTerrainZoneLoaded(const int, const int) {}
    virtual void onTerrainAreaLoaded(const int, const int) {}
    virtual void onCellDataLoaded(const int, const int,
                                  const std::vector<std::shared_ptr<std::string>>&,
                                  const std::vector<float>&) {}

    std::vector<std::shared_ptr<MapCellT>> m_cells;

private:
    int m_id{0};
    int m_mapWidth{0};
    int m_mapHeight{0};
    int m_terrainWidth{0};
    int m_terrainHeight{0};
    std::string m_name;
};
