// StringHelpers.h - String and general utility functions for Dreadmyst client
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstdarg>
#include <sstream>
#include <algorithm>
#include <random>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Graphics/Color.hpp>

namespace Util
{

// ============================================================================
// String Functions
// ============================================================================

inline std::string fmtStr(const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return std::string(buffer);
}

inline std::vector<std::string> splitStr(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter))
    {
        result.push_back(item);
    }
    return result;
}

inline std::string trimStr(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return str.substr(start, end - start + 1);
}

inline std::string toLowerCase(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

inline std::string strReplaceAll(std::string str, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos)
    {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

inline std::wstring toUtf(const std::string& str)
{
    return std::wstring(str.begin(), str.end());
}

inline std::string toRoman(int num)
{
    static const std::pair<int, const char*> romans[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}
    };
    std::string result;
    for (const auto& [value, symbol] : romans)
    {
        while (num >= value)
        {
            result += symbol;
            num -= value;
        }
    }
    return result;
}

inline bool compareNaturalSort(const std::string& a, const std::string& b)
{
    return a < b;  // Simplified natural sort
}

// ============================================================================
// Number Formatting
// ============================================================================

inline std::string formatMoneyCommas(int64_t value)
{
    std::string str = std::to_string(value);
    int insertPos = static_cast<int>(str.length()) - 3;
    while (insertPos > 0)
    {
        str.insert(insertPos, ",");
        insertPos -= 3;
    }
    return str;
}

inline std::string formatTimeBySeconds(int seconds)
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (hours > 0)
        return fmtStr("%d:%02d:%02d", hours, minutes, secs);
    else
        return fmtStr("%d:%02d", minutes, secs);
}

inline int parseIntExpression(const std::string& expr)
{
    // Simple integer parsing, could be extended for expressions
    try { return std::stoi(expr); }
    catch (...) { return 0; }
}

// ============================================================================
// Random Functions
// ============================================================================

#ifndef UTIL_IRAND_DEFINED
#define UTIL_IRAND_DEFINED
inline int irand(int min, int max)
{
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}
#endif

inline float frand(float min, float max)
{
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

inline bool roll_chance_i(int chance)
{
    return irand(0, 99) < chance;
}

template<typename T>
inline const T& randomChoice(const std::vector<T>& vec)
{
    return vec[irand(0, static_cast<int>(vec.size()) - 1)];
}

// Variadic version - accepts multiple arguments directly
// Usage: randomChoice("a", "b", "c") or randomChoice(1, 2, 3)
template<typename T, typename... Args>
inline T randomChoice(T first, Args... args)
{
    std::vector<T> options = {first, args...};
    return options[irand(0, static_cast<int>(options.size()) - 1)];
}

// ============================================================================
// Bit Manipulation
// ============================================================================

template<typename T>
inline bool maskHas(T mask, T flag)
{
    return (mask & flag) == flag;
}

// Overload for int mask with enum class flag
template<typename EnumT>
inline bool maskHas(int mask, EnumT flag)
{
    return (mask & static_cast<int>(flag)) == static_cast<int>(flag);
}

// Overload for 64-bit masks with enum class flag
template<typename EnumT>
inline bool maskHas(uint64_t mask, EnumT flag)
{
    return (mask & static_cast<uint64_t>(flag)) == static_cast<uint64_t>(flag);
}

// ============================================================================
// Color Functions
// ============================================================================

inline sf::Color brightenColor(const sf::Color& color, float factor)
{
    return sf::Color(
        std::min(255, static_cast<int>(color.r * factor)),
        std::min(255, static_cast<int>(color.g * factor)),
        std::min(255, static_cast<int>(color.b * factor)),
        color.a
    );
}

// ============================================================================
// Keyboard
// ============================================================================

// Only define if SfExtensions.h hasn't already provided this function
#ifndef UTIL_SFKEYTOSTRING_DEFINED
inline std::string sfKeyToString(sf::Keyboard::Key key)
{
    static const char* keyNames[] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "Escape", "LCtrl", "LShift", "LAlt", "LSystem",
        "RCtrl", "RShift", "RAlt", "RSystem", "Menu",
        "[", "]", ";", ",", ".", "'", "/", "\\", "~", "=", "-",
        "Space", "Enter", "Backspace", "Tab",
        "PageUp", "PageDown", "End", "Home", "Insert", "Delete",
        "NumpadAdd", "NumpadSub", "NumpadMul", "NumpadDiv",
        "Left", "Right", "Up", "Down",
        "Numpad0", "Numpad1", "Numpad2", "Numpad3", "Numpad4",
        "Numpad5", "Numpad6", "Numpad7", "Numpad8", "Numpad9",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
        "F9", "F10", "F11", "F12", "F13", "F14", "F15",
        "Pause"
    };

    int k = static_cast<int>(key);
    if (k >= 0 && k < static_cast<int>(sizeof(keyNames) / sizeof(keyNames[0])))
        return keyNames[k];
    return "Unknown";
}
#endif // UTIL_SFKEYTOSTRING_DEFINED

// ============================================================================
// Encoding
// ============================================================================

inline std::string base64_encode(const std::string& input)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((input.size() + 2) / 3 * 4);

    for (size_t i = 0; i < input.size(); i += 3)
    {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size()) n |= static_cast<uint8_t>(input[i + 2]);

        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += (i + 1 < input.size()) ? table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < input.size()) ? table[n & 0x3F] : '=';
    }
    return result;
}

// ============================================================================
// Windows Input Simulation (stub for non-Windows)
// ============================================================================

#ifdef _WIN32
void simulateInputBox(const std::string& text);
#else
inline void simulateInputBox(const std::string&) {}
#endif

} // namespace Util
