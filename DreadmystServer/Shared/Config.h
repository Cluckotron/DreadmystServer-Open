#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>

// INI configuration file parser
// Used for game/config.ini and client configuration

class Config
{
public:
    static Config* instance()
    {
        static Config inst;
        return &inst;
    }

    Config() = default;
    explicit Config(const std::string& filepath) { load(filepath); }

    // Set the config source file(s)
    // isLocal: if true, config is in local directory; if false, it's in a system directory (e.g., AppData)
    void setSource(const char* filepath, bool isLocal = true)
    {
        m_filepath = filepath;
        m_isLocal = isLocal;
        load(filepath);
    }

    bool load(const std::string& filepath);
    bool save() { return save(m_filepath); }
    bool save(const std::string& filepath);

    // Get values with default fallback
    std::string getString(const std::string& section, const std::string& key, const std::string& defaultVal = "") const;
    int getInt(const std::string& section, const std::string& key, int defaultVal = 0) const;
    float getFloat(const std::string& section, const std::string& key, float defaultVal = 0.0f) const;
    bool getBool(const std::string& section, const std::string& key, bool defaultVal = false) const;

    // Set values
    void setString(const std::string& section, const std::string& key, const std::string& value);
    void setInt(const std::string& section, const std::string& key, int value);
    void setFloat(const std::string& section, const std::string& key, float value);
    void setBool(const std::string& section, const std::string& key, bool value);

    // Cache system for fast access to commonly used config values
    void registerCache(int cacheId, const std::string& section, const std::string& key)
    {
        m_cacheMap[cacheId] = getInt(section, key, 0);
        m_cacheSections[cacheId] = section;
        m_cacheKeys[cacheId] = key;
    }

    int getCache(int cacheId) const
    {
        auto it = m_cacheMap.find(cacheId);
        return (it != m_cacheMap.end()) ? it->second : 0;
    }

    void setCache(int cacheId, int value)
    {
        m_cacheMap[cacheId] = value;
        auto secIt = m_cacheSections.find(cacheId);
        auto keyIt = m_cacheKeys.find(cacheId);
        if (secIt != m_cacheSections.end() && keyIt != m_cacheKeys.end())
            setInt(secIt->second, keyIt->second, value);
    }

private:
    std::string m_filepath;
    bool m_isLocal = true;
    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::map<int, int> m_cacheMap;
    std::map<int, std::string> m_cacheSections;
    std::map<int, std::string> m_cacheKeys;
};

// Singleton accessor
#define sConfig Config::instance()
