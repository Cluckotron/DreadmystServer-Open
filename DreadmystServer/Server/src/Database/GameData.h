// GameData - Loads and caches game template data from game.db
// Task 2.3: Load game.db Templates

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

// ============================================================================
// Template Structures
// ============================================================================

struct SpellTemplate
{
    int32_t entry = 0;
    std::string name;
    std::string icon;
    std::string description;
    std::string auraDescription;
    std::string manaFormula;
    int32_t manaPct = 0;

    // Effects (3 max)
    int32_t effect[3] = {0};
    int32_t effectData1[3] = {0};
    int32_t effectData2[3] = {0};
    int32_t effectData3[3] = {0};
    int32_t effectTargetType[3] = {0};
    int32_t effectRadius[3] = {0};
    int32_t effectPositive[3] = {0};
    std::string effectScaleFormula[3];

    int32_t maxTargets = 0;
    int32_t dispel = 0;
    uint64_t attributes = 0;
    int32_t castTime = 0;
    int32_t cooldown = 0;
    int32_t cooldownCategory = 0;
    int32_t auraInterruptFlags = 0;
    int32_t castInterruptFlags = 0;
    int32_t magicRollSchool = 0;
    int32_t duration = 0;
    int32_t speed = 0;
    int32_t preventionType = 1;
    int32_t castSchool = 1;
    int32_t range = 0;
    int32_t rangeMin = 0;
    int32_t stackAmount = 0;
    int32_t requiredEquipment = 0;
    int32_t healthCost = 0;
    int32_t healthPctCost = 0;
    int32_t interval = 0;
    int32_t activatedByIn = 0;
    int32_t activatedByOut = 0;
    int32_t abilitiesTab = 0;
    int32_t canLevelUp = 0;

    // Reagents (5 max)
    int32_t reagent[5] = {0};
    int32_t reagentCount[5] = {0};

    int32_t reqCasterMechanic = 0;
    int32_t reqTargetMechanic = 0;
    int32_t reqTargetAura = 0;
    int32_t reqCasterAura = 0;
    int32_t statScale1 = 0;
    int32_t statScale2 = 0;
};

struct ItemTemplate
{
    int32_t entry = 0;
    int32_t sortEntry = 0;
    std::string name;
    std::string icon;
    std::string iconSound;
    std::string model;
    int32_t requiredLevel = 0;
    int32_t weaponType = 0;
    int32_t armorType = 0;
    int32_t equipType = 0;
    int32_t weaponMaterial = 0;
    int32_t numSockets = 0;
    int32_t quality = 0;
    int32_t itemLevel = 0;
    int32_t durability = 0;
    int32_t sellPrice = 0;
    int32_t stackCount = 1;
    int32_t requiredClass = 0;
    int32_t description = 0;
    int32_t flags = 0;
    int32_t questOffer = 0;
    int32_t buyCostRatio = 3;
    int32_t generated = 0;

    // Spells (5 max)
    int32_t spell[5] = {0};

    // Stats (10 max)
    int32_t statType[10] = {0};
    int32_t statValue[10] = {0};
};

struct NpcTemplate
{
    int32_t entry = 0;
    std::string name;
    std::string subname;
    int32_t modelId = 1;
    int32_t minLevel = 1;
    int32_t maxLevel = 1;
    int32_t faction = 1;
    int32_t modelScale = 100;
    int32_t leashRange = 0;
    int32_t type = 0;
    int32_t npcFlags = 0;
    int32_t gameFlags = 0;
    int32_t gossipMenuId = 0;
    int32_t movementType = 0;
    int32_t pathId = 0;
    int32_t aiType = 0;
    int32_t mechanicImmuneMask = 0;

    // Resistances
    int32_t resistanceHoly = 0;
    int32_t resistanceFrost = 0;
    int32_t resistanceShadow = 0;
    int32_t resistanceFire = 0;

    // Stats
    int32_t strength = 0;
    int32_t agility = 0;
    int32_t intellect = 0;
    int32_t willpower = 0;
    int32_t courage = 0;
    int32_t armor = 25;

    int32_t health = -1;  // -1 means auto-calculate
    int32_t mana = -1;
    int32_t weaponValue = -1;
    int32_t meleeSkill = -1;
    int32_t rangedSkill = 0;
    int32_t shieldSkill = 0;
    int32_t rangedWeaponValue = 0;
    int32_t meleeSpeed = 2000;
    int32_t rangedSpeed = 0;

    // Loot chances
    float lootGreenChance = -1;
    float lootBlueChance = -1;
    float lootGoldChance = -1;
    float lootPurpleChance = -1;

    std::string portrait;
    int32_t customLoot = -1;
    int32_t customGoldRatio = -1;
    int32_t boolElite = 0;
    int32_t boolBoss = 0;

    // Spells
    int32_t spellPrimary = 0;
    struct SpellSlot {
        int32_t id = 0;
        int32_t chance = 30;
        int32_t interval = 1500;
        int32_t cooldown = 0;
        int32_t targetType = 0;
    } spells[4];
};

struct QuestTemplate
{
    int32_t entry = 0;
    std::string name;
    std::string description;
    std::string objective;
    std::string offerRewardText;
    std::string exploreDescription;
    int32_t minLevel = 0;
    int32_t flags = 0;
    int32_t prevQuest[3] = {0};

    // Requirements (4 max each)
    int32_t reqItem[4] = {0};
    int32_t reqNpc[4] = {0};
    int32_t reqGo[4] = {0};
    int32_t reqSpell[4] = {0};
    int32_t reqCount[4] = {0};

    // Choice rewards (4 max)
    int32_t rewChoiceItem[4] = {0};
    int32_t rewChoiceCount[4] = {0};

    // Fixed rewards (4 max)
    int32_t rewItem[4] = {0};
    int32_t rewItemCount[4] = {0};

    int32_t rewPvpPoints = 0;
    int32_t rewMoney = 0;
    int32_t rewXp = 0;
    int32_t startScript = 0;
    int32_t completeScript = 0;
    int32_t startNpcEntry = 0;
    int32_t finishNpcEntry = 0;
    int32_t providedItem = 0;
};

struct MapTemplate
{
    int32_t id = 0;
    std::string name;
    std::string music;
    std::string ambience;
    int32_t losVision = 1;
    int32_t startX = 0;
    int32_t startY = 0;
    int32_t startO = 0;
};

struct ExpLevelInfo
{
    int32_t level = 0;
    int32_t exp = 0;
    int32_t killExp = 0;
    std::string name;
};

struct ClassLevelStats
{
    int32_t health = 0;
    int32_t mana = 0;
    int32_t strength = 0;
    int32_t agility = 0;
    int32_t willpower = 0;
    int32_t intelligence = 0;
    int32_t courage = 0;
    int32_t staffSkill = 0;
    int32_t maceSkill = 0;
    int32_t axesSkill = 0;
    int32_t swordSkill = 0;
    int32_t rangedSkill = 0;
    int32_t daggerSkill = 0;
    int32_t wandSkill = 0;
    int32_t shieldSkill = 0;
};

struct GameObjectTemplate
{
    int32_t entry = 0;
    std::string name;
    int32_t type = 0;
    int32_t flags = 0;
    int32_t model = 0;
    int32_t requiredQuest = 0;
    int32_t requiredItem = 0;
    int32_t data[12] = {0};
};

// ============================================================================
// GameData Manager
// ============================================================================

class GameData
{
public:
    static GameData& instance();

    // Load all data from game.db
    bool loadFromDatabase(const std::string& path);
    void unload();

    // Template lookups (returns nullptr if not found)
    const SpellTemplate* getSpell(int32_t entry) const;
    const ItemTemplate* getItem(int32_t entry) const;
    const NpcTemplate* getNpc(int32_t entry) const;
    const QuestTemplate* getQuest(int32_t entry) const;
    const std::unordered_map<int32_t, QuestTemplate>& getAllQuests() const { return m_quests; }
    const MapTemplate* getMap(int32_t id) const;
    const GameObjectTemplate* getGameObject(int32_t entry) const;
    const ExpLevelInfo* getExpLevel(int32_t level) const;
    const ClassLevelStats* getClassStats(int32_t classId, int32_t level) const;
    const std::vector<int32_t>& getStartingSpells(int32_t classId) const;

    // Get counts for logging/debugging
    size_t getSpellCount() const { return m_spells.size(); }
    size_t getItemCount() const { return m_items.size(); }
    size_t getNpcCount() const { return m_npcs.size(); }
    size_t getQuestCount() const { return m_quests.size(); }
    size_t getMapCount() const { return m_maps.size(); }
    size_t getGameObjectCount() const { return m_gameObjects.size(); }

    // Get max level
    int32_t getMaxLevel() const { return static_cast<int32_t>(m_expLevels.size()); }

    // Get experience needed for level
    int32_t getExpForLevel(int32_t level) const;

private:
    GameData() = default;

    bool loadSpells(sqlite3* db);
    bool loadItems(sqlite3* db);
    bool loadNpcs(sqlite3* db);
    bool loadQuests(sqlite3* db);
    bool loadMaps(sqlite3* db);
    bool loadGameObjects(sqlite3* db);
    bool loadExpLevels(sqlite3* db);
    bool loadClassStats(sqlite3* db);
    bool loadStartingSpells(sqlite3* db);

    std::unordered_map<int32_t, SpellTemplate> m_spells;
    std::unordered_map<int32_t, ItemTemplate> m_items;
    std::unordered_map<int32_t, NpcTemplate> m_npcs;
    std::unordered_map<int32_t, QuestTemplate> m_quests;
    std::unordered_map<int32_t, MapTemplate> m_maps;
    std::unordered_map<int32_t, GameObjectTemplate> m_gameObjects;
    std::vector<ExpLevelInfo> m_expLevels;  // Indexed by level
    std::unordered_map<int32_t, std::unordered_map<int32_t, ClassLevelStats>> m_classStats;
    std::unordered_map<int32_t, std::vector<int32_t>> m_startingSpells;
};

#define sGameData GameData::instance()
