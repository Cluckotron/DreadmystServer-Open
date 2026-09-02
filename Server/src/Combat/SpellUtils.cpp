// SpellUtils - Spell calculation utilities implementation
// Task 5.1: Spell Template Loader (formula evaluation extension)

#include "stdafx.h"
#include "Combat/SpellUtils.h"
#include <cmath>
#include <cctype>
#include <stack>

namespace SpellUtils
{

// ============================================================================
// Formula Evaluation
// ============================================================================

// Simple expression evaluator for formulas like "2+((clvl*20)/20)"
// Supports: +, -, *, /, (), and the variable 'clvl'
int32_t evaluateFormula(const std::string& formula, int32_t clvl)
{
    if (formula.empty())
    {
        return 0;
    }

    // Replace 'clvl' with the actual value
    std::string expr = formula;
    size_t pos;
    while ((pos = expr.find("clvl")) != std::string::npos)
    {
        expr.replace(pos, 4, std::to_string(clvl));
    }

    // Shunting-yard algorithm for expression evaluation
    std::stack<double> values;
    std::stack<char> ops;

    auto precedence = [](char op) -> int {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        return 0;
    };

    auto applyOp = [](double a, double b, char op) -> double {
        switch (op)
        {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return b != 0 ? a / b : 0;
            default: return 0;
        }
    };

    size_t i = 0;
    while (i < expr.length())
    {
        // Skip whitespace
        if (std::isspace(expr[i]))
        {
            i++;
            continue;
        }

        // Number (or negative number at start or after opening paren)
        if (std::isdigit(expr[i]) || (expr[i] == '-' && (i == 0 || expr[i-1] == '(')))
        {
            double num = 0;
            bool negative = false;
            if (expr[i] == '-')
            {
                negative = true;
                i++;
            }
            while (i < expr.length() && (std::isdigit(expr[i]) || expr[i] == '.'))
            {
                if (expr[i] == '.')
                {
                    double frac = 0.1;
                    i++;
                    while (i < expr.length() && std::isdigit(expr[i]))
                    {
                        num += (expr[i] - '0') * frac;
                        frac *= 0.1;
                        i++;
                    }
                }
                else
                {
                    num = num * 10 + (expr[i] - '0');
                    i++;
                }
            }
            values.push(negative ? -num : num);
        }
        // Opening parenthesis
        else if (expr[i] == '(')
        {
            ops.push('(');
            i++;
        }
        // Closing parenthesis
        else if (expr[i] == ')')
        {
            while (!ops.empty() && ops.top() != '(')
            {
                if (values.size() < 2) break;
                double b = values.top(); values.pop();
                double a = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(applyOp(a, b, op));
            }
            if (!ops.empty()) ops.pop();  // Remove '('
            i++;
        }
        // Operator
        else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
        {
            while (!ops.empty() && ops.top() != '(' &&
                   precedence(ops.top()) >= precedence(expr[i]))
            {
                if (values.size() < 2) break;
                double b = values.top(); values.pop();
                double a = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(applyOp(a, b, op));
            }
            ops.push(expr[i]);
            i++;
        }
        else
        {
            i++;  // Skip unknown character
        }
    }

    // Apply remaining operators
    while (!ops.empty())
    {
        if (values.size() < 2) break;
        double b = values.top(); values.pop();
        double a = values.top(); values.pop();
        char op = ops.top(); ops.pop();
        values.push(applyOp(a, b, op));
    }

    return values.empty() ? 0 : static_cast<int32_t>(std::round(values.top()));
}

// ============================================================================
// Mana Cost Calculation
// ============================================================================

int32_t calculateManaCost(const SpellTemplate* spell, int32_t casterLevel, int32_t maxMana)
{
    if (!spell)
        return 0;

    // Percentage-based mana cost takes priority
    if (spell->manaPct > 0)
    {
        return (maxMana * spell->manaPct) / 100;
    }

    // Formula-based cost
    if (!spell->manaFormula.empty())
    {
        return evaluateFormula(spell->manaFormula, casterLevel);
    }

    return 0;
}

// ============================================================================
// Duration Calculation
// ============================================================================

int32_t calculateDuration(const SpellTemplate* spell, int32_t casterLevel)
{
    (void)casterLevel;  // Reserved for level-scaled durations (durationFormula field exists in schema)

    if (!spell)
        return 0;

    // Use fixed duration from template
    return spell->duration;
}

// ============================================================================
// Effect Value Calculation
// ============================================================================

int32_t calculateEffectValue(const SpellTemplate* spell, int effectIndex, int32_t casterLevel)
{
    if (!spell || effectIndex < 0 || effectIndex >= 3)
        return 0;

    // If there's a scale formula, use it
    if (!spell->effectScaleFormula[effectIndex].empty())
    {
        return evaluateFormula(spell->effectScaleFormula[effectIndex], casterLevel);
    }

    // Otherwise use base data1 value with simple level scaling via data2
    int32_t base = spell->effectData1[effectIndex];
    int32_t perLevel = spell->effectData2[effectIndex];

    return base + (perLevel * casterLevel);
}

// ============================================================================
// Spell Type Queries
// ============================================================================

SpellDefines::Effects getPrimaryEffect(const SpellTemplate* spell)
{
    if (!spell)
        return SpellDefines::Effects::None;

    // Return first non-None effect
    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] != 0)
        {
            return static_cast<SpellDefines::Effects>(spell->effect[i]);
        }
    }

    return SpellDefines::Effects::None;
}

bool isDamageSpell(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    for (int i = 0; i < 3; ++i)
    {
        auto eff = static_cast<SpellDefines::Effects>(spell->effect[i]);
        if (eff == SpellDefines::Effects::Damage ||
            eff == SpellDefines::Effects::MeleeAtk ||
            eff == SpellDefines::Effects::RangedAtk)
        {
            return true;
        }
    }

    return false;
}

bool isHealSpell(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    for (int i = 0; i < 3; ++i)
    {
        auto eff = static_cast<SpellDefines::Effects>(spell->effect[i]);
        if (eff == SpellDefines::Effects::Heal)
        {
            return true;
        }
    }

    return false;
}

bool isAuraSpell(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    for (int i = 0; i < 3; ++i)
    {
        auto eff = static_cast<SpellDefines::Effects>(spell->effect[i]);
        if (eff == SpellDefines::Effects::ApplyAura ||
            eff == SpellDefines::Effects::ApplyAreaAura)
        {
            return true;
        }
    }

    return false;
}

bool isInstant(const SpellTemplate* spell)
{
    return spell && spell->castTime == 0;
}

bool requiresTarget(const SpellTemplate* spell)
{
    if (!spell)
        return true;

    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] == 0)
            continue;

        const auto targetType = static_cast<SpellDefines::TargetType>(spell->effectTargetType[i]);
        switch (targetType)
        {
            case SpellDefines::TargetType::Unit_Caster:
            case SpellDefines::TargetType::Unit_AreaSrc_Friendly:
            case SpellDefines::TargetType::Unit_AreaSrc_Hostile:
            case SpellDefines::TargetType::None:
                break; // caster/source-centered; no explicit target required
            default:
                return true;
        }
    }

    return false;
}

bool canTargetFriendly(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] == 0)
            continue;

        const auto targetType = static_cast<SpellDefines::TargetType>(spell->effectTargetType[i]);
        switch (targetType)
        {
            case SpellDefines::TargetType::None:
            case SpellDefines::TargetType::Unit_Caster:
            case SpellDefines::TargetType::Unit_Friendly:
            case SpellDefines::TargetType::Unit_AreaSrc_Friendly:
            case SpellDefines::TargetType::Unit_AreaDst_Friendly:
            case SpellDefines::TargetType::Unit_AreaDst_Friendly_FromDst:
            case SpellDefines::TargetType::Unit_Any:
                return true;
            default:
                break;
        }
    }

    return false;
}

bool canTargetHostile(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] == 0)
            continue;

        const auto targetType = static_cast<SpellDefines::TargetType>(spell->effectTargetType[i]);
        switch (targetType)
        {
            case SpellDefines::TargetType::Unit_Hostile:
            case SpellDefines::TargetType::Unit_AreaSrc_Hostile:
            case SpellDefines::TargetType::Unit_AreaDst_Hostile:
            case SpellDefines::TargetType::Unit_AreaDst_Hostile_FromDst:
            case SpellDefines::TargetType::Unit_Any:
                return true;
            default:
                break;
        }
    }

    return false;
}

bool isSelfOnly(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    bool hasEffect = false;
    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] == 0)
            continue;

        hasEffect = true;
        const auto targetType = static_cast<SpellDefines::TargetType>(spell->effectTargetType[i]);
        if (targetType != SpellDefines::TargetType::None &&
            targetType != SpellDefines::TargetType::Unit_Caster)
        {
            return false;
        }
    }

    return hasEffect;
}

bool isAoE(const SpellTemplate* spell)
{
    if (!spell)
        return false;

    // Check for AoE radius on any effect
    for (int i = 0; i < 3; ++i)
    {
        if (spell->effect[i] != 0 && spell->effectRadius[i] > 0)
        {
            return true;
        }
    }

    // Also check for ApplyAreaAura effect
    for (int i = 0; i < 3; ++i)
    {
        if (static_cast<SpellDefines::Effects>(spell->effect[i]) == SpellDefines::Effects::ApplyAreaAura)
        {
            return true;
        }
    }

    return false;
}

SpellDefines::School getSpellSchool(const SpellTemplate* spell)
{
    if (!spell)
        return SpellDefines::School::Physical;

    return static_cast<SpellDefines::School>(spell->castSchool);
}

} // namespace SpellUtils
