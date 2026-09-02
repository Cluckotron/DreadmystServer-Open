#pragma once
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

template <typename T>
inline bool vectorContains(const std::vector<T>& values, const T& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

template <typename T>
inline bool vectorRemove(std::vector<T>& values, const T& value)
{
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) return false;
    values.erase(it);
    return true;
}
