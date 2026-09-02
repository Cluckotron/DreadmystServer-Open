// AccountDb - Account database operations
// Task 3.1: Account Database Operations

#include "stdafx.h"
#include "Database/AccountDb.h"
#include "Database/DatabaseManager.h"
#include "Core/Logger.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

// ============================================================================
// Configuration
// ============================================================================

namespace AccountConfig
{
    constexpr int MIN_USERNAME_LENGTH = 3;
    constexpr int MAX_USERNAME_LENGTH = 20;
    constexpr int MIN_PASSWORD_LENGTH = 4;
    constexpr int MAX_PASSWORD_LENGTH = 64;
    constexpr int SALT_LENGTH = 16;
}

// ============================================================================
// Utility: Parse SQLite datetime string (YYYY-MM-DD HH:MM:SS)
// Cross-platform replacement for strptime
// ============================================================================

static std::time_t parseSqliteDatetime(const std::string& datetime)
{
    if (datetime.empty())
    {
        return 0;
    }

    int year, month, day, hour, minute, second;
    if (std::sscanf(datetime.c_str(), "%d-%d-%d %d:%d:%d",
                    &year, &month, &day, &hour, &minute, &second) == 6)
    {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;  // Let mktime determine DST
        return std::mktime(&tm);
    }

    return 0;
}

// ============================================================================
// Password Hashing (Simple for local dev - upgrade to bcrypt for production)
// ============================================================================

std::string AccountDb::generateSalt()
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string salt;
    salt.reserve(AccountConfig::SALT_LENGTH);
    for (int i = 0; i < AccountConfig::SALT_LENGTH; ++i)
    {
        salt += charset[dis(gen)];
    }
    return salt;
}

// Simple hash function for local development
// Format: "salt:hash" where hash is a hex string
std::string AccountDb::hashPassword(const std::string& password, const std::string& salt)
{
    std::string actualSalt = salt.empty() ? generateSalt() : salt;

    // Simple hash: combine password and salt, then hash
    std::string combined = actualSalt + password;

    // Use std::hash as base, then add some mixing
    std::size_t hash1 = std::hash<std::string>{}(combined);
    std::size_t hash2 = std::hash<std::string>{}(combined + std::to_string(hash1));
    std::size_t hash3 = std::hash<std::string>{}(std::to_string(hash2) + combined);

    // Combine into final hash
    uint64_t finalHash = hash1 ^ (hash2 << 1) ^ (hash3 >> 1);

    // Convert to hex string
    std::ostringstream oss;
    oss << actualSalt << ":" << std::hex << std::setfill('0') << std::setw(16) << finalHash;
    return oss.str();
}

bool AccountDb::verifyPassword(const std::string& password, const std::string& storedHash)
{
    // Parse salt from stored hash (format: "salt:hash")
    auto colonPos = storedHash.find(':');
    if (colonPos == std::string::npos || colonPos == 0)
    {
        // Invalid hash format
        return false;
    }

    std::string salt = storedHash.substr(0, colonPos);

    // Re-hash with the same salt
    std::string newHash = hashPassword(password, salt);

    // Compare (timing-safe comparison would be better for production)
    return newHash == storedHash;
}

// ============================================================================
// Validation
// ============================================================================

bool AccountDb::isValidUsername(const std::string& username)
{
    if (username.length() < AccountConfig::MIN_USERNAME_LENGTH ||
        username.length() > AccountConfig::MAX_USERNAME_LENGTH)
    {
        return false;
    }

    // Only allow alphanumeric and underscore
    for (char c : username)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
        {
            return false;
        }
    }

    // Must start with a letter
    if (!std::isalpha(static_cast<unsigned char>(username[0])))
    {
        return false;
    }

    return true;
}

bool AccountDb::isValidPassword(const std::string& password)
{
    return password.length() >= AccountConfig::MIN_PASSWORD_LENGTH &&
           password.length() <= AccountConfig::MAX_PASSWORD_LENGTH;
}

// ============================================================================
// Account Operations
// ============================================================================

std::optional<int32_t> AccountDb::createAccount(const std::string& username, const std::string& password)
{
    if (!isValidUsername(username))
    {
        LOG_WARN("AccountDb: Invalid username format: %s", username.c_str());
        return std::nullopt;
    }

    if (!isValidPassword(password))
    {
        LOG_WARN("AccountDb: Invalid password format for user: %s", username.c_str());
        return std::nullopt;
    }

    // Check if username already exists
    auto existing = getAccount(username);
    if (existing.has_value())
    {
        LOG_INFO("AccountDb: Username already taken: %s", username.c_str());
        return std::nullopt;
    }

    // Hash the password
    std::string passwordHash = hashPassword(password);

    // Insert the new account
    auto stmt = sDatabase.prepare(
        "INSERT INTO accounts (username, password_hash) VALUES (?, ?)"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("AccountDb: Failed to prepare insert statement");
        return std::nullopt;
    }

    stmt.bind(1, username);
    stmt.bind(2, passwordHash);

    if (stmt.execute())
    {
        int64_t newId = sDatabase.lastInsertId();
        if (newId > 0)
        {
            LOG_INFO("AccountDb: Created account '%s' with ID %lld", username.c_str(), newId);
            return static_cast<int32_t>(newId);
        }
    }

    LOG_ERROR("AccountDb: Failed to create account '%s': %s", username.c_str(),
              sDatabase.handle() ? sqlite3_errmsg(sDatabase.handle()) : "database unavailable");
    return std::nullopt;
}

std::optional<AccountInfo> AccountDb::getAccount(const std::string& username)
{
    auto stmt = sDatabase.prepare(
        "SELECT id, username, password_hash, email, is_gm, banned_until, ban_reason, "
        "failed_logins, created_at, last_login "
        "FROM accounts WHERE username = ? COLLATE NOCASE"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("AccountDb: Failed to prepare select statement");
        return std::nullopt;
    }

    stmt.bind(1, username);

    if (stmt.step())
    {
        AccountInfo info;
        info.id = stmt.getInt(0);
        info.username = stmt.getString(1);
        info.passwordHash = stmt.getString(2);
        info.email = stmt.getString(3);
        info.isGm = stmt.getInt(4) != 0;

        if (!stmt.isNull(5))
        {
            std::string bannedStr = stmt.getString(5);
            std::time_t banTime = parseSqliteDatetime(bannedStr);
            if (banTime > 0)
            {
                info.bannedUntil = banTime;
            }
        }

        info.banReason = stmt.isNull(6) ? "" : stmt.getString(6);
        info.failedLogins = stmt.getInt(7);

        return info;
    }

    return std::nullopt;
}

std::optional<AccountInfo> AccountDb::getAccountById(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "SELECT id, username, password_hash, email, is_gm, banned_until, ban_reason, "
        "failed_logins, created_at, last_login "
        "FROM accounts WHERE id = ?"
    );

    if (!stmt.valid())
    {
        return std::nullopt;
    }

    stmt.bind(1, accountId);

    if (stmt.step())
    {
        AccountInfo info;
        info.id = stmt.getInt(0);
        info.username = stmt.getString(1);
        info.passwordHash = stmt.getString(2);
        info.email = stmt.getString(3);
        info.isGm = stmt.getInt(4) != 0;

        if (!stmt.isNull(5))
        {
            std::string bannedStr = stmt.getString(5);
            std::time_t banTime = parseSqliteDatetime(bannedStr);
            if (banTime > 0)
            {
                info.bannedUntil = banTime;
            }
        }

        info.banReason = stmt.isNull(6) ? "" : stmt.getString(6);
        info.failedLogins = stmt.getInt(7);

        return info;
    }

    return std::nullopt;
}

bool AccountDb::validatePassword(const std::string& username, const std::string& password)
{
    auto account = getAccount(username);
    if (!account.has_value())
    {
        return false;
    }

    return verifyPassword(password, account->passwordHash);
}

void AccountDb::updateLastLogin(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "UPDATE accounts SET last_login = CURRENT_TIMESTAMP WHERE id = ?"
    );

    if (stmt.valid())
    {
        stmt.bind(1, accountId);
        stmt.step();
    }
}

bool AccountDb::isBanned(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "SELECT banned_until FROM accounts WHERE id = ?"
    );

    if (!stmt.valid())
    {
        return false;
    }

    stmt.bind(1, accountId);

    if (stmt.step())
    {
        if (stmt.isNull(0))
        {
            return false;
        }

        std::string bannedStr = stmt.getString(0);
        std::time_t bannedUntil = parseSqliteDatetime(bannedStr);
        if (bannedUntil > 0)
        {
            std::time_t now = std::time(nullptr);
            return now < bannedUntil;
        }
    }

    return false;
}

std::time_t AccountDb::getBanExpiry(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "SELECT banned_until FROM accounts WHERE id = ?"
    );

    if (!stmt.valid())
    {
        return 0;
    }

    stmt.bind(1, accountId);

    if (stmt.step() && !stmt.isNull(0))
    {
        std::string bannedStr = stmt.getString(0);
        return parseSqliteDatetime(bannedStr);
    }

    return 0;
}

void AccountDb::recordFailedLogin(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "UPDATE accounts SET failed_logins = failed_logins + 1, "
        "last_failed_login = CURRENT_TIMESTAMP WHERE id = ?"
    );

    if (stmt.valid())
    {
        stmt.bind(1, accountId);
        stmt.step();
    }
}

void AccountDb::resetFailedLogins(int32_t accountId)
{
    auto stmt = sDatabase.prepare(
        "UPDATE accounts SET failed_logins = 0 WHERE id = ?"
    );

    if (stmt.valid())
    {
        stmt.bind(1, accountId);
        stmt.step();
    }
}

int32_t AccountDb::getAccountCount()
{
    auto result = sDatabase.query("SELECT COUNT(*) FROM accounts");
    if (result.success() && !result.empty())
    {
        return result[0].getInt("COUNT(*)");
    }
    return 0;
}
