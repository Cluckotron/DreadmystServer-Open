// Geo2d.h - 2D Geometry utilities for Dreadmyst client
#pragma once

#ifdef DREADMYST_HEADLESS_SERVER
namespace sf
{
template <typename T>
struct Vector2
{
    T x{};
    T y{};
    constexpr Vector2() = default;
    constexpr Vector2(T xValue, T yValue) : x(xValue), y(yValue) {}
    template <typename U>
    explicit constexpr Vector2(const Vector2<U>& other)
        : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

    constexpr Vector2 operator+(const Vector2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Vector2 operator-(const Vector2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr Vector2& operator+=(const Vector2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    constexpr Vector2& operator-=(const Vector2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr Vector2 operator*(T scalar) const { return {x * scalar, y * scalar}; }
};
using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;
}
#else
#include <SFML/System/Vector2.hpp>
#endif
#include <cmath>
#include <cstdlib>

// Geo2d namespace for 2D geometry types used by client
namespace Geo2d
{
    // Extended Vector2 class with additional methods expected by client code
    struct Vector2 : public sf::Vector2f
    {
        // Inherit constructors
        using sf::Vector2f::Vector2f;
        Vector2() : sf::Vector2f(0, 0) {}
        Vector2(float x, float y) : sf::Vector2f(x, y) {}
        Vector2(const sf::Vector2f& v) : sf::Vector2f(v) {}
        Vector2(const sf::Vector2i& v) : sf::Vector2f(static_cast<float>(v.x), static_cast<float>(v.y)) {}

        // In-place modification methods
        void add(const sf::Vector2f& other) { x += other.x; y += other.y; }
        void subtract(const sf::Vector2f& other) { x -= other.x; y -= other.y; }
        void multiply(float scalar) { x *= scalar; y *= scalar; }
        void divide(float scalar) { if (scalar != 0) { x /= scalar; y /= scalar; } }
        void floorSelf() { x = std::floor(x); y = std::floor(y); }
        void ceilSelf() { x = std::ceil(x); y = std::ceil(y); }

        // Length/magnitude
        float length() const { return std::sqrt(x * x + y * y); }
        float lengthSquared() const { return x * x + y * y; }

        // Normalize in-place
        void normalizeSelf()
        {
            float len = length();
            if (len > 0.0001f) { x /= len; y /= len; }
        }

        // Conversion to integer vector
        sf::Vector2i toInt() const { return sf::Vector2i(static_cast<int>(x), static_cast<int>(y)); }

        // Distance to another point
        float getDist(const Vector2& other) const
        {
            float dx = x - other.x;
            float dy = y - other.y;
            return std::sqrt(dx * dx + dy * dy);
        }
    };

    inline float distance(const Vector2& a, const Vector2& b)
    {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    inline Vector2 normalize(const Vector2& v)
    {
        float len = std::sqrt(v.x * v.x + v.y * v.y);
        if (len < 0.0001f) return {0, 0};
        return {v.x / len, v.y / len};
    }

    // Integer coordinate distance
    inline float distance2di(int x1, int y1, int x2, int y2)
    {
        float dx = static_cast<float>(x2 - x1);
        float dy = static_cast<float>(y2 - y1);
        return std::sqrt(dx * dx + dy * dy);
    }

    // Float coordinate distance (alias for common usage)
    inline float distance2d(float x1, float y1, float x2, float y2)
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    // Floor a vector's components (returns new vector)
    inline Vector2 floor(const Vector2& v)
    {
        return Vector2(std::floor(v.x), std::floor(v.y));
    }

    // Compute linear cell ID from world coordinates
    inline int computeCellId(int worldX, int worldY, int mapWidth)
    {
        if (mapWidth <= 0) return 0;
        return worldY * mapWidth + worldX;
    }

    // Overload for Vector2 input
    inline int computeCellId(const Vector2& pos, int mapWidth)
    {
        return computeCellId(static_cast<int>(pos.x), static_cast<int>(pos.y), mapWidth);
    }

    // Compute cell coordinates from linear cell ID
    inline void computeCellPos(int cellId, int& outX, int& outY, int mapWidth)
    {
        if (mapWidth <= 0) { outX = outY = 0; return; }
        outX = cellId % mapWidth;
        outY = cellId / mapWidth;
    }
}

namespace Util
{

struct GeoBox
{
    float left = 0, top = 0, right = 0, bottom = 0;

    GeoBox() = default;
    GeoBox(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

    float width() const { return right - left; }
    float height() const { return bottom - top; }

    bool contains(float x, float y) const
    {
        return x >= left && x <= right && y >= top && y <= bottom;
    }

    bool contains(const sf::Vector2f& p) const
    {
        return contains(p.x, p.y);
    }

    bool intersects(const GeoBox& other) const
    {
        return !(left > other.right || right < other.left ||
                 top > other.bottom || bottom < other.top);
    }
};

// Alias for compatibility with code using GeoBox2d
using GeoBox2d = GeoBox;

inline bool cordsInBox(float x, float y, const GeoBox& box)
{
    return box.contains(x, y);
}

inline bool cordsInBox(const sf::Vector2f& point, const GeoBox& box)
{
    return box.contains(point);
}

// Legacy overloads for code that passes individual components
inline bool cordsInBox(const sf::Vector2i& point, const sf::Vector2i& topLeft, int width, int height)
{
    return point.x >= topLeft.x && point.x < topLeft.x + width &&
           point.y >= topLeft.y && point.y < topLeft.y + height;
}

inline bool cordsInBox(const sf::Vector2f& point, const sf::Vector2f& topLeft, float width, float height)
{
    return point.x >= topLeft.x && point.x < topLeft.x + width &&
           point.y >= topLeft.y && point.y < topLeft.y + height;
}

inline float pointDistance(const sf::Vector2f& a, const sf::Vector2f& b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline sf::Vector2f normalize(const sf::Vector2f& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return {0, 0};
    return {v.x / len, v.y / len};
}

// Random number helper - only define if not already defined by StringHelpers.h
#ifndef UTIL_IRAND_DEFINED
#define UTIL_IRAND_DEFINED
inline int irand(int min, int max)
{
    return min + (std::rand() % (max - min + 1));
}
#endif

} // namespace Util
