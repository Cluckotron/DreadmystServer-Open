// Files.h - File system utilities for Dreadmyst client
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Util
{

inline std::string readTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline std::vector<std::string> readLinesFromFile(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open())
        return lines;

    std::string line;
    while (std::getline(file, line))
    {
        lines.push_back(line);
    }
    return lines;
}

// Overload that takes output reference (legacy style)
inline void readLinesFromFile(const std::string& path, std::vector<std::string>& output)
{
    output = readLinesFromFile(path);
}

inline bool writeTextFile(const std::string& path, const std::string& content)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << content;
    return true;
}

inline std::vector<std::string> getFileList(const std::string& directory, const std::string& extension = "")
{
    std::vector<std::string> files;

    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                std::string filepath = entry.path().string();
                if (extension.empty() || entry.path().extension() == extension)
                {
                    files.push_back(filepath);
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Directory doesn't exist or can't be read
    }

    return files;
}

// Overload that takes output reference (legacy style)
inline void getFileList(const std::string& directory, std::vector<std::string>& output)
{
    output = getFileList(directory);
}

inline bool fileExists(const std::string& path)
{
    return std::filesystem::exists(path);
}

inline bool directoryExists(const std::string& path)
{
    return std::filesystem::is_directory(path);
}

inline bool createDirectory(const std::string& path)
{
    return std::filesystem::create_directories(path);
}

} // namespace Util
