// Session Manager - Manages all client sessions
// Task 2.6: Added update() for timeout checking

#include "stdafx.h"
#include "Network/SessionManager.h"
#include "Network/Session.h"
#include "Core/Logger.h"
#include "SfSocket.h"

SessionManager& SessionManager::instance()
{
    static SessionManager instance;
    return instance;
}

Session* SessionManager::createSession()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    uint32_t id = m_nextId++;
    auto session = std::make_unique<Session>(id);
    auto* ptr = session.get();
    m_sessions[id] = std::move(session);
    LOG_INFO("Session %u created", id);
    return ptr;
}

void SessionManager::removeSession(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_sessions.erase(id) > 0) {
        LOG_INFO("Session %u removed", id);
    }
}

Session* SessionManager::getSession(uint32_t id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_sessions.find(id);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}

void SessionManager::update()
{
    std::vector<uint32_t> toRemove;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        for (auto& [id, session] : m_sessions) {
            // Check for disconnected sessions
            if (session->shouldRemove()) {
                toRemove.push_back(id);
                continue;
            }

            // Check for timeout based on state
            if (isSessionTimedOut(*session)) {
                session->initiateDisconnect("Connection timeout");
                toRemove.push_back(id);
                continue;
            }

            // Check if socket disconnected unexpectedly
            if (!session->isConnected() && !session->isDisconnecting()) {
                LOG_INFO("Session %u: Connection lost", id);
                session->initiateDisconnect("Connection lost");
                toRemove.push_back(id);
            }
        }
    }

    // Remove sessions outside of lock
    for (uint32_t id : toRemove) {
        removeSession(id);
    }
}

Session* SessionManager::getSessionByAccountId(uint32_t accountId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& [id, session] : m_sessions) {
        if (session->getAccountId() == accountId &&
            !session->isDisconnecting() &&
            session->getState() != SessionState::Connected)
        {
            return session.get();
        }
    }
    return nullptr;
}

void SessionManager::kickDuplicateLogin(uint32_t accountId, const std::string& reason)
{
    Session* existingSession = nullptr;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        for (auto& [id, session] : m_sessions) {
            if (session->getAccountId() == accountId &&
                !session->isDisconnecting() &&
                session->getState() != SessionState::Connected)
            {
                existingSession = session.get();
                break;
            }
        }
    }

    if (existingSession) {
        LOG_INFO("Session %u: Kicked for duplicate login (account %u)",
                 existingSession->getId(), accountId);
        existingSession->initiateDisconnect(reason);
    }
}

void SessionManager::disconnectAll(const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    for (auto& [id, session] : m_sessions) {
        if (!session->isDisconnecting()) {
            session->initiateDisconnect(reason);

            // Close the socket
            if (session->getSocket()) {
                session->getSocket()->disconnect();
            }
        }
    }

    LOG_INFO("Disconnected all %zu sessions: %s",
             m_sessions.size(), reason.empty() ? "Server shutdown" : reason.c_str());
}

size_t SessionManager::getSessionCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_sessions.size();
}

bool SessionManager::isSessionTimedOut(const Session& session) const
{
    switch (session.getState()) {
        case SessionState::Connected:
            // Unauthenticated connections timeout quickly
            return session.isTimedOut(Session::AUTH_TIMEOUT_SECONDS);

        case SessionState::Authenticated:
            // Character select can take longer
            return session.isTimedOut(Session::CHAR_SELECT_TIMEOUT);

        case SessionState::InWorld:
            // In-world sessions check ping timeout
            return session.isPingTimedOut(Session::INWORLD_PING_TIMEOUT);

        case SessionState::Disconnecting:
            // Already disconnecting, don't timeout again
            return false;

        default:
            return false;
    }
}
