// Math.h - Math utilities for Dreadmyst client
#pragma once

#include <cmath>
#include <algorithm>

namespace Math
{

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

inline float clamp(float value, float min, float max)
{
    return std::max(min, std::min(max, value));
}

inline int clamp(int value, int min, int max)
{
    return std::max(min, std::min(max, value));
}

inline float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

inline float distance(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline float angleTo(float x1, float y1, float x2, float y2)
{
    return std::atan2(y2 - y1, x2 - x1);
}

inline float normalizeAngle(float angle)
{
    while (angle < 0) angle += 2 * PI;
    while (angle >= 2 * PI) angle -= 2 * PI;
    return angle;
}

template<typename T>
inline T sign(T value)
{
    return (T(0) < value) - (value < T(0));
}

} // namespace Math
