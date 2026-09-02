// World Entry, Movement, and Combat Packet Handlers
// Task 4.5: World Entry Packet Flow
// Task 5.15: Combat Handlers

#include "stdafx.h"
#include "Handlers/WorldHandlers.h"
#include "Network/Session.h"
#include "Network/PacketRouter.h"
#include "Database/CharacterDb.h"
#include "Database/GameData.h"
#include "Database/DatabaseManager.h"
#include "World/Player.h"
#include "World/Npc.h"
#include "World/MapManager.h"
#include "World/WorldManager.h"
#include "World/Map.h"
#include "Combat/SpellCaster.h"
#include "Combat/CombatFormulas.h"
#include "Combat/CombatMessenger.h"
#include "Combat/SpellUtils.h"
#include "Systems/Inventory.h"
#include "Systems/Equipment.h"
#include "Systems/LootSystem.h"
#include "Systems/VendorSystem.h"
#include "Systems/BankSystem.h"
#include "Systems/TradeSystem.h"
#include "Systems/GossipSystem.h"
#include "Systems/ExperienceSystem.h"
#include "Systems/QuestManager.h"
#include "Systems/ChatSystem.h"
#include "Systems/PartySystem.h"
#include "Systems/GuildSystem.h"
#include "Systems/DuelSystem.h"
#include "Core/Logger.h"
#include "SpellDefines.h"
#include "PlayerDefines.h"
#include "QuestDefines.h"
#include "GamePacketBase.h"
#include "GamePacketClient.h"
#include "GamePacketServer.h"

#include <cmath>
#include <queue>
#include <vector>

namespace Handlers
{

// ============================================================================
// Helper: Send world error to player
// ============================================================================

static void sendWorldError(Player* player, PlayerDefines::WorldError error, const std::string& message = "")
{
    if (!player)
        return;

    GP_Server_WorldError errorPacket;
    errorPacket.m_code = static_cast<uint8_t>(error);
    errorPacket.m_message = message;

    StlBuffer buf;
    uint16_t opcode = errorPacket.getOpcode();
    buf << opcode;
    errorPacket.pack(buf);
    player->sendPacket(buf);
}

// ============================================================================
// Helper: Build GP_Server_Player packet from Player entity
// ============================================================================

static void buildPlayerPacket(GP_Server_Player& packet, const Player* player)
{
    packet.m_guid = static_cast<uint32_t>(player->getGuid());
    packet.m_name = player->getName();
    packet.m_subName = "";  // No subtitle for players initially
    packet.m_classId = static_cast<uint8_t>(player->getClassId());
    packet.m_gender = static_cast<uint8_t>(player->getGender());
    packet.m_portraitId = player->getPortraitId();
    packet.m_x = player->getX();
    packet.m_y = player->getY();
    packet.m_orientation = player->getOrientation();

    // Populate equipment (Phase 6.3)
    const auto& equipSlots = player->getEquipment().getSlots();
    for (int slot = 0; slot < static_cast<int>(equipSlots.size()); ++slot)
    {
        const auto& item = equipSlots[slot];
        if (!item.isEmpty())
        {
            packet.m_equipment[slot] = item.itemId;
        }
    }

    // Add current variables (health, stats, progression, etc.)
    packet.m_variables = player->getVariables();
}

// ============================================================================
// Packet sending helpers
// ============================================================================

void sendWorldEntry(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_ERROR("Session %u: sendWorldEntry called without player", session.getId());
        return;
    }

    // 1. Send NewWorld packet (tells client which map to load)
    {
        GP_Server_NewWorld packet;
        packet.m_mapId = player->getMapId();

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        session.sendPacket(buf);

        LOG_DEBUG("Session %u: Sent NewWorld (mapId=%d)", session.getId(), packet.m_mapId);
    }

    // 2. Send Player packet BEFORE SetController.
    // The released r1189 client resolves the controller GUID immediately; if
    // the corresponding ClientPlayer has not been received yet, it logs out.
    sendPlayerSelf(session);

    // 3. Tell the client which already-created player entity it controls.
    {
        GP_Server_SetController packet;
        packet.m_guid = static_cast<uint32_t>(player->getGuid());

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        session.sendPacket(buf);

        LOG_DEBUG("Session %u: Sent SetController (guid=%u)", session.getId(), packet.m_guid);
    }

    // 3b. Send ChannelInfo packet (server instance info for minimap display).
    {
        GP_Server_ChannelInfo packet;
        packet.m_myChannel = 0;  // Single server - always channel 0
        packet.m_channelSize = static_cast<int32_t>(sWorldManager.getPlayerCount());
        packet.m_channels.push_back(packet.m_channelSize);  // Just one channel

        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        session.sendPacket(buf);

        LOG_DEBUG("Session %u: Sent ChannelInfo (channel=0, size=%d)",
                  session.getId(), packet.m_channelSize);
    }

    // 4. Send Inventory
    sendInventory(session);

    // 5. Send Equipment (Task 6.3)
    sendEquipment(session);

    // 6. Send Spellbook
    sendSpellbook(session);

    // 6. Send Quest List
    sendQuestList(session);

    // 7. Send active cooldowns (Task 5.7)
    sendCooldowns(session);

    // 8. Send active auras (Task 5.8)
    sendAuras(session);

    // 9. Load ignore list for chat system (Phase 8, Task 8.1)
    sChatManager.loadIgnoreList(player);

    // 10. Notify guild system of login (Phase 8, Task 8.6)
    sGuildManager.onPlayerLogin(player);
}

void sendPlayerSelf(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Server_Player packet;
    buildPlayerPacket(packet, player);

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    session.sendPacket(buf);

    LOG_DEBUG("Session %u: Sent Player self packet (guid=%u, name='%s')",
              session.getId(), packet.m_guid, packet.m_name.c_str());
}

void sendInventory(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: sendInventory called without player", session.getId());
        return;
    }

    // Delegate to the Player's sendInventory method which has the real implementation
    player->sendInventory();
}

void sendEquipment(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: sendEquipment called without player", session.getId());
        return;
    }

    // Delegate to the Player's sendEquipment method
    player->sendEquipment();
}

void sendSpellbook(Session& session)
{
    GP_Server_Spellbook packet;

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: sendSpellbook called without player", session.getId());
        return;
    }

    // Backfill the released game's class starter spells for old development
    // characters as well as newly created ones. This is idempotent.
    CharacterDb::ensureStartingSpells(player->getCharacterGuid(), player->getClassId());

    // Load spells from character_spells table
    auto stmt = sDatabase.prepare(
        "SELECT spell_id, rank FROM character_spells WHERE character_guid = ?"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("Spellbook: Failed to prepare load statement");
        // Send empty spellbook
        StlBuffer buf;
        uint16_t opcode = packet.getOpcode();
        buf << opcode;
        packet.pack(buf);
        session.sendPacket(buf);
        return;
    }

    stmt.bind(1, player->getCharacterGuid());

    int32_t investedSpells = 0;
    while (stmt.step())
    {
        int32_t spellId = stmt.getInt(0);
        int32_t rank = stmt.getInt(1);

        // Create spell slot
        GP_Server_Spellbook::SpellSlot slot;
        slot.spellId = spellId;
        slot.level = static_cast<uint8_t>(rank > 0 ? rank : 1);

        // Get spell template for base points calculation
        const SpellTemplate* spellTmpl = sGameData.getSpell(spellId);
        if (spellTmpl)
        {
            if (spellTmpl->canLevelUp != 0 && rank > 1)
                investedSpells += (rank - 1);

            // Add base points from effect data
            // effectData1[i] and effectData2[i] form pairs for each effect
            for (int i = 0; i < 3; ++i)
            {
                if (spellTmpl->effect[i] != 0)
                {
                    slot.bpoints.emplace_back(
                        static_cast<int16_t>(spellTmpl->effectData1[i]),
                        static_cast<int16_t>(spellTmpl->effectData2[i])
                    );
                }
            }
        }

        packet.m_slots.push_back(slot);
    }

    if (packet.m_slots.empty())
    {
        LOG_WARN("Spellbook: Character %d (class %d) has no learned spells after starter migration",
                 player->getCharacterGuid(), player->getClassId());
    }

    player->setVariable(ObjDefines::Variable::NumInvestedSpells, investedSpells);
    player->broadcastVariable(ObjDefines::Variable::NumInvestedSpells, investedSpells);

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    session.sendPacket(buf);

    LOG_DEBUG("Session %u: Sent Spellbook (%zu spells)",
              session.getId(), packet.m_slots.size());
}

static void sendSpellbookUpdate(Player* player, int32_t spellId, int32_t level)
{
    if (!player)
        return;

    GP_Server_Spellbook_Update packet;
    packet.m_spellId = spellId;
    packet.m_level = static_cast<uint8_t>(level);

    const SpellTemplate* spellTmpl = sGameData.getSpell(spellId);
    if (spellTmpl)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (spellTmpl->effect[i] != 0)
            {
                packet.m_bpoints.emplace_back(
                    static_cast<int16_t>(spellTmpl->effectData1[i]),
                    static_cast<int16_t>(spellTmpl->effectData2[i])
                );
            }
        }
    }

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);
}

void sendQuestList(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Server_QuestList packet;

    for (const auto& [questId, state] : player->getQuestLog().getQuests())
    {
        if (state.status == QuestDefines::Status::Rewarded)
            continue;

        GP_Server_QuestList::Quest questEntry;
        questEntry.id = questId;
        questEntry.done = (state.status == QuestDefines::Status::Complete);

        const QuestTemplate* quest = sGameData.getQuest(questId);
        if (quest)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (quest->reqItem[i] > 0)
                    questEntry.tallyItems[quest->reqItem[i]] = state.progress[i];
                else if (quest->reqNpc[i] > 0)
                    questEntry.tallyNpcs[quest->reqNpc[i]] = state.progress[i];
                else if (quest->reqGo[i] > 0)
                    questEntry.tallyGameObjects[quest->reqGo[i]] = state.progress[i];
                else if (quest->reqSpell[i] > 0)
                    questEntry.tallySpells[quest->reqSpell[i]] = state.progress[i];
            }
        }

        packet.m_quests.push_back(questEntry);
    }

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    session.sendPacket(buf);

    LOG_DEBUG("Session %u: Sent QuestList (%zu quests)",
              session.getId(), packet.m_quests.size());
}

void sendCooldowns(Session& session)
{
    // Task 5.7: Send active cooldowns on login
    Player* player = session.getPlayer();
    if (!player)
        return;

    player->sendAllCooldowns();
}

void sendAuras(Session& session)
{
    // Task 5.8: Send active auras on login
    Player* player = session.getPlayer();
    if (!player)
        return;

    // Broadcast current aura state to the player
    player->getAuras().broadcastAuras();
}

// ============================================================================
// Handlers
// ============================================================================

void handleEnterWorld(Session& session, StlBuffer& data)
{
    // Parse the incoming packet
    GP_Client_EnterWorld packet;
    packet.unpack(data);

    LOG_INFO("Session %u: EnterWorld request for character GUID %u",
             session.getId(), packet.m_playerGuid);

    // Security check: Verify the character belongs to this session's account
    auto characterInfo = CharacterDb::getCharacterByGuid(static_cast<int32_t>(packet.m_playerGuid));

    if (!characterInfo)
    {
        LOG_WARN("Session %u: EnterWorld - character GUID %u not found",
                 session.getId(), packet.m_playerGuid);

        // Send error and return to character select
        GP_Server_WorldError errorPacket;
        errorPacket.m_code = 1;  // Generic error
        errorPacket.m_message = "Character not found";

        StlBuffer buf;
        uint16_t opcode = errorPacket.getOpcode();
        buf << opcode;
        errorPacket.pack(buf);
        session.sendPacket(buf);
        return;
    }

    // Verify ownership - critical security check!
    if (characterInfo->accountId != static_cast<int32_t>(session.getAccountId()))
    {
        LOG_WARN("Session %u: EnterWorld - character GUID %u belongs to account %d, not %u",
                 session.getId(), packet.m_playerGuid,
                 characterInfo->accountId, session.getAccountId());

        GP_Server_WorldError errorPacket;
        errorPacket.m_code = 2;  // Not your character
        errorPacket.m_message = "Character does not belong to this account";

        StlBuffer buf;
        uint16_t opcode = errorPacket.getOpcode();
        buf << opcode;
        errorPacket.pack(buf);
        session.sendPacket(buf);

        // This is suspicious - log at higher level
        LOG_ERROR("SECURITY: Session %u (account %u) attempted to access character %u (account %d)",
                  session.getId(), session.getAccountId(),
                  packet.m_playerGuid, characterInfo->accountId);
        return;
    }

    // Clean up any existing player (shouldn't happen, but be safe)
    if (session.getPlayer())
    {
        LOG_WARN("Session %u: Already has a player, cleaning up", session.getId());
        session.clearPlayer();
    }

    // Create the Player entity from character info
    Player* player = new Player(session, *characterInfo);

    // Set the unique GUID for the entity (use character GUID)
    player->setGuid(packet.m_playerGuid);

    // Bind player to session
    session.setPlayer(player);
    session.setPlayerGuid(packet.m_playerGuid);

    // Get the map and add player to it
    Map* map = sMapManager.getMap(characterInfo->mapId);
    if (!map)
    {
        LOG_ERROR("Session %u: Map %d not found for character '%s'",
                  session.getId(), characterInfo->mapId, characterInfo->name.c_str());

        // Fall back to default starting map
        map = sMapManager.getMap(sMapManager.getDefaultStartMapId());
        if (map)
        {
            float startX, startY, orientation;
            sMapManager.getStartPosition(sMapManager.getDefaultStartMapId(), startX, startY, orientation);
            player->setPosition(sMapManager.getDefaultStartMapId(), startX, startY);
            player->setOrientation(orientation);
        }
    }

    if (map)
    {
        // v0.2.0-v0.2.3 created new characters at the temporary reconstruction
        // coordinate (100,100).  Migrate that exact placeholder once so
        // existing test characters do not remain stuck in the trees after the
        // canonical Fanadin start point was recovered from game.db.
        constexpr float LEGACY_START_X = 100.0f;
        constexpr float LEGACY_START_Y = 100.0f;
        constexpr float LEGACY_EPSILON = 0.01f;
        if (player->getMapId() == sMapManager.getDefaultStartMapId() &&
            std::fabs(player->getX() - LEGACY_START_X) <= LEGACY_EPSILON &&
            std::fabs(player->getY() - LEGACY_START_Y) <= LEGACY_EPSILON)
        {
            float startX = player->getX();
            float startY = player->getY();
            float startO = player->getOrientation();
            if (sMapManager.getStartPosition(player->getMapId(), startX, startY, startO) &&
                map->isWalkable(static_cast<int>(std::floor(startX)),
                                static_cast<int>(std::floor(startY))))
            {
                LOG_INFO("Session %u: Migrating legacy placeholder spawn for '%s' "
                         "from (%.1f, %.1f) to canonical start (%.1f, %.1f)",
                         session.getId(), player->getName().c_str(),
                         player->getX(), player->getY(), startX, startY);
                player->setPosition(player->getMapId(), startX, startY);
                player->setOrientation(startO);
                CharacterDb::updatePosition(characterInfo->guid, player->getMapId(),
                                            startX, startY, startO);
            }
        }

        player->setMap(map);
        player->setSpawned(true);

        LOG_INFO("Session %u: Player '%s' spawned on map %d at (%.1f, %.1f)",
                 session.getId(), player->getName().c_str(),
                 player->getMapId(), player->getX(), player->getY());
    }

    // Update last login time in database
    CharacterDb::updateLastLogin(characterInfo->guid);

    // Send all world entry packets
    sendWorldEntry(session);

    // Spawn player in world (broadcasts to other players on same map)
    sWorldManager.spawnPlayer(player);

    // Session state is now InWorld (set by setPlayer)
    LOG_INFO("Session %u: Player '%s' (GUID %u) entered world on map %d",
             session.getId(), player->getName().c_str(),
             player->getGuid(), player->getMapId());
}

// ============================================================================
// Movement Handlers (Task 4.8)
// ============================================================================

static bool buildAuthoritativeMovePath(Map* map,
                                       float startX, float startY,
                                       float destX, float destY,
                                       std::vector<Geo2d::Vector2>& outPath)
{
    outPath.clear();
    if (!map || !std::isfinite(startX) || !std::isfinite(startY) ||
        !std::isfinite(destX) || !std::isfinite(destY))
        return false;

    const int width = map->getWidth();
    const int height = map->getHeight();
    const int sx = static_cast<int>(std::floor(startX));
    const int sy = static_cast<int>(std::floor(startY));
    const int ex = static_cast<int>(std::floor(destX));
    const int ey = static_cast<int>(std::floor(destY));

    auto inBounds = [width, height](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height;
    };
    auto walkable = [map, &inBounds](int x, int y) {
        return inBounds(x, y) && map->isWalkable(x, y);
    };

    if (!inBounds(sx, sy) || !walkable(ex, ey))
        return false;

    // Fast path: if every crossed map cell is walkable, retain the exact
    // requested destination. This keeps ordinary WASD/click movement smooth.
    auto directWalkable = [&]() {
        int x0 = sx, y0 = sy;
        const int dx = std::abs(ex - sx);
        const int dy = -std::abs(ey - sy);
        const int stepX = sx < ex ? 1 : -1;
        const int stepY = sy < ey ? 1 : -1;
        int err = dx + dy;
        while (true)
        {
            if (!(x0 == sx && y0 == sy) && !walkable(x0, y0))
                return false;
            if (x0 == ex && y0 == ey)
                return true;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += stepX; }
            if (e2 <= dx) { err += dx; y0 += stepY; }
        }
    };

    if (directWalkable())
    {
        outPath.push_back({destX, destY});
        return true;
    }

    // Collision-safe 8-way BFS. We cap the request distance in the packet
    // handler, so this remains bounded even on the 700x700 outdoor map.
    const int total = width * height;
    const int startId = sy * width + sx;
    const int endId = ey * width + ex;
    std::vector<int> parent(static_cast<size_t>(total), -1);
    std::queue<int> open;
    parent[startId] = startId;
    open.push(startId);

    static constexpr int dirs[8][2] = {
        { 1, 0}, {-1, 0}, {0, 1}, {0,-1},
        { 1, 1}, { 1,-1}, {-1,1}, {-1,-1}
    };

    bool found = false;
    while (!open.empty())
    {
        const int current = open.front();
        open.pop();
        if (current == endId) { found = true; break; }

        const int cx = current % width;
        const int cy = current / width;
        for (const auto& d : dirs)
        {
            const int nx = cx + d[0];
            const int ny = cy + d[1];
            if (!walkable(nx, ny))
                continue;

            // Do not cut through the corner of two blocked cells.
            if (d[0] != 0 && d[1] != 0 &&
                (!walkable(cx + d[0], cy) || !walkable(cx, cy + d[1])))
                continue;

            const int nid = ny * width + nx;
            if (parent[nid] != -1)
                continue;
            parent[nid] = current;
            open.push(nid);
        }
    }

    if (!found)
        return false;

    std::vector<int> reverseIds;
    for (int cur = endId; cur != startId; cur = parent[cur])
    {
        if (cur < 0 || parent[cur] < 0)
            return false;
        reverseIds.push_back(cur);
    }

    outPath.reserve(reverseIds.size());
    for (auto it = reverseIds.rbegin(); it != reverseIds.rend(); ++it)
    {
        const int x = *it % width;
        const int y = *it / width;
        outPath.push_back({x + 0.5f, y + 0.5f});
    }

    // Preserve the client's exact sub-cell destination on the final cell.
    if (!outPath.empty())
        outPath.back() = {destX, destY};
    return !outPath.empty();
}

void handleRequestMove(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: RequestMove without player", session.getId());
        return;
    }

    GP_Client_RequestMove packet;
    packet.unpack(data);

    const float destX = packet.m_destX;
    const float destY = packet.m_destY;
    if (!std::isfinite(destX) || !std::isfinite(destY))
    {
        LOG_WARN("Session %u: Player '%s' sent non-finite movement destination",
                 session.getId(), player->getName().c_str());
        player->broadcastTeleport();
        return;
    }

    // This packet is a destination request, not a position update. A practical
    // cap prevents a modified client from using click-to-move as teleport.
    constexpr float MAX_REQUEST_DISTANCE = 64.0f; // logical map cells
    const float distance = player->distanceTo(destX, destY);
    if (distance > MAX_REQUEST_DISTANCE)
    {
        LOG_WARN("Session %u: Player '%s' move rejected - %.2f cells exceeds %.2f",
                 session.getId(), player->getName().c_str(), distance, MAX_REQUEST_DISTANCE);
        player->broadcastTeleport();
        return;
    }

    Map* map = sMapManager.getMap(player->getMapId());
    std::vector<Geo2d::Vector2> path;
    if (!buildAuthoritativeMovePath(map, player->getX(), player->getY(), destX, destY, path))
    {
        LOG_DEBUG("Session %u: Player '%s' movement rejected by map collision/pathing",
                  session.getId(), player->getName().c_str());
        player->broadcastTeleport();
        return;
    }

    player->updateOrientationFromMovement(destX, destY);
    player->setMovementPath(path);
    broadcastMovement(player, path);

    LOG_DEBUG("Session %u: Player '%s' authoritative path accepted to (%.2f, %.2f), nodes=%zu",
              session.getId(), player->getName().c_str(), destX, destY, path.size());
}

void handleRequestStop(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: RequestStop without player", session.getId());
        return;
    }

    GP_Client_RequestStop packet;
    packet.unpack(data);

    // The released client predicts a small amount ahead between 20 Hz server
    // ticks.  Reconcile only a tightly bounded, collision-safe lead so releasing
    // WASD does not visibly snap the character backwards.  This never trusts an
    // arbitrary client position: the allowed lead is at most 150 ms of the
    // server-owned movement speed and must be reachable through walkable cells.
    const float oldX = player->getX();
    const float oldY = player->getY();
    bool reconciledPrediction = false;

    if (std::isfinite(packet.m_myX) && std::isfinite(packet.m_myY))
    {
        const float advisoryDistance = player->distanceTo(packet.m_myX, packet.m_myY);
        const float maxLeadDistance = std::max(0.25f, player->getMovementSpeed() * 0.15f);

        if (advisoryDistance <= maxLeadDistance)
        {
            Map* map = sMapManager.getMap(player->getMapId());
            std::vector<Geo2d::Vector2> reconcilePath;
            const bool samePosition = advisoryDistance <= 0.0001f;
            if (samePosition || buildAuthoritativeMovePath(
                    map, player->getX(), player->getY(), packet.m_myX, packet.m_myY, reconcilePath))
            {
                if (!samePosition)
                {
                    player->updateOrientationFromMovement(packet.m_myX, packet.m_myY);
                    player->setPosition(packet.m_myX, packet.m_myY);
                    sWorldManager.onPlayerMoved(player, oldX, oldY);
                }
                reconciledPrediction = true;
            }
        }
    }

    player->clearMovementPath();
    player->markDirty();

    // An empty authoritative spline is sufficient to stop both the local and
    // remote client.  Do not follow it with a second teleport packet; the old
    // double-correction was a visible stop hitch.
    broadcastStop(player);

    LOG_DEBUG("Session %u: Player '%s' stopped at (%.2f, %.2f), prediction=%s",
              session.getId(), player->getName().c_str(), player->getX(), player->getY(),
              reconciledPrediction ? "reconciled" : "server");
}

void broadcastMovement(Player* player, const std::vector<Geo2d::Vector2>& path)
{
    if (!player || path.empty())
        return;

    GP_Server_UnitSpline packet;
    packet.m_guid = static_cast<uint32_t>(player->getGuid());
    packet.m_startX = player->getX();
    packet.m_startY = player->getY();
    packet.m_spline = path;
    packet.m_slide = false;
    packet.m_silent = false;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Include the controller: its local prediction is advisory and the server's
    // accepted path is the source of truth.
    sWorldManager.broadcastToVisible(player, buf, true);
}

void broadcastMovement(Player* player, float destX, float destY)
{
    std::vector<Geo2d::Vector2> path{{destX, destY}};
    broadcastMovement(player, path);
}

void broadcastStop(Player* player)
{
    if (!player)
        return;

    GP_Server_UnitSpline packet;
    packet.m_guid = static_cast<uint32_t>(player->getGuid());
    packet.m_startX = player->getX();
    packet.m_startY = player->getY();
    packet.m_spline.clear();
    packet.m_slide = false;
    packet.m_silent = false;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    sWorldManager.broadcastToVisible(player, buf, true);
}

// ============================================================================
// Respawn Handler (Task 5.12)
// ============================================================================

void handleRequestRespawn(Session& session, StlBuffer& data)
{
    (void)data;  // No payload in respawn request

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: RequestRespawn without player", session.getId());
        return;
    }

    // Validate player is dead
    if (!player->canRespawn())
    {
        LOG_WARN("Session %u: Player '%s' requested respawn but is not dead",
                 session.getId(), player->getName().c_str());

        // Send failure response
        GP_Server_RespawnResponse response;
        response.m_success = false;

        StlBuffer buf;
        uint16_t opcode = response.getOpcode();
        buf << opcode;
        response.pack(buf);
        session.sendPacket(buf);
        return;
    }

    // Perform respawn
    player->respawn();

    LOG_DEBUG("Session %u: Player '%s' respawned",
              session.getId(), player->getName().c_str());
}

// ============================================================================
// Combat Handlers (Task 5.15)
// ============================================================================

Entity* findTargetEntity(uint32_t guid)
{
    if (guid == 0)
        return nullptr;

    // Check if it's an NPC (high bit set)
    if (guid >= 0x80000000)
    {
        return sWorldManager.getNpc(guid);
    }
    else
    {
        return sWorldManager.getPlayer(guid);
    }
}

void sendCastError(Session& session, int32_t spellId, int32_t errorCode)
{
    // Use GP_Server_CastStop to indicate failure
    // The client uses this to show error message
    GP_Server_CastStop packet;
    packet.m_guid = session.getPlayer() ? session.getPlayer()->getGuid() : 0;
    packet.m_spellId = spellId;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    session.sendPacket(buf);

    LOG_DEBUG("Session %u: Cast spell %d failed with error %d",
              session.getId(), spellId, errorCode);
}

void sendSpellGo(Player* caster, int32_t spellId, const std::vector<std::pair<Entity*, uint8_t>>& targets)
{
    if (!caster)
        return;

    GP_Server_SpellGo packet;
    packet.m_casterGuid = caster->getGuid();
    packet.m_spellId = spellId;

    for (const auto& [target, hitResult] : targets)
    {
        if (target)
        {
            packet.m_targets[static_cast<uint32_t>(target->getGuid())] = hitResult;
        }
    }

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Send to caster
    caster->sendPacket(buf);

    // Send to all visible players
    for (Entity* viewer : caster->getVisibleTo())
    {
        if (Player* viewerPlayer = dynamic_cast<Player*>(viewer))
        {
            viewerPlayer->sendPacket(buf);
        }
    }
}

void handleSetSelected(Session& session, StlBuffer& data)
{
    GP_Client_SetSelected packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    // Store selected target on player (for targeting)
    player->setSelectedTarget(packet.m_guid);

    LOG_DEBUG("Session %u: Player '%s' selected target %u",
              session.getId(), player->getName().c_str(), packet.m_guid);

    // If target is a player, send inspection data (Task 8.9)
    if (packet.m_guid != 0)
    {
        Player* target = sWorldManager.getPlayer(packet.m_guid);
        if (target && target != player)
        {
            // Check if target is nearby (within 100 units for inspection)
            constexpr float INSPECT_RANGE = 100.0f;
            if (player->isInRange(target, INSPECT_RANGE))
            {
                // Build inspection packet
                GP_Server_InspectReveal inspectPacket;
                inspectPacket.m_targetGuid = target->getGuid();
                inspectPacket.m_classId = static_cast<uint8_t>(target->getClassId());
                inspectPacket.m_guildName = sGuildManager.getGuildName(target->getGuid());

                // Add equipment (slot -> itemId)
                const auto& equipment = target->getEquipment();
                for (int slot = static_cast<int>(UnitDefines::EquipSlot::Helm);
                     slot < static_cast<int>(UnitDefines::EquipSlot::MaxSlot); ++slot)
                {
                    const auto* item = equipment.getItem(static_cast<UnitDefines::EquipSlot>(slot));
                    if (item && !item->isEmpty())
                    {
                        inspectPacket.m_equipment[slot] = item->itemId;
                    }
                }

                // Add key variables for display
                inspectPacket.m_variables[static_cast<int32_t>(ObjDefines::Variable::Level)] = target->getLevel();
                inspectPacket.m_variables[static_cast<int32_t>(ObjDefines::Variable::MaxHealth)] = target->getMaxHealth();
                inspectPacket.m_variables[static_cast<int32_t>(ObjDefines::Variable::MaxMana)] = target->getMaxMana();
                inspectPacket.m_variables[static_cast<int32_t>(ObjDefines::Variable::Health)] = target->getHealth();
                inspectPacket.m_variables[static_cast<int32_t>(ObjDefines::Variable::Mana)] = target->getMana();

                // Send to inspector
                StlBuffer buf;
                uint16_t opcode = inspectPacket.getOpcode();
                buf << opcode;
                inspectPacket.pack(buf);
                player->sendPacket(buf);

                LOG_DEBUG("Session %u: Sent inspection data for player '%s'",
                          session.getId(), target->getName().c_str());
            }
        }
    }
}

void handleClickedGossipOption(Session& session, StlBuffer& data)
{
    GP_Client_ClickedGossipOption packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    uint32_t npcGuid = player->getGossipTargetGuid();
    Npc* npc = sWorldManager.getNpc(npcGuid);
    if (!npc)
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    sGossipManager.selectOption(player, npc, packet.m_entry);
}

void handleAcceptQuest(Session& session, StlBuffer& data)
{
    GP_Client_AcceptQuest packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    // The released r1189 multi-option gossip path can open QuestOffer without
    // copying the NPC GUID into the panel. Prefer the explicit GUID, but fall
    // back to the server-owned current gossip target for compatibility.
    uint32_t questGiverGuid = packet.m_questGiverGuid;
    if (questGiverGuid == 0)
        questGiverGuid = player->getGossipTargetGuid();

    Npc* npc = sWorldManager.getNpc(questGiverGuid);
    const QuestTemplate* quest = sGameData.getQuest(packet.m_questId);
    if (!npc || !npc->isQuestGiver() || !quest || quest->startNpcEntry != npc->getEntry())
    {
        LOG_WARN("Session %u: AcceptQuest invalid giver=%u quest=%d gossipTarget=%u",
                 session.getId(), packet.m_questGiverGuid, packet.m_questId,
                 player->getGossipTargetGuid());
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    if (!sQuestManager.acceptQuest(player, packet.m_questId))
        return;

    LOG_INFO("Session %u: Player '%s' accepted quest %d from NPC %u",
             session.getId(), player->getName().c_str(), packet.m_questId, questGiverGuid);
    sendQuestList(session);
}

void handleCompleteQuest(Session& session, StlBuffer& data)
{
    GP_Client_CompleteQuest packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    uint32_t questGiverGuid = packet.m_questGiverGuid;
    if (questGiverGuid == 0)
        questGiverGuid = player->getGossipTargetGuid();

    Npc* npc = sWorldManager.getNpc(questGiverGuid);
    const QuestTemplate* quest = sGameData.getQuest(packet.m_questId);
    if (!npc || !npc->isQuestGiver() || !quest || quest->finishNpcEntry != npc->getEntry())
    {
        LOG_WARN("Session %u: CompleteQuest invalid giver=%u quest=%d gossipTarget=%u",
                 session.getId(), packet.m_questGiverGuid, packet.m_questId,
                 player->getGossipTargetGuid());
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    if (!sQuestManager.completeQuest(player, packet.m_questId, packet.m_itemChoiceIdx))
        return;

    sendQuestList(session);
    player->sendInventory();
}

void handleAbandonQuest(Session& session, StlBuffer& data)
{
    GP_Client_AbandonQuest packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    if (!sQuestManager.abandonQuest(player, packet.m_questId))
        return;

    sendQuestList(session);
}

void handleLevelUp(Session& session, StlBuffer& data)
{
    GP_Client_LevelUp packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
        return;

    std::vector<std::pair<UnitDefines::Stat, int32_t>> statInvestments;
    int32_t totalStatPoints = 0;
    for (const auto& [statId, points] : packet.m_statInvestments)
    {
        if (points <= 0)
            continue;
        if (statId <= 0 || statId >= static_cast<int32_t>(UnitDefines::Stat::NumStats))
            continue;

        statInvestments.emplace_back(static_cast<UnitDefines::Stat>(statId), points);
        totalStatPoints += points;
    }

    std::vector<std::pair<int32_t, int32_t>> spellInvestments;
    int32_t totalSpellPoints = 0;
    for (const auto& [spellId, points] : packet.m_spellInvestments)
    {
        if (spellId <= 0 || points <= 0)
            continue;

        const SpellTemplate* tmpl = sGameData.getSpell(spellId);
        if (!tmpl || tmpl->canLevelUp == 0)
            continue;

        spellInvestments.emplace_back(spellId, points);
        totalSpellPoints += points;
    }

    int32_t totalCost = totalStatPoints + totalSpellPoints;
    if (totalCost <= 0)
    {
        GP_Server_LvlResponse response;
        response.m_newLevel = player->getLevel();
        response.m_bool = true;

        StlBuffer buf;
        uint16_t opcode = response.getOpcode();
        buf << opcode;
        response.pack(buf);
        player->sendPacket(buf);
        return;
    }

    if (player->getExperience() < totalCost)
    {
        sendWorldError(player, PlayerDefines::WorldError::NotEnoughExp);

        GP_Server_LvlResponse response;
        response.m_newLevel = player->getLevel();
        response.m_bool = false;

        StlBuffer buf;
        uint16_t opcode = response.getOpcode();
        buf << opcode;
        response.pack(buf);
        player->sendPacket(buf);
        return;
    }

    // Apply stat bonuses
    if (!statInvestments.empty())
    {
        for (const auto& [stat, points] : statInvestments)
        {
            player->addStatBonus(stat, points);
        }

        sExperienceSystem.applyLevelStats(player, player->getLevel(), true, true);
    }

    // Apply spell investments
    std::unordered_map<int32_t, int32_t> currentRanks;
    {
        auto stmt = sDatabase.prepare(
            "SELECT spell_id, rank FROM character_spells WHERE character_guid = ?"
        );
        if (stmt.valid())
        {
            stmt.bind(1, player->getCharacterGuid());
            while (stmt.step())
            {
                currentRanks[stmt.getInt(0)] = stmt.getInt(1);
            }
        }
        else
        {
            LOG_ERROR("LevelUp: Failed to load current spells for %d", player->getCharacterGuid());
        }
    }

    auto updateStmt = sDatabase.prepare(
        "UPDATE character_spells SET rank = ? WHERE character_guid = ? AND spell_id = ?"
    );
    auto insertStmt = sDatabase.prepare(
        "INSERT INTO character_spells (character_guid, spell_id, rank) VALUES (?, ?, ?)"
    );

    for (const auto& [spellId, points] : spellInvestments)
    {
        const SpellTemplate* tmpl = sGameData.getSpell(spellId);
        if (!tmpl)
            continue;

        int32_t currentRank = 0;
        auto itr = currentRanks.find(spellId);
        if (itr != currentRanks.end())
            currentRank = itr->second;

        int32_t newRank = currentRank + points;
        if (newRank < 1)
            newRank = 1;

        if (currentRank > 0 && updateStmt.valid())
        {
            updateStmt.reset();
            updateStmt.bind(1, newRank);
            updateStmt.bind(2, player->getCharacterGuid());
            updateStmt.bind(3, spellId);
            updateStmt.step();
        }
        else if (currentRank == 0 && insertStmt.valid())
        {
            insertStmt.reset();
            insertStmt.bind(1, player->getCharacterGuid());
            insertStmt.bind(2, spellId);
            insertStmt.bind(3, newRank);
            insertStmt.step();

            GP_Server_LearnedSpell learned;
            learned.m_spellId = spellId;

            StlBuffer learnedBuf;
            uint16_t learnedOpcode = learned.getOpcode();
            learnedBuf << learnedOpcode;
            learned.pack(learnedBuf);
            player->sendPacket(learnedBuf);
        }

        currentRanks[spellId] = newRank;
        sendSpellbookUpdate(player, spellId, newRank);
    }

    if (totalSpellPoints > 0)
    {
        int32_t invested = player->getVariable(ObjDefines::Variable::NumInvestedSpells);
        invested += totalSpellPoints;
        player->setVariable(ObjDefines::Variable::NumInvestedSpells, invested);
        player->broadcastVariable(ObjDefines::Variable::NumInvestedSpells, invested);
    }

    // Spend XP
    int32_t remainingXp = player->getExperience() - totalCost;
    if (remainingXp < 0)
        remainingXp = 0;

    player->setExperience(remainingXp);
    player->broadcastVariable(ObjDefines::Variable::Experience, remainingXp);
    player->broadcastVariable(ObjDefines::Variable::Progression, remainingXp);

    GP_Server_LvlResponse response;
    response.m_newLevel = player->getLevel();
    response.m_bool = true;

    StlBuffer buf;
    uint16_t opcode = response.getOpcode();
    buf << opcode;
    response.pack(buf);
    player->sendPacket(buf);
}

void handleCastSpell(Session& session, StlBuffer& data)
{
    GP_Client_CastSpell packet;
    packet.unpack(data);

    Player* caster = session.getPlayer();
    if (!caster)
    {
        LOG_WARN("Session %u: CastSpell with no player", session.getId());
        return;
    }

    int32_t spellId = packet.m_spellId;
    uint32_t targetGuid = packet.m_targetGuid;

    LOG_DEBUG("Session %u: Player '%s' casting spell %d on target %u",
              session.getId(), caster->getName().c_str(), spellId, targetGuid);

    // Handle special static spells (Task 6.4: Loot System)
    if (spellId == SpellDefines::StaticSpells::LootUnit ||
        spellId == SpellDefines::StaticSpells::LootGameObj)
    {
        Entity* target = findTargetEntity(targetGuid);
        if (!target)
        {
            LOG_DEBUG("Session %u: Loot target %u not found", session.getId(), targetGuid);
            return;
        }

        // For LootUnit, target must be dead
        if (spellId == SpellDefines::StaticSpells::LootUnit)
        {
            if (!target->isDead())
            {
                LOG_DEBUG("Session %u: Cannot loot living target %u", session.getId(), targetGuid);
                return;
            }
        }

        // Check distance (must be within interaction range)
        float distance = caster->distanceTo(target);
        if (distance > 100.0f)  // Interaction range
        {
            LOG_DEBUG("Session %u: Target %u too far to loot (%.1f)",
                      session.getId(), targetGuid, distance);
            return;
        }

        // Open loot window
        sLootManager.openLoot(caster, target);
        return;
    }

    // NPC Gossip (Phase 7, Task 7.5)
    if (spellId == SpellDefines::StaticSpells::NpcGossip)
    {
        Entity* target = findTargetEntity(targetGuid);
        Npc* npc = dynamic_cast<Npc*>(target);
        if (!npc || npc->isDead())
        {
            sendWorldError(caster, PlayerDefines::WorldError::InvalidTarget);
            return;
        }

        sGossipManager.showGossip(caster, npc);
        return;
    }

    // Offer Duel (Phase 8, Task 8.8)
    if (spellId == SpellDefines::StaticSpells::OfferDuel)
    {
        Entity* target = findTargetEntity(targetGuid);
        Player* targetPlayer = dynamic_cast<Player*>(target);
        if (!targetPlayer || targetPlayer->isDead())
        {
            sendWorldError(caster, PlayerDefines::WorldError::InvalidTarget);
            return;
        }

        sDuelManager.requestDuel(caster, targetPlayer);
        return;
    }

    // Get spell template
    const SpellTemplate* spell = sGameData.getSpell(spellId);
    if (!spell)
    {
        sendCastError(session, spellId, static_cast<int32_t>(CastResult::UnknownSpell));
        return;
    }

    // Find target entity
    Entity* target = findTargetEntity(targetGuid);

    // Validate cast
    CastResult result = SpellCaster::validateCast(caster, spell, target);
    if (result != CastResult::Success)
    {
        sendCastError(session, spellId, static_cast<int32_t>(result));
        LOG_DEBUG("Session %u: Cast failed - %s", session.getId(), getCastResultString(result));
        return;
    }

    // Get targets for the spell (handles AoE, self-cast, etc.)
    std::vector<Entity*> targets = SpellCaster::getTargets(caster, spell, target);
    if (targets.empty())
    {
        sendCastError(session, spellId, static_cast<int32_t>(CastResult::NoTarget));
        return;
    }

    // For instant spells, execute immediately
    // TODO: Handle cast time spells with GP_Server_CastStart
    if (spell->castTime <= 0)
    {
        // Process spell effects on each target
        std::vector<std::pair<Entity*, uint8_t>> hitTargets;

        for (Entity* effectTarget : targets)
        {
            // Determine spell effect type (use first effect slot)
            SpellDefines::Effects effectType = static_cast<SpellDefines::Effects>(spell->effect[0]);

            if (effectType == SpellDefines::Effects::Damage ||
                effectType == SpellDefines::Effects::MeleeAtk ||
                effectType == SpellDefines::Effects::RangedAtk)
            {
                // Damage spell
                DamageInfo damage = CombatFormulas::calculateDamage(caster, effectTarget, spell, 0);

                // Record hit result
                hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                    CombatMessenger::toPacketHitResult(damage.hitResult))});

                // Apply damage if not a complete miss
                if (damage.hitResult != HitResult::Miss &&
                    damage.hitResult != HitResult::Dodge &&
                    damage.hitResult != HitResult::Parry)
                {
                    effectTarget->takeDamage(damage.finalDamage, caster);

                    // Send combat message
                    CombatMessenger::sendDamageMessage(caster, effectTarget, spellId, damage, effectType);

                    // Add threat to NPCs
                    if (Npc* npc = dynamic_cast<Npc*>(effectTarget))
                    {
                        npc->addThreat(caster, damage.finalDamage);
                    }
                }
                else
                {
                    CombatMessenger::sendMissMessage(caster, effectTarget, spellId, damage.hitResult, effectType);
                }
            }
            else if (effectType == SpellDefines::Effects::Heal ||
                     effectType == SpellDefines::Effects::RestoreMana ||
                     effectType == SpellDefines::Effects::RestoreManaPct)
            {
                // Healing spell
                HealInfo heal = CombatFormulas::calculateHeal(caster, effectTarget, spell, 0);

                hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                    CombatMessenger::toPacketHitResult(heal.hitResult))});

                // Apply heal
                effectTarget->heal(heal.finalHeal, caster);

                // Send combat message
                CombatMessenger::sendHealMessage(caster, effectTarget, spellId, heal);

                // Healing threat: NPCs attacking the healed target should aggro the healer
                if (Player* healedPlayer = dynamic_cast<Player*>(effectTarget))
                {
                    std::vector<Npc*> npcs = sWorldManager.getNpcsOnMap(healedPlayer->getMapId());
                    for (Npc* npc : npcs)
                    {
                        if (!npc || npc->isDead())
                            continue;

                        if (npc->getAIState() != NpcAIState::Combat)
                            continue;

                        if (npc->getTarget() == healedPlayer ||
                            npc->getThreatManager().getThreat(healedPlayer) > 0)
                        {
                            npc->addThreat(caster, heal.finalHeal);
                        }
                    }
                }
            }
            else if (effectType == SpellDefines::Effects::ApplyAura ||
                     effectType == SpellDefines::Effects::ApplyAreaAura)
            {
                // Buff/debuff spell - apply aura using the AuraManager API
                // The AuraManager builds the aura from the spell template
                bool applied = effectTarget->getAuras().applyAura(caster, spell, 0);

                hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});

                if (applied)
                {
                    LOG_DEBUG("Applied aura from spell %d to target %llu",
                              spellId, effectTarget->getGuid());
                }
            }
            else
            {
                // Other effect types - just record as hit
                hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});
            }
        }

        // Send spell execution notification
        sendSpellGo(caster, spellId, hitTargets);

        // Quest progress: spell cast
        sQuestManager.onSpellCast(caster, spellId);

        // Consume mana (calculate from formula)
        int32_t casterLevel = caster->getVariable(ObjDefines::Variable::Level);
        int32_t maxMana = caster->getMaxMana();
        int32_t manaCost = SpellUtils::calculateManaCost(spell, casterLevel, maxMana);
        if (manaCost > 0)
        {
            int32_t currentMana = caster->getMana();
            caster->setVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
            caster->broadcastVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
        }

        // Start cooldown
        caster->getCooldowns().startCooldown(spellId, spell->cooldown);

        LOG_INFO("Session %u: Player '%s' cast spell '%s' on %zu targets",
                 session.getId(), caster->getName().c_str(), spell->name.c_str(), targets.size());
    }
    else
    {
        // Cast time spell - send cast start to caster and nearby players
        GP_Server_CastStart castStart;
        castStart.m_guid = caster->getGuid();
        castStart.m_spellId = spellId;
        castStart.m_timer = spell->castTime;

        StlBuffer buf;
        uint16_t opcode = castStart.getOpcode();
        buf << opcode;
        castStart.pack(buf);
        caster->sendPacket(buf);
        sWorldManager.broadcastToVisible(caster, buf, false);

        // Store pending cast for completion after cast time
        caster->startCast(spellId, targetGuid, static_cast<float>(spell->castTime));

        LOG_INFO("Session %u: Player '%s' started casting spell '%s' (%d ms cast time)",
                 session.getId(), caster->getName().c_str(), spell->name.c_str(), spell->castTime);
    }
}

void handleCancelCast(Session& session, StlBuffer& data)
{
    (void)data;  // No data in cancel cast packet

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: CancelCast with no player", session.getId());
        return;
    }

    if (player->isCasting())
    {
        player->cancelCast();
        LOG_DEBUG("Session %u: Player '%s' cancelled cast",
                  session.getId(), player->getName().c_str());
    }
}

void executePendingCast(Player* caster, int32_t spellId, uint32_t targetGuid)
{
    if (!caster)
        return;

    // Get spell template
    const SpellTemplate* spell = sGameData.getSpell(spellId);
    if (!spell)
    {
        LOG_ERROR("executePendingCast: Unknown spell %d", spellId);
        return;
    }

    // Find target entity
    Entity* target = findTargetEntity(targetGuid);

    // Re-validate (conditions may have changed during cast time)
    CastResult result = SpellCaster::validateCast(caster, spell, target);
    if (result != CastResult::Success)
    {
        sendCastError(caster->getSession(), spellId, static_cast<int32_t>(result));
        LOG_DEBUG("executePendingCast: Cast validation failed - %s", getCastResultString(result));
        return;
    }

    // Get targets for the spell
    std::vector<Entity*> targets = SpellCaster::getTargets(caster, spell, target);
    if (targets.empty())
    {
        sendCastError(caster->getSession(), spellId, static_cast<int32_t>(CastResult::NoTarget));
        return;
    }

    // Process spell effects on each target
    std::vector<std::pair<Entity*, uint8_t>> hitTargets;

    for (Entity* effectTarget : targets)
    {
        // Determine spell effect type
        SpellDefines::Effects effectType = static_cast<SpellDefines::Effects>(spell->effect[0]);

        if (effectType == SpellDefines::Effects::Damage ||
            effectType == SpellDefines::Effects::MeleeAtk ||
            effectType == SpellDefines::Effects::RangedAtk)
        {
            // Damage spell
            DamageInfo damage = CombatFormulas::calculateDamage(caster, effectTarget, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                CombatMessenger::toPacketHitResult(damage.hitResult))});

            if (damage.hitResult != HitResult::Miss &&
                damage.hitResult != HitResult::Dodge &&
                damage.hitResult != HitResult::Parry)
            {
                effectTarget->takeDamage(damage.finalDamage, caster);
                CombatMessenger::sendDamageMessage(caster, effectTarget, spellId, damage, effectType);

                if (Npc* npc = dynamic_cast<Npc*>(effectTarget))
                {
                    npc->addThreat(caster, damage.finalDamage);
                }
            }
            else
            {
                CombatMessenger::sendMissMessage(caster, effectTarget, spellId, damage.hitResult, effectType);
            }
        }
        else if (effectType == SpellDefines::Effects::Heal ||
                 effectType == SpellDefines::Effects::RestoreMana ||
                 effectType == SpellDefines::Effects::RestoreManaPct)
        {
            // Healing spell
            HealInfo heal = CombatFormulas::calculateHeal(caster, effectTarget, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                CombatMessenger::toPacketHitResult(heal.hitResult))});

            effectTarget->heal(heal.finalHeal, caster);
            CombatMessenger::sendHealMessage(caster, effectTarget, spellId, heal);

            if (Player* healedPlayer = dynamic_cast<Player*>(effectTarget))
            {
                std::vector<Npc*> npcs = sWorldManager.getNpcsOnMap(healedPlayer->getMapId());
                for (Npc* npc : npcs)
                {
                    if (!npc || npc->isDead())
                        continue;

                    if (npc->getAIState() != NpcAIState::Combat)
                        continue;

                    if (npc->getTarget() == healedPlayer ||
                        npc->getThreatManager().getThreat(healedPlayer) > 0)
                    {
                        npc->addThreat(caster, heal.finalHeal);
                    }
                }
            }
        }
        else if (effectType == SpellDefines::Effects::ApplyAura ||
                 effectType == SpellDefines::Effects::ApplyAreaAura)
        {
            // Buff/debuff spell
            bool applied = effectTarget->getAuras().applyAura(caster, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});

            if (applied)
            {
                LOG_DEBUG("Applied aura from spell %d to target %llu", spellId, effectTarget->getGuid());
            }
        }
        else
        {
            // Other effect types
            hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});
        }
    }

    // Send spell execution notification
    sendSpellGo(caster, spellId, hitTargets);

    // Quest progress: spell cast
    sQuestManager.onSpellCast(caster, spellId);

    // Consume mana
    int32_t casterLevel = caster->getVariable(ObjDefines::Variable::Level);
    int32_t maxMana = caster->getMaxMana();
    int32_t manaCost = SpellUtils::calculateManaCost(spell, casterLevel, maxMana);
    if (manaCost > 0)
    {
        int32_t currentMana = caster->getMana();
        caster->setVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
        caster->broadcastVariable(ObjDefines::Variable::Mana, currentMana - manaCost);
    }

    // Start cooldown
    caster->getCooldowns().startCooldown(spellId, spell->cooldown);

    LOG_INFO("Player '%s' completed cast of spell '%s' on %zu targets",
             caster->getName().c_str(), spell->name.c_str(), targets.size());
}

// ============================================================================
// Inventory Handlers (Phase 6, Task 6.2)
// ============================================================================

void handleMoveItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: MoveItem without player", session.getId());
        return;
    }

    GP_Client_MoveItem packet;
    packet.unpack(data);

    int fromSlot = packet.m_fromSlot;
    int toSlot = packet.m_toSlot;

    // Validate slot range
    if (fromSlot < 0 || fromSlot >= Inventory::MAX_SLOTS ||
        toSlot < 0 || toSlot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: MoveItem invalid slots (%d -> %d)",
                 session.getId(), fromSlot, toSlot);
        return;
    }

    // Perform the move
    bool success = player->getInventory().moveItem(fromSlot, toSlot);

    if (success)
    {
        LOG_DEBUG("Session %u: Player '%s' moved item from slot %d to %d",
                  session.getId(), player->getName().c_str(), fromSlot, toSlot);

        // Mark for save and send updated inventory to client
        player->markDirty();
        player->sendInventory();
    }
    else
    {
        LOG_DEBUG("Session %u: MoveItem failed (%d -> %d)",
                  session.getId(), fromSlot, toSlot);
    }
}

void handleSplitItemStack(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: SplitItemStack without player", session.getId());
        return;
    }

    GP_Client_SplitItemStack packet;
    packet.unpack(data);

    int fromSlot = packet.m_fromSlot;
    int toSlot = packet.m_toSlot;
    int32_t amount = packet.m_amount;

    // Validate slot range
    if (fromSlot < 0 || fromSlot >= Inventory::MAX_SLOTS ||
        toSlot < 0 || toSlot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: SplitItemStack invalid slots (%d -> %d)",
                 session.getId(), fromSlot, toSlot);
        return;
    }

    // Validate amount
    if (amount <= 0)
    {
        LOG_WARN("Session %u: SplitItemStack invalid amount %d",
                 session.getId(), amount);
        return;
    }

    // Perform the split
    bool success = player->getInventory().splitStack(fromSlot, toSlot, amount);

    if (success)
    {
        LOG_DEBUG("Session %u: Player '%s' split %d items from slot %d to %d",
                  session.getId(), player->getName().c_str(), amount, fromSlot, toSlot);

        // Mark for save and send updated inventory to client
        player->markDirty();
        player->sendInventory();
    }
    else
    {
        LOG_DEBUG("Session %u: SplitItemStack failed (%d items from %d to %d)",
                  session.getId(), amount, fromSlot, toSlot);
    }
}

void handleDestroyItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: DestroyItem without player", session.getId());
        return;
    }

    GP_Client_DestroyItem packet;
    packet.unpack(data);

    int slot = packet.m_slot;

    // Validate slot range
    if (slot < 0 || slot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: DestroyItem invalid slot %d", session.getId(), slot);
        return;
    }

    // Check if there's an item to destroy
    const Inventory::InventoryItem* item = player->getInventory().getItem(slot);
    if (!item)
    {
        LOG_DEBUG("Session %u: DestroyItem - slot %d is empty", session.getId(), slot);
        return;
    }

    // Log what's being destroyed (for debugging/auditing)
    LOG_INFO("Session %u: Player '%s' destroyed item %d (x%d) from slot %d",
             session.getId(), player->getName().c_str(),
             item->itemId, item->stackCount, slot);

    // Destroy the item
    player->getInventory().clearSlot(slot);

    // Mark for save and send updated inventory to client
    player->markDirty();
    player->sendInventory();
}

// ============================================================================
// Equipment Handlers (Phase 6, Task 6.3)
// ============================================================================

void handleEquipItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: EquipItem without player", session.getId());
        return;
    }

    GP_Client_EquipItem packet;
    packet.unpack(data);

    int invSlot = packet.m_slotInv;
    UnitDefines::EquipSlot equipSlot = packet.m_slotEquip;
    const ItemDefines::ItemId& itemId = packet.m_itemId;

    LOG_DEBUG("Session %u: EquipItem invSlot=%d, equipSlot=%d, itemId=%d",
              session.getId(), invSlot, static_cast<int>(equipSlot), itemId.m_itemId);

    // Validate inventory slot range
    if (invSlot < 0 || invSlot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: EquipItem invalid inventory slot %d", session.getId(), invSlot);
        return;
    }

    // Get item from inventory
    const Inventory::InventoryItem* invItem = player->getInventory().getItem(invSlot);
    if (!invItem || invItem->isEmpty())
    {
        LOG_WARN("Session %u: EquipItem - inventory slot %d is empty", session.getId(), invSlot);
        return;
    }

    // Anti-cheat: verify item ID matches what client claims
    if (invItem->itemId != itemId.m_itemId)
    {
        LOG_WARN("Session %u: EquipItem item mismatch - slot has %d, client says %d",
                 session.getId(), invItem->itemId, itemId.m_itemId);
        return;
    }

    // Get item template to check equip_type
    const ItemTemplate* itemTemplate = sGameData.getItem(invItem->itemId);
    if (!itemTemplate)
    {
        LOG_WARN("Session %u: EquipItem - item %d has no template", session.getId(), invItem->itemId);
        return;
    }

    // If no specific equipment slot provided, determine from item's equip_type
    if (equipSlot == UnitDefines::EquipSlot::None)
    {
        equipSlot = Equipment::PlayerEquipment::getSlotForEquipType(itemTemplate->equipType);
        if (equipSlot == UnitDefines::EquipSlot::None)
        {
            LOG_WARN("Session %u: EquipItem - item %d (equipType=%d) is not equippable",
                     session.getId(), invItem->itemId, itemTemplate->equipType);
            return;
        }
    }

    // Validate that the item can go in the requested slot
    if (!Equipment::PlayerEquipment::canEquipInSlot(itemTemplate->equipType, equipSlot))
    {
        LOG_WARN("Session %u: EquipItem - item %d (equipType=%d) cannot go in slot %d",
                 session.getId(), invItem->itemId, itemTemplate->equipType, static_cast<int>(equipSlot));
        return;
    }

    // Check if there's already something in the equipment slot
    Equipment::EquippedItem previousEquipped;
    bool hadPreviousItem = false;

    if (!player->getEquipment().isSlotEmpty(equipSlot))
    {
        previousEquipped = player->getEquipment().unequipItem(equipSlot);
        hadPreviousItem = true;
    }

    // Create equipment item from inventory item
    Equipment::EquippedItem newEquipped;
    newEquipped.itemId = invItem->itemId;
    newEquipped.durability = 100;  // TODO: Track durability in inventory
    newEquipped.fromItemId(itemId);  // Copy affixes/gems from client packet

    // Remove item from inventory
    player->getInventory().clearSlot(invSlot);

    // If we had a previous item, put it in the inventory slot we just cleared
    if (hadPreviousItem)
    {
        Inventory::InventoryItem swappedItem;
        swappedItem.itemId = previousEquipped.itemId;
        swappedItem.stackCount = 1;  // Equipment is always stack of 1
        player->getInventory().setItem(invSlot, swappedItem);
    }

    // Equip the new item
    if (!player->getEquipment().equipItem(equipSlot, newEquipped))
    {
        LOG_ERROR("Session %u: EquipItem - failed to equip item %d in slot %d",
                  session.getId(), newEquipped.itemId, static_cast<int>(equipSlot));
        // Restore inventory state on failure
        Inventory::InventoryItem restored;
        restored.itemId = invItem->itemId;
        restored.stackCount = 1;
        player->getInventory().setItem(invSlot, restored);
        return;
    }

    // Broadcast equipment change to nearby players
    player->broadcastEquipmentChange(equipSlot);

    // Recalculate stats (equipment bonuses may have changed)
    player->recalculateStats();

    // Mark for save and send updated inventory
    player->markDirty();
    player->sendInventory();

    LOG_INFO("Session %u: Player '%s' equipped item %d in slot %d",
             session.getId(), player->getName().c_str(),
             newEquipped.itemId, static_cast<int>(equipSlot));
}

void handleUnequipItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: UnequipItem without player", session.getId());
        return;
    }

    GP_Client_UnequipItem packet;
    packet.unpack(data);

    UnitDefines::EquipSlot equipSlot = static_cast<UnitDefines::EquipSlot>(packet.m_equipSlot);

    LOG_DEBUG("Session %u: UnequipItem equipSlot=%d",
              session.getId(), static_cast<int>(equipSlot));

    // Validate equipment slot range
    if (static_cast<int>(equipSlot) >= Equipment::NUM_SLOTS)
    {
        LOG_WARN("Session %u: UnequipItem invalid slot %d", session.getId(), static_cast<int>(equipSlot));
        return;
    }

    // Check if there's an item to unequip
    if (player->getEquipment().isSlotEmpty(equipSlot))
    {
        LOG_DEBUG("Session %u: UnequipItem - slot %d is empty", session.getId(), static_cast<int>(equipSlot));
        return;
    }

    // Find an empty inventory slot
    int freeSlot = player->getInventory().getFirstEmptySlot();
    if (freeSlot < 0)
    {
        LOG_DEBUG("Session %u: UnequipItem - inventory is full", session.getId());
        // TODO: Send error message to client
        return;
    }

    // Get the equipped item
    Equipment::EquippedItem unequipped = player->getEquipment().unequipItem(equipSlot);

    // Add to inventory
    Inventory::InventoryItem invItem;
    invItem.itemId = unequipped.itemId;
    invItem.stackCount = 1;
    player->getInventory().setItem(freeSlot, invItem);

    // Broadcast equipment change (now empty slot)
    player->broadcastEquipmentChange(equipSlot);

    // Recalculate stats (equipment bonuses may have changed)
    player->recalculateStats();

    // Mark for save and send updated inventory
    player->markDirty();
    player->sendInventory();

    LOG_INFO("Session %u: Player '%s' unequipped item %d from slot %d to inventory slot %d",
             session.getId(), player->getName().c_str(),
             unequipped.itemId, static_cast<int>(equipSlot), freeSlot);
}

// ============================================================================
// Loot Handlers (Phase 6, Task 6.4)
// ============================================================================

void handleLootItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: LootItem without player", session.getId());
        return;
    }

    GP_Client_LootItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: LootItem sourceGuid=%u itemId=%d",
              session.getId(), packet.m_sourceGuid, packet.m_itemId.m_itemId);

    // Check for "take all" special value
    if (packet.m_itemId.m_itemId == GP_Client_LootItem::TakeAll)
    {
        sLootManager.lootAll(player, packet.m_sourceGuid);
    }
    else
    {
        sLootManager.lootItem(player, packet.m_sourceGuid, packet.m_itemId);
    }
}

// ============================================================================
// Vendor Handlers (Phase 6, Task 6.5)
// ============================================================================

void handleBuyVendorItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: BuyVendorItem without player", session.getId());
        return;
    }

    GP_Client_BuyVendorItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: BuyVendorItem vendorGuid=%u itemIndex=%d count=%d",
              session.getId(), packet.m_vendorGuid, packet.m_itemIndex, packet.m_count);

    // Find vendor NPC
    Npc* vendor = sWorldManager.getNpc(packet.m_vendorGuid);
    if (!vendor)
    {
        LOG_WARN("Session %u: BuyVendorItem invalid vendor %u", session.getId(), packet.m_vendorGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Validate distance
    float dx = player->getX() - vendor->getX();
    float dy = player->getY() - vendor->getY();
    if (dx * dx + dy * dy > 300.0f * 300.0f)
    {
        LOG_WARN("Session %u: BuyVendorItem vendor too far", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TooFarAway);
        return;
    }

    // Process purchase
    sVendorManager.buyItem(player, vendor, packet.m_itemIndex, packet.m_count);
}

void handleSellItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: SellItem without player", session.getId());
        return;
    }

    GP_Client_SellItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: SellItem vendorGuid=%u slot=%d",
              session.getId(), packet.m_vendorGuid, packet.m_inventorySlot);

    // Find vendor NPC
    Npc* vendor = sWorldManager.getNpc(packet.m_vendorGuid);
    if (!vendor)
    {
        LOG_WARN("Session %u: SellItem invalid vendor %u", session.getId(), packet.m_vendorGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Validate distance
    float dx = player->getX() - vendor->getX();
    float dy = player->getY() - vendor->getY();
    if (dx * dx + dy * dy > 300.0f * 300.0f)
    {
        LOG_WARN("Session %u: SellItem vendor too far", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TooFarAway);
        return;
    }

    // Process sale
    sVendorManager.sellItem(player, vendor, packet.m_inventorySlot);
}

void handleBuyback(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: Buyback without player", session.getId());
        return;
    }

    GP_Client_Buyback packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: Buyback vendorGuid=%u slot=%d",
              session.getId(), packet.m_vendorGuid, packet.m_buybackSlot);

    // Find vendor NPC
    Npc* vendor = sWorldManager.getNpc(packet.m_vendorGuid);
    if (!vendor)
    {
        LOG_WARN("Session %u: Buyback invalid vendor %u", session.getId(), packet.m_vendorGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Validate distance
    float dx = player->getX() - vendor->getX();
    float dy = player->getY() - vendor->getY();
    if (dx * dx + dy * dy > 300.0f * 300.0f)
    {
        LOG_WARN("Session %u: Buyback vendor too far", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TooFarAway);
        return;
    }

    // Process buyback
    sVendorManager.buyback(player, vendor, packet.m_buybackSlot);
}

// ============================================================================
// Bank Handlers (Phase 6, Task 6.6)
// ============================================================================

void handleOpenBank(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: OpenBank without player", session.getId());
        return;
    }

    GP_Client_OpenBank packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: OpenBank bankerGuid=%u", session.getId(), packet.m_bankerGuid);

    // Find banker NPC
    Npc* banker = sWorldManager.getNpc(packet.m_bankerGuid);
    if (!banker)
    {
        LOG_WARN("Session %u: OpenBank invalid banker %u", session.getId(), packet.m_bankerGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Verify NPC has banker flag
    if (!banker->isBanker())
    {
        LOG_WARN("Session %u: OpenBank NPC %u is not a banker", session.getId(), packet.m_bankerGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Validate distance
    float dx = player->getX() - banker->getX();
    float dy = player->getY() - banker->getY();
    if (dx * dx + dy * dy > 300.0f * 300.0f)
    {
        LOG_WARN("Session %u: OpenBank banker too far", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TooFarAway);
        return;
    }

    // Send open bank signal to client
    GP_Server_OpenBank openPacket;
    openPacket.m_bankerGuid = packet.m_bankerGuid;

    StlBuffer buf;
    uint16_t opcode = openPacket.getOpcode();
    buf << opcode;
    openPacket.pack(buf);
    player->sendPacket(buf);

    // Send bank contents
    sendBank(session);
}

void handleMoveInventoryToBank(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: MoveInventoryToBank without player", session.getId());
        return;
    }

    GP_Client_MoveInventoryToBank packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: MoveInventoryToBank from=%d to=%d autoSelect=%d",
              session.getId(), packet.m_from, packet.m_to, packet.m_autoSelectForMe);

    // Get item from inventory
    const Inventory::InventoryItem* invItem = player->getInventory().getItem(packet.m_from);
    if (!invItem)
    {
        LOG_WARN("Session %u: MoveInventoryToBank empty inventory slot %d", session.getId(), packet.m_from);
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Determine bank slot
    int bankSlot = packet.m_to;
    if (packet.m_autoSelectForMe)
    {
        bankSlot = player->getBank().getFirstEmptySlot();
        if (bankSlot == Bank::INVALID_SLOT)
        {
            LOG_WARN("Session %u: MoveInventoryToBank bank is full", session.getId());
            sendWorldError(player, PlayerDefines::WorldError::InventoryFull);
            return;
        }
    }
    else if (bankSlot < 0 || bankSlot >= Bank::MAX_SLOTS)
    {
        LOG_WARN("Session %u: MoveInventoryToBank invalid bank slot %d", session.getId(), bankSlot);
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Check if bank slot is empty or has same item type for stacking
    Bank::BankItem* bankItem = player->getBank().getItemMutable(bankSlot);
    if (bankItem && bankItem->itemId != invItem->itemId)
    {
        // Swap: move bank item to inventory first
        Inventory::InventoryItem tempInv;
        tempInv.itemId = bankItem->itemId;
        tempInv.stackCount = bankItem->stackCount;
        tempInv.durability = bankItem->durability;
        tempInv.enchantId = bankItem->enchantId;
        tempInv.flags = 0;

        // Clear bank slot and set with inventory item
        Bank::BankItem newBankItem;
        newBankItem.itemId = invItem->itemId;
        newBankItem.stackCount = invItem->stackCount;
        newBankItem.durability = invItem->durability;
        newBankItem.enchantId = invItem->enchantId;
        player->getBank().setItem(bankSlot, newBankItem);

        // Set inventory slot with bank item
        player->getInventory().setItem(packet.m_from, tempInv);
    }
    else if (bankItem && bankItem->itemId == invItem->itemId)
    {
        // Stack same items
        const ItemTemplate* tmpl = sGameData.getItem(invItem->itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (maxStack <= 0) maxStack = 1;

        int32_t canAdd = maxStack - bankItem->stackCount;
        int32_t toAdd = std::min(canAdd, invItem->stackCount);

        if (toAdd > 0)
        {
            bankItem->stackCount += toAdd;
            player->getBank().markDirty();

            if (invItem->stackCount - toAdd <= 0)
            {
                player->getInventory().clearSlot(packet.m_from);
            }
            else
            {
                player->getInventory().getItemMutable(packet.m_from)->stackCount -= toAdd;
                player->getInventory().markDirty();
            }
        }
    }
    else
    {
        // Empty bank slot - simple move
        Bank::BankItem newBankItem;
        newBankItem.itemId = invItem->itemId;
        newBankItem.stackCount = invItem->stackCount;
        newBankItem.durability = invItem->durability;
        newBankItem.enchantId = invItem->enchantId;

        player->getBank().setItem(bankSlot, newBankItem);
        player->getInventory().clearSlot(packet.m_from);
    }

    // Send updated bank and inventory
    sendBank(session);
    player->sendInventory();
}

void handleMoveBankToBank(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: MoveBankToBank without player", session.getId());
        return;
    }

    GP_Client_MoveBankToBank packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: MoveBankToBank from=%d to=%d",
              session.getId(), packet.m_from, packet.m_to);

    // Validate slots
    if (packet.m_from < 0 || packet.m_from >= Bank::MAX_SLOTS ||
        packet.m_to < 0 || packet.m_to >= Bank::MAX_SLOTS)
    {
        LOG_WARN("Session %u: MoveBankToBank invalid slot", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Move within bank (handles swapping/stacking)
    if (!player->getBank().moveItem(packet.m_from, packet.m_to))
    {
        LOG_WARN("Session %u: MoveBankToBank move failed", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Send updated bank
    sendBank(session);
}

void handleUnBankItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: UnBankItem without player", session.getId());
        return;
    }

    GP_Client_UnBankItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: UnBankItem slot=%d invSlot=%d autoSelect=%d",
              session.getId(), packet.m_slot, packet.m_inventorySlot, packet.m_chooseInvSlotForMe);

    // Get item from bank
    const Bank::BankItem* bankItem = player->getBank().getItem(packet.m_slot);
    if (!bankItem)
    {
        LOG_WARN("Session %u: UnBankItem empty bank slot %d", session.getId(), packet.m_slot);
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Determine inventory slot
    int invSlot = packet.m_inventorySlot;
    if (packet.m_chooseInvSlotForMe)
    {
        invSlot = player->getInventory().getFirstEmptySlot();
        if (invSlot == Inventory::INVALID_SLOT)
        {
            LOG_WARN("Session %u: UnBankItem inventory is full", session.getId());
            sendWorldError(player, PlayerDefines::WorldError::InventoryFull);
            return;
        }
    }
    else if (invSlot < 0 || invSlot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: UnBankItem invalid inventory slot %d", session.getId(), invSlot);
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Check if inventory slot is empty or has same item type for stacking
    const Inventory::InventoryItem* invItem = player->getInventory().getItem(invSlot);
    if (invItem && invItem->itemId != bankItem->itemId)
    {
        // Swap: move inventory item to bank first
        Bank::BankItem tempBank;
        tempBank.itemId = invItem->itemId;
        tempBank.stackCount = invItem->stackCount;
        tempBank.durability = invItem->durability;
        tempBank.enchantId = invItem->enchantId;

        // Set inventory slot with bank item
        Inventory::InventoryItem newInvItem;
        newInvItem.itemId = bankItem->itemId;
        newInvItem.stackCount = bankItem->stackCount;
        newInvItem.durability = bankItem->durability;
        newInvItem.enchantId = bankItem->enchantId;
        newInvItem.flags = 0;
        player->getInventory().setItem(invSlot, newInvItem);

        // Set bank slot with inventory item
        player->getBank().setItem(packet.m_slot, tempBank);
    }
    else if (invItem && invItem->itemId == bankItem->itemId)
    {
        // Stack same items
        const ItemTemplate* tmpl = sGameData.getItem(bankItem->itemId);
        int32_t maxStack = tmpl ? tmpl->stackCount : 1;
        if (maxStack <= 0) maxStack = 1;

        int32_t canAdd = maxStack - invItem->stackCount;
        int32_t toAdd = std::min(canAdd, bankItem->stackCount);

        if (toAdd > 0)
        {
            player->getInventory().getItemMutable(invSlot)->stackCount += toAdd;
            player->getInventory().markDirty();

            if (bankItem->stackCount - toAdd <= 0)
            {
                player->getBank().clearSlot(packet.m_slot);
            }
            else
            {
                player->getBank().getItemMutable(packet.m_slot)->stackCount -= toAdd;
                player->getBank().markDirty();
            }
        }
    }
    else
    {
        // Empty inventory slot - simple move
        Inventory::InventoryItem newInvItem;
        newInvItem.itemId = bankItem->itemId;
        newInvItem.stackCount = bankItem->stackCount;
        newInvItem.durability = bankItem->durability;
        newInvItem.enchantId = bankItem->enchantId;
        newInvItem.flags = 0;

        player->getInventory().setItem(invSlot, newInvItem);
        player->getBank().clearSlot(packet.m_slot);
    }

    // Send updated bank and inventory
    sendBank(session);
    player->sendInventory();
}

void handleSortBank(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: SortBank without player", session.getId());
        return;
    }

    (void)data;  // No payload

    LOG_DEBUG("Session %u: SortBank", session.getId());

    // Sort bank items
    player->getBank().sort();

    // Send updated bank
    sendBank(session);
}

void sendBank(Session& session)
{
    Player* player = session.getPlayer();
    if (!player)
        return;

    GP_Server_Bank bankPacket;

    const auto& slots = player->getBank().getSlots();
    for (int slot = 0; slot < Bank::MAX_SLOTS; ++slot)
    {
        if (!slots[slot].isEmpty())
        {
            GP_Server_Bank::Slot s;
            s.slot = slot;
            s.itemId.m_itemId = slots[slot].itemId;
            s.itemId.m_durability = slots[slot].durability;
            s.itemId.m_enchant = slots[slot].enchantId;
            s.itemId.m_count = slots[slot].stackCount;
            s.stackCount = slots[slot].stackCount;
            bankPacket.m_slots.push_back(s);
        }
    }

    StlBuffer buf;
    uint16_t opcode = bankPacket.getOpcode();
    buf << opcode;
    bankPacket.pack(buf);
    player->sendPacket(buf);

    LOG_DEBUG("Session %u: Sent bank with %zu items", session.getId(), bankPacket.m_slots.size());
}

// ============================================================================
// Trade Handlers (Phase 6, Task 6.7)
// ============================================================================

void handleOpenTradeWith(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: OpenTradeWith without player", session.getId());
        return;
    }

    GP_Client_OpenTradeWith packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: OpenTradeWith targetGuid=%u", session.getId(), packet.m_targetGuid);

    // Check if player is already in a trade
    if (sTradeManager.isInTrade(player))
    {
        LOG_WARN("Session %u: Player already in trade", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::AlreadyInTrade);
        return;
    }

    // Find target player
    Player* target = sWorldManager.getPlayer(packet.m_targetGuid);
    if (!target)
    {
        LOG_WARN("Session %u: OpenTradeWith invalid target %u", session.getId(), packet.m_targetGuid);
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Check if target is already in a trade
    if (sTradeManager.isInTrade(target))
    {
        LOG_WARN("Session %u: Target %s already in trade", session.getId(), target->getName().c_str());
        sendWorldError(player, PlayerDefines::WorldError::TargetInTrade);
        return;
    }

    // Validate distance
    float dx = player->getX() - target->getX();
    float dy = player->getY() - target->getY();
    if (dx * dx + dy * dy > PlayerDefines::Trade::TradeDistance * PlayerDefines::Trade::TradeDistance)
    {
        LOG_WARN("Session %u: OpenTradeWith target too far", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TooFarAway);
        return;
    }

    // Start trade session
    Trade::TradeSession* tradeSession = sTradeManager.startTrade(player, target);
    if (!tradeSession)
    {
        LOG_WARN("Session %u: Failed to start trade", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget);
        return;
    }

    // Send initial update to both players (opens trade window)
    sTradeManager.sendTradeUpdate(tradeSession);
}

void handleTradeAddItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: TradeAddItem without player", session.getId());
        return;
    }

    GP_Client_TradeAddItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: TradeAddItem invSlot=%d", session.getId(), packet.m_invSlot);

    Trade::TradeSession* tradeSession = sTradeManager.getTradeSession(player);
    if (!tradeSession)
    {
        LOG_WARN("Session %u: TradeAddItem not in trade", session.getId());
        return;
    }

    // Check trade is not full
    const auto& offer = tradeSession->getOffer(player);
    if (offer.countItems() >= Trade::MAX_SLOTS)
    {
        LOG_WARN("Session %u: TradeAddItem trade full", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::TradeFull);
        return;
    }

    if (!tradeSession->addItem(player, packet.m_invSlot))
    {
        LOG_WARN("Session %u: TradeAddItem failed", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Send update to both players
    sTradeManager.sendTradeUpdate(tradeSession);
}

void handleTradeRemoveItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: TradeRemoveItem without player", session.getId());
        return;
    }

    GP_Client_TradeRemoveItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: TradeRemoveItem itemGuid=%d", session.getId(), packet.m_itemGuid);

    Trade::TradeSession* tradeSession = sTradeManager.getTradeSession(player);
    if (!tradeSession)
    {
        LOG_WARN("Session %u: TradeRemoveItem not in trade", session.getId());
        return;
    }

    if (!tradeSession->removeItem(player, packet.m_itemGuid))
    {
        LOG_WARN("Session %u: TradeRemoveItem failed", session.getId());
        return;
    }

    // Send update to both players
    sTradeManager.sendTradeUpdate(tradeSession);
}

void handleTradeSetGold(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: TradeSetGold without player", session.getId());
        return;
    }

    GP_Client_TradeSetGold packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: TradeSetGold amount=%d", session.getId(), packet.m_amount);

    Trade::TradeSession* tradeSession = sTradeManager.getTradeSession(player);
    if (!tradeSession)
    {
        LOG_WARN("Session %u: TradeSetGold not in trade", session.getId());
        return;
    }

    // Validate gold amount
    if (packet.m_amount < 0)
    {
        LOG_WARN("Session %u: TradeSetGold negative amount", session.getId());
        return;
    }

    if (packet.m_amount > player->getGold())
    {
        LOG_WARN("Session %u: TradeSetGold not enough gold", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::NotEnoughGold);
        // Set to max available
        tradeSession->setGold(player, player->getGold());
    }
    else
    {
        tradeSession->setGold(player, packet.m_amount);
    }

    // Send update to both players
    sTradeManager.sendTradeUpdate(tradeSession);
}

void handleTradeConfirm(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: TradeConfirm without player", session.getId());
        return;
    }

    (void)data;  // No payload

    LOG_DEBUG("Session %u: TradeConfirm", session.getId());

    Trade::TradeSession* tradeSession = sTradeManager.getTradeSession(player);
    if (!tradeSession)
    {
        LOG_WARN("Session %u: TradeConfirm not in trade", session.getId());
        return;
    }

    // Toggle confirmation
    bool currentlyConfirmed = tradeSession->isConfirmed(player);
    tradeSession->setConfirmed(player, !currentlyConfirmed);

    // Check if both confirmed - complete the trade
    if (tradeSession->areBothConfirmed())
    {
        if (!sTradeManager.completeTrade(tradeSession))
        {
            // Trade failed - reset confirmations
            tradeSession->setConfirmed(player, false);
            tradeSession->setConfirmed(tradeSession->getOtherPlayer(player), false);
            sTradeManager.sendTradeUpdate(tradeSession);
        }
        // If successful, completeTrade already cleaned up and sent notifications
    }
    else
    {
        // Send update to both players showing confirmation state
        sTradeManager.sendTradeUpdate(tradeSession);
    }
}

void handleTradeCancel(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: TradeCancel without player", session.getId());
        return;
    }

    (void)data;  // No payload

    LOG_DEBUG("Session %u: TradeCancel", session.getId());

    sTradeManager.cancelTrade(player);
}

// ============================================================================
// Item Use Handlers (Phase 6, Task 6.8)
// ============================================================================

void handleUseItem(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: UseItem without player", session.getId());
        return;
    }

    // Parse packet
    GP_Client_UseItem packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: UseItem slot=%d targetGuid=%u",
              session.getId(), packet.m_slot, packet.m_targetGuid);

    // Validate slot
    if (packet.m_slot < 0 || packet.m_slot >= Inventory::MAX_SLOTS)
    {
        LOG_WARN("Session %u: UseItem invalid slot %d", session.getId(), packet.m_slot);
        return;
    }

    // Get item from inventory
    const Inventory::InventoryItem* item = player->getInventory().getItem(packet.m_slot);
    if (!item || item->isEmpty())
    {
        LOG_WARN("Session %u: UseItem slot %d is empty", session.getId(), packet.m_slot);
        sendWorldError(player, PlayerDefines::WorldError::InvalidItem);
        return;
    }

    // Look up item template
    const ItemTemplate* itemTemplate = sGameData.getItem(item->itemId);
    if (!itemTemplate)
    {
        LOG_ERROR("Session %u: UseItem unknown item template %d", session.getId(), item->itemId);
        return;
    }

    // Check if item has a use spell (spell_1 / spell[0])
    int32_t spellId = itemTemplate->spell[0];
    if (spellId <= 0)
    {
        LOG_DEBUG("Session %u: Item %d has no use spell", session.getId(), item->itemId);
        sendWorldError(player, PlayerDefines::WorldError::ItemNotUsable);
        return;
    }

    // Get spell template
    const SpellTemplate* spell = sGameData.getSpell(spellId);
    if (!spell)
    {
        LOG_ERROR("Session %u: Item %d has invalid spell %d", session.getId(), item->itemId, spellId);
        return;
    }

    // Check item/spell cooldown (uses category cooldown for shared item cooldowns)
    if (spell->cooldownCategory > 0 && player->getCooldowns().isCategoryOnCooldown(spell->cooldownCategory))
    {
        LOG_DEBUG("Session %u: Item spell on category cooldown", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::ItemOnCooldown);
        return;
    }
    if (player->getCooldowns().isOnCooldown(spellId))
    {
        LOG_DEBUG("Session %u: Item spell on cooldown", session.getId());
        sendWorldError(player, PlayerDefines::WorldError::ItemOnCooldown);
        return;
    }

    // Find target entity (self if no target specified)
    Entity* target = nullptr;
    if (packet.m_targetGuid != 0)
    {
        target = findTargetEntity(packet.m_targetGuid);
    }
    if (!target)
    {
        target = player;  // Default to self for consumables
    }

    // Validate cast (same as regular spell cast)
    CastResult result = SpellCaster::validateCast(player, spell, target);
    if (result != CastResult::Success)
    {
        sendCastError(session, spellId, static_cast<int32_t>(result));
        LOG_DEBUG("Session %u: Item use cast validation failed - %s",
                  session.getId(), getCastResultString(result));
        return;
    }

    // Get targets for the spell
    std::vector<Entity*> targets = SpellCaster::getTargets(player, spell, target);
    if (targets.empty())
    {
        sendCastError(session, spellId, static_cast<int32_t>(CastResult::NoTarget));
        return;
    }

    // Process spell effects on each target (same logic as executePendingCast)
    std::vector<std::pair<Entity*, uint8_t>> hitTargets;

    for (Entity* effectTarget : targets)
    {
        SpellDefines::Effects effectType = static_cast<SpellDefines::Effects>(spell->effect[0]);

        if (effectType == SpellDefines::Effects::Damage ||
            effectType == SpellDefines::Effects::MeleeAtk ||
            effectType == SpellDefines::Effects::RangedAtk)
        {
            // Damage effect
            DamageInfo damage = CombatFormulas::calculateDamage(player, effectTarget, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                CombatMessenger::toPacketHitResult(damage.hitResult))});

            if (damage.hitResult != HitResult::Miss &&
                damage.hitResult != HitResult::Dodge &&
                damage.hitResult != HitResult::Parry)
            {
                effectTarget->takeDamage(damage.finalDamage, player);
                CombatMessenger::sendDamageMessage(player, effectTarget, spellId, damage, effectType);
            }
            else
            {
                CombatMessenger::sendMissMessage(player, effectTarget, spellId, damage.hitResult, effectType);
            }
        }
        else if (effectType == SpellDefines::Effects::Heal ||
                 effectType == SpellDefines::Effects::RestoreMana ||
                 effectType == SpellDefines::Effects::RestoreManaPct)
        {
            // Healing effect
            HealInfo heal = CombatFormulas::calculateHeal(player, effectTarget, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(
                CombatMessenger::toPacketHitResult(heal.hitResult))});

            effectTarget->heal(heal.finalHeal, player);
            CombatMessenger::sendHealMessage(player, effectTarget, spellId, heal);
        }
        else if (effectType == SpellDefines::Effects::ApplyAura ||
                 effectType == SpellDefines::Effects::ApplyAreaAura)
        {
            // Buff/debuff effect
            bool applied = effectTarget->getAuras().applyAura(player, spell, 0);

            hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});

            if (applied)
            {
                LOG_DEBUG("Applied aura from item spell %d to target %llu", spellId, effectTarget->getGuid());
            }
        }
        else
        {
            // Other effect types
            hitTargets.push_back({effectTarget, static_cast<uint8_t>(SpellDefines::HitResult::Normal)});
        }
    }

    // Send spell go to all players who can see
    sendSpellGo(player, spellId, hitTargets);

    // Start cooldown for the spell
    if (spell->cooldown > 0)
    {
        player->getCooldowns().startCooldown(spellId, spell->cooldown, spell->cooldownCategory);

        // Send cooldown update to client
        GP_Server_Cooldown cooldownPacket;
        cooldownPacket.m_id = spellId;
        cooldownPacket.m_totalDuration = spell->cooldown;

        StlBuffer buf;
        uint16_t opcode = cooldownPacket.getOpcode();
        buf << opcode;
        cooldownPacket.pack(buf);
        player->sendPacket(buf);
    }

    // Consume the item (reduce stack or remove)
    // Check if item is consumable (stackCount > 0 means consumable in many systems)
    // For potions and consumables, we always consume one charge
    player->getInventory().removeItem(packet.m_slot, 1);
    player->getInventory().markDirty();

    // Send inventory update to client
    player->sendInventory();

    LOG_DEBUG("Session %u: Used item %d (spell %d) from slot %d",
              session.getId(), item->itemId, spellId, packet.m_slot);
}

// ============================================================================
// Repair Handlers (Phase 6, Task 6.9)
// ============================================================================

void handleRepair(Session& session, StlBuffer& data)
{
    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: Repair without player", session.getId());
        return;
    }

    // Parse packet
    GP_Client_Repair packet;
    packet.unpack(data);

    LOG_DEBUG("Session %u: Repair confirmed=%d", session.getId(), packet.m_confirmed);

    if (!packet.m_confirmed)
    {
        // First request - calculate cost and send to client
        int32_t repairCost = player->getEquipment().calculateRepairCost();

        if (repairCost <= 0)
        {
            // Nothing to repair
            LOG_DEBUG("Session %u: Nothing to repair", session.getId());
            return;
        }

        // Send repair cost to client
        GP_Server_RepairCost costPacket;
        costPacket.m_finished = false;
        costPacket.m_amount = repairCost;

        StlBuffer buf;
        uint16_t opcode = costPacket.getOpcode();
        buf << opcode;
        costPacket.pack(buf);
        player->sendPacket(buf);

        LOG_DEBUG("Session %u: Sent repair cost %d", session.getId(), repairCost);
    }
    else
    {
        // Confirmed - do the repair
        int32_t repairCost = player->getEquipment().calculateRepairCost();

        if (repairCost <= 0)
        {
            LOG_DEBUG("Session %u: Nothing to repair on confirm", session.getId());
            return;
        }

        // Check if player has enough gold
        int32_t playerGold = player->getGold();
        if (playerGold < repairCost)
        {
            LOG_DEBUG("Session %u: Not enough gold for repair (have %d, need %d)",
                      session.getId(), playerGold, repairCost);
            sendWorldError(player, PlayerDefines::WorldError::NotEnoughGold);
            return;
        }

        // Deduct gold
        player->setGold(playerGold - repairCost);

        // Repair all items
        player->getEquipment().repairAll();

        // Send gold spent notification
        GP_Server_SpentGold goldPacket;
        goldPacket.m_amount = repairCost;

        StlBuffer buf;
        uint16_t opcode = goldPacket.getOpcode();
        buf << opcode;
        goldPacket.pack(buf);
        player->sendPacket(buf);

        // Notify client that repair completed
        GP_Server_RepairCost donePacket;
        donePacket.m_finished = true;
        donePacket.m_amount = repairCost;

        StlBuffer doneBuf;
        uint16_t doneOpcode = donePacket.getOpcode();
        doneBuf << doneOpcode;
        donePacket.pack(doneBuf);
        player->sendPacket(doneBuf);

        // Send updated equipment to show repaired durability
        player->sendEquipment();

        LOG_DEBUG("Session %u: Repaired equipment for %d gold", session.getId(), repairCost);
    }
}

// ============================================================================
// Phase 9 - Additional Handlers (Missing Opcodes)
// ============================================================================

void handleReqAbilityList(Session& session, StlBuffer& data)
{
    (void)data;  // Empty packet

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleReqAbilityList - no player", session.getId());
        return;
    }

    // Reuse the sendSpellbook function which sends GP_Server_Spellbook
    sendSpellbook(session);
    LOG_DEBUG("Session %u: Sent spellbook on ability list request", session.getId());
}

void handleTogglePvP(Session& session, StlBuffer& data)
{
    GP_Client_TogglePvP packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleTogglePvP - no player", session.getId());
        return;
    }

    // Set the PvP flag as an object variable
    int32_t newPvPState = packet.m_enabled ? 1 : 0;
    player->setVariable(ObjDefines::Variable::PvPFlag, newPvPState);

    // Broadcast the variable change to nearby players
    player->broadcastVariable(ObjDefines::Variable::PvPFlag, newPvPState);

    LOG_DEBUG("Session %u: PvP flag set to %s", session.getId(), packet.m_enabled ? "enabled" : "disabled");
}

void handleRespec(Session& session, StlBuffer& data)
{
    GP_Client_Respec packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleRespec - no player", session.getId());
        return;
    }

    // Calculate respec cost based on level (10 gold per level)
    int32_t respecCost = player->getLevel() * 10;

    if (!packet.m_confirmed)
    {
        // Not confirmed - send cost prompt
        GP_Server_PromptRespec response;
        response.m_cost = respecCost;

        StlBuffer buf;
        uint16_t opcode = response.getOpcode();
        buf << opcode;
        response.pack(buf);
        player->sendPacket(buf);

        LOG_DEBUG("Session %u: Sent respec cost prompt (%d gold)", session.getId(), respecCost);
    }
    else
    {
        // Confirmed - do the respec
        if (player->getGold() < respecCost)
        {
            sendWorldError(player, PlayerDefines::WorldError::NotEnoughGold);
            return;
        }

        // Deduct gold
        player->spendGold(respecCost);

        // Reset all primary stat bonuses (Strength through Courage)
        const UnitDefines::Stat primaryStats[] = {
            UnitDefines::Stat::Strength,
            UnitDefines::Stat::Agility,
            UnitDefines::Stat::Willpower,
            UnitDefines::Stat::Intelligence,
            UnitDefines::Stat::Courage
        };
        for (auto stat : primaryStats)
        {
            player->setStatBonus(stat, 0);
        }

        // Reset spell ranks to 1 for all spells that can level up
        auto stmt = sDatabase.prepare(
            "UPDATE character_spells SET rank = 1 WHERE character_guid = ? AND spell_id IN "
            "(SELECT id FROM spell_template WHERE can_level_up != 0)"
        );
        if (stmt.valid())
        {
            stmt.bind(1, player->getCharacterGuid());
            stmt.step();
        }

        // Recalculate stats
        player->recalculateStats();

        // Send updated spellbook
        sendSpellbook(session);

        // Send updated player info
        sendPlayerSelf(session);

        // Notify gold spent
        GP_Server_SpentGold goldPacket;
        goldPacket.m_amount = respecCost;
        StlBuffer buf;
        uint16_t op = goldPacket.getOpcode();
        buf << op;
        goldPacket.pack(buf);
        player->sendPacket(buf);

        LOG_INFO("Session %u: Player %s respecced for %d gold",
                 session.getId(), player->getName().c_str(), respecCost);
    }
}

void handleQueryWaypoints(Session& session, StlBuffer& data)
{
    (void)data;  // Empty packet

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleQueryWaypoints - no player", session.getId());
        return;
    }

    GP_Server_QueryWaypointsResponse response;

    // Load all waypoints and check which ones the player has discovered
    // First get all waypoints from the waypoint table
    auto waypointStmt = sDatabase.prepare(
        "SELECT id, name, map_id, x, y FROM waypoint"
    );

    if (waypointStmt.valid())
    {
        // Get player's discovered waypoints
        std::set<int32_t> discovered;
        auto discStmt = sDatabase.prepare(
            "SELECT waypoint_id FROM character_waypoints WHERE character_guid = ?"
        );
        if (discStmt.valid())
        {
            discStmt.bind(1, player->getCharacterGuid());
            while (discStmt.step())
            {
                discovered.insert(discStmt.getInt(0));
            }
        }

        // Build response with all waypoints
        while (waypointStmt.step())
        {
            const int32_t waypointId = waypointStmt.getInt(0);
            if (discovered.find(waypointId) != discovered.end())
                response.m_guids.push_back(waypointId);
        }
    }

    StlBuffer buf;
    uint16_t opcode = response.getOpcode();
    buf << opcode;
    response.pack(buf);
    player->sendPacket(buf);

    LOG_DEBUG("Session %u: Sent %zu waypoints", session.getId(), response.m_guids.size());
}

void handleActivateWaypoint(Session& session, StlBuffer& data)
{
    GP_Client_ActivateWaypoint packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleActivateWaypoint - no player", session.getId());
        return;
    }

    // Check if player is in combat
    if (player->isInCombat())
    {
        sendWorldError(player, PlayerDefines::WorldError::CantDoInCombat);
        return;
    }

    // Check if waypoint is discovered
    auto discStmt = sDatabase.prepare(
        "SELECT 1 FROM character_waypoints WHERE character_guid = ? AND waypoint_id = ?"
    );
    if (discStmt.valid())
    {
        discStmt.bind(1, player->getCharacterGuid());
        discStmt.bind(2, packet.m_waypointId);
        if (!discStmt.step())
        {
            sendWorldError(player, PlayerDefines::WorldError::WaypointNotDiscovered, "Waypoint not discovered");
            return;
        }
    }

    // Get waypoint info from database
    auto wpStmt = sDatabase.prepare(
        "SELECT map_id, x, y FROM waypoint WHERE id = ?"
    );
    if (!wpStmt.valid())
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget, "Invalid waypoint");
        return;
    }

    wpStmt.bind(1, packet.m_waypointId);
    if (!wpStmt.step())
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidTarget, "Waypoint not found");
        return;
    }

    int32_t mapId = wpStmt.getInt(0);
    float x = static_cast<float>(wpStmt.getDouble(1));
    float y = static_cast<float>(wpStmt.getDouble(2));

    // Teleport the player
    player->teleportTo(mapId, x, y);

    LOG_INFO("Session %u: Player %s teleported to waypoint %d",
             session.getId(), player->getName().c_str(), packet.m_waypointId);
}

void handleResetDungeons(Session& session, StlBuffer& data)
{
    (void)data;  // Empty packet

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleResetDungeons - no player", session.getId());
        return;
    }

    // Reset all dungeon lockouts for this player
    auto stmt = sDatabase.prepare(
        "DELETE FROM character_dungeon_lockouts WHERE character_guid = ?"
    );
    if (stmt.valid())
    {
        stmt.bind(1, player->getCharacterGuid());
        stmt.step();
    }

    LOG_INFO("Session %u: Player %s reset dungeon lockouts",
             session.getId(), player->getName().c_str());
}

void handleSocketItem(Session& session, StlBuffer& data)
{
    GP_Client_SocketItem packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleSocketItem - no player", session.getId());
        return;
    }

    auto& inventory = player->getInventory();

    // Validate target slot (item to socket into)
    if (packet.m_targetSlot < 0 || packet.m_targetSlot >= PlayerDefines::Inventory::NumSlots)
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidSlot);
        return;
    }

    // Validate gem slot (gem to use)
    if (packet.m_gemSlot < 0 || packet.m_gemSlot >= PlayerDefines::Inventory::NumSlots)
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidSlot);
        return;
    }

    auto* targetItem = inventory.getItemMutable(packet.m_targetSlot);
    const auto* gemItem = inventory.getItem(packet.m_gemSlot);

    if (!targetItem || targetItem->isEmpty())
    {
        sendWorldError(player, PlayerDefines::WorldError::ItemNotFound);
        return;
    }

    if (!gemItem || gemItem->isEmpty())
    {
        sendWorldError(player, PlayerDefines::WorldError::ItemNotFound);
        return;
    }

    // Validate socket index (0, 1, or 2)
    if (packet.m_socketIndex < 0 || packet.m_socketIndex > 2)
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidSlot);
        return;
    }

    // Socket the gem into the item
    int32_t gemId = gemItem->itemId;
    switch (packet.m_socketIndex)
    {
        case 0: targetItem->gem1 = gemId; break;
        case 1: targetItem->gem2 = gemId; break;
        case 2: targetItem->gem3 = gemId; break;
    }

    // Consume the gem
    inventory.removeItem(packet.m_gemSlot, 1);

    // Send result
    GP_Server_SocketResult result;
    result.m_success = true;

    StlBuffer buf;
    uint16_t opcode = result.getOpcode();
    buf << opcode;
    result.pack(buf);
    player->sendPacket(buf);

    // Send updated inventory
    player->sendInventory();

    LOG_DEBUG("Session %u: Socketed gem %d into item at slot %d, socket %d",
              session.getId(), gemId, packet.m_targetSlot, packet.m_socketIndex);
}

void handleEmpowerItem(Session& session, StlBuffer& data)
{
    GP_Client_EmpowerItem packet;
    packet.unpack(data);

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleEmpowerItem - no player", session.getId());
        return;
    }

    auto& inventory = player->getInventory();

    // Validate slots
    if (packet.m_targetSlot < 0 || packet.m_targetSlot >= PlayerDefines::Inventory::NumSlots ||
        packet.m_materialSlot < 0 || packet.m_materialSlot >= PlayerDefines::Inventory::NumSlots)
    {
        sendWorldError(player, PlayerDefines::WorldError::InvalidSlot);
        return;
    }

    auto* targetItem = inventory.getItemMutable(packet.m_targetSlot);
    const auto* materialItem = inventory.getItem(packet.m_materialSlot);

    if (!targetItem || targetItem->isEmpty() || !materialItem || materialItem->isEmpty())
    {
        sendWorldError(player, PlayerDefines::WorldError::ItemNotFound);
        return;
    }

    // TODO: Implement actual empowerment logic based on game design
    // For now, we'll just consume the material and mark success
    // This would typically increase item stats, level, or add affixes

    // Consume the material
    inventory.removeItem(packet.m_materialSlot, 1);

    // Send result
    GP_Server_EmpowerResult result;
    result.m_success = true;

    StlBuffer buf;
    uint16_t opcode = result.getOpcode();
    buf << opcode;
    result.pack(buf);
    player->sendPacket(buf);

    // Send updated inventory
    player->sendInventory();

    LOG_DEBUG("Session %u: Empowered item at slot %d with material from slot %d",
              session.getId(), packet.m_targetSlot, packet.m_materialSlot);
}

void handleSortInventory(Session& session, StlBuffer& data)
{
    (void)data;  // Empty packet

    Player* player = session.getPlayer();
    if (!player)
    {
        LOG_WARN("Session %u: handleSortInventory - no player", session.getId());
        return;
    }

    auto& inventory = player->getInventory();

    // Sort inventory: consolidate stacks, then sort by item ID
    inventory.sort();

    // Send updated inventory to client
    player->sendInventory();

    LOG_DEBUG("Session %u: Sorted inventory", session.getId());
}

// ============================================================================
// Handler Registration
// ============================================================================

void registerWorldHandlers()
{
    // Enter World - must be authenticated (at character select)
    sPacketRouter.registerHandler(
        Opcode::Client_EnterWorld,
        handleEnterWorld,
        SessionState::Authenticated,
        false,  // Exact state required - not already in world
        "Client_EnterWorld"
    );

    // Movement - must be in world
    sPacketRouter.registerHandler(
        Opcode::Client_RequestMove,
        handleRequestMove,
        SessionState::InWorld,
        true,   // Allow higher states (though InWorld is the highest play state)
        "Client_RequestMove"
    );

    sPacketRouter.registerHandler(
        Opcode::Client_RequestStop,
        handleRequestStop,
        SessionState::InWorld,
        true,
        "Client_RequestStop"
    );

    // Respawn - must be in world (Task 5.12)
    sPacketRouter.registerHandler(
        Opcode::Client_RequestRespawn,
        handleRequestRespawn,
        SessionState::InWorld,
        true,
        "Client_RequestRespawn"
    );

    // Combat - spell casting (Task 5.15)
    sPacketRouter.registerHandler(
        Opcode::Client_CastSpell,
        handleCastSpell,
        SessionState::InWorld,
        true,
        "Client_CastSpell"
    );

    // Combat - cancel cast
    sPacketRouter.registerHandler(
        Opcode::Client_CancelCast,
        handleCancelCast,
        SessionState::InWorld,
        true,
        "Client_CancelCast"
    );

    // Combat - target selection (Task 5.15)
    sPacketRouter.registerHandler(
        Opcode::Client_SetSelected,
        handleSetSelected,
        SessionState::InWorld,
        true,
        "Client_SetSelected"
    );

    // Gossip - clicked option (Phase 7, Task 7.5)
    sPacketRouter.registerHandler(
        Opcode::Client_ClickedGossipOption,
        handleClickedGossipOption,
        SessionState::InWorld,
        true,
        "Client_ClickedGossipOption"
    );

    // Quest - accept quest (Phase 7, Task 7.7)
    sPacketRouter.registerHandler(
        Opcode::Client_AcceptQuest,
        handleAcceptQuest,
        SessionState::InWorld,
        true,
        "Client_AcceptQuest"
    );

    // Quest - complete quest (Phase 7, Task 7.7)
    sPacketRouter.registerHandler(
        Opcode::Client_CompleteQuest,
        handleCompleteQuest,
        SessionState::InWorld,
        true,
        "Client_CompleteQuest"
    );

    // Quest - abandon quest (Phase 7, Task 7.7)
    sPacketRouter.registerHandler(
        Opcode::Client_AbandonQuest,
        handleAbandonQuest,
        SessionState::InWorld,
        true,
        "Client_AbandonQuest"
    );

    // Level up spend points (Phase 7, Task 7.11)
    sPacketRouter.registerHandler(
        Opcode::Client_LevelUp,
        handleLevelUp,
        SessionState::InWorld,
        true,
        "Client_LevelUp"
    );

    // Inventory - move item (Phase 6, Task 6.2)
    sPacketRouter.registerHandler(
        Opcode::Client_MoveItem,
        handleMoveItem,
        SessionState::InWorld,
        true,
        "Client_MoveItem"
    );

    // Inventory - split item stack (Phase 6, Task 6.2)
    sPacketRouter.registerHandler(
        Opcode::Client_SplitItemStack,
        handleSplitItemStack,
        SessionState::InWorld,
        true,
        "Client_SplitItemStack"
    );

    // Inventory - destroy item (Phase 6, Task 6.2)
    sPacketRouter.registerHandler(
        Opcode::Client_DestroyItem,
        handleDestroyItem,
        SessionState::InWorld,
        true,
        "Client_DestroyItem"
    );

    // Equipment - equip item (Phase 6, Task 6.3)
    sPacketRouter.registerHandler(
        Opcode::Client_EquipItem,
        handleEquipItem,
        SessionState::InWorld,
        true,
        "Client_EquipItem"
    );

    // Equipment - unequip item (Phase 6, Task 6.3)
    sPacketRouter.registerHandler(
        Opcode::Client_UnequipItem,
        handleUnequipItem,
        SessionState::InWorld,
        true,
        "Client_UnequipItem"
    );

    // Loot - loot item from target (Phase 6, Task 6.4)
    sPacketRouter.registerHandler(
        Opcode::Client_LootItem,
        handleLootItem,
        SessionState::InWorld,
        true,
        "Client_LootItem"
    );

    // Vendor - buy item (Phase 6, Task 6.5)
    sPacketRouter.registerHandler(
        Opcode::Client_BuyVendorItem,
        handleBuyVendorItem,
        SessionState::InWorld,
        true,
        "Client_BuyVendorItem"
    );

    // Vendor - sell item (Phase 6, Task 6.5)
    sPacketRouter.registerHandler(
        Opcode::Client_SellItem,
        handleSellItem,
        SessionState::InWorld,
        true,
        "Client_SellItem"
    );

    // Vendor - buyback (Phase 6, Task 6.5)
    sPacketRouter.registerHandler(
        Opcode::Client_Buyback,
        handleBuyback,
        SessionState::InWorld,
        true,
        "Client_Buyback"
    );

    // Bank - open bank (Phase 6, Task 6.6)
    sPacketRouter.registerHandler(
        Opcode::Client_OpenBank,
        handleOpenBank,
        SessionState::InWorld,
        true,
        "Client_OpenBank"
    );

    // Bank - move inventory to bank (Phase 6, Task 6.6)
    sPacketRouter.registerHandler(
        Opcode::Client_MoveInventoryToBank,
        handleMoveInventoryToBank,
        SessionState::InWorld,
        true,
        "Client_MoveInventoryToBank"
    );

    // Bank - move within bank (Phase 6, Task 6.6)
    sPacketRouter.registerHandler(
        Opcode::Client_MoveBankToBank,
        handleMoveBankToBank,
        SessionState::InWorld,
        true,
        "Client_MoveBankToBank"
    );

    // Bank - withdraw from bank (Phase 6, Task 6.6)
    sPacketRouter.registerHandler(
        Opcode::Client_UnBankItem,
        handleUnBankItem,
        SessionState::InWorld,
        true,
        "Client_UnBankItem"
    );

    // Bank - sort bank (Phase 6, Task 6.6)
    sPacketRouter.registerHandler(
        Opcode::Client_SortBank,
        handleSortBank,
        SessionState::InWorld,
        true,
        "Client_SortBank"
    );

    // Trade - open trade (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_OpenTradeWith,
        handleOpenTradeWith,
        SessionState::InWorld,
        true,
        "Client_OpenTradeWith"
    );

    // Trade - add item (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_TradeAddItem,
        handleTradeAddItem,
        SessionState::InWorld,
        true,
        "Client_TradeAddItem"
    );

    // Trade - remove item (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_TradeRemoveItem,
        handleTradeRemoveItem,
        SessionState::InWorld,
        true,
        "Client_TradeRemoveItem"
    );

    // Trade - set gold (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_TradeSetGold,
        handleTradeSetGold,
        SessionState::InWorld,
        true,
        "Client_TradeSetGold"
    );

    // Trade - confirm (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_TradeConfirm,
        handleTradeConfirm,
        SessionState::InWorld,
        true,
        "Client_TradeConfirm"
    );

    // Trade - cancel (Phase 6, Task 6.7)
    sPacketRouter.registerHandler(
        Opcode::Client_TradeCancel,
        handleTradeCancel,
        SessionState::InWorld,
        true,
        "Client_TradeCancel"
    );

    // Item use (Phase 6, Task 6.8)
    sPacketRouter.registerHandler(
        Opcode::Client_UseItem,
        handleUseItem,
        SessionState::InWorld,
        true,
        "Client_UseItem"
    );

    // Repair (Phase 6, Task 6.9)
    sPacketRouter.registerHandler(
        Opcode::Client_Repair,
        handleRepair,
        SessionState::InWorld,
        true,
        "Client_Repair"
    );

    // =========================================================================
    // Chat Handlers (Phase 8, Task 8.1)
    // =========================================================================

    // Chat message
    sPacketRouter.registerHandler(
        Opcode::Client_ChatMsg,
        ChatSystem::handleChatMsg,
        SessionState::InWorld,
        true,
        "Client_ChatMsg"
    );

    // Ignore player
    sPacketRouter.registerHandler(
        Opcode::Client_SetIgnorePlayer,
        ChatSystem::handleSetIgnorePlayer,
        SessionState::InWorld,
        true,
        "Client_SetIgnorePlayer"
    );

    // =========================================================================
    // Party Handlers (Phase 8, Task 8.4)
    // =========================================================================

    // Party invite
    sPacketRouter.registerHandler(
        Opcode::Client_PartyInviteMember,
        Party::handlePartyInvite,
        SessionState::InWorld,
        true,
        "Client_PartyInviteMember"
    );

    // Party invite response
    sPacketRouter.registerHandler(
        Opcode::Client_PartyInviteResponse,
        Party::handlePartyInviteResponse,
        SessionState::InWorld,
        true,
        "Client_PartyInviteResponse"
    );

    // Party changes (leave, kick, promote)
    sPacketRouter.registerHandler(
        Opcode::Client_PartyChanges,
        Party::handlePartyChanges,
        SessionState::InWorld,
        true,
        "Client_PartyChanges"
    );

    // =========================================================================
    // Guild Handlers (Phase 8, Task 8.6)
    // =========================================================================

    // Guild create
    sPacketRouter.registerHandler(
        Opcode::Client_GuildCreate,
        GuildHandlers::handleGuildCreate,
        SessionState::InWorld,
        true,
        "Client_GuildCreate"
    );

    // Guild invite member
    sPacketRouter.registerHandler(
        Opcode::Client_GuildInviteMember,
        GuildHandlers::handleGuildInviteMember,
        SessionState::InWorld,
        true,
        "Client_GuildInviteMember"
    );

    // Guild invite response
    sPacketRouter.registerHandler(
        Opcode::Client_GuildInviteResponse,
        GuildHandlers::handleGuildInviteResponse,
        SessionState::InWorld,
        true,
        "Client_GuildInviteResponse"
    );

    // Guild quit
    sPacketRouter.registerHandler(
        Opcode::Client_GuildQuit,
        GuildHandlers::handleGuildQuit,
        SessionState::InWorld,
        true,
        "Client_GuildQuit"
    );

    // Guild kick member
    sPacketRouter.registerHandler(
        Opcode::Client_GuildKickMember,
        GuildHandlers::handleGuildKickMember,
        SessionState::InWorld,
        true,
        "Client_GuildKickMember"
    );

    // Guild promote member
    sPacketRouter.registerHandler(
        Opcode::Client_GuildPromoteMember,
        GuildHandlers::handleGuildPromoteMember,
        SessionState::InWorld,
        true,
        "Client_GuildPromoteMember"
    );

    // Guild demote member
    sPacketRouter.registerHandler(
        Opcode::Client_GuildDemoteMember,
        GuildHandlers::handleGuildDemoteMember,
        SessionState::InWorld,
        true,
        "Client_GuildDemoteMember"
    );

    // Guild disband
    sPacketRouter.registerHandler(
        Opcode::Client_GuildDisband,
        GuildHandlers::handleGuildDisband,
        SessionState::InWorld,
        true,
        "Client_GuildDisband"
    );

    // Guild MOTD
    sPacketRouter.registerHandler(
        Opcode::Client_GuildMotd,
        GuildHandlers::handleGuildMotd,
        SessionState::InWorld,
        true,
        "Client_GuildMotd"
    );

    // Guild roster request
    sPacketRouter.registerHandler(
        Opcode::Client_GuildRosterRequest,
        GuildHandlers::handleGuildRosterRequest,
        SessionState::InWorld,
        true,
        "Client_GuildRosterRequest"
    );

    // =========================================================================
    // Duel Handlers (Phase 8, Task 8.8)
    // =========================================================================

    // Duel response (accept/decline)
    sPacketRouter.registerHandler(
        Opcode::Client_DuelResponse,
        DuelHandlers::handleDuelResponse,
        SessionState::InWorld,
        true,
        "Client_DuelResponse"
    );

    // Yield duel (surrender)
    sPacketRouter.registerHandler(
        Opcode::Client_YieldDuel,
        DuelHandlers::handleYieldDuel,
        SessionState::InWorld,
        true,
        "Client_YieldDuel"
    );

    // =========================================================================
    // Phase 9 - Additional Handlers (Missing Opcodes)
    // =========================================================================

    // Request ability list (sends spellbook)
    sPacketRouter.registerHandler(
        Opcode::Client_ReqAbilityList,
        handleReqAbilityList,
        SessionState::InWorld,
        true,
        "Client_ReqAbilityList"
    );

    // Toggle PvP flag
    sPacketRouter.registerHandler(
        Opcode::Client_TogglePvP,
        handleTogglePvP,
        SessionState::InWorld,
        true,
        "Client_TogglePvP"
    );

    // Respec (stat/talent reset)
    sPacketRouter.registerHandler(
        Opcode::Client_Respec,
        handleRespec,
        SessionState::InWorld,
        true,
        "Client_Respec"
    );

    // Query waypoints
    sPacketRouter.registerHandler(
        Opcode::Client_QueryWaypoints,
        handleQueryWaypoints,
        SessionState::InWorld,
        true,
        "Client_QueryWaypoints"
    );

    // Activate waypoint (teleport)
    sPacketRouter.registerHandler(
        Opcode::Client_ActivateWaypoint,
        handleActivateWaypoint,
        SessionState::InWorld,
        true,
        "Client_ActivateWaypoint"
    );

    // Reset dungeon lockouts
    sPacketRouter.registerHandler(
        Opcode::Client_ResetDungeons,
        handleResetDungeons,
        SessionState::InWorld,
        true,
        "Client_ResetDungeons"
    );

    // Socket gem into item
    sPacketRouter.registerHandler(
        Opcode::Client_SocketItem,
        handleSocketItem,
        SessionState::InWorld,
        true,
        "Client_SocketItem"
    );

    // Empower item with material
    sPacketRouter.registerHandler(
        Opcode::Client_EmpowerItem,
        handleEmpowerItem,
        SessionState::InWorld,
        true,
        "Client_EmpowerItem"
    );

    // Sort inventory
    sPacketRouter.registerHandler(
        Opcode::Client_SortInventory,
        handleSortInventory,
        SessionState::InWorld,
        true,
        "Client_SortInventory"
    );

    LOG_DEBUG("World handlers registered");
}

} // namespace Handlers
