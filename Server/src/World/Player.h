#pragma once

#include "Entity.h"
#include "../Database/CharacterDb.h"
#include "../Combat/CooldownManager.h"
#include "../Systems/Inventory.h"
#include "../Systems/Equipment.h"
#include "../Systems/BankSystem.h"
#include "../Systems/PlayerQuestLog.h"
#include "../../../Geo2d.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class Session;
class StlBuffer;

// Player entity - represents a player character in the world
class Player : public Entity
{
public:
    // Create player from character info and bind to session
    Player(Session& session, const CharacterInfo& info);
    ~Player() override;

    // Entity interface
    void update(float deltaTime) override;
    MutualObject::Type getType() const override { return MutualObject::Type::Player; }
    const std::string& getName() const override { return m_characterName; }

    // Session
    Session& getSession() { return m_session; }
    const Session& getSession() const { return m_session; }

    // Character info
    int32_t getCharacterGuid() const { return m_characterGuid; }
    int32_t getAccountId() const { return m_accountId; }
    int32_t getClassId() const { return m_classId; }
    int32_t getGender() const { return m_gender; }
    int32_t getLevel() const { return m_level; }
    int32_t getExperience() const { return m_experience; }
    int32_t getPortraitId() const { return m_portraitId; }
    int32_t getSkinColor() const { return m_skinColor; }
    int32_t getHairStyle() const { return m_hairStyle; }
    int32_t getHairColor() const { return m_hairColor; }
    int32_t getGold() const { return m_gold; }
    int32_t getPlayedTime() const { return m_playedTime; }

    // Stats
    int32_t getHealth() const { return getVariable(ObjDefines::Variable::Health); }
    int32_t getMaxHealth() const { return getVariable(ObjDefines::Variable::MaxHealth); }
    int32_t getMana() const { return getVariable(ObjDefines::Variable::Mana); }
    int32_t getMaxMana() const { return getVariable(ObjDefines::Variable::MaxMana); }

    void setHealth(int32_t value) { setVariable(ObjDefines::Variable::Health, value); }
    void setMaxHealth(int32_t value) { setVariable(ObjDefines::Variable::MaxHealth, value); }
    void setMana(int32_t value) { setVariable(ObjDefines::Variable::Mana, value); }
    void setMaxMana(int32_t value) { setVariable(ObjDefines::Variable::MaxMana, value); }

    // Experience/Level
    void addExperience(int32_t amount);
    void setExperience(int32_t value);
    void setLevel(int32_t level);

    // Gold
    void addGold(int32_t amount);
    bool spendGold(int32_t amount);
    void setGold(int32_t amount) { m_gold = amount; }

    // Packet sending helper
    void sendPacket(const StlBuffer& packet);

    // Persistence
    void save();
    void loadFromDatabase();

    // Generate CharacterInfo for packets
    CharacterInfo toCharacterInfo() const;

    // Time tracking
    void updatePlayedTime();

    // Movement
    bool isMoving() const { return m_isMoving; }
    void setMoving(bool moving) { m_isMoving = moving; }
    void setMovementPath(const std::vector<Geo2d::Vector2>& path);
    void clearMovementPath();
    const std::vector<Geo2d::Vector2>& getMovementPath() const { return m_movePath; }
    float getMovementSpeed() const;

    // Combat
    bool isInCombat() const;

    // Target selection (Task 5.15)
    uint32_t getSelectedTarget() const { return m_selectedTarget; }
    void setSelectedTarget(uint32_t guid) { m_selectedTarget = guid; }

    // Gossip tracking (Phase 7)
    uint32_t getGossipTargetGuid() const { return m_gossipTargetGuid; }
    void setGossipTargetGuid(uint32_t guid) { m_gossipTargetGuid = guid; }

    // Death handling (Task 5.11) - overrides Entity
    void onDeath(Entity* killer) override;

    // Respawn system (Task 5.12)
    void respawn();
    bool canRespawn() const;

    // Cooldown system (Task 5.7)
    CooldownManager& getCooldowns() { return m_cooldowns; }
    const CooldownManager& getCooldowns() const { return m_cooldowns; }

    // Cast system (cast time spells)
    struct PendingCast
    {
        int32_t spellId = 0;
        uint32_t targetGuid = 0;
        float remainingTime = 0.0f;  // Seconds remaining
        bool active = false;
    };

    bool isCasting() const { return m_pendingCast.active; }
    const PendingCast& getPendingCast() const { return m_pendingCast; }
    void startCast(int32_t spellId, uint32_t targetGuid, float castTime);
    void cancelCast();
    void completeCast();

    // Start cooldown for a spell and notify client
    void startCooldown(int32_t spellId, int32_t durationMs, int32_t categoryId = 0);

    // Start global cooldown and notify client
    void startGCD();

    // Send all active cooldowns to client (called on login)
    void sendAllCooldowns();

    // Inventory system (Phase 6, Task 6.1)
    Inventory::PlayerInventory& getInventory() { return m_inventory; }
    const Inventory::PlayerInventory& getInventory() const { return m_inventory; }

    // Send full inventory to client
    void sendInventory();

    // Equipment system (Phase 6, Task 6.3)
    Equipment::PlayerEquipment& getEquipment() { return m_equipment; }
    const Equipment::PlayerEquipment& getEquipment() const { return m_equipment; }

    // Send equipment state to client (broadcasts GP_Server_EquipItem for each slot)
    void sendEquipment();

    // Bank system (Phase 6, Task 6.6)
    Bank::PlayerBank& getBank() { return m_bank; }
    const Bank::PlayerBank& getBank() const { return m_bank; }

    // Quest log (Phase 7)
    Quest::PlayerQuestLog& getQuestLog() { return m_questLog; }
    const Quest::PlayerQuestLog& getQuestLog() const { return m_questLog; }

    // Inventory change hook (Phase 7 quest objectives)
    void onInventoryChanged();

    // Stat bonuses (Phase 7 level-up)
    int32_t getStatBonus(UnitDefines::Stat stat) const;           // Manual stat investments only
    int32_t getEquipmentStatBonus(UnitDefines::Stat stat) const;  // Equipment bonuses only
    int32_t getTotalStatBonus(UnitDefines::Stat stat) const;      // Manual + equipment + aura
    void addStatBonus(UnitDefines::Stat stat, int32_t amount);
    void setStatBonus(UnitDefines::Stat stat, int32_t value);
    const std::unordered_map<UnitDefines::Stat, int32_t>& getStatBonuses() const { return m_statBonuses; }

    // Stat recalculation (called when equipment changes)
    void recalculateStats();

    // Equipment queries for combat (Phase 9)
    bool hasShieldEquipped() const;
    bool hasWeaponEquipped() const;
    int32_t getWeaponDamage() const;
    int32_t getArmorValue() const;

    // Broadcast equipment change to nearby players
    void broadcastEquipmentChange(UnitDefines::EquipSlot slot);

    // Save tracking
    void markDirty() { m_needsSave = true; }
    bool needsSave() const { return m_needsSave; }

    // Position sync (Task 4.9)
    void teleportTo(float x, float y);              // Forcibly move player
    void teleportTo(int mapId, float x, float y);   // Teleport to different map
    void broadcastTeleport();                       // Send teleport to viewers
    void updateOrientationFromMovement(float destX, float destY);  // Calculate facing

    // Save interval constant (seconds)
    static constexpr float SAVE_INTERVAL = 30.0f;

private:
    void loadStatBonuses();
    void saveStatBonuses();
    // Bound session
    Session& m_session;

    // Character data (from database)
    int32_t m_characterGuid = 0;
    int32_t m_accountId = 0;
    std::string m_characterName;
    int32_t m_classId = 0;
    int32_t m_gender = 0;
    int32_t m_level = 1;
    int32_t m_experience = 0;
    int32_t m_portraitId = 0;
    int32_t m_skinColor = 0;
    int32_t m_hairStyle = 0;
    int32_t m_hairColor = 0;
    int32_t m_gold = 0;
    int32_t m_playedTime = 0;

    // State
    bool m_isMoving = false;
    std::vector<Geo2d::Vector2> m_movePath;
    size_t m_movePathIndex = 0;
    bool m_needsSave = false;
    uint32_t m_selectedTarget = 0;  // Currently selected target GUID
    uint32_t m_gossipTargetGuid = 0;  // Last gossip NPC GUID

    // Session start time for played time tracking
    int64_t m_sessionStartTime = 0;

    // Periodic save timer (Task 4.9)
    float m_saveTimer = 0.0f;

    // Cooldown manager (Task 5.7)
    CooldownManager m_cooldowns;

    // Pending cast (cast time spells)
    PendingCast m_pendingCast;

    // Inventory (Phase 6, Task 6.1)
    Inventory::PlayerInventory m_inventory;

    // Equipment (Phase 6, Task 6.3)
    Equipment::PlayerEquipment m_equipment;

    // Bank (Phase 6, Task 6.6)
    Bank::PlayerBank m_bank;

    // Quest log (Phase 7)
    Quest::PlayerQuestLog m_questLog;

    // Stat bonus storage (Phase 7 level-up)
    std::unordered_map<UnitDefines::Stat, int32_t> m_statBonuses;
    bool m_statBonusesDirty = false;
};
