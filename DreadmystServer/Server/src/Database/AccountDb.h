// AccountDb - Account database operations
// Task 3.1: Account Database Operations

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <ctime>

// Account information structure
struct AccountInfo
{
    int32_t id = 0;
    std::string username;
    std::string passwordHash;
    std::string email;
    bool isGm = false;
    std::optional<std::time_t> bannedUntil;
    std::string banReason;
    int32_t failedLogins = 0;
    std::time_t createdAt = 0;
    std::time_t lastLogin = 0;
};

// Account database operations
class AccountDb
{
public:
    // Create a new account
    // Returns the new account ID on success, std::nullopt if username taken or error
    static std::optional<int32_t> createAccount(const std::string& username, const std::string& password);

    // Get account info by username
    // Returns std::nullopt if account doesn't exist
    static std::optional<AccountInfo> getAccount(const std::string& username);

    // Get account info by ID
    static std::optional<AccountInfo> getAccountById(int32_t accountId);

    // Validate password for an account
    // Returns true if password matches
    static bool validatePassword(const std::string& username, const std::string& password);

    // Update the last login timestamp
    static void updateLastLogin(int32_t accountId);

    // Check if account is currently banned
    static bool isBanned(int32_t accountId);

    // Get ban info (returns bannedUntil time, 0 if not banned)
    static std::time_t getBanExpiry(int32_t accountId);

    // Record a failed login attempt
    static void recordFailedLogin(int32_t accountId);

    // Reset failed login counter (after successful login)
    static void resetFailedLogins(int32_t accountId);

    // Check if username is valid (length, allowed characters)
    static bool isValidUsername(const std::string& username);

    // Check if password meets requirements (minimum length)
    static bool isValidPassword(const std::string& password);

    // Get the count of accounts (for server full check)
    static int32_t getAccountCount();

private:
    // Password hashing (simple for local dev, can be upgraded to bcrypt)
    static std::string hashPassword(const std::string& password, const std::string& salt = "");
    static std::string generateSalt();
    static bool verifyPassword(const std::string& password, const std::string& storedHash);
};
