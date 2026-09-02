// AuraSystem - Buff/Debuff management
// Task 5.8: Aura System
// Task 5.9: Aura Effects

#include "stdafx.h"
#include "Combat/AuraSystem.h"
#include "Combat/CombatFormulas.h"
#include "Combat/CombatMessenger.h"
#include "Combat/SpellUtils.h"
#include "Database/GameData.h"
#include "World/Entity.h"
#include "World/Player.h"
#include "World/WorldManager.h"
#include "Core/Logger.h"
#include "GamePacketServer.h"
#include "ObjDefines.h"

#include <algorithm>

// ============================================================================
// AuraUtils - Helper functions
// ============================================================================

namespace AuraUtils
{

Aura createAuraFromSpell(Entity* caster, const SpellTemplate* spell, int effectIndex)
{
    Aura aura;

    if (!spell || effectIndex < 0 || effectIndex >= 3)
        return aura;

    aura.spellId = spell->entry;
    aura.casterGuid = caster ? caster->getGuid() : 0;
    aura.effectIndex = effectIndex;

    // Duration from spell template
    aura.maxDurationMs = spell->duration;
    aura.elapsedMs = 0;

    // Stacking from spell template
    aura.maxStacks = spell->stackAmount > 0 ? spell->stackAmount : 1;
    aura.stacks = 1;

    // Determine if positive based on effect
    bool isPositive = (spell->effectPositive[effectIndex] != 0);
    if (isPositive)
        aura.flags |= AuraConfig::Flags::Positive;

    // Create the aura effect
    AuraEffect effect;
    effect.type = static_cast<SpellDefines::AuraType>(spell->effectData1[effectIndex]);

    // Calculate effect value using spell formula
    int32_t casterLevel = caster ? caster->getVariable(ObjDefines::Variable::Level) : 1;
    effect.baseValue = SpellUtils::calculateEffectValue(spell, effectIndex, casterLevel);
    effect.perStackValue = 0;  // TODO: Could be derived from spell data

    // Misc value depends on aura type
    effect.miscValue = spell->effectData2[effectIndex];

    // Periodic interval for DoT/HoT
    if (spell->interval > 0)
    {
        effect.periodicIntervalMs = spell->interval;
    }
    else if (effect.type == SpellDefines::AuraType::PeriodicDamage ||
             effect.type == SpellDefines::AuraType::PeriodicHeal)
    {
        effect.periodicIntervalMs = AuraConfig::DEFAULT_PERIODIC_INTERVAL_MS;
    }
    effect.periodicTimer = 0;

    aura.effects.push_back(effect);

    return aura;
}

bool isPositiveAuraType(SpellDefines::AuraType type)
{
    switch (type)
    {
        case SpellDefines::AuraType::PeriodicHeal:
        case SpellDefines::AuraType::PeriodicRestoreMana:
            return true;

        case SpellDefines::AuraType::PeriodicDamage:
        case SpellDefines::AuraType::PeriodicBurnMana:
        case SpellDefines::AuraType::Stun:
        case SpellDefines::AuraType::Root:
        case SpellDefines::AuraType::Silence:
            return false;

        // Stat modifiers depend on the value (positive = buff)
        case SpellDefines::AuraType::ModStat:
        case SpellDefines::AuraType::ModDamage:
        case SpellDefines::AuraType::ModSpeed:
            return true;  // Assume positive by default, actual sign matters

        default:
            return true;
    }
}

bool isCrowdControl(SpellDefines::AuraType type)
{
    switch (type)
    {
        case SpellDefines::AuraType::Stun:
        case SpellDefines::AuraType::Root:
        case SpellDefines::AuraType::Silence:
            return true;
        default:
            return false;
    }
}

const char* getAuraTypeName(SpellDefines::AuraType type)
{
    switch (type)
    {
        case SpellDefines::AuraType::None:               return "None";
        case SpellDefines::AuraType::PeriodicDamage:     return "PeriodicDamage";
        case SpellDefines::AuraType::PeriodicHeal:       return "PeriodicHeal";
        case SpellDefines::AuraType::ModStat:            return "ModStat";
        case SpellDefines::AuraType::ModDamage:          return "ModDamage";
        case SpellDefines::AuraType::ModSpeed:           return "ModSpeed";
        case SpellDefines::AuraType::Stun:               return "Stun";
        case SpellDefines::AuraType::Root:               return "Root";
        case SpellDefines::AuraType::Silence:            return "Silence";
        case SpellDefines::AuraType::PeriodicBurnMana:   return "PeriodicBurnMana";
        case SpellDefines::AuraType::PeriodicRestoreMana: return "PeriodicRestoreMana";
        default:                                         return "Unknown";
    }
}

} // namespace AuraUtils

// ============================================================================
// AuraManager - Application
// ============================================================================

bool AuraManager::applyAura(Entity* caster, const SpellTemplate* spell, int effectIndex)
{
    if (!spell || !m_owner)
        return false;

    Aura aura = AuraUtils::createAuraFromSpell(caster, spell, effectIndex);
    if (aura.spellId == 0)
        return false;

    return applyAura(aura);
}

bool AuraManager::applyAura(const Aura& newAura)
{
    if (!m_owner || newAura.spellId == 0)
        return false;

    // Check if we can apply (respect limits)
    if (!canApplyAura(newAura))
    {
        LOG_DEBUG("AuraManager: Cannot apply aura {} - limit reached", newAura.spellId);
        return false;
    }

    // Check for existing aura that can be refreshed/stacked
    Aura* existing = findStackableAura(newAura.spellId, newAura.casterGuid);

    if (existing)
    {
        // Refresh duration
        existing->elapsedMs = 0;

        // Add stacks if possible
        if (existing->stacks < existing->maxStacks)
        {
            existing->stacks++;
            LOG_DEBUG("AuraManager: Aura {} stacked to {}/{} on entity {}",
                      existing->spellId, existing->stacks, existing->maxStacks, m_owner->getGuid());
        }
        else
        {
            LOG_DEBUG("AuraManager: Aura {} refreshed on entity {}",
                      existing->spellId, m_owner->getGuid());
        }

        markDirty();
        return true;
    }

    // Apply new aura
    Aura aura = newAura;

    // Apply each effect
    for (const auto& effect : aura.effects)
    {
        applyAuraEffect(aura, effect);
    }

    m_auras.push_back(aura);
    markDirty();

    LOG_DEBUG("AuraManager: Applied aura {} (type={}) to entity {}",
              aura.spellId,
              aura.effects.empty() ? "none" : AuraUtils::getAuraTypeName(aura.effects[0].type),
              m_owner->getGuid());

    return true;
}

void AuraManager::removeAura(int32_t spellId, uint64_t casterGuid)
{
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [this, spellId, casterGuid](const Aura& aura)
        {
            if (aura.spellId != spellId)
                return false;
            if (casterGuid != 0 && aura.casterGuid != casterGuid)
                return false;

            // Remove effects before erasing
            for (const auto& effect : aura.effects)
            {
                removeAuraEffect(aura, effect);
            }

            LOG_DEBUG("AuraManager: Removed aura {} from entity {}",
                      aura.spellId, m_owner->getGuid());
            return true;
        });

    if (it != m_auras.end())
    {
        m_auras.erase(it, m_auras.end());
        markDirty();
    }
}

void AuraManager::removeAurasFromCaster(uint64_t casterGuid)
{
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [this, casterGuid](const Aura& aura)
        {
            if (aura.casterGuid != casterGuid)
                return false;

            for (const auto& effect : aura.effects)
            {
                removeAuraEffect(aura, effect);
            }
            return true;
        });

    if (it != m_auras.end())
    {
        m_auras.erase(it, m_auras.end());
        markDirty();
    }
}

void AuraManager::removeAurasByType(SpellDefines::AuraType type)
{
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [this, type](const Aura& aura)
        {
            for (const auto& effect : aura.effects)
            {
                if (effect.type == type)
                {
                    for (const auto& eff : aura.effects)
                    {
                        removeAuraEffect(aura, eff);
                    }
                    return true;
                }
            }
            return false;
        });

    if (it != m_auras.end())
    {
        m_auras.erase(it, m_auras.end());
        markDirty();
    }
}

void AuraManager::removeDispellableAuras(bool positive)
{
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [this, positive](const Aura& aura)
        {
            if (aura.isPositive() != positive)
                return false;
            if (aura.flags & AuraConfig::Flags::CannotDispel)
                return false;

            for (const auto& effect : aura.effects)
            {
                removeAuraEffect(aura, effect);
            }
            return true;
        });

    if (it != m_auras.end())
    {
        m_auras.erase(it, m_auras.end());
        markDirty();
    }
}

void AuraManager::clearAll(bool includePersistent)
{
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [this, includePersistent](const Aura& aura)
        {
            if (!includePersistent && (aura.flags & AuraConfig::Flags::Persistent))
                return false;

            for (const auto& effect : aura.effects)
            {
                removeAuraEffect(aura, effect);
            }
            return true;
        });

    if (it != m_auras.end())
    {
        m_auras.erase(it, m_auras.end());
        markDirty();
    }
}

// ============================================================================
// AuraManager - Queries
// ============================================================================

bool AuraManager::hasAura(int32_t spellId) const
{
    return std::any_of(m_auras.begin(), m_auras.end(),
        [spellId](const Aura& aura) { return aura.spellId == spellId; });
}

bool AuraManager::hasAura(int32_t spellId, uint64_t casterGuid) const
{
    return std::any_of(m_auras.begin(), m_auras.end(),
        [spellId, casterGuid](const Aura& aura)
        {
            return aura.spellId == spellId && aura.casterGuid == casterGuid;
        });
}

bool AuraManager::hasAuraType(SpellDefines::AuraType type) const
{
    for (const auto& aura : m_auras)
    {
        for (const auto& effect : aura.effects)
        {
            if (effect.type == type)
                return true;
        }
    }
    return false;
}

Aura* AuraManager::getAura(int32_t spellId)
{
    auto it = std::find_if(m_auras.begin(), m_auras.end(),
        [spellId](const Aura& aura) { return aura.spellId == spellId; });
    return it != m_auras.end() ? &(*it) : nullptr;
}

const Aura* AuraManager::getAura(int32_t spellId) const
{
    auto it = std::find_if(m_auras.begin(), m_auras.end(),
        [spellId](const Aura& aura) { return aura.spellId == spellId; });
    return it != m_auras.end() ? &(*it) : nullptr;
}

size_t AuraManager::getBuffCount() const
{
    return std::count_if(m_auras.begin(), m_auras.end(),
        [](const Aura& aura) { return aura.isPositive(); });
}

size_t AuraManager::getDebuffCount() const
{
    return std::count_if(m_auras.begin(), m_auras.end(),
        [](const Aura& aura) { return !aura.isPositive(); });
}

// ============================================================================
// AuraManager - Modifiers (Task 5.9)
// ============================================================================

int32_t AuraManager::getAuraModifier(SpellDefines::AuraType type) const
{
    int32_t total = 0;

    for (const auto& aura : m_auras)
    {
        for (size_t i = 0; i < aura.effects.size(); ++i)
        {
            if (aura.effects[i].type == type)
            {
                total += aura.getEffectValue(i);
            }
        }
    }

    return total;
}

int32_t AuraManager::getStatModifier(int32_t statType) const
{
    int32_t total = 0;

    for (const auto& aura : m_auras)
    {
        for (size_t i = 0; i < aura.effects.size(); ++i)
        {
            const auto& effect = aura.effects[i];
            if (effect.type == SpellDefines::AuraType::ModStat && effect.miscValue == statType)
            {
                total += aura.getEffectValue(i);
            }
        }
    }

    return total;
}

bool AuraManager::isStunned() const
{
    return hasAuraType(SpellDefines::AuraType::Stun);
}

bool AuraManager::isSilenced() const
{
    return hasAuraType(SpellDefines::AuraType::Silence);
}

bool AuraManager::isRooted() const
{
    return hasAuraType(SpellDefines::AuraType::Root);
}

int32_t AuraManager::getAbsorbRemaining() const
{
    // TODO: Implement absorb auras in future
    // For now, return 0
    return 0;
}

int32_t AuraManager::consumeAbsorb(int32_t damage)
{
    // TODO: Implement absorb consumption
    (void)damage;
    return 0;
}

// ============================================================================
// AuraManager - Update
// ============================================================================

void AuraManager::update(int32_t deltaTimeMs)
{
    if (m_auras.empty())
        return;

    bool anyExpired = false;

    for (auto& aura : m_auras)
    {
        // Update duration
        if (aura.maxDurationMs > 0)
        {
            aura.elapsedMs += deltaTimeMs;

            if (aura.isExpired())
            {
                anyExpired = true;
                continue;  // Will be removed after loop
            }
        }

        // Process periodic effects
        processPeriodicEffects(aura, deltaTimeMs);
    }

    // Remove expired auras
    if (anyExpired)
    {
        auto it = std::remove_if(m_auras.begin(), m_auras.end(),
            [this](const Aura& aura)
            {
                if (aura.isExpired())
                {
                    for (const auto& effect : aura.effects)
                    {
                        removeAuraEffect(aura, effect);
                    }
                    LOG_DEBUG("AuraManager: Aura {} expired on entity {}",
                              aura.spellId, m_owner->getGuid());
                    return true;
                }
                return false;
            });

        m_auras.erase(it, m_auras.end());
        markDirty();
    }

    // Broadcast if dirty
    if (m_dirty)
    {
        broadcastAuras();
    }
}

void AuraManager::processPeriodicEffects(Aura& aura, int32_t deltaTimeMs)
{
    if (!m_owner)
        return;

    for (auto& effect : aura.effects)
    {
        if (effect.periodicIntervalMs <= 0)
            continue;

        effect.periodicTimer += deltaTimeMs;

        while (effect.periodicTimer >= effect.periodicIntervalMs)
        {
            effect.periodicTimer -= effect.periodicIntervalMs;

            // Apply periodic effect
            int32_t value = aura.getEffectValue(&effect - &aura.effects[0]);

            switch (effect.type)
            {
                case SpellDefines::AuraType::PeriodicDamage:
                {
                    // Send combat message first (Task 5.10)
                    CombatMessenger::sendPeriodicDamage(aura.casterGuid, m_owner, aura.spellId, value);

                    // Apply damage using Entity::takeDamage for proper death handling (Task 5.11)
                    // Note: We need the caster Entity, but we only have GUID
                    // For DoT, the attacker is whoever cast the original aura
                    // TODO: Could track caster entity reference in Aura for proper death credit
                    m_owner->takeDamage(value, nullptr);

                    LOG_DEBUG("AuraManager: DoT {} dealt {} periodic damage to entity {}",
                              aura.spellId, value, m_owner->getGuid());
                    break;
                }

                case SpellDefines::AuraType::PeriodicHeal:
                {
                    // Calculate actual heal before applying
                    int32_t health = m_owner->getVariable(ObjDefines::Variable::Health);
                    int32_t maxHealth = m_owner->getVariable(ObjDefines::Variable::MaxHealth);
                    int32_t actualHeal = std::min(value, maxHealth - health);

                    // Send combat message (Task 5.10)
                    CombatMessenger::sendPeriodicHeal(aura.casterGuid, m_owner, aura.spellId, actualHeal);

                    // Apply heal using Entity::heal for consistent handling
                    m_owner->heal(value, nullptr);

                    LOG_DEBUG("AuraManager: HoT {} healed {} on entity {}",
                              aura.spellId, actualHeal, m_owner->getGuid());
                    break;
                }

                case SpellDefines::AuraType::PeriodicBurnMana:
                {
                    int32_t mana = m_owner->getVariable(ObjDefines::Variable::Mana);
                    mana = std::max(0, mana - value);
                    m_owner->setVariable(ObjDefines::Variable::Mana, mana);
                    break;
                }

                case SpellDefines::AuraType::PeriodicRestoreMana:
                {
                    int32_t mana = m_owner->getVariable(ObjDefines::Variable::Mana);
                    int32_t maxMana = m_owner->getVariable(ObjDefines::Variable::MaxMana);
                    mana = std::min(maxMana, mana + value);
                    m_owner->setVariable(ObjDefines::Variable::Mana, mana);
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// ============================================================================
// AuraManager - Effect Application/Removal
// ============================================================================

void AuraManager::applyAuraEffect(const Aura& aura, const AuraEffect& effect)
{
    if (!m_owner)
        return;

    // Apply immediate effect based on type
    switch (effect.type)
    {
        case SpellDefines::AuraType::ModStat:
            // Stat modifiers are calculated dynamically via getStatModifier()
            // No immediate action needed
            break;

        case SpellDefines::AuraType::ModDamage:
        case SpellDefines::AuraType::ModSpeed:
            // These are also calculated dynamically
            break;

        case SpellDefines::AuraType::Stun:
            // Set stunned flag/variable
            m_owner->setVariable(ObjDefines::Variable::IsStunned, 1);
            LOG_DEBUG("AuraManager: Entity {} is now stunned", m_owner->getGuid());
            break;

        case SpellDefines::AuraType::Silence:
            m_owner->setVariable(ObjDefines::Variable::IsSilenced, 1);
            LOG_DEBUG("AuraManager: Entity {} is now silenced", m_owner->getGuid());
            break;

        case SpellDefines::AuraType::Root:
            m_owner->setVariable(ObjDefines::Variable::IsRooted, 1);
            LOG_DEBUG("AuraManager: Entity {} is now rooted", m_owner->getGuid());
            break;

        default:
            break;
    }

    (void)aura;  // Used for logging if needed
}

void AuraManager::removeAuraEffect(const Aura& aura, const AuraEffect& effect)
{
    if (!m_owner)
        return;

    // Remove effect based on type
    switch (effect.type)
    {
        case SpellDefines::AuraType::Stun:
            // Only clear if no other stuns active
            if (!hasAuraType(SpellDefines::AuraType::Stun))
            {
                m_owner->setVariable(ObjDefines::Variable::IsStunned, 0);
                LOG_DEBUG("AuraManager: Entity {} is no longer stunned", m_owner->getGuid());
            }
            break;

        case SpellDefines::AuraType::Silence:
            if (!hasAuraType(SpellDefines::AuraType::Silence))
            {
                m_owner->setVariable(ObjDefines::Variable::IsSilenced, 0);
                LOG_DEBUG("AuraManager: Entity {} is no longer silenced", m_owner->getGuid());
            }
            break;

        case SpellDefines::AuraType::Root:
            if (!hasAuraType(SpellDefines::AuraType::Root))
            {
                m_owner->setVariable(ObjDefines::Variable::IsRooted, 0);
                LOG_DEBUG("AuraManager: Entity {} is no longer rooted", m_owner->getGuid());
            }
            break;

        default:
            break;
    }

    (void)aura;  // Used for logging if needed
}

// ============================================================================
// AuraManager - Helpers
// ============================================================================

bool AuraManager::canApplyAura(const Aura& aura) const
{
    size_t currentBuffs = getBuffCount();
    size_t currentDebuffs = getDebuffCount();

    if (aura.isPositive())
    {
        return currentBuffs < AuraConfig::MAX_BUFFS;
    }
    else
    {
        return currentDebuffs < AuraConfig::MAX_DEBUFFS;
    }
}

Aura* AuraManager::findStackableAura(int32_t spellId, uint64_t casterGuid)
{
    // Find existing aura with same spell ID
    // Some auras stack per-caster, others are shared
    for (auto& aura : m_auras)
    {
        if (aura.spellId == spellId)
        {
            // For now, same spell ID = same aura (refreshes/stacks)
            // Future: Could check casterGuid for per-caster stacking
            (void)casterGuid;
            return &aura;
        }
    }
    return nullptr;
}

// ============================================================================
// AuraManager - Client Sync
// ============================================================================

void AuraManager::broadcastAuras()
{
    if (!m_owner)
        return;

    // Build the packet
    GP_Server_UnitAuras packet;
    packet.m_unitGuid = static_cast<uint32_t>(m_owner->getGuid());

    for (const auto& aura : m_auras)
    {
        GP_Server_UnitAuras::AuraInfo info;
        info.spellId = aura.spellId;
        info.casterGuid = static_cast<uint32_t>(aura.casterGuid);
        info.maxDuration = aura.maxDurationMs;
        info.elapsedTime = aura.elapsedMs;
        info.stacks = aura.stacks;
        info.positive = aura.isPositive();

        if (aura.isPositive())
        {
            packet.m_buffs.push_back(info);
        }
        else
        {
            packet.m_debuffs.push_back(info);
        }
    }

    // Serialize
    StlBuffer buf;
    uint16_t opcode = packet.getOpcode();
    buf << opcode;
    packet.pack(buf);

    // Send to the owner if it's a player
    Player* playerOwner = dynamic_cast<Player*>(m_owner);
    if (playerOwner)
    {
        playerOwner->sendPacket(buf);

        // Broadcast to nearby players (only for player entities)
        sWorldManager.broadcastToVisible(playerOwner, buf, false);  // Self already handled above
    }
    // TODO: For NPCs, would need to broadcast to all players who can see the NPC

    clearDirty();

    LOG_DEBUG("AuraManager: Broadcast {} buffs, {} debuffs for entity {}",
              packet.m_buffs.size(), packet.m_debuffs.size(), m_owner->getGuid());
}
