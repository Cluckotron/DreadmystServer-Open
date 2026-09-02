// PartySystem - Manages player parties
// Phase 8, Task 8.4: Party System

#include "stdafx.h"
#include "Systems/PartySystem.h"
#include "Systems/ChatSystem.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Network/Session.h"
#include "Core/Logger.h"
#include "PlayerDefines.h"

#include "GamePacketClient.h"
#include "GamePacketServer.h"

#include <ctime>
#include <algorithm>

namespace Party
{

// Invite expiration time in seconds
constexpr int64_t INVITE_EXPIRE_SECONDS = 60;

// ============================================================================
// PartyData Methods
// ============================================================================

bool PartyData::isMember(uint32_t guid) const
{
    return std::find(memberGuids.begin(), memberGuids.end(), guid) != memberGuids.end();
}

// ============================================================================
// PartyManager Singleton
// ============================================================================

PartyManager& PartyManager::instance()
{
    static PartyManager inst;
    return inst;
}

// ============================================================================
// Invite Flow
// ============================================================================

void PartyManager::invitePlayer(Player* inviter, const std::string& targetName)
{
    if (!inviter)
        return;

    // Find target by name
    Player* target = sWorldManager.getPlayerByName(targetName);
    if (!target)
    {
        sChatManager.sendSystemMessage(inviter, "Player not found.");
        return;
    }

    // Can't invite yourself
    if (target == inviter)
    {
        sChatManager.sendSystemMessage(inviter, "You cannot invite yourself.");
        return;
    }

    // Check if target is already in a party
    if (isInParty(target->getGuid()))
    {
        sChatManager.sendSystemMessage(inviter, target->getName() + " is already in a party.");
        return;
    }

    // Check if inviter is in a party they don't lead
    PartyData* inviterParty = getPartyByGuid(inviter->getGuid());
    if (inviterParty && !inviterParty->isLeader(inviter->getGuid()))
    {
        sChatManager.sendSystemMessage(inviter, "Only the party leader can invite players.");
        return;
    }

    // Check if party is full
    if (inviterParty && inviterParty->isFull())
    {
        sChatManager.sendSystemMessage(inviter, "Your party is full.");
        return;
    }

    // Check if target already has a pending invite
    auto it = m_pendingInvites.find(target->getGuid());
    if (it != m_pendingInvites.end())
    {
        sChatManager.sendSystemMessage(inviter, target->getName() + " already has a pending party invite.");
        return;
    }

    // Create pending invite
    PendingInvite invite;
    invite.inviterGuid = inviter->getGuid();
    invite.inviterName = inviter->getName();
    invite.expireTime = std::time(nullptr) + INVITE_EXPIRE_SECONDS;
    m_pendingInvites[target->getGuid()] = invite;

    // Send offer to target
    GP_Server_OfferParty packet;
    packet.m_inviterGuid = inviter->getGuid();
    packet.m_inviterName = inviter->getName();

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    target->sendPacket(buf);

    sChatManager.sendSystemMessage(inviter, "Party invite sent to " + target->getName() + ".");
    LOG_DEBUG("Player %s invited %s to party",
              inviter->getName().c_str(), target->getName().c_str());
}

void PartyManager::respondToInvite(Player* target, bool accept)
{
    if (!target)
        return;

    // Find pending invite
    auto it = m_pendingInvites.find(target->getGuid());
    if (it == m_pendingInvites.end())
    {
        sChatManager.sendSystemMessage(target, "No pending party invite.");
        return;
    }

    PendingInvite invite = it->second;
    m_pendingInvites.erase(it);

    // Find inviter
    Player* inviter = sWorldManager.getPlayer(invite.inviterGuid);

    if (!accept)
    {
        // Declined
        if (inviter)
        {
            sChatManager.sendSystemMessage(inviter, target->getName() + " declined your party invite.");
        }
        LOG_DEBUG("Player %s declined party invite from %s",
                  target->getName().c_str(), invite.inviterName.c_str());
        return;
    }

    // Accepted - check if inviter is still available
    if (!inviter)
    {
        sChatManager.sendSystemMessage(target, "The inviter is no longer online.");
        return;
    }

    // Check if target somehow got into a party
    if (isInParty(target->getGuid()))
    {
        sChatManager.sendSystemMessage(target, "You are already in a party.");
        return;
    }

    // Check if inviter's party is now full
    PartyData* inviterParty = getPartyByGuid(inviter->getGuid());
    if (inviterParty && inviterParty->isFull())
    {
        sChatManager.sendSystemMessage(target, "The party is now full.");
        if (inviter)
        {
            sChatManager.sendSystemMessage(inviter, "Party is full, " + target->getName() + " could not join.");
        }
        return;
    }

    // Join or create party
    if (inviterParty)
    {
        // Add to existing party
        addToParty(inviterParty, target);
    }
    else
    {
        // Create new party
        createParty(inviter, target);
    }

    LOG_DEBUG("Player %s joined party with %s",
              target->getName().c_str(), inviter->getName().c_str());
}

// ============================================================================
// Party Management
// ============================================================================

void PartyManager::leaveParty(Player* player)
{
    if (!player)
        return;

    PartyData* party = getPartyByGuid(player->getGuid());
    if (!party)
    {
        sChatManager.sendSystemMessage(player, "You are not in a party.");
        return;
    }

    removeFromParty(party, player->getGuid());

    // Send empty party list to the leaving player
    sendPartyList(player, nullptr);

    sChatManager.sendSystemMessage(player, "You have left the party.");
    LOG_DEBUG("Player %s left party", player->getName().c_str());
}

void PartyManager::kickMember(Player* leader, uint32_t targetGuid)
{
    if (!leader)
        return;

    PartyData* party = getPartyByGuid(leader->getGuid());
    if (!party)
    {
        sChatManager.sendSystemMessage(leader, "You are not in a party.");
        return;
    }

    if (!party->isLeader(leader->getGuid()))
    {
        sChatManager.sendSystemMessage(leader, "Only the party leader can kick members.");
        return;
    }

    // Can't kick yourself
    if (targetGuid == leader->getGuid())
    {
        sChatManager.sendSystemMessage(leader, "You cannot kick yourself. Use leave party instead.");
        return;
    }

    // Check if target is in the party
    if (!party->isMember(targetGuid))
    {
        sChatManager.sendSystemMessage(leader, "That player is not in your party.");
        return;
    }

    // Get target player for messaging
    Player* target = sWorldManager.getPlayer(targetGuid);
    std::string targetName = target ? target->getName() : "Unknown";

    // Remove from party
    removeFromParty(party, targetGuid);

    // Send empty party list to kicked player
    if (target)
    {
        sendPartyList(target, nullptr);
        sChatManager.sendSystemMessage(target, "You have been removed from the party.");
    }

    // Notify leader
    sChatManager.sendSystemMessage(leader, targetName + " has been removed from the party.");
    LOG_DEBUG("Player %s kicked %s from party", leader->getName().c_str(), targetName.c_str());
}

void PartyManager::promoteLeader(Player* leader, uint32_t targetGuid)
{
    if (!leader)
        return;

    PartyData* party = getPartyByGuid(leader->getGuid());
    if (!party)
    {
        sChatManager.sendSystemMessage(leader, "You are not in a party.");
        return;
    }

    if (!party->isLeader(leader->getGuid()))
    {
        sChatManager.sendSystemMessage(leader, "Only the party leader can promote members.");
        return;
    }

    // Can't promote yourself
    if (targetGuid == leader->getGuid())
    {
        sChatManager.sendSystemMessage(leader, "You are already the party leader.");
        return;
    }

    // Check if target is in the party
    if (!party->isMember(targetGuid))
    {
        sChatManager.sendSystemMessage(leader, "That player is not in your party.");
        return;
    }

    // Promote
    party->leaderGuid = targetGuid;

    // Broadcast updated party list
    broadcastPartyList(party);

    // Get target for messaging
    Player* target = sWorldManager.getPlayer(targetGuid);
    if (target)
    {
        sChatManager.sendSystemMessage(target, "You are now the party leader.");
    }
    sChatManager.sendSystemMessage(leader, "You have promoted " +
        (target ? target->getName() : "a player") + " to party leader.");

    LOG_DEBUG("Player %s promoted %u to party leader",
              leader->getName().c_str(), targetGuid);
}

// ============================================================================
// Party Queries
// ============================================================================

PartyData* PartyManager::getParty(Player* player)
{
    if (!player)
        return nullptr;
    return getPartyByGuid(player->getGuid());
}

PartyData* PartyManager::getPartyByGuid(uint32_t playerGuid)
{
    auto it = m_playerPartyMap.find(playerGuid);
    if (it == m_playerPartyMap.end())
        return nullptr;

    auto partyIt = m_parties.find(it->second);
    if (partyIt == m_parties.end())
        return nullptr;

    return partyIt->second.get();
}

std::vector<Player*> PartyManager::getPartyMembers(Player* player)
{
    std::vector<Player*> result;

    PartyData* party = getParty(player);
    if (!party)
        return result;

    for (uint32_t guid : party->memberGuids)
    {
        Player* member = sWorldManager.getPlayer(guid);
        if (member)
        {
            result.push_back(member);
        }
    }

    return result;
}

bool PartyManager::isInParty(uint32_t playerGuid) const
{
    return m_playerPartyMap.find(playerGuid) != m_playerPartyMap.end();
}

// ============================================================================
// Chat Integration
// ============================================================================

void PartyManager::sendPartyMessage(Player* sender, const std::string& message)
{
    if (!sender)
        return;

    PartyData* party = getParty(sender);
    if (!party)
    {
        sChatManager.sendSystemMessage(sender, "You are not in a party.");
        return;
    }

    // Send to all party members
    for (uint32_t guid : party->memberGuids)
    {
        Player* member = sWorldManager.getPlayer(guid);
        if (member)
        {
            // Use ChatSystem to send the message
            GP_Server_ChatMsg packet;
            packet.m_channelId = static_cast<uint8_t>(ChatDefines::Channels::Party);
            packet.m_fromGuid = sender->getGuid();
            packet.m_fromName = sender->getName();
            packet.m_text = message;

            StlBuffer buf;
            uint16_t opcode = packet.getOpcode();
            buf << opcode;
            packet.pack(buf);
            member->sendPacket(buf);
        }
    }
}

// ============================================================================
// Cleanup
// ============================================================================

void PartyManager::onPlayerDisconnect(uint32_t playerGuid)
{
    // Remove any pending invites for/from this player
    m_pendingInvites.erase(playerGuid);

    // Remove invites from this player
    for (auto it = m_pendingInvites.begin(); it != m_pendingInvites.end(); )
    {
        if (it->second.inviterGuid == playerGuid)
        {
            it = m_pendingInvites.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Remove from party if in one
    PartyData* party = getPartyByGuid(playerGuid);
    if (party)
    {
        removeFromParty(party, playerGuid);
    }
}

void PartyManager::update()
{
    // Expire old invites
    int64_t now = std::time(nullptr);

    for (auto it = m_pendingInvites.begin(); it != m_pendingInvites.end(); )
    {
        if (it->second.expireTime <= now)
        {
            // Notify target if online
            Player* target = sWorldManager.getPlayer(it->first);
            if (target)
            {
                sChatManager.sendSystemMessage(target, "Party invite from " +
                    it->second.inviterName + " has expired.");
            }
            it = m_pendingInvites.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// ============================================================================
// Private Helpers
// ============================================================================

void PartyManager::createParty(Player* leader, Player* member)
{
    auto party = std::make_unique<PartyData>();
    party->id = m_nextPartyId++;
    party->leaderGuid = leader->getGuid();
    party->memberGuids.push_back(leader->getGuid());
    party->memberGuids.push_back(member->getGuid());

    // Update mappings
    m_playerPartyMap[leader->getGuid()] = party->id;
    m_playerPartyMap[member->getGuid()] = party->id;

    PartyData* partyPtr = party.get();
    m_parties[party->id] = std::move(party);

    // Broadcast party list to both
    broadcastPartyList(partyPtr);

    sChatManager.sendSystemMessage(leader, "Party formed with " + member->getName() + ".");
    sChatManager.sendSystemMessage(member, "You have joined " + leader->getName() + "'s party.");

    LOG_INFO("Party %u created: leader=%s, member=%s",
             partyPtr->id, leader->getName().c_str(), member->getName().c_str());
}

void PartyManager::addToParty(PartyData* party, Player* player)
{
    if (!party || !player)
        return;

    party->memberGuids.push_back(player->getGuid());
    m_playerPartyMap[player->getGuid()] = party->id;

    // Broadcast updated list
    broadcastPartyList(party);

    // Notify all members
    for (uint32_t guid : party->memberGuids)
    {
        Player* member = sWorldManager.getPlayer(guid);
        if (member && member != player)
        {
            sChatManager.sendSystemMessage(member, player->getName() + " has joined the party.");
        }
    }
    sChatManager.sendSystemMessage(player, "You have joined the party.");
}

void PartyManager::removeFromParty(PartyData* party, uint32_t playerGuid)
{
    if (!party)
        return;

    // Remove from member list
    auto it = std::find(party->memberGuids.begin(), party->memberGuids.end(), playerGuid);
    if (it != party->memberGuids.end())
    {
        party->memberGuids.erase(it);
    }

    // Remove from mapping
    m_playerPartyMap.erase(playerGuid);

    // Get player name for messaging
    Player* leavingPlayer = sWorldManager.getPlayer(playerGuid);
    std::string leavingName = leavingPlayer ? leavingPlayer->getName() : "A player";

    // If party is now empty or has only 1 member, disband
    if (party->memberGuids.size() <= 1)
    {
        disbandParty(party);
        return;
    }

    // If leader left, promote first remaining member
    if (party->leaderGuid == playerGuid)
    {
        party->leaderGuid = party->memberGuids.front();
        Player* newLeader = sWorldManager.getPlayer(party->leaderGuid);
        if (newLeader)
        {
            sChatManager.sendSystemMessage(newLeader, "You are now the party leader.");
        }
    }

    // Broadcast updated list to remaining members
    broadcastPartyList(party);

    // Notify remaining members
    for (uint32_t guid : party->memberGuids)
    {
        Player* member = sWorldManager.getPlayer(guid);
        if (member)
        {
            sChatManager.sendSystemMessage(member, leavingName + " has left the party.");
        }
    }
}

void PartyManager::disbandParty(PartyData* party)
{
    if (!party)
        return;

    LOG_INFO("Party %u disbanded", party->id);

    // Notify all remaining members and clear their party
    for (uint32_t guid : party->memberGuids)
    {
        m_playerPartyMap.erase(guid);

        Player* member = sWorldManager.getPlayer(guid);
        if (member)
        {
            sendPartyList(member, nullptr);
            sChatManager.sendSystemMessage(member, "Your party has been disbanded.");
        }
    }

    // Remove party
    m_parties.erase(party->id);
}

void PartyManager::broadcastPartyList(PartyData* party)
{
    if (!party)
        return;

    for (uint32_t guid : party->memberGuids)
    {
        Player* member = sWorldManager.getPlayer(guid);
        if (member)
        {
            sendPartyList(member, party);
        }
    }
}

void PartyManager::sendPartyList(Player* player, PartyData* party)
{
    if (!player)
        return;

    GP_Server_PartyList packet;

    if (party)
    {
        packet.m_leaderGuid = party->leaderGuid;
        for (uint32_t guid : party->memberGuids)
        {
            packet.m_members.push_back(static_cast<int>(guid));
        }
    }
    // If party is nullptr, we send an empty list (player not in party)

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

// ============================================================================
// Packet Handlers
// ============================================================================

void handlePartyInvite(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_PartyInviteMember packet;
    packet.unpack(data);

    LOG_DEBUG("Player %s inviting '%s' to party",
              player->getName().c_str(), packet.m_playerName.c_str());

    sPartyManager.invitePlayer(player, packet.m_playerName);
}

void handlePartyInviteResponse(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_PartyInviteResponse packet;
    packet.unpack(data);

    LOG_DEBUG("Player %s %s party invite",
              player->getName().c_str(), packet.m_accept ? "accepted" : "declined");

    sPartyManager.respondToInvite(player, packet.m_accept);
}

void handlePartyChanges(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_PartyChanges packet;
    packet.unpack(data);

    LOG_DEBUG("Player %s party change: type=%d, target=%u",
              player->getName().c_str(), packet.m_changeType, packet.m_targetGuid);

    switch (static_cast<ChangeType>(packet.m_changeType))
    {
        case ChangeType::Leave:
            sPartyManager.leaveParty(player);
            break;
        case ChangeType::Kick:
            sPartyManager.kickMember(player, packet.m_targetGuid);
            break;
        case ChangeType::Promote:
            sPartyManager.promoteLeader(player, packet.m_targetGuid);
            break;
        default:
            LOG_WARN("Unknown party change type: %d", packet.m_changeType);
            break;
    }
}

} // namespace Party
