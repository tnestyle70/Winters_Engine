#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ShieldComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

#include <algorithm>
#include <cmath>

namespace
{
    u64_t SecondsToTicksCeil(f32_t seconds)
    {
        if (seconds <= 0.f)
            return 0u;

        return static_cast<u64_t>(std::ceil(
            static_cast<f64_t>(seconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
    }

    f32_t ResolveRemainingSec(const ShieldComponent& shield, u64_t tickIndex)
    {
        if (shield.uExpireTick <= tickIndex)
            return 0.f;

        return static_cast<f32_t>(shield.uExpireTick - tickIndex) *
            DeterministicTime::kFixedDt;
    }

    void SyncShieldMirrors(
        CWorld& world,
        EntityID target,
        const ShieldComponent& shield,
        u64_t tickIndex)
    {
        const f32_t current = (std::max)(0.f, shield.fCurrent);
        const f32_t remainingSec = ResolveRemainingSec(shield, tickIndex);

        if (world.HasComponent<ChampionComponent>(target))
            world.GetComponent<ChampionComponent>(target).shield = current;

        // Transitional mirrors keep existing Yasuo/Riven client and tuning paths
        // coherent while ShieldComponent remains the only server authority.
        if (world.HasComponent<YasuoStateComponent>(target))
        {
            YasuoStateComponent& state =
                world.GetComponent<YasuoStateComponent>(target);
            state.fPassiveShieldRemaining = current;
            state.fPassiveShieldMax = (std::max)(0.f, shield.fMaximum);
            state.fPassiveShieldTimer = remainingSec;
        }
        if (world.HasComponent<RivenStateComponent>(target))
        {
            RivenStateComponent& state =
                world.GetComponent<RivenStateComponent>(target);
            state.fShieldRemaining = current;
            state.fShieldTimer = remainingSec;
        }
    }

    void ClearShieldMirrors(CWorld& world, EntityID target)
    {
        if (world.HasComponent<ChampionComponent>(target))
            world.GetComponent<ChampionComponent>(target).shield = 0.f;

        if (world.HasComponent<YasuoStateComponent>(target))
        {
            YasuoStateComponent& state =
                world.GetComponent<YasuoStateComponent>(target);
            state.fPassiveShieldRemaining = 0.f;
            state.fPassiveShieldTimer = 0.f;
        }
        if (world.HasComponent<RivenStateComponent>(target))
        {
            RivenStateComponent& state =
                world.GetComponent<RivenStateComponent>(target);
            state.fShieldRemaining = 0.f;
            state.fShieldTimer = 0.f;
        }
    }
}

bool_t CShieldSystem::Grant(
    CWorld& world,
    const TickContext& tc,
    EntityID target,
    f32_t amount,
    f32_t durationSec)
{
    if (target == NULL_ENTITY ||
        !world.IsAlive(target) ||
        !world.HasComponent<ChampionComponent>(target) ||
        !std::isfinite(amount) ||
        !std::isfinite(durationSec) ||
        amount <= 0.f ||
        durationSec <= 0.f)
    {
        return false;
    }

    const u64_t durationTicks = SecondsToTicksCeil(durationSec);
    if (durationTicks == 0u)
        return false;

    ShieldComponent shield{};
    shield.fCurrent = amount;
    shield.fMaximum = amount;
    shield.uExpireTick = tc.tickIndex + durationTicks;

    if (world.HasComponent<ShieldComponent>(target))
        world.GetComponent<ShieldComponent>(target) = shield;
    else
        world.AddComponent<ShieldComponent>(target, shield);

    SyncShieldMirrors(world, target, shield, tc.tickIndex);
    return true;
}

f32_t CShieldSystem::Absorb(
    CWorld& world,
    const TickContext& tc,
    EntityID target,
    f32_t incomingDamage,
    bool_t& outShielded)
{
    if (!std::isfinite(incomingDamage) ||
        incomingDamage <= 0.f ||
        target == NULL_ENTITY ||
        !world.HasComponent<ShieldComponent>(target))
    {
        return incomingDamage;
    }

    ShieldComponent& shield = world.GetComponent<ShieldComponent>(target);
    if (!std::isfinite(shield.fCurrent) ||
        !std::isfinite(shield.fMaximum) ||
        shield.fCurrent <= 0.f ||
        tc.tickIndex >= shield.uExpireTick)
    {
        Clear(world, target);
        return incomingDamage;
    }

    const f32_t absorbed = (std::min)(incomingDamage, shield.fCurrent);
    shield.fCurrent -= absorbed;
    outShielded = outShielded || absorbed > 0.f;

    const f32_t remainingDamage = incomingDamage - absorbed;
    if (shield.fCurrent <= 0.f)
        Clear(world, target);
    else
        SyncShieldMirrors(world, target, shield, tc.tickIndex);

    return remainingDamage;
}

void CShieldSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto shields =
        DeterministicEntityIterator<ShieldComponent>::CollectSorted(world);
    for (EntityID target : shields)
    {
        if (!world.IsAlive(target) ||
            !world.HasComponent<ShieldComponent>(target))
        {
            continue;
        }

        ShieldComponent& shield = world.GetComponent<ShieldComponent>(target);
        const bool_t bChampionUnavailable =
            !world.HasComponent<ChampionComponent>(target) ||
            world.GetComponent<ChampionComponent>(target).hp <= 0.f;
        if (bChampionUnavailable ||
            !std::isfinite(shield.fCurrent) ||
            !std::isfinite(shield.fMaximum) ||
            shield.fCurrent <= 0.f ||
            tc.tickIndex >= shield.uExpireTick)
        {
            Clear(world, target);
            continue;
        }

        SyncShieldMirrors(world, target, shield, tc.tickIndex);
    }
}

void CShieldSystem::Clear(CWorld& world, EntityID target)
{
    if (target == NULL_ENTITY)
        return;

    ClearShieldMirrors(world, target);
    if (world.HasComponent<ShieldComponent>(target))
        world.RemoveComponent<ShieldComponent>(target);
}
