Session - 서버 미니언 facing, 공격 거리, target scan cadence를 서버 권위 파이프라인 기준으로 보정한다.

목표는 `GameRoom`에 뭉쳐 있는 서버 미니언 파이프라인을 단계적으로 분리하면서, 먼저 미니언 이동/시선/타겟팅/공격 사거리 문제를 서버 권위 흐름 안에서 보정하는 것이다.

기본 전제:

- 서버 권위 흐름은 유지한다: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`.
- 미니언의 gameplay truth는 서버 월드에 둔다. 클라이언트 `CMinion_Manager`는 네트워크 권위 모드에서 snapshot visual/animation만 담당한다.
- `GameRoom`은 방, 세션, 네트워크, phase orchestration만 남기는 쪽으로 줄인다.
- 미니언 로직 분리는 동작 변경을 작게 검증할 수 있는 단위로 진행한다.
- 기존 더러운 작업 트리는 되돌리지 않는다. 이 작업에서 만진 파일만 좁게 추적한다.

1. 반영해야 하는 코드

1-0. 세션 진행 순서

| 세션 | 상태 | 목표 | 산출물 |
| --- | --- | --- | --- |
| Session 00 | 완료 | 기준선 문서화 | 서버 권위 미니언 파이프라인과 `GameRoom` 분리 방향 기록 |
| Session 01 | 완료 | 서버 미니언 facing과 공격 사거리 기준 통일 | `GameRoom.cpp` 안에 분리 예정 helper anchor 추가, 이동/공격 yaw와 공격 거리 보정 |
| Session 02 | 완료 | target acquisition을 0.5초 tick 기준으로 정리 | current target validation과 신규 target scan cadence 분리 |
| Session 03 | 다음 진행 | stuck/avoidance와 path movement 분리 | `TryResolveMinionMoveStep(...)` 계열 이동/회피 로직을 서버 미니언 시스템으로 이동 |
| Session 04 | 예정 | wave spawn/waypoint cache를 `GameRoom` 밖으로 이동 | wave timer, waypoint cache, spawn helper를 `ServerMinionSystem`으로 이전 |
| Session 05 | 예정 | debug/Imgui tuning 위치 정리 | client visual-only tuner와 server-authoritative debug readout 분리 |

진행사항:

- 완료: 세션 문서 생성 및 서버 권위 미니언 파이프라인 기준선 기록.
- 완료: `GameRoom.cpp`에 서버 미니언 facing helper, 공격 거리 helper, target validation helper를 추가.
- 완료: 이동 중 yaw를 실제 이동 결과인 `vNext - vPos` 기준으로 갱신.
- 완료: 공격 중 yaw를 `targetPos - selfPos` 기준으로 갱신.
- 완료: 공격 거리를 `attackRange + selfRadius + targetRadius` 기준으로 통일.
- 완료: 신규 target full scan을 매 tick 수행하지 않고 0.5초 cadence와 spawn stagger를 사용하도록 변경.
- 검증 완료: `git diff --check -- Server/Private/Game/GameRoom.cpp .md/TODO/05-19/MINION_GAMEROOM_SPLIT_PIPELINE_SESSIONS.md`.
- 검증 완료: `Server/Include/Server.vcxproj` Debug x64 빌드 성공.
- 남음: 인게임 wave 충돌 상황에서 stuck 감소와 snapshot yaw를 런타임으로 확인.
- 남음: `GameRoom.cpp` 안에 남아 있는 미니언 로직을 `ServerMinionSystem`으로 실제 분리.

다음 세션 진행 내용:

- Session 03: `TryResolveMinionMoveStep(...)`, `IsAvoidanceCandidateClear(...)`, 이동 후보 선택 로직을 서버 미니언 이동 모듈 경계로 묶는다.
- Session 03: blocked frame 누적, repath cooldown, same-team lateral detour가 한 흐름에서 동작하도록 정리한다.
- Session 03: 회피 후보 선택 후 최종 `vNext - vPos`가 계속 authoritative facing source가 되도록 유지한다.
- Session 03 검증: 양쪽 미니언 wave가 일자로 마주칠 때 완전 정지하지 않고, 공격 감지 범위에 들어온 대상과 전투로 전환하는지 확인한다.
- Session 04: `m_nextMinionWaveTick`, `m_minionWaveIndex`, `m_serverMinionWaypoints`를 `GameRoom` 밖으로 옮길 소유 구조를 만든다.
- Session 04: `Phase_ServerMinionWave(...)`와 `Phase_ServerMinionAI(...)`는 `GameRoom`에서 system 호출만 남기도록 줄인다.
- Session 05: authoritative mode에서 클라이언트 ImGui 미니언 튜너가 gameplay truth를 바꾸는 것처럼 보이지 않게 label/debug 범위를 정리한다.

1-1. Session 00 - 기준선 문서화

추가 파일:

```text
C:/Users/user/Desktop/Winters/.md/TODO/05-19/MINION_GAMEROOM_SPLIT_PIPELINE_SESSIONS.md
```

기록할 기준선:

- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp`
  - `Phase_SimulationSystems(...)`에서 `Phase_ServerMinionWave(tc)`와 `Phase_ServerMinionAI(tc)`가 서버 tick의 미니언 진입점이다.
  - `Phase_ServerMinionWave(...)`는 wave spawn scheduling을 담당한다.
  - `Phase_ServerMinionAI(...)`는 death sync, target scan, attack, chase, lane movement, animation state까지 모두 담당한다.
  - `TryMoveServerMinionToward(...)`와 `TryResolveMinionMoveStep(...)`는 path/avoidance movement를 담당하지만 현재 server transform yaw 갱신이 빠져 있다.
  - `FindClosestEnemyCombatTarget(...)`는 target priority/range filter를 담당한다.
- `C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp`
  - 서버 transform yaw와 replicated animation state가 snapshot으로 내려간다.
- `C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp`
  - 클라이언트 미니언 visual은 snapshot position/yaw/animation state를 적용한다.
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp`
  - 네트워크 권위 모드에서는 로컬 미니언 AI/분리/충돌 gameplay system을 등록하지 않는다.

1-2. Session 01 - 서버 미니언 방향/facing과 공격 사거리 기준 통일

목표:

- 이동 중 미니언은 waypoint 방향이 아니라 실제 선택된 이동 step 방향을 바라본다.
- 공격 중 미니언은 target 위치에서 자기 위치를 뺀 벡터를 바라본다.
- 공격 가능 거리는 `attackRange + selfRadius + targetRadius` 기준으로 통일한다.
- 2026-05-19 반영: `GameRoom.cpp`에 세션 01 helper anchor를 추가하고, attack/move branch에서 yaw와 effective range를 우선 보정했다.

주요 변경 후보:

```text
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
```

추가/분리 후보:

```text
C:/Users/user/Desktop/Winters/Server/Public/Game/ServerMinionSystem.h
C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionSystem.cpp
```

구현 방향:

- `ResolveServerMinionAttackRange(world, self, target, state)` helper를 만든다.
- `FaceServerMinionTowardDirection(transform, direction)` helper를 만든다.
- `TryMoveServerMinionToward(...)`는 `vNext - vPos`를 기준으로 yaw를 갱신한다.
- `Phase_ServerMinionAI(...)`의 attack branch는 damage/animation 전에 `targetPos - selfPos`로 yaw를 갱신한다.
- 이 세션에서는 `GameRoom` 전체를 한 번에 뜯지 않고, 새 helper 경계부터 만들어 이후 분리의 anchor로 삼는다.

현재 반영 코드 anchor:

```text
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
- ResolveServerMinionAttackRange(...)
- FaceServerMinionTowardDirection(...)
- FaceServerMinionTowardTarget(...)
- Phase_ServerMinionAI(...)
- CGameRoom::TryMoveServerMinionToward(...)
```

1-3. Session 02 - target acquisition을 0.5초 tick 기준으로 정리

목표:

- target scan은 실시간 매 프레임 전체 검색이 아니라 0.5초 내외의 staggered scan으로 제한한다.
- current target이 이미 있으면 매 tick cheap validation만 수행한다.
- current target이 죽었거나, 사라졌거나, range/ lane 규칙에서 벗어나면 scan 후보로 되돌린다.
- 2026-05-19 반영: 서버 미니언 spawn에서 0.5초 scan interval과 staggered initial cooldown을 설정하고, `Phase_ServerMinionAI(...)`에서 current target validation과 신규 scan cadence를 분리했다.

주요 변경 후보:

```text
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionSystem.cpp
```

구현 방향:

- `MinionStateComponent`에 이미 있는 target scan 관련 필드를 우선 사용한다.
- scan interval 기본값은 0.5초로 둔다.
- wave 전체가 같은 frame에 scan하지 않도록 entity id 또는 spawn order 기반 offset을 유지한다.
- `FindClosestEnemyCombatTarget(...)`의 핵심 규칙은 유지한다: range reject가 priority compare보다 먼저 와야 한다.

현재 반영 코드 anchor:

```text
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
- kServerMinionTargetScanInterval
- kServerMinionTargetScanStaggerBuckets
- TryResolveServerMinionTargetCandidate(...)
- FindClosestEnemyCombatTarget(...)
- Phase_ServerMinionAI(...)
- CGameRoom::SpawnServerMinion(...)
```

1-4. Session 03 - stuck/avoidance와 path movement 분리

목표:

- 일자로 마주 오는 미니언이 서로 밀어내는 gameplay collision에 의존하지 않고 steering/path step으로 회피한다.
- 막힘이 반복되면 deterministic side bias와 repath를 사용한다.
- 이동 path cell/segment 기준 forward와 visual yaw가 일치한다.

주요 변경 후보:

```text
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionSystem.cpp
```

구현 방향:

- `TryResolveMinionMoveStep(...)`를 새 미니언 system 파일로 이동한다.
- 후보 angle 선택 결과를 caller가 알 수 있게 하거나, 최종 `vNext - vPos`만 authoritative facing source로 사용한다.
- same-team blocker는 진행을 완전히 막기보다 lane forward progress와 lateral detour를 우선한다.
- opposite-team blocker는 attack detect range와 함께 전투 상태 전환으로 풀어낸다.

1-5. Session 04 - wave spawn/waypoint cache를 GameRoom 밖으로 이동

목표:

- `GameRoom`에서 `m_serverMinionWaypoints`, wave timer, spawn helper를 분리한다.
- `GameRoom::Phase_ServerMinionWave(...)`는 system 호출만 남긴다.

주요 변경 후보:

```text
C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp
C:/Users/user/Desktop/Winters/Server/Public/Game/ServerMinionSystem.h
C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionSystem.cpp
C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj
C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters
```

구현 방향:

- `CServerMinionSystem` 또는 같은 역할의 server-private module을 만든다.
- `GameRoom`은 world, tick context, room event/snapshot bridge, nav query access만 넘긴다.
- stage waypoint cache와 wave scheduling state는 미니언 system이 소유한다.
- project file에는 새 `.h/.cpp`만 추가한다.

1-6. Session 05 - debug/Imgui tuning 위치 정리

목표:

- 클라이언트 local-only `CMinion_Manager::OnImGui_Tuner()`를 서버 권위 튜닝인 것처럼 보이게 두지 않는다.
- 서버 미니언 튜닝은 우선 debug readout으로 제공하고, gameplay 변경 knob은 명시적으로 서버 설정 경로를 만든 뒤에만 연다.

주요 변경 후보:

```text
C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp
C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp
C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionSystem.cpp
```

구현 방향:

- authoritative mode에서는 local minion tuner label을 visual-only로 명확히 한다.
- 서버 scan interval, attack range, stuck/repath counters는 snapshot/debug event/readout으로 확인하는 방향을 우선한다.
- runtime에서 gameplay truth를 클라이언트 ImGui가 직접 바꾸는 구조는 만들지 않는다.

2. 검증

2-1. 문서/변경 검증

```powershell
git diff --check
```

2-2. 서버 gameplay 빌드

```powershell
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

2-3. 클라이언트 snapshot/visual 빌드

```powershell
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

2-4. 런타임 확인

- 미니언이 lane waypoint를 향해 이동할 때 실제 이동 방향으로 바라본다.
- path avoidance로 옆으로 비켜갈 때도 최종 이동 step 방향으로 yaw가 갱신된다.
- 적 미니언을 공격할 때 `target - self` 방향을 바라본다.
- 양쪽 wave가 만났을 때 미니언들이 서로 완전히 끼지 않고, 감지 범위 안의 적을 0.5초 scan cadence로 잡아 공격한다.
- 공격 가능 거리 판정이 `attackRange + selfRadius + targetRadius`와 일치한다.
- 서버 snapshot을 받은 클라이언트 visual이 별도 local AI 없이 같은 yaw/animation을 보여준다.

2-5. 세션 종료 기준

- 각 세션은 코드 변경, 빌드/런타임 확인, 남은 리스크를 같은 날짜 TODO 문서 또는 후속 세션 문서에 기록하고 닫는다.
- `GameRoom.cpp` 라인 수 감소 또는 minion 관련 responsibility 감소가 관찰 가능해야 한다.
- gameplay 결과가 클라이언트 local system에 새로 의존하면 실패로 간주한다.
