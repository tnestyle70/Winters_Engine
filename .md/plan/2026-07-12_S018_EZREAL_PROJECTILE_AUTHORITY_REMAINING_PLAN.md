Session - S018 이즈리얼 투사체 권위의 실제 잔여를 AI 세션 Handoff 이후 충돌 없이 닫고, 결정성·late join·실제 LoL 패시브까지 단계적으로 완성한다.

상태: `Handoff` — 2026-07-13 구현·제품 빌드·GameRoom 서버 통합 검증 완료. Client packet-delivery를 포함한 실제 다중 클라이언트 렌더 스모크와 historical old-schema byte fixture는 아래 후속 경계로 분리한다.

## 1. 반영해야 하는 코드

현재 코드 증거와 잔여 판정:

- 현재 `SkillProjectileComponent`와 `EzrealGameSim.cpp`에는 BA/Q/W/E/R의 destroy/pierce, Yasuo barrier 차단, W source-target relation, E targeted bolt, R unique-hit ledger가 이미 들어와 있다. 이 계획은 그 동작을 다시 만드는 문서가 아니라 accept-time mana, passive, moving-target CCD, typed contact, lifetime-safe Net identity, late join/reconnect를 닫는 잔여 계획이다.
- 공용 projectile/dash/mark architecture의 장기 방향은 `.md/plan/2026-07-12_CHAMPION_PROJECTILE_MOTION_DETAIL_ARCHITECTURE.md`를 기준으로 하고, 이 문서는 그중 S018 Ezreal vertical slice의 실제 코드 anchor와 handoff gate만 구체화한다.
- RESOLVED 2026-07-13: canonical `Data/Gameplay/ChampionGameData/champions.json`과 `Shared/GameSim/Generated/ChampionGameData.generated.cpp`를 단일 생성기 경로로 맞췄고 양쪽 계산 hash가 `0xB8EF76C4`로 일치한다.
- 현재 `Snapshot.fbs` tail은 S017의 `simSpeedMul`이고 `SnapshotApplier`에는 timeline rebase가 이미 존재한다. 새 state는 이 경로 뒤에 append/reconcile하며 두 번째 timeline owner를 만들지 않는다.
- RESOLVED 2026-07-13: AI handoff 뒤 `Tools/SimLab/main.cpp`의 Shared 회귀 probe를 갱신했고, 실제 Server lifecycle은 `Tools/Harness/GameRoomProjectileIntegrationProbe.cpp`가 별도로 소유한다. Client packet delivery는 제품 링크와 production mutation comparator 계약까지 검증했으며 실제 다중 클라이언트 렌더 스모크는 후속이다.

소유 경계와 단계:

- P0 Shared/Data owner: generated Ezreal parity, accept-time paid mana, 범용 `BuffComponent` 기반 Rising Spell Force. `StatSystem`에 Ezreal 전용 분기를 넣지 않는다.
- P1 Server owner: continuous-vs-discontinuous motion, exact T-1 history, quantized CCD/contact, stored Net identity. gameplay truth는 `Shared/GameSim`/Server에만 둔다.
- P2 Schema/Client owner: append-only codegen, event/snapshot 단일 presentation, full-snapshot reconciliation, timeline rebase. Client는 결과를 재계산하지 않는다.
- P3 Data tooling owner: 위 hardcoded behavior와 integration fixture가 통과한 뒤에만 designer projectile profile로 치환한다. 이 단계는 S018 출하 blocker가 아니라 후속 work packet이다.
- active AI 세션 handoff 전에는 이 계획서 외 파일을 건드리지 않는다. handoff 뒤에도 `GameRoomTick`, `CommandExecutor`, `Snapshot`, `SimLab`, generated pack은 한 세션씩 순차 소유한다.

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h

`GameplayHookContext`의 아래 기존 코드:

```cpp
	eChampion casterChampion = eChampion::NONE;
	u8_t skillRank = 1;
	const SkillDef* pDef = nullptr;
```

아래로 교체:

```cpp
	eChampion casterChampion = eChampion::NONE;
	u8_t skillRank = 1;
	f32_t fPaidManaCost = 0.f;
	const SkillDef* pDef = nullptr;
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`DispatchGameplayHookIfAvailable`의 아래 기존 signature:

```cpp
    bool_t DispatchGameplayHookIfAvailable(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd,
        u32_t hookId,
        eChampion champion,
        u8_t rank)
```

아래로 교체:

```cpp
    bool_t DispatchGameplayHookIfAvailable(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd,
        u32_t hookId,
        eChampion champion,
        u8_t rank,
        f32_t fPaidManaCost)
```

아래 기존 코드:

```cpp
        ctx.casterChampion = champion;
        ctx.skillRank = rank;
        ctx.pDef = &def;
```

아래로 교체:

```cpp
        ctx.casterChampion = champion;
        ctx.skillRank = rank;
        ctx.fPaidManaCost = fPaidManaCost;
        ctx.pDef = &def;
```

`bGameplayHookHandled` 계산의 아래 기존 코드:

```cpp
        DispatchGameplayHookIfAvailable(
            world, tc, resolvedCmd, primaryHookId, hookChampion, rank);
```

아래로 교체:

```cpp
        DispatchGameplayHookIfAvailable(
            world,
            tc,
            resolvedCmd,
            primaryHookId,
            hookChampion,
            rank,
            manaCost);
```

### 1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/EzrealSimComponent.h

아래 기존 코드:

```cpp
    Vec3 vDirection{};
    u64_t uLaunchTick = 0u;
    u8_t uSlot = 0u;
```

아래로 교체:

```cpp
    Vec3 vDirection{};
    u64_t uLaunchTick = 0u;
    f32_t fPaidManaCost = 0.f;
    u8_t uSlot = 0u;
```

`EzrealPendingCastComponent` 바로 위에 추가:

```cpp
inline constexpr u32_t kEzrealRisingSpellForceBuffDefId = 0x455A5001u;
```

패시브 stat owner는 Ezreal 전용 컴포넌트가 아니라 이미 `StatSystem`과 keyframe이 지원하는 범용 `BuffComponent`다. 따라서 이 헤더에는 조회용 stable buff id만 두고 새 stat 경로를 만들지 않는다.

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SkillAtomData.h

`eSkillEffectParamId`의 기존 마지막 값 바로 아래에 append:

```cpp
    CooldownReductionPerRank,
    MaxStacks,
```

기존 practice override와 generated pack이 쓰는 numeric param id를 보존하기 위해 `StackWindowSec` 뒤나 enum 중간에 삽입하지 않는다.

### 1-5. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

`SKILL_EFFECT_PARAM_IDS`의 아래 기존 코드:

```python
    "markDurationSec": "MarkDurationSec",
    "stackWindowSec": "StackWindowSec",
    "gap": "Gap",
```

아래로 교체:

```python
    "markDurationSec": "MarkDurationSec",
    "stackWindowSec": "StackWindowSec",
    "maxStacks": "MaxStacks",
    "gap": "Gap",
```

### 1-6. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

아래 기존 코드:

```json
    {
      "key": "skill.ezreal.basic_attack",
      "params": {
        "speed": 20.0
      }
    },
```

아래로 교체:

```json
    {
      "key": "skill.ezreal.basic_attack",
      "params": {
        "bonusAttackSpeed": 0.10,
        "maxStacks": 5.0,
        "speed": 20.0,
        "stackWindowSec": 6.0
      }
    },
```

삭제할 코드:

```json
        "range": 12.0,
```

삭제 위치는 `skill.ezreal.q`와 `skill.ezreal.w`의 `params`에서 각각 한 번이다.

삭제할 코드:

```json
        "range": 250.0,
```

삭제 위치는 `skill.ezreal.r`의 `params`다. Q/W/R 사거리는 `Data/Gameplay/ChampionGameData/champions.json -> SkillGameplayDefs.range.maximum`만 canonical owner로 남긴다.

### 1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp

아래 기존 include:

```cpp
#include "Shared/GameSim/Components/EzrealSimComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
```

아래 기존 include:

```cpp
#include "Shared/GameSim/Systems/Rune/RuneSystem.h"
```

바로 위에 추가:

```cpp
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
```

`EzrealPendingCastComponent`와 `EzrealEssenceFluxMarkComponent`만 champion-local keyframe registry에 남긴다. `BuffComponent`는 `WorldKeyframe.cpp`의 중앙 registry에 이미 등록되어 있으므로 중복 등록하지 않는다.

삭제할 범위:
`f32_t ResolvePaidManaCost(` 시작부터 바로 다음 `eTeam ResolveSourceTeam(` 직전까지 삭제. 실제 지불량은 command accept 시점의 `GameplayHookContext::fPaidManaCost`만 사용한다.

`IsEzrealProjectile` 바로 아래에 추가:

```cpp
    void RegisterRisingSpellForceHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source)
    {
        if (source == NULL_ENTITY ||
            !world.IsAlive(source) ||
            !world.HasComponent<ChampionComponent>(source) ||
            world.GetComponent<ChampionComponent>(source).id != eChampion::EZREAL)
        {
            return;
        }

        const f32_t fStackWindowSec = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealBasicAttackSlot,
            eSkillEffectParamId::StackWindowSec,
            6.f);
        const f32_t fMaxStacks = ResolveEffectParam(
            world,
            tc,
            source,
            kEzrealBasicAttackSlot,
            eSkillEffectParamId::MaxStacks,
            5.f);
        if (!std::isfinite(fStackWindowSec) ||
            fStackWindowSec <= 0.f ||
            !std::isfinite(fMaxStacks))
        {
            return;
        }
        const u8_t uMaxStacks = static_cast<u8_t>((std::clamp)(
            static_cast<i32_t>(std::lround(fMaxStacks)),
            1,
            255));

        BuffComponent& buffs = world.HasComponent<BuffComponent>(source)
            ? world.GetComponent<BuffComponent>(source)
            : world.AddComponent<BuffComponent>(source, BuffComponent{});

        BuffInstance passive{};
        passive.buffDefId = kEzrealRisingSpellForceBuffDefId;
        passive.source = source;
        passive.fDurationRemaining = (std::max)(0.f, fStackWindowSec);
        passive.uExpireTick =
            tc.tickIndex + SecondsToTicksCeil(fStackWindowSec);
        passive.stackCount = 1u;
        passive.bonusAttackSpeedPerStack = (std::max)(
            0.f,
            ResolveEffectParam(
                world,
                tc,
                source,
                kEzrealBasicAttackSlot,
                eSkillEffectParamId::BonusAttackSpeed,
                0.10f));

        for (u8_t i = 0u;
            i < buffs.count && i < BuffComponent::kMaxBuffs;
            ++i)
        {
            const BuffInstance& existing = buffs.buffs[i];
            if (existing.buffDefId == kEzrealRisingSpellForceBuffDefId &&
                existing.source == source)
            {
                passive.stackCount = static_cast<u8_t>((std::min)(
                    static_cast<u16_t>(existing.stackCount) + 1u,
                    static_cast<u16_t>(uMaxStacks)));
                break;
            }
        }

        if (CBuffSystem::AddOrRefresh(buffs, passive) &&
            world.HasComponent<StatComponent>(source))
        {
            world.GetComponent<StatComponent>(source).bDirty = true;
        }
    }
```

`LaunchMysticShot`의 아래 기존 signature:

```cpp
    void LaunchMysticShot(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& direction)
```

아래로 교체:

```cpp
    void LaunchMysticShot(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& direction,
        f32_t fPaidManaCost)
```

같은 함수의 아래 기존 코드:

```cpp
            true,
            ResolvePaidManaCost(world, tc, source, kEzrealQSlot));
```

아래로 교체:

```cpp
            true,
            fPaidManaCost);
```

`LaunchArcaneShift`의 아래 기존 signature:

```cpp
    void LaunchArcaneShift(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& groundTarget,
        bool_t bHasGroundTarget,
        const Vec3& direction)
```

아래로 교체:

```cpp
    void LaunchArcaneShift(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& groundTarget,
        bool_t bHasGroundTarget,
        const Vec3& direction,
        f32_t fPaidManaCost)
```

같은 함수의 아래 기존 코드:

```cpp
            true,
            ResolvePaidManaCost(world, tc, source, kEzrealESlot));
```

아래로 교체:

```cpp
            true,
            fPaidManaCost);
```

`LaunchTrueshotBarrage`의 아래 기존 signature:

```cpp
    void LaunchTrueshotBarrage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& origin,
        const Vec3& direction)
```

아래로 교체:

```cpp
    void LaunchTrueshotBarrage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        u8_t rank,
        const Vec3& origin,
        const Vec3& direction,
        f32_t fPaidManaCost)
```

같은 함수의 아래 기존 코드:

```cpp
            true,
            ResolvePaidManaCost(world, tc, source, kEzrealRSlot));
```

아래로 교체:

```cpp
            true,
            fPaidManaCost);
```

`QueuePendingCast`의 아래 기존 코드:

```cpp
        pending.uSlot = slot;
        pending.uRank = SanitizeRank(ctx.skillRank);
        pending.bHasGroundTarget = slot == kEzrealESlot;
```

아래로 교체:

```cpp
        pending.uSlot = slot;
        pending.uRank = SanitizeRank(ctx.skillRank);
        pending.fPaidManaCost = ctx.fPaidManaCost;
        pending.bHasGroundTarget = slot == kEzrealESlot;
```

`TickPendingCasts`의 Q/E/R launch 호출을 아래로 교체:

```cpp
            case kEzrealQSlot:
                LaunchMysticShot(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
```

```cpp
            case kEzrealESlot:
                LaunchArcaneShift(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vGroundTarget,
                    pending.bHasGroundTarget,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
```

```cpp
            case kEzrealRSlot:
                LaunchTrueshotBarrage(
                    world,
                    tc,
                    resolvedCaster,
                    pending.uRank,
                    pending.vOrigin,
                    pending.vDirection,
                    pending.fPaidManaCost);
                break;
```

`Tick`에는 패시브 전용 loop를 추가하지 않는다. 1-8B~D의 범용 Buff pre-command prune와 duration advance가 만료·stat dirty를 소유한다.

`HandleProjectileHit`의 아래 기존 코드:

```cpp
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return true;

        if (projectile.kind == eProjectileKind::EssenceFlux)
```

아래로 교체:

```cpp
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return true;

        if (projectile.kind != eProjectileKind::EzrealBasicAttack)
        {
            RegisterRisingSpellForceHit(
                world,
                tc,
                projectile.sourceEntity);
        }

        if (projectile.kind == eProjectileKind::EssenceFlux)
```

아래 기존 코드:

```cpp
        event.effectId = kGlobalEffectFlashBlink;
```

아래로 교체:

```cpp
        event.effectId = kEzrealEffectArcaneShiftBlink;
```

`LaunchArcaneShift`의 아래 기존 코드:

```cpp
        transform.SetPosition(destination);
        transform.m_bLocalDirty = true;
```

아래로 교체:

```cpp
        transform.SetPosition(destination);
        PositionDiscontinuityComponent& discontinuity =
            world.HasComponent<PositionDiscontinuityComponent>(source)
                ? world.GetComponent<PositionDiscontinuityComponent>(source)
                : world.AddComponent<PositionDiscontinuityComponent>(
                    source,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;
        transform.m_bLocalDirty = true;
```

### 1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.cpp

이 파일은 수정하지 않는다. 현재 `BuffComponent` loop가 `bonusAttackSpeedPerStack * stackCount`를 이미 합산한다. Ezreal 전용 include나 분기를 범용 stat system에 추가하지 않는 것이 이 설계의 dependency gate다.

### 1-8A. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/BuffComponent.h

`BuffInstance`의 기존 마지막 필드 바로 아래에 append:

```cpp
    f32_t moveSpeedMul = 1.f;
    u64_t uExpireTick = 0u;
```

`uExpireTick == 0`은 기존 float-duration buff의 backward-compatible runtime 의미이고, 0이 아니면 정수 tick 만료가 canonical owner다. raw keyframe layout은 달라지므로 1-11의 cross-build keyframe 정책을 함께 적용한다.

### 1-8B. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Buff/BuffSystem.h

아래 기존 declaration:

```cpp
    static void Execute(CWorld& world, const TickContext& tc);
```

아래로 교체:

```cpp
    static bool_t PruneExpiredTickBuffs(
        CWorld& world,
        const TickContext& tc);
    static void AdvanceDurationsAfterStat(
        CWorld& world,
        const TickContext& tc);
```

### 1-8C. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Buff/BuffSystem.cpp

include 영역에 추가:

```cpp
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
```

기존 `CBuffSystem::Execute` 전체를 아래 두 함수로 교체:

```cpp
bool_t CBuffSystem::PruneExpiredTickBuffs(
    CWorld& world,
    const TickContext& tc)
{
    bool_t bAnyChanged = false;
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            const BuffInstance buff = component.buffs[uRead];
            if (buff.uExpireTick != 0u &&
                tc.tickIndex >= buff.uExpireTick)
            {
                bChanged = true;
                continue;
            }
            component.buffs[uWrite++] = buff;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
        bAnyChanged = bAnyChanged || bChanged;
    }
    return bAnyChanged;
}

void CBuffSystem::AdvanceDurationsAfterStat(
    CWorld& world,
    const TickContext& tc)
{
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            BuffInstance buff = component.buffs[uRead];
            bool_t bKeep = true;
            if (buff.uExpireTick != 0u)
            {
                const u64_t uRemainingTicks = buff.uExpireTick > tc.tickIndex
                    ? buff.uExpireTick - tc.tickIndex
                    : 0u;
                buff.fDurationRemaining =
                    static_cast<f32_t>(uRemainingTicks) *
                    DeterministicTime::kFixedDt;
                bKeep = uRemainingTicks != 0u;
            }
            else
            {
                if (buff.fDurationRemaining > 0.f)
                    buff.fDurationRemaining -= tc.fDt;
                bKeep = buff.fDurationRemaining > 0.f;
            }

            if (bKeep)
                component.buffs[uWrite++] = buff;
            else
                bChanged = true;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
    }
}
```

정상 Server 경로에서 tick-based expiry 제거 owner는 pre-command `PruneExpiredTickBuffs`다. `AdvanceDurationsAfterStat`의 zero-remaining 제거는 SimLab/local caller가 phase 계약을 어겼을 때 영구 buff를 남기지 않는 안전망이며, Server fixture는 T+180의 phase 진입 전에 해당 buff가 이미 component에서 사라졌는지 검사한다.

### 1-8D. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

`Tick`의 아래 기존 코드:

```cpp
	GameplayStatus::TickStatusEffects(m_world, tc);
	GameplayStatus::TickForcedMotions(m_world, tc);
	Phase_DrainCommands(tc);
```

아래로 교체:

```cpp
	GameplayStatus::TickStatusEffects(m_world, tc);
	GameplayStatus::TickForcedMotions(m_world, tc);
	if (CBuffSystem::PruneExpiredTickBuffs(m_world, tc))
		CStatSystem::Execute(m_world, definitions);
	Phase_DrainCommands(tc);
```

`Phase_SimulationSystems`의 아래 기존 호출:

```cpp
	CBuffSystem::Execute(m_world, tc);
```

아래로 교체:

```cpp
	CBuffSystem::AdvanceDurationsAfterStat(m_world, tc);
```

pre-command prune와 조건부 stat recompute는 player command, bot command, `CombatActionSystem`보다 모두 앞선다. hit tick `T`에 저장한 `T+180` buff는 T+180의 첫 기본공격 action tick 계산부터 제외된다. tick 중 새 buff는 기존 마지막 `CStatSystem`이 반영한다.

### 1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/DamageRequestComponent.h

CONFIRM_NEEDED: `DamageFlag_OnHit`는 기본공격 전용이 아니며 여러 챔피언 스킬이 공유한다. `DamageResult::finalAmount`만 기준으로 receipt를 발행하면 스킬이 Lethal Tempo를 잘못 쌓고, 실드에 전부 흡수된 정상 attack hit는 누락된다. 따라서 S018에서는 `RuneSystem.cpp`, `DamageQueueSystem.cpp`, `CombatActionSystem.cpp`, Ezreal의 기존 Lethal Tempo call을 변경하지 않는다.

별도 HitConfirmed/attack-proc packet에서 아래 계약을 먼저 확정한 뒤 직접 call을 제거한다.

```text
AttackProcPolicy = None | BasicAttack | AppliesBasicAttackOnHit
HitOutcome = Miss | Blocked | Shielded | HealthDamage
GrantsAttackProc는 DamageFlag_OnHit와 별도 bit/typed field
receipt는 source/target generation과 action/projectile execution id 보존
spell shield, ordinary shield, invulnerability의 proc matrix 명시
```

### 1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

`ForcedMotionComponent` 바로 아래에 추가:

```cpp
struct PositionDiscontinuityComponent
{
    u64_t uTick = 0u;
};
```

`StructureProjectileComponent`의 아래 기존 코드:

```cpp
    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    Vec3 currentPos{};
```

아래로 교체:

```cpp
    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    u32_t uProjectileNetAtSpawn = 0u;
    u32_t uSourceNetAtSpawn = 0u;
    u32_t uTargetNetAtSpawn = 0u;
    u16_t uContactOrdinal = 0u;
    Vec3 currentPos{};
    Vec3 direction{ 0.f, 0.f, 1.f };
    f32_t maxDistance = 48.f;
    f32_t traveledDistance = 0.f;
```

skill과 structure projectile 모두 entity destroy/slot reuse 뒤 serializer가 raw local id를 다시 해석하지 않도록 spawn 시점 wire identity를 보존한다.

이 컴포넌트는 위치가 T-1부터 T까지 연속 궤적을 그리지 않은 tick만 표시한다. dash, knockback, airborne arc는 표시하지 않고 Flash, blink, swap, recall/respawn teleport, practice teleport만 표시한다. 오래된 `uTick`은 무해하며 매 tick remove하는 별도 system은 만들지 않는다.

### 1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp

아래 기존 코드:

```cpp
		reg.Register<GameplayStateComponent>("GameplayStateComponent");
```

바로 아래에 추가:

```cpp
		reg.Register<PositionDiscontinuityComponent>(
			"PositionDiscontinuityComponent");
```

keyframe blob은 raw component layout을 사용하므로 이 컴포넌트 추가 뒤 이전 build의 persisted keyframe을 호환 입력으로 간주하지 않는다. 외부 보관 keyframe이 실제 제품 계약이면 format version bump/migration owner를 먼저 확정하고, 아니면 세션 종료 시 폐기하는 정책을 검증에 고정한다.

### 1-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`HandleFlash`의 아래 기존 코드:

```cpp
    transform.SetPosition(dest);
    spells.cooldownRemaining[flashSlot] = cooldown;
```

아래로 교체:

```cpp
    transform.SetPosition(dest);
    PositionDiscontinuityComponent& discontinuity =
        world.HasComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
            ? world.GetComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
            : world.AddComponent<PositionDiscontinuityComponent>(
                cmd.issuerEntity,
                PositionDiscontinuityComponent{});
    discontinuity.uTick = tc.tickIndex;
    spells.cooldownRemaining[flashSlot] = cooldown;
```

RESOLVED 2026-07-13: persistent mobile entity의 순간 위치 변경 call site를 재분류했고 Flash, Ezreal E, Zed W/R, Recall, Yasuo R, Yone return correction에 discontinuity 표식을 연결했다. spawn 초기 배치와 연속 dash/forced motion은 표식 대상에서 제외했다.

```powershell
rg -n "SetPosition\(" Shared/GameSim Server/Private/Game
```

### 1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/SkillProjectileComponent.h

아래 기존 코드:

```cpp
    EntityHandle sourceHandle = NULL_ENTITY_HANDLE;
    EntityHandle targetHandle = NULL_ENTITY_HANDLE;
    eTeam sourceTeam = eTeam::Neutral;
```

아래로 교체:

```cpp
    EntityHandle sourceHandle = NULL_ENTITY_HANDLE;
    EntityHandle targetHandle = NULL_ENTITY_HANDLE;
    u32_t uProjectileNetAtSpawn = 0u;
    u32_t uSourceNetAtSpawn = 0u;
    u32_t uTargetNetAtSpawn = 0u;
    eTeam sourceTeam = eTeam::Neutral;
```

아래 기존 코드:

```cpp
    std::array<EntityID, kMaxPiercingProjectileHits> hitEntities{};
    u16_t hitEntityCount = 0u;
```

아래로 교체:

```cpp
    std::array<EntityHandle, kMaxPiercingProjectileHits> hitEntities{};
    u16_t hitEntityCount = 0u;
    u16_t uContactOrdinal = 0u;
```

`sharedHitLedgerEntity`는 Ashe volley owner까지 함께 바꿔야 하므로 이 슬라이스에서 수기 변경하지 않는다. 해당 필드는 별도 owner가 `EntityHandle` 전환을 맡는다.

### 1-14. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`ILagCompensationQuery`의 아래 기존 코드:

```cpp
    virtual bool_t TryGetHistoricalState(
        EntityID entity,
        u64_t rewindTicks,
        LagCompensatedEntityState& outState) const = 0;
```

아래로 교체:

```cpp
    virtual bool_t TryGetHistoricalStateAtTick(
        EntityHandle hEntity,
        u64_t uExpectedTick,
        LagCompensatedEntityState& outState) const = 0;
```

`rewindTicks=0` 같은 상대 의미를 projectile CCD에 재사용하지 않는다. 호출자가 필요한 절대 T-1을 명시하고 history gap은 성공으로 보정하지 않는다.

### 1-15. C:/Users/user/Desktop/Winters/Server/Public/Security/LagCompensation.h

아래 기존 코드:

```cpp
	bool_t TryGetHistoricalState(
		EntityID entity,
		u64_t rewindTicks,
		LagCompensatedEntityState& outState) const override;
```

아래로 교체:

```cpp
	bool_t TryGetHistoricalStateAtTick(
		EntityHandle hEntity,
		u64_t uExpectedTick,
		LagCompensatedEntityState& outState) const override;
```

### 1-16. C:/Users/user/Desktop/Winters/Server/Private/Security/LagCompensation.cpp

`TryGetHistoricalState` 전체를 아래로 교체:

```cpp
bool_t CLagCompensation::TryGetHistoricalStateAtTick(
    EntityHandle hEntity,
    u64_t uExpectedTick,
    LagCompensatedEntityState& outState) const
{
    if (!hEntity.IsValid() || uExpectedTick > m_latestTick)
        return false;

    const auto it = m_history.find(hEntity.GetIndex());
    if (it == m_history.end())
        return false;

    for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit)
    {
        if (rit->generation != hEntity.GetGeneration())
            continue;
        if (rit->tickIndex == uExpectedTick)
        {
            outState = rit->state;
            return true;
        }
        if (rit->tickIndex < uExpectedTick)
            break;
    }

    return false;
}
```

### 1-16A. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

Chrono Break restore의 아래 기존 코드:

```cpp
    if (m_pLagCompensation)
        m_pLagCompensation->Reset();
```

아래로 교체:

```cpp
    if (m_pLagCompensation)
    {
        m_pLagCompensation->Reset();
        m_pLagCompensation->RecordHistory(m_world, restoredTick);
    }
```

restore 뒤 첫 resume tick은 `restoredTick + 1`이므로 이 seed가 정확한 T-1 sample이 된다. history gap에서는 current-position fallback을 쓰되 과거 T-2를 연속 궤적으로 연결하지 않는다.

### 1-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ReplicatedEventComponent.h

`eKillFeedObjectKind` 바로 아래에 추가:

```cpp
enum class ProjectileContactReason : u8_t
{
    None = 0,
    UnitHit,
    Barrier,
    Terrain,
    RangeExpired,
    SourceInvalid,
    TargetInvalid,
    InvalidTrajectory,
    HitLimit,
};
```

아래 기존 상수:

```cpp
inline constexpr u32_t kGlobalEffectFlashBlink = 0xF1A50001u;
inline constexpr u32_t kEzrealEffectEssenceFluxMark = 0x455A5701u;
```

아래로 교체:

```cpp
inline constexpr u32_t kGlobalEffectFlashBlink = 0xF1A50001u;
inline constexpr u32_t kEzrealEffectArcaneShiftBlink = 0x455A4501u;
inline constexpr u32_t kEzrealEffectEssenceFluxMark = 0x455A5701u;
```

아래 기존 코드:

```cpp
    u32_t sourceNetOverride = 0u;
    u32_t targetNetOverride = 0u;
```

아래로 교체:

```cpp
    u32_t sourceNetOverride = 0u;
    u32_t targetNetOverride = 0u;
    u32_t projectileNetOverride = 0u;
```

아래 기존 코드:

```cpp
    u16_t durationMs = 0;
    u16_t flags = 0;

    u8_t slot = 0;
```

아래로 교체:

```cpp
    u16_t durationMs = 0;
    u16_t flags = 0;
    u16_t uContactOrdinal = 0u;

    u8_t slot = 0;
```

아래 기존 코드:

```cpp
    eDamageType damageType = eDamageType::Physical;
    bool_t bWasCrit = false;
```

아래로 교체:

```cpp
    eDamageType damageType = eDamageType::Physical;
    ProjectileContactReason eContactReason = ProjectileContactReason::None;
    bool_t bWasCrit = false;
```

### 1-18. C:/Users/user/Desktop/Winters/Shared/Schemas/Event.fbs

`ProjectileSpawnEvent` 바로 위에 추가:

```text
enum ProjectileContactReason : ubyte {
    None = 0,
    UnitHit = 1,
    Barrier = 2,
    Terrain = 3,
    RangeExpired = 4,
    SourceInvalid = 5,
    TargetInvalid = 6,
    InvalidTrajectory = 7,
    HitLimit = 8
}
```

`ProjectileSpawnEvent`의 기존 마지막 필드 바로 아래에 append:

```text
    targetNet:uint;
```

기준 기존 table 끝:

```text
    speed:float;
    maxDist:float;
}
```

`ProjectileHitEvent`의 기존 마지막 필드 바로 아래에 append:

```text
    posZ:float;
    bDestroyed:bool;
    contactReason:ProjectileContactReason = None;
    contactOrdinal:ushort;
}
```

`EventPacket`의 기존 마지막 필드 `killFeed` 바로 아래에 append:

```text
    eventOrdinal:uint;
```

FlatBuffers field id 호환성을 위해 새 필드를 table 중간에 삽입하지 않는다. 기존 `netId`부터 `maxDist`, `netId`부터 `bDestroyed`, `kind`부터 `killFeed`의 순서와 id는 그대로 보존한다. old wire의 ordinal은 0이다.

### 1-19. C:/Users/user/Desktop/Winters/Server/Public/Game/ServerProjectileAuthority.h

`class CWorld;` 바로 아래에 추가:

```cpp
struct ILagCompensationQuery;
```

`FindSkillProjectileHitTarget` signature의 `const SkillProjectileComponent& projectile,` 바로 아래에 추가:

```cpp
        const ILagCompensationQuery* pLagCompensation,
        u64_t uCurrentTick,
```

`FindTargetedProjectileHit` signature의 `const SkillProjectileComponent& projectile,` 바로 아래에 추가:

```cpp
        const ILagCompensationQuery* pLagCompensation,
        u64_t uCurrentTick,
```

`BuildProjectileHitEvent`의 아래 기존 signature:

```cpp
    static ReplicatedEventComponent BuildProjectileHitEvent(
        EntityID sourceEntity,
        EntityID targetEntity,
        EntityID projectileEntity,
        u16_t projectileKind,
        const Vec3& position,
        u64_t startTick,
        bool_t bDestroyed = true);
```

아래로 교체:

```cpp
    static ReplicatedEventComponent BuildProjectileHitEvent(
        EntityID sourceEntity,
        EntityID targetEntity,
        EntityID projectileEntity,
        u16_t projectileKind,
        const Vec3& position,
        u64_t startTick,
        ProjectileContactReason eContactReason,
        u16_t uContactOrdinal,
        bool_t bDestroyed = true);

    static u32_t QuantizeContactT(f32_t fContactT);
    static f32_t DequantizeContactT(u32_t uContactT);
```

### 1-20. C:/Users/user/Desktop/Winters/Server/Private/Game/ServerProjectileAuthority.cpp

아래 기존 include 바로 아래에 추가:

```cpp
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

기준 기존 코드:

```cpp
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
```

`HasAlreadyHit` 전체를 아래로 교체:

```cpp
    bool_t HasAlreadyHit(
        CWorld& world,
        const SkillProjectileComponent& projectile,
        EntityID target)
    {
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hTarget.IsValid())
            return false;

        const u16_t count = (std::min)(
            projectile.hitEntityCount,
            kMaxPiercingProjectileHits);
        for (u16_t i = 0u; i < count; ++i)
        {
            if (projectile.hitEntities[i] == hTarget)
                return true;
        }
        return false;
    }
```

`FindSweptCircleEntryXZ` 바로 아래에 추가:

```cpp
    Vec3 ResolvePreviousTargetPosition(
        CWorld& world,
        EntityID target,
        const ILagCompensationQuery* pLagCompensation,
        u64_t uCurrentTick,
        const Vec3& currentPosition)
    {
        if (!pLagCompensation || uCurrentTick == 0u)
            return currentPosition;

        if (world.HasComponent<PositionDiscontinuityComponent>(target) &&
            world.GetComponent<PositionDiscontinuityComponent>(target).uTick ==
                uCurrentTick)
        {
            return currentPosition;
        }

        const EntityHandle hTarget = world.GetEntityHandle(target);
        LagCompensatedEntityState previous{};
        if (!hTarget.IsValid() ||
            !pLagCompensation->TryGetHistoricalStateAtTick(
                hTarget,
                uCurrentTick - 1u,
                previous))
        {
            return currentPosition;
        }
        if (!std::isfinite(previous.vPosition.x) ||
            !std::isfinite(previous.vPosition.y) ||
            !std::isfinite(previous.vPosition.z))
        {
            return currentPosition;
        }
        return previous.vPosition;
    }
```

`IsMinionRangedProjectileKind` 바로 위에 추가:

```cpp
u32_t CServerProjectileAuthority::QuantizeContactT(f32_t fContactT)
{
    constexpr f32_t kContactTScale = 1048576.f;
    if (!std::isfinite(fContactT))
        return static_cast<u32_t>(kContactTScale);
    const f32_t fClamped = (std::clamp)(fContactT, 0.f, 1.f);
    return static_cast<u32_t>(std::floor(fClamped * kContactTScale + 0.5f));
}

f32_t CServerProjectileAuthority::DequantizeContactT(u32_t uContactT)
{
    constexpr f32_t kContactTScale = 1048576.f;
    const u32_t uClamped = (std::min)(
        uContactT,
        static_cast<u32_t>(kContactTScale));
    return static_cast<f32_t>(uClamped) / kContactTScale;
}
```

`FindSkillProjectileHitTarget` signature에 `const ILagCompensationQuery* pLagCompensation, u64_t uCurrentTick`을 추가한 뒤, 아래 기존 코드:

```cpp
                    HasAlreadyHit(projectile, entity))
```

아래로 교체:

```cpp
                    HasAlreadyHit(world, projectile, entity))
```

같은 함수의 아래 기존 코드:

```cpp
                const Vec3 targetPos = transform.GetPosition();
                const f32_t projectileRadius = bYasuoTornado
                    ? std::max(projectile.hitRadius, 2.25f)
                    : projectile.hitRadius;
                const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                f32_t t = 0.f;
                if (!FindSweptCircleEntryXZ(start, end, targetPos, radius, t))
                    return;
                const bool_t bCloser =
                    bestTarget == NULL_ENTITY ||
                    t < bestT ||
                    (t == bestT && entity < bestTarget);
```

아래로 교체:

```cpp
                const Vec3 targetPos = transform.GetPosition();
                if (!std::isfinite(targetPos.x) ||
                    !std::isfinite(targetPos.y) ||
                    !std::isfinite(targetPos.z))
                {
                    return;
                }
                const Vec3 previousTargetPos = ResolvePreviousTargetPosition(
                    world,
                    entity,
                    pLagCompensation,
                    uCurrentTick,
                    targetPos);
                const f32_t projectileRadius = bYasuoTornado
                    ? std::max(projectile.hitRadius, 2.25f)
                    : projectile.hitRadius;
                const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                const Vec3 relativeStart{
                    start.x - previousTargetPos.x,
                    start.y - previousTargetPos.y,
                    start.z - previousTargetPos.z };
                const Vec3 relativeEnd{
                    end.x - targetPos.x,
                    end.y - targetPos.y,
                    end.z - targetPos.z };
                f32_t t = 0.f;
                if (!FindSweptCircleEntryXZ(relativeStart, relativeEnd, Vec3{}, radius, t))
                    return;

                const u32_t uCandidateT = QuantizeContactT(t);
                const u32_t uBestT = QuantizeContactT(bestT);
                const EntityHandle hCandidate = world.GetEntityHandle(entity);
                const EntityHandle hBest = world.GetEntityHandle(bestTarget);
                const bool_t bCloser =
                    bestTarget == NULL_ENTITY ||
                    uCandidateT < uBestT ||
                    (uCandidateT == uBestT && hCandidate.ToU64() < hBest.ToU64());
```

`FindProjectileBarrierHit`의 아래 기존 비교:

```cpp
                if (!bHit || t < bestT || (t == bestT && entity < bestBarrier))
```

아래로 교체:

```cpp
                const u32_t uCandidateT = QuantizeContactT(t);
                const u32_t uBestT = QuantizeContactT(bestT);
                const EntityHandle hCandidate = world.GetEntityHandle(entity);
                const EntityHandle hBest = world.GetEntityHandle(bestBarrier);
                if (!bHit ||
                    uCandidateT < uBestT ||
                    (uCandidateT == uBestT && hCandidate.ToU64() < hBest.ToU64()))
```

`FindTargetedProjectileHit` signature에 `const ILagCompensationQuery* pLagCompensation, u64_t uCurrentTick`을 추가한 뒤 아래 기존 범위:

```cpp
    const Vec3 targetPos =
        world.GetComponent<TransformComponent>(targetEntity).GetPosition();
    const f32_t radius = (std::max)(0.f, projectile.hitRadius) +
        ResolveCombatRadius(world, targetEntity);
    f32_t t = 0.f;
    if (!FindSweptCircleEntryXZ(start, end, targetPos, radius, t))
        return false;
```

아래로 교체:

```cpp
    const Vec3 targetPos =
        world.GetComponent<TransformComponent>(targetEntity).GetPosition();
    if (!std::isfinite(targetPos.x) ||
        !std::isfinite(targetPos.y) ||
        !std::isfinite(targetPos.z))
    {
        return false;
    }
    const Vec3 previousTargetPos = ResolvePreviousTargetPosition(
        world,
        targetEntity,
        pLagCompensation,
        uCurrentTick,
        targetPos);
    const f32_t radius = (std::max)(0.f, projectile.hitRadius) +
        ResolveCombatRadius(world, targetEntity);
    const Vec3 relativeStart{
        start.x - previousTargetPos.x,
        start.y - previousTargetPos.y,
        start.z - previousTargetPos.z };
    const Vec3 relativeEnd{
        end.x - targetPos.x,
        end.y - targetPos.y,
        end.z - targetPos.z };
    f32_t t = 0.f;
    if (!FindSweptCircleEntryXZ(relativeStart, relativeEnd, Vec3{}, radius, t))
        return false;
```

candidate가 선택될 때 `bestT` 또는 `outT`에는 raw `t`를 저장하지 않고 `DequantizeContactT(QuantizeContactT(t))`를 저장한다. hit position도 이 dequantized TOI로 계산해 동일한 ordered contact가 wire position hash까지 같게 한다.

`BuildProjectileHitEvent` 전체를 아래로 교체:

```cpp
ReplicatedEventComponent CServerProjectileAuthority::BuildProjectileHitEvent(
    EntityID sourceEntity,
    EntityID targetEntity,
    EntityID projectileEntity,
    u16_t projectileKind,
    const Vec3& position,
    u64_t startTick,
    ProjectileContactReason eContactReason,
    u16_t uContactOrdinal,
    bool_t bDestroyed)
{
    ReplicatedEventComponent event{};
    event.kind = eReplicatedEventKind::ProjectileHit;
    event.sourceEntity = sourceEntity;
    event.targetEntity = targetEntity;
    event.projectileEntity = projectileEntity;
    event.projectileKind = projectileKind;
    event.position = position;
    event.eContactReason = eContactReason;
    event.uContactOrdinal = uContactOrdinal;
    event.bDestroyed = bDestroyed;
    event.startTick = startTick;
    return event;
}
```

### 1-21. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomProjectiles.cpp

`StructureProjectileComponent` loop의 `currentProjectileNet` 계산 바로 아래에 추가:

```cpp
        auto EnqueueStructureContact =
            [&](EntityID target,
                const Vec3& position,
                ProjectileContactReason eReason,
                bool_t bDestroyed)
            {
                ReplicatedEventComponent event =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        target,
                        entity,
                        CServerProjectileAuthority::kStructureProjectileKind,
                        position,
                        tc.tickIndex,
                        eReason,
                        projectile.uContactOrdinal++,
                        bDestroyed);
                event.projectileNetOverride = projectile.uProjectileNetAtSpawn;
                event.sourceNetOverride = projectile.uSourceNetAtSpawn;
                event.targetNetOverride = target != NULL_ENTITY
                    ? m_entityMap.ToNet(target)
                    : (eReason == ProjectileContactReason::TargetInvalid
                        ? projectile.uTargetNetAtSpawn
                        : NULL_NET_ENTITY);
                EnqueueReplicatedEvent(m_world, event);
                if (bDestroyed &&
                    event.projectileNetOverride != NULL_NET_ENTITY)
                {
                    m_entityMap.Unbind(event.projectileNetOverride);
                }
            };
```

structure spawn에서 `IssueNew` 직후에 추가:

```cpp
            projectile.uProjectileNetAtSpawn = projectileNet;
            projectile.uSourceNetAtSpawn =
                m_entityMap.ToNet(projectile.sourceEntity);
            projectile.uTargetNetAtSpawn =
                m_entityMap.ToNet(projectile.targetEntity);
```

structure spawn event는 `const`를 제거하고 enqueue 전에 아래 override를 설정한다:

```cpp
            spawn.projectileNetOverride = projectile.uProjectileNetAtSpawn;
            spawn.sourceNetOverride = projectile.uSourceNetAtSpawn;
            spawn.targetNetOverride = projectile.uTargetNetAtSpawn;
```

spawn에서 `dir = NormalizeXZOrForward(...)` 직후에 추가:

```cpp
            projectile.direction = dir;
```

`BuildProjectileSpawnEvent`의 hardcoded `48.f` 인자는 아래로 교체:

```cpp
                    projectile.maxDistance,
```

structure spawn 뒤 이동 계산 전 추가:

```cpp
        if (!std::isfinite(projectile.currentPos.x) ||
            !std::isfinite(projectile.currentPos.y) ||
            !std::isfinite(projectile.currentPos.z) ||
            !std::isfinite(projectile.speed) ||
            !std::isfinite(projectile.maxDistance) ||
            !std::isfinite(projectile.traveledDistance) ||
            !std::isfinite(projectile.hitRadius) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f ||
            projectile.traveledDistance < 0.f ||
            projectile.hitRadius < 0.f)
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::InvalidTrajectory,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }
```

structure target loss와 hit의 기존 `BuildProjectileHitEvent` block은 각각 `EnqueueStructureContact(..., TargetInvalid, true)`, `EnqueueStructureContact(..., UnitHit, true)`로 치환한다. 이 경로도 terminal event serialize/unbind까지 저장된 projectile Net ID를 사용한다.

structure 이동의 아래 기존 코드:

```cpp
        const f32_t dist = std::sqrt(distSq);
        if (dist <= std::numeric_limits<f32_t>::epsilon())
        {
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t step = projectile.speed * tc.fDt;
        const f32_t t = (step >= dist) ? 1.f : (step / dist);
        Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t
        };
        projectile.currentPos = next;
        transform.SetPosition(next);
```

아래로 교체:

```cpp
        const f32_t dist = std::sqrt(distSq);
        const f32_t remaining =
            projectile.maxDistance - projectile.traveledDistance;
        if (dist <= std::numeric_limits<f32_t>::epsilon() ||
            remaining <= 0.f)
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                pos,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t fInvDistance = 1.f / dist;
        projectile.direction = Vec3{
            delta.x * fInvDistance,
            delta.y * fInvDistance,
            delta.z * fInvDistance };
        const f32_t step = (std::min)(projectile.speed * tc.fDt, remaining);
        const f32_t actualStep = (std::min)(step, dist);
        const f32_t t = actualStep / dist;
        const Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t };
        projectile.currentPos = next;
        projectile.traveledDistance += actualStep;
        transform.SetPosition(next);

        if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            EnqueueStructureContact(
                NULL_ENTITY,
                next,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
        }
```

`SkillProjectileComponent`와 `TransformComponent`를 얻은 직후에 추가:

```cpp
        auto EnqueueProjectileContact =
            [&](EntityID target,
                const Vec3& position,
                ProjectileContactReason eReason,
                bool_t bDestroyed)
            {
                ReplicatedEventComponent event =
                    CServerProjectileAuthority::BuildProjectileHitEvent(
                        projectile.sourceEntity,
                        target,
                        entity,
                        static_cast<u16_t>(projectile.kind),
                        position,
                        tc.tickIndex,
                        eReason,
                        projectile.uContactOrdinal++,
                        bDestroyed);
                event.sourceNetOverride = projectile.uSourceNetAtSpawn;
                event.projectileNetOverride = projectile.uProjectileNetAtSpawn;
                event.targetNetOverride = target != NULL_ENTITY
                    ? m_entityMap.ToNet(target)
                    : (eReason == ProjectileContactReason::TargetInvalid
                        ? projectile.uTargetNetAtSpawn
                        : NULL_NET_ENTITY);
                EnqueueReplicatedEvent(m_world, event);
                if (bDestroyed &&
                    event.projectileNetOverride != NULL_NET_ENTITY)
                {
                    m_entityMap.Unbind(event.projectileNetOverride);
                }
            };
```

terminal helper는 contact payload에 Net override를 복사한 다음 local projectile entity를 destroy하기 전에 즉시 `EntityIdMap::Unbind`한다. 그래야 BroadcastEvents 전 raw ECS slot이 재사용되어도 새 entity가 old projectile Net ID를 상속하지 않는다. serializer가 반환하는 delayed unbind는 이미 해제된 ID에 대한 idempotent no-op이어야 한다.

아래 기존 spawn 초기화 코드:

```cpp
            projectile.bSpawned = true;

            const ReplicatedEventComponent spawn =
```

아래로 교체:

```cpp
            projectile.bSpawned = true;
            projectile.uProjectileNetAtSpawn = projectileNet;
            projectile.uSourceNetAtSpawn = m_entityMap.ToNet(projectile.sourceEntity);
            projectile.uTargetNetAtSpawn = m_entityMap.ToNet(projectile.targetEntity);

            ReplicatedEventComponent spawn =
```

`spawn` 생성 직후, enqueue 전에 추가:

```cpp
            spawn.projectileNetOverride = projectile.uProjectileNetAtSpawn;
            spawn.sourceNetOverride = projectile.uSourceNetAtSpawn;
            spawn.targetNetOverride = projectile.uTargetNetAtSpawn;
```

source handle resolve 실패 뒤 아래 기존 코드:

```cpp
                if (!projectile.bPersistAfterSourceDeath)
                {
                    m_world.DestroyEntity(entity);
                    continue;
                }
```

아래로 교체:

```cpp
                if (!projectile.bPersistAfterSourceDeath)
                {
                    if (projectile.bSpawned)
                    {
                        EnqueueProjectileContact(
                            NULL_ENTITY,
                            projectile.currentPos,
                            ProjectileContactReason::SourceInvalid,
                            true);
                    }
                    m_world.DestroyEntity(entity);
                    continue;
                }
```

아래 기존 합류 조건:

```cpp
        if ((!projectile.bPersistAfterSourceDeath &&
                !IsAliveHealth(m_world, projectile.sourceEntity)) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f)
```

source health 종료와 definition 오류가 서로 다른 reason을 내도록 두 block으로 분리한다:

```cpp
        if (!projectile.bPersistAfterSourceDeath &&
            !IsAliveHealth(m_world, projectile.sourceEntity))
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::SourceInvalid,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t fDirectionLengthSq =
            projectile.direction.x * projectile.direction.x +
            projectile.direction.y * projectile.direction.y +
            projectile.direction.z * projectile.direction.z;
        const bool_t bFiniteTrajectory =
            std::isfinite(projectile.currentPos.x) &&
            std::isfinite(projectile.currentPos.y) &&
            std::isfinite(projectile.currentPos.z) &&
            std::isfinite(projectile.direction.x) &&
            std::isfinite(projectile.direction.y) &&
            std::isfinite(projectile.direction.z) &&
            std::isfinite(projectile.speed) &&
            std::isfinite(projectile.maxDistance) &&
            std::isfinite(projectile.traveledDistance) &&
            std::isfinite(projectile.hitRadius);
        const bool_t bLinearDirectionInvalid =
            projectile.targetEntity == NULL_ENTITY &&
            fDirectionLengthSq <= std::numeric_limits<f32_t>::epsilon();
        if (!bFiniteTrajectory ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f ||
            projectile.traveledDistance < 0.f ||
            projectile.hitRadius < 0.f ||
            bLinearDirectionInvalid)
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                projectile.currentPos,
                ProjectileContactReason::InvalidTrajectory,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }
```

targeted projectile의 `next` 계산 직후, `FindTargetedProjectileHit` 호출 전에 추가:

```cpp
            if (actualStep > 0.f &&
                dist > std::numeric_limits<f32_t>::epsilon())
            {
                const f32_t fInvDistance = 1.f / dist;
                projectile.direction = Vec3{
                    delta.x * fInvDistance,
                    delta.y * fInvDistance,
                    delta.z * fInvDistance };
            }
```

BA/E homing의 snapshot 방향은 cast-time 방향이 아니라 이 tick의 실제 velocity 방향을 소유한다.

targeted branch의 `targetPos` 계산 직후에 추가:

```cpp
            if (!std::isfinite(targetPos.x) ||
                !std::isfinite(targetPos.y) ||
                !std::isfinite(targetPos.z))
            {
                EnqueueProjectileContact(
                    NULL_ENTITY,
                    projectile.currentPos,
                    ProjectileContactReason::TargetInvalid,
                    true);
                m_world.DestroyEntity(entity);
                continue;
            }
```

NaN target transform이 `dist`와 `actualStep`을 오염시켜 영구 projectile을 만들지 않게 한다.

target handle resolve 실패, target health/state 무효, speed/maxDistance 무효, barrier, terrain, range, hit-cap, unit-hit 분기는 각각 기존 `BuildProjectileHitEvent` 블록을 아래 단일 호출 중 해당 이유로 교체한다:

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    projectile.currentPos,
    ProjectileContactReason::TargetInvalid,
    true);
```

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    projectile.currentPos,
    ProjectileContactReason::InvalidTrajectory,
    true);
```

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    barrierHitPos,
    ProjectileContactReason::Barrier,
    true);
```

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    projectile.currentPos,
    ProjectileContactReason::Terrain,
    true);
```

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    projectile.currentPos,
    ProjectileContactReason::RangeExpired,
    true);
```

```cpp
EnqueueProjectileContact(
    NULL_ENTITY,
    hitPos,
    ProjectileContactReason::HitLimit,
    true);
```

```cpp
EnqueueProjectileContact(
    target,
    hitPos,
    ProjectileContactReason::UnitHit,
    !bPiercingProjectile);
```

targeted projectile의 unit hit block은 위 linear 변수명을 사용하지 않고 아래 호출로 교체한다:

```cpp
EnqueueProjectileContact(
    projectile.targetEntity,
    targetHitPos,
    ProjectileContactReason::UnitHit,
    true);
```

마지막 `bBlockedByNavigation || traveledDistance` 합류 분기는 아래처럼 이유를 분리한다:

```cpp
            EnqueueProjectileContact(
                NULL_ENTITY,
                end,
                bBlockedByNavigation
                    ? ProjectileContactReason::Terrain
                    : ProjectileContactReason::RangeExpired,
                true);
```

두 `FindTargetedProjectileHit` / `FindSkillProjectileHitTarget` 호출에는 `tc.pLagCompensation`, `tc.tickIndex`를 `projectile` 바로 다음 인자로 전달한다.

두 barrier-vs-target 비교의 아래 기존 형태:

```cpp
            const bool_t bBarrierFirst = bBarrierHit &&
                (!bTargetHit || barrierHitT <= targetHitT);
```

및

```cpp
            const bool_t bBarrierFirst = bBarrierHit &&
                (target == NULL_ENTITY || barrierHitT <= targetHitT);
```

아래처럼 quantized TOI 비교로 교체한다:

```cpp
            const bool_t bBarrierFirst = bBarrierHit &&
                (!bTargetHit ||
                    CServerProjectileAuthority::QuantizeContactT(barrierHitT) <=
                    CServerProjectileAuthority::QuantizeContactT(targetHitT));
```

```cpp
            const bool_t bBarrierFirst = bBarrierHit &&
                (target == NULL_ENTITY ||
                    CServerProjectileAuthority::QuantizeContactT(barrierHitT) <=
                    CServerProjectileAuthority::QuantizeContactT(targetHitT));
```

아래 기존 코드:

```cpp
                projectile.hitEntities[projectile.hitEntityCount++] = target;
```

아래로 교체:

```cpp
                projectile.hitEntities[projectile.hitEntityCount++] =
                    m_world.GetEntityHandle(target);
```

linear projectile branch의 아래 기존 코드:

```cpp
        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
```

아래로 교체:

```cpp
        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        if (remaining <= 0.f)
        {
            EnqueueProjectileContact(
                NULL_ENTITY,
                start,
                ProjectileContactReason::RangeExpired,
                true);
            m_world.DestroyEntity(entity);
            continue;
        }
        const f32_t step = (std::min)(projectile.speed * tc.fDt, remaining);
```

`StructureProjectileComponent` 분기의 contact도 `ProjectileContactReason::TargetInvalid`, `ProjectileContactReason::UnitHit`, `ProjectileContactReason::InvalidTrajectory`와 저장된 `projectile.uContactOrdinal++`을 사용한다.

RESOLVED 2026-07-13: 최신 파일에서 `BuildProjectileHitEvent`를 다시 조사해 선언·정의와 structure/skill terminal helper의 네 anchor로 중앙화했고, typed reason/ordinal 없는 terminal call을 제거했다.

### 1-22. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp

`CReplicatedEventSerializer::Build` signature의 `u64_t serverTick` 바로 아래에 추가:

```cpp
        u32_t uEventOrdinal,
```

ProjectileSpawn 분기의 아래 기존 코드:

```cpp
            NetEntityId projectileNet = NULL_NET_ENTITY;
            if (event.projectileEntity != NULL_ENTITY &&
                world.IsAlive(event.projectileEntity))
            {
                projectileNet = entityMap.IssueNew(event.projectileEntity);
            }
```

아래로 교체:

```cpp
            NetEntityId projectileNet = event.projectileNetOverride;
            if (projectileNet == NULL_NET_ENTITY &&
                event.projectileEntity != NULL_ENTITY &&
                world.IsAlive(event.projectileEntity))
            {
                projectileNet = entityMap.IssueNew(event.projectileEntity);
            }
```

ProjectileSpawn 분기의 아래 기존 마지막 인자:

```cpp
                event.direction.z,
                event.speed,
                event.maxDistance);
```

아래로 교체:

```cpp
                event.direction.z,
                event.speed,
                event.maxDistance,
                event.targetNetOverride != NULL_NET_ENTITY
                    ? event.targetNetOverride
                    : entityMap.ToNet(event.targetEntity));
```

같은 분기의 owner 인자는 아래로 교체:

```cpp
                event.sourceNetOverride != NULL_NET_ENTITY
                    ? event.sourceNetOverride
                    : entityMap.ToNet(event.sourceEntity),
```

ProjectileHit 분기의 아래 기존 인자:

```cpp
            const NetEntityId projectileNet =
                entityMap.ToNet(event.projectileEntity);
```

아래로 교체:

```cpp
            const NetEntityId projectileNet =
                event.projectileNetOverride != NULL_NET_ENTITY
                    ? event.projectileNetOverride
                    : entityMap.ToNet(event.projectileEntity);
```

ProjectileHit 분기의 아래 기존 인자:

```cpp
                entityMap.ToNet(event.sourceEntity),
                entityMap.ToNet(event.targetEntity),
                event.projectileKind,
                event.position.x,
                event.position.y,
                event.position.z,
                event.bDestroyed);
```

아래로 교체:

```cpp
                event.sourceNetOverride != NULL_NET_ENTITY
                    ? event.sourceNetOverride
                    : entityMap.ToNet(event.sourceEntity),
                event.targetNetOverride != NULL_NET_ENTITY
                    ? event.targetNetOverride
                    : entityMap.ToNet(event.targetEntity),
                event.projectileKind,
                event.position.x,
                event.position.y,
                event.position.z,
                event.bDestroyed,
                static_cast<Shared::Schema::ProjectileContactReason>(
                    static_cast<u8_t>(event.eContactReason)),
                event.uContactOrdinal);
```

RESOLVED 2026-07-13: Event schema codegen 뒤 serializer의 persistent-state mutation event에 `uEventOrdinal`을 전달했고, `BuildActionStart`는 기본 ordinal 0을 유지한다. append-only field ID와 omitted-tail default contract를 독립 probe로 고정했다.

```powershell
rg -n "CreateEventPacket\(" Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp
```

### 1-22A. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h

아래 기존 signature:

```cpp
        static bool_t Build(
            CWorld& world,
            EntityIdMap& entityMap,
            const ReplicatedEventComponent& event,
            u64_t serverTick,
            SerializedReplicatedEvent& out);
```

아래로 교체:

```cpp
        static bool_t Build(
            CWorld& world,
            EntityIdMap& entityMap,
            const ReplicatedEventComponent& event,
            u64_t serverTick,
            u32_t uEventOrdinal,
            SerializedReplicatedEvent& out);
```

### 1-22B. C:/Users/user/Desktop/Winters/Server/Public/Game/ReplicationEmitter.h

`TryBuildReplicatedEvent` signature의 `u64_t serverTick` 바로 아래에 추가:

```cpp
        u32_t uEventOrdinal,
```

### 1-22C. C:/Users/user/Desktop/Winters/Server/Private/Game/ReplicationEmitter.cpp

`TryBuildReplicatedEvent` definition의 `u64_t serverTick` 바로 아래에 추가:

```cpp
    u32_t uEventOrdinal,
```

같은 함수의 아래 기존 호출:

```cpp
        event,
        serverTick,
        outSerialized);
```

아래로 교체:

```cpp
        event,
        serverTick,
        uEventOrdinal,
        outSerialized);
```

### 1-22D. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

아래 기존 호출:

```cpp
            entity,
            tc.tickIndex,
            serialized))
```

아래로 교체:

```cpp
            entity,
            tc.tickIndex,
            static_cast<u32_t>(entity),
            serialized))
```

replicated event entity는 broadcast 전까지 동시에 alive이고 `DeterministicEntityIterator`가 raw `EntityID` 오름차순으로 수집하므로 같은 tick에서 이 값이 unique broadcast-order tie-breaker다. client stamp는 `(tick, phase, ordinal)` 순으로 비교해 terminal/clear가 spawn/mark보다, snapshot truth가 모든 event보다 우선하고 같은 phase에서만 ordinal을 쓴다.

### 1-23. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`EntitySnapshot` table의 기존 마지막 필드 `forcedMotionRemainingSec` 바로 아래에 append:

```text
    projectileDirX:float;
    projectileDirY:float;
    projectileDirZ:float;
    projectileTraveledDist:float;
```

`Snapshot` table 바로 위에 추가:

```text
enum GameplayStateKind : ushort {
    None = 0,
    EzrealRisingSpellForce = 1,
    EzrealEssenceFlux = 2,
    YasuoWindWall = 3
}

table GameplayStateSnapshot {
    kind:GameplayStateKind;
    sourceNet:uint;
    targetNet:uint;
    startTick:ulong;
    expireTick:ulong;
    stackCount:ushort;
    rank:ubyte;
    flags:uint;
    posX:float;
    posY:float;
    posZ:float;
    dirX:float;
    dirY:float;
    dirZ:float;
    magnitude0:float;
    magnitude1:float;
}
```

`Snapshot` table의 실제 기존 마지막 필드 `simSpeedMul` 바로 아래에 append:

```text
    gameplayStates:[GameplayStateSnapshot];
```

기존 `EntitySnapshot`의 `netId`부터 `forcedMotionRemainingSec`, 기존 `Snapshot`의 `serverTick`부터 `simSpeedMul`까지 field 순서와 id는 절대 바꾸지 않는다. 특히 S017이 append한 `timelineEpoch`, `branchId`, `toolRevision`, `simPaused`, `simSpeedMul` 사이에 새 필드를 끼우지 않는다.

### 1-24. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

아래 include를 추가:

```cpp
#include "Game/ServerProjectileAuthority.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
```

`StructureProjectileComponent` 분기의 아래 기존 코드:

```cpp
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileOwnerNet = entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            ownerNet = projectileOwnerNet;
```

아래로 교체:

```cpp
            entityKind = Shared::Schema::EntityKind::Projectile;
            projectileKind =
                CServerProjectileAuthority::kStructureProjectileKind;
            projectileOwnerNet = projectile.uSourceNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uSourceNetAtSpawn
                : entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = projectile.uTargetNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uTargetNetAtSpawn
                : entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
            projectileDirection = projectile.direction;
            projectileTraveledDist = projectile.traveledDistance;
            ownerNet = projectileOwnerNet;
```

per-entity 변수의 아래 기존 코드:

```cpp
        f32_t projectileRadius = 0.f;
        f32_t projectileMaxDist = 0.f;
```

아래로 교체:

```cpp
        f32_t projectileRadius = 0.f;
        f32_t projectileMaxDist = 0.f;
        Vec3 projectileDirection{};
        f32_t projectileTraveledDist = 0.f;
```

`SkillProjectileComponent` 분기의 아래 기존 코드:

```cpp
            projectileOwnerNet = entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = NULL_NET_ENTITY;
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
```

아래로 교체:

```cpp
            projectileOwnerNet = projectile.uSourceNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uSourceNetAtSpawn
                : entityMap.ToNet(projectile.sourceEntity);
            projectileTargetNet = projectile.uTargetNetAtSpawn != NULL_NET_ENTITY
                ? projectile.uTargetNetAtSpawn
                : entityMap.ToNet(projectile.targetEntity);
            projectileSpeed = projectile.speed;
            projectileRadius = projectile.hitRadius;
            projectileMaxDist = projectile.maxDistance;
            projectileDirection = projectile.direction;
            projectileTraveledDist = projectile.traveledDistance;
```

`CreateEntitySnapshot` 호출의 기존 마지막 인자 `forcedMotionRemainingSec` 다음에 append:

```cpp
            projectileDirection.x,
            projectileDirection.y,
            projectileDirection.z,
            projectileTraveledDist,
```

RESOLVED 2026-07-13: codegen된 exact signature를 기준으로 `simSpeedMul` 뒤에 `gameplayStates`를 append했고 `(kind, sourceNet, targetNet, startTick)` 정렬과 첫 슬라이스 상태를 반영했다.

```cpp
// EzrealRisingSpellForce:
// BuffComponent에서 kEzrealRisingSpellForceBuffDefId를 찾아
// sourceNet, stackCount, magnitude0 = bonusAttackSpeedPerStack,
// expireTick = BuffInstance::uExpireTick

// EzrealEssenceFlux:
// uSourceNet, uTargetNet, uExpireTick, uRank

// YasuoWindWall:
// sourceNet, spawnTick, expireTick, center, direction,
// magnitude0 = halfLength, magnitude1 = halfThickness
```

### 1-25. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

`class CEventApplier` 바로 위에 추가:

```cpp
enum class ePresentationMutationPhase : u8_t
{
    SpawnOrMark = 0,
    ContactOrClear = 1,
    SnapshotTruth = 2,
};

struct PresentationMutationStamp
{
    u64_t uServerTick = 0u;
    u32_t uEventOrdinal = 0u;
    ePresentationMutationPhase ePhase =
        ePresentationMutationPhase::SpawnOrMark;
    bool_t bValid = false;
};
```

아래 기존 signature:

```cpp
    void RebaseTimeline(CWorld& world);
```

아래로 교체:

```cpp
    void RebaseTimeline(CWorld& world, EntityIdMap& entityMap);
```

`ApplyProjectileHit`의 아래 기존 signature:

```cpp
    void ApplyProjectileHit(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileHitEvent* ev);
```

아래로 교체:

```cpp
    void ApplyProjectileHit(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileHitEvent* ev,
        u64_t serverTick,
        u32_t uEventOrdinal);
```

`ApplyProjectileSpawn`의 아래 기존 signature:

```cpp
    void ApplyProjectileSpawn(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileSpawnEvent* ev,
        u64_t serverTick);
```

아래로 교체:

```cpp
    void ApplyProjectileSpawn(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileSpawnEvent* ev,
        u64_t serverTick,
        u32_t uEventOrdinal);
```

`ApplyEffectTrigger`의 아래 기존 signature:

```cpp
    void ApplyEffectTrigger(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::EffectTriggerEvent* ev);
```

아래로 교체:

```cpp
    void ApplyEffectTrigger(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::EffectTriggerEvent* ev,
        u64_t uServerTick,
        u32_t uEventOrdinal);
```

public 영역의 `RetryCurrentActionVisual` 바로 아래에 추가:

```cpp
    void BeginSnapshotReconciliation(
        u64_t uServerTick,
        bool_t bFullSnapshot);
    void UpsertProjectileSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uProjectileNet,
        u16_t uProjectileKind,
        const Vec3& vPosition,
        const Vec3& vDirection,
        f32_t fSpeed,
        f32_t fMaxDistance,
        f32_t fTraveledDistance);
    void UpsertEzrealFluxSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uSourceNet,
        NetEntityId uTargetNet,
        u64_t uExpireTick);
    void UpsertYasuoWindWallSnapshot(
        CWorld& world,
        NetEntityId uSourceNet,
        u64_t uSpawnTick,
        const Vec3& vCenter,
        const Vec3& vDirection,
        f32_t fHalfLength,
        f32_t fHalfThickness,
        u64_t uExpireTick);
    void EndSnapshotReconciliation(CWorld& world, EntityIdMap& entityMap);
```

private 영역의 projectile helper 선언 바로 위에 추가:

```cpp
    EntityID EnsureProjectilePresentation(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uProjectileNet,
        u16_t uProjectileKind,
        const Vec3& vPosition,
        const Vec3& vDirection,
        f32_t fSpeed,
        f32_t fRemainingDistance);
```

private data 끝에 추가:

```cpp
    std::unordered_set<NetEntityId> m_snapshotProjectileNetIds;
    std::unordered_set<u64_t> m_snapshotEzrealFluxKeys;
    std::unordered_set<u64_t> m_snapshotYasuoWindWallKeys;
    std::unordered_map<u64_t, EntityID> m_yasuoWindWallAnchors;
    std::unordered_map<u64_t, std::vector<EntityID>>
        m_yasuoWindWallVisualEntities;
    std::unordered_set<u64_t> m_seenProjectileHitCueKeys;
    std::unordered_map<NetEntityId, PresentationMutationStamp>
        m_projectileMutationStamps;
    std::unordered_map<u64_t, PresentationMutationStamp>
        m_ezrealFluxMutationStamps;
    u64_t m_reconcileServerTick = 0u;
    bool_t m_bReconcileFullSnapshot = false;
```

RESOLVED 2026-07-13: schema codegen 뒤 `EventApplier.cpp`와 `SnapshotApplier.cpp`에 공용 presentation mutation 경로를 반영하고 production comparator를 독립 contract probe로 고정했다. 구현 불변식은 다음이다.

```cpp
// 1. 동일 projectileNet visual은 한 세트만 존재한다.
// 2. WFX spawn cue는 authoritative projectile entity에 attachTo한다.
// 3. snapshot Transform이 BA/E homing 위치를 소유한다.
// 4. hit dedupe key는 (projectileNet,targetNet,serverTick,contactOrdinal)다.
//    old wire의 contactOrdinal=0인 R 다중 hit도 targetNet으로 분리한다.
// 5. deltaBaseTick==0 full snapshot에서 보이지 않은 projectile/W mark/wall만 제거한다.
// 6. (serverTick,phase,eventOrdinal) stamp보다 오래된 mutation은
//    event/snapshot 어느 쪽에서도 상태를 되살리지 않는다.
// 7. event와 snapshot은 EnsureProjectilePresentation 한 경로를 공유한다.
```

### 1-26. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

STL include 영역에 추가:

```cpp
#include <algorithm>
#include <limits>
```

client projectile include는 기존 `ProjectileVisualCatalog` include를 사용한다. 별도 Shared gameplay component 의존은 추가하지 않는다.

generated schema macro guard 안의 `Event_generated.h` 바로 아래에 추가:

```cpp
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
```

anonymous namespace의 `BuildCueKey` 바로 아래에 추가:

```cpp
    bool_t IsNewerMutation(
        const PresentationMutationStamp& candidate,
        const PresentationMutationStamp& current)
    {
        if (!candidate.bValid)
            return false;
        if (!current.bValid)
            return true;
        if (candidate.uServerTick != current.uServerTick)
            return candidate.uServerTick > current.uServerTick;
        if (candidate.ePhase != current.ePhase)
        {
            return static_cast<u8_t>(candidate.ePhase) >
                static_cast<u8_t>(current.ePhase);
        }
        return candidate.uEventOrdinal > current.uEventOrdinal;
    }

    bool_t TryAdvanceMutation(
        PresentationMutationStamp& current,
        const PresentationMutationStamp& candidate)
    {
        if (!IsNewerMutation(candidate, current))
            return false;
        current = candidate;
        return true;
    }

    PresentationMutationStamp MakeEventMutation(
        u64_t uServerTick,
        u32_t uEventOrdinal,
        ePresentationMutationPhase ePhase)
    {
        return PresentationMutationStamp{
            uServerTick,
            uEventOrdinal,
            ePhase,
            true };
    }

    PresentationMutationStamp MakeSnapshotMutation(u64_t uServerTick)
    {
        return PresentationMutationStamp{
            uServerTick,
            (std::numeric_limits<u32_t>::max)(),
            ePresentationMutationPhase::SnapshotTruth,
            true };
    }
```


`RebaseTimeline`의 아래 기존 signature:

```cpp
void CEventApplier::RebaseTimeline(CWorld& world)
```

아래로 교체:

```cpp
void CEventApplier::RebaseTimeline(
    CWorld& world,
    EntityIdMap& entityMap)
```

같은 함수의 아래 기존 projectile visual loop:

```cpp
    for (const auto& entry : m_projectileVisualEntities)
    {
        for (EntityID entity : entry.second)
            DestroyEntityIfAlive(world, entity);
    }
```

아래로 교체:

```cpp
    for (const auto& [projectileNet, visuals] : m_projectileVisualEntities)
    {
        for (EntityID entity : visuals)
            DestroyEntityIfAlive(world, entity);
        const EntityID projectileEntity = entityMap.FromNet(projectileNet);
        DestroyEntityIfAlive(world, projectileEntity);
        entityMap.Unbind(projectileNet);
    }
```

기존 Ezreal flux loop 바로 위에 추가:

```cpp
    for (const auto& [_, entity] : m_yasuoWindWallAnchors)
        DestroyEntityIfAlive(world, entity);
    for (const auto& [_, visuals] : m_yasuoWindWallVisualEntities)
    {
        for (EntityID entity : visuals)
            DestroyEntityIfAlive(world, entity);
    }
```

cue가 없는 projectile도 empty-vector entry가 있으므로 old branch entity/binding을 놓치지 않는다.

같은 함수의 기존 map/set clear 목록 바로 아래에 추가:

```cpp
    m_yasuoWindWallAnchors.clear();
    m_yasuoWindWallVisualEntities.clear();
    m_snapshotProjectileNetIds.clear();
    m_snapshotEzrealFluxKeys.clear();
    m_snapshotYasuoWindWallKeys.clear();
    m_seenProjectileHitCueKeys.clear();
    m_projectileMutationStamps.clear();
    m_ezrealFluxMutationStamps.clear();
    m_reconcileServerTick = 0u;
    m_bReconcileFullSnapshot = false;
```

Chrono branch 변경은 tick 대소 비교보다 우선한다. old branch의 presentation과 mutation clock을 남긴 채 새 branch snapshot을 적용하지 않는다.

`OnEvent`의 아래 기존 코드:

```cpp
    case Shared::Schema::EventKind::ProjectileHit:
        ApplyProjectileHit(world, entityMap, packet->projectileHit());
        break;
```

아래로 교체:

```cpp
    case Shared::Schema::EventKind::ProjectileHit:
        ApplyProjectileHit(
            world,
            entityMap,
            packet->projectileHit(),
            packet->serverTick(),
            packet->eventOrdinal());
        break;
```

`ProjectileSpawn`과 `EffectTrigger` case도 각각 기존 호출 마지막에 `packet->eventOrdinal()`을 추가한다.

`ApplyProjectileSpawn`, `ApplyProjectileHit`, `ApplyEffectTrigger` definition에 header와 같은 `u32_t uEventOrdinal` 인자를 추가한다.

`ApplyProjectileHit` 함수 첫 null check 바로 아래에 추가:

```cpp
    const u64_t cueKey = BuildCueKey(
        ev->netId(),
        ev->targetNet(),
        ev->kind(),
        serverTick,
        ev->contactOrdinal());
    if (m_seenProjectileHitCueKeys.size() > 4096u)
        m_seenProjectileHitCueKeys.clear();
    if (!m_seenProjectileHitCueKeys.insert(cueKey).second)
        return;
```

terminal hit 처리 전 아래 코드를 추가한다:

```cpp
    bool_t bApplyTerminalMutation = true;
    if (ev->bDestroyed() && ev->netId() != NULL_NET_ENTITY)
    {
        bApplyTerminalMutation = TryAdvanceMutation(
            m_projectileMutationStamps[ev->netId()],
            MakeEventMutation(
                serverTick,
                uEventOrdinal,
                ePresentationMutationPhase::ContactOrClear));
    }
```

terminal entity/visual destroy block은 `ev->bDestroyed() && bApplyTerminalMutation`일 때만 상태를 변경한다. hit cue dedupe/재생은 이 gate 앞에 두어 snapshot-first에서도 one-shot fact를 잃지 않는다.

`ApplyProjectileSpawn`은 cue 이름/인자만 해석한 뒤 아래 stamp를 advance한 경우에만 `EnsureProjectilePresentation`으로 entity와 persistent spawn cue를 생성한다. stamp gate 전에 `CFxCuePlayer::Play/PlayAll`을 호출하지 않는다. snapshot이나 같은 tick의 더 늦은 hit가 먼저 적용된 뒤 도착한 spawn은 visual state를 되살리지 않는다.

```cpp
    const bool_t bApplySpawnMutation = TryAdvanceMutation(
        m_projectileMutationStamps[ev->netId()],
        MakeEventMutation(
            serverTick,
            uEventOrdinal,
            ePresentationMutationPhase::SpawnOrMark));
```

`UpsertProjectileSnapshot`은 `MakeSnapshotMutation(m_reconcileServerTick)`을 advance한 경우에만 ensure하고, `EndSnapshotReconciliation`의 absence도 같은 snapshot stamp를 advance한 경우에만 제거한다. `SnapshotTruth` phase가 가장 높고 ordinal도 `UINT32_MAX`이므로 같은 tick의 모든 event 뒤 최종 truth다.

모든 snapshot upsert는 stamp gate보다 먼저 해당 seen-set에 key를 넣는다. 같은 tick full snapshot을 재적용해 stamp가 equal이어도 `EndSnapshotReconciliation`이 존재 중인 presentation을 unseen으로 오인하지 않게 한다.

Ezreal W mark/detonate/clear branch에서 relation key를 만든 직후 아래 코드를 추가한다:

```cpp
        const ePresentationMutationPhase ePhase =
            effectId == kEzrealEffectEssenceFluxMark
                ? ePresentationMutationPhase::SpawnOrMark
                : ePresentationMutationPhase::ContactOrClear;
        const bool_t bApplyRelationMutation = TryAdvanceMutation(
            m_ezrealFluxMutationStamps[relationKey],
            MakeEventMutation(
                uServerTick,
                uEventOrdinal,
                ePhase));
```

W mark create/refresh와 그 persistent mark cue는 `bApplyRelationMutation`일 때만 수행한다. detonate/clear의 state 제거도 gate를 따르되, detonate impact 같은 one-shot cue만 별도 dedupe 뒤 재생할 수 있다. `UpsertEzrealFluxSnapshot`과 reconcile absence는 projectile과 동일한 snapshot stamp를 사용한다.

`BeginSnapshotReconciliation`은 세 seen-set을 비우고 tick/full 여부만 저장한다. `EndSnapshotReconciliation`은 `m_bReconcileFullSnapshot == true`일 때만 unseen presentation을 제거한다. projectile 제거 시 `DestroyProjectileVisuals -> entity destroy -> EntityIdMap::Unbind`를 한 block에서 수행하고, 더 새로운 mutation stamp가 있으면 보존한다. Ezreal W refresh는 동일 relation key를 유지하되 남은 lifetime을 새 `expireTick`으로 다시 설정한다.

Yasuo wall key는 source 하나가 아니라 아래 stable presentation key를 사용한다:

```cpp
const u64_t uWallKey = BuildCueKey(
    uSourceNet,
    0u,
    static_cast<u32_t>(Shared::Schema::GameplayStateKind::YasuoWindWall),
    uSpawnTick,
    0u);
```

동일 source가 practice cooldown reset으로 활성 장막을 둘 이상 만들더라도 서로 다른 key를 가진다. `UpsertYasuoWindWallSnapshot`은 preset에 `2.f * fHalfLength`를 full width로 전달하고, `expireTick`이 바뀌면 기존 wall visuals를 교체해 남은 lifetime을 갱신한다.

아래 기존 코드:

```cpp
    if (effectId == kGlobalEffectFlashBlink)
```

아래로 교체:

```cpp
    if (effectId == kEzrealEffectArcaneShiftBlink)
```

CONFIRM_NEEDED: `kGlobalEffectFlashBlink`는 승인된 범용 Summoner Flash cue가 아직 없으므로 Ezreal FX로 다시 연결하지 않는다. 범용 cue asset과 manifest entry가 확정된 뒤 별도 branch로 재생한다.

`ApplyProjectileSpawn`과 `UpsertProjectileSnapshot`은 각각 cue를 직접 만들지 않고 `EnsureProjectilePresentation`을 호출한다. snapshot-only 호출은 `fRemainingDistance = max(0, maxDistance - traveledDistance)`, spawn event는 `fRemainingDistance = maxDistance`를 넘긴다. helper는 entity/binding/Transform/`ReplicatedProjectilePresentationTag`를 먼저 ensure하고, 빈 vector라도 `m_projectileVisualEntities[net]` entry를 만든다. 기존 entry가 있으면 재사용하고 없을 때만 `remainingDistance / speed` lifetime으로 catalog spawn cue를 만든다. 따라서 event-first, snapshot-first, snapshot-only가 같은 presentation 한 세트로 수렴하며 cue가 없는 projectile도 terminal-loss cleanup 대상에 남는다.

helper의 cue context에서 아래 코드를 사용한다:

```cpp
        fx.vVelocity = {};
        fx.bOverrideVelocity = true;
        fx.attachTo = entity;
```

기존처럼 `initial direction * speed`로 WFX 자체를 이동시키지 않는다. `UpsertProjectileSnapshot`은 authoritative entity Transform과 visual entity의 `FxMeshComponent::vRotation.y`/`FxBillboardComponent::fYaw`를 최신 direction yaw로 보정한다. late spawn event는 기존 snapshot presentation을 파괴하거나 lifetime을 처음부터 다시 시작하지 않는다.

### 1-27. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/SnapshotApplier.h

forward declaration 영역에 추가:

```cpp
class CEventApplier;
```

public setter 영역에 추가:

```cpp
    void SetEventApplier(CEventApplier* pEventApplier)
    {
        m_pEventApplier = pEventApplier;
    }
```

private data의 `m_seenNetIds` 바로 아래에 추가:

```cpp
    CEventApplier* m_pEventApplier = nullptr;
```

### 1-28. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

아래 include를 추가:

```cpp
#include "Network/Client/EventApplier.h"
#include "GameObject/Projectile/ProjectileVisualCatalog.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
```

`ShouldRebaseTimeline` 계산 직후, `m_timelineState = nextTimeline;`보다 앞에 추가:

```cpp
    if (!bTimelineRebased &&
        m_bHasTimelineState &&
        snapshot->serverTick() < m_lastServerTick)
    {
        return;
    }
```

tick이 감소해도 epoch/branch가 바뀐 snapshot은 먼저 rebase callback을 거쳐야 한다. 단순 `serverTick < m_lastServerTick` guard를 `GetSnapshot` 직후에 두지 않는다.

`const auto* entities = snapshot->entities();` 바로 위에 추가:

```cpp
    const bool_t bFullSnapshot = snapshot->deltaBaseTick() == 0u;
    if (m_pEventApplier)
    {
        m_pEventApplier->BeginSnapshotReconciliation(
            snapshot->serverTick(),
            bFullSnapshot);
    }
```

아래 기존 코드:

```cpp
    const auto* entities = snapshot->entities();
    if (!entities)
        return;

    std::unordered_set<u32_t> snapshotNetIds;
    snapshotNetIds.reserve(entities->size());

    for (const auto* es : *entities)
```

아래로 교체하고 기존 entity loop body 끝에 대응하는 `}`를 하나 추가한다:

```cpp
    const auto* entities = snapshot->entities();
    std::unordered_set<u32_t> snapshotNetIds;
    if (entities)
        snapshotNetIds.reserve(entities->size());

    if (entities)
    {
        for (const auto* es : *entities)
```

entity loop에서 `EnsureEntity` 성공 직후, projectile kind일 때 추가:

```cpp
        if (es->entityKind() == Shared::Schema::EntityKind::Projectile)
        {
            if (!world.HasComponent<ReplicatedProjectilePresentationTag>(e))
            {
                world.AddComponent<ReplicatedProjectilePresentationTag>(
                    e,
                    ReplicatedProjectilePresentationTag{});
            }
            if (m_pEventApplier)
            {
                m_pEventApplier->UpsertProjectileSnapshot(
                    world,
                    entityMap,
                    es->netId(),
                    es->projectileKind(),
                    Vec3{ es->posX(), es->posY(), es->posZ() },
                    Vec3{
                        es->projectileDirX(),
                        es->projectileDirY(),
                        es->projectileDirZ() },
                    es->projectileSpeed(),
                    es->projectileMaxDist(),
                    es->projectileTraveledDist());
            }
        }
```

stale cleanup의 아래 기존 코드:

```cpp
        const bool_t bWard =
            world.HasComponent<VisionSensorComponent>(entity);
        if (!bServerMinion && !bViegoSoul && !bKalistaSentinel && !bWard)
            continue;
```

아래로 교체:

```cpp
        const bool_t bWard =
            world.HasComponent<VisionSensorComponent>(entity);
        const bool_t bProjectilePresentation =
            world.HasComponent<ReplicatedProjectilePresentationTag>(entity);
        if (!bServerMinion &&
            !bViegoSoul &&
            !bKalistaSentinel &&
            !bWard &&
            !bProjectilePresentation)
        {
            continue;
        }
        if (bProjectilePresentation && m_pEventApplier)
            continue;
```

RESOLVED 2026-07-13: codegen된 exact accessor를 사용해 `snapshot->gameplayStates()` parsing과 full/delta reconciliation 순서를 반영했다.

```cpp
// timeline 판정 및 필요 시 RebaseTimeline
// BeginSnapshotReconciliation(serverTick, deltaBaseTick == 0)
// entity loop: projectile UpsertProjectileSnapshot
// EzrealRisingSpellForce -> client UI/presentation stack state upsert/remove
// (client Stat/BuffComponent를 권위 계산에 사용하지 않음)
// EzrealEssenceFlux -> UpsertEzrealFluxSnapshot
// YasuoWindWall -> UpsertYasuoWindWallSnapshot(sourceNet, startTick, ...)
// EndSnapshotReconciliation(world, entityMap)
// full snapshot일 때만 기존 m_seenNetIds stale cleanup
```

gameplay-state dispatch 직후, 아래 기존 코드 바로 위에 추가:

```cpp
    if (m_pEventApplier)
        m_pEventApplier->EndSnapshotReconciliation(world, entityMap);

    if (!bFullSnapshot)
        return;
```

기준 기존 코드:

```cpp
    std::vector<u32_t> staleNetIds;
```

`EndSnapshotReconciliation`은 full snapshot에서 unseen projectile의 visual/entity/EntityIdMap binding을 제거하는 단일 owner다. `SnapshotApplier`의 generic stale cleanup은 `ReplicatedProjectilePresentationTag` entity를 다시 destroy/unbind하지 않는다. delta snapshot은 absence를 deletion으로 해석하지 않는다. `entities()`가 비어 있어도 Begin/End가 실행되게 조기 return 위치를 조정한다.

### 1-29. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

아래 기존 코드:

```cpp
    m_pSnapshotApplier = CSnapshotApplier::Create();
    m_pEventApplier = CEventApplier::Create();
    m_pCommandSerializer = CCommandSerializer::Create();
```

아래로 교체:

```cpp
    m_pSnapshotApplier = CSnapshotApplier::Create();
    m_pEventApplier = CEventApplier::Create();
    m_pCommandSerializer = CCommandSerializer::Create();

    if (m_pSnapshotApplier)
        m_pSnapshotApplier->SetEventApplier(m_pEventApplier.get());
```

`RebaseNetworkTimeline`의 아래 기존 코드:

```cpp
    if (m_pEventApplier)
        m_pEventApplier->RebaseTimeline(m_World);
```

아래로 교체:

```cpp
    if (m_pEventApplier && m_pEntityIdMap)
        m_pEventApplier->RebaseTimeline(m_World, *m_pEntityIdMap);
```

### 1-30. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h

`FxCueContext`의 size override 필드 바로 아래에 추가:

```cpp
	bool_t bOverrideScaleMultiplier = false;
	Vec3 vScaleMultiplier{ 1.f, 1.f, 1.f };
```

기준 기존 코드:

```cpp
	bool_t bOverrideSize = false;
```

### 1-31. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

`BuildCueMesh`의 아래 기존 코드:

```cpp
        mesh.vScale = emitter.vScale;
```

아래로 교체:

```cpp
        mesh.vScale = emitter.vScale;
        if (ctx.bOverrideScaleMultiplier)
        {
            mesh.vScale.x *= ctx.vScaleMultiplier.x;
            mesh.vScale.y *= ctx.vScaleMultiplier.y;
            mesh.vScale.z *= ctx.vScaleMultiplier.z;
        }
```

### 1-32. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Yasuo/YasuoFxPresets.h

include 영역에 추가:

```cpp
#include <vector>
```

아래 기존 signature:

```cpp
    void SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime, f32_t fWidth, f32_t fHeight,
        f32_t fMeshScale = 0.01f);
```

아래로 교체:

```cpp
    void SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward,
        f32_t fLifetime, f32_t fWidth, f32_t fHeight,
        f32_t fMeshScale = 0.01f,
        EntityID attachTo = NULL_ENTITY,
        std::vector<EntityID>* pSpawnedEntities = nullptr);
```

### 1-33. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp

`SpawnWWindWall` signature에 `EntityID attachTo`, `std::vector<EntityID>* pSpawnedEntities`를 추가한다. 기존 local-only 호출은 두 default 인자로 source compatibility를 유지한다.

아래 기존 코드:

```cpp
    (void)fWidth;
    (void)fHeight;
    (void)fMeshScale;
```

아래로 교체:

```cpp
    (void)fHeight;
    (void)fMeshScale;
```

아래 기존 코드:

```cpp
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.bOverrideLifetime = true;
```

아래로 교체:

```cpp
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.attachTo = attachTo;
    cue.bOverrideLifetime = true;
    cue.bOverrideScaleMultiplier = true;
    cue.vScaleMultiplier = {
        (std::max)(0.1f, fWidth / 6.2f),
        1.f,
        1.f };
```

아래 기존 호출:

```cpp
    CFxCuePlayer::Play(world, kCueWWindWall, cue);
```

아래로 교체:

```cpp
    CFxCuePlayer::PlayAll(
        world,
        kCueWWindWall,
        cue,
        pSpawnedEntities);
```

snapshot owner는 반환된 모든 visual entity를 wall key별로 보관해 refresh, full-snapshot absence, timeline rebase에서 즉시 파괴한다.

이 파일에 `<algorithm>` include가 없으면 include 영역에 추가:

```cpp
#include <algorithm>
```

### 1-34. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp

`OnCastAccepted_W_Visual`의 null check 바로 아래에 추가:

```cpp
        if (ctx.bAuthoritativeEvent)
            return;
```

네트워크 권위 경로의 장막 visual은 persistent snapshot 한 경로만 소유한다. local-only smoke는 기존 visual hook을 유지한다.

### 1-35. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Yasuo/w_windwall.wfx

파일 안의 모든 아래 값을:

```json
"lifetime": 5.00
```

아래로 교체:

```json
"lifetime": 4.00
```

모든 `attach_offset`의 세 번째 값 `1.50`을 `0.0`으로 교체한다. authoritative wall anchor가 이미 서버 `center`를 소유하므로 WFX가 전방으로 1.5m 중복 이동하지 않게 한다.

### 1-36. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

RESOLVED 2026-07-13: AI 세션 Handoff 뒤 최신 SimLab probe와 linkage를 다시 읽어 legacy Buff API 이관, Ezreal 수치·mana snapshot·passive·E 보정·keyframe 회귀를 반영했다.

Handoff 뒤 모든 기존 `CBuffSystem::Execute(world, tc)` call은 해당 probe의 기존 stat 이후 위치에서 `CBuffSystem::AdvanceDurationsAfterStat(world, tc)`로 바꾸고, Ezreal tick driver의 command/hit 처리 전에는 `PruneExpiredTickBuffs`와 changed일 때의 stat recompute를 추가한다. 아래 검색 결과가 0이 아니면 API 이관 미완료다.

```powershell
rg -n "CBuffSystem::Execute" Server Tools/SimLab Shared
```

확정 후 같은 함수에만 다음 regression을 추가한다.

```cpp
// ChampionGameDataDB Ezreal Q/W/E/R target/cd/range/mana/action-lock parity
// Q rank 5 accept(40 mana) -> launch 전 rank 1 -> paidManaCost 40 유지
// W2 skill trigger = 60 + 실제 paid cost, BA trigger = 총 mana restore 0
// Rising Spell Force: 능력 적중당 1, R은 적마다 1, BA는 0, max 5, 6초 [start,end)
// tick T+180: pre-command prune -> conditional Stat -> command, bonus AS 0
// E 4.75 clamp, short-point partial blink, invalid landing backward correction/fallback
// pending + W relation + projectile + passive keyframe save/restore
// source/target/projectile entity slot reuse generation safety
```

RESOLVED 2026-07-13: SimLab에 Server 소스를 억지로 링크하지 않고 `Tools/Harness/GameRoomProjectileIntegrationProbe.cpp`와 실행 스크립트를 추가했다. 이 fixture는 freshness-build한 실제 Server Debug object를 링크해 Ezreal BA·포탑 spawn, `CGameRoom::Phase_ServerProjectiles`, skill/structure target의 동일 ID·새 generation 재사용, terminal Net unbind/serializer delayed-unbind, Rising Spell Force pre-command expiry를 실행한다. Client applier를 포함한 packet-delivery 통합은 이 fixture 범위가 아니며 실제 다중 클라이언트 스모크로 후속 검증한다.

### 1-37. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/ProjectileGameplayDefs.json

CONFIRM_NEEDED: 신규 designer-authorable projectile profile은 현재 `SkillProjectileComponent`의 하드코딩 정책을 먼저 검증한 뒤 별도 work packet으로 만든다. 새 파일 body와 cooker contract를 확정하기 전에는 빈 JSON이나 런타임 JSON 파서를 추가하지 않는다.

최초 profile이 반드시 소유할 필드는 아래와 같다.

```text
DefinitionKey
FlightMode
TargetLossPolicy
UnitHitPolicy
TargetKindMask
MaxUniqueHits
CollidesWithTerrain
BlockedByProjectileBarriers
PersistAfterSourceDeath
Speed
MaxDistance
HitRadius
```

Ezreal BA/Q/W/E/R의 현재 동작, typed contact event, late-join snapshot이 모두 통과한 뒤 C++ call-site 상수를 profile lookup으로 한 항목씩 치환한다.

### 1-38. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h

`ProjectileVisualDesc` 바로 위에 추가:

```cpp
struct ReplicatedProjectilePresentationTag
{
};
```

아래 기존 struct:

```cpp
struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
};
```

아래로 교체:

```cpp
struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
    const char* pszBarrierCue = nullptr;
    const char* pszTerrainCue = nullptr;
    const char* pszExpireCue = nullptr;
};
```

필드를 뒤에만 추가하므로 현재 3-field initializer의 의미는 바뀌지 않는다.

### 1-39. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`ApplyProjectileHit`에서 아래 기존 코드:

```cpp
    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    if (visual.pszHitCue && ev->targetNet() != NULL_NET_ENTITY)
```

아래로 교체:

```cpp
    const Vec3 pos{ ev->posX(), ev->posY(), ev->posZ() };
    const ProjectileVisualDesc& visual = ProjectileVisualCatalog::Resolve(ev->kind());
    const char* pszContactCue = nullptr;
    switch (ev->contactReason())
    {
    case Shared::Schema::ProjectileContactReason::None:
        pszContactCue = ev->targetNet() != NULL_NET_ENTITY
            ? visual.pszHitCue
            : visual.pszExpireCue;
        break;
    case Shared::Schema::ProjectileContactReason::UnitHit:
        pszContactCue = visual.pszHitCue;
        break;
    case Shared::Schema::ProjectileContactReason::Barrier:
        pszContactCue = visual.pszBarrierCue;
        break;
    case Shared::Schema::ProjectileContactReason::Terrain:
        pszContactCue = visual.pszTerrainCue;
        break;
    case Shared::Schema::ProjectileContactReason::RangeExpired:
    case Shared::Schema::ProjectileContactReason::SourceInvalid:
    case Shared::Schema::ProjectileContactReason::TargetInvalid:
    case Shared::Schema::ProjectileContactReason::InvalidTrajectory:
    case Shared::Schema::ProjectileContactReason::HitLimit:
        pszContactCue = visual.pszExpireCue;
        break;
    default:
        break;
    }

    if (pszContactCue)
```

같은 block 안의 아래 기존 호출:

```cpp
        CFxCuePlayer::Play(world, visual.pszHitCue, fx);
```

아래로 교체:

```cpp
        CFxCuePlayer::Play(world, pszContactCue, fx);
```

unit hit가 아닌 contact에는 `pszAttachedCue`를 재생하지 않도록 아래 기존 코드:

```cpp
    if (visual.pszAttachedCue)
```

아래로 교체:

```cpp
    if (visual.pszAttachedCue &&
        (ev->contactReason() ==
                Shared::Schema::ProjectileContactReason::UnitHit ||
            (ev->contactReason() ==
                Shared::Schema::ProjectileContactReason::None &&
                ev->targetNet() != NULL_NET_ENTITY)))
```

### 1-40. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

CONFIRM_NEEDED: Ezreal BA가 현재 `Ezreal.Q.Hit`을 재사용한다. `Ezreal.BA.Hit`, 공용 Wind Wall block, terrain, expire cue의 WFX asset과 manifest가 승인된 뒤에만 해당 initializer의 새 필드를 채운다. 존재하지 않는 cue 이름을 먼저 넣어 silent fallback을 만들지 않는다.

### 1-41. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Yasuo/Yasuo_Tuning.h

local-only smoke fallback의 아래 기존 코드:

```cpp
        f32_t wLifetime = 5.0f;
        f32_t wWidth = 6.0f;
```

아래로 교체:

```cpp
        f32_t wLifetime = 4.0f;
        f32_t wWidth = 3.2f;
```

네트워크 권위 visual은 이 fallback 수치를 읽지 않고 snapshot의 `expireTick`과 `halfLength`를 사용한다.

## 2. 검증

충돌 회피 게이트:

- 다른 AI 작업이 `Handoff`가 되기 전에는 이 계획서 외의 파일을 수정하지 않는다.
- Handoff 직후 `git status --short`와 `git diff -- Tools/SimLab/main.cpp Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Data/Gameplay/ChampionGameData/champions.json Data/LoL`을 다시 읽고, 계획서 anchor가 달라졌으면 최신 코드 기준으로 갱신한다.
- 기존 dirty change를 reset/revert/checkout하지 않는다.
- schema, generated data, projectile runtime은 한 세션이 순차 소유한다.
- GameSim/SimLab/Server/Client build를 병렬 실행하지 않는다. 동일 `WintersGameSim.lib` 경합으로 `LNK1104`가 발생한 이력이 있다.

생성기 순서:

```powershell
python Tools/ChampionData/build_champion_game_data.py
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
Shared\Schemas\run_codegen.bat
```

수기 수정 금지 생성물:

- `Shared/GameSim/Generated/ChampionGameData.generated.*`
- `Shared/Schemas/Generated/**`
- generated gameplay/visual definition pack C++

데이터 검증:

- canonical `champions.json`의 Ezreal Q/W/E/R와 `ChampionGameData.generated.cpp::MakeChampion_EZREAL`의 target/cooldown/range/mana/action-lock가 일치해야 한다.
- stale legacy hash `0xE5ADA227`가 재생성 뒤 남지 않아야 하고 Client Hello hash와 Server lobby hash가 같아야 한다. 이 hash는 legacy ChampionGameData timing 계약만 증명하므로 SkillEffect/profile parity는 `Build-LoLDefinitionPack.py --check`와 DefinitionManifest hash로 별도 확인한다.
- Q/W/R effect JSON에서 duplicate `range`가 0개여야 한다.
- 현재 PC LoL 기준은 Riot Ezreal 페이지의 패시브 5스택 계약과 26.3/26.9/26.13 patch note 수치를 기준으로 고정하고, 공식 페이지에 노출되지 않는 10%/6초 세부값은 LoL Wiki data template과 교차 확인한다: https://www.leagueoflegends.com/en-us/champions/ezreal/ , https://www.leagueoflegends.com/en-ph/news/game-updates/patch-26-3-notes/ , https://www.leagueoflegends.com/en-gb/news/game-updates/league-of-legends-patch-26-9-notes/ , https://www.leagueoflegends.com/en-us/news/game-updates/league-of-legends-patch-26-13-notes/ , https://wiki.leagueoflegends.com/en-us/Template%3AData_Ezreal/Rising_Spell_Force .
- Rising Spell Force는 적 하나를 능력으로 맞힐 때마다 1스택, 10% bonus AS, 최대 5, 6초 refresh를 검사한다. R 다중 적중은 적마다 스택을 올리고 BA는 올리지 않는다.

정적 검증:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
rg -n "ResolvePaidManaCost" Shared/GameSim/Champions/Ezreal
rg -n "CBuffSystem::Execute" Server Tools/SimLab Shared
rg -n "BuildProjectileHitEvent" Server Shared
python -c "import json,pathlib; p=pathlib.Path(r'Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json'); d=json.loads(p.read_text(encoding='utf-8')); m={x['key']:x['params'] for x in d['skillEffects']}; assert all('range' not in m[k] for k in ('skill.ezreal.q','skill.ezreal.w','skill.ezreal.r'))"
git diff --check
```

정적 성공 조건:

- `ResolvePaidManaCost` 결과 0건.
- legacy `CBuffSystem::Execute` 결과 0건이며 Server pre-command prune가 first `Phase_ExecuteCommands`보다 앞선다.
- attack-proc receipt는 1-9의 별도 계약이 확정되기 전까지 기존 호출 구조를 그대로 둔다.
- 모든 projectile hit event call은 contact reason과 contact ordinal을 전달한다.
- `GameRoomProjectiles.cpp`의 terminal structure/skill path는 두 enqueue helper만 사용하고 stored projectile Net override 복사와 즉시 unbind 뒤 entity를 destroy한다.
- Shared/GameSim은 Engine, Client, Renderer, UI, ImGui, DX type을 include하지 않는다.

순차 빌드:

```powershell
msbuild Shared/GameSim/Include/GameSim.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Tools/SimLab/SimLab.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
Tools/Bin/Debug/SimLab.exe --ezreal-only
Tools/Bin/Debug/SimLab.exe 1800 42
msbuild Server/Include/Server.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
```

Shared/SimLab acceptance:

- Q rank 5 accept 시 40 mana를 지불하고 발사 전 rank가 바뀌어도 projectile의 `paidManaCost`는 40이다.
- Q/E/R로 W를 터뜨리면 `60 + 실제 지불 mana`, BA로 터뜨리면 mana를 전혀 복구하지 않는다.
- Q 첫 unit contact는 damage/on-hit/cooldown refund를 정확히 한 번 적용하고 소멸한다.
- W 첫 contact는 damage 없이 source-target 표식 하나만 남기며 refresh, 다른 Ezreal 격리, consume, expire가 `[start,end)`로 동작한다.
- E는 marked champion, marked structure, nearest valid target 순으로 고르고 4.75 이내 partial blink와 landing correction을 지킨다.
- R은 champion full formula와 non-epic formula를 구분하고 ordered unique pierce를 수행한다.
- passive/pending/W relation/projectile/barrier가 keyframe restore 뒤 동일 hash와 결과를 낸다.
- `BuffComponent::uExpireTick` 기반 6초 만료가 정확히 180 tick의 `[T,T+180)`으로 stat dirty와 함께 한 번만 일어난다. T+180의 `pre-command prune -> conditional Stat -> first Phase_ExecuteCommands` trace에서 bonus AS가 이미 0이고 이후 CombatAction에서도 0이며, float subtraction 때문에 181 tick 유지되지 않고 Ezreal 전용 `StatSystem` include/분기도 0개다.
- S018이 `BuffComponent`, `SkillProjectileComponent`, `StructureProjectileComponent`, `ReplicatedEventComponent` raw layout을 바꾸므로 이전 build keyframe의 reject/version 정책을 fixture로 고정한다. cross-build keyframe 호환을 요구한다면 구현 전에 format version/migration owner를 확정한다.

Server projectile integration acceptance:

- BA/Q/W/E/R 모두 moving-target relative CCD로 tunnel 없이 맞는다.
- 연속 dash가 projectile segment를 가로지르면 swept hit가 나고, 같은 기하를 Flash/E/Zed swap 같은 discontinuous move로 건너면 이동 경로상의 허위 hit가 나지 않는다.
- lag history에 정확한 T-1이 없으면 T-2를 사용하지 않고 current-position fallback을 쓰며, Chrono restore 직후 첫 resume tick은 seeded restored pose로 같은 CCD 결과를 낸다.
- BA/Q/W/E/R 모두 적 Yasuo W에 막히고 UnitHit gameplay payload를 실행하지 않는다.
- moving wall과 projectile이 같은 tick에 교차해도 barrier sweep이 차단한다.
- barrier와 unit의 quantized TOI가 같으면 barrier가 우선한다.
- R이 같은 tick에 여러 적을 맞혀도 `(projectileNet, targetNet, serverTick, contactOrdinal)`이 모두 다르다.
- entity slot을 재사용해도 R hit ledger와 lag history가 이전 generation을 현재 적으로 취급하지 않는다.
- terminal enqueue 직후 stored Net ID가 entity destroy 전에 즉시 unbind된다. BroadcastEvents 전 local projectile slot을 재사용해도 새 entity는 새 Net ID를 받고, serializer의 delayed old-ID unbind는 no-op이며 새 mapping을 지우지 않는다.
- skill/structure projectile의 zero direction, zero speed, NaN/Inf position·direction·speed·range·radius는 한 tick 안에 `InvalidTrajectory` contact를 내고 제거된다.
- targeted BA/E는 매 tick authoritative `direction`을 갱신하며 snapshot yaw가 실제 velocity와 일치한다.
- structure projectile도 direction/maxDistance/traveledDistance를 권위 상태와 snapshot에 기록하며 range에서 만료되고 snapshot-only turret projectile의 yaw/남은 lifetime이 맞는다.
- source invalid, target invalid, barrier, terrain, range, hit limit, unit hit가 서로 다른 typed reason으로 serialize된다.
- source-death persistence가 true인 Ezreal projectile은 발사자 사망 뒤 계속 비행하고 attribution net id를 보존한다.
- terrain contact를 다른 contact와 완전 통합하려면 `ILoLWalkabilityQuery`에 first-contact TOI/normal API가 필요하다. 이 API owner가 정해지기 전에는 Ezreal의 `bCollidesWithTerrain=false` 범위만 출하 판정한다.
- schema diff에서 기존 Event/Snapshot field 순서가 한 칸도 이동하지 않고 새 field가 각 table tail에만 append됐는지 검토한다.
- EventPacket의 `eventOrdinal`은 기존 `killFeed` 뒤에만 append되고 ProjectileSpawn/Hit의 기존 field ID를 건드리지 않는다.
- Snapshot의 `gameplayStates`는 반드시 기존 `simSpeedMul` 뒤이며 S017 timeline field ID가 유지된다.
- codegen 전 old Event/ProjectileHit/ProjectileSpawn/Snapshot fixture를 새 reader로 읽어 기존 `bDestroyed`, `lastAckedCommandSeq`, gold/stat/AI/timeline 필드가 보존되고 `eventOrdinal/targetNet/contactReason/contactOrdinal/gameplayStates`는 default로 읽히는지 검사한다.
- 동일 seed·동일 supported MSVC/x64 build는 contact order와 wire position hash가 일치한다. 다른 compiler/architecture까지 보장한다고 주장하려면 quantization 경계 fixture와 허용 오차를 별도로 기록한다.

Client/reconnect acceptance:

- event-first, snapshot-first, snapshot-only가 projectile visual 한 세트로 수렴한다.
- snapshot-only projectile은 `(maxDistance - traveledDistance) / speed`만큼의 남은 lifetime으로 생성되고 늦은 spawn event가 visual/lifetime을 재시작하지 않는다.
- BA/E homing visual anchor와 authoritative Transform의 XZ 오차가 snapshot 직후 0.25 world unit 이하다.
- 중간 접속 시 비행 중 Q/W/R/BA/E projectile이 즉시 보인다.
- 중간 접속 시 W 표식과 형성/이동 중 Yasuo W가 남은 lifetime, center, direction, width로 복원된다.
- terminal event 유실 뒤 다음 full snapshot에서 projectile entity, visual, EntityIdMap binding이 모두 제거된다.
- delta snapshot의 absence는 projectile/W/wall 삭제로 해석하지 않고 `deltaBaseTick == 0` full snapshot에서만 unseen state를 제거한다.
- 중복 hit event는 hit cue를 한 번만 재생하고 R의 서로 다른 contact ordinal은 각각 한 번 재생한다. old wire의 ordinal 0 hit도 targetNet이 다르면 합쳐지지 않는다.
- 같은 tick의 structure spawn→terminal hit와 Ezreal W mark→detonate packet을 역순 전달해도 `(tick, phase, eventOrdinal)` stamp가 terminal/clear 또는 snapshot truth를 우선해 state를 부활시키지 않는다.
- 낮아진 serverTick의 새 timeline epoch/branch snapshot은 버리지 않고 rebase하며 old projectile/W/wall visual, seen-set, mutation map, dedupe key를 모두 비운다.
- 동일 Yasuo source의 활성 wall 두 개는 `(sourceNet, spawnTick)`으로 분리되고, snapshot `halfLength`는 preset full width에 `2 * halfLength`로 전달된다.
- 동일 Ezreal W relation refresh는 새 expireTick의 남은 lifetime으로 시각 시간을 갱신한다.
- Ezreal E는 `kEzrealEffectArcaneShiftBlink`, Summoner Flash는 `kGlobalEffectFlashBlink`로 분리된다. 범용 Flash cue가 승인되기 전에는 이 항목을 시각 완료로 판정하지 않는다.
- authoritative Yasuo W는 cast visual hook과 snapshot visual이 중복 생성되지 않는다.

최종 handoff:

- P0 데이터/mana/passive와 P1 collision/event를 각각 작은 diff로 리뷰한 뒤 P2 schema/client reconciliation을 시작한다.
- 생성기 diff에 Ezreal 외 대량 변경이 생기면 자동 채택하지 않고 원본 JSON 변경 owner를 확인한다.
- 실제 Server integration fixture는 Ezreal BA·포탑 lifecycle, skill/structure target generation 재사용, terminal immediate/delayed unbind와 passive pre-command expiry를 production object 경로로 통과해야 한다.
- append-only/current-schema omitted-tail, event/snapshot mutation ordering, timeline rebase와 gameplay-state 상태 계약은 독립 contract probe와 Client 제품 링크로 판정한다. historical old-schema byte fixture와 Client packet-delivery를 포함한 event-first·snapshot-first·terminal-loss 실제 다중 클라이언트 스모크는 완료 범위를 과장하지 않고 보고서의 후속 경계로 남긴다.
- 모든 현재 게이트 통과 뒤 `.md/build/2026-07-13_S018_EZREAL_PROJECTILE_AUTHORITY_REPORT.md`를 작성하고 work packet을 `Active -> Handoff`로 전환한다.
