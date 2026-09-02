// Session Manager - Manages all client sessions
// Task 2.6: Added update() for timeout checking

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <string>

class Session;

class SessionManager
{
public:
    static SessionManager& instance();

    // Session management
    Session* createSession();
    void removeSession(uint32_t id);
    Session* getSession(uint32_t id);

    // Update all sessions - check timeouts, remove disconnected
    void update();

    // Disconnect all sessions (for shutdown)
    void disconnectAll(const std::string& reason = "");

    // Find a session by account ID (for duplicate login handling)
    Session* getSessionByAccountId(uint32_t accountId);

    // Kick any existing session for this account (for duplicate login handling)
    void kickDuplicateLogin(uint32_t accountId, const std::string& reason = "Logged in from another location");

    // Iterate all sessions
    template<typename Func>
    void forEachSession(Func&& func)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& [id, session] : m_sessions) {
            func(*session);
        }
    }

    // Statistics
    size_t getSessionCount() const;

private:
    SessionManager() = default;

    // Check if a session has timed out based on its state
    bool isSessionTimedOut(const Session& session) const;

    std::unordered_map<uint32_t, std::unique_ptr<Session>> m_sessions;
    mutable std::recursive_mutex m_mutex;  // Recursive to allow nested calls (e.g., kickDuplicateLogin from packet handlers)
    uint32_t m_nextId = 1;
};

#define sSessionManager SessionManager::instance()
