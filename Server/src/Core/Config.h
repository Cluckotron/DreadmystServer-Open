// Server Configuration System
// TODO: Implement in Task 1.12

#pragma once

#include <string>
#include <cstdint>

class Config
{
public:
    static Config& instance();

    bool load(const std::string& filename);

    // Server settings
    uint16_t getServerPort() const { return m_serverPort; }
    int getMaxConnections() const { return m_maxConnections; }

    // Database paths
    const std::string& getGameDbPath() const { return m_gameDbPath; }
    const std::string& getServerDbPath() const { return m_serverDbPath; }
    const std::string& getMapsPath() const { return m_mapsPath; }

    // Logging
    const std::string& getLogLevel() const { return m_logLevel; }

private:
    Config() = default;

    uint16_t m_serverPort = 8080;
    int m_maxConnections = 100;
    std::string m_gameDbPath = "../game/game.db";
    std::string m_mapsPath = "../game/maps";
    std::string m_serverDbPath = "data/server.db";
    std::string m_logLevel = "info";
};

#define sConfig Config::instance()
