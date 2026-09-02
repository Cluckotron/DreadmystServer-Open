// PlayerQuestLog - Tracks quest state for a player
// Phase 7, Task 7.6: Quest System

#include "stdafx.h"
#include "Systems/PlayerQuestLog.h"
#include "Database/DatabaseManager.h"
#include "Core/Logger.h"

#include <sstream>

namespace Quest
{

std::array<int32_t, 4> PlayerQuestLog::parseProgress(const std::string& text)
{
    std::array<int32_t, 4> result{{0, 0, 0, 0}};
    if (text.empty())
        return result;

    std::stringstream ss(text);
    std::string token;
    size_t index = 0;
    while (std::getline(ss, token, ',') && index < result.size())
    {
        try
        {
            result[index] = std::stoi(token);
        }
        catch (const std::exception&)
        {
            result[index] = 0;
        }
        ++index;
    }

    return result;
}

std::string PlayerQuestLog::serializeProgress(const std::array<int32_t, 4>& progress)
{
    std::ostringstream oss;
    for (size_t i = 0; i < progress.size(); ++i)
    {
        if (i != 0)
            oss << ",";
        oss << progress[i];
    }
    return oss.str();
}

void PlayerQuestLog::load(int32_t characterGuid)
{
    m_quests.clear();

    auto stmt = sDatabase.prepare(
        "SELECT quest_id, status, progress FROM character_quests WHERE character_guid = ?"
    );

    if (!stmt.valid())
    {
        LOG_ERROR("QuestLog: Failed to prepare load statement");
        return;
    }

    stmt.bind(1, characterGuid);

    while (stmt.step())
    {
        QuestState state;
        state.questId = stmt.getInt(0);
        state.status = static_cast<QuestDefines::Status>(stmt.getInt(1));
        state.progress = parseProgress(stmt.getString(2));
        m_quests[state.questId] = state;
    }

    m_dirty = false;

    LOG_DEBUG("QuestLog: Loaded %zu quests for character %d", m_quests.size(), characterGuid);
}

void PlayerQuestLog::save(int32_t characterGuid) const
{
    if (!m_dirty)
        return;

    auto deleteStmt = sDatabase.prepare(
        "DELETE FROM character_quests WHERE character_guid = ?"
    );
    if (!deleteStmt.valid())
    {
        LOG_ERROR("QuestLog: Failed to prepare delete statement");
        return;
    }
    deleteStmt.bind(1, characterGuid);
    deleteStmt.step();

    auto insertStmt = sDatabase.prepare(
        "INSERT INTO character_quests (character_guid, quest_id, status, progress) "
        "VALUES (?, ?, ?, ?)"
    );
    if (!insertStmt.valid())
    {
        LOG_ERROR("QuestLog: Failed to prepare insert statement");
        return;
    }

    for (const auto& [questId, state] : m_quests)
    {
        insertStmt.reset();
        insertStmt.bind(1, characterGuid);
        insertStmt.bind(2, questId);
        insertStmt.bind(3, static_cast<int32_t>(state.status));
        insertStmt.bind(4, serializeProgress(state.progress));
        insertStmt.step();
    }

    LOG_DEBUG("QuestLog: Saved %zu quests for character %d", m_quests.size(), characterGuid);
}

bool PlayerQuestLog::hasQuest(int32_t questId) const
{
    return m_quests.find(questId) != m_quests.end();
}

QuestState* PlayerQuestLog::getQuest(int32_t questId)
{
    auto it = m_quests.find(questId);
    return it != m_quests.end() ? &it->second : nullptr;
}

const QuestState* PlayerQuestLog::getQuest(int32_t questId) const
{
    auto it = m_quests.find(questId);
    return it != m_quests.end() ? &it->second : nullptr;
}

bool PlayerQuestLog::addQuest(int32_t questId)
{
    if (hasQuest(questId))
        return false;

    QuestState state;
    state.questId = questId;
    state.status = QuestDefines::Status::InProgress;
    m_quests[questId] = state;
    m_dirty = true;
    return true;
}

void PlayerQuestLog::removeQuest(int32_t questId)
{
    m_quests.erase(questId);
    m_dirty = true;
}

void PlayerQuestLog::setStatus(int32_t questId, QuestDefines::Status status)
{
    auto it = m_quests.find(questId);
    if (it == m_quests.end())
        return;

    it->second.status = status;
    m_dirty = true;
}

void PlayerQuestLog::setProgress(int32_t questId, const std::array<int32_t, 4>& progress)
{
    auto it = m_quests.find(questId);
    if (it == m_quests.end())
        return;

    it->second.progress = progress;
    m_dirty = true;
}

} // namespace Quest
