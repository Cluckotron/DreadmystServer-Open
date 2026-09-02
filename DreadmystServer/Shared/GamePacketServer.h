#pragma once

#include "GamePacketBase.h"
#include "ItemDefines.h"
#include "SpellDefines.h"
#include "../Geo2d.h"
#include <vector>
#include <map>
#include <string>

// ============================================
// Helper functions for ItemId serialization
// ============================================
inline void packItemId(StlBuffer& buf, const ItemDefines::ItemId& item)
{
    buf << item.m_itemId << item.m_affix1 << item.m_affix2
        << item.m_gem1 << item.m_gem2 << item.m_gem3;
}

inline void unpackItemId(StlBuffer& buf, ItemDefines::ItemId& item)
{
    buf >> item.m_itemId >> item.m_affix1 >> item.m_affix2
        >> item.m_gem1 >> item.m_gem2 >> item.m_gem3;
}

inline void unpackItemId(StlBuffer& buf, ItemDefines::ItemDefinition& item)
{
    buf >> item.m_itemId >> item.m_affix1 >> item.m_affix2
        >> item.m_gem1 >> item.m_gem2 >> item.m_gem3;
}

// Helper for map serialization
template<typename K, typename V>
inline void packMap(StlBuffer& buf, const std::map<K, V>& m)
{
    buf << static_cast<uint16_t>(m.size());
    for (const auto& [key, val] : m) {
        buf << key << val;
    }
}

template<typename K, typename V>
inline void unpackMap(StlBuffer& buf, std::map<K, V>& m)
{
    uint16_t count;
    buf >> count;
    m.clear();
    for (uint16_t i = 0; i < count; ++i) {
        K key;
        V val;
        buf >> key >> val;
        m[key] = val;
    }
}

// ============================================
// Server -> Client Packets
// ============================================

struct GP_Mutual_Ping : public GamePacket
{
    uint16_t getOpcode() const override { return Opcode::Mutual_Ping; }
    void pack(StlBuffer& buf) const override { (void)buf; }
    void unpack(StlBuffer& buf) override { (void)buf; }
};

struct GP_Server_Validate : public GamePacket
{
    uint8_t m_result = 0;  // AccountDefines::AuthenticateResult
    int64_t m_serverTime = 0;

    uint16_t getOpcode() const override { return Opcode::Server_Validate; }
    void pack(StlBuffer& buf) const override { buf << m_result << m_serverTime; }
    void unpack(StlBuffer& buf) override { buf >> m_result >> m_serverTime; }
};

struct GP_Server_QueuePosition : public GamePacket
{
    int32_t m_position = 0;

    uint16_t getOpcode() const override { return Opcode::Server_QueuePosition; }
    void pack(StlBuffer& buf) const override { buf << m_position; }
    void unpack(StlBuffer& buf) override { buf >> m_position; }
};

struct GP_Server_CharacterList : public GamePacket
{
    struct Character
    {
        uint32_t guid = 0;
        std::string name;
        uint8_t classId = 0;
        uint8_t gender = 0;
        int32_t level = 0;
        int32_t portrait = 0;
    };
    std::vector<Character> m_characters;

    uint16_t getOpcode() const override { return Opcode::Server_CharacterList; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_characters.size());
        for (const auto& c : m_characters) {
            buf << c.guid << c.name << c.classId << c.gender << c.level << c.portrait;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_characters.clear();
        m_characters.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Character c;
            buf >> c.guid >> c.name >> c.classId >> c.gender >> c.level >> c.portrait;
            m_characters.push_back(c);
        }
    }
};

struct GP_Server_CharaCreateResult : public GamePacket
{
    uint8_t m_result = 0;  // CharacterDefines::NameError

    uint16_t getOpcode() const override { return Opcode::Server_CharaCreateResult; }
    void pack(StlBuffer& buf) const override { buf << m_result; }
    void unpack(StlBuffer& buf) override { buf >> m_result; }
};

struct GP_Server_NewWorld : public GamePacket
{
    int32_t m_mapId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_NewWorld; }
    void pack(StlBuffer& buf) const override { buf << m_mapId; }
    void unpack(StlBuffer& buf) override { buf >> m_mapId; }
};

struct GP_Server_SetController : public GamePacket
{
    uint32_t m_guid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_SetController; }
    void pack(StlBuffer& buf) const override { buf << m_guid; }
    void unpack(StlBuffer& buf) override { buf >> m_guid; }
};

struct GP_Server_ChannelInfo : public GamePacket
{
    int32_t m_myChannel = 0;
    int32_t m_channelSize = 0;
    std::vector<int32_t> m_channels;

    uint16_t getOpcode() const override { return Opcode::Server_ChannelInfo; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_myChannel << m_channelSize;
        buf << static_cast<uint16_t>(m_channels.size());
        for (int32_t ch : m_channels) {
            buf << ch;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_myChannel >> m_channelSize;
        uint16_t count;
        buf >> count;
        m_channels.clear();
        m_channels.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            int32_t ch;
            buf >> ch;
            m_channels.push_back(ch);
        }
    }
};

struct GP_Server_ChannelChangeConfirm : public GamePacket
{
    int32_t m_newChannel = 0;
    bool m_success = false;

    uint16_t getOpcode() const override { return Opcode::Server_ChannelChangeConfirm; }
    void pack(StlBuffer& buf) const override { buf << m_newChannel << m_success; }
    void unpack(StlBuffer& buf) override { buf >> m_newChannel >> m_success; }
};

struct GP_Server_Player : public GamePacket
{
    uint32_t m_guid = 0;
    std::string m_name;
    std::string m_subName;
    uint8_t m_classId = 0;
    uint8_t m_gender = 0;
    int32_t m_portraitId = 0;
    float m_x = 0, m_y = 0;
    float m_orientation = 0;
    std::map<int32_t, int32_t> m_equipment;  // slot -> itemId
    std::map<int32_t, int32_t> m_variables;  // variableId -> value

    uint16_t getOpcode() const override { return Opcode::Server_Player; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_guid << m_name << m_subName << m_classId << m_gender << m_portraitId;
        buf << m_x << m_y << m_orientation;
        packMap(buf, m_equipment);
        packMap(buf, m_variables);
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_guid >> m_name >> m_subName >> m_classId >> m_gender >> m_portraitId;
        buf >> m_x >> m_y >> m_orientation;
        unpackMap(buf, m_equipment);
        unpackMap(buf, m_variables);
    }
};

struct GP_Server_Npc : public GamePacket
{
    uint32_t m_guid = 0;
    int32_t m_entry = 0;
    float m_x = 0, m_y = 0;
    float m_orientation = 0;
    std::map<int32_t, int32_t> m_variables;

    uint16_t getOpcode() const override { return Opcode::Server_Npc; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_guid << m_entry << m_x << m_y << m_orientation;
        packMap(buf, m_variables);
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_guid >> m_entry >> m_x >> m_y >> m_orientation;
        unpackMap(buf, m_variables);
    }
};

struct GP_Server_GameObject : public GamePacket
{
    uint32_t m_guid = 0;
    int32_t m_entry = 0;
    float m_x = 0, m_y = 0;
    std::map<int32_t, int32_t> m_variables;

    uint16_t getOpcode() const override { return Opcode::Server_GameObject; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_guid << m_entry << m_x << m_y;
        packMap(buf, m_variables);
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_guid >> m_entry >> m_x >> m_y;
        unpackMap(buf, m_variables);
    }
};

struct GP_Server_DestroyObject : public GamePacket
{
    uint32_t m_guid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_DestroyObject; }
    void pack(StlBuffer& buf) const override { buf << m_guid; }
    void unpack(StlBuffer& buf) override { buf >> m_guid; }
};

struct GP_Server_SetSubname : public GamePacket
{
    uint32_t m_objectGuid = 0;
    std::string m_name;

    uint16_t getOpcode() const override { return Opcode::Server_SetSubname; }
    void pack(StlBuffer& buf) const override { buf << m_objectGuid << m_name; }
    void unpack(StlBuffer& buf) override { buf >> m_objectGuid >> m_name; }
};

struct GP_Server_UnitSpline : public GamePacket
{
    uint32_t m_guid = 0;
    float m_startX = 0, m_startY = 0;
    std::vector<Geo2d::Vector2> m_spline;
    bool m_slide = false;
    bool m_silent = false;

    uint16_t getOpcode() const override { return Opcode::Server_UnitSpline; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_guid << m_startX << m_startY;
        buf << static_cast<uint16_t>(m_spline.size());
        for (const auto& point : m_spline) {
            buf << point.x << point.y;
        }
        buf << m_slide << m_silent;
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_guid >> m_startX >> m_startY;
        uint16_t count;
        buf >> count;
        m_spline.clear();
        m_spline.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Geo2d::Vector2 point;
            buf >> point.x >> point.y;
            m_spline.push_back(point);
        }
        buf >> m_slide >> m_silent;
    }
};

struct GP_Server_UnitTeleport : public GamePacket
{
    uint32_t m_guid = 0;
    float m_newX = 0, m_newY = 0;
    float m_newOri = 0;  // Orientation

    uint16_t getOpcode() const override { return Opcode::Server_UnitTeleport; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_newX << m_newY << m_newOri; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_newX >> m_newY >> m_newOri; }
};

struct GP_Server_UnitOrientation : public GamePacket
{
    uint32_t m_guid = 0;
    float m_newOri = 0;  // Orientation

    uint16_t getOpcode() const override { return Opcode::Server_UnitOrientation; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_newOri; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_newOri; }
};

struct GP_Server_ObjectVariable : public GamePacket
{
    uint32_t m_guid = 0;
    int32_t m_variableId = 0;
    int32_t m_value = 0;

    uint16_t getOpcode() const override { return Opcode::Server_ObjectVariable; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_variableId << m_value; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_variableId >> m_value; }
};

struct GP_Server_CastStart : public GamePacket
{
    uint32_t m_guid = 0;      // Caster GUID
    int32_t m_spellId = 0;
    int32_t m_timer = 0;      // Cast time in ms

    uint16_t getOpcode() const override { return Opcode::Server_CastStart; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_spellId << m_timer; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_spellId >> m_timer; }
};

struct GP_Server_CastStop : public GamePacket
{
    uint32_t m_guid = 0;      // Caster GUID
    int32_t m_spellId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_CastStop; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_spellId; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_spellId; }
};

struct GP_Server_SpellGo : public GamePacket
{
    uint32_t m_casterGuid = 0;
    int32_t m_spellId = 0;
    float m_groundX = 0.0f;
    float m_groundY = 0.0f;
    // Target guid -> hit result
    std::map<uint32_t, uint8_t> m_targets;

    uint16_t getOpcode() const override { return Opcode::Server_SpellGo; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_casterGuid << m_spellId;
        buf << static_cast<uint16_t>(m_targets.size());
        for (const auto& [guid, result] : m_targets) {
            buf << guid << result;
        }
        buf << m_groundX << m_groundY;
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_casterGuid >> m_spellId;
        uint16_t count;
        buf >> count;
        m_targets.clear();
        for (uint16_t i = 0; i < count; ++i) {
            uint32_t guid;
            uint8_t result;
            buf >> guid >> result;
            m_targets[guid] = result;
        }
        buf >> m_groundX >> m_groundY;
    }
};

struct GP_Server_CombatMsg : public GamePacket
{
    uint32_t m_casterGuid = 0;
    uint32_t m_targetGuid = 0;
    int32_t m_spellId = 0;
    int32_t m_amount = 0;
    uint8_t m_spellEffect = 0;   // SpellDefines::Effects
    uint8_t m_auraEffect = 0;    // SpellDefines::AuraType
    uint8_t m_spellResult = 0;   // SpellDefines::HitResult
    bool m_periodic = false;
    bool m_positive = false;

    uint16_t getOpcode() const override { return Opcode::Server_CombatMsg; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_casterGuid << m_targetGuid << m_spellId << m_amount;
        buf << m_spellEffect << m_auraEffect << m_spellResult << m_periodic << m_positive;
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_casterGuid >> m_targetGuid >> m_spellId >> m_amount;
        buf >> m_spellEffect >> m_auraEffect >> m_spellResult >> m_periodic >> m_positive;
    }
};

struct GP_Server_UnitAuras : public GamePacket
{
    struct AuraInfo
    {
        int32_t spellId = 0;
        uint32_t casterGuid = 0;
        int32_t maxDuration = 0;
        int32_t elapsedTime = 0;
        int32_t stacks = 1;
        bool positive = true;
        // Client-side computed
        int64_t startDate = 0;
        int64_t endDate = 0;
        // Alias for client code
        int32_t stackCount() const { return stacks; }
    };

    uint32_t m_unitGuid = 0;
    std::vector<AuraInfo> m_buffs;
    std::vector<AuraInfo> m_debuffs;

    uint16_t getOpcode() const override { return Opcode::Server_UnitAuras; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_unitGuid;

        // Buffs
        buf << static_cast<uint16_t>(m_buffs.size());
        for (const auto& aura : m_buffs) {
            buf << aura.spellId << aura.casterGuid << aura.maxDuration
                << aura.elapsedTime << aura.stacks << aura.positive;
        }

        // Debuffs
        buf << static_cast<uint16_t>(m_debuffs.size());
        for (const auto& aura : m_debuffs) {
            buf << aura.spellId << aura.casterGuid << aura.maxDuration
                << aura.elapsedTime << aura.stacks << aura.positive;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_unitGuid;

        // Buffs
        uint16_t buffCount;
        buf >> buffCount;
        m_buffs.clear();
        m_buffs.reserve(buffCount);
        for (uint16_t i = 0; i < buffCount; ++i) {
            AuraInfo aura;
            buf >> aura.spellId >> aura.casterGuid >> aura.maxDuration
                >> aura.elapsedTime >> aura.stacks >> aura.positive;
            m_buffs.push_back(aura);
        }

        // Debuffs
        uint16_t debuffCount;
        buf >> debuffCount;
        m_debuffs.clear();
        m_debuffs.reserve(debuffCount);
        for (uint16_t i = 0; i < debuffCount; ++i) {
            AuraInfo aura;
            buf >> aura.spellId >> aura.casterGuid >> aura.maxDuration
                >> aura.elapsedTime >> aura.stacks >> aura.positive;
            m_debuffs.push_back(aura);
        }
    }
};

struct GP_Server_Cooldown : public GamePacket
{
    int32_t m_id = 0;              // Spell ID
    int32_t m_totalDuration = 0;   // Remaining cooldown in ms

    uint16_t getOpcode() const override { return Opcode::Server_Cooldown; }
    void pack(StlBuffer& buf) const override { buf << m_id << m_totalDuration; }
    void unpack(StlBuffer& buf) override { buf >> m_id >> m_totalDuration; }
};

struct GP_Server_AggroMob : public GamePacket
{
    uint32_t m_guid = 0;
    bool m_aggro = false;

    uint16_t getOpcode() const override { return Opcode::Server_AggroMob; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_aggro; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_aggro; }
};

struct GP_Server_Inventory : public GamePacket
{
    struct Slot
    {
        int32_t slot = 0;
        ItemDefines::ItemDefinition itemId;
        int32_t stackCount = 0;
    };
    std::vector<Slot> m_slots;
    int32_t m_gold = 0;

    uint16_t getOpcode() const override { return Opcode::Server_Inventory; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_gold;
        buf << static_cast<uint16_t>(m_slots.size());
        for (const auto& slot : m_slots) {
            buf << slot.slot;
            packItemId(buf, slot.itemId);
            buf << slot.stackCount;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_gold;
        uint16_t count;
        buf >> count;
        m_slots.clear();
        m_slots.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Slot slot;
            buf >> slot.slot;
            unpackItemId(buf, slot.itemId);
            buf >> slot.stackCount;
            m_slots.push_back(slot);
        }
    }
};

struct GP_Server_Bank : public GamePacket
{
    struct Slot
    {
        int32_t slot = 0;
        ItemDefines::ItemDefinition itemId;
        int32_t stackCount = 0;
    };
    std::vector<Slot> m_slots;

    uint16_t getOpcode() const override { return Opcode::Server_Bank; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_slots.size());
        for (const auto& slot : m_slots) {
            buf << slot.slot;
            packItemId(buf, slot.itemId);
            buf << slot.stackCount;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_slots.clear();
        m_slots.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Slot slot;
            buf >> slot.slot;
            unpackItemId(buf, slot.itemId);
            buf >> slot.stackCount;
            m_slots.push_back(slot);
        }
    }
};

struct GP_Server_OpenBank : public GamePacket
{
    uint32_t m_bankerGuid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_OpenBank; }
    void pack(StlBuffer& buf) const override { buf << m_bankerGuid; }
    void unpack(StlBuffer& buf) override { buf >> m_bankerGuid; }
};

struct GP_Server_EquipItem : public GamePacket
{
    uint32_t m_guid = 0;        // Player whose equipment changed
    int32_t m_slot = 0;         // Equipment slot (EquipSlot enum)
    ItemDefines::ItemDefinition m_itemId;
    bool m_silent = false;

    uint16_t getOpcode() const override { return Opcode::Server_EquipItem; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_slot; packItemId(buf, m_itemId); buf << m_silent; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_slot; unpackItemId(buf, m_itemId); buf >> m_silent; }
};

struct GP_Server_NotifyItemAdd : public GamePacket
{
    ItemDefines::ItemDefinition m_itemId;
    int32_t m_amount = 0;
    std::string m_looterName;

    uint16_t getOpcode() const override { return Opcode::Server_NotifyItemAdd; }
    void pack(StlBuffer& buf) const override { packItemId(buf, static_cast<ItemDefines::ItemId>(m_itemId)); buf << m_amount << m_looterName; }
    void unpack(StlBuffer& buf) override { unpackItemId(buf, m_itemId); buf >> m_amount >> m_looterName; }
};

struct GP_Server_OpenLootWindow : public GamePacket
{
    uint32_t m_objGuid = 0;
    std::vector<std::pair<ItemDefines::ItemDefinition, int32_t>> m_items;  // (item, count) pairs
    int32_t m_money = 0;

    uint16_t getOpcode() const override { return Opcode::Server_OpenLootWindow; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_objGuid << m_money;
        buf << static_cast<uint16_t>(m_items.size());
        for (const auto& item : m_items) {
            packItemId(buf, item.first);
            buf << item.second;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_objGuid >> m_money;
        uint16_t count;
        buf >> count;
        m_items.clear();
        m_items.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            ItemDefines::ItemDefinition itemId;
            int32_t itemCount;
            unpackItemId(buf, itemId);
            buf >> itemCount;
            m_items.push_back({itemId, itemCount});
        }
    }
};

struct GP_Server_OnObjectWasLooted : public GamePacket
{
    uint32_t m_guid = 0;
    uint32_t m_looterGuid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_OnObjectWasLooted; }
    void pack(StlBuffer& buf) const override { buf << m_guid << m_looterGuid; }
    void unpack(StlBuffer& buf) override { buf >> m_guid >> m_looterGuid; }
};

struct GP_Server_UpdateVendorStock : public GamePacket
{
    uint32_t m_vendorGuid = 0;
    ItemDefines::ItemDefinition m_itemId;
    int32_t m_amount = 0;

    uint16_t getOpcode() const override { return Opcode::Server_UpdateVendorStock; }
    void pack(StlBuffer& buf) const override { buf << m_vendorGuid; packItemId(buf, m_itemId); buf << m_amount; }
    void unpack(StlBuffer& buf) override { buf >> m_vendorGuid; unpackItemId(buf, m_itemId); buf >> m_amount; }
};

struct GP_Server_RepairCost : public GamePacket
{
    bool m_finished = false;
    int32_t m_amount = 0;

    uint16_t getOpcode() const override { return Opcode::Server_RepairCost; }
    void pack(StlBuffer& buf) const override { buf << m_finished << m_amount; }
    void unpack(StlBuffer& buf) override { buf >> m_finished >> m_amount; }
};

struct GP_Server_SocketResult : public GamePacket
{
    bool m_success = false;

    uint16_t getOpcode() const override { return Opcode::Server_SocketResult; }
    void pack(StlBuffer& buf) const override { buf << m_success; }
    void unpack(StlBuffer& buf) override { buf >> m_success; }
};

struct GP_Server_EmpowerResult : public GamePacket
{
    bool m_success = false;

    uint16_t getOpcode() const override { return Opcode::Server_EmpowerResult; }
    void pack(StlBuffer& buf) const override { buf << m_success; }
    void unpack(StlBuffer& buf) override { buf >> m_success; }
};

struct GP_Server_Spellbook : public GamePacket
{
    // SpellSlot contains spell data with level and base points
    struct SpellSlot
    {
        int32_t spellId = 0;
        uint8_t level = 1;
        std::vector<std::pair<int16_t, int16_t>> bpoints;  // Base points (effect data pairs)
    };

    std::vector<SpellSlot> m_slots;

    uint16_t getOpcode() const override { return Opcode::Server_Spellbook; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_slots.size());
        for (const auto& slot : m_slots) {
            buf << slot.spellId << slot.level;
            buf << static_cast<uint16_t>(slot.bpoints.size());
            for (const auto& bp : slot.bpoints) {
                buf << bp.first << bp.second;
            }
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_slots.clear();
        m_slots.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            SpellSlot slot;
            buf >> slot.spellId >> slot.level;
            uint16_t bpCount;
            buf >> bpCount;
            slot.bpoints.reserve(bpCount);
            for (uint16_t j = 0; j < bpCount; ++j) {
                int16_t first, second;
                buf >> first >> second;
                slot.bpoints.emplace_back(first, second);
            }
            m_slots.push_back(slot);
        }
    }
};

struct GP_Server_Spellbook_Update : public GamePacket
{
    int32_t m_spellId = 0;
    uint8_t m_level = 1;
    std::vector<std::pair<int16_t, int16_t>> m_bpoints;  // Base points for spell effects

    uint16_t getOpcode() const override { return Opcode::Server_Spellbook_Update; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_spellId << m_level;
        buf << static_cast<uint16_t>(m_bpoints.size());
        for (const auto& bp : m_bpoints) {
            buf << bp.first << bp.second;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_spellId >> m_level;
        uint16_t bpCount;
        buf >> bpCount;
        m_bpoints.clear();
        m_bpoints.reserve(bpCount);
        for (uint16_t i = 0; i < bpCount; ++i) {
            int16_t first, second;
            buf >> first >> second;
            m_bpoints.emplace_back(first, second);
        }
    }
};

struct GP_Server_LearnedSpell : public GamePacket
{
    int32_t m_spellId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_LearnedSpell; }
    void pack(StlBuffer& buf) const override { buf << m_spellId; }
    void unpack(StlBuffer& buf) override { buf >> m_spellId; }
};

struct GP_Server_ExpNotify : public GamePacket
{
    int32_t m_amount = 0;
    int32_t m_newLevel = 0;

    uint16_t getOpcode() const override { return Opcode::Server_ExpNotify; }
    void pack(StlBuffer& buf) const override { buf << m_amount << m_newLevel; }
    void unpack(StlBuffer& buf) override { buf >> m_amount >> m_newLevel; }
};

struct GP_Server_LvlResponse : public GamePacket
{
    int32_t m_newLevel = 0;
    bool m_bool = true;

    uint16_t getOpcode() const override { return Opcode::Server_LvlResponse; }
    void pack(StlBuffer& buf) const override { buf << m_newLevel << m_bool; }
    void unpack(StlBuffer& buf) override { buf >> m_newLevel >> m_bool; }
};

struct GP_Server_SpentGold : public GamePacket
{
    int32_t m_amount = 0;

    uint16_t getOpcode() const override { return Opcode::Server_SpentGold; }
    void pack(StlBuffer& buf) const override { buf << m_amount; }
    void unpack(StlBuffer& buf) override { buf >> m_amount; }
};

struct GP_Server_QuestList : public GamePacket
{
    struct Quest
    {
        int32_t id = 0;
        bool done = false;
        std::map<int32_t, int32_t> tallyItems;
        std::map<int32_t, int32_t> tallyNpcs;
        std::map<int32_t, int32_t> tallyGameObjects;
        std::map<int32_t, int32_t> tallySpells;
    };
    std::vector<Quest> m_quests;

    uint16_t getOpcode() const override { return Opcode::Server_QuestList; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_quests.size());
        for (const auto& q : m_quests) {
            buf << q.id << q.done;
            packMap(buf, q.tallyItems);
            packMap(buf, q.tallyNpcs);
            packMap(buf, q.tallyGameObjects);
            packMap(buf, q.tallySpells);
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_quests.clear();
        m_quests.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Quest q;
            buf >> q.id >> q.done;
            unpackMap(buf, q.tallyItems);
            unpackMap(buf, q.tallyNpcs);
            unpackMap(buf, q.tallyGameObjects);
            unpackMap(buf, q.tallySpells);
            m_quests.push_back(q);
        }
    }
};

struct GP_Server_AcceptedQuest : public GamePacket
{
    int32_t m_questId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_AcceptedQuest; }
    void pack(StlBuffer& buf) const override { buf << m_questId; }
    void unpack(StlBuffer& buf) override { buf >> m_questId; }
};

struct GP_Server_QuestTally : public GamePacket
{
    int32_t m_questId = 0;
    uint8_t m_type = 0;  // QuestDefines::TallyType
    int32_t m_entry = 0;
    int32_t m_tally = 0;

    uint16_t getOpcode() const override { return Opcode::Server_QuestTally; }
    void pack(StlBuffer& buf) const override { buf << m_questId << m_type << m_entry << m_tally; }
    void unpack(StlBuffer& buf) override { buf >> m_questId >> m_type >> m_entry >> m_tally; }
};

struct GP_Server_QuestComplete : public GamePacket
{
    int32_t m_questId = 0;
    bool m_done = false;

    uint16_t getOpcode() const override { return Opcode::Server_QuestComplete; }
    void pack(StlBuffer& buf) const override { buf << m_questId << m_done; }
    void unpack(StlBuffer& buf) override { buf >> m_questId >> m_done; }
};

struct GP_Server_RewardedQuest : public GamePacket
{
    int32_t m_questId = 0;
    int32_t m_rewardChoice = 0;

    uint16_t getOpcode() const override { return Opcode::Server_RewardedQuest; }
    void pack(StlBuffer& buf) const override { buf << m_questId << m_rewardChoice; }
    void unpack(StlBuffer& buf) override { buf >> m_questId >> m_rewardChoice; }
};

struct GP_Server_AbandonQuest : public GamePacket
{
    int32_t m_questId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_AbandonQuest; }
    void pack(StlBuffer& buf) const override { buf << m_questId; }
    void unpack(StlBuffer& buf) override { buf >> m_questId; }
};

struct GP_Server_AvailableWorldQuests : public GamePacket
{
    std::vector<int32_t> m_list;

    uint16_t getOpcode() const override { return Opcode::Server_AvailableWorldQuests; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_list.size());
        for (int32_t id : m_list) {
            buf << id;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_list.clear();
        m_list.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            int32_t id;
            buf >> id;
            m_list.push_back(id);
        }
    }
};

struct GP_Server_ChatMsg : public GamePacket
{
    uint8_t m_channelId = 0;
    uint32_t m_fromGuid = 0;
    std::string m_fromName;
    std::string m_text;
    ItemDefines::ItemId m_itemId;  // Optional linked item

    uint16_t getOpcode() const override { return Opcode::Server_ChatMsg; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_channelId << m_fromGuid << m_fromName << m_text;
        packItemId(buf, m_itemId);
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_channelId >> m_fromGuid >> m_fromName >> m_text;
        unpackItemId(buf, m_itemId);
    }
};

struct GP_Server_ChatError : public GamePacket
{
    uint8_t m_code = 0;

    uint16_t getOpcode() const override { return Opcode::Server_ChatError; }
    void pack(StlBuffer& buf) const override { buf << m_code; }
    void unpack(StlBuffer& buf) override { buf >> m_code; }
};

struct GP_Server_GossipMenu : public GamePacket
{
    uint32_t m_targetGuid = 0;
    int32_t m_gossipEntry = 0;  // Gossip text entry ID

    // Gossip option entry IDs (from gossip_option)
    std::vector<int32_t> m_gossipOptions;

    // Vendor items
    struct VendorSlot
    {
        ItemDefines::ItemDefinition m_itemId;
        int32_t m_cost = 0;
        int32_t m_supply = -1;  // -1 = unlimited supply
    };
    std::vector<VendorSlot> m_vendorItems;

    // Quest offers (quest IDs)
    std::vector<int32_t> m_questOffers;

    // Quest completion (quest IDs ready to turn in)
    std::vector<int32_t> m_questCompletes;

    uint16_t getOpcode() const override { return Opcode::Server_GossipMenu; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_targetGuid << m_gossipEntry;

        // Gossip options
        buf << static_cast<uint16_t>(m_gossipOptions.size());
        for (int32_t entry : m_gossipOptions) {
            buf << entry;
        }

        // Vendor items
        buf << static_cast<uint16_t>(m_vendorItems.size());
        for (const auto& item : m_vendorItems) {
            packItemId(buf, item.m_itemId);
            buf << item.m_cost << item.m_supply;
        }

        // Quest offers
        buf << static_cast<uint16_t>(m_questOffers.size());
        for (int32_t questId : m_questOffers) {
            buf << questId;
        }

        // Quest completes
        buf << static_cast<uint16_t>(m_questCompletes.size());
        for (int32_t questId : m_questCompletes) {
            buf << questId;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_targetGuid >> m_gossipEntry;

        // Gossip options
        uint16_t gossipCount;
        buf >> gossipCount;
        m_gossipOptions.clear();
        m_gossipOptions.reserve(gossipCount);
        for (uint16_t i = 0; i < gossipCount; ++i) {
            int32_t entry = 0;
            buf >> entry;
            m_gossipOptions.push_back(entry);
        }

        // Vendor items
        uint16_t vendorCount;
        buf >> vendorCount;
        m_vendorItems.clear();
        m_vendorItems.reserve(vendorCount);
        for (uint16_t i = 0; i < vendorCount; ++i) {
            VendorSlot item;
            unpackItemId(buf, item.m_itemId);
            buf >> item.m_cost >> item.m_supply;
            m_vendorItems.push_back(item);
        }

        // Quest offers
        uint16_t offerCount;
        buf >> offerCount;
        m_questOffers.clear();
        m_questOffers.reserve(offerCount);
        for (uint16_t i = 0; i < offerCount; ++i) {
            int32_t questId = 0;
            buf >> questId;
            m_questOffers.push_back(questId);
        }

        // Quest completes
        uint16_t completeCount;
        buf >> completeCount;
        m_questCompletes.clear();
        m_questCompletes.reserve(completeCount);
        for (uint16_t i = 0; i < completeCount; ++i) {
            int32_t questId = 0;
            buf >> questId;
            m_questCompletes.push_back(questId);
        }
    }
};

struct GP_Server_GuildRoster : public GamePacket
{
    struct Member
    {
        uint32_t guid = 0;
        std::string name;
        uint8_t rank = 0;
        int32_t level = 0;
        uint8_t classId = 0;
        bool online = false;
    };
    int32_t m_guildId = 0;
    std::string m_guildName;
    std::string m_motd;
    std::vector<Member> m_members;

    uint16_t getOpcode() const override { return Opcode::Server_GuildRoster; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_guildId << m_guildName << m_motd;
        buf << static_cast<uint16_t>(m_members.size());
        for (const auto& m : m_members) {
            buf << m.guid << m.name << m.rank << m.level << m.classId << m.online;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_guildId >> m_guildName >> m_motd;
        uint16_t count;
        buf >> count;
        m_members.clear();
        m_members.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            Member m;
            buf >> m.guid >> m.name >> m.rank >> m.level >> m.classId >> m.online;
            m_members.push_back(m);
        }
    }
};

struct GP_Server_GuildInvite : public GamePacket
{
    int32_t m_guildId = 0;
    std::string m_guildName;
    std::string m_inviterName;

    uint16_t getOpcode() const override { return Opcode::Server_GuildInvite; }
    void pack(StlBuffer& buf) const override { buf << m_guildId << m_guildName << m_inviterName; }
    void unpack(StlBuffer& buf) override { buf >> m_guildId >> m_guildName >> m_inviterName; }
};

struct GP_Server_GuildAddMember : public GamePacket
{
    uint32_t m_memberGuid = 0;
    std::string m_playerName;

    uint16_t getOpcode() const override { return Opcode::Server_GuildAddMember; }
    void pack(StlBuffer& buf) const override { buf << m_memberGuid << m_playerName; }
    void unpack(StlBuffer& buf) override { buf >> m_memberGuid >> m_playerName; }
};

struct GP_Server_GuildRemoveMember : public GamePacket
{
    uint32_t m_memberGuid = 0;
    std::string m_playerName;

    uint16_t getOpcode() const override { return Opcode::Server_GuildRemoveMember; }
    void pack(StlBuffer& buf) const override { buf << m_memberGuid << m_playerName; }
    void unpack(StlBuffer& buf) override { buf >> m_memberGuid >> m_playerName; }
};

struct GP_Server_GuildOnlineStatus : public GamePacket
{
    std::string m_playerName;
    bool m_online = false;

    uint16_t getOpcode() const override { return Opcode::Server_GuildOnlineStatus; }
    void pack(StlBuffer& buf) const override { buf << m_playerName << m_online; }
    void unpack(StlBuffer& buf) override { buf >> m_playerName >> m_online; }
};

struct GP_Server_GuildNotifyRoleChange : public GamePacket
{
    std::string m_playerName;
    uint8_t m_role = 0;

    uint16_t getOpcode() const override { return Opcode::Server_GuildNotifyRoleChange; }
    void pack(StlBuffer& buf) const override { buf << m_playerName << m_role; }
    void unpack(StlBuffer& buf) override { buf >> m_playerName >> m_role; }
};

struct GP_Server_PartyList : public GamePacket
{
    std::vector<int> m_members;
    uint32_t m_leaderGuid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_PartyList; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_leaderGuid;
        buf << static_cast<uint16_t>(m_members.size());
        for (int guid : m_members) {
            buf << guid;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_leaderGuid;
        uint16_t count;
        buf >> count;
        m_members.clear();
        m_members.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            int guid = 0;
            buf >> guid;
            m_members.push_back(guid);
        }
    }
};

struct GP_Server_OfferParty : public GamePacket
{
    uint32_t m_inviterGuid = 0;
    std::string m_inviterName;

    uint16_t getOpcode() const override { return Opcode::Server_OfferParty; }
    void pack(StlBuffer& buf) const override { buf << m_inviterGuid << m_inviterName; }
    void unpack(StlBuffer& buf) override { buf >> m_inviterGuid >> m_inviterName; }
};

struct GP_Server_TradeUpdate : public GamePacket
{
    using TradeItemList = std::vector<std::pair<ItemDefines::ItemDefinition, int32_t>>;
    std::map<int32_t, TradeItemList> m_myItems;
    std::map<int32_t, TradeItemList> m_theirItems;
    int32_t m_myMoney = 0;
    int32_t m_hisMoney = 0;
    bool m_myReady = false;
    bool m_theirReady = false;
    uint32_t m_partnerGuid = 0;

    uint16_t getOpcode() const override { return Opcode::Server_TradeUpdate; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_partnerGuid << m_myMoney << m_hisMoney << m_myReady << m_theirReady;

        buf << static_cast<uint16_t>(m_myItems.size());
        for (const auto& [itemGuid, items] : m_myItems) {
            buf << itemGuid;
            buf << static_cast<uint16_t>(items.size());
            for (const auto& [itemDef, stackCount] : items) {
                packItemId(buf, itemDef);
                buf << stackCount;
            }
        }

        buf << static_cast<uint16_t>(m_theirItems.size());
        for (const auto& [itemGuid, items] : m_theirItems) {
            buf << itemGuid;
            buf << static_cast<uint16_t>(items.size());
            for (const auto& [itemDef, stackCount] : items) {
                packItemId(buf, itemDef);
                buf << stackCount;
            }
        }
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_partnerGuid >> m_myMoney >> m_hisMoney >> m_myReady >> m_theirReady;

        uint16_t myCount;
        buf >> myCount;
        m_myItems.clear();
        for (uint16_t i = 0; i < myCount; ++i) {
            int32_t itemGuid = 0;
            uint16_t itemCount = 0;
            buf >> itemGuid >> itemCount;

            TradeItemList items;
            items.reserve(itemCount);
            for (uint16_t j = 0; j < itemCount; ++j) {
                ItemDefines::ItemDefinition itemDef;
                int32_t stackCount = 0;
                unpackItemId(buf, itemDef);
                buf >> stackCount;
                items.push_back({itemDef, stackCount});
            }
            m_myItems[itemGuid] = std::move(items);
        }

        uint16_t theirCount;
        buf >> theirCount;
        m_theirItems.clear();
        for (uint16_t i = 0; i < theirCount; ++i) {
            int32_t itemGuid = 0;
            uint16_t itemCount = 0;
            buf >> itemGuid >> itemCount;

            TradeItemList items;
            items.reserve(itemCount);
            for (uint16_t j = 0; j < itemCount; ++j) {
                ItemDefines::ItemDefinition itemDef;
                int32_t stackCount = 0;
                unpackItemId(buf, itemDef);
                buf >> stackCount;
                items.push_back({itemDef, stackCount});
            }
            m_theirItems[itemGuid] = std::move(items);
        }
    }
};

struct GP_Server_TradeCanceled : public GamePacket
{
    uint16_t getOpcode() const override { return Opcode::Server_TradeCanceled; }
    void pack(StlBuffer& buf) const override { (void)buf; }
    void unpack(StlBuffer& buf) override { (void)buf; }
};

struct GP_Server_QueryWaypointsResponse : public GamePacket
{
    std::vector<int32_t> m_guids;

    uint16_t getOpcode() const override { return Opcode::Server_QueryWaypointsResponse; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_guids.size());
        for (int32_t guid : m_guids) {
            buf << guid;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_guids.clear();
        m_guids.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            int32_t guid = 0;
            buf >> guid;
            m_guids.push_back(guid);
        }
    }
};

struct GP_Server_DiscoverWaypointNotify : public GamePacket
{
    int32_t m_waypointId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_DiscoverWaypointNotify; }
    void pack(StlBuffer& buf) const override { buf << m_waypointId; }
    void unpack(StlBuffer& buf) override { buf >> m_waypointId; }
};

struct GP_Server_ArenaQueued : public GamePacket
{
    bool m_joined = false;

    uint16_t getOpcode() const override { return Opcode::Server_ArenaQueued; }
    void pack(StlBuffer& buf) const override { buf << m_joined; }
    void unpack(StlBuffer& buf) override { buf >> m_joined; }
};

struct GP_Server_ArenaReady : public GamePacket
{
    int32_t m_arenaId = 0;

    uint16_t getOpcode() const override { return Opcode::Server_ArenaReady; }
    void pack(StlBuffer& buf) const override { buf << m_arenaId; }
    void unpack(StlBuffer& buf) override { buf >> m_arenaId; }
};

struct GP_Server_ArenaStatus : public GamePacket
{
    bool m_hasBegun = false;

    uint16_t getOpcode() const override { return Opcode::Server_ArenaStatus; }
    void pack(StlBuffer& buf) const override { buf << m_hasBegun; }
    void unpack(StlBuffer& buf) override { buf >> m_hasBegun; }
};

struct GP_Server_ArenaOutcome : public GamePacket
{
    bool m_won = false;

    uint16_t getOpcode() const override { return Opcode::Server_ArenaOutcome; }
    void pack(StlBuffer& buf) const override { buf << m_won; }
    void unpack(StlBuffer& buf) override { buf >> m_won; }
};

struct GP_Server_OfferDuel : public GamePacket
{
    uint32_t m_challengerGuid = 0;
    std::string m_challengerName;

    uint16_t getOpcode() const override { return Opcode::Server_OfferDuel; }
    void pack(StlBuffer& buf) const override { buf << m_challengerGuid << m_challengerName; }
    void unpack(StlBuffer& buf) override { buf >> m_challengerGuid >> m_challengerName; }
};

struct GP_Server_PkNotify : public GamePacket
{
    std::string m_playerName;

    uint16_t getOpcode() const override { return Opcode::Server_PkNotify; }
    void pack(StlBuffer& buf) const override { buf << m_playerName; }
    void unpack(StlBuffer& buf) override { buf >> m_playerName; }
};

struct GP_Server_WorldError : public GamePacket
{
    uint8_t m_code = 0;
    std::string m_message;

    uint16_t getOpcode() const override { return Opcode::Server_WorldError; }
    void pack(StlBuffer& buf) const override { buf << m_code << m_message; }
    void unpack(StlBuffer& buf) override { buf >> m_code >> m_message; }
};

struct GP_Server_InspectReveal : public GamePacket
{
    uint32_t m_targetGuid = 0;
    uint8_t m_classId = 0;
    std::string m_guildName;
    std::map<int32_t, int32_t> m_equipment;
    std::map<int32_t, int32_t> m_variables;

    uint16_t getOpcode() const override { return Opcode::Server_InspectReveal; }

    void pack(StlBuffer& buf) const override
    {
        buf << m_targetGuid << m_classId << m_guildName;
        packMap(buf, m_equipment);
        packMap(buf, m_variables);
    }

    void unpack(StlBuffer& buf) override
    {
        buf >> m_targetGuid >> m_classId >> m_guildName;
        unpackMap(buf, m_equipment);
        unpackMap(buf, m_variables);
    }
};

struct GP_Server_PromptRespec : public GamePacket
{
    int32_t m_cost = 0;

    uint16_t getOpcode() const override { return Opcode::Server_PromptRespec; }
    void pack(StlBuffer& buf) const override { buf << m_cost; }
    void unpack(StlBuffer& buf) override { buf >> m_cost; }
};

struct GP_Server_RespawnResponse : public GamePacket
{
    bool m_success = false;

    uint16_t getOpcode() const override { return Opcode::Server_RespawnResponse; }
    void pack(StlBuffer& buf) const override { buf << m_success; }
    void unpack(StlBuffer& buf) override { buf >> m_success; }
};

struct GP_Server_UnlockGameObj : public GamePacket
{
    int32_t m_objEntry = 0;

    uint16_t getOpcode() const override { return Opcode::Server_UnlockGameObj; }
    void pack(StlBuffer& buf) const override { buf << m_objEntry; }
    void unpack(StlBuffer& buf) override { buf >> m_objEntry; }
};

struct GP_Server_MarkNpcsOnMap : public GamePacket
{
    std::set<int> m_npcs;

    uint16_t getOpcode() const override { return Opcode::Server_MarkNpcsOnMap; }

    void pack(StlBuffer& buf) const override
    {
        buf << static_cast<uint16_t>(m_npcs.size());
        for (int guid : m_npcs) {
            buf << guid;
        }
    }

    void unpack(StlBuffer& buf) override
    {
        uint16_t count;
        buf >> count;
        m_npcs.clear();
        for (uint16_t i = 0; i < count; ++i) {
            int guid = 0;
            buf >> guid;
            m_npcs.insert(guid);
        }
    }
};
