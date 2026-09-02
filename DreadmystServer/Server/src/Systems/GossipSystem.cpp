// GossipSystem - NPC gossip/dialog handling
// Phase 7, Task 7.5: Gossip/Dialog System

#include "stdafx.h"
#include "Systems/GossipSystem.h"
#include "Systems/VendorSystem.h"
#include "Systems/QuestManager.h"
#include "Core/Config.h"
#include "Core/Logger.h"
#include "World/Player.h"
#include "World/Npc.h"
#include "World/WorldManager.h"
#include "GamePacketServer.h"
#include "StlBuffer.h"

#include <sqlite3.h>

namespace Gossip
{

GossipManager& GossipManager::instance()
{
    static GossipManager instance;
    return instance;
}

void GossipManager::loadGossipData()
{
    if (m_loaded)
        return;

    m_loaded = true;
    m_gossipById.clear();
    m_optionsByGossipId.clear();
    m_optionsByEntry.clear();

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(sConfig.getGameDbPath().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("GossipManager: Failed to open game database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // Load gossip text entries
    {
        const char* sql = "SELECT entry, gossipId FROM gossip";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            LOG_ERROR("GossipManager: Failed to prepare gossip query: %s", sqlite3_errmsg(db));
            sqlite3_close(db);
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            GossipText text;
            text.entry = sqlite3_column_int(stmt, 0);
            text.gossipId = sqlite3_column_int(stmt, 1);
            m_gossipById[text.gossipId].push_back(text);
        }

        sqlite3_finalize(stmt);
    }

    // Load gossip options
    {
        const char* sql = "SELECT entry, gossipId, required_npc_flag, click_new_gossip, click_script "
                          "FROM gossip_option";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            LOG_ERROR("GossipManager: Failed to prepare gossip_option query: %s", sqlite3_errmsg(db));
            sqlite3_close(db);
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            GossipOption option;
            option.entry = sqlite3_column_int(stmt, 0);
            option.gossipId = sqlite3_column_int(stmt, 1);
            option.requiredNpcFlag = sqlite3_column_int(stmt, 2);
            option.clickNewGossip = sqlite3_column_int(stmt, 3);
            option.clickScript = sqlite3_column_int(stmt, 4);

            m_optionsByEntry[option.entry] = option;
            m_optionsByGossipId[option.gossipId].push_back(option.entry);
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);

    LOG_INFO("GossipManager: Loaded %zu gossip menus, %zu options",
             m_gossipById.size(), m_optionsByEntry.size());
}

int32_t GossipManager::selectGossipEntry(int32_t gossipId) const
{
    auto it = m_gossipById.find(gossipId);
    if (it == m_gossipById.end() || it->second.empty())
        return 0;

    return it->second.front().entry;
}

void GossipManager::showGossip(Player* player, Npc* npc, int32_t overrideGossipId)
{
    if (!player || !npc)
        return;

    loadGossipData();

    int32_t gossipId = overrideGossipId != 0 ? overrideGossipId : npc->getGossipMenuId();
    GP_Server_GossipMenu packet;
    packet.m_targetGuid = static_cast<uint32_t>(npc->getGuid());
    packet.m_gossipEntry = selectGossipEntry(gossipId);

    auto optIt = m_optionsByGossipId.find(gossipId);
    if (optIt != m_optionsByGossipId.end())
    {
        for (int32_t optionEntry : optIt->second)
        {
            packet.m_gossipOptions.push_back(optionEntry);
        }
    }

    if (npc->isVendor())
    {
        sVendorManager.populateVendorGossip(player, npc, packet.m_vendorItems);
    }

    if (npc->isQuestGiver())
    {
        sQuestManager.getQuestsForNpc(player, npc,
                                      packet.m_questOffers,
                                      packet.m_questCompletes);
    }

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);

    player->setGossipTargetGuid(static_cast<uint32_t>(npc->getGuid()));
}

void GossipManager::selectOption(Player* player, Npc* npc, int32_t optionEntry)
{
    if (!player || !npc)
        return;

    loadGossipData();

    auto it = m_optionsByEntry.find(optionEntry);
    if (it == m_optionsByEntry.end())
    {
        LOG_WARN("GossipManager: Option entry {} not found", optionEntry);
        return;
    }

    const GossipOption& option = it->second;

    if (option.clickNewGossip > 0)
    {
        showGossip(player, npc, option.clickNewGossip);
        return;
    }

    if (option.clickScript > 0)
    {
        LOG_DEBUG("GossipManager: Option {} has script {} (not implemented)",
                  optionEntry, option.clickScript);
    }
}

} // namespace Gossip
