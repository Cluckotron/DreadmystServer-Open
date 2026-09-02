// GossipSystem - NPC gossip/dialog handling
// Phase 7, Task 7.5: Gossip/Dialog System

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

class Player;
class Npc;

namespace Gossip
{

// ---------------------------------------------------------------------------
// Gossip data
// ---------------------------------------------------------------------------

struct GossipText
{
    int32_t entry = 0;
    int32_t gossipId = 0;
};

struct GossipOption
{
    int32_t entry = 0;
    int32_t gossipId = 0;
    int32_t requiredNpcFlag = 0;
    int32_t clickNewGossip = 0;
    int32_t clickScript = 0;
};

// ---------------------------------------------------------------------------
// GossipManager - Singleton
// ---------------------------------------------------------------------------

class GossipManager
{
public:
    static GossipManager& instance();

    void loadGossipData();

    void showGossip(Player* player, Npc* npc, int32_t overrideGossipId = 0);
    void selectOption(Player* player, Npc* npc, int32_t optionEntry);

private:
    GossipManager() = default;
    ~GossipManager() = default;
    GossipManager(const GossipManager&) = delete;
    GossipManager& operator=(const GossipManager&) = delete;

    int32_t selectGossipEntry(int32_t gossipId) const;

    std::unordered_map<int32_t, std::vector<GossipText>> m_gossipById;
    std::unordered_map<int32_t, std::vector<int32_t>> m_optionsByGossipId;
    std::unordered_map<int32_t, GossipOption> m_optionsByEntry;
    bool m_loaded = false;
};

#define sGossipManager Gossip::GossipManager::instance()

} // namespace Gossip
