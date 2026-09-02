// Packet Router Framework - Routes packets to handlers with state validation
// Task 2.5: Packet Dispatch System

#include "stdafx.h"
#include "Network/PacketRouter.h"
#include "Handlers/AuthHandlers.h"
#include "Handlers/CharacterHandlers.h"
#include "Handlers/WorldHandlers.h"
#include "Handlers/MiscHandlers.h"
#include "Core/Logger.h"
#include "GamePacketBase.h"

// Opcode name lookup table
const char* getOpcodeName(uint16_t opcode)
{
    switch (opcode) {
        // Bidirectional
        case Opcode::Mutual_Ping: return "Mutual_Ping";

        // Server -> Client
        case Opcode::Server_Validate: return "Server_Validate";
        case Opcode::Server_QueuePosition: return "Server_QueuePosition";
        case Opcode::Server_CharacterList: return "Server_CharacterList";
        case Opcode::Server_CharaCreateResult: return "Server_CharaCreateResult";
        case Opcode::Server_NewWorld: return "Server_NewWorld";
        case Opcode::Server_SetController: return "Server_SetController";
        case Opcode::Server_ChannelInfo: return "Server_ChannelInfo";
        case Opcode::Server_ChannelChangeConfirm: return "Server_ChannelChangeConfirm";
        case Opcode::Server_Player: return "Server_Player";
        case Opcode::Server_Npc: return "Server_Npc";
        case Opcode::Server_GameObject: return "Server_GameObject";
        case Opcode::Server_DestroyObject: return "Server_DestroyObject";
        case Opcode::Server_SetSubname: return "Server_SetSubname";
        case Opcode::Server_UnitSpline: return "Server_UnitSpline";
        case Opcode::Server_UnitTeleport: return "Server_UnitTeleport";
        case Opcode::Server_UnitOrientation: return "Server_UnitOrientation";
        case Opcode::Server_CastStart: return "Server_CastStart";
        case Opcode::Server_CastStop: return "Server_CastStop";
        case Opcode::Server_SpellGo: return "Server_SpellGo";
        case Opcode::Server_CombatMsg: return "Server_CombatMsg";
        case Opcode::Server_UnitAuras: return "Server_UnitAuras";
        case Opcode::Server_Cooldown: return "Server_Cooldown";
        case Opcode::Server_ObjectVariable: return "Server_ObjectVariable";
        case Opcode::Server_AggroMob: return "Server_AggroMob";
        case Opcode::Server_Inventory: return "Server_Inventory";
        case Opcode::Server_Bank: return "Server_Bank";
        case Opcode::Server_OpenBank: return "Server_OpenBank";
        case Opcode::Server_EquipItem: return "Server_EquipItem";
        case Opcode::Server_NotifyItemAdd: return "Server_NotifyItemAdd";
        case Opcode::Server_OpenLootWindow: return "Server_OpenLootWindow";
        case Opcode::Server_OnObjectWasLooted: return "Server_OnObjectWasLooted";
        case Opcode::Server_UpdateVendorStock: return "Server_UpdateVendorStock";
        case Opcode::Server_RepairCost: return "Server_RepairCost";
        case Opcode::Server_SocketResult: return "Server_SocketResult";
        case Opcode::Server_EmpowerResult: return "Server_EmpowerResult";
        case Opcode::Server_Spellbook: return "Server_Spellbook";
        case Opcode::Server_Spellbook_Update: return "Server_Spellbook_Update";
        case Opcode::Server_LearnedSpell: return "Server_LearnedSpell";
        case Opcode::Server_ExpNotify: return "Server_ExpNotify";
        case Opcode::Server_LvlResponse: return "Server_LvlResponse";
        case Opcode::Server_SpentGold: return "Server_SpentGold";
        case Opcode::Server_QuestList: return "Server_QuestList";
        case Opcode::Server_AcceptedQuest: return "Server_AcceptedQuest";
        case Opcode::Server_QuestTally: return "Server_QuestTally";
        case Opcode::Server_QuestComplete: return "Server_QuestComplete";
        case Opcode::Server_RewardedQuest: return "Server_RewardedQuest";
        case Opcode::Server_AbandonQuest: return "Server_AbandonQuest";
        case Opcode::Server_AvailableWorldQuests: return "Server_AvailableWorldQuests";
        case Opcode::Server_ChatMsg: return "Server_ChatMsg";
        case Opcode::Server_ChatError: return "Server_ChatError";
        case Opcode::Server_GossipMenu: return "Server_GossipMenu";
        case Opcode::Server_GuildRoster: return "Server_GuildRoster";
        case Opcode::Server_GuildInvite: return "Server_GuildInvite";
        case Opcode::Server_GuildAddMember: return "Server_GuildAddMember";
        case Opcode::Server_GuildRemoveMember: return "Server_GuildRemoveMember";
        case Opcode::Server_GuildOnlineStatus: return "Server_GuildOnlineStatus";
        case Opcode::Server_GuildNotifyRoleChange: return "Server_GuildNotifyRoleChange";
        case Opcode::Server_PartyList: return "Server_PartyList";
        case Opcode::Server_OfferParty: return "Server_OfferParty";
        case Opcode::Server_TradeUpdate: return "Server_TradeUpdate";
        case Opcode::Server_TradeCanceled: return "Server_TradeCanceled";
        case Opcode::Server_QueryWaypointsResponse: return "Server_QueryWaypointsResponse";
        case Opcode::Server_DiscoverWaypointNotify: return "Server_DiscoverWaypointNotify";
        case Opcode::Server_ArenaQueued: return "Server_ArenaQueued";
        case Opcode::Server_ArenaReady: return "Server_ArenaReady";
        case Opcode::Server_ArenaStatus: return "Server_ArenaStatus";
        case Opcode::Server_ArenaOutcome: return "Server_ArenaOutcome";
        case Opcode::Server_OfferDuel: return "Server_OfferDuel";
        case Opcode::Server_PkNotify: return "Server_PkNotify";
        case Opcode::Server_WorldError: return "Server_WorldError";
        case Opcode::Server_InspectReveal: return "Server_InspectReveal";
        case Opcode::Server_PromptRespec: return "Server_PromptRespec";
        case Opcode::Server_RespawnResponse: return "Server_RespawnResponse";
        case Opcode::Server_UnlockGameObj: return "Server_UnlockGameObj";
        case Opcode::Server_MarkNpcsOnMap: return "Server_MarkNpcsOnMap";

        // Client -> Server
        case Opcode::Client_Authenticate: return "Client_Authenticate";
        case Opcode::Client_CharacterList: return "Client_CharacterList";
        case Opcode::Client_CharCreate: return "Client_CharCreate";
        case Opcode::Client_DeleteCharacter: return "Client_DeleteCharacter";
        case Opcode::Client_EnterWorld: return "Client_EnterWorld";
        case Opcode::Client_RequestMove: return "Client_RequestMove";
        case Opcode::Client_RequestStop: return "Client_RequestStop";
        case Opcode::Client_CastSpell: return "Client_CastSpell";
        case Opcode::Client_CancelCast: return "Client_CancelCast";
        case Opcode::Client_SetSelected: return "Client_SetSelected";
        case Opcode::Client_EquipItem: return "Client_EquipItem";
        case Opcode::Client_UnequipItem: return "Client_UnequipItem";
        case Opcode::Client_MoveItem: return "Client_MoveItem";
        case Opcode::Client_SplitItemStack: return "Client_SplitItemStack";
        case Opcode::Client_DestroyItem: return "Client_DestroyItem";
        case Opcode::Client_UseItem: return "Client_UseItem";
        case Opcode::Client_SortInventory: return "Client_SortInventory";
        case Opcode::Client_ReqAbilityList: return "Client_ReqAbilityList";
        case Opcode::Client_OpenBank: return "Client_OpenBank";
        case Opcode::Client_MoveInventoryToBank: return "Client_MoveInventoryToBank";
        case Opcode::Client_MoveBankToBank: return "Client_MoveBankToBank";
        case Opcode::Client_UnBankItem: return "Client_UnBankItem";
        case Opcode::Client_SortBank: return "Client_SortBank";
        case Opcode::Client_BuyVendorItem: return "Client_BuyVendorItem";
        case Opcode::Client_SellItem: return "Client_SellItem";
        case Opcode::Client_Buyback: return "Client_Buyback";
        case Opcode::Client_OpenTradeWith: return "Client_OpenTradeWith";
        case Opcode::Client_TradeAddItem: return "Client_TradeAddItem";
        case Opcode::Client_TradeRemoveItem: return "Client_TradeRemoveItem";
        case Opcode::Client_TradeSetGold: return "Client_TradeSetGold";
        case Opcode::Client_TradeConfirm: return "Client_TradeConfirm";
        case Opcode::Client_TradeCancel: return "Client_TradeCancel";
        case Opcode::Client_Repair: return "Client_Repair";
        case Opcode::Client_AcceptQuest: return "Client_AcceptQuest";
        case Opcode::Client_CompleteQuest: return "Client_CompleteQuest";
        case Opcode::Client_AbandonQuest: return "Client_AbandonQuest";
        case Opcode::Client_ClickedGossipOption: return "Client_ClickedGossipOption";
        case Opcode::Client_ChatMsg: return "Client_ChatMsg";
        case Opcode::Client_SetIgnorePlayer: return "Client_SetIgnorePlayer";
        case Opcode::Client_GuildCreate: return "Client_GuildCreate";
        case Opcode::Client_GuildInviteMember: return "Client_GuildInviteMember";
        case Opcode::Client_GuildInviteResponse: return "Client_GuildInviteResponse";
        case Opcode::Client_GuildQuit: return "Client_GuildQuit";
        case Opcode::Client_GuildKickMember: return "Client_GuildKickMember";
        case Opcode::Client_GuildPromoteMember: return "Client_GuildPromoteMember";
        case Opcode::Client_GuildDemoteMember: return "Client_GuildDemoteMember";
        case Opcode::Client_GuildDisband: return "Client_GuildDisband";
        case Opcode::Client_GuildMotd: return "Client_GuildMotd";
        case Opcode::Client_GuildRosterRequest: return "Client_GuildRosterRequest";
        case Opcode::Client_PartyInviteMember: return "Client_PartyInviteMember";
        case Opcode::Client_PartyInviteResponse: return "Client_PartyInviteResponse";
        case Opcode::Client_PartyChanges: return "Client_PartyChanges";
        case Opcode::Client_DuelResponse: return "Client_DuelResponse";
        case Opcode::Client_YieldDuel: return "Client_YieldDuel";
        case Opcode::Client_TogglePvP: return "Client_TogglePvP";
        case Opcode::Client_UpdateArenaStatus: return "Client_UpdateArenaStatus";
        case Opcode::Client_Respec: return "Client_Respec";
        case Opcode::Client_LevelUp: return "Client_LevelUp";
        case Opcode::Client_QueryWaypoints: return "Client_QueryWaypoints";
        case Opcode::Client_ActivateWaypoint: return "Client_ActivateWaypoint";
        case Opcode::Client_RequestRespawn: return "Client_RequestRespawn";
        case Opcode::Client_ResetDungeons: return "Client_ResetDungeons";
        case Opcode::Client_SocketItem: return "Client_SocketItem";
        case Opcode::Client_EmpowerItem: return "Client_EmpowerItem";
        case Opcode::Client_RollDice: return "Client_RollDice";
        case Opcode::Client_ReportPlayer: return "Client_ReportPlayer";
        case Opcode::Client_MOD: return "Client_MOD";
        case Opcode::Client_RecoverMailLoot: return "Client_RecoverMailLoot";

        default: return "Unknown";
    }
}

PacketRouter& PacketRouter::instance()
{
    static PacketRouter instance;
    return instance;
}

void PacketRouter::registerHandler(
    uint16_t opcode,
    PacketHandler handler,
    SessionState requiredState,
    bool allowHigherStates,
    const char* name)
{
    PacketHandlerInfo info;
    info.handler = std::move(handler);
    info.requiredState = requiredState;
    info.allowHigherStates = allowHigherStates;
    info.name = name ? name : getOpcodeName(opcode);

    m_handlers[opcode] = std::move(info);
}

void PacketRouter::dispatch(Session& session, uint16_t opcode, StlBuffer& data)
{
    // Don't process packets for disconnecting sessions
    if (session.isDisconnecting()) {
        LOG_DEBUG("Session %u: Ignoring packet %s (disconnecting)",
                  session.getId(), getOpcodeName(opcode));
        return;
    }

    // Find handler
    auto it = m_handlers.find(opcode);
    if (it == m_handlers.end()) {
        LOG_WARN("Session %u: Unknown opcode %u (%s)",
                 session.getId(), opcode, getOpcodeName(opcode));
        return;
    }

    const PacketHandlerInfo& info = it->second;

    // Validate session state
    if (!checkState(session, info)) {
        LOG_WARN("Session %u: Invalid state for %s (state=%s, required=%s)",
                 session.getId(),
                 info.name.c_str(),
                 sessionStateToString(session.getState()),
                 sessionStateToString(info.requiredState));

        // Optionally disconnect on invalid state packets
        // session.initiateDisconnect("Invalid packet state");
        return;
    }

    // Update activity timestamp
    session.updateLastActivity();

    // Log at debug level (can be noisy with movement packets)
    LOG_DEBUG("Session %u: Handling %s (size=%zu)",
              session.getId(), info.name.c_str(), data.size());

    // Call the handler with comprehensive error handling
    try {
        info.handler(session, data);
    } catch (const std::exception& e) {
        LOG_ERROR("Session %u: Exception in handler %s: %s",
                  session.getId(), info.name.c_str(), e.what());
        // Server continues running - don't disconnect for handler errors
    } catch (...) {
        LOG_ERROR("Session %u: Unknown exception in handler %s",
                  session.getId(), info.name.c_str());
        // Server continues running - don't disconnect for handler errors
    }
}

void PacketRouter::initialize()
{
    // Register all packet handlers
    Handlers::registerMiscHandlers();
    Handlers::registerAuthHandlers();
    Handlers::registerCharacterHandlers();
    Handlers::registerWorldHandlers();

    // Note: Additional handler registrations will be added in later phases
    // - Combat handlers (Phase 5)
    // - Inventory handlers (Phase 6)
    // - etc.

    LOG_INFO("Packet router initialized with %zu handlers", m_handlers.size());
}

bool PacketRouter::hasHandler(uint16_t opcode) const
{
    return m_handlers.find(opcode) != m_handlers.end();
}

bool PacketRouter::checkState(const Session& session, const PacketHandlerInfo& info) const
{
    SessionState current = session.getState();
    SessionState required = info.requiredState;

    // Exact match always works
    if (current == required) {
        return true;
    }

    // Check if higher states are allowed
    if (info.allowHigherStates) {
        return getStateLevel(current) >= getStateLevel(required);
    }

    return false;
}

int PacketRouter::getStateLevel(SessionState state) const
{
    switch (state) {
        case SessionState::Connected:     return 1;
        case SessionState::Authenticated: return 2;
        case SessionState::InWorld:       return 3;
        case SessionState::Disconnecting: return 0;  // Lowest - no packets allowed
        default:                          return 0;
    }
}
