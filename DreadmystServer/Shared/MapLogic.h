#pragma once

#include <vector>
#include <queue>
#include <cmath>
#include "MapCellT.h"
#include "../Geo2d.h"

// 2D map/pathfinding utilities
namespace MapLogic
{
    // Distance calculation
    inline float distance(float x1, float y1, float x2, float y2)
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Check if point is within radius
    inline bool inRange(float x1, float y1, float x2, float y2, float radius)
    {
        return distance(x1, y1, x2, y2) <= radius;
    }

    // Calculate orientation angle between two points
    inline float orientation(float fromX, float fromY, float toX, float toY)
    {
        return std::atan2(toY - fromY, toX - fromX);
    }

    // Waypoint structure for movement splines
    struct Waypoint
    {
        float x, y;
    };

    // A* pathfinding result
    using Path = std::vector<Waypoint>;

    // Get all cell indexes in a square area around a center cell
    // Used for render culling and area-of-interest calculations
    inline void getIndexesAround(std::vector<int>& output, int centerIndex, int mapWidth, int radius)
    {
        if (mapWidth <= 0) return;

        int centerX = centerIndex % mapWidth;
        int centerY = centerIndex / mapWidth;

        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                int x = centerX + dx;
                int y = centerY + dy;

                if (x >= 0 && y >= 0 && x < mapWidth)
                {
                    int index = y * mapWidth + x;
                    if (index >= 0)
                        output.push_back(index);
                }
            }
        }
    }

    // Line-of-sight check across map cells (blocked by flag)
    template<typename MapT>
    inline bool checkLosToC(const MapT& map, const Geo2d::Vector2& from, const Geo2d::Vector2& to, MapCellT::Flags blockedFlag)
    {
        const int width = map.getMapWidth();
        const int height = map.getMapHeight();
        if (width <= 0 || height <= 0) return false;

        int x0 = static_cast<int>(std::floor(from.x));
        int y0 = static_cast<int>(std::floor(from.y));
        int x1 = static_cast<int>(std::floor(to.x));
        int y1 = static_cast<int>(std::floor(to.y));

        auto cellBlocked = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= width || y >= height) return true;
            const int cellId = y * width + x;
            auto cell = map.getCell(cellId);
            if (!cell) return true;
            return cell->hasFlag(blockedFlag);
        };

        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
            if (cellBlocked(x0, y0))
                return false;
            if (x0 == x1 && y0 == y1)
                break;

            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }

        return true;
    }

    // Build a simple grid path (4-way) between points; empty if unreachable
    template<typename MapT>
    inline void constructPathTo(const MapT& map, std::vector<Geo2d::Vector2>& outPath,
                                const Geo2d::Vector2& from, const Geo2d::Vector2& to,
                                MapCellT::Flags blockedFlag = MapCellT::Unwalkable)
    {
        outPath.clear();

        const int width = map.getMapWidth();
        const int height = map.getMapHeight();
        if (width <= 0 || height <= 0) return;

        const int startX = static_cast<int>(std::floor(from.x));
        const int startY = static_cast<int>(std::floor(from.y));
        const int endX = static_cast<int>(std::floor(to.x));
        const int endY = static_cast<int>(std::floor(to.y));

        auto inBounds = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < width && y < height;
        };

        auto cellBlocked = [&](int x, int y) {
            if (!inBounds(x, y)) return true;
            const int cellId = y * width + x;
            auto cell = map.getCell(cellId);
            if (!cell) return true;
            return cell->hasFlag(blockedFlag);
        };

        if (!inBounds(startX, startY) || !inBounds(endX, endY)) return;
        if (cellBlocked(endX, endY)) return;

        const int startId = startY * width + startX;
        const int endId = endY * width + endX;

        std::vector<int> parent(static_cast<size_t>(width * height), -1);
        std::queue<int> open;
        parent[startId] = startId;
        open.push(startId);

        const int dirX[4] = {1, -1, 0, 0};
        const int dirY[4] = {0, 0, 1, -1};

        bool found = false;
        while (!open.empty())
        {
            const int current = open.front();
            open.pop();
            if (current == endId) { found = true; break; }

            const int cx = current % width;
            const int cy = current / width;

            for (int d = 0; d < 4; ++d)
            {
                const int nx = cx + dirX[d];
                const int ny = cy + dirY[d];
                if (!inBounds(nx, ny)) continue;
                if (cellBlocked(nx, ny)) continue;

                const int nid = ny * width + nx;
                if (parent[nid] != -1) continue;

                parent[nid] = current;
                open.push(nid);
            }
        }

        if (!found) return;

        std::vector<Geo2d::Vector2> reversePath;
        int current = endId;
        while (current != startId)
        {
            const int cx = current % width;
            const int cy = current / width;
            reversePath.push_back(Geo2d::Vector2(static_cast<float>(cx), static_cast<float>(cy)));
            current = parent[current];
            if (current < 0) return;
        }

        outPath.assign(reversePath.rbegin(), reversePath.rend());
    }

    // Total path length from start to each path node
    inline float getPathLength(const Geo2d::Vector2& start, const std::vector<Geo2d::Vector2>& path)
    {
        float total = 0.0f;
        Geo2d::Vector2 prev = start;
        for (const auto& point : path) {
            total += prev.getDist(point);
            prev = point;
        }
        return total;
    }
}
