// PartySystem - Manages player parties
// Phase 8, Task 8.4: Party System

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

class Player;
class Session;
class StlBuffer;

namespace Party
{

// ============================================================================
// Constants
// ============================================================================

constexpr size_t MAX_PARTY_SIZE = 5;

// Party change types (from GP_Client_PartyChanges)
enum class ChangeType : uint8_t
{
    Leave = 1,
    Kick = 2,
    Promote = 3,
};

// ============================================================================
// Party Structure
// ============================================================================

struct PartyData
{
    uint32_t id = 0;                      // Unique party ID
    uint32_t leaderGuid = 0;              // Party leader GUID
    std::vector<uint32_t> memberGuids;    // All member GUIDs (including leader)

    bool isMember(uint32_t guid) const;
    bool isLeader(uint32_t guid) const { return leaderGuid == guid; }
    size_t size() const { return memberGuids.size(); }
    bool isFull() const { return memberGuids.size() >= MAX_PARTY_SIZE; }
};

// ============================================================================
// Pending Invite
// ============================================================================

struct PendingInvite
{
    uint32_t inviterGuid = 0;
    std::string inviterName;
    int64_t expireTime = 0;  // Unix timestamp when invite expires
};

// ============================================================================
// PartyManager - Singleton
// ============================================================================

class PartyManager
{
public:
    static PartyManager& instance();

    // -------------------------------------------------------------------------
    // Invite Flow
    // -------------------------------------------------------------------------

    // Invite a player to party (creates pending invite)
    void invitePlayer(Player* inviter, const std::string& targetName);

    // Accept/decline pending invite
    void respondToInvite(Player* target, bool accept);

    // -------------------------------------------------------------------------
    // Party Management
    // -------------------------------------------------------------------------

    // Leave current party
    void leaveParty(Player* player);

    // Kick a member (leader only)
    void kickMember(Player* leader, uint32_t targetGuid);

    // Promote new leader (leader only)
    void promoteLeader(Player* leader, uint32_t targetGuid);

    // -------------------------------------------------------------------------
    // Party Queries
    // -------------------------------------------------------------------------

    // Get party a player is in (nullptr if not in party)
    PartyData* getParty(Player* player);
    PartyData* getPartyByGuid(uint32_t playerGuid);

    // Get all party members as Player pointers (online only)
    std::vector<Player*> getPartyMembers(Player* player);

    // Check if player is in a party
    bool isInParty(uint32_t playerGuid) const;

    // -------------------------------------------------------------------------
    // Chat Integration
    // -------------------------------------------------------------------------

    // Send message to all party members
    void sendPartyMessage(Player* sender, const std::string& message);

    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------

    // Called when player disconnects - handles party cleanup
    void onPlayerDisconnect(uint32_t playerGuid);

    // Expire old invites
    void update();

private:
    PartyManager() = default;
    ~PartyManager() = default;
    PartyManager(const PartyManager&) = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    // Create a new party with these two players
    void createParty(Player* leader, Player* member);

    // Add member to existing party
    void addToParty(PartyData* party, Player* player);

    // Remove member from party (handles disbanding if needed)
    void removeFromParty(PartyData* party, uint32_t playerGuid);

    // Disband party (notify all members)
    void disbandParty(PartyData* party);

    // Send party list update to all members
    void broadcastPartyList(PartyData* party);

    // Send party list to single player (empty = not in party)
    void sendPartyList(Player* player, PartyData* party = nullptr);

    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------

    // All parties by party ID
    std::unordered_map<uint32_t, std::unique_ptr<PartyData>> m_parties;

    // Player GUID -> Party ID mapping
    std::unordered_map<uint32_t, uint32_t> m_playerPartyMap;

    // Pending invites: target GUID -> invite data
    std::unordered_map<uint32_t, PendingInvite> m_pendingInvites;

    // Next party ID
    uint32_t m_nextPartyId = 1;
};

// Convenience macro
#define sPartyManager Party::PartyManager::instance()

// ============================================================================
// Packet Handlers
// ============================================================================

// Handle GP_Client_PartyInviteMember
void handlePartyInvite(Session& session, StlBuffer& data);

// Handle GP_Client_PartyInviteResponse
void handlePartyInviteResponse(Session& session, StlBuffer& data);

// Handle GP_Client_PartyChanges (leave, kick, promote)
void handlePartyChanges(Session& session, StlBuffer& data);

} // namespace Party
