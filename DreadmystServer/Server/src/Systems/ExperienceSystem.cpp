// ExperienceSystem - XP gain and leveling
// Phase 7, Task 7.10-7.11: Experience/Level Up

#include "stdafx.h"
#include "Systems/ExperienceSystem.h"
#include "Database/GameData.h"
#include "World/Player.h"
#include "World/Npc.h"
#include "Core/Logger.h"
#include "GamePacketServer.h"
#include "StlBuffer.h"
#include "ObjDefines.h"
#include "PlayerDefines.h"
#include "UnitDefines.h"

namespace Experience
{

ExperienceSystem& ExperienceSystem::instance()
{
    static ExperienceSystem instance;
    return instance;
}

int32_t ExperienceSystem::getKillBaseXP(int32_t npcLevel) const
{
    const ExpLevelInfo* info = sGameData.getExpLevel(npcLevel);
    return info ? info->killExp : 0;
}

int32_t ExperienceSystem::calculateKillXP(int32_t playerLevel, int32_t npcLevel, int32_t npcBaseXP) const
{
    if (playerLevel <= 0 || npcLevel <= 0 || npcBaseXP <= 0)
        return 0;

    int32_t diff = npcLevel - playerLevel;
    int32_t maxDiff = PlayerDefines::Experience::MaxLevelDiffExp;

    if (diff <= -maxDiff)
        return 0;

    float multiplier = 1.0f;
    if (diff < 0)
        multiplier = std::max(0.1f, 1.0f + diff * 0.1f);
    else if (diff > 0)
        multiplier = 1.0f + diff * 0.1f;

    int32_t xp = static_cast<int32_t>(npcBaseXP * multiplier);
    return std::max(1, xp);
}

void ExperienceSystem::giveExperience(Player* player, int32_t amount, const std::string& source)
{
    if (!player || amount <= 0)
        return;

    int32_t oldLevel = player->getLevel();

    player->addExperience(amount);

    int32_t newLevel = checkLevelUp(player);
    int32_t currentXp = player->getExperience();

    player->setVariable(ObjDefines::Variable::Experience, currentXp);
    player->setVariable(ObjDefines::Variable::Progression, currentXp);
    player->broadcastVariable(ObjDefines::Variable::Experience, currentXp);
    player->broadcastVariable(ObjDefines::Variable::Progression, currentXp);

    GP_Server_ExpNotify packet;
    packet.m_amount = amount;
    packet.m_newLevel = newLevel > oldLevel ? newLevel : 0;

    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);
    player->sendPacket(buf);

    if (!source.empty())
    {
        LOG_DEBUG("Experience: '{}' gained {} XP from {} (level {} -> {}, xp={})",
                  player->getName(), amount, source, oldLevel, newLevel, currentXp);
    }
    else
    {
        LOG_DEBUG("Experience: '{}' gained {} XP (level {} -> {}, xp={})",
                  player->getName(), amount, oldLevel, newLevel, currentXp);
    }
}

void ExperienceSystem::givePartyExperience(const std::vector<Player*>& party, int32_t totalXP)
{
    if (party.empty() || totalXP <= 0)
        return;

    int32_t split = totalXP / static_cast<int32_t>(party.size());
    for (Player* member : party)
    {
        if (member)
            giveExperience(member, split, "party");
    }
}

void ExperienceSystem::onNpcKilled(Player* player, Npc* npc)
{
    if (!player || !npc)
        return;

    int32_t playerLevel = player->getLevel();
    int32_t npcLevel = npc->getVariable(ObjDefines::Variable::Level);
    int32_t baseXp = getKillBaseXP(npcLevel);
    int32_t xp = calculateKillXP(playerLevel, npcLevel, baseXp);

    if (xp <= 0)
        return;

    giveExperience(player, xp, npc->getName());
}

void ExperienceSystem::applyLevelStats(Player* player, int32_t level, bool preserveCurrent, bool broadcast)
{
    if (!player)
        return;

    const ClassLevelStats* stats = sGameData.getClassStats(player->getClassId(), level);
    if (!stats)
    {
        LOG_WARN("Experience: Missing class stats for class {} level {}", player->getClassId(), level);
        return;
    }

    int32_t maxHealth = stats->health;
    int32_t maxMana = stats->mana;

    player->setMaxHealth(maxHealth);
    player->setMaxMana(maxMana);

    int32_t newHealth = preserveCurrent ? std::min(player->getHealth(), maxHealth) : maxHealth;
    int32_t newMana = preserveCurrent ? std::min(player->getMana(), maxMana) : maxMana;

    player->setHealth(newHealth);
    player->setMana(newMana);

    auto setStat = [player, broadcast](UnitDefines::Stat stat, int32_t value)
    {
        ObjDefines::Variable var = static_cast<ObjDefines::Variable>(
            static_cast<int32_t>(ObjDefines::Variable::StatsStart) + static_cast<int32_t>(stat));
        player->setVariable(var, value);
        if (broadcast)
            player->broadcastVariable(var, value);
    };

    // Primary stats (base + manual + equipment + auras)
    setStat(UnitDefines::Stat::Strength, stats->strength + player->getTotalStatBonus(UnitDefines::Stat::Strength));
    setStat(UnitDefines::Stat::Agility, stats->agility + player->getTotalStatBonus(UnitDefines::Stat::Agility));
    setStat(UnitDefines::Stat::Willpower, stats->willpower + player->getTotalStatBonus(UnitDefines::Stat::Willpower));
    setStat(UnitDefines::Stat::Intelligence, stats->intelligence + player->getTotalStatBonus(UnitDefines::Stat::Intelligence));
    setStat(UnitDefines::Stat::Courage, stats->courage + player->getTotalStatBonus(UnitDefines::Stat::Courage));

    // Weapon skills
    setStat(UnitDefines::Stat::StaffSkill, stats->staffSkill + player->getTotalStatBonus(UnitDefines::Stat::StaffSkill));
    setStat(UnitDefines::Stat::MaceSkill, stats->maceSkill + player->getTotalStatBonus(UnitDefines::Stat::MaceSkill));
    setStat(UnitDefines::Stat::AxesSkill, stats->axesSkill + player->getTotalStatBonus(UnitDefines::Stat::AxesSkill));
    setStat(UnitDefines::Stat::SwordSkill, stats->swordSkill + player->getTotalStatBonus(UnitDefines::Stat::SwordSkill));
    setStat(UnitDefines::Stat::RangedSkill, stats->rangedSkill + player->getTotalStatBonus(UnitDefines::Stat::RangedSkill));
    setStat(UnitDefines::Stat::DaggerSkill, stats->daggerSkill + player->getTotalStatBonus(UnitDefines::Stat::DaggerSkill));
    setStat(UnitDefines::Stat::WandSkill, stats->wandSkill + player->getTotalStatBonus(UnitDefines::Stat::WandSkill));
    setStat(UnitDefines::Stat::ShieldSkill, stats->shieldSkill + player->getTotalStatBonus(UnitDefines::Stat::ShieldSkill));

    // Combat stats from equipment/buffs (no base value from class)
    setStat(UnitDefines::Stat::ArmorValue, player->getTotalStatBonus(UnitDefines::Stat::ArmorValue));
    setStat(UnitDefines::Stat::WeaponValue, player->getTotalStatBonus(UnitDefines::Stat::WeaponValue));
    setStat(UnitDefines::Stat::MeleeCritical, player->getTotalStatBonus(UnitDefines::Stat::MeleeCritical));
    setStat(UnitDefines::Stat::RangedCritical, player->getTotalStatBonus(UnitDefines::Stat::RangedCritical));
    setStat(UnitDefines::Stat::SpellCritical, player->getTotalStatBonus(UnitDefines::Stat::SpellCritical));
    setStat(UnitDefines::Stat::DodgeRating, player->getTotalStatBonus(UnitDefines::Stat::DodgeRating));
    setStat(UnitDefines::Stat::BlockRating, player->getTotalStatBonus(UnitDefines::Stat::BlockRating));
    setStat(UnitDefines::Stat::ParryRating, player->getTotalStatBonus(UnitDefines::Stat::ParryRating));

    // Resistances from equipment/buffs
    setStat(UnitDefines::Stat::ResistFrost, player->getTotalStatBonus(UnitDefines::Stat::ResistFrost));
    setStat(UnitDefines::Stat::ResistFire, player->getTotalStatBonus(UnitDefines::Stat::ResistFire));
    setStat(UnitDefines::Stat::ResistShadow, player->getTotalStatBonus(UnitDefines::Stat::ResistShadow));
    setStat(UnitDefines::Stat::ResistHoly, player->getTotalStatBonus(UnitDefines::Stat::ResistHoly));

    player->markDirty();

    if (broadcast)
    {
        player->broadcastVariable(ObjDefines::Variable::MaxHealth, maxHealth);
        player->broadcastVariable(ObjDefines::Variable::MaxMana, maxMana);
        player->broadcastVariable(ObjDefines::Variable::Health, newHealth);
        player->broadcastVariable(ObjDefines::Variable::Mana, newMana);
    }
}

int32_t ExperienceSystem::checkLevelUp(Player* player)
{
    if (!player)
        return 0;

    int32_t level = player->getLevel();
    int32_t maxLevel = sGameData.getMaxLevel();
    int32_t xp = player->getExperience();

    while (level < maxLevel)
    {
        int32_t required = sGameData.getExpForLevel(level);
        if (required <= 0 || xp < required)
            break;

        xp -= required;
        level += 1;
        player->setLevel(level);
        player->broadcastVariable(ObjDefines::Variable::Level, level);

        applyLevelStats(player, level, false, true);
    }

    if (xp != player->getExperience())
        player->setExperience(xp);

    return level;
}

} // namespace Experience
