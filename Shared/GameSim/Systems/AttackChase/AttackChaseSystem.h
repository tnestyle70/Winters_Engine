#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <vector>

class CWorld;

namespace AttackChaseGeometry
{
    f32_t ResolveMoveArriveRadius(
        f32_t effectiveRange,
        const Vec3& targetPosition,
        const Vec3& resolvedMoveTarget);
}

class CAttackChaseSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc,
        std::vector<GameCommand>& outCommands);

private:
    CAttackChaseSystem() = delete;
};
