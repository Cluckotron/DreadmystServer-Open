// Trade System - Player-to-player trading
// Phase 6, Task 6.7: Trading System

#include "stdafx.h"
#include "Systems/TradeSystem.h"
#include "World/Player.h"
#include "Systems/Inventory.h"
#include "Database/GameData.h"
#include "Core/Logger.h"
#include "GamePacketServer.h"

namespace Trade
{

// ============================================================================
// Static initialization
// ============================================================================

uint64_t TradeSession::s_nextSessionId = 1;

// ============================================================================
// TradeOffer
// ============================================================================

int TradeOffer::getFirstEmptySlot() const
{
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (items[i].isEmpty())
            return i;
    }
    return INVALID_SLOT;
}

int TradeOffer::findItemByInventorySlot(int32_t invSlot) const
{
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (items[i].inventorySlot == invSlot)
            return i;
    }
    return INVALID_SLOT;
}

int TradeOffer::findItemByGuid(int32_t itemGuid) const
{
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (items[i].itemGuid == itemGuid)
            return i;
    }
    return INVALID_SLOT;
}

int TradeOffer::countItems() const
{
    int count = 0;
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (!items[i].isEmpty())
            ++count;
    }
    return count;
}

void TradeOffer::clear()
{
    for (auto& item : items)
        item.clear();
    gold = 0;
    confirmed = false;
}

// ============================================================================
// TradeSession
// ============================================================================

TradeSession::TradeSession(Player* initiator, Player* target)
    : m_sessionId(s_nextSessionId++)
{
    m_offers[0].player = initiator;
    m_offers[1].player = target;
    m_offers[0].clear();
    m_offers[1].clear();

    LOG_DEBUG("TradeSession: Created session %llu between %s and %s",
              m_sessionId, initiator->getName().c_str(), target->getName().c_str());
}

Player* TradeSession::getOtherPlayer(Player* player) const
{
    if (m_offers[0].player == player)
        return m_offers[1].player;
    if (m_offers[1].player == player)
        return m_offers[0].player;
    return nullptr;
}

int TradeSession::getOfferIndex(Player* player) const
{
    if (m_offers[0].player == player)
        return 0;
    if (m_offers[1].player == player)
        return 1;
    return -1;
}

TradeOffer& TradeSession::getOffer(Player* player)
{
    int idx = getOfferIndex(player);
    return m_offers[idx >= 0 ? idx : 0];
}

const TradeOffer& TradeSession::getOffer(Player* player) const
{
    int idx = getOfferIndex(player);
    return m_offers[idx >= 0 ? idx : 0];
}

TradeOffer& TradeSession::getOtherOffer(Player* player)
{
    int idx = getOfferIndex(player);
    return m_offers[idx == 0 ? 1 : 0];
}

const TradeOffer& TradeSession::getOtherOffer(Player* player) const
{
    int idx = getOfferIndex(player);
    return m_offers[idx == 0 ? 1 : 0];
}

bool TradeSession::addItem(Player* player, int32_t inventorySlot)
{
    int idx = getOfferIndex(player);
    if (idx < 0)
        return false;

    auto& offer = m_offers[idx];

    // Check if item is already in trade
    if (offer.findItemByInventorySlot(inventorySlot) != INVALID_SLOT)
    {
        LOG_DEBUG("TradeSession: Item from slot %d already in trade", inventorySlot);
        return false;
    }

    // Find empty trade slot
    int tradeSlot = offer.getFirstEmptySlot();
    if (tradeSlot == INVALID_SLOT)
    {
        LOG_DEBUG("TradeSession: No empty trade slots");
        return false;
    }

    // Get item from inventory
    const Inventory::InventoryItem* invItem = player->getInventory().getItem(inventorySlot);
    if (!invItem)
    {
        LOG_DEBUG("TradeSession: Invalid inventory slot %d", inventorySlot);
        return false;
    }

    // Add to trade
    offer.items[tradeSlot].inventorySlot = inventorySlot;
    offer.items[tradeSlot].itemId = invItem->itemId;
    offer.items[tradeSlot].stackCount = invItem->stackCount;
    // Generate a unique item GUID for this trade instance
    offer.items[tradeSlot].itemGuid = static_cast<int32_t>((m_sessionId << 16) | (idx << 8) | tradeSlot);

    // Reset confirmations when trade changes
    m_offers[0].confirmed = false;
    m_offers[1].confirmed = false;

    LOG_DEBUG("TradeSession: Player %s added item %d from slot %d to trade slot %d",
              player->getName().c_str(), invItem->itemId, inventorySlot, tradeSlot);

    return true;
}

bool TradeSession::removeItem(Player* player, int32_t itemGuid)
{
    int idx = getOfferIndex(player);
    if (idx < 0)
        return false;

    auto& offer = m_offers[idx];

    int tradeSlot = offer.findItemByGuid(itemGuid);
    if (tradeSlot == INVALID_SLOT)
    {
        LOG_DEBUG("TradeSession: Item guid %d not found in trade", itemGuid);
        return false;
    }

    offer.items[tradeSlot].clear();

    // Reset confirmations when trade changes
    m_offers[0].confirmed = false;
    m_offers[1].confirmed = false;

    LOG_DEBUG("TradeSession: Player %s removed item from trade slot %d",
              player->getName().c_str(), tradeSlot);

    return true;
}

bool TradeSession::setGold(Player* player, int32_t amount)
{
    int idx = getOfferIndex(player);
    if (idx < 0)
        return false;

    if (amount < 0)
        amount = 0;

    // Validate player has enough gold
    if (amount > player->getGold())
        amount = player->getGold();

    m_offers[idx].gold = amount;

    // Reset confirmations when trade changes
    m_offers[0].confirmed = false;
    m_offers[1].confirmed = false;

    LOG_DEBUG("TradeSession: Player %s set gold offer to %d",
              player->getName().c_str(), amount);

    return true;
}

void TradeSession::setConfirmed(Player* player, bool confirmed)
{
    int idx = getOfferIndex(player);
    if (idx >= 0)
    {
        m_offers[idx].confirmed = confirmed;
        LOG_DEBUG("TradeSession: Player %s %s trade",
                  player->getName().c_str(), confirmed ? "confirmed" : "unconfirmed");
    }
}

bool TradeSession::isConfirmed(Player* player) const
{
    int idx = getOfferIndex(player);
    return idx >= 0 ? m_offers[idx].confirmed : false;
}

bool TradeSession::areBothConfirmed() const
{
    return m_offers[0].confirmed && m_offers[1].confirmed;
}

bool TradeSession::hasPlayer(Player* player) const
{
    return m_offers[0].player == player || m_offers[1].player == player;
}

// ============================================================================
// TradeManager
// ============================================================================

TradeManager& TradeManager::instance()
{
    static TradeManager inst;
    return inst;
}

TradeSession* TradeManager::startTrade(Player* initiator, Player* target)
{
    if (!initiator || !target)
        return nullptr;

    // Check if either player is already in a trade
    if (isInTrade(initiator))
    {
        LOG_DEBUG("TradeManager: %s is already in a trade", initiator->getName().c_str());
        return nullptr;
    }

    if (isInTrade(target))
    {
        LOG_DEBUG("TradeManager: %s is already in a trade", target->getName().c_str());
        return nullptr;
    }

    // Create new session
    auto session = std::make_unique<TradeSession>(initiator, target);
    TradeSession* sessionPtr = session.get();

    // Register both players
    m_playerSessions[initiator->getGuid()] = sessionPtr;
    m_playerSessions[target->getGuid()] = sessionPtr;
    m_sessions[session->getId()] = std::move(session);

    LOG_INFO("TradeManager: Started trade between %s and %s",
             initiator->getName().c_str(), target->getName().c_str());

    return sessionPtr;
}

void TradeManager::cancelTrade(Player* player)
{
    if (!player)
        return;

    auto it = m_playerSessions.find(player->getGuid());
    if (it == m_playerSessions.end())
        return;

    TradeSession* session = it->second;
    Player* other = session->getOtherPlayer(player);

    // Send cancel notification to both players
    sendTradeCanceled(player);
    if (other)
        sendTradeCanceled(other);

    // Remove player mappings
    m_playerSessions.erase(player->getGuid());
    if (other)
        m_playerSessions.erase(other->getGuid());

    // Remove session
    m_sessions.erase(session->getId());

    LOG_INFO("TradeManager: Trade canceled by %s", player->getName().c_str());
}

TradeSession* TradeManager::getTradeSession(Player* player)
{
    auto it = m_playerSessions.find(player->getGuid());
    return (it != m_playerSessions.end()) ? it->second : nullptr;
}

const TradeSession* TradeManager::getTradeSession(Player* player) const
{
    auto it = m_playerSessions.find(player->getGuid());
    return (it != m_playerSessions.end()) ? it->second : nullptr;
}

bool TradeManager::isInTrade(Player* player) const
{
    return m_playerSessions.find(player->getGuid()) != m_playerSessions.end();
}

bool TradeManager::completeTrade(TradeSession* session)
{
    if (!session || !session->areBothConfirmed())
        return false;

    Player* player1 = session->getInitiator();
    Player* player2 = session->getTarget();

    if (!player1 || !player2)
        return false;

    const auto& offer1 = session->getOffer(player1);
    const auto& offer2 = session->getOffer(player2);

    // Validate both players have enough gold
    if (player1->getGold() < offer1.gold)
    {
        LOG_WARN("TradeManager: %s doesn't have enough gold", player1->getName().c_str());
        return false;
    }
    if (player2->getGold() < offer2.gold)
    {
        LOG_WARN("TradeManager: %s doesn't have enough gold", player2->getName().c_str());
        return false;
    }

    // Count items each player will receive
    int items1Receives = offer2.countItems();
    int items2Receives = offer1.countItems();

    // Check inventory space (accounting for items being removed)
    int player1Space = player1->getInventory().countEmptySlots() + offer1.countItems();
    int player2Space = player2->getInventory().countEmptySlots() + offer2.countItems();

    if (player1Space < items1Receives)
    {
        LOG_WARN("TradeManager: %s doesn't have enough inventory space", player1->getName().c_str());
        return false;
    }
    if (player2Space < items2Receives)
    {
        LOG_WARN("TradeManager: %s doesn't have enough inventory space", player2->getName().c_str());
        return false;
    }

    // Validate items still exist in inventory
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (!offer1.items[i].isEmpty())
        {
            const auto* item = player1->getInventory().getItem(offer1.items[i].inventorySlot);
            if (!item || item->itemId != offer1.items[i].itemId)
            {
                LOG_WARN("TradeManager: Item mismatch for %s", player1->getName().c_str());
                return false;
            }
        }
        if (!offer2.items[i].isEmpty())
        {
            const auto* item = player2->getInventory().getItem(offer2.items[i].inventorySlot);
            if (!item || item->itemId != offer2.items[i].itemId)
            {
                LOG_WARN("TradeManager: Item mismatch for %s", player2->getName().c_str());
                return false;
            }
        }
    }

    // ===== Execute trade =====

    // Remove items from player 1's inventory (store them temporarily)
    std::vector<Inventory::InventoryItem> player1Items;
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (!offer1.items[i].isEmpty())
        {
            Inventory::InventoryItem temp;
            const auto* item = player1->getInventory().getItem(offer1.items[i].inventorySlot);
            if (item)
            {
                temp = *item;
                player1Items.push_back(temp);
                player1->getInventory().clearSlot(offer1.items[i].inventorySlot);
            }
        }
    }

    // Remove items from player 2's inventory (store them temporarily)
    std::vector<Inventory::InventoryItem> player2Items;
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (!offer2.items[i].isEmpty())
        {
            Inventory::InventoryItem temp;
            const auto* item = player2->getInventory().getItem(offer2.items[i].inventorySlot);
            if (item)
            {
                temp = *item;
                player2Items.push_back(temp);
                player2->getInventory().clearSlot(offer2.items[i].inventorySlot);
            }
        }
    }

    // Add player 1's items to player 2
    for (const auto& item : player1Items)
    {
        player2->getInventory().addItem(item.itemId, item.stackCount);
    }

    // Add player 2's items to player 1
    for (const auto& item : player2Items)
    {
        player1->getInventory().addItem(item.itemId, item.stackCount);
    }

    // Transfer gold
    if (offer1.gold > 0)
    {
        player1->spendGold(offer1.gold);
        player2->addGold(offer1.gold);
    }
    if (offer2.gold > 0)
    {
        player2->spendGold(offer2.gold);
        player1->addGold(offer2.gold);
    }

    // Send inventory updates
    player1->sendInventory();
    player2->sendInventory();

    // Send trade complete (canceled packet with empty state acts as completion)
    sendTradeCanceled(player1);
    sendTradeCanceled(player2);

    // Remove player mappings
    m_playerSessions.erase(player1->getGuid());
    m_playerSessions.erase(player2->getGuid());

    // Remove session
    m_sessions.erase(session->getId());

    LOG_INFO("TradeManager: Trade completed between %s and %s",
             player1->getName().c_str(), player2->getName().c_str());

    return true;
}

void TradeManager::sendTradeUpdate(TradeSession* session)
{
    if (!session)
        return;

    Player* player1 = session->getInitiator();
    Player* player2 = session->getTarget();

    if (!player1 || !player2)
        return;

    auto buildItemDef = [](Player* owner, const TradeItem& tradeItem) {
        ItemDefines::ItemDefinition def;
        def.m_itemId = tradeItem.itemId;
        if (owner)
        {
            if (const auto* invItem = owner->getInventory().getItem(tradeItem.inventorySlot))
            {
                def.m_affix1 = invItem->affix1;
                def.m_affix2 = invItem->affix2;
                def.m_gem1 = invItem->gem1;
                def.m_gem2 = invItem->gem2;
                def.m_gem3 = invItem->gem3;
                def.m_gem4 = 0;
                def.m_durability = invItem->durability;
                def.m_enchant = invItem->enchantId;
                def.m_count = tradeItem.stackCount;
            }
        }
        return def;
    };

    auto appendItems = [&](Player* owner, const TradeOffer& offer,
                           std::map<int32_t, GP_Server_TradeUpdate::TradeItemList>& outMap)
    {
        for (int i = 0; i < MAX_SLOTS; ++i)
        {
            if (!offer.items[i].isEmpty())
            {
                const int32_t itemGuid = offer.items[i].itemGuid;
                if (itemGuid == 0)
                    continue;

                ItemDefines::ItemDefinition def = buildItemDef(owner, offer.items[i]);
                outMap[itemGuid].push_back({def, offer.items[i].stackCount});
            }
        }
    };

    // Send update to player 1
    {
        const auto& myOffer = session->getOffer(player1);
        const auto& theirOffer = session->getOffer(player2);

        GP_Server_TradeUpdate packet;
        packet.m_partnerGuid = player2->getGuid();
        packet.m_myMoney = myOffer.gold;
        packet.m_hisMoney = theirOffer.gold;
        packet.m_myReady = myOffer.confirmed;
        packet.m_theirReady = theirOffer.confirmed;
        appendItems(player1, myOffer, packet.m_myItems);
        appendItems(player2, theirOffer, packet.m_theirItems);

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        player1->sendPacket(buf);
    }

    // Send update to player 2
    {
        const auto& myOffer = session->getOffer(player2);
        const auto& theirOffer = session->getOffer(player1);

        GP_Server_TradeUpdate packet;
        packet.m_partnerGuid = player1->getGuid();
        packet.m_myMoney = myOffer.gold;
        packet.m_hisMoney = theirOffer.gold;
        packet.m_myReady = myOffer.confirmed;
        packet.m_theirReady = theirOffer.confirmed;
        appendItems(player2, myOffer, packet.m_myItems);
        appendItems(player1, theirOffer, packet.m_theirItems);

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        player2->sendPacket(buf);
    }
}

void TradeManager::sendTradeCanceled(Player* player)
{
    if (!player)
        return;

    GP_Server_TradeCanceled packet;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

} // namespace Trade
