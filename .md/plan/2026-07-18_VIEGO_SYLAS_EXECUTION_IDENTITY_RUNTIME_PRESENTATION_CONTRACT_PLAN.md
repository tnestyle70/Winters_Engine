Session - 비에고/사일러스 실행 정체성·대여 런타임 수명·투사체 표현 계약 전수 조사 및 회귀 차단 계획
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: `2026-07-18_IRELIA_VIEGO_SYLAS_EZREAL_RUNTIME_ACCEPTANCE_RECOVERY_PLAN.md` / `_RESULT.md`, `Champion/31~35_VIEGO_SYLAS_*` 역사 문서

## 1. 결정 기록

```text
① 문제·제약: 17챔피언 로스터에서 비에고→애쉬 BA 투사체 미생성 1건은 재현 전 코드 원인이 확정됐고, 같은 물리-ID 게이트가 이즈리얼·칼리스타까지 총 3개 발사기에 존재한다. 사일러스→비에고 R은 이동 컴포넌트 경로가 이미 범용으로 열려 있어 “이동 실패” 원인은 아직 미확정이나, 비에고 전용 상태를 사일러스에게 영구 생성하는 런타임 누수 1건은 확정이다.
② 단순 대안의 실패: 모든 `ChampionComponent/StatComponent` 검사를 실행 챔피언으로 일괄 교체하면 영혼 생성·패시브 소유권·원본 스탯까지 훼손한다. 새 네트워크 identity 필드 추가도 `ActionState.sourceChampion`, `CombatAction.eSourceChampion`, snapshot action/form 필드와 중복되므로 채택하지 않는다.
③ 메커니즘: 본체 정체성은 소유권/스탯에만, `SkillOverrideResolveResult.hookChampion`에서 내려온 실행 정체성은 훅·피해·BA 투사체·Action/FX에만 사용한다. 비에고 R 대시는 비에고 상태 없이도 동작하게 두고, 실제 비에고가 소유 중인 상태가 있을 때만 possession을 해제한다.
④ 대조·경계: Riot 원본의 “비에고는 몸의 기본 스킬을 사용하고 R은 자기 R로 대체”, “사일러스는 상대 궁극기를 사용”을 계약 기준으로 삼는다. Shared/GameSim이 진실을 소유하고 Client는 snapshot/action/projectile kind를 FBX/애니메이션/FX로 번역하며 Engine에는 LoL 챔피언 분기를 추가하지 않는다.
⑤ 대가·예산: 70%는 보고된 BA/R 문제와 자동 회귀 probe, 30% ceiling은 17챔피언 OWNER/EXECUTION/RUNTIME/PRESENTATION 매트릭스 및 대여 상태 정리 계약에 쓴다. 이 범위를 넘어 모든 챔피언 스킬 의미를 한 번에 재구현하지 않고, 매트릭스의 `AUDIT_REQUIRED`는 후속 slice로 남긴다.
```

### 1-1. 현재 코드 증거와 원인 판정

| 경로 | 현재 증거 | 판정 |
|---|---|---|
| 비에고 폼 BA 명령 | `CommandExecutor::HandleBasicAttack`이 `ResolveBasicAttack(...).hookChampion`을 `CombatActionComponent.eSourceChampion`에 기록 | 실행 정체성 전달 정상 |
| BA 피해/FX | `CombatActionSystem.cpp`가 `action.eSourceChampion`으로 피해와 effect id를 계산 | 실행 정체성 전달 정상 |
| 애쉬 BA 발사 | `AsheGameSim.cpp:438-442`가 공격자 `StatComponent.championId == ASHE`를 요구 | 비에고 본체에서 발사 거부 — **확정 원인** |
| 이즈리얼 BA 발사 | `EzrealGameSim.cpp:1291-1295`가 공격자 `StatComponent.championId == EZREAL`을 요구 | 같은 회귀 위험 — **확정 구조 결함** |
| 칼리스타 BA 발사 | `KalistaGameSim.cpp:992-995`가 `IsKalistaEntity`로 물리 ID를 요구 | 같은 회귀 위험 — **확정 구조 결함** |
| 애쉬 화살 FBX | projectile kind 18 → `ProjectileVisualCatalog`의 `Ashe.BA.Arrow` → `ba_arrow.wfx` → `ashe_base_aa_arrow.fbx`; 실제 리소스 존재 | 클라이언트 자산 누락 아님 |
| 사일러스→비에고 R 명령 | Spellbook override가 hook champion VIEGO/source slot R로 해석되고 GroundTarget `groundPos`도 명령에 실림 | 입력/훅 정체성 정상 |
| 비에고 R 이동 | `OnR`이 모든 caster에 `ViegoDashComponent`를 만들고, 전역 `ViegoGameSim::Tick`이 물리 챔피언 검사 없이 Transform을 갱신 | 서버상 이동해야 함; 실제 실패 원인 **미확정** |
| 비에고 R 상태 | `OnR`이 무조건 `EnsureViegoState` 호출 | 사일러스에게 `ViegoSimComponent` 영구 잔류 — **확정 누수** |
| 기존 SimLab | base Viego R의 착지 피해 중심과 Sylas 16개 궁 훅/소비만 검사 | stolen Viego R 변위, Viego 상태 비잔류, possessed ranged BA 투사체 미검사 |

### 1-2. 정체성 계약

| 정체성 | 소유자/전달 필드 | 반드시 사용하는 곳 | 사용하면 안 되는 곳 |
|---|---|---|---|
| OWNER/본체 | `ChampionComponent.id`, `StatComponent.championId` | 영혼 생성 자격, 원본 인벤토리/스탯/자원, 부활·킬 소유권 | 대여한 스킬/BA의 투사체 종류·애니메이션·effect id 결정 |
| EXECUTION/실행 | `SkillOverrideResolveResult.hookChampion`, `CombatAction.eSourceChampion`, `ActionState.sourceChampion` | 훅, 사거리/타이밍 정의, 피해 공식, projectile kind, 서버 cue | 본체 교체, 원본 스탯 ID 변경 |
| PRESENTATION/표현 | `FormOverride.visualChampion`, snapshot action source, projectile kind | 몸 FBX, BA/스킬 애니메이션, 투사체/FX/사운드 | 서버 피해·이동·상태의 진실 생성 |
| RUNTIME/대여 수명 | caster에 생성된 챔피언별 상태 컴포넌트 | 해당 폼/대여 스킬이 활성인 동안의 연속 상태 | 폼 종료 뒤 pending cast/dash/표식 소유 상태의 무기한 잔류 |

공식 동작 참고:

- [Riot Games — Viego](https://www.leagueoflegends.com/en-us/champions/viego/)
- [Riot Games — Sylas](https://www.leagueoflegends.com/en-us/champions/sylas/)

### 1-3. 현재 17챔피언 위험 매트릭스

`PASS`는 이 계획 시점의 코드 존재가 아니라 자동 probe로 의미/수명/표현까지 확인된 경우에만 쓴다. 아래 `TARGET`은 이번 slice에서 닫을 항목이다.

| 챔피언 | 비에고 BA/QWE | 폼 종료 runtime | 사일러스 R | 현재 판정 |
|---|---|---|---|---|
| 이렐리아 | 근접 BA + QWE 훅 | `IreliaSimComponent` 제거 경로 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 야스오 | 근접 BA + QWE 훅 | `CancelRuntime` 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 칼리스타 | ranged BA와 Martial Poise arm/consume이 물리-ID에서 차단 | passive dash 제거 존재 | 다단 Fate's Call 기존 probe | **AT_RISK → TARGET** |
| 사일러스 | 근접 BA + QWE 훅 | Sylas state/dash 제거 존재 | 자기 R 탈취 금지 | AUDIT_REQUIRED/N/A |
| 비에고 | 자기 QWE | 본체 runtime | 사일러스가 훔친 R의 변위 미검증, 상태 누수 | **TARGET** |
| 애니 | 근접/원거리 BA 표현 및 QWE | Annie state 제거 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 애쉬 | ranged BA가 물리-ID에서 차단 | Ashe state 제거 존재 | R 훅 존재 | **FAIL_ROOT_PROVEN → TARGET** |
| 피오라 | 근접 BA + QWE | `CancelRuntime` 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 가렌 | 근접 BA + QWE | 별도 runtime 없음 | R 훅 존재 | AUDIT_REQUIRED |
| 리븐 | 근접 BA + QWE | Riven state 제거 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 제드 | 근접 BA + QWE | state/vanish/source mark 제거 | R 훅 존재 | AUDIT_REQUIRED |
| 이즈리얼 | ranged BA가 물리-ID에서 차단 | pending cast 정리 경로 없음 | R 훅 존재 | **AT_RISK → TARGET** |
| 요네 | 근접 BA + QWE | `CancelRuntime` 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 잭스 | 근접 BA + QWE | `CancelRuntime` 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 마스터 이 | 근접 BA + QWE | Yi state 제거 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 킨드레드 | ranged BA 경로는 generic hitscan 여부 재확인 필요 | Kindred state 제거 존재 | R 훅 존재 | AUDIT_REQUIRED |
| 리 신 | 근접 BA + QWE | state/dash 제거 존재 | R 훅 존재 | AUDIT_REQUIRED |

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ashe/AsheGameSim.h

include 블록의 기존 코드:

```cpp
#include "Shared/GameSim/Components/GameplayComponents.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
```

`TryLaunchBasicAttackProjectile` 선언의 기존 코드:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest);
```

아래로 교체:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest);
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ashe/AsheGameSim.cpp

삭제할 코드:

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
```

실행 정체성 인자로 물리 스탯 ID 검사를 대체하면 이 include는 더 이상 사용되지 않는다.

`TryLaunchBasicAttackProjectile` 시작부의 기존 코드:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<StatComponent>(attacker) ||
            world.GetComponent<StatComponent>(attacker).championId !=
                eChampion::ASHE ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

아래로 교체:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest)
    {
        if (effectiveAttackChampion != eChampion::ASHE ||
            !world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

의도: `attacker`는 계속 비에고의 EntityID/스탯/소유권을 유지하되, 이 공격의 실행 정체성이 애쉬일 때만 애쉬 BA 투사체를 만든다.

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.h

include 블록의 기존 코드:

```cpp
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersTypes.h"
```

namespace 시작부의 기존 코드:

```cpp
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);

    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest);
```

아래로 교체:

```cpp
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
    void CancelBorrowedRuntime(CWorld& world, EntityID caster);

    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest);
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp

`EzrealGameSim::Tick`의 기존 코드:

```cpp
    void Tick(CWorld& world, const TickContext& tc)
    {
        TickEssenceFluxMarks(world, tc);
        TickPendingCasts(world, tc);
    }

    bool_t TryLaunchBasicAttackProjectile(
```

아래로 교체:

```cpp
    void Tick(CWorld& world, const TickContext& tc)
    {
        TickEssenceFluxMarks(world, tc);
        TickPendingCasts(world, tc);
    }

    void CancelBorrowedRuntime(CWorld& world, EntityID caster)
    {
        if (caster != NULL_ENTITY &&
            world.HasComponent<EzrealPendingCastComponent>(caster))
        {
            world.RemoveComponent<EzrealPendingCastComponent>(caster);
        }
    }

    bool_t TryLaunchBasicAttackProjectile(
```

`TryLaunchBasicAttackProjectile` 시작부의 기존 코드:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<StatComponent>(attacker) ||
            world.GetComponent<StatComponent>(attacker).championId !=
                eChampion::EZREAL ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

아래로 교체:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest)
    {
        if (effectiveAttackChampion != eChampion::EZREAL ||
            !world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

`CancelBorrowedRuntime`은 아직 발사되지 않은 `EzrealPendingCastComponent`만 제거한다. 이미 발사된 투사체와 대상에 부착된 source-owned W 표식은 서버가 독립 엔티티/핸들로 수명을 관리하므로 이 함수에서 일괄 삭제하지 않는다.

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.h

include 블록의 기존 코드:

```cpp
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"
```

`TryLaunchBasicAttackProjectile` 선언의 기존 코드:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest);
```

아래로 교체:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest);
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp

삭제할 코드:

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
```

익명 namespace의 아래 함수 전체를 삭제:

```cpp
    bool_t IsKalistaEntity(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
        {
            return world.GetComponent<ChampionComponent>(entity).id ==
                eChampion::KALISTA;
        }
        return world.HasComponent<StatComponent>(entity) &&
            world.GetComponent<StatComponent>(entity).championId ==
                eChampion::KALISTA;
    }
```

`TryLaunchBasicAttackProjectile` 시작부의 기존 코드:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext&,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !IsKalistaEntity(world, attacker) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

아래로 교체:

```cpp
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext&,
        EntityID attacker,
        EntityID target,
        eChampion effectiveAttackChampion,
        const DamageRequest& damageRequest)
    {
        if (effectiveAttackChampion != eChampion::KALISTA ||
            !world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }
```

`IsKalistaEntity`의 유일한 호출부가 이 BA 발사기이므로 함수와 `StatComponent` include를 함께 제거한다. Fate's Call 등 실제 칼리스타 소유권 판단부의 개별 `ChampionComponent.id` 검사는 그대로 둔다.

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

`ApplyBasicAttackImpact`에서 아래 선언을 삭제:

```cpp
        const eChampion resolvedChampion = ResolveChampion(world, source);
```

아래 애쉬 후처리 조건을 `actionChampion`으로 바꾸면 이 지역 변수의 마지막 사용처가 사라진다.

`bProjectileImpactDeferred` 계산의 기존 코드:

```cpp
        const bool_t bProjectileImpactDeferred =
            AsheGameSim::TryLaunchBasicAttackProjectile(
                world,
                tc,
                source,
                target,
                request) ||
            EzrealGameSim::TryLaunchBasicAttackProjectile(
                world,
                tc,
                source,
                target,
                request) ||
            KalistaGameSim::TryLaunchBasicAttackProjectile(
                world,
                tc,
                source,
                target,
                request);
```

아래로 교체:

```cpp
        bool_t bProjectileImpactDeferred = false;
        switch (actionChampion)
        {
        case eChampion::ASHE:
            bProjectileImpactDeferred =
                AsheGameSim::TryLaunchBasicAttackProjectile(
                    world,
                    tc,
                    source,
                    target,
                    actionChampion,
                    request);
            break;
        case eChampion::EZREAL:
            bProjectileImpactDeferred =
                EzrealGameSim::TryLaunchBasicAttackProjectile(
                    world,
                    tc,
                    source,
                    target,
                    actionChampion,
                    request);
            break;
        case eChampion::KALISTA:
            bProjectileImpactDeferred =
                KalistaGameSim::TryLaunchBasicAttackProjectile(
                    world,
                    tc,
                    source,
                    target,
                    actionChampion,
                    request);
            break;
        default:
            break;
        }
```

아래 기존 코드:

```cpp
        if (bProjectileImpactDeferred && resolvedChampion == eChampion::ASHE)
            return true;
```

아래로 교체:

```cpp
        if (bProjectileImpactDeferred && actionChampion == eChampion::ASHE)
            return true;
```

의도: 세 발사기를 물리 엔티티에 순차 대입하지 않고, 이미 권위적으로 결정된 실행 챔피언 하나만 호출한다. 애쉬 BA의 generic EffectTrigger 억제도 본체가 아니라 실행 챔피언을 따른다.

### 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`ArmKalistaPassiveDashWindow` 시작부의 기존 코드:

```cpp
    {
        if (ResolveChampion(world, entity) != eChampion::KALISTA)
            return;
        if (!world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<ActionStateComponent>(entity))
        {
            return;
        }

        auto& action = world.GetComponent<ActionStateComponent>(entity);
```

아래로 교체:

```cpp
    {
        if (!world.HasComponent<TransformComponent>(entity) ||
            !world.HasComponent<ActionStateComponent>(entity))
        {
            return;
        }

        auto& action = world.GetComponent<ActionStateComponent>(entity);
        if (action.sourceChampion != eChampion::KALISTA)
            return;
```

`TryConsumeKalistaPassiveDashMove` 시작부의 기존 코드:

```cpp
    {
        if (ResolveChampion(world, cmd.issuerEntity) != eChampion::KALISTA)
            return false;

        KalistaPassiveDashComponent* pExistingDash = nullptr;
```

아래로 교체:

```cpp
    {
        KalistaPassiveDashComponent* pExistingDash = nullptr;
```

같은 함수에서 기존 코드:

```cpp
        auto& action = world.GetComponent<ActionStateComponent>(cmd.issuerEntity);
        const auto actionId = static_cast<eActionStateId>(action.actionId);
        if (!IsKalistaPassiveDashAction(actionId))
```

아래로 교체:

```cpp
        auto& action = world.GetComponent<ActionStateComponent>(cmd.issuerEntity);
        const auto actionId = static_cast<eActionStateId>(action.actionId);
        if (action.sourceChampion != eChampion::KALISTA ||
            !IsKalistaPassiveDashAction(actionId))
```

같은 함수의 fallback 방향 기존 코드:

```cpp
            dashDir = ForwardFromYaw(transform.GetRotation().y, ResolveChampion(world, cmd.issuerEntity));
```

아래로 교체:

```cpp
            dashDir = ForwardFromYaw(
                transform.GetRotation().y,
                action.sourceChampion);
```

이미 arm된 `KalistaPassiveDashComponent`의 active/pending fast path는 그대로 둔다. 컴포넌트는 실행 정체성이 KALISTA인 Action에서만 만들어지고 비에고 폼 종료 시 제거되므로, 물리 본체가 VIEGO여도 BA/Q 뒤 이동 입력을 소비할 수 있다.

### 2-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp

include 시작부의 기존 코드:

```cpp
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
```

`ClearBorrowedChampionRuntime` switch의 기존 코드:

```cpp
        case eChampion::ASHE: RemoveBorrowedComponent<AsheSimComponent>(world, caster); break;
        case eChampion::FIORA: FioraGameSim::CancelRuntime(world, caster); break;
```

아래로 교체:

```cpp
        case eChampion::ASHE: RemoveBorrowedComponent<AsheSimComponent>(world, caster); break;
        case eChampion::EZREAL: EzrealGameSim::CancelBorrowedRuntime(world, caster); break;
        case eChampion::FIORA: FioraGameSim::CancelRuntime(world, caster); break;
```

`OnR` 시작부의 기존 코드:

```cpp
        ViegoSimComponent& viegoState = EnsureViegoState(*ctx.pWorld, ctx.casterEntity);
        if (viegoState.bPossessionActive ||
            viegoState.bPossessionPending ||
            ctx.pWorld->HasComponent<FormOverrideComponent>(ctx.casterEntity))
        {
            ClearViegoPossession(
                *ctx.pWorld,
                ctx.casterEntity,
                viegoState,
                ctx.pTickCtx);
        }
```

아래로 교체:

```cpp
        if (ctx.pWorld->HasComponent<ViegoSimComponent>(ctx.casterEntity))
        {
            ViegoSimComponent& viegoState =
                ctx.pWorld->GetComponent<ViegoSimComponent>(ctx.casterEntity);
            if (viegoState.bPossessionActive ||
                viegoState.bPossessionPending ||
                ctx.pWorld->HasComponent<FormOverrideComponent>(ctx.casterEntity))
            {
                ClearViegoPossession(
                    *ctx.pWorld,
                    ctx.casterEntity,
                    viegoState,
                    ctx.pTickCtx);
            }
        }
```

사일러스가 훔친 비에고 R은 `ViegoDashComponent`만 생성한다. 실제 비에고는 spawn assembly가 이미 `ViegoSimComponent`를 소유하므로 possession 해제 동작은 유지된다.

### 2-10. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

include 블록의 기존 코드:

```cpp
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/KalistaRendComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
```

#### (a) 기존 직접 발사기 호출의 새 실행 정체성 인자

칼리스타 probe의 기존 코드:

```cpp
        if (KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, jax, target, attack))
```

아래로 교체:

```cpp
        if (KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, jax, target, eChampion::JAX, attack))
```

칼리스타 probe의 기존 코드:

```cpp
        if (!KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, kalista, target, attack))
```

아래로 교체:

```cpp
        if (!KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, kalista, target, eChampion::KALISTA, attack))
```

이즈리얼 probe의 기존 코드:

```cpp
            if (EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, jax, target, attack))
```

아래로 교체:

```cpp
            if (EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, jax, target, eChampion::JAX, attack))
```

이즈리얼 probe의 기존 코드:

```cpp
            if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, ezreal, target, attack))
```

아래로 교체:

```cpp
            if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, ezreal, target, eChampion::EZREAL, attack))
```

#### (b) 비에고가 빼앗은 ranged BA 실행 정체성·투사체 생성·hit-handler probe

기존 `RunViegoRLandingCenterProbe` 함수 바로 위에 추가:

```cpp
    bool_t RunViegoBorrowedRangedBasicAttackProbe()
    {
        struct BorrowedAttackCase
        {
            eChampion champion;
            eProjectileKind projectileKind;
        };

        static constexpr BorrowedAttackCase kCases[] =
        {
            { eChampion::ASHE, eProjectileKind::AsheBasicAttack },
            { eChampion::EZREAL, eProjectileKind::EzrealBasicAttack },
            { eChampion::KALISTA, eProjectileKind::KalistaBasicAttack },
        };

        for (const BorrowedAttackCase& testCase : kCases)
        {
            CWorld world;
            DeterministicRng rng(
                2026071810ull + static_cast<u64_t>(testCase.champion));
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();

            const EntityID viego = SpawnChampion(
                world,
                entityMap,
                eChampion::VIEGO,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            const EntityID target = SpawnChampion(
                world,
                entityMap,
                eChampion::JAX,
                static_cast<u8_t>(eTeam::Red),
                5u);
            world.GetComponent<TransformComponent>(viego).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 3.f, 0.f, 0.f });

            FormOverrideComponent form{};
            form.baseChampion = eChampion::VIEGO;
            form.visualChampion = testCase.champion;
            form.skillChampion = testCase.champion;
            form.skillSlotMask = static_cast<u8_t>(
                1u << static_cast<u8_t>(eSkillSlot::BasicAttack));
            form.fRemainingSec = -1.f;
            form.bActive = true;
            world.AddComponent<FormOverrideComponent>(viego, form);

            TickContext commandTick = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            GameCommand attack{};
            attack.kind = eCommandKind::BasicAttack;
            attack.issuerEntity = viego;
            attack.targetEntity = target;
            attack.sequenceNum = 1u;
            attack.issuedAtTick = commandTick.tickIndex;

            const CommandExecutionResult result =
                executor->ExecuteCommand(world, commandTick, attack);
            if (result.state != eCommandExecutionState::Accepted ||
                !world.HasComponent<CombatActionComponent>(viego))
            {
                std::printf(
                    "[SimLab][ViegoBorrowedBA] FAIL: command rejected champion=%u\n",
                    static_cast<unsigned>(testCase.champion));
                return false;
            }

            const CombatActionComponent action =
                world.GetComponent<CombatActionComponent>(viego);
            if (action.eSourceChampion != testCase.champion ||
                world.GetComponent<ChampionComponent>(viego).id != eChampion::VIEGO ||
                world.GetComponent<StatComponent>(viego).championId != eChampion::VIEGO)
            {
                std::printf(
                    "[SimLab][ViegoBorrowedBA] FAIL: owner/execution identity mismatch champion=%u\n",
                    static_cast<unsigned>(testCase.champion));
                return false;
            }

            TickContext impactTick = MakeProbeTickContext(
                action.uImpactTick, rng, entityMap, walkable);
            CCombatActionSystem::Execute(world, impactTick);

            EntityID matchingProjectileEntity = NULL_ENTITY;
            u32_t matchingProjectiles = 0u;
            u32_t pendingDamageRequests = 0u;
            u32_t basicAttackEffectTriggers = 0u;
            world.ForEach<SkillProjectileComponent>(
                [&](EntityID entity, SkillProjectileComponent& projectile)
                {
                    if (projectile.kind == testCase.projectileKind &&
                        projectile.sourceEntity == viego &&
                        projectile.targetEntity == target)
                    {
                        matchingProjectileEntity = entity;
                        ++matchingProjectiles;
                    }
                });
            world.ForEach<DamageRequestComponent>(
                [&](EntityID, DamageRequestComponent& request)
                {
                    if (request.source == viego && request.target == target)
                        ++pendingDamageRequests;
                });
            world.ForEach<ReplicatedEventComponent>(
                [&](EntityID, ReplicatedEventComponent& event)
                {
                    if (event.kind == eReplicatedEventKind::EffectTrigger &&
                        event.sourceEntity == viego &&
                        event.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                    {
                        ++basicAttackEffectTriggers;
                    }
                });

            const u32_t expectedBasicAttackEffectTriggers =
                testCase.champion == eChampion::ASHE ? 0u : 1u;
            if (matchingProjectiles != 1u ||
                matchingProjectileEntity == NULL_ENTITY ||
                pendingDamageRequests != 0u ||
                basicAttackEffectTriggers != expectedBasicAttackEffectTriggers)
            {
                std::printf(
                    "[SimLab][ViegoBorrowedBA] FAIL: projectile/damage/cue mismatch champion=%u projectile=%u damage=%u cue=%u\n",
                    static_cast<unsigned>(testCase.champion),
                    matchingProjectiles,
                    pendingDamageRequests,
                    basicAttackEffectTriggers);
                return false;
            }

            const SkillProjectileComponent projectile =
                world.GetComponent<SkillProjectileComponent>(
                    matchingProjectileEntity);
            DamageRequest resolvedHit{};
            bool_t bEnqueueHit = false;
            if (testCase.champion == eChampion::ASHE)
            {
                if (!projectile.bApplyOnHitStatus ||
                    projectile.onHitStatus.sourceEntity != viego ||
                    projectile.onHitStatus.fDurationSec <= 0.f ||
                    projectile.onHitStatus.fMoveSpeedMul >= 1.f ||
                    !AsheGameSim::HandleProjectileHit(
                        world,
                        impactTick,
                        projectile,
                        target,
                        resolvedHit,
                        bEnqueueHit) ||
                    !bEnqueueHit ||
                    resolvedHit.source != viego ||
                    resolvedHit.target != target)
                {
                    std::printf(
                        "[SimLab][ViegoBorrowedBA] FAIL: borrowed Ashe hit/slow identity mismatch\n");
                    return false;
                }
            }
            else if (testCase.champion == eChampion::EZREAL)
            {
                if (!EzrealGameSim::HandleProjectileHit(
                        world,
                        impactTick,
                        projectile,
                        target,
                        resolvedHit,
                        bEnqueueHit) ||
                    !bEnqueueHit ||
                    resolvedHit.source != viego ||
                    resolvedHit.target != target)
                {
                    std::printf(
                        "[SimLab][ViegoBorrowedBA] FAIL: borrowed Ezreal hit identity mismatch\n");
                    return false;
                }
            }
            else
            {
                KalistaGameSim::ApplyRendStackOnHit(
                    world, impactTick, viego, target);
                if (!world.HasComponent<KalistaRendStackComponent>(target) ||
                    world.GetComponent<KalistaRendStackComponent>(target)
                        .sourceEntity != viego ||
                    world.GetComponent<KalistaRendStackComponent>(target)
                        .stackCount != 1u)
                {
                    std::printf(
                        "[SimLab][ViegoBorrowedBA] FAIL: borrowed Kalista Rend identity mismatch\n");
                    return false;
                }

                TickContext moveTick = MakeProbeTickContext(
                    action.uImpactTick + 1u, rng, entityMap, walkable);
                GameCommand move{};
                move.kind = eCommandKind::Move;
                move.issuerEntity = viego;
                move.groundPos = Vec3{ 0.f, 0.f, -4.f };
                move.direction = Vec3{ 0.f, 0.f, -1.f };
                move.sequenceNum = 2u;
                move.issuedAtTick = moveTick.tickIndex;
                const Vec3 beforeDash =
                    world.GetComponent<TransformComponent>(viego).GetPosition();
                const CommandExecutionResult moveResult =
                    executor->ExecuteCommand(world, moveTick, move);
                if (moveResult.state != eCommandExecutionState::Accepted ||
                    !world.HasComponent<KalistaPassiveDashComponent>(viego))
                {
                    std::printf(
                        "[SimLab][ViegoBorrowedBA] FAIL: borrowed Kalista passive move was not armed\n");
                    return false;
                }
                for (u64_t tick = moveTick.tickIndex;
                    tick <= moveTick.tickIndex + 90u;
                    ++tick)
                {
                    TickContext tc = MakeProbeTickContext(
                        tick, rng, entityMap, walkable);
                    KalistaGameSim::Tick(world, tc);
                }
                const Vec3 afterDash =
                    world.GetComponent<TransformComponent>(viego).GetPosition();
                if (DistanceSqXZLocal(beforeDash, afterDash) <= 0.01f)
                {
                    std::printf(
                        "[SimLab][ViegoBorrowedBA] FAIL: borrowed Kalista passive dash had no displacement\n");
                    return false;
                }
            }
        }

        std::printf(
            "[SimLab][ViegoBorrowedBA] PASS: Ashe/Ezreal/Kalista execution identity launches owned projectiles\n");
        return true;
    }
```

#### (c) 사일러스가 훔친 비에고 R의 실제 변위/identity/runtime probe

`RunViegoBorrowedRangedBasicAttackProbe` 바로 아래에 추가:

```cpp
    bool_t RunSylasStolenViegoRExecutionProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071814ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID sylas = SpawnChampion(
            world,
            entityMap,
            eChampion::SYLAS,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID clickedCenterTarget = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Red),
            5u);
        const EntityID oldMaxRangeTarget = SpawnChampion(
            world,
            entityMap,
            eChampion::ANNIE,
            static_cast<u8_t>(eTeam::Red),
            6u);
        world.GetComponent<TransformComponent>(sylas).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(clickedCenterTarget).SetPosition(
            Vec3{ 3.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(oldMaxRangeTarget).SetPosition(
            Vec3{ 6.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(sylas)
            .ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;

        SpellbookOverrideComponent stolenR{};
        stolenR.sourceChampion = eChampion::VIEGO;
        stolenR.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        stolenR.localSlot = static_cast<u8_t>(eSkillSlot::R);
        stolenR.sourceRank = 1u;
        stolenR.fRemainingSec = 45.f;
        stolenR.bActive = true;
        world.AddComponent<SpellbookOverrideComponent>(sylas, stolenR);

        TickContext castTick = MakeProbeTickContext(
            1ull, rng, entityMap, walkable);
        GameCommand cast{};
        cast.kind = eCommandKind::CastSkill;
        cast.issuerEntity = sylas;
        cast.slot = static_cast<u8_t>(eSkillSlot::R);
        cast.groundPos = Vec3{ 3.f, 0.f, 0.f };
        cast.direction = Vec3{ 1.f, 0.f, 0.f };
        cast.sequenceNum = 1u;
        cast.issuedAtTick = castTick.tickIndex;

        const CommandExecutionResult result =
            executor->ExecuteCommand(world, castTick, cast);
        if (result.state != eCommandExecutionState::Accepted ||
            world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            !world.HasComponent<ActionStateComponent>(sylas))
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] FAIL: dispatch/consume mismatch\n");
            return false;
        }

        const ActionStateComponent& action =
            world.GetComponent<ActionStateComponent>(sylas);
        if (action.sourceChampion != eChampion::VIEGO ||
            action.sourceSlot != static_cast<u8_t>(eSkillSlot::R) ||
            world.GetComponent<ChampionComponent>(sylas).id != eChampion::SYLAS ||
            world.GetComponent<StatComponent>(sylas).championId != eChampion::SYLAS)
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] FAIL: owner/execution identity mismatch\n");
            return false;
        }
        const bool_t bRuntimeLeakedAtCast =
            world.HasComponent<ViegoSimComponent>(sylas);

        bool_t clickedTargetDamaged = false;
        bool_t oldMaxRangeTargetDamaged = false;
        world.ForEach<DamageRequestComponent>(
            [&](EntityID, DamageRequestComponent& request)
            {
                if (request.source != sylas)
                    return;
                if (request.target == clickedCenterTarget)
                    clickedTargetDamaged = true;
                if (request.target == oldMaxRangeTarget)
                    oldMaxRangeTargetDamaged = true;
            });
        if (!clickedTargetDamaged || oldMaxRangeTargetDamaged)
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] FAIL: landing-center damage mismatch\n");
            return false;
        }

        for (u64_t tick = 2ull; tick <= 91ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick, rng, entityMap, walkable);
            ViegoGameSim::Tick(world, tc);
        }

        const Vec3 finalPosition =
            world.GetComponent<TransformComponent>(sylas).GetPosition();
        bool_t bPass = true;
        if (DistanceSqXZLocal(
                finalPosition,
                Vec3{ 3.f, 0.f, 0.f }) > 0.01f)
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] FAIL: displacement mismatch x=%.3f z=%.3f\n",
                finalPosition.x,
                finalPosition.z);
            bPass = false;
        }
        else
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] displacement PASS: x=%.3f z=%.3f\n",
                finalPosition.x,
                finalPosition.z);
        }

        if (bRuntimeLeakedAtCast ||
            world.HasComponent<ViegoSimComponent>(sylas))
        {
            std::printf(
                "[SimLab][SylasStolenViegoR] FAIL: Viego owner-state leaked onto Sylas\n");
            bPass = false;
        }

        if (!bPass)
            return false;

        std::printf(
            "[SimLab][SylasStolenViegoR] PASS: ground displacement, Viego action identity, no Viego owner-state leak\n");
        return true;
    }
```

#### (d) 비에고 폼 종료 시 이즈리얼 pending cast 정리 probe

`RunSylasStolenViegoRExecutionProbe` 바로 아래에 추가:

```cpp
    bool_t RunViegoBorrowedRuntimeCleanupProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071815ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;

        const EntityID viego = SpawnChampion(
            world,
            entityMap,
            eChampion::VIEGO,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        world.GetComponent<TransformComponent>(viego).SetPosition(Vec3{});

        auto& viegoState = world.GetComponent<ViegoSimComponent>(viego);
        viegoState.bPossessionActive = true;
        viegoState.possessionChampion = eChampion::EZREAL;

        FormOverrideComponent form{};
        form.baseChampion = eChampion::VIEGO;
        form.visualChampion = eChampion::EZREAL;
        form.skillChampion = eChampion::EZREAL;
        form.skillSlotMask = static_cast<u8_t>(
            1u << static_cast<u8_t>(eSkillSlot::Q));
        form.fRemainingSec = -1.f;
        form.bActive = true;
        world.AddComponent<FormOverrideComponent>(viego, form);

        EzrealPendingCastComponent pending{};
        pending.hCaster = world.GetEntityHandle(viego);
        pending.vOrigin = Vec3{};
        pending.vGroundTarget = Vec3{ 5.f, 0.f, 0.f };
        pending.vDirection = Vec3{ 1.f, 0.f, 0.f };
        pending.uLaunchTick = 5u;
        pending.uSlot = static_cast<u8_t>(eSkillSlot::Q);
        pending.uRank = 1u;
        pending.bHasGroundTarget = true;
        world.AddComponent<EzrealPendingCastComponent>(viego, pending);

        TickContext clearTick = MakeProbeTickContext(
            1ull, rng, entityMap, walkable);
        ViegoGameSim::ClearPossession(world, viego, &clearTick);
        if (world.HasComponent<FormOverrideComponent>(viego) ||
            world.HasComponent<EzrealPendingCastComponent>(viego) ||
            world.GetComponent<ViegoSimComponent>(viego).bPossessionActive)
        {
            std::printf(
                "[SimLab][ViegoBorrowedRuntime] FAIL: Ezreal pending/form survived ClearPossession\n");
            return false;
        }

        for (u64_t tick = 2ull; tick <= 30ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick, rng, entityMap, walkable);
            EzrealGameSim::Tick(world, tc);
        }

        u32_t launchedAfterExit = 0u;
        world.ForEach<SkillProjectileComponent>(
            [&](EntityID, SkillProjectileComponent& projectile)
            {
                if (projectile.sourceEntity == viego &&
                    projectile.kind == eProjectileKind::MysticShot)
                {
                    ++launchedAfterExit;
                }
            });
        if (launchedAfterExit != 0u)
        {
            std::printf(
                "[SimLab][ViegoBorrowedRuntime] FAIL: Ezreal pending cast launched after form exit\n");
            return false;
        }

        std::printf(
            "[SimLab][ViegoBorrowedRuntime] PASS: Ezreal pending cast cancelled at form exit\n");
        return true;
    }
```

#### (e) 전체 실행 게이트 연결

`bViegoPossessionProbePass` 인접 기존 코드:

```cpp
    const bool_t bViegoPossessionProbePass = RunViegoPossessionProbe();
    const bool_t bViegoRLandingCenterProbePass =
        RunViegoRLandingCenterProbe();
```

아래로 교체:

```cpp
    const bool_t bViegoPossessionProbePass = RunViegoPossessionProbe();
    const bool_t bViegoBorrowedRangedBasicAttackProbePass =
        RunViegoBorrowedRangedBasicAttackProbe();
    const bool_t bSylasStolenViegoRExecutionProbePass =
        RunSylasStolenViegoRExecutionProbe();
    const bool_t bViegoBorrowedRuntimeCleanupProbePass =
        RunViegoBorrowedRuntimeCleanupProbe();
    const bool_t bViegoRLandingCenterProbePass =
        RunViegoRLandingCenterProbe();
```

최종 `bPass` 체인의 기존 코드:

```cpp
        bYoneEReturnProbePass &&
        bViegoPossessionProbePass &&
        bViegoRLandingCenterProbePass &&
```

아래로 교체:

```cpp
        bYoneEReturnProbePass &&
        bViegoPossessionProbePass &&
        bViegoBorrowedRangedBasicAttackProbePass &&
        bSylasStolenViegoRExecutionProbePass &&
        bViegoBorrowedRuntimeCleanupProbePass &&
        bViegoRLandingCenterProbePass &&
```

`--stage-input-only`에는 이 두 probe를 중복 연결하지 않는다. 전체 SimLab 회귀에서 실행 정체성/투사체/변위를 닫고, 기존 stage-input gate는 데이터 입력 회귀에 한정한다.

### 2-11. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomProjectileIntegrationProbe.cpp

include 블록의 기존 코드:

```cpp
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
```

기존 include:

```cpp
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
```

바로 위에 추가:

```cpp
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

`CGameRoomIntegrationProbeAccess::EnterInGame` 바로 아래에 추가:

```cpp
    static void InitializeServerSystems(CGameRoom& room)
    {
        room.InitializeServerSimSystems();
    }
```

#### (a) 기존 Ezreal 직접 발사 호출 갱신

두 곳의 기존 호출:

```cpp
        if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world,
                spawnTick,
                source,
                target,
                request))
```

#### (b) canonical GameRoom tick의 사일러스→비에고 R 통합 probe

`CheckPassiveExpiryBeforeFirstCommand` 바로 위에 추가:

```cpp
    bool_t CheckSylasStolenViegoRFullTickLifecycle()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9106u);
        CGameRoomIntegrationProbeAccess::InitializeServerSystems(*room);
        CGameRoomIntegrationProbeAccess::SetExecutor(
            *room,
            CDefaultCommandExecutor::Create());
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID sylas = SpawnChampion(
            world,
            entityMap,
            eChampion::SYLAS,
            eTeam::Blue,
            Vec3{});
        if (sylas == NULL_ENTITY)
            return false;

        SkillRankComponent ranks{};
        ranks.ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;
        world.AddComponent<SkillRankComponent>(sylas, ranks);
        world.AddComponent<SkillStateComponent>(
            sylas,
            SkillStateComponent{});

        SpellbookOverrideComponent stolenR{};
        stolenR.sourceChampion = eChampion::VIEGO;
        stolenR.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        stolenR.localSlot = static_cast<u8_t>(eSkillSlot::R);
        stolenR.sourceRank = 1u;
        stolenR.fRemainingSec = 45.f;
        stolenR.bActive = true;
        world.AddComponent<SpellbookOverrideComponent>(sylas, stolenR);

        GameCommand cast{};
        cast.kind = eCommandKind::CastSkill;
        cast.issuerEntity = sylas;
        cast.slot = static_cast<u8_t>(eSkillSlot::R);
        cast.groundPos = Vec3{ 3.f, 0.f, 0.f };
        cast.direction = Vec3{ 1.f, 0.f, 0.f };
        cast.sequenceNum = 1u;
        cast.issuedAtTick = 1u;
        CGameRoomIntegrationProbeAccess::PushCommand(*room, cast);
        CGameRoomIntegrationProbeAccess::SetTickIndex(*room, 0u);
        CGameRoomIntegrationProbeAccess::RunFullTick(*room);

        if (!world.HasComponent<ActionStateComponent>(sylas))
            return false;
        const ActionStateComponent& action =
            world.GetComponent<ActionStateComponent>(sylas);
        const bool_t bIdentityPass =
            action.sourceChampion == eChampion::VIEGO &&
            action.sourceSlot == static_cast<u8_t>(eSkillSlot::R) &&
            !world.HasComponent<SpellbookOverrideComponent>(sylas) &&
            world.GetComponent<ChampionComponent>(sylas).id == eChampion::SYLAS &&
            world.GetComponent<StatComponent>(sylas).championId == eChampion::SYLAS;
        const bool_t bRuntimePass =
            !world.HasComponent<ViegoSimComponent>(sylas);

        for (u32_t i = 0u; i < 90u; ++i)
            CGameRoomIntegrationProbeAccess::RunFullTick(*room);

        const Vec3 finalPosition =
            world.GetComponent<TransformComponent>(sylas).GetPosition();
        const bool_t bDisplacementPass =
            std::fabs(finalPosition.x - 3.f) <= 0.1f &&
            std::fabs(finalPosition.z) <= 0.1f;
        if (!bIdentityPass || !bRuntimePass || !bDisplacementPass)
        {
            std::printf(
                "[GameRoomProjectileIntegration][SylasViegoR] FAIL identity=%u runtime=%u displacement=%u pos=(%.3f,%.3f)\n",
                static_cast<u32_t>(bIdentityPass),
                static_cast<u32_t>(bRuntimePass),
                static_cast<u32_t>(bDisplacementPass),
                finalPosition.x,
                finalPosition.z);
            return false;
        }
        return true;
    }
```

#### (c) canonical GameRoom projectile/damage/status의 비에고→애쉬 BA 통합 probe

`CheckSylasStolenViegoRFullTickLifecycle` 바로 아래에 추가:

```cpp
    bool_t CheckViegoBorrowedAsheBasicAttackFullTickLifecycle()
    {
        auto room = CGameRoomIntegrationProbeAccess::CreateRoom(9107u);
        CGameRoomIntegrationProbeAccess::InitializeServerSystems(*room);
        CGameRoomIntegrationProbeAccess::SetExecutor(
            *room,
            CDefaultCommandExecutor::Create());
        CGameRoomIntegrationProbeAccess::EnterInGame(*room);
        CWorld& world = CGameRoomIntegrationProbeAccess::World(*room);
        EntityIdMap& entityMap =
            CGameRoomIntegrationProbeAccess::EntityMap(*room);

        const EntityID viego = SpawnChampion(
            world,
            entityMap,
            eChampion::VIEGO,
            eTeam::Blue,
            Vec3{});
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            eTeam::Red,
            Vec3{ 3.f, 0.f, 0.f });
        if (viego == NULL_ENTITY || target == NULL_ENTITY)
            return false;

        world.AddComponent<SkillStateComponent>(
            viego,
            SkillStateComponent{});
        FormOverrideComponent form{};
        form.baseChampion = eChampion::VIEGO;
        form.visualChampion = eChampion::ASHE;
        form.skillChampion = eChampion::ASHE;
        form.skillSlotMask = static_cast<u8_t>(
            1u << static_cast<u8_t>(eSkillSlot::BasicAttack));
        form.fRemainingSec = -1.f;
        form.bActive = true;
        world.AddComponent<FormOverrideComponent>(viego, form);

        const f32_t healthBefore =
            world.GetComponent<HealthComponent>(target).fCurrent;
        GameCommand attack{};
        attack.kind = eCommandKind::BasicAttack;
        attack.issuerEntity = viego;
        attack.targetEntity = target;
        attack.sequenceNum = 1u;
        attack.issuedAtTick = 1u;
        CGameRoomIntegrationProbeAccess::PushCommand(*room, attack);
        CGameRoomIntegrationProbeAccess::SetTickIndex(*room, 0u);
        CGameRoomIntegrationProbeAccess::RunFullTick(*room);

        if (!world.HasComponent<ActionStateComponent>(viego) ||
            world.GetComponent<ActionStateComponent>(viego).sourceChampion !=
                eChampion::ASHE ||
            world.GetComponent<ChampionComponent>(viego).id != eChampion::VIEGO ||
            world.GetComponent<StatComponent>(viego).championId != eChampion::VIEGO)
        {
            return false;
        }

        bool_t bHit = false;
        bool_t bSlowOwnedByViego = false;
        for (u32_t i = 0u; i < 120u && !bHit; ++i)
        {
            CGameRoomIntegrationProbeAccess::RunFullTick(*room);
            const f32_t currentHealth =
                world.GetComponent<HealthComponent>(target).fCurrent;
            if (currentHealth >= healthBefore - 0.0001f)
                continue;

            bHit = true;
            if (world.HasComponent<StatusEffectComponent>(target))
            {
                const StatusEffectComponent& statuses =
                    world.GetComponent<StatusEffectComponent>(target);
                for (u8_t statusIndex = 0u;
                    statusIndex < statuses.count;
                    ++statusIndex)
                {
                    const StatusEffectInstance& status =
                        statuses.active[statusIndex];
                    if (status.sourceEntity == viego &&
                        status.fRemainingSec > 0.f &&
                        status.fMoveSpeedMul < 1.f)
                    {
                        bSlowOwnedByViego = true;
                    }
                }
            }
        }

        const f32_t healthAfterHit =
            world.GetComponent<HealthComponent>(target).fCurrent;
        for (u32_t i = 0u; i < 10u; ++i)
            CGameRoomIntegrationProbeAccess::RunFullTick(*room);
        const f32_t healthAfterSettling =
            world.GetComponent<HealthComponent>(target).fCurrent;
        const bool_t bSingleHit =
            bHit && healthAfterHit > 0.f &&
            std::fabs(healthAfterSettling - healthAfterHit) <= 0.0001f;
        return bSingleHit && bSlowOwnedByViego;
    }
```

`main`의 기존 코드:

```cpp
    const bool_t bPassivePass = CheckPassiveExpiryBeforeFirstCommand();
    const bool_t bPass = bSkillPass &&
        bStructurePass &&
        bSkillGenerationPass &&
        bStructureGenerationPass &&
        bPassivePass;
```

아래로 교체:

```cpp
    const bool_t bPassivePass = CheckPassiveExpiryBeforeFirstCommand();
    const bool_t bSylasViegoRPass =
        CheckSylasStolenViegoRFullTickLifecycle();
    const bool_t bViegoAsheBAPass =
        CheckViegoBorrowedAsheBasicAttackFullTickLifecycle();
    const bool_t bPass = bSkillPass &&
        bStructurePass &&
        bSkillGenerationPass &&
        bStructureGenerationPass &&
        bPassivePass &&
        bSylasViegoRPass &&
        bViegoAsheBAPass;
```

기존 `std::printf` 블록 전체:

```cpp
    std::printf(
        "[GameRoomProjectileIntegration] %s: skill=%u structure=%u skill_generation=%u structure_generation=%u passive_pre_command=%u\n",
        bPass ? "PASS" : "FAIL",
        static_cast<u32_t>(bSkillPass),
        static_cast<u32_t>(bStructurePass),
        static_cast<u32_t>(bSkillGenerationPass),
        static_cast<u32_t>(bStructureGenerationPass),
        static_cast<u32_t>(bPassivePass));
```

아래로 교체:

```cpp
    std::printf(
        "[GameRoomProjectileIntegration] %s: skill=%u structure=%u skill_generation=%u structure_generation=%u passive_pre_command=%u sylas_viego_r=%u viego_ashe_ba=%u\n",
        bPass ? "PASS" : "FAIL",
        static_cast<u32_t>(bSkillPass),
        static_cast<u32_t>(bStructurePass),
        static_cast<u32_t>(bSkillGenerationPass),
        static_cast<u32_t>(bStructureGenerationPass),
        static_cast<u32_t>(bPassivePass),
        static_cast<u32_t>(bSylasViegoRPass),
        static_cast<u32_t>(bViegoAsheBAPass));
```

각각 아래로 교체:

```cpp
        if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world,
                spawnTick,
                source,
                target,
                eChampion::EZREAL,
                request))
```

### 2-12. 의도적으로 수정하지 않는 파일/경계

- snapshot/Event wire schema: `visualChampionId`, `skillChampionId`, action source champion/slot이 이미 존재하므로 필드를 추가하지 않는다.
- Client `ProjectileVisualCatalog.cpp`, `ObjectVisualDefs.json`, `ba_arrow.wfx`: 애쉬 BA kind→cue→FBX 연결과 실제 파일 존재가 확인됐으므로 수정하지 않는다.
- Engine: LoL 챔피언 분기나 Viego/Sylas 전용 상태를 넣지 않는다. Engine은 Client가 선택한 mesh/animation/FX descriptor만 재생한다.
- `ChampionComponent.id`/`StatComponent.championId`: 비에고/사일러스 본체 ID를 대여 챔피언으로 덮어쓰지 않는다.
- `SkillCast` replicated event: Client가 이 이벤트를 표현 경로로 사용하지 않고 ActionStart/EffectTrigger/snapshot이 이미 실행 정체성을 운반하므로 확장하지 않는다.

### 2-13. 조건부 추적 게이트 — 자동 probe 통과 후에도 인게임 사일러스 R이 안 움직일 때만

새 `RunSylasStolenViegoRExecutionProbe`가 통과하는데 인게임에서만 실패하면 gameplay 수식을 다시 바꾸지 않는다. 먼저 Debug 빌드에서 다음 4개 좌표를 동일 `issuer/sequence/tick`으로 1회성 `OutputDebugStringA` 추적한다.

1. `CommandExecutor::HandleCastSkill`: resolved hook champion/slot, groundPos, accepted/rejected reason.
2. `ViegoGameSim::OnR/Tick`: caster, start/end, duration, 완료 position.
3. 서버 SnapshotBuilder: Sylas net id와 authoritative position.
4. Client SnapshotApplier/interpolation: 같은 net id의 received/applied position.

이 추적으로 서버 snapshot까지 좌표가 바뀌면 Client interpolation/visual root 문제로, 서버 snapshot이 안 바뀌면 GameRoom tick ordering/상태 gate 문제로 다음 slice를 분리한다. 증거 없이 SnapshotApplier나 Viego R 이동 공식을 동시에 수정하지 않는다.

## 3. 검증·예측·누락

### 예측

- 비에고가 애쉬/이즈리얼/칼리스타 폼에서 BA하면 `CombatAction.eSourceChampion`은 빌린 챔피언이고, 본체 `ChampionComponent/StatComponent`는 VIEGO인 채 정확히 1개의 해당 projectile kind가 생성된다.
- 애쉬 폼 BA는 `AsheBasicAttack(18)` → `Ashe.BA.Arrow` → `Data/LoL/FX/Champions/Ashe/ba_arrow.wfx` → `ashe_base_aa_arrow.fbx`로 표현되며 즉시 hitscan DamageRequest를 만들지 않는다.
- 사일러스가 훔친 비에고 R은 클릭한 3m 지점까지 권위 이동하고 그 지점의 적만 피해 대상으로 잡으며, Action source는 VIEGO/R, 본체는 SYLAS, `ViegoSimComponent`는 생성되지 않는다.
- canonical `CGameRoom::Tick` 통합 probe에서도 사일러스→비에고 R 변위가 같아야 하며, 비에고→애쉬 BA는 실제 projectile phase를 거쳐 정확히 1회 피해와 비에고 source의 slow를 적용해야 한다.
- 실제 비에고가 possession 중 R을 쓰면 기존대로 폼/QWE runtime을 정리하고 원본 랭크/쿨다운을 복원한다.
- base Ashe/Ezreal/Kalista BA 보존은 기존 개별 projectile authority probe의 PASS로 판정하고, same-seed 두 실행은 결정성만 판정한다. 새 컴포넌트/직렬화/데이터 hash 변경은 기대하지 않는다.
- 다른 14개 챔피언은 이번 slice에서 자동으로 PASS 승격하지 않는다. 17챔피언 표는 RESULT에서 실측 상태로 갱신한다.

### 검증 명령

```powershell
git diff --check -- `
  Shared/GameSim/Champions/Ashe/AsheGameSim.h `
  Shared/GameSim/Champions/Ashe/AsheGameSim.cpp `
  Shared/GameSim/Champions/Ezreal/EzrealGameSim.h `
  Shared/GameSim/Champions/Ezreal/EzrealGameSim.cpp `
  Shared/GameSim/Champions/Kalista/KalistaGameSim.h `
  Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp `
  Shared/GameSim/Champions/Viego/ViegoGameSim.cpp `
  Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp `
  Shared/GameSim/Systems/Combat/CombatActionSystem.cpp `
  Tools/SimLab/main.cpp `
  Tools/Harness/GameRoomProjectileIntegrationProbe.cpp

msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe 600 1234
Tools/Bin/Debug/SimLab.exe 600 1234
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomProjectileIntegrationProbe.ps1

msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

두 SimLab 실행에서 다음 로그와 동일 final hash를 요구한다.

```text
[SimLab][ViegoBorrowedBA] PASS: Ashe/Ezreal/Kalista execution identity launches owned projectiles
[SimLab][SylasStolenViegoR] PASS: ground displacement, Viego action identity, no Viego owner-state leak
[SimLab][ViegoBorrowedRuntime] PASS: Ezreal pending cast cancelled at form exit
[SimLab] PASS
[GameRoomProjectileIntegration] PASS: ... sylas_viego_r=1 viego_ashe_ba=1
```

### 인게임 수동 게이트

1. 비에고로 애쉬/이즈리얼/칼리스타를 각각 possession하고 같은 원거리 대상에게 BA: 몸 FBX, 빌린 BA 애니메이션, 투사체 mesh, 적중 피해가 한 번씩 모두 보인다.
2. 애쉬 possession은 기본 상태와 Q 활성 상태를 나눠 화살 개수/slow/피해 중복이 없는지 본다.
3. 칼리스타 possession BA 적중 뒤 E가 Rend stack을 인식하고, 폼 종료 뒤 passive dash/runtime이 잔류하지 않는지 본다.
4. 사일러스로 비에고 R을 2m/4m/최대 사거리 밖에 각각 사용: 클릭/클램프 지점으로 이동하고 피해 중심과 모델 위치가 일치하는지 본다.
5. 비에고 본체가 다른 폼에서 R로 복귀: 원본 모델/QWE rank/cooldown, 폼 runtime 정리가 기존대로인지 본다.

### 미검증

- 사용자가 본 사일러스 R 인게임 실패의 정확한 서버/클라이언트 단절점은 새 변위 probe 및 수동 재현 전에는 확정하지 않는다.
- unit-level SimLab과 canonical GameRoom 통합 probe가 변경 전에도 변위를 PASS하면, 이번 생산 코드의 ViegoSim 누수 수정만으로 “이동 버그 해결”을 주장하지 않는다. 그 경우 snapshot/client 조건부 추적 결과가 별도 완료 조건이다.
- Client 화면의 실제 FBX/애니메이션/투사체 프레임은 자동 C++ probe만으로 증명하지 못하므로 인게임 캡처가 필요하다.
- 17챔피언 모든 QWE/R의 공식 의미·다단계·소환물·표식 수명은 이번 30% ceiling 밖이다. RESULT 매트릭스에서 `AUDIT_REQUIRED`를 거짓 PASS로 바꾸지 않는다.
- 이즈리얼 폼 종료 시 이미 발사된 투사체/W 표식을 제거할지의 원본 세부 의미는 이번 계획에서 변경하지 않는다. pending cast만 폼 소유 runtime으로 정리한다.

확인 필요:

- 새 자동 probe가 현재 코드에서 사일러스 R 최종 위치를 이미 PASS시키는 경우, 보고된 이동 실패는 Client/snapshot 조건부 추적 게이트로 넘기고 Viego dash 수식에는 추가 변경을 하지 않는다.
- 병행 Claude 세션이 위 10개 소스 파일을 수정한 경우 구현 세션 시작 시 최신 diff를 다시 읽고 앵커를 갱신한다. 사용자 변경을 덮어쓰지 않는다.

## 서브 에이전트 비평

독립 read-only 비평 완료: `item_plan_critique`(Descartes), `resource_plan_critique`(Gibbs). P0는 두 비평 모두 없음으로 판정했다.

| 등급 | 비평 | 판정 | 계획서 반영 |
|---|---|---|---|
| P1 | 사일러스-비에고 R probe가 `ViegoSimComponent` 누수에서 조기 실패하면 변위를 관찰하지 못한다. | 수용 | 캐스팅 시 누수 여부를 별도로 기록하되 끝까지 tick하고, 변위와 runtime 누수를 독립 결과로 출력하도록 2-9를 수정했다. |
| P1 | unit-level `ViegoGameSim::Tick`만으로는 실제 `CGameRoom::Tick` phase ordering을 검증하지 못한다. | 수용 | canonical harness에 실제 GameRoom 전체 틱을 통과하는 `CheckSylasStolenViegoRFullTickLifecycle`를 추가했다. |
| P1 | possession 종료 시 새로 정리할 `EzrealPendingCastComponent`의 회귀 probe가 없다. | 수용 | 실제 `ViegoGameSim::ClearPossession`을 호출하고 지연 Q가 나중에 발사되지 않음을 검사하는 2-10 probe를 추가했다. |
| P1 | 애쉬 possession BA의 generic `EffectTrigger` 억제 계약이 테스트되지 않는다. | 수용 | 애쉬는 generic BA cue 0회, 이즈리얼/칼리스타는 각 1회라는 cue 계수 단언을 2-8에 추가했다. |
| P1 | 칼리스타 Martial Poise arm/consume 경로가 여전히 물리 `KALISTA` 판정이라 possession에서 막힌다. | 수용 | `ActionState.sourceChampion == KALISTA` 실행 정체성 판정으로 교체하고 BA 뒤 move-command dash probe를 추가했다. |
| P1 | `resolvedChampion`은 변경 뒤 미사용 지역 변수가 된다. | 수용 | 선언 삭제를 2-7에 명시했다. |
| P1 | 계획서 표준 머리말은 `좌표:` 형식이어야 한다. | 수용 | 문서 머리말을 표준 3줄 형식으로 교정했다. |
| P2 | 투사체 생성만으로 실제 적중·상태이상·스택 소유권까지 증명할 수 없다. | 부분 수용 | SimLab 직접 hit-handler 단언과 canonical GameRoom 애쉬 실투사체 적중/slow 검증을 추가했다. 이즈리얼·칼리스타의 전체 실투사체 수명은 기존 base authority probe 보존 및 인게임 게이트로 확인하고, 이번 slice를 17챔피언 전수 통합 재작성으로 확장하지 않는다. |

비평 반영 결론: 구현 게이트는 `REVIEWED`. 다만 “사일러스가 비에고 R을 훔쳤을 때 이동하지 않는다”의 정확한 원인은 아직 `CONFIRM_NEEDED`이며, 자동 probe가 변경 전부터 변위를 통과하면 생산 이동 수식을 임의 변경하지 않고 snapshot/client 조건부 추적으로 전환한다.
