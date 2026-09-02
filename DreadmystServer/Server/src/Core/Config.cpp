// Server Configuration System
// INI file parser for server settings

#include "stdafx.h"
#include "Core/Config.h"
#include "Core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

Config& Config::instance()
{
    static Config instance;
    return instance;
}

// Helper: Trim whitespace from both ends of a string
static std::string trim(const std::string& str)
{
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool Config::load(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARN("Could not open config file: %s", filename.c_str());
        return false;
    }

    std::string currentSection;
    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header: [SectionName]
        if (line[0] == '[') {
            auto endBracket = line.find(']');
            if (endBracket == std::string::npos) {
                LOG_WARN("Malformed section at line %d: %s", lineNum, line.c_str());
                continue;
            }
            currentSection = line.substr(1, endBracket - 1);
            continue;
        }

        // Key=Value pair
        auto equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            LOG_WARN("Malformed line %d (no '='): %s", lineNum, line.c_str());
            continue;
        }

        std::string key = trim(line.substr(0, equalPos));
        std::string value = trim(line.substr(equalPos + 1));

        // Remove quotes from value if present
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // Apply setting based on section and key
        if (currentSection == "Server") {
            if (key == "Port") {
                m_serverPort = static_cast<uint16_t>(std::stoi(value));
            } else if (key == "MaxConnections") {
                m_maxConnections = std::stoi(value);
            }
        }
        else if (currentSection == "Database") {
            if (key == "GameDbPath") {
                m_gameDbPath = value;
            } else if (key == "MapsPath") {
                m_mapsPath = value;
            } else if (key == "ServerDbPath") {
                m_serverDbPath = value;
            }
        }
        else if (currentSection == "Logging") {
            if (key == "Level") {
                m_logLevel = value;
                // Convert to lowercase for consistency
                std::transform(m_logLevel.begin(), m_logLevel.end(),
                               m_logLevel.begin(), ::tolower);
            }
        }
    }

    LOG_INFO("Loaded configuration from %s", filename.c_str());
    return true;
}
