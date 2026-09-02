// Client Session - Manages a single client connection
// Task 2.4: Session Lifecycle Management

#include "stdafx.h"
#include "Network/Session.h"
#include "SfSocket.h"
#include "Core/Logger.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Systems/ChatSystem.h"
#include "Systems/PartySystem.h"
#include "Systems/GuildSystem.h"
#include "Systems/DuelSystem.h"
#include "Systems/TradeSystem.h"

const char* sessionStateToString(SessionState state)
{
    switch (state) {
        case SessionState::Connected:     return "Connected";
        case SessionState::Authenticated: return "Authenticated";
        case SessionState::InWorld:       return "InWorld";
        case SessionState::Disconnecting: return "Disconnecting";
        default:                          return "Unknown";
    }
}

Session::Session(uint32_t id)
    : m_id(id)
{
    int64_t now = std::time(nullptr);
    m_lastActivity = now;
    m_lastPing = now;
    m_connectedAt = now;
}

Session::~Session()
{
    // Ensure player is cleaned up
    clearPlayer();
}

void Session::setSocket(std::unique_ptr<SfSocket> socket)
{
    m_socket = std::move(socket);
}

bool Session::isConnected() const
{
    return m_socket && m_socket->isConnected();
}

void Session::setState(SessionState state)
{
    if (m_state != state) {
        LOG_DEBUG("Session %u state change: %s -> %s",
                  m_id, sessionStateToString(m_state), sessionStateToString(state));
        m_state = state;

        // Reset activity timer on state change
        updateLastActivity();
    }
}

void Session::setAuthenticated(uint32_t accountId, const std::string& username, bool isGm)
{
    m_accountId = accountId;
    m_username = username;
    m_isGm = isGm;
    setState(SessionState::Authenticated);
    LOG_INFO("Session %u authenticated as '%s' (account %u)%s",
             m_id, username.c_str(), accountId, isGm ? " [GM]" : "");
}

void Session::setPlayer(Player* player)
{
    m_player = player;
    if (player) {
        setState(SessionState::InWorld);
    }
}

void Session::clearPlayer()
{
    if (m_player) {
        // Clear chat system data for this player
        uint32_t playerGuid = m_player->getGuid();
        sChatManager.clearIgnoreList(playerGuid);
        sChatManager.clearRateLimitData(playerGuid);

        // Handle party disconnect (removes from party, notifies members)
        sPartyManager.onPlayerDisconnect(playerGuid);

        // Handle guild disconnect (updates online status)
        sGuildManager.onPlayerLogout(playerGuid);

        // Handle duel disconnect (cancels any active duel)
        sDuelManager.onPlayerDisconnect(playerGuid);

        // Handle trade disconnect (cancels any active trade, items stay in inventory)
        sTradeManager.cancelTrade(m_player);

        // Despawn from world (broadcasts to other players)
        sWorldManager.despawnPlayer(m_player);

        // Save player data before clearing
        m_player->save();

        // Remove player from world
        m_player->setSpawned(false);
        m_player->setMap(nullptr);

        // Delete the player object
        delete m_player;
        m_player = nullptr;
        m_playerGuid = 0;

        // Return to authenticated state if still connected
        if (m_state == SessionState::InWorld && isConnected()) {
            setState(SessionState::Authenticated);
        }
    }
}

void Session::sendPacket(const StlBuffer& data)
{
    if (m_socket && !isDisconnecting()) {
        m_socket->send(data);
    }
}

void Session::updateLastActivity()
{
    m_lastActivity = std::time(nullptr);
}

void Session::updateLastPing()
{
    m_lastPing = std::time(nullptr);
    updateLastActivity();
}

bool Session::isTimedOut(int timeoutSeconds) const
{
    return (std::time(nullptr) - m_lastActivity) > timeoutSeconds;
}

bool Session::isPingTimedOut(int timeoutSeconds) const
{
    return (std::time(nullptr) - m_lastPing) > timeoutSeconds;
}

void Session::initiateDisconnect(const std::string& reason)
{
    if (m_state == SessionState::Disconnecting) {
        return; // Already disconnecting
    }

    m_disconnectReason = reason;
    setState(SessionState::Disconnecting);

    if (!reason.empty()) {
        LOG_INFO("Session %u disconnecting: %s", m_id, reason.c_str());
    } else {
        LOG_DEBUG("Session %u disconnecting", m_id);
    }

    // Clean up player if in world
    if (m_player) {
        clearPlayer();
    }
}

bool Session::shouldRemove() const
{
    // Remove if marked for removal
    if (m_markedForRemoval) {
        return true;
    }

    // Remove if disconnecting and socket closed
    if (m_state == SessionState::Disconnecting && !isConnected()) {
        return true;
    }

    // Remove if socket disconnected unexpectedly
    if (!isConnected() && m_state != SessionState::Disconnecting) {
        return true;
    }

    return false;
}

std::string Session::getRemoteAddress() const
{
    if (m_socket) {
        return m_socket->getRemoteAddress();
    }
    return "unknown";
}
