// AuraSystem - Buff/Debuff management
// Task 5.8: Aura System
// Task 5.9: Aura Effects

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include "SpellDefines.h"

// Forward declarations
class Entity;
class Player;
struct SpellTemplate;

// ============================================================================
// Aura Configuration
// ============================================================================

namespace AuraConfig
{
    // Maximum number of auras per entity
    constexpr size_t MAX_BUFFS = 32;
    constexpr size_t MAX_DEBUFFS = 16;

    // Default values
    constexpr int32_t DEFAULT_STACK_LIMIT = 1;
    constexpr int32_t DEFAULT_PERIODIC_INTERVAL_MS = 3000;  // 3 seconds

    // Aura flags for special behavior
    namespace Flags
    {
        constexpr uint32_t None = 0;
        constexpr uint32_t Positive = 1 << 0;      // Is a buff (vs debuff)
        constexpr uint32_t Hidden = 1 << 1;        // Don't show in UI
        constexpr uint32_t Passive = 1 << 2;       // Passive effect (no icon)
        constexpr uint32_t Persistent = 1 << 3;    // Survives death
        constexpr uint32_t CannotDispel = 1 << 4;  // Cannot be dispelled
        constexpr uint32_t FromItem = 1 << 5;      // Applied by item
    }
}

// ============================================================================
// Aura Effect - A single effect within an aura
// ============================================================================

struct AuraEffect
{
    SpellDefines::AuraType type = SpellDefines::AuraType::None;
    int32_t baseValue = 0;           // Base effect value
    int32_t perStackValue = 0;       // Additional value per stack
    int32_t miscValue = 0;           // Additional data (stat type, school, etc.)
    int32_t periodicIntervalMs = 0;  // For DoT/HoT: tick interval
    int32_t periodicTimer = 0;       // Current timer for next tick
};

// ============================================================================
// Aura - A buff or debuff applied to an entity
// ============================================================================

struct Aura
{
    // Identification
    int32_t spellId = 0;             // Source spell ID
    uint64_t casterGuid = 0;         // Who applied this aura
    int32_t effectIndex = 0;         // Which spell effect created this (0-2)

    // Duration
    int32_t maxDurationMs = 0;       // Total duration (0 = permanent)
    int32_t elapsedMs = 0;           // Time elapsed since application

    // Stacking
    int32_t stacks = 1;              // Current stack count
    int32_t maxStacks = 1;           // Maximum stacks

    // Flags
    uint32_t flags = 0;              // AuraConfig::Flags

    // Effects (up to 3 per aura, matching spell effects)
    std::vector<AuraEffect> effects;

    // Helper methods
    bool isPositive() const { return (flags & AuraConfig::Flags::Positive) != 0; }
    bool isExpired() const { return maxDurationMs > 0 && elapsedMs >= maxDurationMs; }
    int32_t getRemainingMs() const { return maxDurationMs > 0 ? std::max(0, maxDurationMs - elapsedMs) : -1; }

    // Get scaled effect value (base + per-stack bonus)
    int32_t getEffectValue(size_t effectIdx) const
    {
        if (effectIdx >= effects.size())
            return 0;
        return effects[effectIdx].baseValue + effects[effectIdx].perStackValue * (stacks - 1);
    }
};

// ============================================================================
// AuraManager - Manages all auras on a single entity
// ============================================================================

class AuraManager
{
public:
    AuraManager() = default;
    ~AuraManager() = default;

    // Set the owning entity (called on creation)
    void setOwner(Entity* owner) { m_owner = owner; }
    Entity* getOwner() const { return m_owner; }

    // ========================================================================
    // Aura Application
    // ========================================================================

    // Apply an aura from a spell effect
    // Returns true if aura was applied/refreshed
    bool applyAura(Entity* caster, const SpellTemplate* spell, int effectIndex);

    // Apply an aura directly (for scripted/item effects)
    bool applyAura(const Aura& aura);

    // Remove an aura by spell ID
    // If casterGuid is 0, removes any aura with that spellId
    void removeAura(int32_t spellId, uint64_t casterGuid = 0);

    // Remove all auras from a specific caster
    void removeAurasFromCaster(uint64_t casterGuid);

    // Remove all auras of a specific type
    void removeAurasByType(SpellDefines::AuraType type);

    // Remove all dispellable auras (for dispel effects)
    void removeDispellableAuras(bool positive);

    // Clear all auras (death, zone change, etc.)
    void clearAll(bool includePersistent = false);

    // ========================================================================
    // Aura Queries
    // ========================================================================

    // Check if entity has a specific aura
    bool hasAura(int32_t spellId) const;
    bool hasAura(int32_t spellId, uint64_t casterGuid) const;

    // Check if entity has any aura of a specific type
    bool hasAuraType(SpellDefines::AuraType type) const;

    // Get aura by spell ID (returns nullptr if not found)
    Aura* getAura(int32_t spellId);
    const Aura* getAura(int32_t spellId) const;

    // Get all auras
    const std::vector<Aura>& getAuras() const { return m_auras; }

    // Get aura count
    size_t getAuraCount() const { return m_auras.size(); }
    size_t getBuffCount() const;
    size_t getDebuffCount() const;

    // ========================================================================
    // Aura Modifiers (Task 5.9)
    // ========================================================================

    // Get total modifier from all auras of a specific type
    int32_t getAuraModifier(SpellDefines::AuraType type) const;

    // Get modifier for a specific stat
    int32_t getStatModifier(int32_t statType) const;

    // Check for crowd control effects
    bool isStunned() const;
    bool isSilenced() const;
    bool isRooted() const;

    // Get damage absorb amount available
    int32_t getAbsorbRemaining() const;

    // Consume absorb (returns amount actually absorbed)
    int32_t consumeAbsorb(int32_t damage);

    // ========================================================================
    // Update
    // ========================================================================

    // Update all auras (call each tick)
    // deltaTimeMs is time since last update in milliseconds
    void update(int32_t deltaTimeMs);

    // ========================================================================
    // Client Sync
    // ========================================================================

    // Mark that auras have changed and need to be sent to clients
    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    // Send aura update to client (for player owner) and nearby players
    void broadcastAuras();

private:
    // Check if aura can be applied (respecting limits)
    bool canApplyAura(const Aura& aura) const;

    // Handle periodic effects (DoT/HoT ticks)
    void processPeriodicEffects(Aura& aura, int32_t deltaTimeMs);

    // Apply aura effect when first added
    void applyAuraEffect(const Aura& aura, const AuraEffect& effect);

    // Remove aura effect when aura expires/removed
    void removeAuraEffect(const Aura& aura, const AuraEffect& effect);

    // Find existing aura that would stack/refresh with new one
    Aura* findStackableAura(int32_t spellId, uint64_t casterGuid);

    Entity* m_owner = nullptr;
    std::vector<Aura> m_auras;
    bool m_dirty = false;  // True if auras changed since last broadcast
};

// ============================================================================
// Helper Functions
// ============================================================================

namespace AuraUtils
{
    // Create an Aura from a spell template
    Aura createAuraFromSpell(Entity* caster, const SpellTemplate* spell, int effectIndex);

    // Check if an aura type is positive (buff)
    bool isPositiveAuraType(SpellDefines::AuraType type);

    // Check if an aura type is a crowd control effect
    bool isCrowdControl(SpellDefines::AuraType type);

    // Get string name for aura type (for debugging)
    const char* getAuraTypeName(SpellDefines::AuraType type);
}
