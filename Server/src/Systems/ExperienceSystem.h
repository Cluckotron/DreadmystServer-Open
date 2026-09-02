// ExperienceSystem - XP gain and leveling
// Phase 7, Task 7.10-7.11: Experience/Level Up

#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Player;
class Npc;

namespace Experience
{

class ExperienceSystem
{
public:
    static ExperienceSystem& instance();

    int32_t calculateKillXP(int32_t playerLevel, int32_t npcLevel, int32_t npcBaseXP) const;
    void giveExperience(Player* player, int32_t amount, const std::string& source);
    void givePartyExperience(const std::vector<Player*>& party, int32_t totalXP);
    void onNpcKilled(Player* player, Npc* npc);

    void applyLevelStats(Player* player, int32_t level, bool preserveCurrent, bool broadcast = true);

private:
    ExperienceSystem() = default;
    int32_t getKillBaseXP(int32_t npcLevel) const;
    int32_t checkLevelUp(Player* player);
};

#define sExperienceSystem Experience::ExperienceSystem::instance()

} // namespace Experience
