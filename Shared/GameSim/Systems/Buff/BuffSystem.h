#pragma once

#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/EconomyGameplayDef.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

class CWorld;
struct DamageRequest;
struct DamageResult;

class CBuffSystem
{
public:
    static bool AddOrRefresh(BuffComponent& component, const BuffInstance& instance);
    static bool_t PruneExpiredTickBuffs(
        CWorld& world,
        const TickContext& tc);
    static void AdvanceDurationsAfterStat(
        CWorld& world,
        const TickContext& tc);
    static const ObjectiveGameplayDef& ResolveObjectiveTuning(const TickContext& tc);
    static bool_t HasObjectiveBuff(
        CWorld& world,
        EntityID entity,
        eObjectiveBuffKind kind);
    static void AddOrRefreshObjectiveBuff(
        CWorld& world,
        EntityID entity,
        eObjectiveBuffKind kind,
        const TickContext& tc);
    static void TickObjectiveEffects(CWorld& world, const TickContext& tc);
    static void CleanupDeadObjectiveState(CWorld& world);
    static void OnDamageResolved(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        DamageResult& result);
    static void ClearObjectiveState(CWorld& world);
    static void UnapplyAllBaronEmpoweredMinions(CWorld& world);
};
