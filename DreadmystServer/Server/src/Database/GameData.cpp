// GameData - Loads and caches game template data from game.db
// Task 2.3: Load game.db Templates

#include "stdafx.h"
#include "Database/GameData.h"
#include "Core/Logger.h"
#include <sqlite3.h>

// Helper to safely get string from SQLite column
static std::string getColumnString(sqlite3_stmt* stmt, int col)
{
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

// Helper to safely get int from SQLite column
static int32_t getColumnInt(sqlite3_stmt* stmt, int col)
{
    return sqlite3_column_int(stmt, col);
}

static int64_t getColumnInt64(sqlite3_stmt* stmt, int col)
{
    return sqlite3_column_int64(stmt, col);
}

// Helper to safely get double from SQLite column
static double getColumnDouble(sqlite3_stmt* stmt, int col)
{
    return sqlite3_column_double(stmt, col);
}

// ============================================================================
// GameData Implementation
// ============================================================================

GameData& GameData::instance()
{
    static GameData instance;
    return instance;
}

bool GameData::loadFromDatabase(const std::string& path)
{
    sqlite3* db = nullptr;
    int result = sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (result != SQLITE_OK) {
        LOG_ERROR("Failed to open game database: %s", path.c_str());
        return false;
    }

    LOG_INFO("Loading game data from: %s", path.c_str());

    bool success = true;
    success = loadSpells(db) && success;
    success = loadItems(db) && success;
    success = loadNpcs(db) && success;
    success = loadQuests(db) && success;
    success = loadMaps(db) && success;
    success = loadGameObjects(db) && success;
    success = loadExpLevels(db) && success;
    success = loadClassStats(db) && success;
    success = loadStartingSpells(db) && success;

    sqlite3_close(db);

    if (success) {
        LOG_INFO("Game data loaded: %zu spells, %zu items, %zu NPCs, %zu quests, %zu maps",
                 m_spells.size(), m_items.size(), m_npcs.size(), m_quests.size(), m_maps.size());
    }

    return success;
}

void GameData::unload()
{
    m_spells.clear();
    m_items.clear();
    m_npcs.clear();
    m_quests.clear();
    m_maps.clear();
    m_gameObjects.clear();
    m_expLevels.clear();
    m_classStats.clear();
    m_startingSpells.clear();
}

const SpellTemplate* GameData::getSpell(int32_t entry) const
{
    auto it = m_spells.find(entry);
    return it != m_spells.end() ? &it->second : nullptr;
}

const ItemTemplate* GameData::getItem(int32_t entry) const
{
    auto it = m_items.find(entry);
    return it != m_items.end() ? &it->second : nullptr;
}

const NpcTemplate* GameData::getNpc(int32_t entry) const
{
    auto it = m_npcs.find(entry);
    return it != m_npcs.end() ? &it->second : nullptr;
}

const QuestTemplate* GameData::getQuest(int32_t entry) const
{
    auto it = m_quests.find(entry);
    return it != m_quests.end() ? &it->second : nullptr;
}

const MapTemplate* GameData::getMap(int32_t id) const
{
    auto it = m_maps.find(id);
    return it != m_maps.end() ? &it->second : nullptr;
}

const GameObjectTemplate* GameData::getGameObject(int32_t entry) const
{
    auto it = m_gameObjects.find(entry);
    return it != m_gameObjects.end() ? &it->second : nullptr;
}

const ExpLevelInfo* GameData::getExpLevel(int32_t level) const
{
    if (level >= 1 && level <= static_cast<int32_t>(m_expLevels.size())) {
        return &m_expLevels[level - 1];
    }
    return nullptr;
}

const ClassLevelStats* GameData::getClassStats(int32_t classId, int32_t level) const
{
    auto classIt = m_classStats.find(classId);
    if (classIt == m_classStats.end())
        return nullptr;

    auto levelIt = classIt->second.find(level);
    return levelIt != classIt->second.end() ? &levelIt->second : nullptr;
}

const std::vector<int32_t>& GameData::getStartingSpells(int32_t classId) const
{
    static const std::vector<int32_t> empty;
    auto it = m_startingSpells.find(classId);
    return it != m_startingSpells.end() ? it->second : empty;
}

int32_t GameData::getExpForLevel(int32_t level) const
{
    const ExpLevelInfo* info = getExpLevel(level);
    return info ? info->exp : 0;
}

bool GameData::loadSpells(sqlite3* db)
{
    const char* sql = R"(
        SELECT entry, name, icon, description, aura_description, mana_formula, mana_pct,
               effect1, effect2, effect3,
               effect1_data1, effect2_data1, effect3_data1,
               effect1_data2, effect2_data2, effect3_data2,
               effect1_data3, effect2_data3, effect3_data3,
               effect1_targetType, effect2_targetType, effect3_targetType,
               effect1_radius, effect2_radius, effect3_radius,
               effect1_positive, effect2_positive, effect3_positive,
               effect1_scale_formula, effect2_scale_formula, effect3_scale_formula,
               maxTargets, dispel, attributes, cast_time, cooldown, cooldown_category,
               aura_interrupt_flags, cast_interrupt_flags, magic_roll_school,
               duration, speed, prevention_type, cast_school, range, stack_amount,
               required_equipment, health_cost, health_pct_cost, interval,
               activated_by_in, activated_by_out, abilities_tab,
               reagent1, reagent2, reagent3, reagent4, reagent5,
               reagent_count1, reagent_count2, reagent_count3, reagent_count4, reagent_count5,
               req_caster_mechanic, req_tar_mechanic, req_tar_aura, req_caster_aura,
               stat_scale_1, stat_scale_2, can_level_up, range_min
        FROM spell_template
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare spell query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SpellTemplate spell;
        int col = 0;

        spell.entry = getColumnInt(stmt, col++);
        spell.name = getColumnString(stmt, col++);
        spell.icon = getColumnString(stmt, col++);
        spell.description = getColumnString(stmt, col++);
        spell.auraDescription = getColumnString(stmt, col++);
        spell.manaFormula = getColumnString(stmt, col++);
        spell.manaPct = getColumnInt(stmt, col++);

        for (int i = 0; i < 3; ++i) spell.effect[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectData1[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectData2[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectData3[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectTargetType[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectRadius[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectPositive[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 3; ++i) spell.effectScaleFormula[i] = getColumnString(stmt, col++);

        spell.maxTargets = getColumnInt(stmt, col++);
        spell.dispel = getColumnInt(stmt, col++);
        spell.attributes = static_cast<uint64_t>(getColumnInt64(stmt, col++));
        spell.castTime = getColumnInt(stmt, col++);
        spell.cooldown = getColumnInt(stmt, col++);
        spell.cooldownCategory = getColumnInt(stmt, col++);
        spell.auraInterruptFlags = getColumnInt(stmt, col++);
        spell.castInterruptFlags = getColumnInt(stmt, col++);
        spell.magicRollSchool = getColumnInt(stmt, col++);
        spell.duration = getColumnInt(stmt, col++);
        spell.speed = getColumnInt(stmt, col++);
        spell.preventionType = getColumnInt(stmt, col++);
        spell.castSchool = getColumnInt(stmt, col++);
        spell.range = getColumnInt(stmt, col++);
        spell.stackAmount = getColumnInt(stmt, col++);
        spell.requiredEquipment = getColumnInt(stmt, col++);
        spell.healthCost = getColumnInt(stmt, col++);
        spell.healthPctCost = getColumnInt(stmt, col++);
        spell.interval = getColumnInt(stmt, col++);
        spell.activatedByIn = getColumnInt(stmt, col++);
        spell.activatedByOut = getColumnInt(stmt, col++);
        spell.abilitiesTab = getColumnInt(stmt, col++);

        for (int i = 0; i < 5; ++i) spell.reagent[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 5; ++i) spell.reagentCount[i] = getColumnInt(stmt, col++);

        spell.reqCasterMechanic = getColumnInt(stmt, col++);
        spell.reqTargetMechanic = getColumnInt(stmt, col++);
        spell.reqTargetAura = getColumnInt(stmt, col++);
        spell.reqCasterAura = getColumnInt(stmt, col++);
        spell.statScale1 = getColumnInt(stmt, col++);
        spell.statScale2 = getColumnInt(stmt, col++);
        spell.canLevelUp = getColumnInt(stmt, col++);
        spell.rangeMin = getColumnInt(stmt, col++);

        m_spells[spell.entry] = std::move(spell);
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu spell templates", m_spells.size());
    return true;
}

bool GameData::loadItems(sqlite3* db)
{
    const char* sql = R"(
        SELECT entry, sort_entry, name, icon, icon_sound, model, required_level,
               weapon_type, armor_type, equip_type, weapon_material, num_sockets,
               quality, item_level, durability, sell_price, stack_count, required_class,
               description, flags, quest_offer, buy_cost_ratio, generated,
               spell_1, spell_2, spell_3, spell_4, spell_5,
               stat_type1, stat_value1, stat_type2, stat_value2,
               stat_type3, stat_value3, stat_type4, stat_value4,
               stat_type5, stat_value5, stat_type6, stat_value6,
               stat_type7, stat_value7, stat_type8, stat_value8,
               stat_type9, stat_value9, stat_type10, stat_value10
        FROM item_template
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare item query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ItemTemplate item;
        int col = 0;

        item.entry = getColumnInt(stmt, col++);
        item.sortEntry = getColumnInt(stmt, col++);
        item.name = getColumnString(stmt, col++);
        item.icon = getColumnString(stmt, col++);
        item.iconSound = getColumnString(stmt, col++);
        item.model = getColumnString(stmt, col++);
        item.requiredLevel = getColumnInt(stmt, col++);
        item.weaponType = getColumnInt(stmt, col++);
        item.armorType = getColumnInt(stmt, col++);
        item.equipType = getColumnInt(stmt, col++);
        item.weaponMaterial = getColumnInt(stmt, col++);
        item.numSockets = getColumnInt(stmt, col++);
        item.quality = getColumnInt(stmt, col++);
        item.itemLevel = getColumnInt(stmt, col++);
        item.durability = getColumnInt(stmt, col++);
        item.sellPrice = getColumnInt(stmt, col++);
        item.stackCount = getColumnInt(stmt, col++);
        item.requiredClass = getColumnInt(stmt, col++);
        item.description = getColumnInt(stmt, col++);
        item.flags = getColumnInt(stmt, col++);
        item.questOffer = getColumnInt(stmt, col++);
        item.buyCostRatio = getColumnInt(stmt, col++);
        item.generated = getColumnInt(stmt, col++);

        for (int i = 0; i < 5; ++i) item.spell[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 10; ++i) {
            item.statType[i] = getColumnInt(stmt, col++);
            item.statValue[i] = getColumnInt(stmt, col++);
        }

        m_items[item.entry] = std::move(item);
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu item templates", m_items.size());
    return true;
}

bool GameData::loadNpcs(sqlite3* db)
{
    const char* sql = R"(
        SELECT entry, name, subname, model_id, min_level, max_level, faction,
               model_scale, leash_range, type, npc_flags, game_flags,
               gossip_menu_id, movement_type, path_id, ai_type, mechanic_immune_mask,
               resistance_holy, resistance_frost, resistance_shadow, resistance_fire,
               strength, agility, intellect, willpower, courage, armor,
               health, mana, weapon_value, melee_skill, ranged_skill, shield_skill,
               ranged_weapon_value, melee_speed, ranged_speed,
               loot_green_chance, loot_blue_chance, loot_gold_chance, loot_purple_chance,
               portrait, custom_loot, custom_gold_ratio, bool_elite, bool_boss,
               spell_primary,
               spell_1_id, spell_1_chance, spell_1_interval, spell_1_cooldown, spell_1_targetType,
               spell_2_id, spell_2_chance, spell_2_interval, spell_2_cooldown, spell_2_targetType,
               spell_3_id, spell_3_chance, spell_3_interval, spell_3_cooldown, spell_3_targetType,
               spell_4_id, spell_4_chance, spell_4_interval, spell_4_cooldown, spell_4_targetType
        FROM npc_template
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare NPC query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NpcTemplate npc;
        int col = 0;

        npc.entry = getColumnInt(stmt, col++);
        npc.name = getColumnString(stmt, col++);
        npc.subname = getColumnString(stmt, col++);
        npc.modelId = getColumnInt(stmt, col++);
        npc.minLevel = getColumnInt(stmt, col++);
        npc.maxLevel = getColumnInt(stmt, col++);
        npc.faction = getColumnInt(stmt, col++);
        npc.modelScale = getColumnInt(stmt, col++);
        npc.leashRange = getColumnInt(stmt, col++);
        npc.type = getColumnInt(stmt, col++);
        npc.npcFlags = getColumnInt(stmt, col++);
        npc.gameFlags = getColumnInt(stmt, col++);
        npc.gossipMenuId = getColumnInt(stmt, col++);
        npc.movementType = getColumnInt(stmt, col++);
        npc.pathId = getColumnInt(stmt, col++);
        npc.aiType = getColumnInt(stmt, col++);
        npc.mechanicImmuneMask = getColumnInt(stmt, col++);

        npc.resistanceHoly = getColumnInt(stmt, col++);
        npc.resistanceFrost = getColumnInt(stmt, col++);
        npc.resistanceShadow = getColumnInt(stmt, col++);
        npc.resistanceFire = getColumnInt(stmt, col++);

        npc.strength = getColumnInt(stmt, col++);
        npc.agility = getColumnInt(stmt, col++);
        npc.intellect = getColumnInt(stmt, col++);
        npc.willpower = getColumnInt(stmt, col++);
        npc.courage = getColumnInt(stmt, col++);
        npc.armor = getColumnInt(stmt, col++);

        npc.health = getColumnInt(stmt, col++);
        npc.mana = getColumnInt(stmt, col++);
        npc.weaponValue = getColumnInt(stmt, col++);
        npc.meleeSkill = getColumnInt(stmt, col++);
        npc.rangedSkill = getColumnInt(stmt, col++);
        npc.shieldSkill = getColumnInt(stmt, col++);
        npc.rangedWeaponValue = getColumnInt(stmt, col++);
        npc.meleeSpeed = getColumnInt(stmt, col++);
        npc.rangedSpeed = getColumnInt(stmt, col++);

        npc.lootGreenChance = static_cast<float>(getColumnDouble(stmt, col++));
        npc.lootBlueChance = static_cast<float>(getColumnDouble(stmt, col++));
        npc.lootGoldChance = static_cast<float>(getColumnDouble(stmt, col++));
        npc.lootPurpleChance = static_cast<float>(getColumnDouble(stmt, col++));

        npc.portrait = getColumnString(stmt, col++);
        npc.customLoot = getColumnInt(stmt, col++);
        npc.customGoldRatio = getColumnInt(stmt, col++);
        npc.boolElite = getColumnInt(stmt, col++);
        npc.boolBoss = getColumnInt(stmt, col++);
        npc.spellPrimary = getColumnInt(stmt, col++);

        for (int i = 0; i < 4; ++i) {
            npc.spells[i].id = getColumnInt(stmt, col++);
            npc.spells[i].chance = getColumnInt(stmt, col++);
            npc.spells[i].interval = getColumnInt(stmt, col++);
            npc.spells[i].cooldown = getColumnInt(stmt, col++);
            npc.spells[i].targetType = getColumnInt(stmt, col++);
        }

        m_npcs[npc.entry] = std::move(npc);
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu NPC templates", m_npcs.size());
    return true;
}

bool GameData::loadQuests(sqlite3* db)
{
    const char* sql = R"(
        SELECT entry, name, description, objective, offer_reward_text, explore_description,
               min_level, flags, prev_quest1, prev_quest2, prev_quest3,
               req_item1, req_item2, req_item3, req_item4,
               req_npc1, req_npc2, req_npc3, req_npc4,
               req_go1, req_go2, req_go3, req_go4,
               req_spell1, req_spell2, req_spell3, req_spell4,
               req_count1, req_count2, req_count3, req_count4,
               rew_choice1_item, rew_choice2_item, rew_choice3_item, rew_choice4_item,
               rew_choice1_count, rew_choice2_count, rew_choice3_count, rew_choice4_count,
               rew_item1, rew_item2, rew_item3, rew_item4,
               rew_item1_count, rew_item2_count, rew_item3_count, rew_item4_count,
               rew_pvp_points, rew_money, rew_xp,
               start_script, complete_script, start_npc_entry, finish_npc_entry, provided_item
        FROM quest_template
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare quest query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QuestTemplate quest;
        int col = 0;

        quest.entry = getColumnInt(stmt, col++);
        quest.name = getColumnString(stmt, col++);
        quest.description = getColumnString(stmt, col++);
        quest.objective = getColumnString(stmt, col++);
        quest.offerRewardText = getColumnString(stmt, col++);
        quest.exploreDescription = getColumnString(stmt, col++);
        quest.minLevel = getColumnInt(stmt, col++);
        quest.flags = getColumnInt(stmt, col++);

        for (int i = 0; i < 3; ++i) quest.prevQuest[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.reqItem[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.reqNpc[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.reqGo[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.reqSpell[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.reqCount[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.rewChoiceItem[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.rewChoiceCount[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.rewItem[i] = getColumnInt(stmt, col++);
        for (int i = 0; i < 4; ++i) quest.rewItemCount[i] = getColumnInt(stmt, col++);

        quest.rewPvpPoints = getColumnInt(stmt, col++);
        quest.rewMoney = getColumnInt(stmt, col++);
        quest.rewXp = getColumnInt(stmt, col++);
        quest.startScript = getColumnInt(stmt, col++);
        quest.completeScript = getColumnInt(stmt, col++);
        quest.startNpcEntry = getColumnInt(stmt, col++);
        quest.finishNpcEntry = getColumnInt(stmt, col++);
        quest.providedItem = getColumnInt(stmt, col++);

        m_quests[quest.entry] = std::move(quest);
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu quest templates", m_quests.size());
    return true;
}

bool GameData::loadMaps(sqlite3* db)
{
    const char* sql = "SELECT id, name, music, ambience, los_vision, start_x, start_y, start_o FROM map";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare map query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MapTemplate map;
        int col = 0;

        map.id = getColumnInt(stmt, col++);
        map.name = getColumnString(stmt, col++);
        map.music = getColumnString(stmt, col++);
        map.ambience = getColumnString(stmt, col++);
        map.losVision = getColumnInt(stmt, col++);
        map.startX = getColumnInt(stmt, col++);
        map.startY = getColumnInt(stmt, col++);
        map.startO = getColumnInt(stmt, col++);

        m_maps[map.id] = std::move(map);
    }

    sqlite3_finalize(stmt);

    // The released game DB leaves Fanadin's map.start_x/start_y empty, but it
    // contains the canonical new-player destination in teleport_names as
    // name='start'.  Resolve that data into the cached MapTemplate so character
    // creation and respawn use the same real starting point instead of a
    // reconstruction placeholder.
    const char* startSql =
        "SELECT target_mapId, target_x, target_y "
        "FROM teleport_names WHERE lower(name) = 'start' ORDER BY entry";
    sqlite3_stmt* startStmt = nullptr;
    if (sqlite3_prepare_v2(db, startSql, -1, &startStmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(startStmt) == SQLITE_ROW) {
            const int32_t mapId = sqlite3_column_int(startStmt, 0);
            const int32_t startX = sqlite3_column_int(startStmt, 1);
            const int32_t startY = sqlite3_column_int(startStmt, 2);

            auto it = m_maps.find(mapId);
            if (it == m_maps.end())
                continue;

            // Prefer an explicit map-table start if one exists.  Fanadin's
            // shipped row is 0/NULL, so it falls through to teleport_names.
            if (it->second.startX <= 0 || it->second.startY <= 0) {
                it->second.startX = startX;
                it->second.startY = startY;
                if (it->second.startO < 0)
                    it->second.startO = 0;

                LOG_INFO("Map %d (%s): using canonical start teleport at (%d, %d)",
                         mapId, it->second.name.c_str(), startX, startY);
            }
        }
        sqlite3_finalize(startStmt);
    } else {
        LOG_WARN("Unable to resolve canonical map start teleport: %s", sqlite3_errmsg(db));
    }

    LOG_DEBUG("Loaded %zu map templates", m_maps.size());
    return true;
}

bool GameData::loadGameObjects(sqlite3* db)
{
    const char* sql = R"(
        SELECT entry, name, type, flags, model, required_quest, required_item,
               data1, data2, data3, data4, data5, data6, data7, data8, data9, data10, data11
        FROM gameobject_template
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare gameobject query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GameObjectTemplate go;
        int col = 0;

        go.entry = getColumnInt(stmt, col++);
        go.name = getColumnString(stmt, col++);
        go.type = getColumnInt(stmt, col++);
        go.flags = getColumnInt(stmt, col++);
        go.model = getColumnInt(stmt, col++);
        go.requiredQuest = getColumnInt(stmt, col++);
        go.requiredItem = getColumnInt(stmt, col++);

        for (int i = 0; i < 11; ++i) {
            go.data[i] = getColumnInt(stmt, col++);
        }

        m_gameObjects[go.entry] = std::move(go);
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu gameobject templates", m_gameObjects.size());
    return true;
}

bool GameData::loadExpLevels(sqlite3* db)
{
    const char* sql = "SELECT level, exp, kill_exp, name FROM player_exp_levels ORDER BY level ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare exp levels query: %s", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ExpLevelInfo info;
        info.level = getColumnInt(stmt, 0);
        info.exp = getColumnInt(stmt, 1);
        info.killExp = getColumnInt(stmt, 2);
        info.name = getColumnString(stmt, 3);
        m_expLevels.push_back(std::move(info));
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %zu experience levels (max level: %d)", m_expLevels.size(), getMaxLevel());
    return true;
}

bool GameData::loadClassStats(sqlite3* db)
{
    const char* sql =
        "SELECT Class, Level, HP, Mana, Strength, Agility, Willpower, Intelligence, Courage, "
        "Staves, Maces, Axes, Swords, Ranged, Daggers, Wands, Shields "
        "FROM player_class_stats";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare class stats query: %s", sqlite3_errmsg(db));
        return false;
    }

    int loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int32_t classId = getColumnInt(stmt, col++);
        int32_t level = getColumnInt(stmt, col++);

        ClassLevelStats stats;
        stats.health = getColumnInt(stmt, col++);
        stats.mana = getColumnInt(stmt, col++);
        stats.strength = getColumnInt(stmt, col++);
        stats.agility = getColumnInt(stmt, col++);
        stats.willpower = getColumnInt(stmt, col++);
        stats.intelligence = getColumnInt(stmt, col++);
        stats.courage = getColumnInt(stmt, col++);
        stats.staffSkill = getColumnInt(stmt, col++);
        stats.maceSkill = getColumnInt(stmt, col++);
        stats.axesSkill = getColumnInt(stmt, col++);
        stats.swordSkill = getColumnInt(stmt, col++);
        stats.rangedSkill = getColumnInt(stmt, col++);
        stats.daggerSkill = getColumnInt(stmt, col++);
        stats.wandSkill = getColumnInt(stmt, col++);
        stats.shieldSkill = getColumnInt(stmt, col++);

        m_classStats[classId][level] = stats;
        ++loaded;
    }

    sqlite3_finalize(stmt);
    LOG_DEBUG("Loaded %d class stat rows", loaded);
    return true;
}

bool GameData::loadStartingSpells(sqlite3* db)
{
    const char* sql = "SELECT class, spell FROM player_create_spell ORDER BY class, spell";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("Failed to prepare player_create_spell query: %s", sqlite3_errmsg(db));
        return false;
    }

    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const int32_t classId = sqlite3_column_int(stmt, 0);
        const int32_t spellId = sqlite3_column_int(stmt, 1);
        if (classId > 0 && spellId > 0)
        {
            m_startingSpells[classId].push_back(spellId);
            ++count;
        }
    }

    sqlite3_finalize(stmt);
    LOG_INFO("Loaded %zu starting spell assignments for %zu classes",
             count, m_startingSpells.size());
    return true;
}
