#pragma once

#include "MutualObject.h"
#include "ObjDefines.h"
#include "../Combat/AuraSystem.h"

#include <string>
#include <functional>
#include <set>
#include <cmath>

class Map;

// Server-side entity with position and world presence
class Entity : public MutualObject
{
public:
    Entity();
    virtual ~Entity() = default;

    // Position
    int getMapId() const { return m_mapId; }
    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getOrientation() const { return m_orientation; }

    void setPosition(int mapId, float x, float y);
    void setPosition(float x, float y);
    void setOrientation(float orientation)
    {
        // Keep the authoritative facing angle in the same wrapped range the
        // released client renderer expects. std::atan2() itself returns
        // [-pi, +pi], so north/east-facing movement would otherwise be sent as
        // a negative angle and could select the wrong sprite direction.
        if (!std::isfinite(orientation))
            return;

        constexpr float TWO_PI = 6.28318530718f;
        orientation = std::fmod(orientation, TWO_PI);
        if (orientation < 0.0f)
            orientation += TWO_PI;

        m_orientation = orientation;
    }

    // Map reference
    Map* getMap() const { return m_map; }
    void setMap(Map* map) { m_map = map; }

    // Variable system with change callback
    using VariableCallback = std::function<void(Entity*, ObjDefines::Variable, int32_t, int32_t)>;

    int32_t getVariable(ObjDefines::Variable var) const;
    void setVariable(ObjDefines::Variable var, int32_t value);
    void modifyVariable(ObjDefines::Variable var, int32_t delta);
    void setVariableCallback(VariableCallback callback) { m_variableCallback = callback; }

    // Spawned state
    bool isSpawned() const { return m_spawned; }
    void setSpawned(bool spawned) { m_spawned = spawned; }

    // Update (called each tick)
    virtual void update(float deltaTime) = 0;

    // Name (for logging/display)
    virtual const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    // Distance calculations
    float distanceTo(const Entity* other) const;
    float distanceTo(float x, float y) const;
    bool isInRange(const Entity* other, float range) const;
    bool isInRange(float x, float y, float range) const;

    // Visibility - entities that can see this entity
    void addVisibleTo(Entity* viewer) { m_visibleTo.insert(viewer); }
    void removeVisibleTo(Entity* viewer) { m_visibleTo.erase(viewer); }
    const std::set<Entity*>& getVisibleTo() const { return m_visibleTo; }
    void clearVisibleTo() { m_visibleTo.clear(); }

    // Entities this entity can see
    void addCanSee(Entity* target) { m_canSee.insert(target); }
    void removeCanSee(Entity* target) { m_canSee.erase(target); }
    const std::set<Entity*>& getCanSee() const { return m_canSee; }
    void clearCanSee() { m_canSee.clear(); }

    // Aura system (Task 5.8)
    AuraManager& getAuras() { return m_auras; }
    const AuraManager& getAuras() const { return m_auras; }

    // Convenience methods for aura queries
    bool hasAura(int32_t spellId) const { return m_auras.hasAura(spellId); }
    bool isStunned() const { return m_auras.isStunned(); }
    bool isSilenced() const { return m_auras.isSilenced(); }
    bool isRooted() const { return m_auras.isRooted(); }

    // Combat - Death handling (Task 5.11)
    virtual bool isDead() const;
    virtual void takeDamage(int32_t amount, Entity* attacker = nullptr);
    virtual void heal(int32_t amount, Entity* healer = nullptr);
    virtual void onDeath(Entity* killer);
    virtual void setDead(bool dead);

    // Invulnerability (for testing/god mode)
    bool isInvulnerable() const { return m_invulnerable; }
    void setInvulnerable(bool invuln) { m_invulnerable = invuln; }

    // Broadcast variable update to nearby players
    void broadcastVariable(ObjDefines::Variable var, int32_t value);

protected:
    std::string m_name;

    // World position
    int m_mapId = 0;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_orientation = 0.0f;

    // Map reference
    Map* m_map = nullptr;

    // Spawned in world
    bool m_spawned = false;

    // Invulnerable (immune to damage, for testing)
    bool m_invulnerable = false;

    // Variable change callback
    VariableCallback m_variableCallback;

    // Visibility tracking
    std::set<Entity*> m_visibleTo;  // Who can see me
    std::set<Entity*> m_canSee;     // Who I can see

    // Aura manager (Task 5.8)
    AuraManager m_auras;
};
