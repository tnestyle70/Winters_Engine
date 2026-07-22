#pragma once

#include "WintersTypes.h"

class CWorld;
struct SkillSlotRuntime;
struct TickContext;

class CSkillCooldownSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);
    static void RemapDefinitionCooldown(
        SkillSlotRuntime& slot,
        f32_t previousDefinitionCooldown,
        f32_t reloadedDefinitionCooldown);

private:
    CSkillCooldownSystem() = delete;
};
