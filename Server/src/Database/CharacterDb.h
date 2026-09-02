// CharacterDb - Character database operations
// Task 3.4: Character Database Operations

#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

// Character information structure (for character list and operations)
struct CharacterInfo
{
    int32_t guid = 0;
    int32_t accountId = 0;
    std::string name;
    int32_t classId = 0;
    int32_t gender = 0;
    int32_t level = 1;
    int32_t experience = 0;
    int32_t portraitId = 0;
    int32_t skinColor = 0;
    int32_t hairStyle = 0;
    int32_t hairColor = 0;
    int32_t mapId = 1;
    float posX = 0.0f;
    float posY = 0.0f;
    float facing = 0.0f;
    int32_t health = 100;
    int32_t maxHealth = 100;
    int32_t mana = 50;
    int32_t maxMana = 50;
    int32_t gold = 0;
    int32_t playedTime = 0;
};

// Starting stats per class (for character creation)
struct ClassStartingStats
{
    int32_t health = 100;
    int32_t mana = 50;
    int32_t startMapId = 1;
    float startX = 17.0f;
    float startY = 106.0f;
};

// Character database operations
class CharacterDb
{
public:
    // Create a new character
    // Returns the new character GUID on success, std::nullopt on failure
    static std::optional<int32_t> createCharacter(
        int32_t accountId,
        const std::string& name,
        int32_t classId,
        int32_t gender,
        int32_t portraitId);

    // Get all characters for an account (non-deleted only)
    static std::vector<CharacterInfo> getCharactersByAccount(int32_t accountId);

    // Get a character by GUID
    static std::optional<CharacterInfo> getCharacterByGuid(int32_t guid);

    // Get a character by name
    static std::optional<CharacterInfo> getCharacterByName(const std::string& name);

    // Delete a character (soft delete)
    // Returns true if deleted, false if not found or not owned by account
    static bool deleteCharacter(int32_t guid, int32_t accountId);

    // Update character position
    static void updatePosition(int32_t guid, int32_t mapId, float x, float y, float facing);

    // Update character stats (health, mana, exp, level)
    static void updateStats(int32_t guid, int32_t health, int32_t mana, int32_t exp, int32_t level);

    // Update character gold
    static void updateGold(int32_t guid, int32_t gold);

    // Update last login time
    static void updateLastLogin(int32_t guid);

    // Update last logout time and played time
    static void updateLastLogout(int32_t guid, int32_t playedTimeIncrement);

    // Full save (saves all mutable fields)
    static void saveCharacter(const CharacterInfo& character);

    // Validation
    static bool isValidName(const std::string& name);
    static bool isNameTaken(const std::string& name);

    // Get count of characters for an account
    static int32_t getCharacterCount(int32_t accountId);

    // Get starting stats for a class
    static ClassStartingStats getStartingStats(int32_t classId);

    // Ensure the released game's class starter spell set exists in the
    // private character database. Safe to call repeatedly (INSERT OR IGNORE).
    static bool ensureStartingSpells(int32_t guid, int32_t classId);

    // Max characters per account
    static constexpr int MAX_CHARACTERS_PER_ACCOUNT = 5;

private:
    // Load character from prepared statement result
    static CharacterInfo loadCharacterFromStatement(class PreparedStatement& stmt);
};
