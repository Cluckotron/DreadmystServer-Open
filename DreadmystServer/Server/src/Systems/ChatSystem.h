// ChatSystem - Handles chat message routing and delivery
// Phase 8, Task 8.1: Chat System

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cstdint>
#include "ChatDefines.h"

class Player;
class StlBuffer;
class Session;

namespace ChatSystem
{

// ============================================================================
// Constants
// ============================================================================

// Chat message constraints
constexpr size_t MAX_MESSAGE_LENGTH = 255;
constexpr size_t MIN_MESSAGE_LENGTH = 1;

// Say/Yell range (in pixels) - 0 means unlimited/full map
constexpr float SAY_RANGE = 400.0f;
constexpr float YELL_RANGE = 800.0f;

// Rate limiting (milliseconds between messages)
constexpr int64_t RATE_LIMIT_MS = 500;  // 500ms between messages
constexpr int64_t YELL_RATE_LIMIT_MS = 3000;  // 3s between yells

// ============================================================================
// ChatError codes
// ============================================================================

enum class ChatError : uint8_t
{
    Success = 0,
    PlayerNotFound = 1,
    PlayerIgnoringYou = 2,
    InvalidMessage = 3,
    RateLimited = 4,
    NotInParty = 5,
    NotInGuild = 6,
    ChannelNotFound = 7,
    Muted = 8,
};

// ============================================================================
// ChatManager - Singleton for chat message handling
// ============================================================================

class ChatManager
{
public:
    static ChatManager& instance();

    // -------------------------------------------------------------------------
    // Core Chat Functions
    // -------------------------------------------------------------------------

    // Handle incoming chat message from client
    void handleChatMessage(Player* sender, ChatDefines::Channels channel,
                           const std::string& message, const std::string& targetName = "");

    // Send a system message to a single player
    void sendSystemMessage(Player* target, const std::string& message);

    // Send a system message to all players on a map
    void sendSystemMessageToMap(int mapId, const std::string& message);

    // Send a system message to all online players
    void sendSystemMessageGlobal(const std::string& message);

    // -------------------------------------------------------------------------
    // Ignore List
    // -------------------------------------------------------------------------

    // Add player to ignore list
    void addIgnore(Player* player, const std::string& targetName);

    // Remove player from ignore list
    void removeIgnore(Player* player, const std::string& targetName);

    // Check if player is ignoring target
    bool isIgnoring(uint32_t playerGuid, uint32_t targetGuid) const;

    // Load ignore list from database on login
    void loadIgnoreList(Player* player);

    // Clear ignore list on logout
    void clearIgnoreList(uint32_t playerGuid);

    // -------------------------------------------------------------------------
    // Rate Limiting
    // -------------------------------------------------------------------------

    // Check if player is rate limited
    bool isRateLimited(Player* player, ChatDefines::Channels channel);

    // Update rate limit timestamp
    void updateRateLimit(Player* player, ChatDefines::Channels channel);

    // Clear rate limit data for player (on logout)
    void clearRateLimitData(uint32_t playerGuid);

private:
    ChatManager() = default;
    ~ChatManager() = default;
    ChatManager(const ChatManager&) = delete;
    ChatManager& operator=(const ChatManager&) = delete;

    // -------------------------------------------------------------------------
    // Channel-specific message handlers
    // -------------------------------------------------------------------------

    void handleSay(Player* sender, const std::string& message);
    void handleYell(Player* sender, const std::string& message);
    void handleWhisper(Player* sender, const std::string& targetName, const std::string& message);
    void handleParty(Player* sender, const std::string& message);
    void handleGuild(Player* sender, const std::string& message);
    void handleAllChat(Player* sender, const std::string& message);

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    // Validate message content
    bool validateMessage(const std::string& message) const;

    // Filter profanity (simple implementation)
    std::string filterMessage(const std::string& message) const;

    // Send chat error to player
    void sendChatError(Player* player, ChatError error);

    // Build and send GP_Server_ChatMsg to a single player
    void sendChatMessage(Player* recipient, ChatDefines::Channels channel,
                         uint32_t senderGuid, const std::string& senderName,
                         const std::string& message);

    // Get players within range of a position on a map
    std::vector<Player*> getPlayersInRange(int mapId, float x, float y, float range) const;

    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------

    // Ignore lists: playerGuid -> set of ignored player GUIDs
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> m_ignoreLists;

    // Rate limiting: playerGuid -> last message timestamp (ms since epoch)
    std::unordered_map<uint32_t, int64_t> m_lastMessageTime;
    std::unordered_map<uint32_t, int64_t> m_lastYellTime;
};

// Convenience macro
#define sChatManager ChatSystem::ChatManager::instance()

// ============================================================================
// Packet Handlers
// ============================================================================

// Handle GP_Client_ChatMsg
void handleChatMsg(Session& session, StlBuffer& data);

// Handle GP_Client_SetIgnorePlayer
void handleSetIgnorePlayer(Session& session, StlBuffer& data);

} // namespace ChatSystem
