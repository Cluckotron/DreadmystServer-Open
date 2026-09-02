// Client Session - Manages a single client connection
// Task 2.4: Session Lifecycle Management

#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <chrono>

class SfSocket;
class StlBuffer;
class Player;

// Session states representing the connection lifecycle
enum class SessionState
{
    Connected,      // Just connected, waiting for authentication
    Authenticated,  // Logged in, can select/create characters
    InWorld,        // Playing with an active character
    Disconnecting   // Being cleaned up, no more packets processed
};

// Convert session state to string for logging
const char* sessionStateToString(SessionState state);

class Session
{
public:
    explicit Session(uint32_t id);
    ~Session();

    // Non-copyable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Session identification
    uint32_t getId() const { return m_id; }

    // Socket management
    void setSocket(std::unique_ptr<SfSocket> socket);
    SfSocket* getSocket() { return m_socket.get(); }
    const SfSocket* getSocket() const { return m_socket.get(); }
    bool isConnected() const;

    // State management
    SessionState getState() const { return m_state; }
    void setState(SessionState state);
    bool isState(SessionState state) const { return m_state == state; }

    // State transition helpers
    bool canAuthenticate() const { return m_state == SessionState::Connected; }
    bool canSelectCharacter() const { return m_state == SessionState::Authenticated; }
    bool canPlay() const { return m_state == SessionState::InWorld; }
    bool isDisconnecting() const { return m_state == SessionState::Disconnecting; }

    // Authentication
    void setAuthenticated(uint32_t accountId, const std::string& username, bool isGm = false);
    uint32_t getAccountId() const { return m_accountId; }
    const std::string& getUsername() const { return m_username; }
    bool isGm() const { return m_isGm; }

    // Character/Player
    void setPlayer(Player* player);
    Player* getPlayer() { return m_player; }
    const Player* getPlayer() const { return m_player; }
    void clearPlayer();

    uint32_t getPlayerGuid() const { return m_playerGuid; }
    void setPlayerGuid(uint32_t guid) { m_playerGuid = guid; }

    // Packet handling
    void sendPacket(const StlBuffer& data);

    // Activity tracking
    void updateLastActivity();
    void updateLastPing();
    int64_t getLastActivityTime() const { return m_lastActivity; }
    int64_t getLastPingTime() const { return m_lastPing; }

    // Timeout checking
    bool isTimedOut(int timeoutSeconds) const;
    bool isPingTimedOut(int timeoutSeconds) const;

    // Different timeouts for different states
    static constexpr int AUTH_TIMEOUT_SECONDS = 30;     // 30 sec to authenticate
    static constexpr int CHAR_SELECT_TIMEOUT = 300;     // 5 min in character select
    static constexpr int INWORLD_PING_TIMEOUT = 120;    // 2 min ping timeout in world

    // Disconnect handling
    void initiateDisconnect(const std::string& reason = "");
    const std::string& getDisconnectReason() const { return m_disconnectReason; }
    bool shouldRemove() const;
    void markForRemoval() { m_markedForRemoval = true; }

    // Remote address for logging
    std::string getRemoteAddress() const;

private:
    uint32_t m_id;
    std::unique_ptr<SfSocket> m_socket;

    // State
    SessionState m_state = SessionState::Connected;

    // Account info (set after authentication)
    uint32_t m_accountId = 0;
    std::string m_username;
    bool m_isGm = false;

    // Player info (set when entering world)
    Player* m_player = nullptr;
    uint32_t m_playerGuid = 0;

    // Timing
    int64_t m_lastActivity = 0;
    int64_t m_lastPing = 0;
    int64_t m_connectedAt = 0;

    // Disconnect handling
    bool m_markedForRemoval = false;
    std::string m_disconnectReason;
};
