// GuildSystem - Guild management implementation
// Task 8.6: Guild System

#include "stdafx.h"
#include "Systems/GuildSystem.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Database/DatabaseManager.h"
#include "Core/Logger.h"
#include "GamePacketServer.h"
#include "ChatDefines.h"
#include <algorithm>
#include <cctype>

namespace Guild
{

// ============================================================================
// GuildData Helper Methods
// ============================================================================

GuildMember* GuildData::getMember(uint32_t guid)
{
    for (auto& m : members)
    {
        if (m.characterGuid == guid)
            return &m;
    }
    return nullptr;
}

const GuildMember* GuildData::getMember(uint32_t guid) const
{
    for (const auto& m : members)
    {
        if (m.characterGuid == guid)
            return &m;
    }
    return nullptr;
}

bool GuildData::isMember(uint32_t guid) const
{
    return getMember(guid) != nullptr;
}

bool GuildData::isLeader(uint32_t guid) const
{
    return guid == leaderGuid;
}

bool GuildData::isOfficer(uint32_t guid) const
{
    const auto* member = getMember(guid);
    return member && member->rank >= Rank::Officer;
}

bool GuildData::canInvite(uint32_t guid) const
{
    const auto* member = getMember(guid);
    return member && member->rank >= Rank::Officer;
}

bool GuildData::canKick(uint32_t guid) const
{
    const auto* member = getMember(guid);
    return member && member->rank >= Rank::Officer;
}

bool GuildData::canPromote(uint32_t guid) const
{
    return isLeader(guid);
}

// ============================================================================
// GuildManager Singleton
// ============================================================================

GuildManager& GuildManager::instance()
{
    static GuildManager instance;
    return instance;
}

// ============================================================================
// Initialization
// ============================================================================

void GuildManager::loadGuildsFromDatabase()
{
    LOG_INFO("Loading guilds from database...");

    m_guilds.clear();
    m_playerToGuild.clear();

    // Load all guilds
    auto result = sDatabase.query("SELECT id, name, leader_guid, motd FROM guilds");
    if (!result.success())
    {
        LOG_ERROR("Failed to load guilds from database");
        return;
    }

    for (const auto& row : result)
    {
        auto guild = std::make_unique<GuildData>();
        guild->id = row.getInt("id");
        guild->name = row.getString("name");
        guild->leaderGuid = static_cast<uint32_t>(row.getInt("leader_guid"));
        guild->motd = row.getString("motd");

        if (guild->id >= m_nextGuildId)
            m_nextGuildId = guild->id + 1;

        m_guilds.push_back(std::move(guild));
    }

    // Load all guild members
    auto memberResult = sDatabase.query(
        "SELECT gm.guild_id, gm.character_guid, gm.rank, c.name, c.level, c.class_id "
        "FROM guild_members gm "
        "JOIN characters c ON c.guid = gm.character_guid"
    );
    if (!memberResult.success())
    {
        LOG_ERROR("Failed to load guild members from database");
        return;
    }

    for (const auto& row : memberResult)
    {
        int32_t guildId = row.getInt("guild_id");
        GuildData* guild = getGuildById(guildId);
        if (!guild)
            continue;

        GuildMember member;
        member.characterGuid = static_cast<uint32_t>(row.getInt("character_guid"));
        member.name = row.getString("name");
        member.rank = static_cast<Rank>(row.getInt("rank"));
        member.level = row.getInt("level");
        member.classId = static_cast<uint8_t>(row.getInt("class_id"));
        member.online = false;  // Will be set when player logs in

        guild->members.push_back(member);
        m_playerToGuild[member.characterGuid] = guildId;
    }

    LOG_INFO("Loaded %zu guilds with members", m_guilds.size());
}

// ============================================================================
// Guild Operations
// ============================================================================

void GuildManager::createGuild(Player* creator, const std::string& guildName)
{
    if (!creator)
        return;

    uint32_t creatorGuid = creator->getGuid();

    // Check if already in a guild
    if (isInGuild(creatorGuid))
    {
        sendGuildError(creator, GuildError::AlreadyInGuild);
        return;
    }

    // Validate guild name
    if (!isValidGuildName(guildName))
    {
        sendGuildError(creator, GuildError::NameInvalid);
        return;
    }

    // Check if name is already taken
    for (const auto& g : m_guilds)
    {
        if (strcasecmp(g->name.c_str(), guildName.c_str()) == 0)
        {
            sendGuildError(creator, GuildError::NameTaken);
            return;
        }
    }

    // Create guild in database
    int32_t guildId = createGuildInDatabase(guildName, creatorGuid);
    if (guildId <= 0)
    {
        LOG_ERROR("Failed to create guild '%s' in database", guildName.c_str());
        return;
    }

    // Add member to database
    addMemberToDatabase(guildId, creatorGuid, Rank::Leader);

    // Create in-memory guild
    auto guild = std::make_unique<GuildData>();
    guild->id = guildId;
    guild->name = guildName;
    guild->leaderGuid = creatorGuid;

    // Add creator as leader
    GuildMember member = createMemberFromPlayer(creator, Rank::Leader);
    member.online = true;
    guild->members.push_back(member);

    m_playerToGuild[creatorGuid] = guildId;

    m_guilds.push_back(std::move(guild));

    LOG_INFO("Guild '%s' created by player %s (guid: %u)", guildName.c_str(),
             creator->getName().c_str(), creatorGuid);

    // Send roster to creator
    sendRoster(creator);
}

void GuildManager::disbandGuild(Player* leader)
{
    if (!leader)
        return;

    uint32_t leaderGuid = leader->getGuid();
    GuildData* guild = getGuildByPlayer(leaderGuid);

    if (!guild)
    {
        sendGuildError(leader, GuildError::NotInGuild);
        return;
    }

    if (!guild->isLeader(leaderGuid))
    {
        sendGuildError(leader, GuildError::NotLeader);
        return;
    }

    int32_t guildId = guild->id;
    std::string guildName = guild->name;

    // Notify all online members (send empty roster or remove member packet)
    for (const auto& member : guild->members)
    {
        m_playerToGuild.erase(member.characterGuid);

        if (member.online)
        {
            if (Player* p = sWorldManager.getPlayer(member.characterGuid))
            {
                // Send remove member for self to indicate guild disbanded
                GP_Server_GuildRemoveMember removePacket;
                removePacket.m_memberGuid = member.characterGuid;
                StlBuffer buf;
                uint16_t opcode = removePacket.getOpcode();
                buf << opcode;
                removePacket.pack(buf);
                p->sendPacket(buf);
            }
        }
    }

    // Delete from database
    deleteGuildFromDatabase(guildId);

    // Remove from memory
    m_guilds.erase(
        std::remove_if(m_guilds.begin(), m_guilds.end(),
            [guildId](const std::unique_ptr<GuildData>& g) { return g->id == guildId; }),
        m_guilds.end()
    );

    LOG_INFO("Guild '%s' (id: %d) disbanded by leader %s", guildName.c_str(),
             guildId, leader->getName().c_str());
}

void GuildManager::setMotd(Player* player, const std::string& motd)
{
    if (!player)
        return;

    uint32_t playerGuid = player->getGuid();
    GuildData* guild = getGuildByPlayer(playerGuid);

    if (!guild)
    {
        sendGuildError(player, GuildError::NotInGuild);
        return;
    }

    if (!guild->isOfficer(playerGuid))
    {
        sendGuildError(player, GuildError::NotOfficer);
        return;
    }

    // Truncate if too long
    std::string newMotd = motd.substr(0, MAX_MOTD_LENGTH);
    guild->motd = newMotd;

    // Update database
    updateMotdInDatabase(guild->id, newMotd);

    // Broadcast updated roster to all online members
    for (const auto& member : guild->members)
    {
        if (member.online)
        {
            if (Player* p = sWorldManager.getPlayer(member.characterGuid))
            {
                sendRoster(p);
            }
        }
    }
}

// ============================================================================
// Member Operations
// ============================================================================

void GuildManager::invitePlayer(Player* inviter, const std::string& targetName)
{
    if (!inviter)
        return;

    uint32_t inviterGuid = inviter->getGuid();
    GuildData* guild = getGuildByPlayer(inviterGuid);

    if (!guild)
    {
        sendGuildError(inviter, GuildError::NotInGuild);
        return;
    }

    if (!guild->canInvite(inviterGuid))
    {
        sendGuildError(inviter, GuildError::NotOfficer);
        return;
    }

    if (guild->members.size() >= MAX_GUILD_MEMBERS)
    {
        sendGuildError(inviter, GuildError::GuildFull);
        return;
    }

    // Find target player (must be online)
    Player* target = sWorldManager.getPlayerByName(targetName);
    if (!target)
    {
        sendGuildError(inviter, GuildError::PlayerNotFound);
        return;
    }

    uint32_t targetGuid = target->getGuid();

    if (isInGuild(targetGuid))
    {
        sendGuildError(inviter, GuildError::PlayerAlreadyInGuild);
        return;
    }

    // Check for existing invite
    if (m_pendingInvites.count(targetGuid))
    {
        // Already has a pending invite, overwrite it
        m_pendingInvites.erase(targetGuid);
    }

    // Create pending invite
    PendingInvite invite;
    invite.guildId = guild->id;
    invite.inviterGuid = inviterGuid;
    invite.timestamp = std::time(nullptr);
    m_pendingInvites[targetGuid] = invite;

    // Send invite packet to target
    GP_Server_GuildInvite invitePacket;
    invitePacket.m_guildId = guild->id;
    invitePacket.m_guildName = guild->name;
    invitePacket.m_inviterName = inviter->getName();
    StlBuffer buf;
    uint16_t opcode = invitePacket.getOpcode();
    buf << opcode;
    invitePacket.pack(buf);
    target->sendPacket(buf);

    LOG_DEBUG("Guild invite sent: %s invites %s to %s",
              inviter->getName().c_str(), targetName.c_str(), guild->name.c_str());
}

void GuildManager::respondToInvite(Player* target, int32_t guildId, bool accept)
{
    if (!target)
        return;

    uint32_t targetGuid = target->getGuid();

    // Check for pending invite
    auto it = m_pendingInvites.find(targetGuid);
    if (it == m_pendingInvites.end())
    {
        LOG_DEBUG("No pending guild invite for player %u", targetGuid);
        return;
    }

    PendingInvite invite = it->second;
    m_pendingInvites.erase(it);

    // Verify guild ID matches
    if (invite.guildId != guildId)
    {
        LOG_DEBUG("Guild invite mismatch: expected %d, got %d", invite.guildId, guildId);
        return;
    }

    if (!accept)
    {
        LOG_DEBUG("Player %s declined guild invite", target->getName().c_str());
        return;
    }

    // Check if already in a guild (might have joined another)
    if (isInGuild(targetGuid))
    {
        sendGuildError(target, GuildError::AlreadyInGuild);
        return;
    }

    GuildData* guild = getGuildById(guildId);
    if (!guild)
    {
        LOG_DEBUG("Guild %d no longer exists", guildId);
        return;
    }

    if (guild->members.size() >= MAX_GUILD_MEMBERS)
    {
        sendGuildError(target, GuildError::GuildFull);
        return;
    }

    // Add member to database
    addMemberToDatabase(guildId, targetGuid, Rank::Member);

    // Add to in-memory guild
    GuildMember member = createMemberFromPlayer(target, Rank::Member);
    member.online = true;
    guild->members.push_back(member);
    m_playerToGuild[targetGuid] = guildId;

    LOG_INFO("Player %s joined guild %s", target->getName().c_str(), guild->name.c_str());

    // Broadcast to other guild members
    broadcastAddMember(guildId, targetGuid, target->getName());

    // Send full roster to new member
    sendRoster(target);
}

void GuildManager::leaveGuild(Player* player)
{
    if (!player)
        return;

    uint32_t playerGuid = player->getGuid();
    GuildData* guild = getGuildByPlayer(playerGuid);

    if (!guild)
    {
        sendGuildError(player, GuildError::NotInGuild);
        return;
    }

    // Leader cannot leave - must disband or promote someone first
    if (guild->isLeader(playerGuid))
    {
        if (guild->members.size() > 1)
        {
            // There are other members - promote someone first
            sendGuildError(player, GuildError::NotLeader);  // Reuse this error
            return;
        }
        else
        {
            // Last member - disband the guild
            disbandGuild(player);
            return;
        }
    }

    int32_t guildId = guild->id;
    std::string playerName = player->getName();

    // Remove from database
    removeMemberFromDatabase(guildId, playerGuid);

    // Remove from in-memory
    guild->members.erase(
        std::remove_if(guild->members.begin(), guild->members.end(),
            [playerGuid](const GuildMember& m) { return m.characterGuid == playerGuid; }),
        guild->members.end()
    );
    m_playerToGuild.erase(playerGuid);

    // Broadcast removal to other members
    broadcastRemoveMember(guildId, playerGuid, playerName);

    // Send remove to the leaving player
    GP_Server_GuildRemoveMember removePacket;
    removePacket.m_memberGuid = playerGuid;
    removePacket.m_playerName = playerName;
    StlBuffer buf;
    uint16_t opcode = removePacket.getOpcode();
    buf << opcode;
    removePacket.pack(buf);
    player->sendPacket(buf);

    LOG_INFO("Player %s left guild (id: %d)", playerName.c_str(), guildId);
}

void GuildManager::kickMember(Player* kicker, uint32_t targetGuid)
{
    if (!kicker)
        return;

    uint32_t kickerGuid = kicker->getGuid();
    GuildData* guild = getGuildByPlayer(kickerGuid);

    if (!guild)
    {
        sendGuildError(kicker, GuildError::NotInGuild);
        return;
    }

    if (!guild->canKick(kickerGuid))
    {
        sendGuildError(kicker, GuildError::NotOfficer);
        return;
    }

    // Cannot kick leader
    if (guild->isLeader(targetGuid))
    {
        sendGuildError(kicker, GuildError::CannotKickLeader);
        return;
    }

    // Find target member
    GuildMember* targetMember = guild->getMember(targetGuid);
    if (!targetMember)
    {
        sendGuildError(kicker, GuildError::PlayerNotFound);
        return;
    }

    std::string targetName = targetMember->name;

    // Officers can only kick members, not other officers
    if (!guild->isLeader(kickerGuid) && targetMember->rank >= Rank::Officer)
    {
        sendGuildError(kicker, GuildError::NoPermission);
        return;
    }

    int32_t guildId = guild->id;
    bool wasOnline = targetMember->online;

    // Remove from database
    removeMemberFromDatabase(guildId, targetGuid);

    // Remove from in-memory
    guild->members.erase(
        std::remove_if(guild->members.begin(), guild->members.end(),
            [targetGuid](const GuildMember& m) { return m.characterGuid == targetGuid; }),
        guild->members.end()
    );
    m_playerToGuild.erase(targetGuid);

    // Broadcast removal to guild
    broadcastRemoveMember(guildId, targetGuid, targetName);

    // Notify kicked player if online
    if (wasOnline)
    {
        if (Player* target = sWorldManager.getPlayer(targetGuid))
        {
            GP_Server_GuildRemoveMember removePacket;
            removePacket.m_memberGuid = targetGuid;
            removePacket.m_playerName = targetName;
            StlBuffer buf;
            uint16_t opcode = removePacket.getOpcode();
            buf << opcode;
            removePacket.pack(buf);
            target->sendPacket(buf);
        }
    }

    LOG_INFO("Player %u kicked from guild (id: %d) by %s",
             targetGuid, guildId, kicker->getName().c_str());
}

void GuildManager::promoteMember(Player* promoter, uint32_t targetGuid)
{
    if (!promoter)
        return;

    uint32_t promoterGuid = promoter->getGuid();
    GuildData* guild = getGuildByPlayer(promoterGuid);

    if (!guild)
    {
        sendGuildError(promoter, GuildError::NotInGuild);
        return;
    }

    if (!guild->canPromote(promoterGuid))
    {
        sendGuildError(promoter, GuildError::NotLeader);
        return;
    }

    GuildMember* target = guild->getMember(targetGuid);
    if (!target)
    {
        sendGuildError(promoter, GuildError::PlayerNotFound);
        return;
    }

    // Cannot promote above Officer (Leader must be transferred)
    if (target->rank >= Rank::Officer)
    {
        // Promote to Leader means transfer leadership
        if (target->rank == Rank::Officer)
        {
            // Transfer leadership
            GuildMember* oldLeader = guild->getMember(promoterGuid);
            if (oldLeader)
            {
                oldLeader->rank = Rank::Officer;
                updateMemberRankInDatabase(guild->id, promoterGuid, Rank::Officer);
                broadcastRoleChange(guild->id, promoterGuid, Rank::Officer);
            }

            target->rank = Rank::Leader;
            guild->leaderGuid = targetGuid;
            updateMemberRankInDatabase(guild->id, targetGuid, Rank::Leader);
            updateLeaderInDatabase(guild->id, targetGuid);
            broadcastRoleChange(guild->id, targetGuid, Rank::Leader);

            LOG_INFO("Guild leadership transferred to %s in guild %d",
                     target->name.c_str(), guild->id);
        }
        else
        {
            sendGuildError(promoter, GuildError::CannotPromoteAboveRank);
        }
        return;
    }

    // Promote Member -> Officer
    Rank newRank = static_cast<Rank>(static_cast<int>(target->rank) + 1);
    target->rank = newRank;

    updateMemberRankInDatabase(guild->id, targetGuid, newRank);
    broadcastRoleChange(guild->id, targetGuid, newRank);

    LOG_DEBUG("Player %s promoted to rank %d in guild %d",
              target->name.c_str(), static_cast<int>(newRank), guild->id);
}

void GuildManager::demoteMember(Player* demoter, uint32_t targetGuid)
{
    if (!demoter)
        return;

    uint32_t demoterGuid = demoter->getGuid();
    GuildData* guild = getGuildByPlayer(demoterGuid);

    if (!guild)
    {
        sendGuildError(demoter, GuildError::NotInGuild);
        return;
    }

    if (!guild->isLeader(demoterGuid))
    {
        sendGuildError(demoter, GuildError::NotLeader);
        return;
    }

    // Cannot demote leader
    if (guild->isLeader(targetGuid))
    {
        sendGuildError(demoter, GuildError::CannotDemoteLeader);
        return;
    }

    GuildMember* target = guild->getMember(targetGuid);
    if (!target)
    {
        sendGuildError(demoter, GuildError::PlayerNotFound);
        return;
    }

    // Cannot demote below Member
    if (target->rank <= Rank::Member)
    {
        return;  // Already at lowest rank
    }

    // Demote
    Rank newRank = static_cast<Rank>(static_cast<int>(target->rank) - 1);
    target->rank = newRank;

    updateMemberRankInDatabase(guild->id, targetGuid, newRank);
    broadcastRoleChange(guild->id, targetGuid, newRank);

    LOG_DEBUG("Player %s demoted to rank %d in guild %d",
              target->name.c_str(), static_cast<int>(newRank), guild->id);
}

// ============================================================================
// Roster
// ============================================================================

void GuildManager::sendRoster(Player* player)
{
    if (!player)
        return;

    GuildData* guild = getGuildByPlayer(player->getGuid());
    if (!guild)
        return;

    GP_Server_GuildRoster roster;
    roster.m_guildId = guild->id;
    roster.m_guildName = guild->name;
    roster.m_motd = guild->motd;

    for (const auto& m : guild->members)
    {
        GP_Server_GuildRoster::Member member;
        member.guid = m.characterGuid;
        member.name = m.name;
        member.rank = static_cast<uint8_t>(m.rank);
        member.level = m.level;
        member.classId = m.classId;
        member.online = m.online;
        roster.m_members.push_back(member);
    }

    StlBuffer buf;
    uint16_t opcode = roster.getOpcode();
    buf << opcode;
    roster.pack(buf);
    player->sendPacket(buf);
}

// ============================================================================
// Chat
// ============================================================================

void GuildManager::sendGuildMessage(Player* sender, const std::string& message)
{
    if (!sender)
        return;

    GuildData* guild = getGuildByPlayer(sender->getGuid());
    if (!guild)
        return;

    GP_Server_ChatMsg chatPacket;
    chatPacket.m_channelId = static_cast<uint8_t>(ChatDefines::Channels::Guild);
    chatPacket.m_fromGuid = sender->getGuid();
    chatPacket.m_fromName = sender->getName();
    chatPacket.m_text = message;

    StlBuffer packet;
    uint16_t opcode = chatPacket.getOpcode();
    packet << opcode;
    chatPacket.pack(packet);

    // Send to all online guild members
    for (const auto& member : guild->members)
    {
        if (member.online)
        {
            if (Player* p = sWorldManager.getPlayer(member.characterGuid))
            {
                p->sendPacket(packet);
            }
        }
    }
}

// ============================================================================
// Player Login/Logout
// ============================================================================

void GuildManager::onPlayerLogin(Player* player)
{
    if (!player)
        return;

    uint32_t playerGuid = player->getGuid();
    GuildData* guild = getGuildByPlayer(playerGuid);

    if (!guild)
        return;

    // Update member as online
    GuildMember* member = guild->getMember(playerGuid);
    if (member)
    {
        member->online = true;
        member->level = player->getLevel();
        member->classId = static_cast<uint8_t>(player->getClassId());
    }

    // Broadcast online status
    broadcastOnlineStatus(guild->id, playerGuid, true);

    // Send roster to the logging-in player
    sendRoster(player);
}

void GuildManager::onPlayerLogout(uint32_t playerGuid)
{
    GuildData* guild = getGuildByPlayer(playerGuid);

    if (!guild)
        return;

    // Update member as offline
    GuildMember* member = guild->getMember(playerGuid);
    if (member)
    {
        member->online = false;
    }

    // Broadcast offline status
    broadcastOnlineStatus(guild->id, playerGuid, false);
}

// ============================================================================
// Query
// ============================================================================

GuildData* GuildManager::getGuildByPlayer(uint32_t playerGuid)
{
    auto it = m_playerToGuild.find(playerGuid);
    if (it == m_playerToGuild.end())
        return nullptr;

    return getGuildById(it->second);
}

GuildData* GuildManager::getGuildById(int32_t guildId)
{
    for (auto& g : m_guilds)
    {
        if (g->id == guildId)
            return g.get();
    }
    return nullptr;
}

int32_t GuildManager::getPlayerGuildId(uint32_t playerGuid) const
{
    auto it = m_playerToGuild.find(playerGuid);
    return (it != m_playerToGuild.end()) ? it->second : 0;
}

bool GuildManager::isInGuild(uint32_t playerGuid) const
{
    return m_playerToGuild.count(playerGuid) > 0;
}

std::string GuildManager::getGuildName(uint32_t playerGuid) const
{
    auto it = m_playerToGuild.find(playerGuid);
    if (it == m_playerToGuild.end())
        return "";

    for (const auto& g : m_guilds)
    {
        if (g->id == it->second)
            return g->name;
    }
    return "";
}

Rank GuildManager::getPlayerRank(uint32_t playerGuid) const
{
    auto it = m_playerToGuild.find(playerGuid);
    if (it == m_playerToGuild.end())
        return Rank::Member;

    for (const auto& g : m_guilds)
    {
        if (g->id == it->second)
        {
            const GuildMember* member = g->getMember(playerGuid);
            return member ? member->rank : Rank::Member;
        }
    }
    return Rank::Member;
}

// ============================================================================
// Database Operations
// ============================================================================

int32_t GuildManager::createGuildInDatabase(const std::string& name, uint32_t leaderGuid)
{
    auto stmt = sDatabase.prepare(
        "INSERT INTO guilds (name, leader_guid, motd) VALUES (?, ?, '')"
    );
    stmt.bind(1, name);
    stmt.bind(2, static_cast<int64_t>(leaderGuid));
    stmt.step();

    return static_cast<int32_t>(sDatabase.lastInsertId());
}

void GuildManager::deleteGuildFromDatabase(int32_t guildId)
{
    // Members are deleted by CASCADE
    auto stmt = sDatabase.prepare("DELETE FROM guilds WHERE id = ?");
    stmt.bind(1, guildId);
    stmt.step();
}

void GuildManager::addMemberToDatabase(int32_t guildId, uint32_t characterGuid, Rank rank)
{
    auto stmt = sDatabase.prepare(
        "INSERT INTO guild_members (guild_id, character_guid, rank) VALUES (?, ?, ?)"
    );
    stmt.bind(1, guildId);
    stmt.bind(2, static_cast<int64_t>(characterGuid));
    stmt.bind(3, static_cast<int>(rank));
    stmt.step();
}

void GuildManager::removeMemberFromDatabase(int32_t guildId, uint32_t characterGuid)
{
    auto stmt = sDatabase.prepare(
        "DELETE FROM guild_members WHERE guild_id = ? AND character_guid = ?"
    );
    stmt.bind(1, guildId);
    stmt.bind(2, static_cast<int64_t>(characterGuid));
    stmt.step();
}

void GuildManager::updateMemberRankInDatabase(int32_t guildId, uint32_t characterGuid, Rank rank)
{
    auto stmt = sDatabase.prepare(
        "UPDATE guild_members SET rank = ? WHERE guild_id = ? AND character_guid = ?"
    );
    stmt.bind(1, static_cast<int>(rank));
    stmt.bind(2, guildId);
    stmt.bind(3, static_cast<int64_t>(characterGuid));
    stmt.step();
}

void GuildManager::updateMotdInDatabase(int32_t guildId, const std::string& motd)
{
    auto stmt = sDatabase.prepare("UPDATE guilds SET motd = ? WHERE id = ?");
    stmt.bind(1, motd);
    stmt.bind(2, guildId);
    stmt.step();
}

void GuildManager::updateLeaderInDatabase(int32_t guildId, uint32_t leaderGuid)
{
    auto stmt = sDatabase.prepare("UPDATE guilds SET leader_guid = ? WHERE id = ?");
    stmt.bind(1, static_cast<int64_t>(leaderGuid));
    stmt.bind(2, guildId);
    stmt.step();
}

// ============================================================================
// Broadcasting
// ============================================================================

void GuildManager::broadcastToGuild(int32_t guildId, const StlBuffer& packet, uint32_t exceptGuid)
{
    GuildData* guild = getGuildById(guildId);
    if (!guild)
        return;

    for (const auto& member : guild->members)
    {
        if (member.online && member.characterGuid != exceptGuid)
        {
            if (Player* p = sWorldManager.getPlayer(member.characterGuid))
            {
                p->sendPacket(packet);
            }
        }
    }
}

void GuildManager::broadcastOnlineStatus(int32_t guildId, uint32_t memberGuid, bool online)
{
    GuildData* guild = getGuildById(guildId);
    if (!guild)
        return;

    const GuildMember* member = guild->getMember(memberGuid);
    if (!member)
        return;

    GP_Server_GuildOnlineStatus pkt;
    pkt.m_playerName = member->name;
    pkt.m_online = online;

    StlBuffer buf;
    uint16_t opcode = pkt.getOpcode();
    buf << opcode;
    pkt.pack(buf);
    broadcastToGuild(guildId, buf, memberGuid);
}

void GuildManager::broadcastAddMember(int32_t guildId, uint32_t memberGuid, const std::string& memberName)
{
    GP_Server_GuildAddMember pkt;
    pkt.m_memberGuid = memberGuid;
    pkt.m_playerName = memberName;

    StlBuffer buf;
    uint16_t opcode = pkt.getOpcode();
    buf << opcode;
    pkt.pack(buf);
    broadcastToGuild(guildId, buf, memberGuid);
}

void GuildManager::broadcastRemoveMember(int32_t guildId, uint32_t memberGuid, const std::string& memberName)
{
    GP_Server_GuildRemoveMember pkt;
    pkt.m_memberGuid = memberGuid;
    pkt.m_playerName = memberName;

    StlBuffer buf;
    uint16_t opcode = pkt.getOpcode();
    buf << opcode;
    pkt.pack(buf);
    broadcastToGuild(guildId, buf, memberGuid);
}

void GuildManager::broadcastRoleChange(int32_t guildId, uint32_t memberGuid, Rank newRank)
{
    GuildData* guild = getGuildById(guildId);
    if (!guild)
        return;

    const GuildMember* member = guild->getMember(memberGuid);
    if (!member)
        return;

    GP_Server_GuildNotifyRoleChange pkt;
    pkt.m_playerName = member->name;
    pkt.m_role = static_cast<uint8_t>(newRank);

    StlBuffer buf;
    uint16_t opcode = pkt.getOpcode();
    buf << opcode;
    pkt.pack(buf);
    broadcastToGuild(guildId, buf);
}

// ============================================================================
// Helpers
// ============================================================================

void GuildManager::sendGuildError(Player* player, GuildError error)
{
    // Use system chat message for errors
    GP_Server_ChatMsg chatPacket;
    chatPacket.m_channelId = static_cast<uint8_t>(ChatDefines::Channels::System);
    chatPacket.m_fromGuid = 0;
    chatPacket.m_fromName = "";

    switch (error)
    {
        case GuildError::NameInvalid:
            chatPacket.m_text = "Invalid guild name.";
            break;
        case GuildError::NameTaken:
            chatPacket.m_text = "That guild name is already taken.";
            break;
        case GuildError::AlreadyInGuild:
            chatPacket.m_text = "You are already in a guild.";
            break;
        case GuildError::NotInGuild:
            chatPacket.m_text = "You are not in a guild.";
            break;
        case GuildError::NotLeader:
            chatPacket.m_text = "Only the guild leader can do that.";
            break;
        case GuildError::NotOfficer:
            chatPacket.m_text = "Only officers can do that.";
            break;
        case GuildError::GuildFull:
            chatPacket.m_text = "The guild is full.";
            break;
        case GuildError::PlayerNotFound:
            chatPacket.m_text = "Player not found.";
            break;
        case GuildError::PlayerAlreadyInGuild:
            chatPacket.m_text = "That player is already in a guild.";
            break;
        case GuildError::CannotKickLeader:
            chatPacket.m_text = "Cannot kick the guild leader.";
            break;
        case GuildError::CannotDemoteLeader:
            chatPacket.m_text = "Cannot demote the guild leader.";
            break;
        case GuildError::CannotPromoteAboveRank:
            chatPacket.m_text = "Cannot promote above current rank.";
            break;
        case GuildError::NoPermission:
            chatPacket.m_text = "You don't have permission to do that.";
            break;
        default:
            chatPacket.m_text = "Guild error.";
            break;
    }

    StlBuffer packet;
    uint16_t opcode = chatPacket.getOpcode();
    packet << opcode;
    chatPacket.pack(packet);
    player->sendPacket(packet);
}

bool GuildManager::isValidGuildName(const std::string& name) const
{
    if (name.length() < MIN_GUILD_NAME_LENGTH || name.length() > MAX_GUILD_NAME_LENGTH)
        return false;

    // Must start with a letter
    if (!std::isalpha(static_cast<unsigned char>(name[0])))
        return false;

    // Allow letters, spaces, and some punctuation
    bool lastWasSpace = false;
    for (char c : name)
    {
        bool isSpace = (c == ' ');
        if (isSpace && lastWasSpace)
            return false;  // No consecutive spaces
        if (!std::isalnum(static_cast<unsigned char>(c)) && !isSpace && c != '\'' && c != '-')
            return false;
        lastWasSpace = isSpace;
    }

    // No trailing space
    if (name.back() == ' ')
        return false;

    return true;
}

GuildMember GuildManager::createMemberFromPlayer(Player* player, Rank rank) const
{
    GuildMember member;
    member.characterGuid = player->getGuid();
    member.name = player->getName();
    member.rank = rank;
    member.level = player->getLevel();
    member.classId = static_cast<uint8_t>(player->getClassId());
    member.online = true;
    return member;
}

}  // namespace Guild

// ============================================================================
// Packet Handlers
// ============================================================================

#include "Network/Session.h"
#include "GamePacketClient.h"

namespace GuildHandlers
{

void handleGuildCreate(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildCreate packet;
    packet.unpack(data);

    sGuildManager.createGuild(player, packet.m_guildName);
}

void handleGuildInviteMember(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildInviteMember packet;
    packet.unpack(data);

    sGuildManager.invitePlayer(player, packet.m_playerName);
}

void handleGuildInviteResponse(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildInviteResponse packet;
    packet.unpack(data);

    sGuildManager.respondToInvite(player, packet.m_guildId, packet.m_accept);
}

void handleGuildQuit(Session& session, StlBuffer& data)
{
    (void)data;  // No data in this packet

    Player* player = session.getPlayer();
    if (!player)
        return;

    sGuildManager.leaveGuild(player);
}

void handleGuildKickMember(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildKickMember packet;
    packet.unpack(data);

    sGuildManager.kickMember(player, packet.m_memberGuid);
}

void handleGuildPromoteMember(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildPromoteMember packet;
    packet.unpack(data);

    sGuildManager.promoteMember(player, packet.m_memberGuid);
}

void handleGuildDemoteMember(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildDemoteMember packet;
    packet.unpack(data);

    sGuildManager.demoteMember(player, packet.m_memberGuid);
}

void handleGuildDisband(Session& session, StlBuffer& data)
{
    (void)data;  // No data in this packet

    Player* player = session.getPlayer();
    if (!player)
        return;

    sGuildManager.disbandGuild(player);
}

void handleGuildMotd(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Client_GuildMotd packet;
    packet.unpack(data);

    sGuildManager.setMotd(player, packet.m_motd);
}

void handleGuildRosterRequest(Session& session, StlBuffer& data)
{
    (void)data;  // No data in this packet

    Player* player = session.getPlayer();
    if (!player)
        return;

    sGuildManager.sendRoster(player);
}

}  // namespace GuildHandlers
