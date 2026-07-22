Session - 파괴된 포탑 캐시를 제거하고 봇 퇴각·귀환을 살아있는 방어 구조물에 재결합
좌표: S10 후속 · 축: C4 수명은 선언된다 · C7 권위와 정합성
관련: 2026-07-18_STRUCTURE_DESTRUCTION_REMNANT_PLAN.md · 2026-07-18_CHAMPIONAI_MID_DEFENSE_LIFECYCLE_PLAN.md · CLAUDE_Legacy.md

## 1. 결정 기록

① 문제: `safeAnchor/retreatGoal`은 봇 생성·스테이지 초기화 때만 계산되고 런타임 갱신은 0회다. 그래서 미드·탑 외곽 포탑 1기가 죽어도 `Retreat`, `MoveToOuterTurret`, 귀환 복귀가 같은 죽은 좌표를 계속 소비한다.
② 순진한 해법이 실패하는 이유: `HasAlliedOuterTurretLost`는 이미 사망을 감지하지만 이동 목적지는 캐시된 `Vec3`다. 상태명 변경, 적 감지 추가, 서버 `RefreshChampionAIGoals()`의 30 Hz 호출은 stale 원인과 waypoint 무방비 fallback을 함께 제거하지 못한다.
③ 채택: Shared `CChampionAISystem`이 현재 ECS 구조물 생존 상태로 단일 resolver를 제공하고, context 구축 때 home lane 기준으로 재평가한다. 우선순위는 살아있는 동일 라인 포탑 → 살아있는 Nexus tier 쌍둥이 포탑 → 살아있는 Nexus → 기존 fallback이다.
④ 대안 대비: 구조물 사망 이벤트에 AI dirty flag를 연결하면 평균 스캔은 줄지만 Death/복원/스폰 경로 결합이 늘어난다. 현재 30 Hz, 최대 10봇, 소수 구조물에서는 결정론적 live scan이 더 작고 복원에도 안전하다.
⑤ 현재 비용/후속: `O(bot × structure)`를 context 생성마다 지불한다. 구조물 수가 수백 단위로 늘거나 프로파일링 예산을 넘을 때만 world structure revision 기반 캐시를 후속으로 검토한다. 현재 성능 실측은 없음.

## 2. 현재 코드 증거와 구현 지시

### 2.1 근본 원인과 권위 경계

- `Server/Private/Game/GameRoomChampionAI.cpp:57-156`의 현재 resolver 자체는 `HealthComponent`가 죽은 포탑을 제외한다. 그러나 `RefreshChampionAIGoals()` 호출은 `Server/Private/Game/GameRoomSpawn.cpp:166,210`뿐이고, 생성 직후 직접 대입도 `:651-652`뿐이다. 파괴 뒤 재계산이 없으므로 올바른 필터가 실행되지 않는다.
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp:3233-3247`, `:3266-3271`, `:3967-3993`은 각각 저체력 퇴각, 귀환 종료 뒤 라인 복귀, 웨이브 대기 이동에서 캐시된 `retreatGoal/safeAnchor`를 소비한다. 탑 애니와 미드 저체력 봇은 서로 다른 상태에서 같은 stale 좌표를 읽는 동일 결함이다.
- `IsAliveTarget()`은 파괴 잔해의 `world.IsAlive(entity)==true`와 별개로 `HealthComponent.bIsDead/fCurrent`를 검사한다. 따라서 잔해 보존 구현을 되돌릴 필요가 없고 Targetable/Structure death 계약도 바꾸지 않는다.
- Bot AI는 오직 `GameCommand` 생산자다. 구조물 체력·사망·타게팅 진실은 변경하지 않고, 살아있는 월드 사실로 이동/귀환 목적지만 다시 계산한다.
- 저체력 퇴각은 hard safety이므로 적 챔피언·미니언을 새로 교전 대상으로 삼지 않는다. 이번 결함은 “적을 무시한다”가 아니라 “보호 불가능한 죽은 좌표를 안전 지점으로 믿는다”는 데 있다.

### 2.2 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h`

현재 앵커:

```cpp
class CWorld;
struct ChampionAIComponent;
```

아래로 교체해 공개 resolver 시그니처의 팀 타입을 전방 선언한다.

```cpp
class CWorld;
enum class eTeam : uint8_t;
struct ChampionAIComponent;
```

현재 앵커:

```cpp
public:
	static void Execute(CWorld& world,
```

위 앵커 바로 아래가 아니라 `public:` 바로 아래에 다음 선언을 추가한다. Server 초기값과 Shared 런타임 갱신이 같은 함수를 사용하게 해 알고리즘을 이중 소유하지 않는다.

```cpp
	static Vec3 ResolveSafeAnchor(
		CWorld& world,
		eTeam team,
		u8_t lane,
		const Vec3& fallback);

```

### 2.3 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` — 살아있는 방어 구조물 resolver

현재 앵커:

```cpp
    constexpr f32_t kChampionAIMidDefenseBehindDistance = 2.25f;
```

아래에 추가:

```cpp
    constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;
```

현재 익명 namespace 종료 앵커:

```cpp
    }
}

AiDecisionTraceV1 CChampionAISystem::BuildResearchDecisionTrace(
```

`AiDecisionTraceV1 ...` 바로 위에 다음 전체 함수를 추가한다.

```cpp
Vec3 CChampionAISystem::ResolveSafeAnchor(
    CWorld& world,
    eTeam team,
    u8_t lane,
    const Vec3& fallback)
{
    const u32_t turretKind =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
    const u32_t nexusKind =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
    const u32_t nexusTier =
        static_cast<u32_t>(Winters::Map::eTurretTier::Nexus);

    EntityID bestLaneTurret = NULL_ENTITY;
    u32_t bestLaneTier = (std::numeric_limits<u32_t>::max)();
    Vec3 bestLaneTurretPos{};

    EntityID bestBaseTurret = NULL_ENTITY;
    f32_t bestBaseDistanceSq = (std::numeric_limits<f32_t>::max)();
    Vec3 bestBaseTurretPos{};

    EntityID bestNexus = NULL_ENTITY;
    Vec3 bestNexusPos{};

    world.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID entity,
            StructureComponent& structure,
            TransformComponent& transform)
        {
            if (structure.team != team || !IsAliveTarget(world, entity))
                return;

            const Vec3 position = transform.GetPosition();
            if (structure.kind == nexusKind)
            {
                if (bestNexus == NULL_ENTITY || entity < bestNexus)
                {
                    bestNexus = entity;
                    bestNexusPos = position;
                }
                return;
            }

            if (structure.kind != turretKind)
                return;

            if (structure.lane == static_cast<u32_t>(lane) &&
                (bestLaneTurret == NULL_ENTITY ||
                    structure.tier < bestLaneTier ||
                    (structure.tier == bestLaneTier && entity < bestLaneTurret)))
            {
                bestLaneTurret = entity;
                bestLaneTier = structure.tier;
                bestLaneTurretPos = position;
            }

            if (structure.tier == nexusTier)
            {
                const f32_t distanceSq =
                    WintersMath::DistanceSqXZ(fallback, position);
                if (bestBaseTurret == NULL_ENTITY ||
                    distanceSq < bestBaseDistanceSq ||
                    (distanceSq == bestBaseDistanceSq && entity < bestBaseTurret))
                {
                    bestBaseTurret = entity;
                    bestBaseDistanceSq = distanceSq;
                    bestBaseTurretPos = position;
                }
            }
        });

    const bool_t bHasLaneTurret = bestLaneTurret != NULL_ENTITY;
    const bool_t bHasBaseTurret = bestBaseTurret != NULL_ENTITY;
    if (!bHasLaneTurret && !bHasBaseTurret)
    {
        Vec3 result = bestNexus != NULL_ENTITY ? bestNexusPos : fallback;
        result.y = 1.f;
        return result;
    }

    const Vec3 turretPos = bHasLaneTurret
        ? bestLaneTurretPos
        : bestBaseTurretPos;
    Vec3 baseDirection = bestNexus != NULL_ENTITY
        ? WintersMath::DirectionXZ(turretPos, bestNexusPos)
        : Vec3{ team == eTeam::Blue ? 1.f : -1.f, 0.f, 0.f };
    if (baseDirection.x == 0.f && baseDirection.z == 0.f)
    {
        baseDirection =
            Vec3{ team == eTeam::Blue ? 1.f : -1.f, 0.f, 0.f };
    }

    Vec3 result{
        turretPos.x + baseDirection.x * kChampionAISafeAnchorBehindTurret,
        1.f,
        turretPos.z + baseDirection.z * kChampionAISafeAnchorBehindTurret
    };
    return result;
}

```

선택 규칙은 다음 계약을 가진다.

- 동일 home lane에서는 `Outer(0) → Inner(1) → Inhibitor(2)` 중 살아있는 가장 바깥 포탑을 선택한다. 파괴된 Outer가 잔해로 남아 있어도 `IsAliveTarget()`에서 탈락한다.
- 동일 라인 포탑이 전부 죽었으면 `tier=Nexus`인 살아있는 쌍둥이 포탑 중 기존 anchor와 가까운 것을 선택한다. 쌍둥이까지 모두 죽었을 때만 살아있는 Nexus 좌표로 물러난다.
- 포탑 anchor는 포탑 중심이 아니라 Nexus 방향으로 3 m 뒤다. 봇이 포탑 앞에서 귀환하는 문제를 피하고 기존 safe-anchor 오프셋을 보존한다.
- 구조물·Nexus가 모두 없는 SimLab 최소 fixture에서는 기존 fallback을 유지한다. 정상 경기에서는 Nexus death가 게임 종료와 겹치므로 이 분기는 보호 지점 보장을 약속하지 않는다.

### 2.4 `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp` — 매 context에서 캐시 무효화

현재 앵커:

```cpp
        ctx.attackRange = ResolveAttackRange(world, self, tc, champion.id);
        const u8_t activeLane = ResolveChampionAIActiveLane(ai);

        EntityID targetChampion = ai.lockedChampion;
```

아래로 교체:

```cpp
        ctx.attackRange = ResolveAttackRange(world, self, tc, champion.id);
        const u8_t activeLane = ResolveChampionAIActiveLane(ai);
        ai.safeAnchor = CChampionAISystem::ResolveSafeAnchor(
            world,
            champion.team,
            ai.lane,
            ai.safeAnchor);
        ai.retreatGoal = ai.safeAnchor;

        EntityID targetChampion = ai.lockedChampion;
```

`activeLane`이 아니라 `ai.lane`을 넘기는 것이 의도다. 미드 수비 rotation 중에도 저체력 hard-safety 퇴각은 자신의 home lane에서 살아있는 포탑으로 돌아가며, `midDefenseAnchor`는 기존 `ResolveMidDefenseAnchor()`가 별도로 현재 수명을 반영한다. 이 위치는 `decisionTimer > 0` 조기 반환과 저체력 emergency 판정보다 앞이므로 파괴 다음 AI 틱(30 Hz 기준 최대 약 33.3 ms)에 stale 목적지가 교체된다.

### 2.5 `Server/Private/Game/GameRoomChampionAI.cpp` — 초기화도 Shared resolver에 위임

현재 include/namespace 블록:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
    constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;
    constexpr u8_t kChampionAIMidLane =
        static_cast<u8_t>(Winters::Map::eLane::Mid);
}
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr u8_t kChampionAIMidLane =
        static_cast<u8_t>(Winters::Map::eLane::Mid);
}
```

현재 `ResolveChampionAISafeAnchor` 전체 본문(`Vec3 CGameRoom::...`부터 다음 `RefreshChampionAIGoals` 직전까지)을 아래로 교체:

```cpp
Vec3 CGameRoom::ResolveChampionAISafeAnchor(eTeam team, u8_t lane)
{
    return CChampionAISystem::ResolveSafeAnchor(
        m_world,
        team,
        lane,
        GetGameSimLaneGatherPosition(lane, TeamByte(team)));
}

```

`RefreshChampionAIGoals()`를 매 틱 호출하지 않는다. 이 함수는 spawn/stage 초기화 역할을 유지하고, 런타임 truth 갱신은 Shared AI context가 소유한다. Server 전용 waypoint 계산과 Shared 런타임 계산이 서로 다른 포탑을 고르는 이중 경로도 제거된다.

### 2.6 `Tools/SimLab/main.cpp` — 탑 애니와 미드 저체력 재현을 기존 AI probe에 고정

`RunChampionAIMidDefenseDeterminismProbe()`의 기존 `SpawnStructure` lambda를 재사용한다. 현재 앵커:

```cpp
        if (engagedState.state != eChampionAIState::LaneCombat ||
            engagedState.intent != eChampionAIIntent::AttackChampion)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: in-range enemy ignored during rotation state=%u intent=%u\n",
                static_cast<unsigned>(engagedState.state),
                static_cast<unsigned>(engagedState.intent));
            return false;
        }

        std::printf(
            "[SimLab][ChampionAI][MidDefense] PASS: deterministic hash=%016llX home lane preserved, PlayerLike bot grouped mid after commitment gate\n",
            static_cast<unsigned long long>(runA.hash));
```

아래로 교체:

```cpp
        if (engagedState.state != eChampionAIState::LaneCombat ||
            engagedState.intent != eChampionAIIntent::AttackChampion)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: in-range enemy ignored during rotation state=%u intent=%u\n",
                static_cast<unsigned>(engagedState.state),
                static_cast<unsigned>(engagedState.intent));
            return false;
        }

        const auto RunLiveSafeAnchorCase = [&](Winters::Map::eLane lane,
                                                bool_t bLowHp,
                                                u64_t seed) -> bool_t
        {
            CWorld safeWorld;
            DeterministicRng safeRng(seed);
            EntityIdMap safeEntityMap;
            FlatWalkable safeWalkable;

            const EntityID bot = SpawnChampion(
                safeWorld,
                safeEntityMap,
                eChampion::ANNIE,
                static_cast<u8_t>(eTeam::Red),
                5u);
            if (bot == NULL_ENTITY)
                return false;

            safeWorld.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
            safeWorld.GetComponent<GoldComponent>(bot).amount = 0u;

            ChampionAIComponent safeAI{};
            safeAI.champion = eChampion::ANNIE;
            safeAI.team = eTeam::Red;
            safeAI.difficulty = 2u;
            safeAI.brainType = eChampionAIBrainType::PlayerLike;
            safeAI.lane = static_cast<u8_t>(lane);
            safeAI.activeLane = safeAI.lane;
            safeAI.state = bLowHp
                ? eChampionAIState::LaneCombat
                : eChampionAIState::MoveToOuterTurret;
            safeAI.intent = eChampionAIIntent::FarmMinion;
            safeAI.decisionTimer = 0.f;
            safeAI.laneGoal = Vec3{ -5.f, 1.f, 0.f };
            safeAI.safeAnchor = Vec3{ -13.f, 1.f, 0.f };
            safeAI.retreatGoal = safeAI.safeAnchor;
            safeWorld.AddComponent<ChampionAIComponent>(bot, safeAI);

            const EntityID deadOuter = SpawnStructure(
                safeWorld,
                eTeam::Red,
                Winters::Map::eObjectKind::Structure_Turret,
                Winters::Map::eTurretTier::Outer,
                lane,
                Vec3{ -10.f, 0.f, 0.f },
                true);
            const EntityID liveInner = SpawnStructure(
                safeWorld,
                eTeam::Red,
                Winters::Map::eObjectKind::Structure_Turret,
                Winters::Map::eTurretTier::Inner,
                lane,
                Vec3{ -20.f, 0.f, 0.f },
                false);
            const EntityID liveBaseTurret = SpawnStructure(
                safeWorld,
                eTeam::Red,
                Winters::Map::eObjectKind::Structure_Turret,
                Winters::Map::eTurretTier::Nexus,
                Winters::Map::eLane::Base,
                Vec3{ -32.f, 0.f, 0.f },
                false);
            const EntityID liveNexus = SpawnStructure(
                safeWorld,
                eTeam::Red,
                Winters::Map::eObjectKind::Structure_Nexus,
                Winters::Map::eTurretTier::Nexus,
                Winters::Map::eLane::Base,
                Vec3{ -40.f, 0.f, 0.f },
                false);
            if (deadOuter == NULL_ENTITY ||
                liveInner == NULL_ENTITY ||
                liveBaseTurret == NULL_ENTITY ||
                liveNexus == NULL_ENTITY)
            {
                return false;
            }

            if (bLowHp)
            {
                auto& health = safeWorld.GetComponent<HealthComponent>(bot);
                health.fCurrent = health.fMaximum * 0.05f;
            }

            const auto IsExpectedMove = [&](const std::vector<GameCommand>& commands,
                                             const Vec3& expected,
                                             eChampionAIState expectedState,
                                             const char* stage) -> bool_t
            {
                const auto& currentAI =
                    safeWorld.GetComponent<ChampionAIComponent>(bot);
                const bool_t bMatched =
                    commands.size() == 1u &&
                    commands[0].kind == eCommandKind::Move &&
                    WintersMath::DistanceSqXZ(commands[0].groundPos, expected) <= 0.0001f &&
                    WintersMath::DistanceSqXZ(currentAI.safeAnchor, expected) <= 0.0001f &&
                    WintersMath::DistanceSqXZ(currentAI.retreatGoal, expected) <= 0.0001f &&
                    currentAI.state == expectedState;
                if (!bMatched)
                {
                    std::printf(
                        "[SimLab][ChampionAI][SafeAnchor] FAIL: lane=%u stage=%s commands=%zu state=%u safe=(%.2f,%.2f,%.2f)\n",
                        static_cast<unsigned>(lane),
                        stage,
                        commands.size(),
                        static_cast<unsigned>(currentAI.state),
                        currentAI.safeAnchor.x,
                        currentAI.safeAnchor.y,
                        currentAI.safeAnchor.z);
                }
                return bMatched;
            };

            TickContext safeTick = MakeProbeTickContext(
                1ull, safeRng, safeEntityMap, safeWalkable);
            std::vector<GameCommand> safeCommands;
            CChampionAISystem::Execute(safeWorld, safeTick, safeCommands);
            const eChampionAIState expectedInitialState = bLowHp
                ? eChampionAIState::Retreat
                : eChampionAIState::MoveToOuterTurret;
            if (!IsExpectedMove(
                    safeCommands,
                    Vec3{ -23.f, 1.f, 0.f },
                    expectedInitialState,
                    "inner"))
            {
                return false;
            }

            if (!bLowHp)
                return true;

            auto MarkDestroyed = [&](EntityID entity)
            {
                auto& structure =
                    safeWorld.GetComponent<StructureComponent>(entity);
                structure.hp = 0.f;
                auto& health =
                    safeWorld.GetComponent<HealthComponent>(entity);
                health.fCurrent = 0.f;
                health.bIsDead = true;
            };

            MarkDestroyed(liveInner);
            safeTick = MakeProbeTickContext(
                2ull, safeRng, safeEntityMap, safeWalkable);
            safeCommands.clear();
            CChampionAISystem::Execute(safeWorld, safeTick, safeCommands);
            if (!IsExpectedMove(
                    safeCommands,
                    Vec3{ -35.f, 1.f, 0.f },
                    eChampionAIState::Retreat,
                    "base-turret"))
            {
                return false;
            }

            MarkDestroyed(liveBaseTurret);
            safeTick = MakeProbeTickContext(
                3ull, safeRng, safeEntityMap, safeWalkable);
            safeCommands.clear();
            CChampionAISystem::Execute(safeWorld, safeTick, safeCommands);
            return IsExpectedMove(
                safeCommands,
                Vec3{ -40.f, 1.f, 0.f },
                eChampionAIState::Retreat,
                "nexus");
        };

        if (!RunLiveSafeAnchorCase(
                Winters::Map::eLane::Top,
                false,
                20260725ull) ||
            !RunLiveSafeAnchorCase(
                Winters::Map::eLane::Mid,
                true,
                20260726ull))
        {
            return false;
        }

        std::printf(
            "[SimLab][ChampionAI][MidDefense] PASS: deterministic hash=%016llX home lane preserved, live safe-anchor lifecycle covered\n",
            static_cast<unsigned long long>(runA.hash));
```

이 회귀는 두 현상을 분리해 증명한다.

- 탑/정상 체력/애니/`MoveToOuterTurret`: dead Outer 좌표 `(-13,1,0)`를 버리고 live Inner 뒤 `(-23,1,0)`으로 Move한다.
- 미드/5% 체력/`Retreat`: live Inner → Inner 파괴 뒤 live Nexus-tier 포탑 → 그 포탑 파괴 뒤 Nexus로 목적지가 3틱 연속 전환된다. 모든 단계에서 `safeAnchor == retreatGoal == Move.groundPos`를 확인한다.

### 2.7 변경하지 않는 것

- `eChampionAIState::MoveToOuterTurret` enum/문자열은 이번에 이름을 바꾸지 않는다. 의미상 “현재 살아있는 safe turret로 이동”이 되지만 rename은 replay/debug/문서 표면을 넓히는 별도 정리다.
- 구조물 사망 시스템, `TargetableTag`, 포탑 AI, Snapshot/잔해 시각 상태는 수정하지 않는다.
- 적 감지, 퇴각 중 교전, recall 채널 시간, 체력 임계값은 변경하지 않는다.
- `ChampionAIComponent` 필드·크기·keyframe schema를 늘리지 않는다.

## 3. 검증 — 예측이 먼저다

```text
예측:
- 정적 배선: `RefreshChampionAIGoals()` 호출 수는 기존 spawn 2곳 그대로이고, 런타임 `BuildChampionAIContext()`가 `ResolveSafeAnchor()`를 호출한다.
- Shared 경계: Shared/GameSim은 Engine/Client/Server 헤더를 새로 include하지 않는다. Server만 Shared resolver를 소비한다.
- 탑 애니 fixture: 첫 명령은 Move, state는 MoveToOuterTurret, groundPos/safeAnchor/retreatGoal은 live Inner 뒤 (-23,1,0)다.
- 미드 저체력 fixture: 1/2/3 tick의 Move target은 각각 (-23,1,0), (-35,1,0), (-40,1,0)이며 state는 Retreat다.
- 파괴 잔해는 world에 남아도 Health dead라 resolver 후보에서 제외된다. 살아있는 turret가 하나라도 있으면 Nexus fallback보다 우선한다.
- 기존 MidDefense same-seed hash A==B, 전체 SimLab same-seed runA==runB, keyframe restore 결정성이 유지된다. 고정 golden hash 변경은 예상하지 않지만 전체 매치 hash 숫자 자체는 AI 경로 수정으로 변할 수 있다.
- 데이터/schema/생성물 변경이 없으므로 DataContract hash와 ChampionGameData hash는 불변이어야 한다.
- Bot AI는 GameCommand 생산자이며 구조물 사망 truth나 이동을 직접 적용하지 않는다. CommandExecutor/MoveSystem 권위 경계는 불변이다.

검증 명령:
1. git diff --check
2. rg -n "ResolveSafeAnchor|RefreshChampionAIGoals" Shared/GameSim Server Tools/SimLab
3. powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
4. powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
   - 이 harness가 unavailable이면 다음을 개별 실행:
     & 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
     & 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
     & 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe' Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
5. Tools/Bin/Debug/SimLab.exe 600 1234
   - 필수 로그: `[SimLab][ChampionAI][MidDefense] PASS: ... live safe-anchor lifecycle covered`
   - 필수 종료: exit 0, same-seed replay OK
6. powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 3 -Configuration Debug
   - 필수: 3회 RESULT status=PASS, 파괴 뒤 dead turret 좌표로 향하는 신규 Move 없음

미검증:
- 실제 맵에서 Red 탑/미드 포탑 파괴 순서별 navmesh 경로 품질과 포탑 뒤 3 m 위치의 체감 안전성.
- 적 챔피언이 살아있는 Inner 근처까지 밀고 들어온 경우에도 recall을 계속 시도할지에 대한 전술 품질. 이번 수정은 anchor 생존성만 보장한다.
- `O(bot × structure)` live scan의 실제 서버 프레임 비용. 현재 구조물 수에서는 작을 것으로 예측하지만 계측값은 없다.

확인 필요:
- 인게임 F10/AI trace에서 탑 애니의 `move-to-outer-turret`, 미드 저체력의 `lane-retreat` 명령 목적지가 파괴 직후 한 틱 내 다음 살아있는 포탑 뒤로 바뀌는지 수동 캡처한다.
- 모든 라인 포탑과 Nexus 쌍둥이 포탑까지 파괴된 비정상/게임종료 직전 장면에서 Nexus 좌표 fallback이 recall UX상 허용되는지 확인한다. 더 안전한 fountain anchor가 필요하면 별도 정책으로 승격한다.

롤백 범위:
- 위 4개 파일의 resolver 선언/구현, context 갱신, Server 위임, SimLab assert만 revert한다. 데이터·schema·생성물·구조물 잔해 구현은 건드리지 않는다.

인계 완료 조건:
- 계획대로 반영 후 Debug GameSim/Server/SimLab 빌드 PASS, Shared boundary PASS, SimLab 600/1234 exit 0, bot soak 3회 PASS를 기록한 `2026-07-18_CHAMPION_AI_LIVE_SAFE_TURRET_RECALL_ANCHOR_RESULT.md`를 생성한다.
```
