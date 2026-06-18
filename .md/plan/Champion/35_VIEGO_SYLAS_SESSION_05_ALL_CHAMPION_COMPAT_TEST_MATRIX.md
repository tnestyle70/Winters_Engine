Session - 모든 현재 챔피언이 Viego/Sylas steal 대상일 때 서버 훅과 검증 매트릭스를 완성한다.

1. 반영해야 하는 코드

이번 세션은 완벽 구현의 마무리다. 성공 기준은 현재 데이터와 HUD에 존재하는 모든 챔피언이 비에고 폼 오버라이드와 사일러스 Hijack에서 generic fallback 없이 서버 권한 훅으로 실행되거나, 명시적으로 테스트에서 불합격 처리되어 다음 세션으로 넘어가지 않는 것이다.

1-1. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
    AnnieGameSim::RegisterHooks();
    AsheGameSim::RegisterHooks();
    FioraGameSim::RegisterHooks();
    IreliaGameSim::RegisterHooks();
    JaxGameSim::RegisterHooks();
    KalistaGameSim::RegisterHooks();
    LeeSinGameSim::RegisterHooks();
    KindredGameSim::RegisterHooks();
    MasterYiGameSim::RegisterHooks();
    RivenGameSim::RegisterHooks();
    SylasGameSim::RegisterHooks();
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
    ZedGameSim::RegisterHooks();
```

아래로 교체:

```cpp
    AnnieGameSim::RegisterHooks();
    AsheGameSim::RegisterHooks();
    EzrealGameSim::RegisterHooks();
    FioraGameSim::RegisterHooks();
    GarenGameSim::RegisterHooks();
    IreliaGameSim::RegisterHooks();
    JaxGameSim::RegisterHooks();
    KalistaGameSim::RegisterHooks();
    LeeSinGameSim::RegisterHooks();
    KindredGameSim::RegisterHooks();
    MasterYiGameSim::RegisterHooks();
    RivenGameSim::RegisterHooks();
    SylasGameSim::RegisterHooks();
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
    ZedGameSim::RegisterHooks();
```

CONFIRM_NEEDED: 현재 `Shared/GameSim/Champions/Garen`과 `Shared/GameSim/Champions/Ezreal` 서버 GameSim 파일이 없다. 이 세션 구현 전에 새 h/cpp를 추가하고 Server.vcxproj/.filters 등록 여부를 확인한다. 계획서 규칙상 vcxproj XML은 여기에 쓰지 않는다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Garen/GarenGameSim.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"

class CWorld;
struct TickContext;

namespace GarenGameSim
{
    void RegisterHooks();
}
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Garen/GarenGameSim.cpp

새 파일:

```cpp
#include "Shared/GameSim/Champions/Garen/GarenGameSim.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

namespace
{
    void EnqueueTargetDamage(GameplayHookContext& ctx, f32_t baseDamage, f32_t perRank)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;
        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY ||
            !world.HasComponent<HealthComponent>(target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, ctx.casterEntity, target))
        {
            return;
        }

        DamageRequest request{};
        request.source = ctx.casterEntity;
        request.target = target;
        request.sourceTeam = ctx.casterTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = baseDamage + perRank * static_cast<f32_t>(ctx.skillRank);
        request.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::GAREN) << 8) | ctx.pCommand->slot);
        request.rank = ctx.skillRank;
        EnqueueDamageRequest(world, request);
    }

    void OnQ(GameplayHookContext& ctx) { EnqueueTargetDamage(ctx, 35.f, 20.f); }
    void OnW(GameplayHookContext&) {}
    void OnE(GameplayHookContext& ctx) { EnqueueTargetDamage(ctx, 50.f, 25.f); }
    void OnR(GameplayHookContext& ctx) { EnqueueTargetDamage(ctx, 120.f, 80.f); }
}

namespace GarenGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::GAREN, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::GAREN, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::GAREN, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::GAREN, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
    }
}
```

CONFIRM_NEEDED: Garen E가 실제로 지속 회전형이면 이 임시 서버 훅은 "fallback 제거용 최소 구현"이다. 완벽 판정 전에는 데이터의 stage/timing과 실제 intended skill shape를 다시 확인한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.h

새 파일:

```cpp
#pragma once

namespace EzrealGameSim
{
    void RegisterHooks();
}
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp

새 파일:

```cpp
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "WintersMath.h"

namespace
{
    Vec3 ResolveDirection(GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction, Vec3{}, 0.0001f);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }
        if (ctx.pWorld &&
            ctx.casterEntity != NULL_ENTITY &&
            ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const f32_t yaw = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }
        return Vec3{ 0.f, 0.f, 1.f };
    }

    void SpawnProjectile(GameplayHookContext& ctx, eProjectileKind kind, f32_t speed, f32_t fallbackRange)
    {
        if (!ctx.pWorld || ctx.casterEntity == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        origin.y += 1.f;

        const u8_t slot = ctx.pCommand ? ctx.pCommand->slot : static_cast<u8_t>(eSkillSlot::Q);
        f32_t range = ChampionGameDataDB::ResolveSkillRange(eChampion::EZREAL, slot);
        if (range <= 0.f)
            range = fallbackRange;

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = ctx.casterEntity;
        projectile.sourceTeam = ctx.casterTeam;
        projectile.kind = kind;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::EZREAL) << 8) | slot);
        projectile.rank = ctx.skillRank;
        projectile.currentPos = origin;
        projectile.direction = ResolveDirection(ctx);
        projectile.speed = speed;
        projectile.maxDistance = range;
        projectile.hitRadius = 0.45f;
        projectile.damage = 55.f + 30.f * static_cast<f32_t>(ctx.skillRank);

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
    }

    void OnQ(GameplayHookContext& ctx) { SpawnProjectile(ctx, eProjectileKind::Generic, 22.f, 11.f); }
    void OnW(GameplayHookContext& ctx) { SpawnProjectile(ctx, eProjectileKind::Generic, 18.f, 10.f); }
    void OnE(GameplayHookContext&) {}
    void OnR(GameplayHookContext& ctx) { SpawnProjectile(ctx, eProjectileKind::Generic, 24.f, 24.f); }
}

namespace EzrealGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::EZREAL, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
    }
}
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
    const bool_t bGameplayHookHandled =
        !bServerProjectileSkill &&
        DispatchGameplayHookIfAvailable(
            world, tc, resolvedCmd, primaryHookId, hookChampion, rank);
    if (!bServerProjectileSkill && !bGameplayHookHandled)
        EnqueueFallbackSkillDamage(world, resolvedCmd, hookChampion, rank);
```

아래로 교체:

```cpp
    const bool_t bGameplayHookHandled =
        !bServerProjectileSkill &&
        DispatchGameplayHookIfAvailable(
            world, tc, resolvedCmd, primaryHookId, hookChampion, rank);
    if (!bServerProjectileSkill && !bGameplayHookHandled)
    {
#if defined(_DEBUG)
        char msg[192]{};
        sprintf_s(msg,
            "[StealCompat][FallbackSkill] champion=%u slot=%u caster=%u\n",
            static_cast<u32_t>(hookChampion),
            static_cast<u32_t>(hookSlot),
            static_cast<u32_t>(resolvedCmd.issuerEntity));
        OutputCommandDebug(msg);
#endif
        EnqueueFallbackSkillDamage(world, resolvedCmd, hookChampion, rank);
    }
```

1-7. C:/Users/tnest/Desktop/Winters/.md/plan/Champion/35_VIEGO_SYLAS_STEAL_MATRIX.md

새 파일:

```markdown
Session - Viego/Sylas steal compatibility matrix

1. 반영해야 하는 코드

현재 챔피언별 검증 표다. 구현 완료 전까지 `PASS` 외 상태가 남아 있으면 완벽 구현으로 보지 않는다.

| Champion | Viego BA | Viego QWE | Viego R keep | Sylas R steal | Server hook status |
| --- | --- | --- | --- | --- | --- |
| ANNIE | TODO | TODO | TODO | TODO | existing |
| ASHE | TODO | TODO | TODO | TODO | existing |
| EZREAL | TODO | TODO | TODO | TODO | new in session 05 |
| FIORA | TODO | TODO | TODO | TODO | existing |
| GAREN | TODO | TODO | TODO | TODO | new in session 05 |
| IRELIA | TODO | TODO | TODO | TODO | existing |
| JAX | TODO | TODO | TODO | TODO | existing |
| KALISTA | TODO | TODO | TODO | TODO | existing |
| KINDRED | TODO | TODO | TODO | TODO | existing |
| LEESIN | TODO | TODO | TODO | TODO | existing |
| MASTERYI | TODO | TODO | TODO | TODO | existing |
| RIVEN | TODO | TODO | TODO | TODO | existing |
| SYLAS | TODO | TODO | TODO | N/A self steal blocked | existing |
| VIEGO | TODO | TODO | TODO | TODO | existing |
| YASUO | TODO | TODO | TODO | TODO | existing |
| YONE | TODO | TODO | TODO | TODO | existing |
| ZED | TODO | TODO | TODO | TODO | existing |

2. 검증

각 챔피언에 대해 비에고가 영혼을 먹은 뒤 기본공격/Q/W/E/R을 한 번씩 사용하고, 사일러스가 대상 R을 훔친 뒤 사용한다. 실패 원인은 fallback, target mode, missing hook, animation mismatch, HUD mismatch 중 하나로 기록한다.
```

2. 검증

`Server/Private/Game/GameRoom.cpp`에 새 Garen/Ezreal hook 등록 include가 필요한지 확인한다.

새 Garen/Ezreal h/cpp가 Server.vcxproj와 filters에 등록되었는지 확인한다. 계획서 규칙상 XML은 여기에 쓰지 않는다.

각 챔피언별로 비에고 영혼 소비 후 BA/Q/W/E/R, 사일러스 R steal/use를 실행하고 `.md/plan/Champion/35_VIEGO_SYLAS_STEAL_MATRIX.md`의 TODO를 PASS/FAIL로 갱신한다.

Debug 빌드에서 `[StealCompat][FallbackSkill]` 로그가 한 번이라도 나오면 해당 챔피언/스킬은 FAIL로 기록하고 fallback이 아닌 서버 훅 구현으로 고친다.

`git diff --check`를 실행한다.

Server와 Client를 빌드한다.
