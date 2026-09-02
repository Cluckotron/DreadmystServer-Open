#pragma once

#include "../Geo2d.h"
#include "UnitDefines.h"
#include "ObjDefines.h"
#include "PlayerDefines.h"
#include "ItemDefines.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

// Client-side movement/stat helper reconstructed for the released r1189 source.
// MutualUnit is intentionally a separate base from MutualObject: ClientUnit already
// inherits MutualObject through ClientObject.  Keeping the two independent avoids
// the diamond/ambiguity that the original emulator introduced.
class MutualUnit
{
public:
    MutualUnit() = default;
    explicit MutualUnit(Geo2d::Vector2& worldPosRef) : m_worldPosRef(&worldPosRef) {}
    virtual ~MutualUnit() = default;

    float getSpeed() const { return m_speed; }
    void setSpeed(float speed) { m_speed = std::max(0.01f, speed); }

    float getOrientation() const { return m_orientation; }
    void setOrientation(float orientation)
    {
        if (!std::isfinite(orientation))
            return;

        constexpr float TWO_PI = 6.28318530718f;
        orientation = std::fmod(orientation, TWO_PI);
        if (orientation < 0.0f)
            orientation += TWO_PI;

        m_orientation = orientation;
    }

    bool isInCombat() const { return getUnitVariable(ObjDefines::Variable::InCombat) != 0; }

    int getMana() const { return getUnitVariable(ObjDefines::Variable::Mana); }
    int getMaxMana() const { return getUnitVariable(ObjDefines::Variable::MaxMana); }
    float getManaPct() const
    {
        const int maxMana = getMaxMana();
        return maxMana > 0 ? static_cast<float>(getMana()) / static_cast<float>(maxMana) : 0.0f;
    }

    int getBaseStat(UnitDefines::Stat stat) const
    {
        return getUnitVariable(static_cast<ObjDefines::Variable>(
            ObjDefines::StatsStart + static_cast<int>(stat)));
    }

    // The server remains authoritative for the final derived stat.  These helpers
    // exist so the released UI can display its pending-investment preview.
    int getBaseStatBonus(UnitDefines::Stat /*stat*/) const { return 0; }

    void applyStatModifierLogic(PlayerDefines::Classes /*cls*/, UnitDefines::Stat /*stat*/,
                                int& /*value*/, std::map<UnitDefines::Stat, int>& /*allStats*/) const
    {
    }

    bool canEquipItem(PlayerDefines::Classes cls, ItemDefines::EquipType type) const
    {
        // The current shared PlayerFunctions implementation does not yet expose
        // the full historical class/equip matrix. Keep the client permissive here;
        // the server validates equipment requests authoritatively.
        (void)cls; (void)type;
        return true;
    }

    bool canEquipItem(PlayerDefines::Classes cls, ItemDefines::WeaponType type) const
    {
        (void)cls; (void)type;
        return true;
    }

    bool canEquipItem(PlayerDefines::Classes cls, ItemDefines::ArmorType armor,
                      ItemDefines::EquipType equip,
                      std::map<UnitDefines::Stat, int>* failedStatReq = nullptr) const
    {
        (void)cls; (void)armor; (void)equip;
        if (failedStatReq) failedStatReq->clear();
        return true;
    }

    bool canEquipItem(ItemDefines::WeaponType weapon, ItemDefines::WeaponMaterial material,
                      std::map<UnitDefines::Stat, int>* failedStatReq = nullptr) const
    {
        (void)weapon; (void)material;
        if (failedStatReq) failedStatReq->clear();
        return true;
    }

    int computeLevelupCost(const std::map<UnitDefines::Stat, int>& statInvestments,
                           const std::map<int, int>& spellInvestments) const
    {
        long long total = 0;
        for (const auto& [stat, count] : statInvestments)
        {
            const int base = std::max(0, getBaseStat(stat));
            for (int i = 0; i < count; ++i)
            {
                const int cost = PlayerFunctions::computeStatUpgradeCost(static_cast<int>(stat), base + i);
                if (cost < 0) break;
                total += cost;
            }
        }
        for (const auto& [spellId, count] : spellInvestments)
        {
            (void)spellId;
            for (int i = 0; i < count; ++i)
                total += PlayerFunctions::computeSpellUpgradeCost(i);
        }
        return static_cast<int>(std::min<long long>(total, 0x7fffffffLL));
    }

    bool seesAsFriendly(const MutualUnit& other) const
    {
        const auto f = other.faction();
        return f == UnitDefines::Faction::Friendly || f == UnitDefines::Faction::PlayerDefault;
    }

    bool seesAsHostile(const MutualUnit& other) const
    {
        return other.faction() == UnitDefines::Faction::Hostile ||
               other.faction() == UnitDefines::Faction::PvP;
    }

    virtual UnitDefines::Faction faction() const { return UnitDefines::Faction::Neutral; }

    bool hasSpline() const { return !m_spline.empty(); }
    const std::vector<Geo2d::Vector2>& spline() const { return m_spline; }
    bool isSlidingSpline() const { return m_slidingSpline; }

    void setSpline(const std::vector<Geo2d::Vector2>& spline, bool sliding = false)
    {
        m_spline = spline;
        m_slidingSpline = sliding;
    }

    void clearSpline()
    {
        m_spline.clear();
        m_slidingSpline = false;
    }

    // Returns true once idle.  Released ClientUnit uses !pumpSpline() && hasSpline()
    // to select its running animation.
    bool pumpSpline() const { return m_spline.empty(); }

    // Smooth local presentation only. The server owns the authoritative position
    // and sends spline/teleport corrections.  Base speed matches Private Protocol
    // v1's server simulation (4 logical map cells/sec at MoveSpeedPct=100).
    void pumpSpline(const std::vector<Geo2d::Vector2>& /*serverSpline*/,
                    const Geo2d::Vector2& /*serverLocation*/, float delta)
    {
        if (!m_worldPosRef || m_spline.empty() || delta <= 0.0f)
            return;

        constexpr float BASE_MOVE_SPEED = 4.0f;
        float budget = BASE_MOVE_SPEED * std::max(m_speed, 0.01f) * delta;

        while (budget > 0.0f && !m_spline.empty())
        {
            const Geo2d::Vector2 node = m_spline.front();
            const float dx = node.x - m_worldPosRef->x;
            const float dy = node.y - m_worldPosRef->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 0.0001f)
                setOrientation(std::atan2(dy, dx));

            if (dist <= 0.0001f || budget >= dist)
            {
                *m_worldPosRef = node;
                budget -= dist;
                m_spline.erase(m_spline.begin());
            }
            else
            {
                const float ratio = budget / dist;
                m_worldPosRef->x += dx * ratio;
                m_worldPosRef->y += dy * ratio;
                budget = 0.0f;
            }
        }
    }

protected:
    virtual int getUnitVariable(ObjDefines::Variable /*var*/) const { return 0; }

private:
    Geo2d::Vector2* m_worldPosRef = nullptr;
    float m_orientation = 0.0f;
    float m_speed = 1.0f;
    bool m_slidingSpline = false;
    std::vector<Geo2d::Vector2> m_spline;
};
