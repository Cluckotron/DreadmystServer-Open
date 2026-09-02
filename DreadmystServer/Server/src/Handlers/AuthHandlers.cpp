// Authentication Packet Handlers
// Task 3.2: Account Creation Handler
// Task 3.3: Login Validation Handler

#include "stdafx.h"
#include "Handlers/AuthHandlers.h"
#include "Handlers/CharacterHandlers.h"
#include "Network/Session.h"
#include "Network/PacketRouter.h"
#include "Network/SessionManager.h"
#include "Database/AccountDb.h"
#include "Core/Logger.h"
#include "GamePacketBase.h"
#include "GamePacketClient.h"
#include "GamePacketServer.h"
#include "AccountDefines.h"

namespace Handlers
{

// ============================================================================
// Configuration
// ============================================================================

namespace AuthConfig
{
    // Expected client build version (0 = accept any)
    constexpr int32_t EXPECTED_BUILD_VERSION = 0;

    // Maximum concurrent sessions per account
    constexpr int MAX_SESSIONS_PER_ACCOUNT = 1;

    // Auto-create accounts in local dev mode
    constexpr bool AUTO_CREATE_ACCOUNTS = true;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Parse "username:password" from token string (for local auth mode)
static bool parseLocalAuthToken(const std::string& token, std::string& username, std::string& password)
{
    auto colonPos = token.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos == token.length() - 1)
    {
        return false;
    }

    username = token.substr(0, colonPos);
    password = token.substr(colonPos + 1);
    return true;
}

// Send authentication result to client
static void sendAuthResult(Session& session, AccountDefines::AuthenticateResult result)
{
    GP_Server_Validate response;
    response.m_result = static_cast<uint8_t>(result);
    response.m_serverTime = std::time(nullptr);

    StlBuffer packet;
    uint16_t opcode = response.getOpcode();
    packet << opcode;
    response.pack(packet);

    session.sendPacket(packet);
}

// ============================================================================
// Handlers
// ============================================================================

void handleAuthenticate(Session& session, StlBuffer& data)
{
    // Parse the incoming packet
    GP_Client_Authenticate authPacket;
    authPacket.unpack(data);

    LOG_INFO("Session %u: Auth request (version=%d, token_len=%zu)",
             session.getId(), authPacket.m_build, authPacket.m_userToken.length());

    // Check client version (if we care about it)
    if (AuthConfig::EXPECTED_BUILD_VERSION != 0 &&
        authPacket.m_build != AuthConfig::EXPECTED_BUILD_VERSION)
    {
        LOG_WARN("Session %u: Version mismatch (client=%d, expected=%d)",
                 session.getId(), authPacket.m_build, AuthConfig::EXPECTED_BUILD_VERSION);
        sendAuthResult(session, AccountDefines::AuthenticateResult::WrongVersion);
        return;
    }

    // Parse credentials from token (local auth mode: "username:password")
    std::string username, password;
    if (!parseLocalAuthToken(authPacket.m_userToken, username, password))
    {
        LOG_WARN("Session %u: Invalid token format (expected 'username:password')", session.getId());
        sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
        return;
    }

    LOG_DEBUG("Session %u: Authenticating user '%s'", session.getId(), username.c_str());

    // Try to get existing account
    auto accountInfo = AccountDb::getAccount(username);

    // If account doesn't exist, try to auto-create in local dev mode
    if (!accountInfo.has_value())
    {
        if (AuthConfig::AUTO_CREATE_ACCOUNTS)
        {
            // Validate username/password format
            if (!AccountDb::isValidUsername(username))
            {
                LOG_WARN("Session %u: Invalid username format: %s", session.getId(), username.c_str());
                sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
                return;
            }

            if (!AccountDb::isValidPassword(password))
            {
                LOG_WARN("Session %u: Invalid password format for user: %s", session.getId(), username.c_str());
                sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
                return;
            }

            // Create the account
            auto newAccountId = AccountDb::createAccount(username, password);
            if (!newAccountId.has_value())
            {
                LOG_ERROR("Session %u: Failed to auto-create account for '%s'", session.getId(), username.c_str());
                sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
                return;
            }

            LOG_INFO("Session %u: Auto-created account '%s' (ID %d)",
                     session.getId(), username.c_str(), *newAccountId);

            // Fetch the newly created account
            accountInfo = AccountDb::getAccount(username);
            if (!accountInfo.has_value())
            {
                LOG_ERROR("Session %u: Failed to fetch newly created account", session.getId());
                sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
                return;
            }
        }
        else
        {
            LOG_INFO("Session %u: Account not found: %s", session.getId(), username.c_str());
            sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
            return;
        }
    }
    else
    {
        // Account exists - validate password
        if (!AccountDb::validatePassword(username, password))
        {
            LOG_INFO("Session %u: Invalid password for user '%s'", session.getId(), username.c_str());
            AccountDb::recordFailedLogin(accountInfo->id);
            sendAuthResult(session, AccountDefines::AuthenticateResult::BadPassword);
            return;
        }
    }

    // Check if account is banned
    if (AccountDb::isBanned(accountInfo->id))
    {
        LOG_INFO("Session %u: Account '%s' is banned", session.getId(), username.c_str());
        sendAuthResult(session, AccountDefines::AuthenticateResult::Banned);
        return;
    }

    // Handle duplicate login - kick any existing session for this account
    sSessionManager.kickDuplicateLogin(accountInfo->id, "Logged in from another location");

    // Update last login time and reset failed logins
    AccountDb::updateLastLogin(accountInfo->id);
    AccountDb::resetFailedLogins(accountInfo->id);

    // Transition session to authenticated state
    session.setAuthenticated(accountInfo->id, accountInfo->username, accountInfo->isGm);

    LOG_INFO("Session %u: User '%s' (ID %d) authenticated successfully%s",
             session.getId(),
             accountInfo->username.c_str(),
             accountInfo->id,
             accountInfo->isGm ? " [GM]" : "");

    // The released r1189 client does not send Client_CharacterList after
    // Server_Validate. The production Dreadmyst server pushed the initial
    // character list as part of successful login, so mirror that behavior.
    // Send Validate first so the client constructs CharacterSelection before
    // processing the list that follows.
    sendAuthResult(session, AccountDefines::AuthenticateResult::Validated);
    sendCharacterList(session);

    LOG_INFO("Session %u: Pushed initial character list after authentication",
              session.getId());
}

void registerAuthHandlers()
{
    // Client_Authenticate - must be in Connected state (not yet authenticated)
    sPacketRouter.registerHandler(
        Opcode::Client_Authenticate,
        handleAuthenticate,
        SessionState::Connected,  // Required state
        false,                    // Don't allow higher states (can't re-auth)
        "Client_Authenticate"
    );

    LOG_DEBUG("Auth handlers registered");
}

} // namespace Handlers
