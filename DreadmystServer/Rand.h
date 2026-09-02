// Rand.h - Random number generation utilities
#pragma once

#include <random>
#include <cstdint>

namespace Rand
{

inline std::mt19937& getGenerator()
{
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

inline int32_t irand(int32_t min, int32_t max)
{
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int32_t> dist(min, max);
    return dist(getGenerator());
}

inline uint32_t urand(uint32_t min, uint32_t max)
{
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(getGenerator());
}

inline float frand(float min = 0.0f, float max = 1.0f)
{
    if (min > max) std::swap(min, max);
    std::uniform_real_distribution<float> dist(min, max);
    return dist(getGenerator());
}

inline bool rollChance(int percent)
{
    return irand(0, 99) < percent;
}

inline bool rollChance(float percent)
{
    return frand(0.0f, 100.0f) < percent;
}

} // namespace Rand
