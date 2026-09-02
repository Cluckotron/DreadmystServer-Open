// DuelSystem - Player duel management implementation
// Phase 8, Task 8.8: Duel System

#include "stdafx.h"
#include "Systems/DuelSystem.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Network/Session.h"
#include "Core/Logger.h"
#include "PlayerDefines.h"
#include "ChatDefines.h"
#include "GamePacketServer.h"
#include "GamePacketClient.h"
#include <algorithm>
#include <cmath>

namespace Duel
{

// ============================================================================
// DuelManager Singleton
// ============================================================================

DuelManager& DuelManager::instance()
{
    static DuelManager instance;
    return instance;
}

// ============================================================================
// Duel Flow
// ============================================================================

void DuelManager::requestDuel(Player* challenger, Player* target)
{
    if (!challenger || !target)
        return;

    uint32_t challengerGuid = challenger->getGuid();
    uint32_t targetGuid = target->getGuid();

    // Can't duel yourself
    if (challengerGuid == targetGuid)
        return;

    // Check if challenger is already in a duel
    if (isInDuel(challengerGuid))
    {
        sendNotification(challenger, static_cast<uint8_t>(PlayerDefines::ChatError::TargetBusy));
        return;
    }

    // Check if target is already in a duel
    if (isInDuel(targetGuid))
    {
        sendNotification(challenger, static_cast<uint8_t>(PlayerDefines::ChatError::TargetBusy));
        return;
    }

    // Check distance
    float distance = challenger->distanceTo(target);
    if (distance > DUEL_INVITE_RANGE)
    {
        LOG_DEBUG("Duel request failed: target too far (%.1f > %.1f)", distance, DUEL_INVITE_RANGE);
        return;
    }

    // Check if there's already a pending invite to this target
    auto it = m_pendingInvites.find(targetGuid);
    if (it != m_pendingInvites.end())
    {
        // Overwrite existing invite
        m_pendingInvites.erase(it);
    }

    // Create pending invite
    PendingInvite invite;
    invite.challengerGuid = challengerGuid;
    invite.timestamp = std::time(nullptr);
    m_pendingInvites[targetGuid] = invite;

    // Send offer to target
    sendOfferDuel(target, challenger);

    LOG_DEBUG("Duel request: %s challenged %s",
              challenger->getName().c_str(), target->getName().c_str());
}

void DuelManager::respondToDuel(Player* target, bool accept)
{
    if (!target)
        return;

    uint32_t targetGuid = target->getGuid();

    // Find pending invite
    auto it = m_pendingInvites.find(targetGuid);
    if (it == m_pendingInvites.end())
    {
        LOG_DEBUG("No pending duel invite for player %u", targetGuid);
        return;
    }

    PendingInvite invite = it->second;
    m_pendingInvites.erase(it);

    // Find challenger
    Player* challenger = sWorldManager.getPlayer(invite.challengerGuid);

    if (!accept)
    {
        if (challenger)
        {
            sendNotification(challenger, static_cast<uint8_t>(PlayerDefines::ChatError::DuelDeclined));
        }
        LOG_DEBUG("Player %s declined duel", target->getName().c_str());
        return;
    }

    // Verify challenger is still valid
    if (!challenger)
    {
        LOG_DEBUG("Duel challenger %u no longer available", invite.challengerGuid);
        return;
    }

    // Verify neither is in a duel now
    if (isInDuel(targetGuid) || isInDuel(invite.challengerGuid))
    {
        sendNotification(target, static_cast<uint8_t>(PlayerDefines::ChatError::TargetBusy));
        return;
    }

    // Start countdown
    startCountdown(invite.challengerGuid, targetGuid);
}

void DuelManager::yieldDuel(Player* player)
{
    if (!player)
        return;

    uint32_t playerGuid = player->getGuid();
    ActiveDuel* duel = findDuel(playerGuid);

    if (!duel || duel->state != DuelState::Active)
    {
        LOG_DEBUG("Player %u tried to yield but not in active duel", playerGuid);
        return;
    }

    // Determine winner (the other player)
    uint32_t winnerGuid = (duel->player1Guid == playerGuid) ? duel->player2Guid : duel->player1Guid;

    endDuel(duel, winnerGuid, true);
}

// ============================================================================
// Combat Integration
// ============================================================================

bool DuelManager::areDueling(uint32_t guid1, uint32_t guid2) const
{
    // Look up duel by either guid
    auto it = m_playerToDuelKey.find(guid1);
    if (it == m_playerToDuelKey.end())
        return false;

    auto duelIt = m_duels.find(it->second);
    if (duelIt == m_duels.end())
        return false;

    const ActiveDuel& duel = duelIt->second;
    if (duel.state != DuelState::Active)
        return false;

    // Check if both players are in this duel
    return (duel.player1Guid == guid1 && duel.player2Guid == guid2) ||
           (duel.player1Guid == guid2 && duel.player2Guid == guid1);
}

bool DuelManager::isInDuel(uint32_t playerGuid) const
{
    auto it = m_playerToDuelKey.find(playerGuid);
    if (it == m_playerToDuelKey.end())
        return false;

    auto duelIt = m_duels.find(it->second);
    if (duelIt == m_duels.end())
        return false;

    return duelIt->second.state == DuelState::Active ||
           duelIt->second.state == DuelState::Countdown;
}

uint32_t DuelManager::getDuelOpponent(uint32_t playerGuid) const
{
    auto it = m_playerToDuelKey.find(playerGuid);
    if (it == m_playerToDuelKey.end())
        return 0;

    auto duelIt = m_duels.find(it->second);
    if (duelIt == m_duels.end())
        return 0;

    const ActiveDuel& duel = duelIt->second;
    if (duel.state != DuelState::Active)
        return 0;

    return (duel.player1Guid == playerGuid) ? duel.player2Guid : duel.player1Guid;
}

bool DuelManager::onDuelDamage(Player* player, int32_t newHealth)
{
    if (!player)
        return false;

    uint32_t playerGuid = player->getGuid();
    ActiveDuel* duel = findDuel(playerGuid);

    if (!duel || duel->state != DuelState::Active)
        return false;

    // Check if health dropped to duel end threshold
    if (newHealth <= DUEL_MIN_HEALTH)
    {
        // Determine winner (the other player)
        uint32_t winnerGuid = (duel->player1Guid == playerGuid) ? duel->player2Guid : duel->player1Guid;
        endDuel(duel, winnerGuid, false);
        return true;  // Duel ended
    }

    return false;
}

// ============================================================================
// Cleanup
// ============================================================================

void DuelManager::onPlayerDisconnect(uint32_t playerGuid)
{
    // Check for pending invite
    m_pendingInvites.erase(playerGuid);

    // Check for active duel
    ActiveDuel* duel = findDuel(playerGuid);
    if (duel)
    {
        cancelDuel(duel, true);
    }
}

void DuelManager::update(float deltaTime)
{
    // Enforce duel boundaries for active duels
    std::vector<uint32_t> duelsToCancel;
    for (auto& pair : m_duels)
    {
        ActiveDuel& duel = pair.second;
        if (duel.state != DuelState::Active)
            continue;

        Player* p1 = sWorldManager.getPlayer(duel.player1Guid);
        Player* p2 = sWorldManager.getPlayer(duel.player2Guid);
        if (!p1 || !p2 || p1->getMapId() != p2->getMapId())
        {
            duelsToCancel.push_back(pair.first);
            continue;
        }

        if (!p1->isInRange(duel.centerX, duel.centerY, DUEL_RANGE) ||
            !p2->isInRange(duel.centerX, duel.centerY, DUEL_RANGE))
        {
            duelsToCancel.push_back(pair.first);
        }
    }

    for (uint32_t key : duelsToCancel)
    {
        auto it = m_duels.find(key);
        if (it != m_duels.end())
        {
            cancelDuel(&it->second, false);
        }
    }

    // Process countdown duels
    m_countdownAccumulator += deltaTime;

    if (m_countdownAccumulator < 1.0f)
        return;

    m_countdownAccumulator -= 1.0f;

    // Iterate through duels and update countdowns
    std::vector<uint32_t> duelsToStart;

    for (auto& pair : m_duels)
    {
        ActiveDuel& duel = pair.second;

        if (duel.state != DuelState::Countdown)
            continue;

        duel.countdownRemaining--;

        // Get players
        Player* p1 = sWorldManager.getPlayer(duel.player1Guid);
        Player* p2 = sWorldManager.getPlayer(duel.player2Guid);

        if (!p1 || !p2)
        {
            // One player disconnected
            cancelDuel(&duel, true);
            continue;
        }

        // Send countdown notification
        uint8_t notifyType = 0;
        switch (duel.countdownRemaining)
        {
            case 2: notifyType = static_cast<uint8_t>(PlayerDefines::ChatError::DuelCount3); break;
            case 1: notifyType = static_cast<uint8_t>(PlayerDefines::ChatError::DuelCount2); break;
            case 0: notifyType = static_cast<uint8_t>(PlayerDefines::ChatError::DuelCount1); break;
        }

        if (notifyType != 0)
        {
            sendNotification(p1, notifyType);
            sendNotification(p2, notifyType);
        }

        if (duel.countdownRemaining <= 0)
        {
            duelsToStart.push_back(pair.first);
        }
    }

    // Start duels that finished countdown
    for (uint32_t key : duelsToStart)
    {
        auto it = m_duels.find(key);
        if (it != m_duels.end())
        {
            startDuel(&it->second);
        }
    }

    // Clean up expired invites
    int64_t now = std::time(nullptr);
    std::vector<uint32_t> expiredInvites;

    for (const auto& pair : m_pendingInvites)
    {
        if (now - pair.second.timestamp > DUEL_INVITE_TIMEOUT_S)
        {
            expiredInvites.push_back(pair.first);
        }
    }

    for (uint32_t targetGuid : expiredInvites)
    {
        m_pendingInvites.erase(targetGuid);
        LOG_DEBUG("Duel invite to %u expired", targetGuid);
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

void DuelManager::startCountdown(uint32_t player1Guid, uint32_t player2Guid)
{
    // Create duel entry
    uint32_t duelKey = std::min(player1Guid, player2Guid);

    ActiveDuel duel;
    duel.player1Guid = player1Guid;
    duel.player2Guid = player2Guid;
    duel.state = DuelState::Countdown;
    duel.countdownRemaining = DUEL_COUNTDOWN_S;

    // Calculate center point
    Player* p1 = sWorldManager.getPlayer(player1Guid);
    Player* p2 = sWorldManager.getPlayer(player2Guid);
    if (p1 && p2)
    {
        duel.centerX = (p1->getX() + p2->getX()) / 2.0f;
        duel.centerY = (p1->getY() + p2->getY()) / 2.0f;
    }

    m_duels[duelKey] = duel;
    m_playerToDuelKey[player1Guid] = duelKey;
    m_playerToDuelKey[player2Guid] = duelKey;

    // Send initial countdown notification
    if (p1)
        sendNotification(p1, static_cast<uint8_t>(PlayerDefines::ChatError::DuelCount3));
    if (p2)
        sendNotification(p2, static_cast<uint8_t>(PlayerDefines::ChatError::DuelCount3));

    LOG_INFO("Duel countdown started between players %u and %u", player1Guid, player2Guid);
}

void DuelManager::startDuel(ActiveDuel* duel)
{
    if (!duel)
        return;

    duel->state = DuelState::Active;
    duel->startTime = std::time(nullptr);

    Player* p1 = sWorldManager.getPlayer(duel->player1Guid);
    Player* p2 = sWorldManager.getPlayer(duel->player2Guid);

    // Send duel begin notification
    if (p1)
        sendNotification(p1, static_cast<uint8_t>(PlayerDefines::ChatError::DuelBegin));
    if (p2)
        sendNotification(p2, static_cast<uint8_t>(PlayerDefines::ChatError::DuelBegin));

    LOG_INFO("Duel started between %s and %s",
             p1 ? p1->getName().c_str() : "unknown",
             p2 ? p2->getName().c_str() : "unknown");
}

void DuelManager::endDuel(ActiveDuel* duel, uint32_t winnerGuid, bool surrender)
{
    if (!duel)
        return;

    Player* winner = sWorldManager.getPlayer(winnerGuid);
    uint32_t loserGuid = (duel->player1Guid == winnerGuid) ? duel->player2Guid : duel->player1Guid;
    Player* loser = sWorldManager.getPlayer(loserGuid);

    // Reset health after duel
    auto resetHealth = [](Player* player)
    {
        if (!player)
            return;

        int32_t maxHealth = player->getMaxHealth();
        player->setHealth(maxHealth);
        player->broadcastVariable(ObjDefines::Variable::Health, maxHealth);
    };

    resetHealth(winner);
    resetHealth(loser);

    // Announce winner via system message
    std::string winMessage;
    if (surrender)
    {
        winMessage = (loser ? loser->getName() : "Player") + " has yielded. " +
                     (winner ? winner->getName() : "Player") + " wins the duel!";
    }
    else
    {
        winMessage = (winner ? winner->getName() : "Player") + " wins the duel!";
    }

    // Send system chat to both players
    GP_Server_ChatMsg chatPacket;
    chatPacket.m_channelId = static_cast<uint8_t>(ChatDefines::Channels::System);
    chatPacket.m_fromGuid = 0;
    chatPacket.m_fromName = "";
    chatPacket.m_text = winMessage;

    StlBuffer packet;
    uint16_t opcode = chatPacket.getOpcode();
    packet << opcode;
    chatPacket.pack(packet);

    if (winner)
        winner->sendPacket(packet);
    if (loser)
        loser->sendPacket(packet);

    LOG_INFO("Duel ended: %s defeated %s%s",
             winner ? winner->getName().c_str() : "unknown",
             loser ? loser->getName().c_str() : "unknown",
             surrender ? " (surrender)" : "");

    // Clean up
    uint32_t duelKey = std::min(duel->player1Guid, duel->player2Guid);
    m_playerToDuelKey.erase(duel->player1Guid);
    m_playerToDuelKey.erase(duel->player2Guid);
    m_duels.erase(duelKey);
}

void DuelManager::cancelDuel(ActiveDuel* duel, bool disconnected)
{
    if (!duel)
        return;

    Player* p1 = sWorldManager.getPlayer(duel->player1Guid);
    Player* p2 = sWorldManager.getPlayer(duel->player2Guid);

    std::string message = disconnected ? "Duel cancelled: opponent disconnected." : "Duel cancelled.";

    GP_Server_ChatMsg chatPacket;
    chatPacket.m_channelId = static_cast<uint8_t>(ChatDefines::Channels::System);
    chatPacket.m_fromGuid = 0;
    chatPacket.m_fromName = "";
    chatPacket.m_text = message;

    StlBuffer packet;
    uint16_t opcode = chatPacket.getOpcode();
    packet << opcode;
    chatPacket.pack(packet);

    if (p1)
        p1->sendPacket(packet);
    if (p2)
        p2->sendPacket(packet);

    // Clean up
    uint32_t duelKey = std::min(duel->player1Guid, duel->player2Guid);
    m_playerToDuelKey.erase(duel->player1Guid);
    m_playerToDuelKey.erase(duel->player2Guid);
    m_duels.erase(duelKey);
}

void DuelManager::sendNotification(Player* player, uint8_t notifyType)
{
    if (!player)
        return;

    GP_Server_ChatError packet;
    packet.m_code = notifyType;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

void DuelManager::sendOfferDuel(Player* target, Player* challenger)
{
    if (!target || !challenger)
        return;

    GP_Server_OfferDuel packet;
    packet.m_challengerGuid = challenger->getGuid();
    packet.m_challengerName = challenger->getName();

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    target->sendPacket(buf);
}

ActiveDuel* DuelManager::findDuel(uint32_t playerGuid)
{
    auto it = m_playerToDuelKey.find(playerGuid);
    if (it == m_playerToDuelKey.end())
        return nullptr;

    auto duelIt = m_duels.find(it->second);
    if (duelIt == m_duels.end())
        return nullptr;

    return &duelIt->second;
}

}  // namespace Duel

// ============================================================================
// Packet Handlers
// ============================================================================

#include "GamePacketClient.h"

namespace DuelHandlers
{

void handleDuelResponse(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_DuelResponse packet;
    packet.unpack(data);

    sDuelManager.respondToDuel(player, packet.m_accept);
}

void handleYieldDuel(Session& session, StlBuffer& data)
{
    (void)data;  // No data in this packet

    Player* player = session.getPlayer();
    if (!player)
        return;

    sDuelManager.yieldDuel(player);
}

}  // namespace DuelHandlers
