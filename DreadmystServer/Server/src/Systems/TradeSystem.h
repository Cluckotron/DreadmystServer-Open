// Trade System - Player-to-player trading
// Phase 6, Task 6.7: Trading System

#pragma once

#include <cstdint>
#include <array>
#include <unordered_map>
#include "PlayerDefines.h"
#include "ItemDefines.h"

class Player;

namespace Trade
{

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_SLOTS = PlayerDefines::Trade::MaxSlots;
constexpr int INVALID_SLOT = -1;

// ============================================================================
// TradeItem - Item offered in trade
// ============================================================================

struct TradeItem
{
    int32_t inventorySlot = INVALID_SLOT;  // Source inventory slot
    int32_t itemId = 0;
    int32_t stackCount = 0;
    int32_t itemGuid = 0;  // Unique identifier for this item instance

    bool isEmpty() const { return inventorySlot == INVALID_SLOT || itemId == 0; }
    void clear() { inventorySlot = INVALID_SLOT; itemId = 0; stackCount = 0; itemGuid = 0; }
};

// ============================================================================
// TradeOffer - One player's side of the trade
// ============================================================================

struct TradeOffer
{
    Player* player = nullptr;
    std::array<TradeItem, MAX_SLOTS> items;
    int32_t gold = 0;
    bool confirmed = false;

    int getFirstEmptySlot() const;
    int findItemByInventorySlot(int32_t invSlot) const;
    int findItemByGuid(int32_t itemGuid) const;
    int countItems() const;
    void clear();
};

// ============================================================================
// TradeSession - Active trade between two players
// ============================================================================

class TradeSession
{
public:
    TradeSession(Player* initiator, Player* target);
    ~TradeSession() = default;

    // Get players
    Player* getInitiator() const { return m_offers[0].player; }
    Player* getTarget() const { return m_offers[1].player; }
    Player* getOtherPlayer(Player* player) const;

    // Get offers
    TradeOffer& getOffer(Player* player);
    const TradeOffer& getOffer(Player* player) const;
    TradeOffer& getOtherOffer(Player* player);
    const TradeOffer& getOtherOffer(Player* player) const;

    // Item operations
    bool addItem(Player* player, int32_t inventorySlot);
    bool removeItem(Player* player, int32_t itemGuid);

    // Gold operations
    bool setGold(Player* player, int32_t amount);

    // Confirmation
    void setConfirmed(Player* player, bool confirmed);
    bool isConfirmed(Player* player) const;
    bool areBothConfirmed() const;

    // Check if player is part of this session
    bool hasPlayer(Player* player) const;

    // Unique session ID
    uint64_t getId() const { return m_sessionId; }

private:
    std::array<TradeOffer, 2> m_offers;
    uint64_t m_sessionId;
    static uint64_t s_nextSessionId;

    int getOfferIndex(Player* player) const;
};

// ============================================================================
// TradeManager - Singleton managing all active trades
// ============================================================================

class TradeManager
{
public:
    static TradeManager& instance();

    // Start a new trade between two players
    TradeSession* startTrade(Player* initiator, Player* target);

    // Cancel a trade
    void cancelTrade(Player* player);

    // Get active trade session for a player
    TradeSession* getTradeSession(Player* player);
    const TradeSession* getTradeSession(Player* player) const;

    // Check if player is in a trade
    bool isInTrade(Player* player) const;

    // Complete the trade (transfer items/gold)
    bool completeTrade(TradeSession* session);

    // Send trade update to both players
    void sendTradeUpdate(TradeSession* session);

    // Send trade canceled to a player
    void sendTradeCanceled(Player* player);

private:
    TradeManager() = default;
    ~TradeManager() = default;
    TradeManager(const TradeManager&) = delete;
    TradeManager& operator=(const TradeManager&) = delete;

    // Map player GUID to their trade session
    std::unordered_map<uint32_t, TradeSession*> m_playerSessions;

    // All active trade sessions (owned)
    std::unordered_map<uint64_t, std::unique_ptr<TradeSession>> m_sessions;
};

// Global accessor
#define sTradeManager Trade::TradeManager::instance()

} // namespace Trade
