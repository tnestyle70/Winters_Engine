Session - 포탑 파괴 정지, 전투 후 미니언 후진, 정글 오브젝트 애니메이션/행동, UI 튜너 저장과 이렐리아 Q 벽 횡단 회귀를 권위 경계별로 분리해 복구한다.

좌표: 신규 좌표 후보 · 축: C2 이동·경로 계산, C7 권위·도구 정합성
관련: Server/GameRoom nav·lane AI, Shared/GameSim Jungle·Irelia, Client jungle/minimap, Engine UI Manager

## 0. 세션 계약과 기존 계획 관계

- 이 문서는 아래 기존 계획/결과에서 실제 F5 검증 뒤 발견된 회귀를 한 번에 추적하는 후속 복구 계획이다.
  - `2026-07-19_TURRET_MINION_NAV_REFRESH_KALISTA_STRUCTURE_CHASE_PLAN.md` / `RESULT.md`
  - `2026-07-19_DRAGON_ANIMATION_TURRET_RANGE_AGGRO_FIX_PLAN.md` / `RESULT.md`
  - `2026-07-19_CHAMPION_WORLD_LEVEL_FX_MINIMAP_UI_TUNER_PLAN.md` / `RESULT.md`
  - `2026-07-19_JUNGLE_OBJECTIVE_ENDGAME_BUFFS_BALANCE_LAB_PLAN.md` / `RESULT.md`
- 이 세션은 위 결과를 폐기하지 않는다. AttackChase raw/resolved 보정, 월드 체력바 레벨 전달, 정글 보상 권위 계약은 유지하고 회귀 부분만 수정한다.
- authoritative flow는 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`을 유지한다.
- 구현 전 독립 sub-agent read-only critique를 받고 P0/P1 disposition을 이 문서에 기록한다.
- 구현 시작 후 같은 이름의 `*_RESULT.md`를 작성하고 자동 검증, 빌드, 수동 F5 잔여 항목을 분리한다.
- 70/30 예산: 아래 명시된 복구와 자동 검증을 70% 범위로 닫고, 30% ceiling은 normal F5 5v5 시각 캡처와 profiler 실측에 남긴다. 추가 튜너/범용 에디터 확장은 금지한다.

## 1. 문제 정의와 확정 원인

### 1-1. 포탑 파괴 후 약 1초 정지

포탑 하나의 blocking state가 바뀔 때 다음 작업이 `CGameRoom::Tick`의 `m_stateMutex` 안에서 동기로 이어진다.

```text
Phase_ServerMinionWave
 -> RefreshServerStructureNavigationIfNeeded
 -> terrain base 복제 + 생존 구조물 carve
 -> BuildInflated(512x512) x 2
 -> PrewarmReachabilityCache(512x512) x 2
 -> mover/patrol/minion waypoint sanitize
 -> six-lane resolved path + flow field 512x512 x 6
 -> champion AI goal refresh
```

- 원인은 structure hash scan이 아니라 hash 변화 뒤의 full-grid cascade다.
- `CServerMinionFlowField::Build`는 6개 field에서 모든 walkable cell과 모든 lane segment 거리를 다시 계산한다.
- 새 derived grid의 cache id마다 reachability cache가 새로 만들어져 파괴 반복 시 메모리 누적 위험도 있다.
- P0 결론: 런타임 structure death에서는 base nav만 복원/re-carve하고 derived grid, cache, flow, authored waypoint sanitize는 건드리지 않는다.
- 보수적 결과: 이미 만들어진 path/flow는 죽은 포탑 영역을 잠시 우회할 수 있지만 base movement clamp는 다음 tick부터 열린다. 전역 rebuild 제거가 우선이다.
- durable 후속은 `terrain nav/flow immutable + health-aware dynamic structure blocker`이며 이번 P0에 섞지 않는다.

### 1-2. 칼리스타/이렐리아 포탑 공격 제자리걸음

이 문제는 nav rebuild와 별개다.

- 공격 사거리 판정은 raw turret center 기준이다.
- path 목표는 carve 때문에 raw center에서 바깥 walkable endpoint로 보정된다.
- 과거 이동 종료 반경은 resolved endpoint 주위에 `effectiveRange - 0.05`를 그대로 적용했다.
- 따라서 resolved endpoint에는 도착했지만 raw center에는 여전히 공격 사거리 밖인 점에서 Move 완료와 AttackChase repath가 반복됐다.
- 현재 반영된 `effectiveRange - distance(raw,resolved) - 0.05` 보정은 유지한다.
- nav rebuild는 죽은 구조물이 영구 obstacle로 남는 별도 미니언 문제를 고치려고 추가됐고, 제자리걸음 해결책은 아니었다.

### 1-3. 전투 후 미니언 후진

- `currentWaypoint`는 0.8m 도착 원 안에 들어왔을 때만 증가한다.
- Chase/Attack 동안 waypoint 진행도는 갱신되지 않는다.
- target 소멸 시 `attackTargetId`만 지우고 기존 `PathWaypoints/PathTarget/PathRebuildCooldown`을 남긴다.
- 전투 중 과거 waypoint를 지나 앞으로 이동한 미니언이 전투 후 stale index/path로 되돌아간다.
- 수정 계약은 `combat -> lane` 전환 시 전투 path 즉시 폐기 + waypoint index 단조 증가다.

### 1-4. Dragon/Baron/Level FX

- Dragon은 이미 매 프레임 animator update 대상이다. 깨진 포즈의 직접 원인은 BasicAttack 때 `sru_dragon_flying_attack1`을 non-loop로 재생하는 clip 교체다.
- Dragon은 idle/move/attack 모두 `sru_dragon_flying_run` loop를 유지하고, 서버 BA EffectTrigger마다 타격 FX만 재생한다.
- Baron은 실제 피해 후 `DamageQueueSystem`이 이미 `bAggro=true`와 피해 source target을 설정한다. 수정하지 않는다.
- Baron 이동 원인은 proximity auto-aggro와 사거리 밖 BA가 공통 AttackChase로 변환되는 것이다.
- Baron은 spawn anchor 고정, 피격 전 선공 금지, 피격 후 effective BA range 안 챔피언만 공격한다.
- Baron attack clip은 유지하고 animator update만 Dragon과 함께 full-rate로 올린다.
- Level-up FX 고정 원인은 `cue.attachTo = NULL_ENTITY`; `entity` attachment로 바꾸고 1초/가시 아군 필터는 유지한다.

### 1-5. UI와 이렐리아 Q

- Balance/UI Manager/Minimap은 `SetNextItemWidth(-1)` 뒤 visible label을 control 오른쪽에 그려 라벨 폭이 사라진다.
- 모든 단일 scalar row는 `왼쪽 라벨 | 오른쪽 ##hidden_id control` 2열 계약으로 통일한다.
- Health Bar와 Minimap 수치는 메모리 전용이다. developer-local versioned INI와 temp+atomic replace로 저장한다.
- 미니맵 챔피언 border는 고정 검정 1px이다. `eTeamId`로 Blue/Red 색을 고르고 0.5~8px 두께를 저장한다.
- 이렐리아 Q는 cast 이후 매 tick `TryClampMoveSegmentXZ(..., 0.5)`를 호출해 벽 첫 cell에서 `bBlocked`로 종료한다.
- Q transit은 벽을 검사하지 않고 보간하며, 도착점만 target-side walkable로 resolve한다. 취소 중 벽 내부면 시작점 쪽 walkable로 복귀한다.

## 2. 소유권 경계

| 동작 | 소유자 | 금지 |
|---|---|---|
| structure base nav 갱신, minion lane progress | Server GameRoom | Client 추측, full derived rebuild |
| Baron stationary/retaliation, Irelia Q transit/impact | Shared GameSim | Client 위치 결정 |
| Dragon/Baron clip playback, level FX, minimap | Client presentation | 서버 결과 재계산 |
| Health bar renderer/tuner | Engine generic UI | Client/LoL include 추가 |
| Minimap tuner | Client UI | Engine이 LoL Minimap 타입 include |
| Dragon/Red FX 형상 | `Data/LoL/FX/Object/Jungle` | 코드 하드코딩 emitter |

## 3. 구현 단계와 정확한 변경 계약

### 3-1. 런타임 structure nav P0

#### `Server/Public/Game/GameRoom.h`

기존 선언:

```cpp
    bool_t CarveServerStructuresOnNavGrid();
```

아래로 교체:

```cpp
    bool_t CarveServerStructuresOnNavGrid(bool_t bRebuildDerived = true);
```

nav grid state 필드 옆에 harness가 실제 call delta를 읽는 monotonic counters를 추가한다.

```cpp
u64_t m_serverPathNavGridBuildCount = 0ull;
u64_t m_serverStructureNavigationRefreshCount = 0ull;
```

#### `Server/Private/Game/GameRoomNav.cpp`

`RefreshServerStructureNavigationIfNeeded` 전체를 아래로 교체한다.

```cpp
void CGameRoom::RefreshServerStructureNavigationIfNeeded()
{
    const u64_t currentHash = ComputeServerStructureNavigationStateHash();
    if (currentHash == m_serverStructureNavigationStateHash)
        return;

    // Runtime structure death opens only the authoritative base walkability.
    // Rebuilding inflated grids, reachability caches and six flow fields here
    // stalls the room tick and is intentionally forbidden.
    if (CarveServerStructuresOnNavGrid(false))
        ++m_serverStructureNavigationRefreshCount;
}
```

`CarveServerStructuresOnNavGrid` signature와 base grid 생성 부분을 아래로 교체한다.

```cpp
bool_t CGameRoom::CarveServerStructuresOnNavGrid(bool_t bRebuildDerived)
{
    if (!m_pTerrainNavGrid)
    {
        OutputServerAITrace(
            "[ServerNav] structure carve aborted: terrain clone is missing\n");
        return false;
    }

    if (!m_pNavGrid ||
        m_pNavGrid->Get_OriginX() != m_pTerrainNavGrid->Get_OriginX() ||
        m_pNavGrid->Get_OriginZ() != m_pTerrainNavGrid->Get_OriginZ())
    {
        m_pNavGrid = Engine::CNavGrid::Create(
            m_pTerrainNavGrid->Get_OriginX(),
            m_pTerrainNavGrid->Get_OriginZ());
        if (!m_pNavGrid)
            return false;
    }
    m_pNavGrid->Load_Bits(
        m_pTerrainNavGrid->Get_Bits(),
        Engine::CNavGrid::kByteSize);

    u32_t carvedStructures = 0u;
    m_world.ForEach<StructureComponent, TransformComponent, HealthComponent>(
        std::function<void(
            EntityID,
            StructureComponent&,
            TransformComponent&,
            HealthComponent&)>(
            [&](EntityID,
                StructureComponent& structure,
                TransformComponent& transform,
                HealthComponent& health)
            {
                if (health.bIsDead || health.fCurrent <= 0.f)
                    return;
                const f32_t radius = ResolveStageStructureRadius(
                    structure.kind, structure.tier);
                const Engine::CNavGrid::Cell center =
                    m_pNavGrid->WorldToCell(transform.GetPosition());
                const int32_t radiusCells = static_cast<int32_t>(
                    std::ceil(radius / Engine::CNavGrid::kCellSize));
                for (int32_t y = -radiusCells; y <= radiusCells; ++y)
                {
                    for (int32_t x = -radiusCells; x <= radiusCells; ++x)
                    {
                        if (x * x + y * y <= radiusCells * radiusCells)
                        {
                            m_pNavGrid->SetWalkable(
                                center.x + x, center.y + y, false);
                        }
                    }
                }
                ++carvedStructures;
            }));

    // 성공한 base 복원/carve 뒤에는 runtime/startup 모두 hash를 갱신한다.
    // 그렇지 않으면 다음 tick에도 같은 carve가 반복된다.
    m_serverStructureNavigationStateHash =
        ComputeServerStructureNavigationStateHash();

    char message[224]{};
    sprintf_s(message,
        "[ServerNav] structures carved=%u derived=%u hash=%08X\n",
        carvedStructures,
        bRebuildDerived ? 1u : 0u,
        m_pNavGrid->ComputeContentHash());
    OutputServerAITrace(message);

    // derived rebuild만 startup 호출에서 허용한다.
    if (bRebuildDerived)
        BuildServerPathNavGrid();
    return true;
}
```

`BuildServerPathNavGrid()` 함수 진입점에는 `++m_serverPathNavGridBuildCount;`를 추가한다. startup 초기 build도 monotonic counter에 포함하고, 회귀는 포탑 사망 직전/직후 delta로 판정한다.

trace에는 `derived=%u`를 추가하여 runtime refresh가 0임을 확인한다. `Phase_ServerMinionWave`의 hash check 호출은 유지한다.

#### `Tools/Harness/GameRoomBotMatchSoak.cpp`

include에 실제 피해 큐용 header를 추가한다.

```cpp
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

anonymous namespace에 evidence 전체 선언을 추가한다. 필드들은 monotonic room counter의 delta만 보관하며 JSON 상수로 대체하지 않는다.

```cpp
struct StructureNavigationProbeEvidence
{
    bool_t bDeadReleased = false;
    bool_t bDerivedStable = false;
    bool_t bSecondRefreshNoop = false;
    u64_t refreshTickUs = 0ull;
    u64_t noopTickUs = 0ull;
    u64_t derivedRebuildCalls = 0ull;
    u64_t refreshWorkCalls = 0ull;
    u64_t secondRefreshWorkCalls = 0ull;
};
```

기존 `RunStructureNavigationRefreshProbe`는 `StructureNavigationProbeEvidence& outEvidence`를 받고, 기존 probe cell 탐색 뒤 death/refresh 부분을 아래로 교체한다. 이 경로는 direct health mutation이 아니라 `EnqueueDamageRequest -> room.Tick() damage -> next room.Tick() nav refresh`를 실행하며 Release harness에서도 동작한다.

```cpp
const auto* pPathGridBefore = room.m_pPathNavGrid.get();
const auto* pLaneGridBefore = room.m_pMinionLaneNavGrid.get();
const u32_t pathRevisionBefore = pPathGridBefore ? pPathGridBefore->GetRevision() : 0u;
const u32_t laneRevisionBefore = pLaneGridBefore ? pLaneGridBefore->GetRevision() : 0u;
const u64_t buildCountBefore = room.m_serverPathNavGridBuildCount;
const u64_t refreshCountBefore = room.m_serverStructureNavigationRefreshCount;

EntityID sourceChampion = NULL_ENTITY;
const eTeam structureTeam = room.m_world.GetComponent<StructureComponent>(
    structureEntity).team;
room.m_world.ForEach<ChampionComponent>(
    [&](EntityID entity, ChampionComponent& champion)
    {
        if (sourceChampion == NULL_ENTITY && champion.team != structureTeam)
            sourceChampion = entity;
    });
if (sourceChampion == NULL_ENTITY)
{
    outError = "no opposing champion for structure damage probe";
    return false;
}

HealthComponent& health =
    room.m_world.GetComponent<HealthComponent>(structureEntity);
DamageRequest lethal{};
lethal.source = sourceChampion;
lethal.target = structureEntity;
lethal.sourceTeam = room.m_world.GetComponent<ChampionComponent>(
    sourceChampion).team;
lethal.type = eDamageType::True;
lethal.flatAmount = health.fMaximum + 1000.f;
lethal.eSourceKind = eDamageSourceKind::BasicAttack;
EnqueueDamageRequest(room.m_world, lethal);

room.Tick(); // damage queue kills the structure after this tick's nav phase.
if (!health.bIsDead || health.fCurrent > 0.f)
{
    outError = "queued lethal structure damage was not applied";
    return false;
}

const auto refreshStart = std::chrono::steady_clock::now();
room.Tick(); // next authoritative room tick observes the structure hash change.
const auto refreshEnd = std::chrono::steady_clock::now();
outEvidence.refreshTickUs = static_cast<u64_t>(
    std::chrono::duration_cast<std::chrono::microseconds>(
        refreshEnd - refreshStart).count());

outEvidence.bDeadReleased =
    room.m_pNavGrid->IsWalkable(probeCell.x, probeCell.y);
outEvidence.bDerivedStable =
    room.m_pPathNavGrid.get() == pPathGridBefore &&
    room.m_pMinionLaneNavGrid.get() == pLaneGridBefore &&
    (!pPathGridBefore || pPathGridBefore->GetRevision() == pathRevisionBefore) &&
    (!pLaneGridBefore || pLaneGridBefore->GetRevision() == laneRevisionBefore);
outEvidence.derivedRebuildCalls =
    room.m_serverPathNavGridBuildCount - buildCountBefore;
outEvidence.refreshWorkCalls =
    room.m_serverStructureNavigationRefreshCount - refreshCountBefore;

const u64_t settledHash = room.m_serverStructureNavigationStateHash;
const u64_t refreshCountAfter = room.m_serverStructureNavigationRefreshCount;
const auto noopStart = std::chrono::steady_clock::now();
room.Tick();
const auto noopEnd = std::chrono::steady_clock::now();
outEvidence.noopTickUs = static_cast<u64_t>(
    std::chrono::duration_cast<std::chrono::microseconds>(
        noopEnd - noopStart).count());
outEvidence.bSecondRefreshNoop =
    room.m_serverStructureNavigationStateHash == settledHash &&
    room.m_pPathNavGrid.get() == pPathGridBefore &&
    room.m_pMinionLaneNavGrid.get() == pLaneGridBefore;
outEvidence.secondRefreshWorkCalls =
    room.m_serverStructureNavigationRefreshCount - refreshCountAfter;

if (!outEvidence.bDeadReleased ||
    !outEvidence.bDerivedStable ||
    !outEvidence.bSecondRefreshNoop ||
    outEvidence.derivedRebuildCalls != 0ull ||
    outEvidence.refreshWorkCalls != 1ull ||
    outEvidence.secondRefreshWorkCalls != 0ull ||
    outEvidence.refreshTickUs > 33333u)
{
    outError = "structure death nav refresh exceeded its runtime contract";
    return false;
}
return true;
```

main wiring은 Release/Debug 공통으로 evidence를 전달하고 실측값을 출력한다. 기존 `structureNavProbeRoom` block을 아래로 교체한다.

```cpp
StructureNavigationProbeEvidence structureNavEvidence{};
auto structureNavProbeRoom = CGameRoom::Create(
    options.roomId ^ 0x20000000u);
if (!structureNavProbeRoom ||
    !CGameRoomIntegrationProbeAccess::PrepareBotMatch(
        *structureNavProbeRoom, options.seed, error) ||
    !CGameRoomIntegrationProbeAccess::RunStructureNavigationRefreshProbe(
        *structureNavProbeRoom, structureNavEvidence, error))
{
    if (structureNavProbeRoom)
        structureNavProbeRoom->Stop();
    std::cerr
        << "RESULT status=FAIL reason=structure_nav_probe_failed detail=\""
        << error << "\"\n";
    return 1;
}
std::cout << "STRUCTURE_NAV_PROBE status=PASS"
    << " live_blocked=1 dead_released=1"
    << " refresh_tick_us=" << structureNavEvidence.refreshTickUs
    << " noop_tick_us=" << structureNavEvidence.noopTickUs
    << " derived_rebuild_calls=" << structureNavEvidence.derivedRebuildCalls
    << " refresh_work_calls=" << structureNavEvidence.refreshWorkCalls
    << " second_refresh_work_calls="
    << structureNavEvidence.secondRefreshWorkCalls << "\n";
structureNavProbeRoom->Stop();
structureNavProbeRoom.reset();
```

같은 run directory에 아래 완전 JSON을 `structure_nav_probe.json`으로 쓴다.

```cpp
std::ofstream navJson("structure_nav_probe.json", std::ios::trunc);
navJson << "{\n"
    << "  \"scope\": \"ServerStructureNav::AuthoritativeDeathRefreshTick\",\n"
    << "  \"refreshTickUs\": " << structureNavEvidence.refreshTickUs << ",\n"
    << "  \"noopTickUs\": " << structureNavEvidence.noopTickUs << ",\n"
    << "  \"derivedRebuildCalls\": "
    << structureNavEvidence.derivedRebuildCalls << ",\n"
    << "  \"refreshWorkCalls\": "
    << structureNavEvidence.refreshWorkCalls << ",\n"
    << "  \"secondRefreshWorkCalls\": "
    << structureNavEvidence.secondRefreshWorkCalls << "\n"
    << "}\n";
if (!navJson.good())
{
    std::cerr << "RESULT status=FAIL reason=structure_nav_json_write_failed\n";
    return 1;
}
```

### 3-2. 미니언 combat-to-lane 단조 진행

#### `Server/Private/Game/GameRoomUnitAI.cpp`

anonymous namespace에는 path clear helper만 추가하고, private waypoint accessor를 쓰는 progress helper는 `CGameRoom` private member로 선언/정의한다.

```cpp
void ClearServerMinionPathRuntime(MinionStateComponent& state)
{
    state.PathTarget = {};
    state.PathResolvedTarget = {};
    state.PathCount = 0u;
    state.PathIndex = 0u;
    state.PathRebuildCooldown = 0.f;
    state.BlockedMoveFrames = 0u;
}

```

`Server/Public/Game/GameRoom.h`의 minion private helper 선언부에 아래를 추가한다.

```cpp
void AdvanceServerMinionWaypointPastPosition(
    MinionStateComponent& state,
    const Vec3& position,
    u8_t waypointLane,
    u32_t waypointCount) const;
```

`GameRoomUnitAI.cpp`에서 `ResolveServerMinionStartWaypoint` 바로 위에 전체 정의를 추가한다. lateral corridor 밖의 half-plane은 통과로 인정하지 않고, combat 복귀 한 tick에는 최대 한 waypoint만 증가시킨다.

```cpp
void CGameRoom::AdvanceServerMinionWaypointPastPosition(
    MinionStateComponent& state,
    const Vec3& position,
    u8_t waypointLane,
    u32_t waypointCount) const
{
    if (state.currentWaypoint >= waypointCount)
        return;

    constexpr f32_t kArriveRadius = 0.8f;
    constexpr f32_t kLaneRebaseCorridor = 5.f;
    const u32_t index = state.currentWaypoint;
    const Vec3 waypoint = GetServerMinionWaypoint(state.team, waypointLane, index);
    if (WintersMath::DistanceSqXZ(position, waypoint) <=
        kArriveRadius * kArriveRadius)
    {
        ++state.currentWaypoint;
        return;
    }

    if (index == 0u || waypointCount < 2u)
        return;
    const Vec3 previous = GetServerMinionWaypoint(
        state.team, waypointLane, index - 1u);
    const Vec3 segment{ waypoint.x - previous.x, 0.f, waypoint.z - previous.z };
    const f32_t segmentLengthSq = segment.x * segment.x + segment.z * segment.z;
    if (segmentLengthSq <= 0.0001f)
        return;
    const Vec3 fromPrevious{
        position.x - previous.x, 0.f, position.z - previous.z };
    const f32_t projection =
        (fromPrevious.x * segment.x + fromPrevious.z * segment.z) /
        segmentLengthSq;
    const Vec3 closest{
        previous.x + segment.x * std::clamp(projection, 0.f, 1.f),
        position.y,
        previous.z + segment.z * std::clamp(projection, 0.f, 1.f) };
    const bool_t bInsideLaneCorridor =
        WintersMath::DistanceSqXZ(position, closest) <=
        kLaneRebaseCorridor * kLaneRebaseCorridor;
    if (bInsideLaneCorridor && projection >= 1.f)
        ++state.currentWaypoint;
}
```

no-target lane 분기의 기존 `laneTarget` 계산 전에 아래 상태 전이를 추가한다.

```cpp
const bool_t bResumeLaneFromCombat =
    state.current == MinionStateComponent::Chase ||
    state.current == MinionStateComponent::Attack ||
    state.attackTargetId != NULL_ENTITY;
state.attackTargetId = NULL_ENTITY;
if (bResumeLaneFromCombat)
    ClearServerMinionPathRuntime(state);

const u8_t waypointLane = ResolveServerWaypointLane(minion.team, state.lane);
const u32_t waypointCount =
    GetServerMinionWaypointCount(minion.team, waypointLane);
AdvanceServerMinionWaypointPastPosition(
    state, pos, waypointLane, waypointCount);
```

기존 0.8m 도착 증가는 helper가 담당하도록 중복을 제거한다. index는 어떤 경로에서도 감소시키지 않는다.

#### `Tools/Harness/GameRoomBotMatchSoak.cpp`

기존 soak tracker와 별도로 deterministic combat-exit fixture를 추가한다. `currentWaypoint=k`, 위치는 `W[k]`를 지난 lane corridor 안, old `PathTarget/PathWaypoints`는 적 위치, `PathRebuildCooldown=0.2`, target은 해당 tick dead 상태로 직접 구성한다. 첫 lane-resume tick에 다음을 실패시킨다.

```text
- combat exit 다음 tick PathCount != 0 while old PathTarget remains
- currentWaypoint decrease
- lane resume first displacement dot lane-forward < -0.05
- hairpin/평행 segment에서 corridor 밖 위치가 currentWaypoint를 건너뜀
```

fixture의 전체 core는 아래다. `PrepareBotMatch` 뒤 별도 probe room에서 실행하고 main은 실패 시 `minion_combat_exit_probe_failed`, 성공 시 `MINION_COMBAT_EXIT_PROBE status=PASS stale_path_cleared=1 monotonic=1 backward=0 off_corridor_skip=0`을 출력한다.

```cpp
static bool_t RunMinionCombatExitWaypointProbe(
    CGameRoom& room,
    std::string& outError)
{
    constexpr eTeam team = eTeam::Blue;
    constexpr u8_t lane = static_cast<u8_t>(Winters::Map::eLane::Mid);
    const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
    const u32_t waypointCount = room.GetServerMinionWaypointCount(
        team, waypointLane);
    if (waypointCount < 3u)
    {
        outError = "minion combat-exit probe needs three waypoints";
        return false;
    }

    constexpr u32_t currentIndex = 1u;
    const Vec3 previous = room.GetServerMinionWaypoint(
        team, waypointLane, currentIndex - 1u);
    const Vec3 current = room.GetServerMinionWaypoint(
        team, waypointLane, currentIndex);
    const Vec3 next = room.GetServerMinionWaypoint(
        team, waypointLane, currentIndex + 1u);
    const Vec3 forward = WintersMath::DirectionXZ(previous, current, Vec3{});
    const Vec3 spawnPos{
        current.x + forward.x * 1.1f,
        current.y,
        current.z + forward.z * 1.1f };
    const EntityID minion = room.SpawnServerMinion(team, 0u, lane, spawnPos);
    if (minion == NULL_ENTITY)
    {
        outError = "minion combat-exit probe spawn failed";
        return false;
    }

    const EntityID deadTarget = room.m_world.CreateEntity();
    TransformComponent deadTransform{};
    const Vec3 staleTargetPos{
        spawnPos.x - forward.x * 8.f,
        spawnPos.y,
        spawnPos.z - forward.z * 8.f };
    deadTransform.SetPosition(staleTargetPos);
    room.m_world.AddComponent<TransformComponent>(deadTarget, deadTransform);
    HealthComponent deadHealth{};
    deadHealth.fCurrent = 0.f;
    deadHealth.fMaximum = 100.f;
    deadHealth.bIsDead = true;
    room.m_world.AddComponent<HealthComponent>(deadTarget, deadHealth);

    MinionStateComponent& state =
        room.m_world.GetComponent<MinionStateComponent>(minion);
    state.current = MinionStateComponent::Chase;
    state.currentWaypoint = currentIndex;
    state.attackTargetId = deadTarget;
    state.targetScanCooldown = 1.f;
    state.PathTarget = staleTargetPos;
    state.PathResolvedTarget = staleTargetPos;
    state.PathWaypoints[0] = staleTargetPos;
    state.PathCount = 1u;
    state.PathIndex = 0u;
    state.PathRebuildCooldown = 0.2f;

    const Vec3 before = room.m_world.GetComponent<TransformComponent>(minion)
        .GetPosition();
    room.Tick();
    const Vec3 after = room.m_world.GetComponent<TransformComponent>(minion)
        .GetPosition();
    const MinionStateComponent& afterState =
        room.m_world.GetComponent<MinionStateComponent>(minion);
    const Vec3 displacement{ after.x - before.x, 0.f, after.z - before.z };
    const Vec3 laneForward = WintersMath::DirectionXZ(current, next, Vec3{});
    const bool_t stalePathCleared =
        afterState.PathCount == 0u ||
        WintersMath::DistanceSqXZ(afterState.PathTarget, staleTargetPos) > 0.01f;
    const bool_t monotonic =
        afterState.currentWaypoint == currentIndex + 1u;
    const bool_t backward =
        displacement.x * laneForward.x + displacement.z * laneForward.z < -0.05f;

    MinionStateComponent offCorridor = state;
    offCorridor.currentWaypoint = currentIndex;
    const Vec3 farSide{
        current.x - forward.z * 10.f,
        current.y,
        current.z + forward.x * 10.f };
    room.AdvanceServerMinionWaypointPastPosition(
        offCorridor, farSide, waypointLane, waypointCount);
    const bool_t offCorridorSkipped =
        offCorridor.currentWaypoint != currentIndex;
    if (!stalePathCleared || !monotonic || backward || offCorridorSkipped)
    {
        outError = "minion combat-exit lane resume contract failed";
        return false;
    }
    return true;
}
```

### 3-3. Dragon/Baron/Level FX

#### `Client/Private/Manager/Jungle_Manager.cpp`

Dragon animation set은 wing loop만 가리킨다.

```cpp
case CJungle_Manager::eJungleSub::Dragon:
    return { "sru_dragon_flying_run", "sru_dragon_flying_run", "", "" };
```

full-rate 판정은 아래로 교체한다.

```cpp
const bool_t bFrameRateAnim =
    m_pWorld->HasComponent<JungleComponent>(it.first) &&
    (static_cast<eJungleSub>(
        m_pWorld->GetComponent<JungleComponent>(it.first).subKind) ==
        eJungleSub::Dragon ||
     static_cast<eJungleSub>(
        m_pWorld->GetComponent<JungleComponent>(it.first).subKind) ==
        eJungleSub::Baron);
```

`Apply_NetworkAnimation`에서 dead 처리 뒤, generic BasicAttack 분기 전에 아래를 추가한다. Dragon BasicAttack sequence는 소비하되 attack clip/bAction을 시작하지 않고, moving flag와 무관한 단일 base id로 wing loop를 유지한다. Baron은 이 분기에 들어오지 않으므로 기존 attack clip 분기를 보존한다.

```cpp
if (sub == eJungleSub::Dragon)
{
    if (pAction &&
        static_cast<eActionStateId>(pAction->actionId) == eActionStateId::BasicAttack &&
        pAction->sequence != 0u &&
        (visual.lastActionSeq != pAction->sequence ||
            visual.lastActionAnimId != pAction->actionId))
    {
        visual.lastActionSeq = pAction->sequence;
        visual.lastActionAnimId = pAction->actionId;
    }

    visual.bAction = false;
    visual.actionTimer = 0.f;
    const u16_t wingLoopId = static_cast<u16_t>(ePoseStateId::Idle);
    if (visual.baseAnimId != wingLoopId)
    {
        if (const char* pWing = Resolve_PlayableAnimation(
                renderer, anims.idle, nullptr))
        {
            renderer.PlayAnimationByNameAdvanced(pWing, true, false, 1.f);
        }
        visual.baseAnimId = wingLoopId;
    }
    return;
}
```

#### `Shared/GameSim/Components/JungleAIComponent.h`

anchor 필드 앞에 아래 필드를 추가한다.

```cpp
bool_t bStationary = false;
```

명시적 padding이 필요하면 static layout을 확인해 `reserved`를 추가한다. trivially-copyable 계약은 유지한다.

#### `Server/Private/Game/GameRoomSpawn.cpp`

Jungle AI 생성 시 아래를 추가한다.

```cpp
jungleAI.bStationary = request.subKind == 0u; // Baron
```

실제 request field 이름은 현재 `JungleSpawnRequest` 정의에 맞춘다.

#### `Shared/GameSim/Systems/JungleAI/JungleAISystem.cpp`

Baron stationary branch는 generic proximity/leash/return보다 먼저 처리한다.

`std::numeric_limits`를 사용하는 helper를 위해 include 목록에 아래를 추가한다.

```cpp
#include <limits>
```

```cpp
if (ai.bStationary)
{
    auto& transform = world.GetComponent<TransformComponent>(entity);
    Vec3 anchored = transform.GetLocalPosition();
    anchored.x = ai.anchorX;
    anchored.z = ai.anchorZ;
    transform.SetPosition(anchored);
    ai.bReturning = false;

    if (world.HasComponent<AttackChaseComponent>(entity))
        world.RemoveComponent<AttackChaseComponent>(entity);
    if (world.HasComponent<MoveTargetComponent>(entity))
        world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};

    if (!ai.bAggro)
    {
        ai.target = NULL_ENTITY;
        continue;
    }

    ai.target = FindChampionInEffectiveAttackRange(
        world, entity, anchored, ai.target);
    if (ai.target == NULL_ENTITY ||
        HasActiveAttackWork(world, entity) ||
        !IsBasicAttackReady(world, entity))
    {
        continue;
    }

    const Vec3 targetPos = world.GetComponent<TransformComponent>(ai.target)
        .GetLocalPosition();
    ++ai.attackSequence;
    outCommands.push_back(MakeJungleBasicAttackCommand(
        tc, entity, ai.target, ai.attackSequence, anchored, targetPos));
    continue;
}
```

`FindChampionInEffectiveAttackRange`는 아래 전체 helper를 사용한다. `StatComponent.attackRange + source radius + target radius`를 사용하고, 기존 target이 유효하고 range 안이면 우선하며 아니면 sorted EntityID 순회에서 거리 최소를 선택한다. 피격 전 proximity acquire는 stationary branch에서 실행되지 않는다.

```cpp
EntityID FindChampionInEffectiveAttackRange(
    CWorld& world,
    EntityID jungle,
    const Vec3& selfPos,
    EntityID preferred)
{
    f32_t baseRange = 5.5f;
    if (world.HasComponent<StatComponent>(jungle))
    {
        const f32_t configured = world.GetComponent<StatComponent>(jungle).attackRange;
        if (configured > 0.f)
            baseRange = configured;
    }
    const f32_t sourceRadius =
        GameplayStateQuery::ResolveGameplayRadius(world, jungle);
    const auto isInRange = [&](EntityID candidate, f32_t& outDistanceSq)
    {
        if (!IsValidChampionTarget(world, jungle, candidate))
            return false;
        const Vec3 targetPos = world.GetComponent<TransformComponent>(candidate)
            .GetLocalPosition();
        outDistanceSq = DistanceSqXZ(targetPos, selfPos.x, selfPos.z);
        const f32_t effectiveRange = baseRange + sourceRadius +
            GameplayStateQuery::ResolveGameplayRadius(world, candidate);
        return outDistanceSq <= effectiveRange * effectiveRange;
    };

    f32_t preferredDistanceSq = 0.f;
    if (preferred != NULL_ENTITY && isInRange(preferred, preferredDistanceSq))
        return preferred;

    EntityID best = NULL_ENTITY;
    f32_t bestDistanceSq = (std::numeric_limits<f32_t>::max)();
    const auto champions =
        DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
    for (EntityID candidate : champions)
    {
        f32_t distanceSq = 0.f;
        if (isInRange(candidate, distanceSq) && distanceSq < bestDistanceSq)
        {
            best = candidate;
            bestDistanceSq = distanceSq;
        }
    }
    return best;
}
```

#### `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

include에 `JungleAIComponent.h`를 추가하고, effective range 계산 직후 아래 flag를 만든다.

```cpp
const bool_t bStationaryJungle =
    world.HasComponent<JungleAIComponent>(cmd.issuerEntity) &&
    world.GetComponent<JungleAIComponent>(cmd.issuerEntity).bStationary;
```

`HandleBasicAttack`의 out-of-range 분기를 아래로 교체한다.

```cpp
if (DistanceSqXZ(world, cmd.issuerEntity, cmd.targetEntity) > rangeSq)
{
    if (bStationaryJungle)
    {
        ClearAttackChase(world, cmd.issuerEntity);
        ClearMoveTarget(world, cmd.issuerEntity);
        LogBasicAttackReject("stationary-out-of-range", cmd);
        return CommandExecutionResult::Rejected(
            cmd.sequenceNum,
            eCommandExecutionReason::OutOfRange);
    }
    StartAttackChase(world, tc, cmd, effectiveRange);
    return CommandExecutionResult::Accepted(cmd.sequenceNum);
}
```

segment-blocked 내부의 `StartAttackChase` 분기도 아래로 교체한다.

```cpp
if (!tc.pWalkable->SegmentWalkableXZ(
        sourcePos,
        targetPos,
        GameplayStateQuery::ResolveGameplayRadius(world, cmd.issuerEntity)))
{
    if (bStationaryJungle)
    {
        ClearAttackChase(world, cmd.issuerEntity);
        ClearMoveTarget(world, cmd.issuerEntity);
        LogBasicAttackReject("stationary-segment-blocked", cmd);
        return CommandExecutionResult::Rejected(
            cmd.sequenceNum,
            eCommandExecutionReason::OutOfRange);
    }
    StartAttackChase(world, tc, cmd, effectiveRange);
    return CommandExecutionResult::Accepted(cmd.sequenceNum);
}
```

정상 in-range Baron BA는 기존 CombatAction/Event 경로를 그대로 탄다.

#### `Client/Private/Scene/Scene_InGameNetwork.cpp`

```cpp
cue.attachTo = NULL_ENTITY;
```

아래로 교체:

```cpp
cue.attachTo = entity;
```

trace의 `fixed=1`은 `follow=1`로 바꾼다.

#### `Client/Private/Network/Client/EventApplier.cpp`

dedupe 뒤 resolved `source`가 Dragon `JungleComponent{subKind=1}`이고 `eventSlot==BasicAttack`이면 아래 block으로 `Objective.Elder.BasicAttack` cue를 target에 붙여 1회 재생한다. spawn 결과를 timeline registry에 등록하여 replay rebase가 제거할 수 있게 한다. 기존 Kindred resolver 및 server EffectTrigger dedupe는 유지한다.

```cpp
if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
    source != NULL_ENTITY &&
    world.HasComponent<JungleComponent>(source) &&
    world.GetComponent<JungleComponent>(source).subKind == 1u)
{
    const EntityID target = ev->targetNet() != NULL_NET_ENTITY
        ? ResolveLiveEntity(world, entityMap, ev->targetNet())
        : NULL_ENTITY;
    FxCueContext fx{};
    fx.attachTo = target;
    fx.vWorldPos = pos;
    fx.vForward = WintersMath::Normalize3D(
        Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
    fx.pFxMeshRenderer = m_pFxMeshRenderer;
    std::vector<EntityID> spawned;
    CFxCuePlayer::PlayAll(
        world, "Objective.Elder.BasicAttack", fx, &spawned);
    for (EntityID spawnedEntity : spawned)
        m_timelineVisualEntities.push_back(world.GetEntityHandle(spawnedEntity));
}
```

#### 새 파일: `Data/LoL/FX/Object/Jungle/elder_basic_attack.wfx`

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Elder.BasicAttack",
  "emitters": [
    {
      "name": "elder_basic_attack_burst",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "OverlayNoDepth",
      "texture": "Client/Bin/Resource/Texture/Character/Annie/particles/annie_hiteffect.png",
      "lifetime": 0.45,
      "fade_in": 0.02,
      "fade_out": 0.24,
      "width": 2.4,
      "height": 2.4,
      "color": [2.4, 0.42, 0.06, 0.90],
      "attach_offset": [0.0, 1.0, 0.0],
      "billboard": true
    }
  ]
}
```

### 3-4. UI Manager label/persistence/minimap team outline

#### 사용자 작업 계약

```text
F8 UI Manager
  Health Bars
    [Show]
    Target                         [Champion v]
    Width                          [==== 104 px ====]
    Height                         [===== 20 px ====]
    Y Offset                       [==== 2.75 m ====]
    Champion Level
    Level X                        [==== -24 px ====]
    Level Y                        [===== +1 px ====]
    Level Font Scale               [===== 0.85 =====]
    [Reset Selected] [Reset All] [Save]
    Saved to .../world_health_bars.ini

  Minimap
    Viewport Height Ratio          [===== 0.39 =====]
    Right Padding                  [===== 12 px ====]
    Bottom Padding                 [===== 12 px ====]
    Icon Scale                     [===== 1.00 =====]
    Champion Scale                 [===== 2.00 =====]
    Champion Outline Thickness     [===== 2.0 px ===]
    World Extent                   [==== 94.385 ====]
    [Reset Projection] [Reset Layout] [Save]
```

#### 범위 표

| 값 | 기본 | 범위 | 저장 |
|---|---:|---:|---|
| Minimap viewport ratio | 0.39 | 0.18~0.55 | yes |
| Right/Bottom padding | 12 | 0~64 px | yes |
| Icon scale | 1.0 | 0.65~2.0 | yes |
| Champion scale | 2.0 | 0.75~3.0 | yes |
| Champion outline | 2.0 | 0.5~8.0 px | yes |
| Projection extent | 94.385 | 70~160 | yes |
| Champion level X/Y/font | -24/1/0.85 | current sliders | yes |
| Champion/Minion/Structure bars | current defaults | current sliders | yes |

#### action budget

- 주요 작업은 수치 조절과 Save 각 1 click이다.
- Reset은 메모리 값만 바꾸고 Save 전에는 재시작 후 유지되지 않는다.
- 잘못된/구버전 파일은 전체를 거부하고 compiled defaults를 유지한다.
- `Promote to shipping default`는 이번 범위에 없다.

#### `Engine/Private/Manager/UI/UI_Manager.cpp`

anonymous namespace에 `DrawLabeledSliderFloat`/`DrawLabeledCombo` 2열 helper를 추가한다. visible label은 첫 column에서 `TextUnformatted`, control은 둘째 column에서 `##stable_id`를 사용한다. Health Bars와 Champion Level의 모든 slider/combo를 이 helper로 교체한다.

`%LOCALAPPDATA%/Winters/Developer/world_health_bars.ini` version 1 load/save를 Status Panel과 같은 엄격 range/finite 검증 및 `.tmp -> MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`로 추가한다. 저장 필드:

```text
show
championWidth/championHeight/championYOffset
championLevelX/championLevelY/championLevelFontScale
minionWidth/minionHeight/minionYOffset
structureWidth/structureHeight/structureYOffset
structureScreenX/structureScreenY
```

`Initialize`에서 load하고 Health Bars tab에 `Reset All`, `Save`, 상태 문구를 추가한다.

Health Bar persistence의 정확한 member 구현은 기존 `ReadStatusPanelSettingValues`/`ReadStatusPanelU32`/`ReadStatusPanelBool`/`ReadStatusPanelFloat` helper를 재사용하여 아래로 고정한다.

```cpp
bool_t CUI_Manager::LoadWorldHealthBarLayoutSettings()
{
    std::filesystem::path path;
    if (!BuildWorldHealthBarLayoutSettingsPath(path))
        return false;
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError)
        return false;

    std::unordered_map<std::string, std::string> values;
    if (!ReadStatusPanelSettingValues(path, values))
        return false;

    u32_t version = 0u;
    bool_t show = true;
    f32_t championWidth = 0.f;
    f32_t championHeight = 0.f;
    f32_t championYOffset = 0.f;
    f32_t championLevelX = 0.f;
    f32_t championLevelY = 0.f;
    f32_t championLevelFontScale = 0.f;
    f32_t minionWidth = 0.f;
    f32_t minionHeight = 0.f;
    f32_t minionYOffset = 0.f;
    f32_t structureWidth = 0.f;
    f32_t structureHeight = 0.f;
    f32_t structureYOffset = 0.f;
    f32_t structureScreenX = 0.f;
    f32_t structureScreenY = 0.f;
    const bool_t valid =
        ReadStatusPanelU32(values, "version", version) && version == 1u &&
        ReadStatusPanelBool(values, "show", show) &&
        ReadStatusPanelFloat(values, "championWidth", 20.f, 200.f, championWidth) &&
        ReadStatusPanelFloat(values, "championHeight", 3.f, 32.f, championHeight) &&
        ReadStatusPanelFloat(values, "championYOffset", 0.5f, 6.f, championYOffset) &&
        ReadStatusPanelFloat(values, "championLevelX", -80.f, 20.f, championLevelX) &&
        ReadStatusPanelFloat(values, "championLevelY", -30.f, 30.f, championLevelY) &&
        ReadStatusPanelFloat(values, "championLevelFontScale", 0.5f, 2.f, championLevelFontScale) &&
        ReadStatusPanelFloat(values, "minionWidth", 20.f, 100.f, minionWidth) &&
        ReadStatusPanelFloat(values, "minionHeight", 3.f, 16.f, minionHeight) &&
        ReadStatusPanelFloat(values, "minionYOffset", 0.5f, 3.f, minionYOffset) &&
        ReadStatusPanelFloat(values, "structureWidth", 50.f, 240.f, structureWidth) &&
        ReadStatusPanelFloat(values, "structureHeight", 6.f, 40.f, structureHeight) &&
        ReadStatusPanelFloat(values, "structureYOffset", 1.f, 8.f, structureYOffset) &&
        ReadStatusPanelFloat(values, "structureScreenX", -120.f, 120.f, structureScreenX) &&
        ReadStatusPanelFloat(values, "structureScreenY", -120.f, 120.f, structureScreenY);
    if (!valid)
        return false;

    m_bShowHealthBars = show;
    m_fHPBarWidth = championWidth;
    m_fHPBarHeight = championHeight;
    m_fHPBarYOffset = championYOffset;
    m_fChampionLevelOffsetX = championLevelX;
    m_fChampionLevelOffsetY = championLevelY;
    m_fChampionLevelFontScale = championLevelFontScale;
    m_fUnitHPBarWidth = minionWidth;
    m_fUnitHPBarHeight = minionHeight;
    m_fUnitHPBarYOffset = minionYOffset;
    m_fStructureHPBarWidth = structureWidth;
    m_fStructureHPBarHeight = structureHeight;
    m_fStructureHPBarYOffset = structureYOffset;
    m_fStructureHPBarScreenOffsetX = structureScreenX;
    m_fStructureHPBarScreenOffsetY = structureScreenY;
    return true;
}

bool_t CUI_Manager::SaveWorldHealthBarLayoutSettings()
{
    std::filesystem::path path;
    if (!BuildWorldHealthBarLayoutSettingsPath(path))
        return false;
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError)
        return false;

    std::filesystem::path temporaryPath = path;
    temporaryPath += L".tmp";
    FILE* file = nullptr;
    if (_wfopen_s(&file, temporaryPath.c_str(), L"wb") != 0 || !file)
        return false;
    const i32_t writeResult = fprintf(
        file,
        "version=1\nshow=%u\n"
        "championWidth=%.9g\nchampionHeight=%.9g\nchampionYOffset=%.9g\n"
        "championLevelX=%.9g\nchampionLevelY=%.9g\nchampionLevelFontScale=%.9g\n"
        "minionWidth=%.9g\nminionHeight=%.9g\nminionYOffset=%.9g\n"
        "structureWidth=%.9g\nstructureHeight=%.9g\nstructureYOffset=%.9g\n"
        "structureScreenX=%.9g\nstructureScreenY=%.9g\n",
        m_bShowHealthBars ? 1u : 0u,
        m_fHPBarWidth, m_fHPBarHeight, m_fHPBarYOffset,
        m_fChampionLevelOffsetX, m_fChampionLevelOffsetY,
        m_fChampionLevelFontScale,
        m_fUnitHPBarWidth, m_fUnitHPBarHeight, m_fUnitHPBarYOffset,
        m_fStructureHPBarWidth, m_fStructureHPBarHeight,
        m_fStructureHPBarYOffset,
        m_fStructureHPBarScreenOffsetX, m_fStructureHPBarScreenOffsetY);
    const i32_t flushResult = fflush(file);
    const i32_t closeResult = fclose(file);
    if (writeResult < 0 || flushResult != 0 || closeResult != 0)
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    if (!MoveFileExW(
            temporaryPath.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    return true;
}
```

anonymous namespace path helper는 아래다.

```cpp
bool_t BuildWorldHealthBarLayoutSettingsPath(
    std::filesystem::path& outPath)
{
    wchar_t localAppData[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData,
        static_cast<DWORD>(std::size(localAppData)));
    if (length == 0u || length >= std::size(localAppData))
        return false;
    outPath = std::filesystem::path(localAppData) /
        L"Winters" / L"Developer" / L"world_health_bars.ini";
    return true;
}
```

#### `Engine/Public/Manager/UI/UI_Manager.h`

`LoadWorldHealthBarLayoutSettings`, `SaveWorldHealthBarLayoutSettings`, save message를 추가한다. 외부 Client tab은 Engine이 Client 타입을 알지 않도록 아래 generic callback만 받는다.

```cpp
using ImGuiExternalTabCallback = void(*)(void*);
void OnImGui_Tuner(
    ImGuiExternalTabCallback pfnExternalTabs = nullptr,
    void* pExternalUser = nullptr);
```

Engine의 tab bar 내부 마지막에 callback을 호출한다.

#### `Engine/Include/GameInstance.h`, `EngineSDK/inc/GameInstance.h`, `Engine/Private/GameInstance.cpp`

`UI_OnImGui_Tuner`에 동일한 raw function pointer와 user pointer를 전달한다. Engine/Client include 역전은 만들지 않는다.

#### `Client/Public/UI/MinimapPanel.h`, `Client/Private/UI/MinimapPanel.cpp`

기존 별도 window를 소유하는 `DrawTunerImGui`를 아래 signature의 tab content 전용 함수로 교체한다. 모든 scalar는 왼쪽 라벨 2열 helper를 사용한다.

```cpp
static bool_t DrawTunerContentsImGui(
    bool_t bProjectionSyncAvailable,
    MinimapProjection& OutAppliedProjection);
```

`MinimapRuntimeLayout`에 추가:

```cpp
f32_t ChampionOutlineThickness = 2.f;
```

champion border color helper:

```cpp
Vec4 ResolveChampionOutlineColor(const UI::MinimapIconView& icon)
{
    const f32_t alpha = icon.bAlive ? 0.98f : 0.58f;
    if (icon.eTeamId == eTeam::Blue)
        return Vec4{ 0.08f, 0.42f, 1.35f, alpha };
    if (icon.eTeamId == eTeam::Red)
        return Vec4{ 1.35f, 0.10f, 0.08f, alpha };
    return Vec4{ 0.015f, 0.018f, 0.022f, alpha };
}
```

portrait보다 `ChampionOutlineThickness`만큼 큰 circle을 먼저 그린다. `bAlly`가 아니라 `eTeamId`를 사용한다.

`%LOCALAPPDATA%/Winters/Developer/minimap_layout.ini` version 1에 layout, extent, outline thickness를 원자 저장한다. `GetDefaultMinimapProjection` 첫 호출 전 `EnsureMinimapLayoutSettingsLoaded()`를 실행하고, valid load 직후 반드시 `ApplyRuntimeMinimapProjection()`을 호출해 Vision/FoW 초기 투영과 일치시킨다. invalid/부분 파일은 전체 거부, `.tmp` write/flush/close 뒤 `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` 실패 시 temp를 삭제한다.

Client persistence의 정확한 helper/body는 아래로 고정한다.

```cpp
bool_t BuildMinimapLayoutSettingsPath(std::filesystem::path& outPath)
{
    wchar_t localAppData[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData,
        static_cast<DWORD>(std::size(localAppData)));
    if (length == 0u || length >= std::size(localAppData))
        return false;
    outPath = std::filesystem::path(localAppData) /
        L"Winters" / L"Developer" / L"minimap_layout.ini";
    return true;
}

bool_t ReadMinimapProfileFloat(
    const std::filesystem::path& path,
    const wchar_t* key,
    f32_t minimum,
    f32_t maximum,
    f32_t& outValue)
{
    wchar_t text[96]{};
    if (GetPrivateProfileStringW(
            L"Minimap", key, L"", text,
            static_cast<DWORD>(std::size(text)), path.c_str()) == 0u)
    {
        return false;
    }
    errno = 0;
    wchar_t* end = nullptr;
    const f32_t parsed = std::wcstof(text, &end);
    if (errno == ERANGE || end == text || !end || *end != L'\0' ||
        !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
    {
        return false;
    }
    outValue = parsed;
    return true;
}

bool_t ReadMinimapProfileVersion(
    const std::filesystem::path& path,
    u32_t& outVersion)
{
    wchar_t text[32]{};
    if (GetPrivateProfileStringW(
            L"Minimap", L"version", L"", text,
            static_cast<DWORD>(std::size(text)), path.c_str()) == 0u)
    {
        return false;
    }
    errno = 0;
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(text, &end, 10);
    if (errno == ERANGE || end == text || !end || *end != L'\0' ||
        parsed > (std::numeric_limits<u32_t>::max)())
    {
        return false;
    }
    outVersion = static_cast<u32_t>(parsed);
    return true;
}

void EnsureMinimapLayoutSettingsLoaded()
{
    if (s_bMinimapLayoutSettingsLoaded)
        return;
    s_bMinimapLayoutSettingsLoaded = true;

    std::filesystem::path path;
    if (!BuildMinimapLayoutSettingsPath(path))
        return;
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError)
        return;

    u32_t version = 0u;
    MinimapRuntimeLayout loaded{};
    f32_t extent = kCanonicalProjectionExtent;
    const bool_t valid =
        ReadMinimapProfileVersion(path, version) && version == 1u &&
        ReadMinimapProfileFloat(path, L"viewportHeightRatio", 0.18f, 0.55f,
            loaded.ViewportHeightRatio) &&
        ReadMinimapProfileFloat(path, L"rightPadding", 0.f, 64.f,
            loaded.RightPadding) &&
        ReadMinimapProfileFloat(path, L"bottomPadding", 0.f, 64.f,
            loaded.BottomPadding) &&
        ReadMinimapProfileFloat(path, L"iconScale", 0.65f, 2.f,
            loaded.IconScale) &&
        ReadMinimapProfileFloat(path, L"championScale", 0.75f, 3.f,
            loaded.ChampionScale) &&
        ReadMinimapProfileFloat(path, L"championOutlineThickness", 0.5f, 8.f,
            loaded.ChampionOutlineThickness) &&
        ReadMinimapProfileFloat(path, L"projectionExtent",
            kMinProjectionExtent, kMaxProjectionExtent, extent);
    if (!valid)
        return;

    s_MinimapLayout = loaded;
    s_fRuntimeProjectionExtent = extent;
    ApplyRuntimeMinimapProjection();
}

bool_t SaveMinimapLayoutSettings()
{
    std::filesystem::path path;
    if (!BuildMinimapLayoutSettingsPath(path))
        return false;
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError)
        return false;
    std::filesystem::path temporaryPath = path;
    temporaryPath += L".tmp";
    FILE* file = nullptr;
    if (_wfopen_s(&file, temporaryPath.c_str(), L"wb") != 0 || !file)
        return false;
    const i32_t writeResult = fprintf(
        file,
        "[Minimap]\nversion=1\n"
        "viewportHeightRatio=%.9g\nrightPadding=%.9g\nbottomPadding=%.9g\n"
        "iconScale=%.9g\nchampionScale=%.9g\n"
        "championOutlineThickness=%.9g\nprojectionExtent=%.9g\n",
        s_MinimapLayout.ViewportHeightRatio,
        s_MinimapLayout.RightPadding,
        s_MinimapLayout.BottomPadding,
        s_MinimapLayout.IconScale,
        s_MinimapLayout.ChampionScale,
        s_MinimapLayout.ChampionOutlineThickness,
        s_fRuntimeProjectionExtent);
    const i32_t flushResult = fflush(file);
    const i32_t closeResult = fclose(file);
    if (writeResult < 0 || flushResult != 0 || closeResult != 0)
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    if (!MoveFileExW(
            temporaryPath.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    return true;
}
```

`s_bMinimapLayoutSettingsLoaded=false`와 save message string을 static runtime state에 추가한다. `GetDefaultMinimapProjection`, `ResolveMinimapRect`, `DrawTunerContentsImGui` 시작에서 ensure를 호출한다. Save button은 성공/실패 상태 문구를 표시한다.

#### `Client/Private/Scene/Scene_InGameImGui.cpp`

non-capturing callback + 아래 local context로 Engine UI Manager tab bar에 `Minimap` tab을 주입한다. callback이 `BeginTabItem("Minimap")/EndTabItem()`을 소유하고 content 함수는 tab body만 소유한다. callback 종료 후 `bProjectionChanged`일 때만 기존처럼 `CVisionSystem::SetFowProjection`에 즉시 반영한다. 별도 `UI Manager - Minimap` 창은 제거한다.

```cpp
struct MinimapTunerContext
{
    bool_t bProjectionSyncAvailable = false;
    bool_t bProjectionChanged = false;
    UI::MinimapProjection AppliedProjection{};
};
```

기존 `UI_OnImGui_Tuner();`와 별도 minimap window 호출 block 전체를 아래로 교체한다.

```cpp
MinimapTunerContext minimapContext{};
minimapContext.bProjectionSyncAvailable = m_pVisionSystem != nullptr;
const auto drawExternalTabs = [](void* user)
{
    auto* context = static_cast<MinimapTunerContext*>(user);
    if (!context || !ImGui::BeginTabItem("Minimap"))
        return;
    context->bProjectionChanged =
        UI::CMinimapPanel::DrawTunerContentsImGui(
            context->bProjectionSyncAvailable,
            context->AppliedProjection);
    ImGui::EndTabItem();
};
CGameInstance::Get()->UI_OnImGui_Tuner(
    drawExternalTabs,
    &minimapContext);
if (minimapContext.bProjectionChanged && m_pVisionSystem)
{
    Engine::CVisionSystem::FowProjection fowProjection{};
    fowProjection.vWorldAtUv00 =
        minimapContext.AppliedProjection.vWorldAtUv00;
    fowProjection.vWorldAtUv10 =
        minimapContext.AppliedProjection.vWorldAtUv10;
    fowProjection.vWorldAtUv01 =
        minimapContext.AppliedProjection.vWorldAtUv01;
    m_pVisionSystem->SetFowProjection(fowProjection);
}
```

#### `Client/Private/UI/ChampionTuner.cpp`

기존 `EditFloat`/`EditDragFloat`는 stat/rank table cell 전용으로 유지하고 visible label을 그리지 않는 `EditFloatCell`/`EditDragFloatCell` 역할로 둔다. 별도 `EditLabeledFloatRow`/`EditLabeledIntRow` 2열 helper를 추가하여 Minions/Towers/Objectives/Attack Resource의 standalone scalar만 교체한다. Champion Stats와 Skill rank table에는 중첩 table을 만들지 않는다.

### 3-5. 이렐리아 Q wall transit

#### `Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp`

`IreliaGameSim.h`에 authoritative cast gate를 선언하고 `IreliaGameSim.cpp`에 target-side landing resolver를 추가한다. resolver는 fail-open하지 않는다.

```cpp
bool_t TryResolveIreliaQTargetSideLanding(
    const TickContext& tc,
    const Vec3& targetPos,
    const Vec3& rawLanding,
    Vec3& outLanding)
{
    outLanding = rawLanding;
    if (!tc.pWalkable)
        return true;
    if (tc.pWalkable->IsWalkableXZ(rawLanding))
        return true;

    return tc.pWalkable->TryResolveMoveTarget(
        targetPos, rawLanding, outLanding) &&
        tc.pWalkable->IsWalkableXZ(outLanding);
}
```

```cpp
bool_t CanCastBladesurge(
    CWorld& world,
    const TickContext& tc,
    EntityID caster,
    EntityID target);
```

`CanCastBladesurge` 전체 정의는 아래다. caster/target transform, live target, Q effective range, target-side landing resolve를 검사한다. `SegmentWalkableXZ`는 의도적으로 검사하지 않는다.

```cpp
bool_t CanCastBladesurge(
    CWorld& world,
    const TickContext& tc,
    EntityID caster,
    EntityID target)
{
    if (caster == NULL_ENTITY || target == NULL_ENTITY ||
        !world.IsAlive(caster) || !world.IsAlive(target) ||
        !world.HasComponent<TransformComponent>(caster) ||
        !world.HasComponent<TransformComponent>(target) ||
        !world.HasComponent<HealthComponent>(target) ||
        !GameplayStateQuery::CanBeTargetedBy(world, caster, target))
    {
        return false;
    }
    const HealthComponent& health = world.GetComponent<HealthComponent>(target);
    if (health.bIsDead || health.fCurrent <= 0.f)
        return false;

    const Vec3 casterPos = world.GetComponent<TransformComponent>(caster)
        .GetLocalPosition();
    const Vec3 targetPos = world.GetComponent<TransformComponent>(target)
        .GetLocalPosition();
    const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
        world, caster, tc, eChampion::IRELIA,
        static_cast<u8_t>(eSkillSlot::Q));
    const f32_t effectiveRange = range +
        GameplayStateQuery::ResolveGameplayRadius(world, caster) +
        GameplayStateQuery::ResolveGameplayRadius(world, target);
    if (range <= 0.f ||
        WintersMath::DistanceSqXZ(casterPos, targetPos) >
            effectiveRange * effectiveRange)
    {
        return false;
    }

    const f32_t gap = ResolveIreliaSkillEffectParam(
        world, tc, caster, eSkillSlot::Q,
        eSkillEffectParamId::Gap, kQStopGapFallback);
    const Vec3 rawLanding = ResolveQDashEndPos(casterPos, targetPos, gap);
    Vec3 landing{};
    if (!TryResolveIreliaQTargetSideLanding(
            tc, targetPos, rawLanding, landing))
    {
        return false;
    }
    const f32_t maxLandingDistance = gap + 1.5f;
    return WintersMath::DistanceSqXZ(landing, targetPos) <=
        maxLandingDistance * maxLandingDistance;
}
```

`CommandExecutor::HandleCastSkill`의 champion rule 구간에서 cooldown/mana 차감 전에 아래를 추가한다.

```cpp
if (!bStage2 &&
    hookChampion == eChampion::IRELIA &&
    hookSlot == static_cast<u8_t>(eSkillSlot::Q) &&
    !IreliaGameSim::CanCastBladesurge(
        world, tc, cmd.issuerEntity, effectiveCmd.targetEntity))
{
    LogCastSkill("reject", "invalid-irelia-q-target-or-landing", cmd, hookChampion, 0.f);
    return CommandExecutionResult::Rejected(
        cmd.sequenceNum,
        eCommandExecutionReason::ChampionRuleBlocked);
}
```

Q Tick의 `TryClampMoveSegmentXZ`/`bBlocked` block을 삭제하고 아래로 교체한다.

```cpp
const Vec3 rawEnd = ResolveQDashEndPos(currentPos, targetPos, gap);
Vec3 desiredEnd{};
if (!TryResolveIreliaQTargetSideLanding(
        tc, targetPos, rawEnd, desiredEnd))
{
    SnapDashArrivalToWalkable(world, tc, entity, state.dashStartPos);
    state.bDashActive = false;
    state.dashSpeed = 0.f;
    state.dashTarget = NULL_ENTITY;
    state.qRank = 0u;
    return;
}
state.dashEndPos = desiredEnd;

// 기존 step/contact 계산을 유지하되 transit은 wall clamp 없이 직접 적용한다.
transform.SetPosition(desiredPos);
```

contact 분기는 아래 계약을 사용한다.

```cpp
if (bContact)
{
    const u8_t impactRank = state.qRank;
    state.bDashActive = false;
    state.dashSpeed = 0.f;
    state.dashTarget = NULL_ENTITY;
    state.qRank = 0u;
    const Vec3 finalPos = transform.GetLocalPosition();
    const f32_t maxImpactDistance = gap + 1.5f;
    const bool_t bValidArrival =
        (!tc.pWalkable || tc.pWalkable->IsWalkableXZ(finalPos)) &&
        WintersMath::DistanceSqXZ(finalPos, targetPos) <=
            maxImpactDistance * maxImpactDistance;
    ReleaseIreliaQAction(world, tc, entity);
    if (bValidArrival)
        ResolveIreliaQImpact(world, tc, entity, target, impactRank);
}
```

`CanMove`/target 소멸 취소 시 현재 위치가 walkable이 아니면 기존 `dashStartPos` anchor로 `SnapDashArrivalToWalkable`을 호출한 뒤 state를 초기화한다. 중간 wall은 의도적으로 cast reject 조건이 아니다.

#### `Tools/SimLab/main.cpp`

`RunCancelledCase(true)`의 옛 “clamp 실패 = Q 취소” 기대를 제거하고 `RunWallTransitCase`로 교체한다.

```text
- segment/clamp query는 실패하도록 설정
- start와 target-side landing은 walkable/resolve 가능
- Q가 반대편에 도착
- damage event 정확히 1회
- mark/kill reset 기존 계약 유지
- transit 중 clamp query count 0
```

`cannot move` 취소 case는 유지하고, mid-wall 취소 후 walkable 복구, cast 시 unresolvable landing reject, 이동 중 landing 소실 시 no-impact case를 추가한다.

### 3-6. Red WFX를 Blue 형상으로 통일

#### `Data/LoL/FX/Object/Jungle/red_buff.wfx`

전체를 아래로 교체한다.

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Objective.Red.Buff",
  "emitters": [
    {
      "name": "red_ground_mark",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/common_flashblue_02.png",
      "lifetime": 300.0,
      "fade_in": 0.15,
      "fade_out": 0.35,
      "width": 2.2,
      "height": 2.2,
      "color": [2.1, 0.20, 0.08, 0.62],
      "attach_offset": [0.0, 0.065, 0.0],
      "billboard": false
    }
  ]
}
```

## 4. 자동 검증과 빌드

### 4-1. 정적/데이터

```powershell
git diff --check
python Tools/LoLData/Test-F4BalanceContracts.py

$blue = Get-Content Data/LoL/FX/Object/Jungle/blue_buff.wfx -Raw | ConvertFrom-Json
$red = Get-Content Data/LoL/FX/Object/Jungle/red_buff.wfx -Raw | ConvertFrom-Json
$elder = Get-Content Data/LoL/FX/Object/Jungle/elder_basic_attack.wfx -Raw | ConvertFrom-Json
$shapeKeys = @('render_type','blend_mode','depth_mode','texture','lifetime','fade_in','fade_out','width','height','attach_offset','billboard')
foreach ($key in $shapeKeys) {
    if (($blue.emitters[0].$key | ConvertTo-Json -Compress) -ne
        ($red.emitters[0].$key | ConvertTo-Json -Compress)) { throw "red shape mismatch: $key" }
}
if ($elder.name -ne 'Objective.Elder.BasicAttack') { throw 'elder cue mismatch' }
```

### 4-2. 자동 실행

```powershell
Tools/Bin/Debug/SimLab.exe --irelia-q-only
Tools/Bin/Debug/SimLab.exe --objective-buffs-only
Tools/Bin/Debug/SimLab.exe --gamefeel-only
Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 2 -Configuration Debug -HeartbeatTicks 1800 -SkipServerBuild
Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 120 -Seed 42 -Runs 1 -Configuration Release -HeartbeatTicks 120 -SkipServerBuild
```

structure nav probe는 runtime refresh wall time과 derived rebuild call count를 JSON 결과에 남긴다.

```text
scope: ServerStructureNav::AuthoritativeDeathRefreshTick
counters: ServerStructureNav::DerivedRebuildCalls,
          ServerStructureNav::RefreshWorkCalls,
          ServerStructureNav::SecondRefreshWorkCalls
budget: RefreshTickUs max <= 33,333us,
        DerivedRebuildCalls == 0,
        RefreshWorkCalls == 1,
        SecondRefreshWorkCalls == 0
artifact: .md/build/evidence/s024_bot_soak/release_*/run_01/structure_nav_probe.json
```

Release client replay 성능 캡처는 동일 roster/replay로 아래 명령을 사용한다.

```powershell
powershell -File Tools/Profiler/run_profile_session.ps1 `
    -Configuration Release `
    -ReplayPath Replay/room1_tick1_1393.wrpl `
    -RunSeconds 40 `
    -AnalyzeTargetFps 144 `
    -Label runtime-nav-jungle-ui-after
python Tools/Profiler/analyze_profiler_capture.py `
    (Get-ChildItem Profiles/profiler_*.json | Sort-Object LastWriteTime | Select-Object -Last 1).FullName `
    --target-fps 144
```

`Jungle::Update` p99는 0.6ms 이하를 유지한다. 초과하면 Baron의 알려진 update 계단 문제를 되살리는 20Hz rollback으로 완료 처리하지 않는다. full-rate를 유지할 비용 절감 또는 사용자가 승인한 새 예산이 없으면 `성능-시각 계약 미해결`로 중단/실패한다. replay에 Baron/Dragon이 모두 없으면 이 gate는 `NOT_EXERCISED`로 기록하고 F5 동일 roster 캡처 전 PASS로 과장하지 않는다.

SimLab objective probe에 Baron matrix를 추가한다.

```text
- 피격 전 proximity command 0
- 실제 champion damage 뒤 bAggro 1
- range 밖 command/chase/move 0, 300 tick anchor 고정
- range 안 BA/damage 1+
- target 이탈 뒤 chase 0, anchor 고정
- 기존 chase/move를 미리 심은 상태에서도 stationary 첫 tick 뒤 둘 다 제거
- Dragon cue replay rebase 뒤 orphan live cue 0
```

### 4-3. 빌드

```powershell
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsroot 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
```

## 5. 수동 F5/ImGui QA

- 단축키는 `F4 Balance`, `F8 UI Manager`, `F7 WFX`로 분리해 확인한다.
- 포탑 3개 연속 파괴: 체감 정지 없음. 자동 probe에서 derived rebuild 0, first refresh work 1, second refresh work 0, 해당 authoritative room tick 33.33ms 이하.
- Kalista/Irelia 포탑 우클릭: raw center 사거리 안까지 진입하고 Run/Idle 제자리 반복 0.
- 미니언: 적 추적 후 lane 앞으로 지나간 상태에서 적 사망, 과거 waypoint로 후진 0.
- Dragon: idle/move/BA 반복 동안 wing loop 유지, 깨진 attack1 선택 0, BA impact마다 FX 1회.
- Baron: 피격 전 선공/이동 0, 피격 후 범위 내 공격, 둥지 XZ 고정, 기존 attack clip 정상.
- Level-up: 1초간 이동하는 가시 아군 champion과 FX 중심 XZ 차 0.05m 이하, 1초 후 제거.
- UI 340px 폭: label 전부 왼쪽에서 잘리지 않음. Health Bar/Minimap 수치 Save -> 재시작 -> 동일값. invalid version 파일은 default fallback.
- 미니맵: Blue champion blue outline, Red champion red outline, thickness 0.5/2/8px 즉시 반영.
- Irelia Q: 벽 반대편 target Q가 벽을 통과해 target-side landing, impact 1회. CC/target loss 취소 위치는 walkable.
- Blue/Red buff: 동일 형상, 각각 청/적색, champion attach와 flag 제거 정상.

## 6. 완료/중단 기준

- 자동 검증 및 네 target Debug x64 빌드가 모두 PASS해야 코드 완료로 기록한다.
- 시각 항목은 실제 F5 캡처가 없으면 `빌드 완료 / 시각 확인 필요`로 기록하며 PASS를 과장하지 않는다.
- 포탑 파괴 tick에서 derived rebuild가 한 번이라도 호출되면 P0 미해결이다.
- Baron snapshot에 moving flag가 남거나 anchor가 변하면 stationary 계약 실패다.
- UI Save가 현재 process에만 남으면 persistence 미완료다.
- critique gate의 residual P0/P1이 0이 되기 전 source edit을 시작하지 않는다.

## 7. 독립 비평 disposition

- 1차 상태: nav `P0 1/P1 4`, UI/Irelia `P0 0/P1 6`, jungle `P0 0/P1 4`로 gate FAIL.
- `ACCEPT` runtime carve 뒤 hash 갱신 및 second-refresh no-op probe: hash 누락은 매 tick carve 회귀를 만든다.
- `ACCEPT` waypoint helper를 private member로 확정하고 lateral corridor + 1-step cap을 추가: private 접근 오류와 parallel/hairpin 과진행을 막는다.
- `ACCEPT` AttackChase `--gamefeel-only`, deterministic combat-exit fixture, runtime refresh timing/counter: soak 우연성으로 검증하지 않는다.
- `ACCEPT` Red WFX를 실제 Blue GroundDecal emitter와 색 이외 동일하게 교체: 최초 Billboard 안은 사용자 요구를 위반했다.
- `ACCEPT` Irelia Q cast gate/fail-closed landing/no-impact recovery: 벽 통과 허용과 벽 안 도착 허용은 다른 계약이다.
- `ACCEPT` Balance cell helper와 standalone labeled-row helper 분리: 기존 3열 stat table 중첩 회귀를 막는다.
- `ACCEPT` minimap valid load 직후 projection apply, callback context/return 계약, persistence full body, F4/F8/F7 단축키 정정.
- `ACCEPT` Dragon 단일 base id exact block, stationary Baron의 두 chase 차단 지점, Dragon cue timeline 등록.
- `ACCEPT` Baron full-frame `Jungle::Update` p99 0.6ms gate. 초과 시 20Hz rollback으로 완료하지 않고 `성능-시각 계약 미해결`로 중단한다.
- `REJECT` runtime마다 derived nav를 background rebuild하는 대안: 이번 P0은 단일-thread room state와 cache 소유권을 확대하지 않고 프리즈를 제거하는 범위다. durable dynamic blocker는 후속 세션이다.
- 최종 재비평: nav `P0 0/P1 0`, UI/Irelia `P0 0/P1 0`, jungle `P0 0/P1 0`. source-edit gate `PASS`.
- `ACCEPT` structure probe의 전체 evidence 선언·caller·stdout·JSON을 실측 counter로 완전 연결하고 성능 계약을 `AuthoritativeDeathRefreshTick <= 33,333us`로 단일화했다.

## 8. Implementation-time nav correction (2026-07-19)

- Additional code inspection found that the original base-only runtime carve was incomplete: `CWalkabilityAuthority` selects `m_pPathNavGrid` before `m_pNavGrid`, so a dead structure could remain blocked for target resolution and path construction even after the base cell reopened.
- The corrected contract is: keep the derived grid allocations stable and forbid full `BuildInflated`/flow-field rebuilds, but locally recompute derived walkability only around structures that changed from blocking to dead. The local value is derived from the already refreshed base grid with each configured clearance radius, so overlapping terrain and live structures remain blocked.
- Because changing derived bits invalidates the Pathfinder reachability cache, verification must include the first path query after refresh, not only the refresh tick. If that query exceeds the 33,333us room-tick budget, this slice is not complete and must not hide the cost in the next tick.

## 9. Implementation-time deterministic keyframe correction (2026-07-19)

- Two same-seed 1,800-tick soaks produced the same replay hash and the same gameplay assertions, but different final world-keyframe hashes. Binary localization identified only `ChampionAssistCreditComponent` as different: 111 bytes across three elements, all inside implicit alignment padding in `Credit` (`EntityID -> u64_t` and trailing bytes after `u8_t`).
- `KeyframeComponentRegistry::PodSave` serializes trivially-copyable component storage byte-for-byte, so indeterminate padding violates the harness byte-determinism contract even though every semantic field is equal.
- Keep `Credit` at 24 bytes and `ChampionAssistCreditComponent` at 192 bytes, but replace the implicit gaps with explicit zero-initialized reserved members. This is an ABI-size-preserving serialization correction; assist ownership, expiry, and reward behavior do not change.
- Verification: rebuild GameSim/Server/harness, repeat two same-seed 1,800-tick soaks, and require both replay SHA-256 and final world-keyframe SHA-256 to match.
- The structure probe must prove all of the following: the base cell is open, both configured derived grids open a valid probe cell, an actual move path crosses/reaches the released region, no full derived rebuild occurs, the second refresh is a no-op, and refresh plus first-path timings are reported separately.
- The earlier `bDerivedStable` revision assertion is replaced by stable allocation plus bounded local revision; revision equality would incorrectly forbid the required functional repair.
- Independent follow-up critique correctly classified base-only release as residual P0. `ACCEPT`: actual path/lane release and first-query timing are now mandatory. `REJECT`: transition-only full `BuildInflated + Prewarm + six flow fields` as the implementation, because that is the measured synchronous cascade responsible for the reported one-second freeze and therefore cannot satisfy the primary user contract. The local repair recomputes every structure footprint on each structure-state transition (covering simultaneous deaths and overlap), keeps allocations stable, and falls back from a stale zero flow cell to the normal minion path builder. Any lazy reachability cost is exposed by the first champion/minion query gates rather than hidden.

## 10. 2026-07-19 사용자 인터뷰 확정 delta

### 10-1. 관찰 사실과 구현 계약

- 포탑 파괴는 전체 navmesh/flow field 재생성이 아니다. 죽은 구조물 주변의 base cell과 이미 존재하는 clearance별 derived grid cell만 국소 재계산한다. derived grid allocation은 유지하고 `BuildInflated`, 전체 reachability prewarm, six-lane flow rebuild는 0회여야 한다.
- 미니언 후진은 일반 이동 중 경로 반전이 아니다. 미니언이 현재 waypoint 도달 전에 적 챔피언/미니언에게 aggro되어 lane을 이탈하고, 교전 종료 뒤 stale waypoint index/path를 재사용하면서 이미 뒤가 된 waypoint로 복귀하는 문제다. `combat -> lane` 전환에서 runtime chase path를 폐기하고, 현재 위치가 segment를 이미 통과한 경우에만 index를 최대 1칸 앞으로 보정한다. index 감소와 일반 lane tick 보정은 금지한다.
- Elder Dragon은 idle/move/attack 모두 같은 wing-flap loop를 유지한다. 공격 판정은 서버 권위 BA이고 Client는 대상 impact FX만 1회 재생한다. 깨진 attack clip은 선택하지 않는다.
- Baron은 피격 전 idle, 피격 후에도 spawn anchor에서 움직이지 않는다. 범위 안 champion만 공격하며 범위 밖 대상을 chase하지 않는다. 기존 정상 attack clip은 유지한다.
- Irelia Q는 cast gate 통과 뒤 transit 중 벽 충돌로 끊지 않는다. target-side landing을 우선하되 CC/target loss 등으로 취소되면 현재 도착점이 walkable이면 그대로 정지하고, 아니면 dash 시작점에서 도달 가능한 현재점 인근의 가장 가까운 walkable 위치로 resolve해 정지한다. 취소 뒤 impact/damage는 0회다.
- 미니맵 champion portrait는 기존 원형 이미지를 유지하고, portrait 바깥에 team별 Blue/Red 원형 outline을 그린다. outline thickness는 UI Manager의 Minimap tab에서 `0.5..8.0 px`로 조절·저장한다.

### 10-2. UI Manager 작업 계약

사용자 작업은 `F8 -> 대상 tab -> 왼쪽 label로 변수 의미 확인 -> 오른쪽 control 조절 -> 해당 tab만 Save` 또는 `여러 tab 조절 -> 하단 Save All`이다.

```text
┌ UI Manager ───────────────────────────────────────────┐
│ [HUD] [Health Bars] [Cursor] [Minimap]                │
│ ┌ scrollable tab body ──────────────────────────────┐ │
│ │ Label                         [value / slider]     │ │
│ │ unit/help/error text                               │ │
│ │ ...                                                │ │
│ │ [Reset] [Save]        <- 독립 저장                 │ │
│ └────────────────────────────────────────────────────┘ │
│ [Save All]  HUD Layout · Health Bars · Minimap 상태   │
└────────────────────────────────────────────────────────┘
```

| scope | 독립 Save | Save All 포함 | 저장 위치/권위 |
|---|---:|---:|---|
| HUD Layout | O | O | Actor HUD layout JSON |
| Health Bars + champion level | O | O | developer-local `world_health_bars.ini` |
| Minimap layout/projection/outline | O | O | developer-local `minimap_layout.ini` |
| Cursor runtime toggle/size | X | X | 이번 요청에서 persistence owner가 없는 runtime-only 값 |
| F4 Balance | 기존 `Save & Hot Load` | X | 서버 권위 canonical gameplay data; UI presentation Save All과 혼합 금지 |

- Save All action budget은 전역 버튼 1개다. 각 tab의 독립 Save는 유지한다.
- Save All은 위 세 저장 함수를 모두 시도하고, 하나라도 실패하면 성공으로 표시하지 않으며 실패 scope를 한 줄로 표시한다. 파일 간 transaction/rollback은 이번 범위 밖이다.
- body는 `BeginChild` scroll 영역, Save All은 child 밖 하단 action 영역에 둔다. 340px 폭에서도 label이 먼저 보이고 control ID는 `##hidden_id`를 쓴다.
- 단일 값 행은 `왼쪽 label | 오른쪽 control`, 단위·도움말·오류는 다음 줄 규칙을 유지한다.

### 10-3. 추가 소스 변경 preview

`Engine/Public/Manager/UI/UI_Manager.h`의 callback 및 tuner 선언을 아래로 교체한다.

```cpp
using ImGuiExternalTabCallback = void(*)(void*);
using ImGuiExternalSaveAllCallback = bool_t(*)(void*);

void OnImGui_Tuner(
    ImGuiExternalTabCallback pfnExternalTabs = nullptr,
    ImGuiExternalSaveAllCallback pfnExternalSaveAll = nullptr,
    void* pExternalUser = nullptr);
```

같은 header의 UI 저장 메시지 멤버 아래에 추가한다.

```cpp
std::string m_strSaveAllMessage;
```

`Engine/Include/GameInstance.h`의 `UI_OnImGui_Tuner` 선언을 아래로 교체하고, `Engine/Private/GameInstance.cpp` forwarding도 같은 인자 순서로 교체한다. `EngineSDK/inc/GameInstance.h`는 수동 편집하지 않고 정상 Engine build의 UpdateLib 산출물로 동기화한다.

```cpp
void UI_OnImGui_Tuner(
    void(*pfnExternalTabs)(void*) = nullptr,
    bool_t(*pfnExternalSaveAll)(void*) = nullptr,
    void* pExternalUser = nullptr);
```

`Client/Public/UI/MinimapPanel.h`의 `DrawTunerContentsImGui` 선언 아래에 추가한다.

```cpp
static bool_t SaveTunerSettings();
```

`Client/Private/UI/MinimapPanel.cpp`의 독립 Save 버튼은 아래 wrapper를 호출하도록 교체하고, namespace의 `DrawTunerContentsImGui` 바로 앞에 wrapper 정의를 추가한다.

```cpp
bool_t CMinimapPanel::SaveTunerSettings()
{
    const bool_t bSaved = SaveMinimapLayoutSettings();
    s_strMinimapLayoutSaveMessage = bSaved
        ? "Saved to %LOCALAPPDATA%/Winters/Developer/minimap_layout.ini"
        : "Save failed";
    return bSaved;
}
```

`Client/Private/Scene/Scene_InGameImGui.cpp`의 UI tuner callback block에 아래 callback을 추가하고 세 인자를 전달한다.

```cpp
const auto saveExternalTabs = [](void*) -> bool_t
{
    return UI::CMinimapPanel::SaveTunerSettings();
};
CGameInstance::Get()->UI_OnImGui_Tuner(
    drawExternalTabs,
    saveExternalTabs,
    &minimapContext);
```

`Engine/Private/Manager/UI/UI_Manager.cpp`의 `OnImGui_Tuner`는 기존 tab body를 scroll child 안에 두고 child 뒤에 아래 Save All action을 추가한다.

```cpp
if (ImGui::Button("Save All"))
{
    std::vector<const char*> failedScopes;
    if (!m_pActorHudPanel || !m_pActorHudPanel->SaveLayout())
        failedScopes.push_back("HUD Layout");
    if (!SaveWorldHealthBarLayoutSettings())
        failedScopes.push_back("Health Bars");
    if (!pfnExternalSaveAll || !pfnExternalSaveAll(pExternalUser))
        failedScopes.push_back("Minimap");

    if (failedScopes.empty())
    {
        m_strSaveAllMessage = "Saved HUD Layout, Health Bars, and Minimap.";
    }
    else
    {
        m_strSaveAllMessage = "Save All failed:";
        for (const char* scope : failedScopes)
        {
            m_strSaveAllMessage += " ";
            m_strSaveAllMessage += scope;
        }
    }
}
```

### 10-4. 추가 검증과 완료 조건

- 기존 `--gamefeel-only`는 aggro가 waypoint 도달 전에 발생하는 fixture, combat 종료 직후 stale path 제거, 현재 segment를 이미 통과했을 때 1-step forward, 아직 통과하지 않았을 때 index 유지, index 감소 0을 검사한다.
- `--irelia-q-only`는 벽 transit 성공 외에 취소 위치 walkable, 시작점 쪽 강제 복귀 없음, 취소 damage 0을 검사한다.
- UI callback signature는 Engine/GameInstance/Client 전체 compile로 검증하고 UpdateLib 뒤 EngineSDK signature도 일치해야 한다.
- Save All 자동 계약은 세 저장 callback이 모두 호출되는 source/build gate로 닫고, 실제 파일 세 개의 재시작 round-trip과 340px scroll/footer 시각 확인은 F5 manual gate로 분리한다.
- 이번 delta도 독립 read-only critique에서 residual P0/P1 0이 되기 전 source edit을 시작하지 않는다.

### 10-5. Delta 1차 독립 비평 disposition

- 1차 판정은 `P0 0 / P1 3`으로 gate FAIL이었다.
- `ACCEPT` Irelia Q 취소 좌표 규약을 production helper의 정확한 호출 의미와 deterministic fixture로 명시한다. `SnapDashArrivalToWalkable(world, tc, entity, state.dashStartPos)`의 네 번째 인자는 복귀 목적지가 아니라 reachability anchor이고, raw target은 현재 dash 위치다. 서버 `ResolveMoveTarget(fromBeforeDash, arrived, snapped)`는 현재점에 가장 가까운 시작점 도달 가능 cell을 반환한다. 시작점 자체로 되감는 코드는 추가하지 않는다.
- `ACCEPT` scroll child가 footer 공간을 먹지 않도록 footer 높이를 먼저 예약한다.
- `ACCEPT` Save All 결과 문자열을 footer에 실제 렌더하고 실패 scope를 comma delimiter로 표시한다.
- `ACCEPT(P2)` 세 저장은 short-circuit 표현식으로 묶지 않고 각각 먼저 호출한 뒤 집계한다. 별도 ImGui unit harness 신설은 이번 surgical 범위를 넘으므로 Client compile과 수동 read-only/writable path QA로 검증한다.

`Shared/GameSim/Systems/Move/DashArrival.h`의 아래 현행 block을 계약 구현으로 고정하며 production source는 재작성하지 않는다.

```cpp
const Vec3 arrived = transform.GetLocalPosition();

if (tc.pWalkable->IsWalkableXZ(arrived))
    return;

Vec3 snapped = arrived;
if (!tc.pWalkable->TryResolveMoveTarget(fromBeforeDash, arrived, snapped))
    return;

f32_t surfaceY = snapped.y;
if (tc.pWalkable->TrySampleHeight(snapped.x, snapped.z, surfaceY))
    snapped.y = surfaceY;
transform.SetPosition(snapped);
```

`Tools/SimLab/main.cpp`의 `FlatWalkable`에 아래 deterministic resolve override를 추가한다.

```cpp
bool_t bOverrideResolvedMoveTarget = false;
Vec3 overriddenResolvedMoveTarget{};
mutable u32_t resolveQueryCount = 0u;
mutable Vec3 lastResolveFrom{};
mutable Vec3 lastResolveRawTarget{};

bool_t TryResolveMoveTarget(
    const Vec3& from, const Vec3& rawTarget, Vec3& outTarget) const override
{
    ++resolveQueryCount;
    lastResolveFrom = from;
    lastResolveRawTarget = rawTarget;
    if (!bResolveMoveTarget)
        return false;
    outTarget = bOverrideResolvedMoveTarget
        ? overriddenResolvedMoveTarget
        : rawTarget;
    return true;
}
```

`RunWallTransitOrCancelledCase(false)`는 dash가 한 tick 전진한 뒤 현재점을 unwalkable로 바꾸고 cannot-move 취소를 발생시킨다. resolve 결과를 현재점 인근 좌표로 고정한 뒤 아래를 추가 검증한다.

```cpp
const Vec3 cancelPosition =
    world.GetComponent<TransformComponent>(irelia).GetPosition();
return !world.GetComponent<IreliaSimComponent>(irelia).bDashActive &&
    walkable.resolveQueryCount == 1u &&
    WintersMath::DistanceSqXZ(
        walkable.lastResolveFrom, Vec3{}) <= 0.0001f &&
    WintersMath::DistanceSqXZ(
        walkable.lastResolveRawTarget, cancelPositionBeforeResolve) <= 0.0001f &&
    WintersMath::DistanceSqXZ(
        cancelPosition, walkable.overriddenResolvedMoveTarget) <= 0.0001f &&
    cancelPosition.x > 0.f &&
    world.GetComponent<HealthComponent>(target).fCurrent == hpBefore &&
    CountQDamageEvents(world, irelia, target) == 0u &&
    world.HasComponent<IreliaMarkComponent>(target) &&
    qState.cooldownRemaining > 0.1f;
```

`Engine/Private/Manager/UI/UI_Manager.cpp`의 body/footer 배치는 아래 exact skeleton으로 교체한다. 기존 tab 내부 body는 표시한 위치에 그대로 이동하며 독립 Save는 유지한다.

```cpp
const f32_t fFooterHeight =
    ImGui::GetFrameHeightWithSpacing() * 2.f +
    ImGui::GetStyle().ItemSpacing.y;
if (ImGui::BeginChild(
        "UIManagerScrollBody",
        ImVec2(0.f, -fFooterHeight),
        false,
        ImGuiWindowFlags_AlwaysVerticalScrollbar))
{
    if (ImGui::BeginTabBar("UIManagerEssentials"))
    {
        // 기존 HUD / Health Bars / Cursor / external Minimap tab body
        ImGui::EndTabBar();
    }
}
ImGui::EndChild();

ImGui::Separator();
if (ImGui::Button("Save All"))
{
    std::vector<const char*> failedScopes;
    const bool_t bHudSaved =
        m_pActorHudPanel && m_pActorHudPanel->SaveLayout();
    const bool_t bHealthBarsSaved =
        SaveWorldHealthBarLayoutSettings();
    const bool_t bMinimapSaved =
        pfnExternalSaveAll && pfnExternalSaveAll(pExternalUser);
    if (!bHudSaved)
        failedScopes.push_back("HUD Layout");
    if (!bHealthBarsSaved)
        failedScopes.push_back("Health Bars");
    if (!bMinimapSaved)
        failedScopes.push_back("Minimap");

    if (failedScopes.empty())
    {
        m_strSaveAllMessage =
            "Saved HUD Layout, Health Bars, and Minimap.";
    }
    else
    {
        m_strSaveAllMessage = "Save All failed: ";
        for (std::size_t i = 0; i < failedScopes.size(); ++i)
        {
            if (i > 0u)
                m_strSaveAllMessage += ", ";
            m_strSaveAllMessage += failedScopes[i];
        }
    }
}
if (!m_strSaveAllMessage.empty())
    ImGui::TextWrapped("%s", m_strSaveAllMessage.c_str());
```

- 2차 독립 비평은 위 보강 뒤 residual P0/P1 0을 다시 요구한다.

### 10-6. Delta 2차 독립 비평 disposition

- 2차 판정은 `P0 0 / P1 1`이었다. scroll/footer와 Save All 결과 렌더 두 항목은 해소되었다.
- `ACCEPT` FlatWalkable fixture가 resolver 반환값만 검사하면 production의 `from/rawTarget` 인자가 뒤바뀌어도 통과한다. 위와 같이 `lastResolveFrom`, `lastResolveRawTarget`, `resolveQueryCount`를 기록하고 dash 시작점과 취소 직전 위치를 각각 단언한다.
- `cancelPositionBeforeResolve`는 cannot-move flag를 세우기 직전 transform 위치로 캡처하며 `x > 0`을 요구한다. 따라서 “한 tick 전진 후 취소”와 “현재점 인근 resolve”를 동시에 증명한다.
- 3차 독립 비평에서 residual P0/P1 0을 다시 요구한다.

### 10-7. Delta 최종 독립 비평

- 3차 판정: residual `P0 0 / P1 0`, source-edit gate `PASS`.
- `ACCEPT` resolver의 from/rawTarget 기록, 호출 횟수, 취소 직전 위치 비교가 Irelia Q의 “전진 후 현재점 인근 보정” 계약을 충분히 증명한다.
