#pragma once

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

struct DamageResult
{
    f32_t finalAmount = 0.f;
    bool_t bWasCrit = false;
    bool_t bWasShielded = false;
    bool_t bKilled = false;
    bool_t bApplied = false;
};

f32_t BuildRawDamage(CWorld& world, const DamageRequest& req);
f32_t ApplyResistance(f32_t amount, f32_t resistance);
f32_t ResolvePostMitigationDamageAmount(
    CWorld& world,
    const DamageRequest& req,
    f32_t rawAmount,
    bool_t bWasCrit);
bool_t WouldNonCriticalDamageRequestKill(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& req);
DamageResult ApplyDamageRequest(CWorld& world, const TickContext& tc,
    const DamageRequest& req);
bool_t PromoteDamageResultToExecution(
    CWorld& world,
    EntityID target,
    DamageResult& result);
void EnqueueDamageRequest(CWorld& world, const DamageRequest& req);
