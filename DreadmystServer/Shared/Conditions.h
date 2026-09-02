#pragma once
namespace Conditions
{
    enum Type
    {
        Source_Player_LevelGreaterThan = 0,
        Source_Player_HasItemInBag,
        Source_Player_HasItemEquipped,
        Source_Player_HasItemBagOrEquipped,
        Source_Unit_HasAura,
        Target_Player_HasItemInBag,
        Target_Player_HasItemEquipped,
        Target_Player_HasItemBagOrEquipped,
        Target_Unit_HasAura,
        Quest_PlayerHasQuest,
        Quest_PlayerHasOrDidQuest,
        Quest_PlayerTurnedInQuest,
        Quest_PlayerHasQuestFinishedInLog,
        Quest_PlayerHasQuestForItem
    };
}
