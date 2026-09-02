// PlayerQuestLog - Tracks quest state for a player
// Phase 7, Task 7.6: Quest System

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#include "QuestDefines.h"

namespace Quest
{

struct QuestState
{
    int32_t questId = 0;
    QuestDefines::Status status = QuestDefines::Status::NotStarted;
    std::array<int32_t, 4> progress{{0, 0, 0, 0}};
};

class PlayerQuestLog
{
public:
    void load(int32_t characterGuid);
    void save(int32_t characterGuid) const;

    bool hasQuest(int32_t questId) const;
    QuestState* getQuest(int32_t questId);
    const QuestState* getQuest(int32_t questId) const;
    const std::unordered_map<int32_t, QuestState>& getQuests() const { return m_quests; }
    std::unordered_map<int32_t, QuestState>& getQuests() { return m_quests; }

    bool addQuest(int32_t questId);
    void removeQuest(int32_t questId);
    void setStatus(int32_t questId, QuestDefines::Status status);
    void setProgress(int32_t questId, const std::array<int32_t, 4>& progress);

    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

private:
    static std::array<int32_t, 4> parseProgress(const std::string& text);
    static std::string serializeProgress(const std::array<int32_t, 4>& progress);

    std::unordered_map<int32_t, QuestState> m_quests;
    bool m_dirty = false;
};

} // namespace Quest
