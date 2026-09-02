// GuildSystem - Guild management
// Task 8.6: Guild System

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

class Player;

namespace Guild
{
    // Guild rank constants (from GuildDefines.h)
    enum class Rank : uint8_t
    {
        Member = 0,
        Officer = 1,
        Leader = 2,
    };

    // Maximum guild members
    constexpr size_t MAX_GUILD_MEMBERS = 100;
    constexpr size_t MAX_GUILD_NAME_LENGTH = 24;
    constexpr size_t MIN_GUILD_NAME_LENGTH = 2;
    constexpr size_t MAX_MOTD_LENGTH = 128;

    // Guild error codes
    enum class GuildError : uint8_t
    {
        Success = 0,
        NameInvalid = 1,
        NameTaken = 2,
        AlreadyInGuild = 3,
        NotInGuild = 4,
        NotLeader = 5,
        NotOfficer = 6,
        GuildFull = 7,
        PlayerNotFound = 8,
        PlayerAlreadyInGuild = 9,
        CannotKickLeader = 10,
        CannotDemoteLeader = 11,
        CannotPromoteAboveRank = 12,
        NoPermission = 13,
    };

    // Guild member info (in-memory cache)
    struct GuildMember
    {
        uint32_t characterGuid = 0;
        std::string name;
        Rank rank = Rank::Member;
        int32_t level = 1;
        uint8_t classId = 0;
        bool online = false;
    };

    // Guild data (in-memory cache)
    struct GuildData
    {
        int32_t id = 0;
        std::string name;
        uint32_t leaderGuid = 0;
        std::string motd;
        std::vector<GuildMember> members;

        // Helper methods
        GuildMember* getMember(uint32_t guid);
        const GuildMember* getMember(uint32_t guid) const;
        bool isMember(uint32_t guid) const;
        bool isLeader(uint32_t guid) const;
        bool isOfficer(uint32_t guid) const;
        bool canInvite(uint32_t guid) const;  // Officers and Leader
        bool canKick(uint32_t guid) const;    // Officers can kick Members, Leader can kick anyone
        bool canPromote(uint32_t guid) const; // Leader only
    };

    // Pending guild invite
    struct PendingInvite
    {
        int32_t guildId = 0;
        uint32_t inviterGuid = 0;
        int64_t timestamp = 0;
    };

    // Guild Manager singleton
    class GuildManager
    {
    public:
        static GuildManager& instance();

        // Initialization
        void loadGuildsFromDatabase();

        // Guild operations
        void createGuild(Player* creator, const std::string& guildName);
        void disbandGuild(Player* leader);
        void setMotd(Player* player, const std::string& motd);

        // Member operations
        void invitePlayer(Player* inviter, const std::string& targetName);
        void respondToInvite(Player* target, int32_t guildId, bool accept);
        void leaveGuild(Player* player);
        void kickMember(Player* kicker, uint32_t targetGuid);
        void promoteMember(Player* promoter, uint32_t targetGuid);
        void demoteMember(Player* demoter, uint32_t targetGuid);

        // Roster request
        void sendRoster(Player* player);

        // Chat
        void sendGuildMessage(Player* sender, const std::string& message);

        // Player login/logout
        void onPlayerLogin(Player* player);
        void onPlayerLogout(uint32_t playerGuid);

        // Query
        GuildData* getGuildByPlayer(uint32_t playerGuid);
        GuildData* getGuildById(int32_t guildId);
        int32_t getPlayerGuildId(uint32_t playerGuid) const;
        bool isInGuild(uint32_t playerGuid) const;
        std::string getGuildName(uint32_t playerGuid) const;
        Rank getPlayerRank(uint32_t playerGuid) const;

    private:
        GuildManager() = default;
        ~GuildManager() = default;
        GuildManager(const GuildManager&) = delete;
        GuildManager& operator=(const GuildManager&) = delete;

        // Database operations
        int32_t createGuildInDatabase(const std::string& name, uint32_t leaderGuid);
        void deleteGuildFromDatabase(int32_t guildId);
        void addMemberToDatabase(int32_t guildId, uint32_t characterGuid, Rank rank);
        void removeMemberFromDatabase(int32_t guildId, uint32_t characterGuid);
        void updateMemberRankInDatabase(int32_t guildId, uint32_t characterGuid, Rank rank);
        void updateMotdInDatabase(int32_t guildId, const std::string& motd);
        void updateLeaderInDatabase(int32_t guildId, uint32_t leaderGuid);

        // Broadcasting
        void broadcastToGuild(int32_t guildId, const class StlBuffer& packet, uint32_t exceptGuid = 0);
        void broadcastOnlineStatus(int32_t guildId, uint32_t memberGuid, bool online);
        void broadcastAddMember(int32_t guildId, uint32_t memberGuid, const std::string& memberName);
        void broadcastRemoveMember(int32_t guildId, uint32_t memberGuid, const std::string& memberName);
        void broadcastRoleChange(int32_t guildId, uint32_t memberGuid, Rank newRank);

        // Helper
        void sendGuildError(Player* player, GuildError error);
        bool isValidGuildName(const std::string& name) const;
        GuildMember createMemberFromPlayer(Player* player, Rank rank) const;

        // In-memory data
        std::vector<std::unique_ptr<GuildData>> m_guilds;
        std::unordered_map<uint32_t, int32_t> m_playerToGuild;  // characterGuid -> guildId
        std::unordered_map<uint32_t, PendingInvite> m_pendingInvites;  // targetGuid -> invite

        int32_t m_nextGuildId = 1;
    };

}  // namespace Guild

// Global accessor macro
#define sGuildManager Guild::GuildManager::instance()

// ============================================================================
// Packet Handlers (defined outside namespace for registration)
// ============================================================================

class Session;
class StlBuffer;

namespace GuildHandlers
{
    void handleGuildCreate(Session& session, StlBuffer& data);
    void handleGuildInviteMember(Session& session, StlBuffer& data);
    void handleGuildInviteResponse(Session& session, StlBuffer& data);
    void handleGuildQuit(Session& session, StlBuffer& data);
    void handleGuildKickMember(Session& session, StlBuffer& data);
    void handleGuildPromoteMember(Session& session, StlBuffer& data);
    void handleGuildDemoteMember(Session& session, StlBuffer& data);
    void handleGuildDisband(Session& session, StlBuffer& data);
    void handleGuildMotd(Session& session, StlBuffer& data);
    void handleGuildRosterRequest(Session& session, StlBuffer& data);
}
