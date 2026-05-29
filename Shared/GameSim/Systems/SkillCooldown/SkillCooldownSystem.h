#pragma once

class CWorld;
struct TickContext;

class CSkillCooldownSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CSkillCooldownSystem() = delete;
};
