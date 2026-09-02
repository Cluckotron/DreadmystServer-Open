// World Entry, Movement, and Combat Packet Handlers
// Task 4.5: World Entry Packet Flow
// Task 4.8: Movement System
// Task 5.15: Combat Handlers

#pragma once

#include <vector>
#include "../../../Geo2d.h"

class Session;
class StlBuffer;
class Player;
class Entity;

namespace Handlers
{

// Register all world entry related packet handlers
void registerWorldHandlers();

// ============================================================================
// World Entry Handlers
// ============================================================================

void handleEnterWorld(Session& session, StlBuffer& data);

// Helper: Send complete world entry packet sequence
void sendWorldEntry(Session& session);

// Helper: Send player self data
void sendPlayerSelf(Session& session);

// Helper: Send inventory (stub for now - Phase 6)
void sendInventory(Session& session);

// Helper: Send spellbook (stub for now - Phase 5)
void sendSpellbook(Session& session);

// Helper: Send quest list (stub for now - Phase 7)
void sendQuestList(Session& session);

// Helper: Send active cooldowns (Task 5.7)
void sendCooldowns(Session& session);

// Helper: Send active auras (Task 5.8)
void sendAuras(Session& session);

// ============================================================================
// Movement Handlers (Task 4.8)
// ============================================================================

void handleRequestMove(Session& session, StlBuffer& data);
void handleRequestStop(Session& session, StlBuffer& data);

// Helper: Broadcast movement spline to all visible players
void broadcastMovement(Player* player, const std::vector<Geo2d::Vector2>& path);
void broadcastMovement(Player* player, float destX, float destY);

// Helper: Broadcast stop to all visible players
void broadcastStop(Player* player);

// Movement constants
constexpr float DEFAULT_MOVE_SPEED = 200.0f;  // Pixels per second
constexpr float MAX_MOVE_SPEED = 400.0f;      // Anti-hack limit
constexpr float POSITION_TOLERANCE = 50.0f;   // Allowed desync distance

// ============================================================================
// Respawn Handler (Task 5.12)
// ============================================================================

void handleRequestRespawn(Session& session, StlBuffer& data);

// ============================================================================
// Combat Handlers (Task 5.15)
// ============================================================================

// Handle spell cast request
void handleCastSpell(Session& session, StlBuffer& data);

// Handle cancel cast request
void handleCancelCast(Session& session, StlBuffer& data);

// Execute a completed cast (called from Player when cast time finishes)
void executePendingCast(Player* caster, int32_t spellId, uint32_t targetGuid);

// Handle target selection
void handleSetSelected(Session& session, StlBuffer& data);

// Handle gossip option selection (Phase 7, Task 7.5)
void handleClickedGossipOption(Session& session, StlBuffer& data);

// Quest handlers (Phase 7, Task 7.7)
void handleAcceptQuest(Session& session, StlBuffer& data);
void handleCompleteQuest(Session& session, StlBuffer& data);
void handleAbandonQuest(Session& session, StlBuffer& data);

// Level up spending (Phase 7, Task 7.11)
void handleLevelUp(Session& session, StlBuffer& data);

// Helper: Find target entity by GUID (player or NPC)
Entity* findTargetEntity(uint32_t guid);

// Helper: Send spell cast result to caster
void sendCastError(Session& session, int32_t spellId, int32_t errorCode);

// Helper: Send spell execution to all relevant players
void sendSpellGo(Player* caster, int32_t spellId, const std::vector<std::pair<Entity*, uint8_t>>& targets);

// ============================================================================
// Inventory Handlers (Phase 6, Task 6.2)
// ============================================================================

// Handle moving an item between inventory slots
void handleMoveItem(Session& session, StlBuffer& data);

// Handle splitting a stack of items
void handleSplitItemStack(Session& session, StlBuffer& data);

// Handle destroying an item
void handleDestroyItem(Session& session, StlBuffer& data);

// ============================================================================
// Equipment Handlers (Phase 6, Task 6.3)
// ============================================================================

// Handle equipping an item from inventory
void handleEquipItem(Session& session, StlBuffer& data);

// Handle unequipping an item to inventory
void handleUnequipItem(Session& session, StlBuffer& data);

// Helper: Send equipment state to client
void sendEquipment(Session& session);

// ============================================================================
// Loot Handlers (Phase 6, Task 6.4)
// ============================================================================

// Handle looting an item from a target (corpse, chest, etc.)
void handleLootItem(Session& session, StlBuffer& data);

// ============================================================================
// Vendor Handlers (Phase 6, Task 6.5)
// ============================================================================

// Handle buying an item from a vendor
void handleBuyVendorItem(Session& session, StlBuffer& data);

// Handle selling an item to a vendor
void handleSellItem(Session& session, StlBuffer& data);

// Handle buying back a previously sold item
void handleBuyback(Session& session, StlBuffer& data);

// ============================================================================
// Bank Handlers (Phase 6, Task 6.6)
// ============================================================================

// Handle opening bank (via NPC interaction)
void handleOpenBank(Session& session, StlBuffer& data);

// Handle moving an item from inventory to bank
void handleMoveInventoryToBank(Session& session, StlBuffer& data);

// Handle moving an item within bank slots
void handleMoveBankToBank(Session& session, StlBuffer& data);

// Handle withdrawing an item from bank to inventory
void handleUnBankItem(Session& session, StlBuffer& data);

// Handle sorting bank items
void handleSortBank(Session& session, StlBuffer& data);

// Helper: Send full bank contents to client
void sendBank(Session& session);

// ============================================================================
// Trade Handlers (Phase 6, Task 6.7)
// ============================================================================

// Handle initiating a trade with another player
void handleOpenTradeWith(Session& session, StlBuffer& data);

// Handle adding an item to trade
void handleTradeAddItem(Session& session, StlBuffer& data);

// Handle removing an item from trade
void handleTradeRemoveItem(Session& session, StlBuffer& data);

// Handle setting gold amount in trade
void handleTradeSetGold(Session& session, StlBuffer& data);

// Handle confirming trade
void handleTradeConfirm(Session& session, StlBuffer& data);

// Handle canceling trade
void handleTradeCancel(Session& session, StlBuffer& data);

// ============================================================================
// Item Use Handlers (Phase 6, Task 6.8)
// ============================================================================

// Handle using a consumable item from inventory
void handleUseItem(Session& session, StlBuffer& data);

// ============================================================================
// Repair Handlers (Phase 6, Task 6.9)
// ============================================================================

// Handle equipment repair request
void handleRepair(Session& session, StlBuffer& data);

// ============================================================================
// Additional Handlers (Phase 9 - Missing Opcodes)
// ============================================================================

// Handle request for ability list (sends spellbook)
void handleReqAbilityList(Session& session, StlBuffer& data);

// Handle PvP flag toggle
void handleTogglePvP(Session& session, StlBuffer& data);

// Handle respec (talent/stat reset)
void handleRespec(Session& session, StlBuffer& data);

// Handle waypoint query (returns discovered waypoints)
void handleQueryWaypoints(Session& session, StlBuffer& data);

// Handle waypoint activation (teleport to waypoint)
void handleActivateWaypoint(Session& session, StlBuffer& data);

// Handle dungeon lockout reset
void handleResetDungeons(Session& session, StlBuffer& data);

// Handle socketing a gem into an item
void handleSocketItem(Session& session, StlBuffer& data);

// Handle empowering an item with materials
void handleEmpowerItem(Session& session, StlBuffer& data);

// Handle sorting inventory
void handleSortInventory(Session& session, StlBuffer& data);

} // namespace Handlers
