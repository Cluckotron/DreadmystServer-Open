// ChatSystem - Handles chat message routing and delivery
// Phase 8, Task 8.1: Chat System

#include "stdafx.h"
#include "Systems/ChatSystem.h"
#include "Systems/PartySystem.h"
#include "Systems/GuildSystem.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Network/Session.h"
#include "Core/Logger.h"
#include "Database/DatabaseManager.h"

#include "GamePacketServer.h"
#include "GamePacketClient.h"

#include <cmath>
#include <algorithm>

namespace ChatSystem
{

// ============================================================================
// ChatManager Singleton
// ============================================================================

ChatManager& ChatManager::instance()
{
    static ChatManager inst;
    return inst;
}

// ============================================================================
// Core Chat Functions
// ============================================================================

void ChatManager::handleChatMessage(Player* sender, ChatDefines::Channels channel,
                                    const std::string& message, const std::string& targetName)
{
    if (!sender)
        return;

    // Validate message content
    if (!validateMessage(message))
    {
        sendChatError(sender, ChatError::InvalidMessage);
        return;
    }

    // Check rate limiting
    if (isRateLimited(sender, channel))
    {
        sendChatError(sender, ChatError::RateLimited);
        return;
    }

    // Update rate limit timestamp
    updateRateLimit(sender, channel);

    // Filter message (profanity filter)
    std::string filteredMessage = filterMessage(message);

    // Route to appropriate handler based on channel
    switch (channel)
    {
        case ChatDefines::Channels::Say:
            handleSay(sender, filteredMessage);
            break;
        case ChatDefines::Channels::Yell:
            handleYell(sender, filteredMessage);
            break;
        case ChatDefines::Channels::Whisper:
            handleWhisper(sender, targetName, filteredMessage);
            break;
        case ChatDefines::Channels::Party:
            handleParty(sender, filteredMessage);
            break;
        case ChatDefines::Channels::Guild:
            handleGuild(sender, filteredMessage);
            break;
        case ChatDefines::Channels::AllChat:
            handleAllChat(sender, filteredMessage);
            break;
        default:
            LOG_WARN("Player %s sent chat with unknown channel %d",
                     sender->getName().c_str(), static_cast<int>(channel));
            break;
    }
}

void ChatManager::sendSystemMessage(Player* target, const std::string& message)
{
    if (!target)
        return;

    sendChatMessage(target, ChatDefines::Channels::System, 0, "", message);
}

void ChatManager::sendSystemMessageToMap(int mapId, const std::string& message)
{
    auto players = sWorldManager.getPlayersOnMap(mapId);
    for (Player* player : players)
    {
        sendSystemMessage(player, message);
    }
}

void ChatManager::sendSystemMessageGlobal(const std::string& message)
{
    auto players = sWorldManager.getAllPlayers();
    for (Player* player : players)
    {
        sendSystemMessage(player, message);
    }
}

// ============================================================================
// Channel-specific handlers
// ============================================================================

void ChatManager::handleSay(Player* sender, const std::string& message)
{
    LOG_DEBUG("Player %s says: %s", sender->getName().c_str(), message.c_str());

    // Get players in say range
    std::vector<Player*> recipients;
    if (SAY_RANGE > 0.0f)
    {
        recipients = getPlayersInRange(sender->getMapId(), sender->getX(), sender->getY(), SAY_RANGE);
    }
    else
    {
        // Unlimited range - all players on map
        recipients = sWorldManager.getPlayersOnMap(sender->getMapId());
    }

    // Send to all recipients (including sender)
    for (Player* recipient : recipients)
    {
        // Check ignore list
        if (isIgnoring(recipient->getGuid(), sender->getGuid()))
            continue;

        sendChatMessage(recipient, ChatDefines::Channels::Say,
                        sender->getGuid(), sender->getName(), message);
    }
}

void ChatManager::handleYell(Player* sender, const std::string& message)
{
    LOG_DEBUG("Player %s yells: %s", sender->getName().c_str(), message.c_str());

    // Get players in yell range
    std::vector<Player*> recipients;
    if (YELL_RANGE > 0.0f)
    {
        recipients = getPlayersInRange(sender->getMapId(), sender->getX(), sender->getY(), YELL_RANGE);
    }
    else
    {
        // Unlimited range - all players on map
        recipients = sWorldManager.getPlayersOnMap(sender->getMapId());
    }

    // Send to all recipients (including sender)
    for (Player* recipient : recipients)
    {
        // Check ignore list
        if (isIgnoring(recipient->getGuid(), sender->getGuid()))
            continue;

        sendChatMessage(recipient, ChatDefines::Channels::Yell,
                        sender->getGuid(), sender->getName(), message);
    }
}

void ChatManager::handleWhisper(Player* sender, const std::string& targetName, const std::string& message)
{
    LOG_DEBUG("Player %s whispers to %s: %s",
              sender->getName().c_str(), targetName.c_str(), message.c_str());

    // Find target player by name
    Player* target = sWorldManager.getPlayerByName(targetName);
    if (!target)
    {
        sendChatError(sender, ChatError::PlayerNotFound);
        return;
    }

    // Can't whisper yourself
    if (target == sender)
    {
        sendChatError(sender, ChatError::InvalidMessage);
        return;
    }

    // Check if target is ignoring sender
    if (isIgnoring(target->getGuid(), sender->getGuid()))
    {
        sendChatError(sender, ChatError::PlayerIgnoringYou);
        return;
    }

    // Send to target
    sendChatMessage(target, ChatDefines::Channels::Whisper,
                    sender->getGuid(), sender->getName(), message);

    // Send confirmation to sender (shows as "To [name]: message")
    sendSystemMessage(sender, "To [" + target->getName() + "]: " + message);
}

void ChatManager::handleParty(Player* sender, const std::string& message)
{
    LOG_DEBUG("Player %s party chat: %s", sender->getName().c_str(), message.c_str());

    // Check if player is in a party
    if (!sPartyManager.isInParty(sender->getGuid()))
    {
        sendChatError(sender, ChatError::NotInParty);
        return;
    }

    // Send to all party members via PartyManager
    sPartyManager.sendPartyMessage(sender, message);
}

void ChatManager::handleGuild(Player* sender, const std::string& message)
{
    LOG_DEBUG("Player %s guild chat: %s", sender->getName().c_str(), message.c_str());

    // Check if player is in a guild
    if (!sGuildManager.isInGuild(sender->getGuid()))
    {
        sendChatError(sender, ChatError::NotInGuild);
        return;
    }

    // Delegate to GuildManager for message delivery
    sGuildManager.sendGuildMessage(sender, message);
}

void ChatManager::handleAllChat(Player* sender, const std::string& message)
{
    LOG_DEBUG("Player %s global chat: %s", sender->getName().c_str(), message.c_str());

    // Get all online players
    auto players = sWorldManager.getAllPlayers();

    // Send to all players (including sender)
    for (Player* recipient : players)
    {
        // Check ignore list
        if (isIgnoring(recipient->getGuid(), sender->getGuid()))
            continue;

        sendChatMessage(recipient, ChatDefines::Channels::AllChat,
                        sender->getGuid(), sender->getName(), message);
    }
}

// ============================================================================
// Ignore List
// ============================================================================

void ChatManager::addIgnore(Player* player, const std::string& targetName)
{
    if (!player)
        return;

    // Find target player to get their GUID
    // Note: For offline players, we'd need a database lookup
    Player* target = sWorldManager.getPlayerByName(targetName);
    if (!target)
    {
        // Try database lookup for offline player
        auto stmt = sDatabase.prepare("SELECT guid FROM characters WHERE name = ? COLLATE NOCASE");
        if (!stmt.valid())
        {
            sendChatError(player, ChatError::PlayerNotFound);
            return;
        }

        stmt.bind(1, targetName);
        if (!stmt.step())
        {
            sendChatError(player, ChatError::PlayerNotFound);
            return;
        }

        uint32_t targetGuid = static_cast<uint32_t>(stmt.getInt(0));

        // Add to memory
        m_ignoreLists[player->getGuid()].insert(targetGuid);

        // Save to database (flags = 1 means ignore)
        auto insertStmt = sDatabase.prepare(
            "INSERT OR REPLACE INTO character_social (character_guid, friend_guid, flags) VALUES (?, ?, 1)");
        if (insertStmt.valid())
        {
            insertStmt.bind(1, static_cast<int>(player->getGuid()));
            insertStmt.bind(2, static_cast<int>(targetGuid));
            insertStmt.step();
        }

        sendSystemMessage(player, "Now ignoring " + targetName);
        return;
    }

    // Can't ignore yourself
    if (target == player)
    {
        sendSystemMessage(player, "You cannot ignore yourself.");
        return;
    }

    // Add to memory
    m_ignoreLists[player->getGuid()].insert(target->getGuid());

    // Save to database (flags = 1 means ignore)
    auto insertStmt = sDatabase.prepare(
        "INSERT OR REPLACE INTO character_social (character_guid, friend_guid, flags) VALUES (?, ?, 1)");
    if (insertStmt.valid())
    {
        insertStmt.bind(1, static_cast<int>(player->getGuid()));
        insertStmt.bind(2, static_cast<int>(target->getGuid()));
        insertStmt.step();
    }

    sendSystemMessage(player, "Now ignoring " + target->getName());
    LOG_INFO("Player %s is now ignoring %s",
             player->getName().c_str(), target->getName().c_str());
}

void ChatManager::removeIgnore(Player* player, const std::string& targetName)
{
    if (!player)
        return;

    // Find target player or look up in database
    Player* target = sWorldManager.getPlayerByName(targetName);
    uint32_t targetGuid = 0;
    std::string resolvedName = targetName;

    if (target)
    {
        targetGuid = target->getGuid();
        resolvedName = target->getName();
    }
    else
    {
        // Database lookup for offline player
        auto stmt = sDatabase.prepare("SELECT guid, name FROM characters WHERE name = ? COLLATE NOCASE");
        if (!stmt.valid())
        {
            sendChatError(player, ChatError::PlayerNotFound);
            return;
        }

        stmt.bind(1, targetName);
        if (!stmt.step())
        {
            sendChatError(player, ChatError::PlayerNotFound);
            return;
        }

        targetGuid = static_cast<uint32_t>(stmt.getInt(0));
        resolvedName = stmt.getString(1);
    }

    // Remove from memory
    auto it = m_ignoreLists.find(player->getGuid());
    if (it != m_ignoreLists.end())
    {
        it->second.erase(targetGuid);
    }

    // Remove from database (only delete if it's an ignore entry, flags = 1)
    auto deleteStmt = sDatabase.prepare(
        "DELETE FROM character_social WHERE character_guid = ? AND friend_guid = ? AND flags = 1");
    if (deleteStmt.valid())
    {
        deleteStmt.bind(1, static_cast<int>(player->getGuid()));
        deleteStmt.bind(2, static_cast<int>(targetGuid));
        deleteStmt.step();
    }

    sendSystemMessage(player, "No longer ignoring " + resolvedName);
    LOG_INFO("Player %s is no longer ignoring %s",
             player->getName().c_str(), resolvedName.c_str());
}

bool ChatManager::isIgnoring(uint32_t playerGuid, uint32_t targetGuid) const
{
    auto it = m_ignoreLists.find(playerGuid);
    if (it == m_ignoreLists.end())
        return false;

    return it->second.count(targetGuid) > 0;
}

void ChatManager::loadIgnoreList(Player* player)
{
    if (!player)
        return;

    auto stmt = sDatabase.prepare(
        "SELECT friend_guid FROM character_social WHERE character_guid = ? AND flags = 1");
    if (!stmt.valid())
    {
        LOG_WARN("ChatSystem: Failed to prepare ignore list query");
        return;
    }

    stmt.bind(1, static_cast<int>(player->getGuid()));

    auto& ignoreSet = m_ignoreLists[player->getGuid()];
    ignoreSet.clear();

    while (stmt.step())
    {
        ignoreSet.insert(static_cast<uint32_t>(stmt.getInt(0)));
    }

    LOG_DEBUG("Loaded %zu ignore entries for player %s",
              ignoreSet.size(), player->getName().c_str());
}

void ChatManager::clearIgnoreList(uint32_t playerGuid)
{
    m_ignoreLists.erase(playerGuid);
}

// ============================================================================
// Rate Limiting
// ============================================================================

bool ChatManager::isRateLimited(Player* player, ChatDefines::Channels channel)
{
    if (!player)
        return true;

    auto now = std::chrono::steady_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    uint32_t guid = player->getGuid();

    // Yell has separate rate limit
    if (channel == ChatDefines::Channels::Yell)
    {
        auto it = m_lastYellTime.find(guid);
        if (it != m_lastYellTime.end())
        {
            if (nowMs - it->second < YELL_RATE_LIMIT_MS)
                return true;
        }
        return false;
    }

    // Standard rate limit for other channels
    auto it = m_lastMessageTime.find(guid);
    if (it != m_lastMessageTime.end())
    {
        if (nowMs - it->second < RATE_LIMIT_MS)
            return true;
    }

    return false;
}

void ChatManager::updateRateLimit(Player* player, ChatDefines::Channels channel)
{
    if (!player)
        return;

    auto now = std::chrono::steady_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    uint32_t guid = player->getGuid();

    if (channel == ChatDefines::Channels::Yell)
    {
        m_lastYellTime[guid] = nowMs;
    }
    else
    {
        m_lastMessageTime[guid] = nowMs;
    }
}

void ChatManager::clearRateLimitData(uint32_t playerGuid)
{
    m_lastMessageTime.erase(playerGuid);
    m_lastYellTime.erase(playerGuid);
}

// ============================================================================
// Helpers
// ============================================================================

bool ChatManager::validateMessage(const std::string& message) const
{
    // Check length
    if (message.length() < MIN_MESSAGE_LENGTH || message.length() > MAX_MESSAGE_LENGTH)
        return false;

    // Check for empty/whitespace-only messages
    bool hasContent = false;
    for (char c : message)
    {
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            hasContent = true;
            break;
        }
    }

    return hasContent;
}

std::string ChatManager::filterMessage(const std::string& message) const
{
    // Simple profanity filter placeholder
    // In production, this would use a dictionary of banned words
    // For now, just return the message as-is
    return message;
}

void ChatManager::sendChatError(Player* player, ChatError error)
{
    if (!player)
        return;

    GP_Server_ChatError packet;
    packet.m_code = static_cast<uint8_t>(error);

    player->sendPacket(packet.build(StlBuffer{}));
}

void ChatManager::sendChatMessage(Player* recipient, ChatDefines::Channels channel,
                                  uint32_t senderGuid, const std::string& senderName,
                                  const std::string& message)
{
    if (!recipient)
        return;

    GP_Server_ChatMsg packet;
    packet.m_channelId = static_cast<uint8_t>(channel);
    packet.m_fromGuid = senderGuid;
    packet.m_fromName = senderName;
    packet.m_text = message;
    // packet.m_itemId left default (no item link)

    recipient->sendPacket(packet.build(StlBuffer{}));
}

std::vector<Player*> ChatManager::getPlayersInRange(int mapId, float x, float y, float range) const
{
    std::vector<Player*> result;

    auto players = sWorldManager.getPlayersOnMap(mapId);
    float rangeSq = range * range;

    for (Player* player : players)
    {
        float dx = player->getX() - x;
        float dy = player->getY() - y;
        float distSq = dx * dx + dy * dy;

        if (distSq <= rangeSq)
        {
            result.push_back(player);
        }
    }

    return result;
}

// ============================================================================
// Packet Handlers
// ============================================================================

void handleChatMsg(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_ChatMsg packet;
    packet.unpack(data);

    ChatDefines::Channels channel = static_cast<ChatDefines::Channels>(packet.m_channelId);

    LOG_DEBUG("Received chat from %s: channel=%d, text='%s', target='%s'",
              player->getName().c_str(), packet.m_channelId,
              packet.m_text.c_str(), packet.m_targetName.c_str());

    sChatManager.handleChatMessage(player, channel, packet.m_text, packet.m_targetName);
}

void handleSetIgnorePlayer(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_SetIgnorePlayer packet;
    packet.unpack(data);

    LOG_DEBUG("Player %s %s ignore for '%s'",
              player->getName().c_str(),
              packet.m_ignore ? "adding" : "removing",
              packet.m_playerName.c_str());

    if (packet.m_ignore)
    {
        sChatManager.addIgnore(player, packet.m_playerName);
    }
    else
    {
        sChatManager.removeIgnore(player, packet.m_playerName);
    }
}

} // namespace ChatSystem
