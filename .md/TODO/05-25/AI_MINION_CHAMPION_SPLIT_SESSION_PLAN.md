Session - AI minion/champion split을 서버 권위 흐름 기준으로 작은 세션들로 나눠 완성한다.

1. 반영해야 하는 코드

1-0. 현재 권위 흐름 고정

현재 흐름은 아래 순서를 보존한다.

```text
Client Input
-> GameCommand
-> Server GameSim
-> ReplicatedEvent/Snapshot
-> Client Visual
```

C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 서버 tick 흐름 기준:

```cpp
    Phase_DrainCommands(tc);
    Phase_ServerBotAI(tc);
    Phase_ExecuteCommands(tc);
    Phase_SimulationSystems(tc);
```

```cpp
    CAttackChaseSystem::Execute(m_world, tc, m_pendingExecCommands);
    Phase_ExecuteCommands(tc);
```

```cpp
    Phase_ServerMinionWave(tc);
    Phase_ServerMinionAI(tc);
```

정리 원칙:
- 챔피언 AI는 `GameCommand` 생산자다.
- 기본 공격의 최종 사거리 판정은 `CommandExecutor`와 `AttackChase`의 서버 GameSim 판정으로 모은다.
- 서버 미니언 AI는 서버 전용 시뮬레이션이다. 미니언은 당장 `GameCommand` 생산자로 바꾸지 않는다.
- 클라이언트와 Engine legacy AI는 입력, 예측, 시각화, 디버그 또는 local smoke로 격리한다.

1-1. Session 00 - 최소 핫픽스 기준선 확정

C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/CoreSystems.cpp

`CAISystem` legacy/simple path가 `AIStateComponent::attackRange`만 보지 않고 `StatComponent::attackRange`를 먼저 읽도록 둔다. 이 세션은 이미 반영된 핫픽스의 기준선이다.

파일 상단 include에 아래가 있어야 한다.

```cpp
#include "../../../../Shared/GameSim/Components/StatComponent.h"
```

`namespace` 안에 아래 helper가 있어야 한다.

```cpp
    f32_t ResolveAIAttackRange(CWorld& world, EntityID entity, const AIStateComponent& ai)
    {
        if (world.HasComponent<StatComponent>(entity))
        {
            const auto& stat = world.GetComponent<StatComponent>(entity);
            if (stat.attackRange > 0.f)
                return stat.attackRange;
        }

        return ai.attackRange;
    }
```

`CAISystem::DescribeAccess()`에는 아래 read가 있어야 한다.

```cpp
        access.Read<StatComponent>();
```

`CAISystem::Execute()`의 lambda는 `EntityID entity`를 이름 있게 받고, 공격 판정은 아래 형태여야 한다.

```cpp
            const f32_t attackRange = ResolveAIAttackRange(world, entity, ai);

            if (fDist <= attackRange)
            {
                ai.current = AIStateComponent::State::Attack;
                vel.fSpeed = 0.f;
            }
```

1-2. Session 01 - BasicAttack range query를 Shared GameSim 서버 경로에 만든다

새 파일: C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Combat/AttackRangeQuery.h

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

class CWorld;

namespace AttackRangeQuery
{
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity);
    f32_t ResolveBasicAttackRange(CWorld& world, EntityID entity);
    f32_t ResolveBasicAttackDamage(CWorld& world, EntityID entity);
    f32_t ResolveEffectiveBasicAttackRange(CWorld& world, EntityID issuer, EntityID target);
    bool_t IsInEffectiveBasicAttackRange(CWorld& world, EntityID issuer, EntityID target);
}
```

새 파일: C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Combat/AttackRangeQuery.cpp

```cpp
#include "Shared/GameSim/Systems/Combat/AttackRangeQuery.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>

namespace AttackRangeQuery
{
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return 1.2f;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return 0.5f;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return 1.5f;

        return 0.5f;
    }

    f32_t ResolveBasicAttackRange(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            const f32_t range = world.GetComponent<StatComponent>(entity).attackRange;
            if (range > 0.f)
                return range;
        }

        if (entity != NULL_ENTITY && world.HasComponent<MinionStateComponent>(entity))
        {
            const f32_t range = world.GetComponent<MinionStateComponent>(entity).attackRange;
            if (range > 0.f)
                return range;
        }

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
        {
            const auto& champion = world.GetComponent<ChampionComponent>(entity);
            const f32_t range = BuildDefaultChampionStat(champion.id).attackRange;
            if (range > 0.f)
                return range;
        }

        return 5.5f;
    }

    f32_t ResolveBasicAttackDamage(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            const f32_t damage = world.GetComponent<StatComponent>(entity).ad;
            if (damage > 0.f)
                return damage;
        }

        if (entity != NULL_ENTITY && world.HasComponent<MinionStateComponent>(entity))
        {
            const f32_t damage = world.GetComponent<MinionStateComponent>(entity).attackDamage;
            if (damage > 0.f)
                return damage;
        }

        return 55.f;
    }

    f32_t ResolveEffectiveBasicAttackRange(CWorld& world, EntityID issuer, EntityID target)
    {
        return ResolveBasicAttackRange(world, issuer) +
            ResolveGameplayRadius(world, issuer) +
            ResolveGameplayRadius(world, target);
    }

    bool_t IsInEffectiveBasicAttackRange(CWorld& world, EntityID issuer, EntityID target)
    {
        if (issuer == NULL_ENTITY || target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(issuer) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        const Vec3 issuerPos =
            world.GetComponent<TransformComponent>(issuer).GetLocalPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetLocalPosition();
        const f32_t range = ResolveEffectiveBasicAttackRange(world, issuer, target);
        return WintersMath::DistanceSqXZ(issuerPos, targetPos) <= range * range;
    }
}
```

C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\Combat\CombatFormula.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\Combat\AttackRangeQuery.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\Combat\CombatFormula.h" />
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\Combat\AttackRangeQuery.h" />
```

C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj.filters

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\Combat\CombatFormula.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\Combat\AttackRangeQuery.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\Combat\CombatFormula.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\Combat\AttackRangeQuery.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

1-3. Session 02 - CommandExecutor와 AttackChase의 사거리 중복을 제거한다

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 include:

```cpp
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/Combat/AttackRangeQuery.h"
```

기존 local helper 삭제:

```cpp
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return 1.2f;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return 0.5f;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return 1.5f;

        return 0.5f;
    }
```

기존 코드:

```cpp
            if (tc.pWalkable->TryClampMoveSegmentXZ(origin, dest, ResolveGameplayRadius(world, cmd.issuerEntity), guardedDest))
```

아래로 교체:

```cpp
            if (tc.pWalkable->TryClampMoveSegmentXZ(
                origin,
                dest,
                AttackRangeQuery::ResolveGameplayRadius(world, cmd.issuerEntity),
                guardedDest))
```

기존 Viego consume range 코드:

```cpp
        const f32_t effectiveRange =
            kConsumeRange + ResolveGameplayRadius(world, cmd.issuerEntity) +
            ResolveGameplayRadius(world, cmd.targetEntity);
```

아래로 교체:

```cpp
        const f32_t effectiveRange =
            kConsumeRange +
            AttackRangeQuery::ResolveGameplayRadius(world, cmd.issuerEntity) +
            AttackRangeQuery::ResolveGameplayRadius(world, cmd.targetEntity);
```

`HandleBasicAttack`의 기존 range/damage resolve 블록:

```cpp
    f32_t range = 5.5f;
    f32_t damage = 55.f;

    if (world.HasComponent<StatComponent>(cmd.issuerEntity))
    {
        const auto& stat = world.GetComponent<StatComponent>(cmd.issuerEntity);
        if (stat.attackRange > 0.f)
            range = stat.attackRange;
        if (stat.ad > 0.f)
            damage = stat.ad;
    }
```

아래로 교체:

```cpp
    const f32_t damage =
        AttackRangeQuery::ResolveBasicAttackDamage(world, cmd.issuerEntity);
```

`HandleBasicAttack`의 기존 effective range 코드:

```cpp
    const f32_t effectiveRange =
        range +
        ResolveGameplayRadius(world, cmd.issuerEntity) +
        ResolveGameplayRadius(world, cmd.targetEntity);
```

아래로 교체:

```cpp
    const f32_t effectiveRange =
        AttackRangeQuery::ResolveEffectiveBasicAttackRange(
            world,
            cmd.issuerEntity,
            cmd.targetEntity);
```

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp

기존 include:

```cpp
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/Combat/AttackRangeQuery.h"
```

기존 local helper 삭제:

```cpp
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);

        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return 1.2f;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return 0.5f;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return 1.5f;

        return 0.5f;
    }

    f32_t ResolveAttackRange(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<StatComponent>(entity))
        {
            const f32_t range = world.GetComponent<StatComponent>(entity).attackRange;
            if (range > 0.f)
                return range;
        }

        return 5.5f;
    }
```

기존 effective range 코드:

```cpp
        const f32_t effectiveRange =
            ResolveAttackRange(world, entity) +
            ResolveGameplayRadius(world, entity) +
            ResolveGameplayRadius(world, chase.target);
```

아래로 교체:

```cpp
        const f32_t effectiveRange =
            AttackRangeQuery::ResolveEffectiveBasicAttackRange(
                world,
                entity,
                chase.target);
```

1-4. Session 03 - Champion AI의 basic attack emit 판정을 같은 query로 맞춘다

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 include:

```cpp
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/Combat/AttackRangeQuery.h"
```

기존 local helper 삭제:

```cpp
    f32_t ResolveAttackRange(CWorld& world, EntityID self, eChampion champion)
    {
        if (world.HasComponent<StatComponent>(self))
        {
            const f32_t range = world.GetComponent<StatComponent>(self).attackRange;
            if (range > 0.f)
                return range;
        }

        return BuildDefaultChampionStat(champion).attackRange;
    }
```

`EmitBasicAttackCommand`의 기존 판정:

```cpp
        const f32_t attackRange = ResolveAttackRange(world, self, champion);
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            return false;
        }
```

아래로 교체:

```cpp
        const f32_t effectiveRange =
            AttackRangeQuery::ResolveEffectiveBasicAttackRange(world, self, target);
        if (effectiveRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > effectiveRange * effectiveRange)
        {
            return false;
        }
```

`BuildChampionAIContext`의 기존 코드:

```cpp
        ctx.attackRange = ResolveAttackRange(world, self, champion.id);
```

아래로 교체:

```cpp
        ctx.attackRange = AttackRangeQuery::ResolveBasicAttackRange(world, self);
```

이 세션의 목표는 챔피언 봇이 `StatComponent.attackRange`와 gameplay radius를 반영한 basic attack 판단을 하게 만드는 것이다. 스킬 사거리는 계속 `GetDefaultChampionSkillRange()` 계열을 따른다.

1-5. Session 04 - ChampionAI 대형 파일을 context/emit/behavior로 1차 분리한다

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

CONFIRM_NEEDED: 이 세션 시작 시 파일 전체를 다시 열어 정확한 helper 경계와 include 의존성을 확인한다. 새 h/cpp 본문은 규칙상 완전한 코드 블록으로 작성해야 하므로, 여기서는 pseudo-code를 두지 않는다.

분리 후보:
- `ChampionAIContext`와 context build/query helper
- `MakeAICommand`, `EmitMoveCommand`, `EmitBasicAttackCommand`, `EmitSkillCommand`, `EmitRecallCommand`
- lane combat/farm/harass behavior helper

새 파일 후보:
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIContext.h
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIContext.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAICommandEmitter.h
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAICommandEmitter.cpp

C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

CONFIRM_NEEDED: 새 cpp가 확정되면 `ChampionAISystem.cpp` 근처에 `ClCompile`, 새 header는 `ClInclude`에 추가한다.

C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj.filters

CONFIRM_NEEDED: 새 파일이 확정되면 `04. Shared\GameSim\Systems` filter에 추가한다.

1-6. Session 05 - Champion AI behavior/policy 경계를 정리한다

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h

CONFIRM_NEEDED: 이 세션에서는 policy가 순수 데이터와 결정 파라미터만 가지는지 확인한다. behavior 실행 코드가 policy 파일로 넘어가지 않게 유지한다.

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

CONFIRM_NEEDED: `TryExecuteMinionFarm`, `ExecuteLaneCombat`, combo/harass/farm leaf들을 behavior 파일로 옮기는 정확한 교체 블록을 만든다. 이때 `CChampionAISystem::Execute()`는 tick entry와 component iteration만 남기는 방향으로 줄인다.

원칙:
- 챔피언 AI는 계속 `std::vector<GameCommand>& outCommands`에 command만 넣는다.
- damage, cooldown, final range truth는 `CommandExecutor`, `CombatActionSystem`, `AttackChaseSystem` 쪽이 담당한다.
- debug trace는 `WintersOutputAIDebugStringA` 경로를 유지한다.

1-7. Session 06 - Server minion AI를 GameRoom에서 서버 전용 시스템으로 추출한다

C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 anchor:

```cpp
void CGameRoom::Phase_ServerMinionAI(TickContext& tc)
```

CONFIRM_NEEDED: 이 함수 전체와 주변 helper를 다시 열어 새 파일 본문을 완전 코드로 작성한다. 미니언은 현 단계에서 `GameCommand`로 바꾸지 않고, 기존처럼 server direct sim과 `DamageRequest` 흐름을 유지한다.

C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 anchor:

```cpp
    void Phase_ServerMinionAI(TickContext& tc);
```

CONFIRM_NEEDED: 추출 후에도 `Phase_ServerMinionAI` wrapper를 남길지, 새 `ServerMinionAISystem`을 직접 호출할지 결정한다. GameRoom 공개/비공개 경계가 바뀌면 정확한 선언 교체 블록을 작성한다.

새 파일 후보:
- C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerMinionAISystem.h
- C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionAISystem.cpp

참조할 기존 파일:
- C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerMinionTuning.h
- C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerMinionWaveRuntime.h
- C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionWaveRuntime.cpp

1-8. Session 07 - Minion target/range helper를 서버 미니언 시스템 내부로 모은다

C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionAISystem.cpp

CONFIRM_NEEDED: Session 06 추출 결과를 기준으로 작성한다.

정리 방향:
- minion target scan, lane target priority, attack windup/recovery, damage enqueue를 `ServerMinionAISystem` 내부 helper로 묶는다.
- `ServerMinionTuning.h`는 숫자와 role/lane tuning만 유지한다.
- `AttackRangeQuery`를 사용할지 여부는 minion의 현재 `MinionStateComponent.attackRange` 의미와 gameplay radius 적용 여부를 확인한 뒤 결정한다.

1-9. Session 08 - Legacy AI와 서버 권위 AI 경계를 문서/검색 기준으로 잠근다

C:/Users/tnest/Desktop/Winters/Engine/Private/ECS/Systems/CoreSystems.cpp

추가 코드 변경은 기본적으로 하지 않는다. `CAISystem`은 legacy/simple path로 남기고, 서버 챔피언 봇 판단의 주 경로가 아님을 검색으로 확인한다.

검색 기준:

```powershell
rg -n "AIStateComponent|CAISystem|AddComponent<AIStateComponent>" Engine Server Shared Client
```

정리 기준:
- 서버 normal F5 flow에서 챔피언 봇은 `CChampionAISystem::Execute()`를 탄다.
- 서버 normal F5 flow에서 미니언은 `Phase_ServerMinionAI` 또는 추출 후 `ServerMinionAISystem`을 탄다.
- Engine `CAISystem`은 local smoke나 legacy entity가 있을 때만 관여한다.

1-10. Session 09 - Runtime debug proof를 추가한다

CONFIRM_NEEDED: 실제 디버그 표시는 기존 UI와 log 위치를 다시 확인한 뒤 작성한다.

확인 후보:
- champion bot: `EmitBasicAttackCommand`가 effective range 안에서만 command를 내는지
- command executor: out-of-range basic attack이 `AttackChaseComponent`로 이어지는지
- attack chase: range 안에 들어오면 basic attack command를 다시 넣는지
- minion: wave, lane move, target scan, windup hit가 기존처럼 서버 snapshot/event로 보이는지

2. 검증

2-1. 매 세션 공통 정적 검증

```powershell
git diff --check
```

```powershell
rg -n "ResolveGameplayRadius|ResolveAttackRange|ResolveBasicAttackRange|ResolveEffectiveBasicAttackRange|AttackRangeQuery" Shared\GameSim\Systems Server Engine
```

```powershell
rg -n "Phase_ServerBotAI|Phase_ServerMinionAI|CChampionAISystem::Execute|CAttackChaseSystem::Execute|HandleBasicAttack" Server\Private\Game Shared\GameSim\Systems
```

2-2. 빌드 검증

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

2-3. 런타임 검증

F5 normal flow에서 확인한다.

- 챔피언 봇이 `StatComponent.attackRange`와 gameplay radius 기준으로 Attack/Chase를 고른다.
- basic attack command가 너무 멀리서 들어오면 `CommandExecutor`가 reject만 반복하지 않고 `AttackChaseComponent`를 만든다.
- `AttackChaseSystem`이 사거리 안에 들어오면 basic attack command를 재발행하고 chase를 종료한다.
- 미니언 wave spawn, lane move, target scan, attack windup, damage enqueue가 기존처럼 동작한다.
- client는 snapshot/event를 시각화만 하며 gameplay truth를 바꾸지 않는다.

2-4. 현재 기준선 검증 결과

Session 00 핫픽스 기준선은 아래 검증을 통과했다.

```text
git diff --check: passed
Server Debug x64 build: passed, warnings only
Client Debug x64 build: passed, warnings only
```
