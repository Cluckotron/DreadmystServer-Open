// DuelSystem - Manages player duels
// Phase 8, Task 8.8: Duel System

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class Player;
class Session;
class StlBuffer;

namespace Duel
{

// ============================================================================
// Constants
// ============================================================================

constexpr float DUEL_RANGE = 200.0f;            // Max distance between duelists
constexpr float DUEL_INVITE_RANGE = 100.0f;     // Max distance to initiate duel
constexpr int64_t DUEL_INVITE_TIMEOUT_S = 60;   // Invite expires after 60s
constexpr int32_t DUEL_COUNTDOWN_S = 3;         // Countdown before duel starts
constexpr int32_t DUEL_MIN_HEALTH = 1;          // Duel ends when health reaches this

// ============================================================================
// Duel State
// ============================================================================

enum class DuelState : uint8_t
{
    None = 0,
    Pending,     // Invite sent, waiting for response
    Countdown,   // Accepted, counting down
    Active,      // Fighting
    Finished,    // Duel ended
};

// ============================================================================
// Active Duel
// ============================================================================

struct ActiveDuel
{
    uint32_t player1Guid = 0;     // Challenger
    uint32_t player2Guid = 0;     // Challenged
    DuelState state = DuelState::None;
    float centerX = 0.0f;
    float centerY = 0.0f;
    int32_t countdownRemaining = 0;  // Ticks remaining in countdown
    int64_t startTime = 0;
};

// ============================================================================
// Pending Invite
// ============================================================================

struct PendingInvite
{
    uint32_t challengerGuid = 0;
    int64_t timestamp = 0;
};

// ============================================================================
// DuelManager - Singleton
// ============================================================================

class DuelManager
{
public:
    static DuelManager& instance();

    // -------------------------------------------------------------------------
    // Duel Flow
    // -------------------------------------------------------------------------

    // Request a duel (from OfferDuel spell)
    void requestDuel(Player* challenger, Player* target);

    // Accept/decline pending invite
    void respondToDuel(Player* target, bool accept);

    // Surrender/yield
    void yieldDuel(Player* player);

    // -------------------------------------------------------------------------
    // Combat Integration
    // -------------------------------------------------------------------------

    // Check if two players are dueling each other
    bool areDueling(uint32_t guid1, uint32_t guid2) const;

    // Check if a player is in a duel
    bool isInDuel(uint32_t playerGuid) const;

    // Get opponent in duel (0 if not dueling)
    uint32_t getDuelOpponent(uint32_t playerGuid) const;

    // Called when a player takes damage in a duel
    // Returns true if the duel should end (player reached min health)
    bool onDuelDamage(Player* player, int32_t newHealth);

    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------

    // Called when player disconnects
    void onPlayerDisconnect(uint32_t playerGuid);

    // Update tick (for countdown)
    void update(float deltaTime);

private:
    DuelManager() = default;
    ~DuelManager() = default;
    DuelManager(const DuelManager&) = delete;
    DuelManager& operator=(const DuelManager&) = delete;

    // Internal helpers
    void startCountdown(uint32_t player1Guid, uint32_t player2Guid);
    void startDuel(ActiveDuel* duel);
    void endDuel(ActiveDuel* duel, uint32_t winnerGuid, bool surrender = false);
    void cancelDuel(ActiveDuel* duel, bool disconnected = false);
    void sendNotification(Player* player, uint8_t notifyType);
    void sendOfferDuel(Player* target, Player* challenger);

    ActiveDuel* findDuel(uint32_t playerGuid);

    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------

    // All active/pending duels
    std::unordered_map<uint32_t, ActiveDuel> m_duels;  // Key is lower guid

    // Pending invites: targetGuid -> invite data
    std::unordered_map<uint32_t, PendingInvite> m_pendingInvites;

    // Player -> duel key mapping
    std::unordered_map<uint32_t, uint32_t> m_playerToDuelKey;

    // Countdown accumulator
    float m_countdownAccumulator = 0.0f;
};

}  // namespace Duel

// Convenience macro
#define sDuelManager Duel::DuelManager::instance()

// ============================================================================
// Packet Handlers
// ============================================================================

namespace DuelHandlers
{
    void handleDuelResponse(Session& session, StlBuffer& data);
    void handleYieldDuel(Session& session, StlBuffer& data);
}
