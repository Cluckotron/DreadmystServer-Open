// QuestManager - Quest templates and progression logic
// Phase 7, Task 7.6+: Quest System

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "QuestDefines.h"
#include "ObjDefines.h"

class Player;
class Npc;
struct QuestTemplate;

namespace Quest
{

struct QuestObjective
{
    QuestDefines::TallyType type = QuestDefines::TallyNpc;
    int32_t targetId = 0;
    int32_t requiredCount = 0;
    int32_t slotIndex = 0;
};

class QuestManager
{
public:
    static QuestManager& instance();

    bool isQuestAvailable(Player* player, int32_t questId) const;
    void getQuestsForNpc(Player* player, Npc* npc,
                         std::vector<int32_t>& outOffers,
                         std::vector<int32_t>& outCompletes) const;

    bool acceptQuest(Player* player, int32_t questId);
    bool completeQuest(Player* player, int32_t questId, int32_t rewardChoice);
    bool abandonQuest(Player* player, int32_t questId);

    void onNpcKilled(Player* player, int32_t npcEntry);
    void onInventoryChanged(Player* player);
    void onSpellCast(Player* player, int32_t spellId);

    ObjDefines::GossipStatus getGossipStatus(Player* player, Npc* npc) const;

private:
    QuestManager() = default;
    ~QuestManager() = default;
    QuestManager(const QuestManager&) = delete;
    QuestManager& operator=(const QuestManager&) = delete;

    std::vector<QuestObjective> buildObjectives(const QuestTemplate& quest) const;
    bool areObjectivesComplete(const QuestTemplate& quest, const std::array<int32_t, 4>& progress) const;
    void updateQuestCompletion(Player* player, const QuestTemplate& quest);
    void sendQuestTally(Player* player, int32_t questId, QuestDefines::TallyType type,
                        int32_t targetId, int32_t progress) const;
    void refreshNpcGossipStatuses(Player* player) const;
    bool removeRequiredItems(Player* player, const QuestTemplate& quest) const;
    bool canReceiveRewards(Player* player, const QuestTemplate& quest, int32_t rewardChoice) const;
    void grantRewards(Player* player, const QuestTemplate& quest, int32_t rewardChoice) const;
};

#define sQuestManager Quest::QuestManager::instance()

} // namespace Quest
