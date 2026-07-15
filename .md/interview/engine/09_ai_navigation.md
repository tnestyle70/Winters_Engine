# 09. AI · 내비게이션 · 이동

> 면접 대본 겸 지식 베이스. 코드 문법은 `.md/interview/cpp/` 세트가 담당하고, 여기서는 도메인 구조와 의사결정만 다룬다.
> 모든 경로는 repo-relative. 인용한 코드는 실제 파일에서 확인한 것만 남겼다.

---

## ① 한 줄 정의

"Winters의 이동 도메인은 **비트팩 워크그리드(NavGrid) 위의 A\*를 코어로, 도달 불가 클릭을 O(1)로 보정하는 reachability 캐시, 실패 원인을 enum으로 구분하는 경로 API, 서버 권위 미니언 4단 이동 폴백, 그리고 MCTS/BT 2계층 봇 AI**로 구성했고, 설계 원칙은 하나다 — 이동이 실패할 수는 있어도 **조용히(silent) 실패하는 것은 금지**한다."

### 30초 오프닝 (이 챕터를 통째로 요약해야 할 때)

"경로탐색은 512×512 비트팩 그리드 위 octile A\*이고, 성능은 세 겹으로 방어했습니다 — 직선이면 A\* 자체를 건너뛰는 direct-bypass, thread_local generation-stamp로 스크래치 clear 제거, 앞단 Phase의 repath 스로틀. 벽 클릭은 로딩 때 구운 connected-component + 최근접 맵으로 O(1) 보정합니다. 실제 서비스 관점에서 제일 아픈 교훈은 미니언이 빈 경로를 받고 조용히 stuck된 사고였는데, 그 뒤로 경로 실패는 원인 enum으로 분류하고 모든 실패 경로에 상한 있는 계측을 붙이는 걸 규칙으로 만들었습니다. 대전 이동은 서버 권위라 클라 nav는 복제 챔피언에 대해 꺼져 있고, 미니언은 플로우필드→A\*→스티어링→depenetration 4단 폴백, 봇은 5초짜리 MCTS가 매크로 목표를 블랙보드에 쓰고 BT가 매 틱 실행하는 2계층입니다."

---

## ② 구조와 데이터 흐름

```
[오프라인/로딩]
  맵 .wmesh ──▶ MapSurfaceSampler (삼각형 rasterize, 겹침은 SelectBetterSurface)
             ──▶ MapWalkableBaker (후보 → 시드 flood-fill → 반경 침식) ──▶ CNavGrid (base)
  서버 로딩:  base ──BuildInflated──▶ m_pPathNavGrid(0.5) / m_pMinionLaneNavGrid(레인 클리어런스)
             각 그리드에 PrewarmReachabilityCache (컴포넌트 + 최근접 맵 프리컴퓨트)

[런타임 — 서버 권위 (챔피언)]
  클라 우클릭(raw XZ) ──Move 커맨드──▶ CommandExecutor
    ──▶ CWalkabilityAuthority::BuildMovePath
          ├─ 목표 walkable + SegmentWalkable  → Direct 모드: raw 타겟 유지 (waypoint 1개)
          └─ 막힘                             → TryFindNearestReachableGoal → A* → string-pull
    ──▶ MoveTargetComponent (waypoints) ──▶ MoveSystem ──▶ 스냅샷 ──▶ 클라 보간
        (복제 챔피언에 대해 클라 CNavigationSystem은 아예 안 돈다)

[런타임 — 서버 미니언]                     [런타임 — 클라 로컬/봇 전용]
  5상태 머신(Idle/LaneMove/Chase/Attack/Dead)   NavigationThrottleSystem (Phase 2, repath 억제)
  이동 4단 폴백:                                        ▼
   1. 플로우필드 (레인 전역 흐름)               CNavigationSystem (Phase 3, JobSystem 병렬)
   2. stall 누적 → A* + string-pull                     ▼
   3. 부채꼴 스티어링 (0,±35°,±70°,±90°)        Velocity ──▶ MovementSystem ──▶ Transform
   4. depenetration (관통 해소)
  각 단계 실패 = stuck reason 문자열 + 카운터

[봇 의사결정 2계층]
  MCTSSystem (Phase 10, 5초/50회) ──macroGoal──▶ Blackboard ──▶ BT (Phase 8, 매 틱)
                                                              └─▶ AIIntentQueue (결정/실행 분리)
```

- 데이터 소유는 컴포넌트가 갖는다. `Engine/Public/ECS/Components/NavAgentComponent.h`의 주석이 흐름을 박제해 뒀다: CommandExecutor가 `vTarget`+`bHasGoal` 세팅 → NavigationSystem이 A* path를 채우고 waypoint로 Velocity 세팅 → MovementSystem이 Velocity→Transform. 같은 헤더에 "동적 충돌 무시 — 이 그래프는 정적 점유만 본다"는 정책도 명시했다.
- 정책 주입도 컴포넌트다. `NavigationControlComponent`가 스턴(bMovementBlocked → velocity 0), 슬로우(fMoveSpeedMul), chase fallback 허용 여부, reverse facing(레거시 미니언이 의도적으로 이동 방향의 역을 바라보는 관례 — `atan2(-x,-z)`)을 데이터로 든다. 시스템 코드는 정책이 아니라 메커니즘만 갖는다.
- 서버 미니언의 A\* 결과는 `Server/Private/Game/GameRoomInternal.cpp`의 `SmoothServerPathCells`가 후처리한다: anchor에서 경로 끝→가까운 쪽으로 훑으며 직선 walkable(`LineCellsWalkableForRadius`)인 가장 먼 지점을 찾아 사이 waypoint를 버리는 greedy line-of-sight string-pull. grid A\*의 고전적 약점(격자 정렬 계단 경로)을 최소 waypoint 집합으로 보정하는 표준 기법이다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 1) 워크그리드 = 512×512 비트팩 + revision/cacheId 무효화

- **왜**: 맵 전체의 걷기 가능 여부를 매 프레임 수천 번 조회한다. 조회가 지배적이므로 표현은 최대한 작고 캐시 친화적이어야 했다.
- **대안**: navmesh(폴리곤), 셀당 1바이트 그리드, 계층 그리드.
- **선택**: `Engine/Public/Manager/Navigation/NavGrid.h` — 512×512셀, 셀당 0.5 유닛, 셀당 1비트로 `std::vector<uint8_t>`(32KB)에 팩. 그리고 `m_uRevision`(SetWalkable마다 bump)과 `m_uCacheId`(생성 시 부여)를 둬서, 파생 캐시들이 "(그리드 포인터, cacheId, revision)" 3-튜플만 비교하면 자기가 stale인지 O(1)로 안다. 높이는 그리드가 다루지 않고(XZ 평면 변환만) 표면 높이는 MapSurfaceSampler가 별도로 답한다.
- **비용**: 셀 해상도 고정(0.5), 곡면/경사 표현력은 navmesh보다 떨어진다. LoL류 탑다운 맵에서는 충분하다고 판단했다.

### 2) A\* 스크래치 = thread_local + generation-stamp (26만 셀 clear 제거)

- **왜**: gScore/parent/closed 배열이 262,144셀인데, 탐색마다 memset하면 A\* 자체보다 초기화가 비싸진다. 여러 유닛이 같은 프레임에 A\*를 돈다.
- **대안**: 탐색마다 clear, 방문 셀 리스트를 기록해 되돌리기, 해시맵 open/closed.
- **선택**: `Engine/Private/Manager/Navigation/Pathfinder.cpp`의 `FindPathInternal` — 배열을 `thread_local`로 한 번만 잡고, `BeginSearchGeneration`이 세대 번호를 ++, `Touch(idx)`가 "세대가 다를 때만" lazy 초기화한다. 즉 탐색당 실제 방문한 셀만 건드린다. uint32 세대가 0으로 롤오버하면 전체 fill 후 1로 리셋하는 방어도 넣었다. 휴리스틱은 octile(`max + (√2−1)·min`), 대각 이동은 인접 2셀 모두 walkable을 요구해(corner-cutting 방지) 벽 모서리를 뚫고 지나가는 경로를 차단했다.
- **비용**: 스레드마다 스크래치 세트가 하나씩 상주한다(메모리). JobSystem 워커 수가 유한해서 감수 가능한 수준이다.

### 3) "벽 클릭" 해석 = connected-component + 최근접 맵 프리컴퓨트

- **왜**: 유저는 벽/절벽을 수없이 클릭한다. 그때마다 BFS로 근처 walkable 셀을 찾으면 최악 케이스가 프레임을 흔든다.
- **대안**: 클릭마다 링 탐색 BFS, 실패 시 이동 무시(최악의 UX).
- **선택**: `Pathfinder.cpp`의 `BuildReachabilityCache` — (1)전 그리드 flood-fill로 셀마다 connected-component id 부여, (2)모든 walkable 셀을 소스로 하는 multi-source BFS로 "unwalkable 셀 → 가장 가까운 walkable 셀/컴포넌트/거리" 맵을 미리 굽는다. 런타임엔 rawGoal이 start와 같은 컴포넌트면 그대로, 아니면 `TryResolveFromNearestMap`으로 O(1) 조회, 그것도 안 되면 한정 BFS 폴백. 캐시는 thread_local 슬롯에 (grid, cacheId, radius)별로 저장하고 revision 변화 시에만 재빌드 — 그래서 락이 없다. 서버는 로딩 때 `PrewarmReachabilityCache`로 미리 굽는다(`Server/Private/Game/GameRoomNav.cpp`의 `BuildServerPathNavGrid`).
- **비용**: 그리드×반경 조합마다 셀 수만큼의 보조 배열(components/nearestCell/...)이 스레드별로 상주. 메모리를 내고 클릭 레이턴시의 꼬리를 잘랐다.

### 4) 2단 경로: 직선 단축(Direct-bypass) 먼저, A\*는 폴백

- **왜**: MOBA 이동의 대부분은 열린 평지 직선이다. 매번 A\*를 돌리는 건 낭비이고, 더 중요하게는 **격자 waypoint가 유저의 raw 클릭 의도를 꺾는다.**
- **선택**: `Pathfinder.cpp`의 `TryBuildDirectCellPath` — start→goal 선분이 `SegmentWalkable`이면 A\*를 건너뛰고 2점 경로만 반환(`AStar::DirectBypass` 카운터). 서버 권위 경로도 같은 원칙이다: `Server/Private/Game/WalkabilityAuthority.cpp`의 `BuildMovePath`는 목표가 walkable이고 선분이 클리어면 **Direct 모드로 raw 타겟을 그대로 유지**하고(높이만 안전 보정), 막힌 클릭만 `TryFindNearestReachableGoal`→A\*로 넘긴다. `.claude/gotchas.md`(2026-05-20 click movement)에 규칙으로 박제: "직선 walkable 클릭은 raw 타겟 유지, 막힌 클릭만 pathfind. raw 의도와 반대인 첫 waypoint가 초기 yaw를 몰면 안 된다."
- **비용**: SegmentWalkable 선분 검사 비용이 매 이동 명령에 붙지만, A\* 한 번보다 훨씬 싸다.

### 5) 에이전트 반경: per-query 검사 vs 사전 침식(inflate) 그리드 — 서버는 후자

- **왜**: 에이전트가 점이 아니라서 벽에 몸이 끼거나 붙는다.
- **두 전략이 공존한다**: (a) `FindPathForRadius`처럼 쿼리마다 반경 검사(`CellPredicate`/`StepPredicate` 함수포인터로 무반경/반경 경로가 같은 A\* 코드를 공유), (b) `CNavGrid::BuildInflated`로 반경만큼 미리 침식한 별도 그리드.
- **서버의 선택**: `GameRoomNav.cpp` — base에서 `m_pPathNavGrid = BuildInflated(kPathAgentRadius)`, `m_pMinionLaneNavGrid = BuildInflated(kMinionLaneClearanceRadius)` 두 파생 그리드를 만들고 둘 다 Prewarm. 경로가 자연스럽게 벽에서 반경만큼 떨어지고, per-query 반경 비용이 0이 된다.
- **비용**: 그리드 3장(각 32KB)의 메모리와, 침식 정도가 로딩 시점에 고정된다는 것. 반경이 크게 다른 유닛이 늘면 그리드 종수가 늘어나는 구조라, 그때는 (a)로 돌아갈 여지를 함수포인터 구조로 남겨뒀다.

### 6) 경로 실패 = enum으로 원인 분류 (silent fail 금지)

- **왜**: 실제 사고를 겪었다(④ 참조). 빈 vector 하나로 "시작 막힘/목표 막힘/길 없음/경로 파손"이 전부 뭉개지면 디버깅이 불가능하다.
- **선택**: `Engine/Public/Manager/Navigation/Pathfinder.h`의 `ePathFindResult{Success, NullGrid, StartBlocked, GoalBlocked, NoRoute, BrokenPath}`. 헤더 주석 그대로 — "빈 vector 하나로 뭉개지던 4가지 실패 원인을 호출자가 구분할 수 있게 한다". A\* 내부의 각 실패 지점이 `pOutResult`에 원인을 기록하고, 리트레이스 중 parent가 끊기면 `BrokenPath`로 명시한다.
- **비용**: API에 out-param 하나 추가. 사실상 공짜였고, 이걸 처음부터 안 한 게 사고 원인이었다.

### 7) 클라 병렬화: "엔티티별 독점 쓰기"가 락을 대체한다

- **선택**: `Engine/Private/ECS/Systems/NavigationSystem.cpp` — 메인 스레드에서 `bHasGoal` 에이전트 id만 수집하고, 16개(kParallelThreshold) 이상이면 에이전트별로 JobSystem에 Submit 후 `WaitForCounter`. 무락의 근거를 코드 주석으로 박제했다: "각 에이전트는 자기 NavAgent/Transform/Velocity만 수정 → race 없음". `DescribeAccess`로 Write/Read 셋을 스케줄러에 선언해 시스템 간 접근 충돌도 선언적으로 드러냈다. 여기에 Phase 2의 `NavigationThrottleSystem`이 앞단에서 쿨다운+타겟 이동 임계값으로 `bPathDirty`를 되돌려 급속 우클릭의 A\* 폭주를 코얼레스한다(빈 경로는 무조건 수락해 재탐색 보장). Phase 순서(스로틀 2 → nav 3)가 곧 데이터 흐름이다.
- **비용**: "자기 것만 쓴다"는 불변식이 코드 리뷰로 지켜져야 한다. 그래서 접근 선언(DescribeAccess)을 남겨 스케줄러가 검증할 수 있는 형태로 만들었다.

### 8) 서버 권위 전환 후, 복제 챔피언의 클라 nav는 끈다

- **왜**: 클라 `CNavigationSystem`이 SnapshotApply와 SyncFromECS 사이에서 **스냅샷으로 적용된 챔피언 yaw를 덮어쓰는** 사고가 났다.
- **선택**: `.claude/gotchas.md`(2026-05-22)에 규칙으로 박제 — 서버 권위 게임플레이에서 복제 챔피언에 대해 클라 NavAgent/Velocity 이동 시스템을 아예 돌리지 않는다. 이동이 step-like해지면 로컬 nav를 되살리는 게 아니라 **스냅샷 보간/예측을 고친다**. 결과적으로 Engine의 NavigationSystem은 로컬/봇 전용이 됐고, 대전의 미니언/챔피언 이동은 서버(`Server/Private/Game/GameRoomUnitAI.cpp` 등)가 같은 A\*/NavGrid를 소비한다.
- **비용**: 클라의 반응성은 예측/보간 품질에 전적으로 의존하게 된다. 권위 이원화(클라도 나름 움직임)보다 버그 표면이 훨씬 작다고 판단했다.

### 9) 미니언 = 5상태 머신 + 이동 4계층 폴백, 각 층의 역할이 다르다

- **상태 머신**: `MinionStateComponent`의 5상태 — `Idle / LaneMove / Chase / Attack / Dead`. `Server/Private/Game/GameRoomUnitAI.cpp`가 서버 권위로 굴린다. 틱마다 `DeterministicEntityIterator`로 **정렬된 순서**로 순회(결정론 보장)하고, hp≤0이면 최우선으로 Dead 전이 + Dead 포즈 복제, 사거리 내 타겟이 있으면 Attack(원거리 미니언은 투사체 사거리 패딩 별도), 타겟이 사거리 밖이면 Chase로 이동, 타겟이 없으면 레인 목표로 LaneMove, 어느 것도 아니면 Idle. 상태 이름은 `ServerMinionDebugStateName`으로 디버그 출력에 그대로 노출된다.
- **이동 4계층**: (1) **플로우필드**(`TryMoveServerMinionByFlowFields`) — 레인 전역 흐름, 경로 계산 0원. 단 "레인 목표에 실제로 가까워졌는가"를 매 스텝 검사해 진행 없으면 stall로 카운트한다. (2) stall 프레임이 임계(kFlowFieldStallFramesBeforePathFallback)를 넘으면 **A\***(`TryBuildServerMinionMovePath` → `TryFindNearestReachableGoal`(반경 96셀) → `SmoothServerPathCells` string-pull) — 막힘 해소, 단 틱당 `PathBuildBudget`으로 상한을 걸고 Chase/Lane별 재빌드 쿨다운을 둔다. (3) 한 스텝의 실제 전진은 **부채꼴 스티어링**(`TryResolveMinionMoveStep`, kAngles 0/±35°/±70°/±90° 라디안 상수) — 지역 회피. (4) 그래도 막히면 **depenetration**(`TryResolveMinionDepenetrationStep`) — 유닛 간 관통 해소. 각 단계가 실패하면 `BlockedMoveFrames++`와 함께 `OutputServerMinionStuckDebug`가 reason 문자열("toward-resolve-failed", "flow-resolve-failed", "stall")을 남긴다.
- **트레이드오프**: 층이 많아 코드가 길어지지만, 각 층이 "전역 흐름/막힘 해소/지역 회피/관통 해소"라는 단일 책임을 가져서 stuck 재현 시 어느 층이 실패했는지 reason으로 바로 특정된다. 그리고 서버 미니언 AI는 의도적으로 단일 스레드 결정론 틱이다 — 클라 시절의 병렬 2-pass(④ 참조)에서 배운 교훈으로, 권위 시뮬레이션은 재현 가능성이 병렬 이득보다 우선한다고 판단했다.

### 10) 경로 저장 = SoA(pathCellsX/pathCellsY 분리) — 그리고 의문을 주석으로 남기는 습관

- **선택**: `NavAgentComponent`는 경로를 `CNavGrid::Cell` 구조체 배열이 아니라 `pathCellsX`, `pathCellsY` 두 개의 `int32_t` 벡터로 나눠 저장한다(SoA). waypoint 추종 루프는 인덱스 하나로 x/y를 각각 읽으므로 AoS 대비 손해가 없고, 컴포넌트가 POD 벡터 두 개라 복사/직렬화 경계에서 단순하다.
- **정직하게 말할 부분**: 이 소스에는 내가 남긴 의문 주석이 그대로 있다 — "왜 navgrid cell 배열 대신 평면을 사용하는 거지?". 처음 설계 때 SoA 이득을 정량화하지 않고 따라간 흔적이고, 지우는 대신 남겨뒀다. 확신 없는 결정은 주석으로 표시해 두면 나중에 검증 대상 목록이 된다 — 협업에서 "확신 있는 척"보다 낫다고 생각한다.
- **비용**: 두 벡터의 길이 동기화를 코드가 보장해야 한다(채우는 곳이 NavigationSystem 한 곳이라 리스크는 작다).

### 11) 챔피언 yaw = 서버 권위 + "연속 상태" 취급

- **왜**: 챔피언마다 메시의 forward 축이 다르고(+Z 고정 가정 금지), body yaw를 매 틱 정규화하면 급속 우클릭에서 ±π 경계를 재교차하며 몸이 획획 도는 사고가 났다.
- **선택**: 오프셋의 단일 소스는 `Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp`의 `GetDefaultChampionVisualYawOffset` 하나. 방향→yaw 변환은 `ResolveChampionVisualYawFromDirection`, **Transform에 쓸 때는 항상 `ResolveChampionVisualYawNear`/`MakeChampionVisualYawNear`(현재 yaw에 가장 가까운 등가각)** 를 쓰고, `NormalizeChampionVisualYaw`는 wire 값/로그/비교 전용으로 격리했다(`.claude/gotchas.md` 2026-05-20~21 시리즈). CommandExecutor/MoveSystem/AttackChase/각 챔피언 GameSim이 전부 이 헬퍼를 경유한다.
- **비용**: 저장된 yaw가 ±π 밖의 값일 수 있어 비교/전송 시 정규화를 잊으면 안 된다 — 그래서 "쓰기=Near, 비교·wire=Normalize"를 규칙 문장으로 박제했다.

---

## ④ 어려웠던 점과 해결 (war stories)

### 미니언 제자리 stuck — silent empty path 사고 (2026-04-28)

Chase 상태 미니언이 빈 경로를 받고 제자리 걷기 애니로 stuck됐다. 당시 empty vector가 "길 없음"인지 "시작 셀 막힘"인지 구분이 안 됐고, 코드 추론으로 stale 분석을 3번 반복하다가 결국 **프로파일러 카운터 5분이 정답**이었다. 이 사고에서 세 가지가 나왔다: (1) `ePathFindResult` 실패 원인 enum, (2) NavigationSystem의 chase fallback — path.empty()여도 목표까지 `SegmentWalkable`이면 직선 이동하고 `bPathDirty=true`로 다음 틱 재시도(`Nav::DirectFallback` 카운터), (3) "이동 버그는 계측 먼저"라는 팀 규칙. 지금은 `Nav::RepathCalls/PathEmpty/DirectFallback`, `AStar::NodesVisited/NearestMapHit/DirectBypass` 카운터와, 서버 쪽 `OutputServerMinionPathDebug/StuckDebug`(tick/셀/waypoint/보정 방향/reason)가 상시 준비돼 있다.

### 클라 nav가 서버 yaw를 덮어쓴 사고 (2026-05-22)

스냅샷이 적용한 챔피언 yaw를 클라 NavigationSystem이 같은 프레임 안에서 덮어썼다. 증상만 보면 "가끔 몸 방향이 튄다"라 원인 특정이 어려웠는데, 적용 순서(SnapshotApply → 클라 nav → SyncFromECS)를 추적해 잡았다. 해법은 부분 수정이 아니라 **권위 재정의**: 복제 엔티티에 대해 클라 이동 시스템 자체를 끄고, 이후 발생한 step-like 움직임은 보간/예측 쪽에서 해결했다. "증상을 되돌리는 수정(로컬 nav 재활성화)은 금지"까지 gotcha로 박제했다.

### 클릭 의도 vs waypoint의 분리 (2026-05-20)

열린 평지에서 우클릭했는데 첫 waypoint가 격자 정렬 때문에 클릭 방향과 반대로 잡혀 캐릭터가 순간 뒤를 보는 문제. 원인은 "클릭 = 경로탐색 입력"이라는 잘못된 등식이었다. 클릭의 raw XZ는 **의도**이고 waypoint는 **수단**이다. 그래서 Move 커맨드는 클라가 보정하지 않은 raw XZ를 실어 보내고(클라 보정은 표면 높이 안전값만), 서버 `BuildMovePath`가 직선 클리어면 raw 타겟을 그대로 쓰고 막힌 경우에만 pathfind한다. 추가로 pending Move 코얼레싱(빠른 연타 시 오래된 Move를 최신으로 교체)으로 스테일 waypoint 조향 자체를 줄였다.

### 다층 지형의 표면 선택

다리/경사처럼 한 XZ에 삼각형이 여러 장 겹치면 어느 표면의 높이/노멀을 구울지 애매하다. `Engine/Private/Manager/Navigation/MapSurfaceSampler.cpp`의 `SelectBetterSurface`가 규칙으로 해소한다: walkable-like 노멀(≥0.60) 우선 → 둘 다 walkable-like면 높이차 0.75 이내에서는 노멀 큰 쪽, 넘으면 높은 쪽. 이 위에 `Engine/Private/Manager/Navigation/MapWalkableBaker.cpp`가 후보 마킹(높이 밴드/경사/이웃 안정성) → 플레이어블 시드 flood-fill(고립 섬 제거) → 에이전트 반경 침식(connected가 1000셀 이상일 때) 순서로 NavGrid를 굽고, 단계별 통계를 1회 로그로 남긴다.

### 클라 시절 미니언 AI의 worker race — 2-pass Decision/Apply

서버 이관 전, Engine의 MinionAISystem이 JobSystem 워커에서 돌 때 프로파일러 thread_local 슬롯과 상태 쓰기가 race를 일으켰다. 해법은 **Decision/Apply 2-pass**: 워커는 읽기만 하며 `MinionDecision`/`DamageEvent`를 워커 슬롯별 버퍼(`m_vecDecisionsPerSlot`)에 쌓고, Apply 패스가 메인에서 일괄 반영한다(main=slot 0, worker=idx+1). 이 시스템 자체는 서버 권위 이관으로 메인 레포에서 제거됐지만 — 현재 서버 미니언 AI는 `DeterministicEntityIterator`로 정렬 순회하는 단일 스레드 결정론 틱이다 — "병렬 구간에서는 결정과 반영을 분리한다"는 패턴은 NavigationSystem의 '독점 쓰기' 규칙, BT의 인텐트 큐로 이어졌다.

### 실패 진단은 bounded, dead diagnostics는 삭제 (2026-07-09 감사)

실패 경로 트레이스는 전부 static 카운터 상한을 갖는다 — 예: `Engine/Private/Manager/Navigation/NavGrid.cpp`의 SegmentWalkable 차단 로그 64회 캡(차단 셀/반경/from/to/origin을 캡 내에서 기록), 서버 flow fallback 64회, 미니언 attack 128회, AttackChase 시작 로그 512회. 반대로 "한 번도 emit되지 않는 sprintf_s 진단(dead diagnostics)은 없느니만 못하다"는 감사 결론에 따라 emit 없는 포맷팅은 삭제 대상이고, 리팩터 중 routine 트레이스 추가도 금지다. 요점은 두 문장이다 — **실패는 반드시 흔적을 남긴다. 단, 흔적이 런타임을 스팸하지 않는다.**

### 디버그 파이프라인 자체를 규칙으로 만든 것

이동 버그에서 코드 추론만 누적하는 실패를 반복한 뒤, CLAUDE.md에 디버깅 순서 자체를 박제했다: 증상 튜닝 전에 (1) 디버그 UI/오버레이(클라 AIDebugPanel/DebugDrawSystem), (2) bounded OutputDebugString 트레이스, (3) 권위 코드 경로 주변의 시각 캡처를 먼저 붙이고, 이동/경로 버그는 현재 셀·다음 waypoint·보정 방향·stuck/resolve reason을 반드시 노출한다. 서버 쪽 `OutputServerMinionPathDebug`가 tick/entity/posCell/candidateCell/desiredDir/selectedDir/blocked 플래그를 한 줄로 찍는 것이 이 규칙의 구현체다. "면접에서 디버깅을 어떻게 하냐"고 물으면 도구 이름이 아니라 이 순서를 답한다.

---

## ⑤ 향후 개선 방향

1. **RL 정책 연결**: `Engine/Private/AI/RLBridge.cpp`는 의도적 스캐폴드다 — `EncodeState`(위치/280 정규화 + hp비율 + 쿨다운 4 + 마나비율)와 `BestAction`(argmax)은 구현돼 있고, `LoadModel`은 항상 false를 반환한다. 액션 공간을 MCTS의 `eMCTSAction`과 공유해서, ONNX 런타임만 붙이면 MCTS 자리에 학습된 정책을 끼울 수 있게 인터페이스를 먼저 고정했다.
2. **MCTS 상태 전이 정밀화**: 현재 `ApplyAction`은 평타 45/스킬 70 고정 데미지, Retreat는 hp+30 같은 손코딩 모델이고 쿨다운/지형을 시뮬하지 않는다. Shared/GameSim이 결정론 시뮬이므로, 장기적으로는 추상 모델 대신 실제 sim의 경량 롤아웃으로 교체할 수 있다.
3. **팀 단위 협조**: `BTContext`에 `pTeamBB` 슬롯과 TeamBlackboardComponent를 이원화해 뒀지만 현재 BT는 개체 블랙보드만 채운다 — 갱/오브젝트 합류 같은 팀 조율의 확장점.
4. **경로 품질**: grid A\* + string-pull은 코너에서 반경 여유가 침식 그리드에 고정된다. 유닛 반경 스펙트럼이 넓어지면 funnel 알고리즘 또는 부분 navmesh 도입을 검토.
5. **동적 장애물**: NavAgent 주석에 박제한 "정적 점유만 본다" 정책의 반대급부로, 유닛 밀집은 스티어링/depenetration이 전담한다. 대규모 한타에서는 국소 유동장(local avoidance field)류가 다음 단계다.
6. **반경 전략 통일**: 지금은 per-query 반경(FindPathForRadius)과 사전 inflate 그리드가 공존한다. 유닛 반경 종류가 고정이면 inflate 쪽으로 완전히 수렴시키고 per-query 경로를 제거해 코드 표면을 줄이는 것이 맞고, 반대로 가변 크기 유닛(예: 성장형 몬스터)이 생기면 per-query 쪽을 남겨야 한다 — 콘텐츠 방향이 정해지는 시점에 한쪽을 정리할 계획이다.

---

## 핵심 수치 카드 (면접 직전 리마인드)

| 항목 | 값 | 근거 파일 |
|---|---|---|
| NavGrid | 512×512셀, 셀 0.5유닛, 1bit/셀 = 32KB | `Engine/Public/Manager/Navigation/NavGrid.h` |
| A\* | 8방향, octile 휴리스틱, 대각은 인접 2셀 walkable 요구 | `Engine/Private/Manager/Navigation/Pathfinder.cpp` |
| 스크래치 | thread_local 262,144셀, generation-stamp lazy clear | 위와 동일 |
| 도달 보정 | component flood-fill + multi-source BFS 최근접 맵, 조회 O(1) | 위와 동일 |
| 클라 병렬 임계 | 에이전트 16개 이상이면 JobSystem 분배 | `Engine/Private/ECS/Systems/NavigationSystem.cpp` |
| 서버 그리드 | base + inflate 2장(path 0.5 / lane 클리어런스), 로딩 Prewarm | `Server/Private/Game/GameRoomNav.cpp` |
| 목표 보정 반경 | 최근접 도달가능 셀 탐색 maxRadius 96셀 | `Server/Private/Game/WalkabilityAuthority.cpp` |
| 미니언 | 5상태, 이동 4단 폴백, 스티어링 0/±35°/±70°/±90° | `Server/Private/Game/GameRoomUnitAI.cpp` |
| MCTS | 5초 간격, 50 iteration, rollout depth 5, UCB1 c=√2 | `Engine/Public/ECS/Systems/MCTSSystem.h`, `Engine/Public/AI/MCTSPlanner.h` |
| MCTS 대상 | difficulty ≥ 2 봇만, 결과는 blackboard `macroGoal`에 기록만 | `Engine/Private/ECS/Systems/MCTSSystem.cpp` |
| RL 스텁 | 상태 24차원 인코딩만 구현, LoadModel 항상 false | `Engine/Private/AI/RLBridge.cpp` |
| 실패 로그 캡 | SegmentBlocked 64 / DirectFallback 80 / MinionAttack 128 등 | `Engine/Private/Manager/Navigation/NavGrid.cpp` 외 |

---

## ⑥ 면접 Q&A

**Q1. 맵 워크그리드를 어떻게 표현했고 왜 navmesh를 안 썼나?**
- 골격: 512×512, 셀당 0.5유닛, 1비트/셀 = 32KB 비트팩(`NavGrid.h`). 조회가 지배적인 워크로드라 캐시 지역성 우선. 높이는 그리드 밖(MapSurfaceSampler)으로 분리. 무효화는 revision/cacheId 3-튜플 비교로 O(1).
- 꼬리질문 대비: "navmesh 대비 손해는?" → 해상도 고정과 곡선 경로 품질. 탑다운 MOBA + string-pull 후처리로 체감 차를 줄였고, 표현 교체가 필요해지면 Pathfinder가 grid 인터페이스 뒤에 있어 국소적이다.

**Q2. 여러 유닛이 매 프레임 A\*를 도는데 성능은?**
- 골격: 3중 방어. (1) 탐색 전 direct-bypass로 대부분의 열린 이동은 A\* 자체를 스킵, (2) thread_local generation-stamp로 26만 셀 clear 비용 제거, (3) 앞단 Phase의 스로틀 시스템이 쿨다운+이동 임계로 repath를 코얼레스, 서버 미니언은 틱당 PathBuildBudget 상한.
- 꼬리질문: "generation 롤오버는?" → uint32가 0으로 돌아오면 전체 fill 후 1로 리셋하는 방어 코드가 있다.

**Q3. 벽이나 도달 불가 지점을 클릭하면?**
- 골격: 로딩 때 connected-component 분할 + 모든 walkable 셀 소스의 multi-source BFS로 "최근접 도달가능 셀" 맵을 프리컴퓨트. 클릭 시 같은 컴포넌트면 그대로, 아니면 O(1) 조회로 목표 보정. 캐시는 thread_local 슬롯이라 락이 없고 revision으로 무효화.
- 꼬리질문: "매번 BFS와의 트레이드오프는?" → 메모리(그리드·반경별 보조 배열)를 내고 최악 레이턴시를 산 것. 프레임 예산 관점에서 꼬리 지연 제거가 더 중요했다.

**Q4. 경로 실패 처리를 어떻게 설계했나?**
- 골격: 실제 사고에서 출발한다 — Chase 미니언이 빈 경로를 받고 제자리 stuck. 원인 구분이 안 되는 bool/empty 반환이 문제였다. `ePathFindResult` 6종(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath)으로 원인을 호출자에 넘기고, empty 시 chase fallback(직선 클리어면 직접 이동+재시도), 그래도 안 되면 목표를 명시적으로 정리. 실패 트레이스는 전부 상한 카운터로 bounded emit.
- 꼬리질문: "그걸 어떻게 찾았나?" → 코드 추론 3회 실패 후 프로파일러 카운터로 5분 만에 특정. 이후 '이동 버그는 계측 먼저'가 팀 규칙이 됐다.

**Q5. 미니언은 어떻게 움직이나?**
- 골격: 서버 권위 5상태 머신(Idle/LaneMove/Chase/Attack/Dead) + 이동 4단 폴백. 플로우필드(전역, 계산 0원) → stall 감지 시 A\*+string-pull(막힘 해소, 예산 상한) → 부채꼴 스티어링 ±90°(지역 회피) → depenetration(관통 해소). 각 층 실패가 reason 문자열로 계측된다.
- 꼬리질문: "왜 층을 나눴나?" → 단일 책임. stuck 재현 시 어느 층이 실패했는지 로그만으로 특정된다.

**Q6. 클라와 서버가 둘 다 경로탐색하면 충돌하지 않나?**
- 골격: 실제로 충돌했다 — 클라 nav가 스냅샷 적용 yaw를 덮어쓴 사고. 결론은 권위 재정의: 복제 챔피언에 대해 클라 이동 시스템을 끄고 스냅샷 보간/예측으로 대체. 클라의 Engine NavigationSystem은 로컬/봇 전용으로 남고, 서버가 같은 A\*/NavGrid 코드를 소비한다.
- 꼬리질문: "반응성 손해는?" → 예측 yaw 보호 규칙(Hello local net id 기억, 서버 yaw가 따라잡을 때까지 로컬 클릭 표현 유지)으로 체감을 방어했다.

**Q7. 클릭 이동에서 '의도'와 '경로'를 왜 구분하나?**
- 골격: raw 클릭 XZ는 의도, waypoint는 수단. 열린 라인에서 격자 waypoint가 클릭 방향과 반대로 잡혀 초기 yaw를 꺾는 사고가 있었다. Move 커맨드는 raw XZ를 유지하고(높이만 안전 보정), 서버 `BuildMovePath`가 직선 클리어면 Direct 모드로 raw 타겟 유지, 막힌 클릭만 pathfind. 연타는 pending Move 코얼레싱.
- 꼬리질문: "챔피언 방향 일관성은?" → 오프셋 단일 함수(GetDefaultChampionVisualYawOffset), Transform 쓰기는 항상 Near 계열(±π 경계 재교차 방지), 정규화는 wire/로그 전용 — 이 세 문장이 규칙의 전부다.

**Q8. MCTS를 실제로 어디에 썼고 한계는?**
- 골격: 봇의 매크로 의사결정. `CMCTSPlanner::Plan`이 반경 30 스냅샷 위에서 UCB1(탐험 상수 √2≈1.4142) 4단계(Select/Expand/Rollout depth 5/Backprop)를 50회 돌고, Phase 10 시스템이 5초마다 결과를 블랙보드 `macroGoal`에 쓴다 — 직접 실행하지 않는다. 실행은 Phase 8의 BT가 매 틱 블랙보드를 읽어 수행하고, BT 액션도 직접 움직이지 않고 AIIntent 큐에 Push만 한다. "느린 계획 / 빠른 실행 / 실행은 큐 소비"의 3단 분리.
- 한계를 먼저 말한다: 상태 전이가 추상 모델(고정 데미지 45/70, Retreat hp+30)이라 전술 정밀도가 없다. 의도된 트레이드오프 — 매크로(교전/후퇴/압박)만 노린 경량 planner이고, 정밀화 경로(결정론 sim 롤아웃, RL 교체)를 인터페이스로 남겼다.

**Q9. 멀티스레드에서 nav 관련 공유 상태 동기화는?**
- 골격: 락 대신 소유권. (1) A\* 스크래치와 reachability 캐시는 thread_local — 워커마다 자기 것, (2) NavigationSystem 병렬화는 "에이전트별 컴포넌트 독점 쓰기" 불변식 + DescribeAccess 선언, (3) 시스템 간 순서는 Phase 정수로 강제(스로틀 2 → nav 3), (4) 과거 클라 미니언 AI는 Decision/Apply 2-pass로 쓰기를 메인에 모았다. 현재 서버 권위 미니언 틱은 반대로 단일 스레드 결정론을 택했다 — 권위 시뮬은 재현 가능성이 우선이라서다.
- 꼬리질문: "thread_local 캐시의 단점은?" → 스레드 수 × 캐시 크기의 메모리 중복, 그리고 스레드마다 첫 쿼리 때 빌드 비용. 서버는 Prewarm으로 로딩 시점에 지불한다.

**Q10. NavGrid는 어디서 나오나? 수작업인가?**
- 골격: 오프라인 bake다. `MapSurfaceSampler`가 맵 .wmesh 삼각형을 barycentric rasterize해 셀당 높이/노멀을 굽고(겹치는 표면은 SelectBetterSurface 규칙), `MapWalkableBaker`가 4단계로 굽는다 — (1) 높이 밴드·경사(minNormalY)·이웃 안정성(HasStableNeighbors) 통과 셀을 후보로, (2) 플레이어블 시드에서 8방향 flood-fill로 고립 섬 제거, (3) connected 1000셀 이상이면 에이전트 반경 침식, (4) 최종 write + 단계별 통계 로그. 서버는 여기에 구조물(타워/억제기 등)을 원형으로 carve하고 파생 그리드 2장을 inflate한다.
- 꼬리질문: "왜 시드 flood-fill이 필요한가?" → 높이/경사 조건만 통과한 셀 중에는 지형 밖 지붕 같은 '조건상 걷기 가능하지만 도달 불가능한 섬'이 생긴다. 플레이 가능 시드에서 연결된 것만 남겨야 reachability가 보장된다.

### 보너스 — 시간이 남으면 꺼낼 두 가지

**BT에서 결정과 실행을 왜 분리했나?**
- BT(`Engine/Public/AI/BehaviorTree.h`)는 Selector/Sequence/Parallel/데코레이터 + std::function 람다 리프로 조립하고(HpBelow 0.3 → Retreat / 교전 / lanePush 우선순위), 액션 노드는 `PushIntent`로 AIIntent(Move/Attack/CastSkill)를 큐에 넣기만 한다. 실행 시스템이 큐를 소비하므로 BT 틱 타이밍과 이동/전투 실행 타이밍이 분리되고, 블랙보드는 `std::variant<bool_t,i32_t,f32_t,Vec3,std::string,uint64_t>` 타입세이프 KV라 계층 간 계약이 명시적이다(타입 불일치 시 fallback 반환). 비용은 1프레임의 실행 지연 — 대신 BT를 어느 스레드/어느 빈도로 돌려도 실행 쪽 불변식이 깨지지 않는다.

**이동 버그 디버깅 프로세스는?**
- 사고 경험으로 순서가 정해졌다 — stale 코드 추론 3회 실패 후 프로파일러 카운터가 5분 만에 원인을 특정한 사건. 이후 규칙: 오버레이/카운터/bounded 트레이스를 먼저 붙이고, 이동 버그는 현재 셀·다음 waypoint·보정 방향·stuck reason을 노출한 뒤에야 코드를 고친다. Engine엔 `Nav::*`/`AStar::*` 프로파일러 카운터, 서버엔 reason 문자열이 붙는 stuck/path 디버그 출력이 이미 상비돼 있다. 로그 스팸은 실패 경로 한정 + static 카운터 상한으로 막는다.

---

## ⑦ 다른 챕터와의 연결

- **ECS/시스템 스케줄링**: Phase 정수 = 데이터 흐름(스로틀 2 → nav 3, BT 8 → MCTS 10), DescribeAccess 접근 선언 — ECS 챕터의 스케줄러 설계와 같은 원칙의 응용이다. `.md/interview/cpp/11_architecture_ecs.md` 참조.
- **네트워크/서버 권위**: 복제 챔피언 클라 nav OFF, raw 클릭 의도 보존, 스냅샷 보간과 yaw 예측 보호 — 서버 권위 이동의 전제가 네트워크 챕터의 커맨드/스냅샷 파이프라인이다. `.md/interview/cpp/12_network_serialization.md` 참조.
- **동시성**: thread_local 스크래치/캐시, generation-stamp, Decision/Apply 2-pass, JobSystem Submit/WaitForCounter — 동시성 챕터(`09_concurrency.md`)의 도구들이 이 도메인에서 실제로 쓰인 사례다.
- **에러 처리 정책**: silent fail 금지, 실패 원인 enum, bounded 진단, dead diagnostics 삭제 — `10_error_handling.md` 및 `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`의 도메인 적용판.
- **자산 파이프라인**: NavGrid는 맵 .wmesh → SurfaceSampler → WalkableBaker 오프라인 bake의 산출물이고, 서버/클라가 같은 그리드 코드를 공유한다 — 데이터 파이프라인 챕터와 연결된다.
- **Shared/GameSim 경계**: 챔피언 이동/yaw의 권위 로직(CommandExecutor/MoveSystem/ChampionRuntimeDefaults)은 Shared에 있어 서버·SimLab·스모크가 같은 코드를 돌린다. Shared가 Engine ECS 헤더를 직접 include하지 않고 어댑터를 경유하는 의존성 경계 규칙은 아키텍처 챕터(의존성 맵)에서 다룬다.
- **프로파일러/툴링**: `WINTERS_PROFILE_SCOPE`/`WINTERS_PROFILE_COUNT`가 이 도메인 계측의 기반이고, "stale 분석 3회보다 카운터 5분" 교훈의 실행 수단이다 — 툴링 챕터와 연결된다.
