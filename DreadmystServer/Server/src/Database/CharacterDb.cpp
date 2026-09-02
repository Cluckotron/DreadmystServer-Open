// CharacterDb - Character database operations
// Task 3.4: Character Database Operations

#include "stdafx.h"
#include "Database/CharacterDb.h"
#include "Database/DatabaseManager.h"
#include "Database/GameData.h"
#include "Core/Logger.h"
#include <cctype>
#include <algorithm>

// ============================================================================
// Configuration
// ============================================================================

namespace CharacterConfig
{
    constexpr int MIN_NAME_LENGTH = 2;
    constexpr int MAX_NAME_LENGTH = 16;
}

// ============================================================================
// Helper Functions
// ============================================================================

CharacterInfo CharacterDb::loadCharacterFromStatement(PreparedStatement& stmt)
{
    CharacterInfo info;
    info.guid = stmt.getInt(0);
    info.accountId = stmt.getInt(1);
    info.name = stmt.getString(2);
    info.classId = stmt.getInt(3);
    info.gender = stmt.getInt(4);
    info.level = stmt.getInt(5);
    info.experience = stmt.getInt(6);
    info.portraitId = stmt.getInt(7);
    info.skinColor = stmt.getInt(8);
    info.hairStyle = stmt.getInt(9);
    info.hairColor = stmt.getInt(10);
    info.mapId = stmt.getInt(11);
    info.posX = static_cast<float>(stmt.getDouble(12));
    info.posY = static_cast<float>(stmt.getDouble(13));
    info.facing = static_cast<float>(stmt.getDouble(14));
    info.health = stmt.getInt(15);
    info.maxHealth = stmt.getInt(16);
    info.mana = stmt.getInt(17);
    info.maxMana = stmt.getInt(18);
    info.gold = stmt.getInt(19);
    info.playedTime = stmt.getInt(20);
    return info;
}

ClassStartingStats CharacterDb::getStartingStats(int32_t classId)
{
    ClassStartingStats stats;

    if (const ClassLevelStats* classStats = sGameData.getClassStats(classId, 1))
    {
        stats.health = classStats->health;
        stats.mana = classStats->mana;
    }
    else
    {
        stats.health = 100;
        stats.mana = 50;
    }

    // Fanadin is the released game's starting map.  Its canonical spawn is
    // resolved by GameData from teleport_names(name='start'), currently
    // (17, 106), beside Laudrus and the opening quest chain.
    stats.startMapId = 1;
    if (const MapTemplate* startMap = sGameData.getMap(stats.startMapId);
        startMap && startMap->startX > 0 && startMap->startY > 0)
    {
        stats.startX = static_cast<float>(startMap->startX);
        stats.startY = static_cast<float>(startMap->startY);
    }
    else
    {
        // Defensive fallback for a malformed/older game.db snapshot.
        stats.startX = 17.0f;
        stats.startY = 106.0f;
    }

    return stats;
}

// ============================================================================
// Validation
// ============================================================================

bool CharacterDb::isValidName(const std::string& name)
{
    if (name.length() < CharacterConfig::MIN_NAME_LENGTH ||
        name.length() > CharacterConfig::MAX_NAME_LENGTH)
    {
        return false;
    }

    // Must start with a letter
    if (!std::isalpha(static_cast<unsigned char>(name[0])))
    {
        return false;
    }

    // Only allow letters and spaces (for names like "Dark Knight")
    // But no consecutive spaces or leading/trailing spaces
    bool lastWasSpace = false;
    for (size_t i = 0; i < name.length(); ++i)
    {
        char c = name[i];
        bool isSpace = (c == ' ');

        if (isSpace)
        {
            // No leading space (i==0 handled by "must start with letter")
            // No trailing space
            if (i == name.length() - 1)
            {
                return false;
            }
            // No consecutive spaces
            if (lastWasSpace)
            {
                return false;
            }
        }
        else if (!std::isalpha(static_cast<unsigned char>(c)))
        {
            // Only letters and spaces allowed
            return false;
        }

        lastWasSpace = isSpace;
    }

    return true;
}

bool CharacterDb::isNameTaken(const std::string& name)
{
    auto stmt = sDatabase.prepare(
        "SELECT 1 FROM characters WHERE name = ? COLLATE NOCASE AND is_deleted = 0"
    );

    if (!stmt.valid())
    {
        return true;  // Err on the side of caution
    }

    stmt.bind(1, name);
    return stmt.step();  // Returns true if a row exists
}

bool CharacterDb::ensureStartingSpells(int32_t guid, int32_t classId)
{
    const auto& startingSpells = sGameData.getStartingSpells(classId);
    if (startingSpells.empty())
    {
        LOG_WARN("CharacterDb: No player_create_spell rows for class %d", classId);
        return false;
    }

    auto insert = sDatabase.prepare(
        "INSERT OR IGNORE INTO character_spells (character_guid, spell_id, rank) "
        "VALUES (?, ?, 1)"
    );
    if (!insert.valid())
    {
        LOG_ERROR("CharacterDb: Failed to prepare starter-spell insert");
        return false;
    }

    int inserted = 0;
    for (int32_t spellId : startingSpells)
    {
        insert.reset();
        insert.bind(1, guid);
        insert.bind(2, spellId);
        if (!insert.execute())
        {
            LOG_ERROR("CharacterDb: Failed seeding spell %d for character %d", spellId, guid);
            return false;
        }
        if (sDatabase.changesCount() > 0)
            ++inserted;
    }

    if (inserted > 0)
    {
        LOG_INFO("CharacterDb: Seeded %d starter spells for character %d (class %d)",
                 inserted, guid, classId);
    }
    return true;
}

// ============================================================================
// Character Operations
// ============================================================================

std::optional<int32_t> CharacterDb::createCharacter(
    int32_t accountId,
    const std::string& name,
    int32_t classId,
    int32_t gender,
    int32_t portraitId)
{
    if (!isValidName(name))
    {
        LOG_WARN("CharacterDb: Invalid character name: %s", name.c_str());
        return std::nullopt;
    }

    if (isNameTaken(name))
    {
        LOG_INFO("CharacterDb: Character name already taken: %s", name.c_str());
        return std::nullopt;
    }

    // Check max characters per account
    int32_t currentCount = getCharacterCount(accountId);
    if (currentCount >= MAX_CHARACTERS_PER_ACCOUNT)
    {
        LOG_INFO("CharacterDb: Account %d already has max characters (%d)",
                 accountId, MAX_CHARACTERS_PER_ACCOUNT);
        return std::nullopt;
    }

    // Get starting stats for this class
    auto startStats = getStartingStats(classId);

    auto stmt = sDatabase.prepare(
        "INSERT INTO characters "
        "(account_id, name, class_id, gender, portrait_id, level, experience, "
        " map_id, position_x, position_y, health, max_health, mana, max_mana) "
        "VALUES (?, ?, ?, ?, ?, 1, 0, ?, ?, ?, ?, ?, ?, ?)"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("CharacterDb: Failed to prepare character insert statement");
        return std::nullopt;
    }

    stmt.bind(1, accountId);
    stmt.bind(2, name);
    stmt.bind(3, classId);
    stmt.bind(4, gender);
    stmt.bind(5, portraitId);
    stmt.bind(6, startStats.startMapId);
    stmt.bind(7, static_cast<double>(startStats.startX));
    stmt.bind(8, static_cast<double>(startStats.startY));
    stmt.bind(9, startStats.health);
    stmt.bind(10, startStats.health);  // max_health
    stmt.bind(11, startStats.mana);
    stmt.bind(12, startStats.mana);    // max_mana

    if (stmt.execute())
    {
        int64_t newGuid = sDatabase.lastInsertId();
        if (newGuid > 0)
        {
            LOG_INFO("CharacterDb: Created character '%s' (GUID %lld) for account %d",
                     name.c_str(), newGuid, accountId);
            ensureStartingSpells(static_cast<int32_t>(newGuid), classId);
            return static_cast<int32_t>(newGuid);
        }
    }

    LOG_ERROR("CharacterDb: Failed to create character '%s': %s", name.c_str(),
              sDatabase.handle() ? sqlite3_errmsg(sDatabase.handle()) : "database unavailable");
    return std::nullopt;
}

std::vector<CharacterInfo> CharacterDb::getCharactersByAccount(int32_t accountId)
{
    std::vector<CharacterInfo> characters;

    auto stmt = sDatabase.prepare(
        "SELECT guid, account_id, name, class_id, gender, level, experience, "
        "portrait_id, skin_color, hair_style, hair_color, map_id, "
        "position_x, position_y, facing, health, max_health, mana, max_mana, gold, played_time "
        "FROM characters WHERE account_id = ? AND is_deleted = 0 ORDER BY guid"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("CharacterDb: Failed to prepare character list statement");
        return characters;
    }

    stmt.bind(1, accountId);

    while (stmt.step())
    {
        characters.push_back(loadCharacterFromStatement(stmt));
    }

    return characters;
}

std::optional<CharacterInfo> CharacterDb::getCharacterByGuid(int32_t guid)
{
    auto stmt = sDatabase.prepare(
        "SELECT guid, account_id, name, class_id, gender, level, experience, "
        "portrait_id, skin_color, hair_style, hair_color, map_id, "
        "position_x, position_y, facing, health, max_health, mana, max_mana, gold, played_time "
        "FROM characters WHERE guid = ? AND is_deleted = 0"
    );

    if (!stmt.valid())
    {
        return std::nullopt;
    }

    stmt.bind(1, guid);

    if (stmt.step())
    {
        return loadCharacterFromStatement(stmt);
    }

    return std::nullopt;
}

std::optional<CharacterInfo> CharacterDb::getCharacterByName(const std::string& name)
{
    auto stmt = sDatabase.prepare(
        "SELECT guid, account_id, name, class_id, gender, level, experience, "
        "portrait_id, skin_color, hair_style, hair_color, map_id, "
        "position_x, position_y, facing, health, max_health, mana, max_mana, gold, played_time "
        "FROM characters WHERE name = ? COLLATE NOCASE AND is_deleted = 0"
    );

    if (!stmt.valid())
    {
        return std::nullopt;
    }

    stmt.bind(1, name);

    if (stmt.step())
    {
        return loadCharacterFromStatement(stmt);
    }

    return std::nullopt;
}

bool CharacterDb::deleteCharacter(int32_t guid, int32_t accountId)
{
    // Verify ownership and perform soft delete
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET is_deleted = 1, deleted_at = CURRENT_TIMESTAMP, "
        "name = '__deleted_' || guid || '_' || name "
        "WHERE guid = ? AND account_id = ? AND is_deleted = 0"
    );

    if (!stmt.valid())
    {
        return false;
    }

    stmt.bind(1, guid);
    stmt.bind(2, accountId);
    stmt.step();

    int changes = sDatabase.changesCount();
    if (changes > 0)
    {
        LOG_INFO("CharacterDb: Deleted character GUID %d for account %d", guid, accountId);
        return true;
    }

    LOG_WARN("CharacterDb: Failed to delete character GUID %d for account %d (not found or not owned)",
             guid, accountId);
    return false;
}

void CharacterDb::updatePosition(int32_t guid, int32_t mapId, float x, float y, float facing)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET map_id = ?, position_x = ?, position_y = ?, facing = ? "
        "WHERE guid = ? AND is_deleted = 0"
    );

    if (stmt.valid())
    {
        stmt.bind(1, mapId);
        stmt.bind(2, static_cast<double>(x));
        stmt.bind(3, static_cast<double>(y));
        stmt.bind(4, static_cast<double>(facing));
        stmt.bind(5, guid);
        stmt.step();
    }
}

void CharacterDb::updateStats(int32_t guid, int32_t health, int32_t mana, int32_t exp, int32_t level)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET health = ?, mana = ?, experience = ?, level = ? "
        "WHERE guid = ? AND is_deleted = 0"
    );

    if (stmt.valid())
    {
        stmt.bind(1, health);
        stmt.bind(2, mana);
        stmt.bind(3, exp);
        stmt.bind(4, level);
        stmt.bind(5, guid);
        stmt.step();
    }
}

void CharacterDb::updateGold(int32_t guid, int32_t gold)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET gold = ? WHERE guid = ? AND is_deleted = 0"
    );

    if (stmt.valid())
    {
        stmt.bind(1, gold);
        stmt.bind(2, guid);
        stmt.step();
    }
}

void CharacterDb::updateLastLogin(int32_t guid)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET last_login = CURRENT_TIMESTAMP WHERE guid = ?"
    );

    if (stmt.valid())
    {
        stmt.bind(1, guid);
        stmt.step();
    }
}

void CharacterDb::updateLastLogout(int32_t guid, int32_t playedTimeIncrement)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET last_logout = CURRENT_TIMESTAMP, "
        "played_time = played_time + ? WHERE guid = ?"
    );

    if (stmt.valid())
    {
        stmt.bind(1, playedTimeIncrement);
        stmt.bind(2, guid);
        stmt.step();
    }
}

void CharacterDb::saveCharacter(const CharacterInfo& character)
{
    auto stmt = sDatabase.prepare(
        "UPDATE characters SET "
        "level = ?, experience = ?, map_id = ?, position_x = ?, position_y = ?, facing = ?, "
        "health = ?, max_health = ?, mana = ?, max_mana = ?, gold = ?, played_time = ? "
        "WHERE guid = ? AND is_deleted = 0"
    );

    if (stmt.valid())
    {
        stmt.bind(1, character.level);
        stmt.bind(2, character.experience);
        stmt.bind(3, character.mapId);
        stmt.bind(4, static_cast<double>(character.posX));
        stmt.bind(5, static_cast<double>(character.posY));
        stmt.bind(6, static_cast<double>(character.facing));
        stmt.bind(7, character.health);
        stmt.bind(8, character.maxHealth);
        stmt.bind(9, character.mana);
        stmt.bind(10, character.maxMana);
        stmt.bind(11, character.gold);
        stmt.bind(12, character.playedTime);
        stmt.bind(13, character.guid);
        stmt.step();

        LOG_DEBUG("CharacterDb: Saved character GUID %d", character.guid);
    }
}

int32_t CharacterDb::getCharacterCount(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "SELECT COUNT(*) FROM characters WHERE account_id = ? AND is_deleted = 0"
    );

    if (!stmt.valid())
    {
        return 0;
    }

    stmt.bind(1, accountId);

    if (stmt.step())
    {
        return stmt.getInt(0);
    }

    return 0;
}
