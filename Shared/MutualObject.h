#pragma once

#include <stdint.h>
#include <map>
#include <string>
#include "ObjDefines.h"
#include "UnitDefines.h"

// Base class for shared object state between client and server
class MutualObject
{
public:
    // Object type for runtime type identification
    enum class Type : uint8_t
    {
        None = 0,
        Player = 1,
        Npc = 2,
        GameObject = 3
    };

    MutualObject() = default;
    explicit MutualObject(uint32_t guid) : m_guid(guid) {}
    virtual ~MutualObject() = default;

    // Type identification
    virtual Type getType() const { return m_type; }
    void initType(Type type) { m_type = type; }

    // Entry ID (NPC/item template ID) - override in derived classes
    virtual int getEntry() const { return 0; }

    uint32_t getGuid() const { return m_guid; }
    void setGuid(uint32_t guid) { m_guid = guid; }

    // Name and subname
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; notifyNameChanged(); }
    const std::string& getSubName() const { return m_subName; }
    void setSubName(const std::string& subName) { m_subName = subName; notifySubnameChanged(); }

    // Variable system for dynamic state
    void setVariable(int variableId, int value)
    {
        m_variables[variableId] = value;
        notifyVariableChange(static_cast<ObjDefines::Variable>(variableId), value);
    }
    void setVariable(ObjDefines::Variable var, int value)
    {
        setVariable(static_cast<int>(var), value);
    }
    int getVariable(int variableId) const
    {
        auto it = m_variables.find(variableId);
        return it != m_variables.end() ? it->second : 0;
    }
    int getVariable(ObjDefines::Variable var) const
    {
        return getVariable(static_cast<int>(var));
    }

    const std::map<int, int>& getVariables() const { return m_variables; }

    // Convenience accessors used throughout the released client.
    int getHealth() const { return getVariable(ObjDefines::Variable::Health); }
    int getMaxHealth() const { return getVariable(ObjDefines::Variable::MaxHealth); }
    float getHealthPct() const
    {
        const int maxHealth = getMaxHealth();
        return maxHealth > 0 ? static_cast<float>(getHealth()) / static_cast<float>(maxHealth) : 0.0f;
    }
    int getLevel() const { return getVariable(ObjDefines::Variable::Level); }
    int getStat(UnitDefines::Stat stat) const
    {
        return getVariable(ObjDefines::StatsStart + static_cast<int>(stat));
    }
    bool isAlive() const { return getHealth() > 0; }

    // Notification callbacks - override in client classes
    virtual void notifyVariableChange(const ObjDefines::Variable /*var*/, const int /*value*/) {}
    virtual void notifyNameChanged() {}
    virtual void notifySubnameChanged() {}

    // Convenience constants for type checking (allows MutualObject::Npc instead of MutualObject::Type::Npc)
    static constexpr Type Player = Type::Player;
    static constexpr Type Npc = Type::Npc;
    static constexpr Type GameObject = Type::GameObject;

protected:
    uint32_t m_guid = 0;
    Type m_type = Type::None;
    std::string m_name;
    std::string m_subName;
    std::map<int, int> m_variables;
};
