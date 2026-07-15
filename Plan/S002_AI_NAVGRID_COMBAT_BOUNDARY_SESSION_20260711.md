Session - authored NavGrid를 이동뿐 아니라 AI 교전 가능성 판정의 최종 서버 경계로 사용해 벽 너머 타게팅/공격을 차단한다.

1. 반영해야 하는 코드

`Data/Stage1.navgrid`는 현재 Stage1을 덮고 있으며 blocked cell도 존재한다. 정상 champion/minion transform 이동은 `TryBuildMovePath`와 `TryClampMoveSegmentXZ`를 통과하므로 authored grid 로드 성공 시 물리적으로 벽을 통과하기 어렵다. 그러나 미니언과 챔피언 AI의 target selection/impact는 거리만 보고, homing projectile도 terrain을 검사하지 않아 벽 너머 교전은 가능하다.

모든 스킬을 일괄 차단하지 않는다. action별 terrain policy를 둔다.

```cpp
enum class eCombatTerrainPolicy : u8_t
{
    IgnoreTerrain = 0,
    RequireClearSegment = 1,
    StopAtFirstBlock = 2,
    BlinkToReachable = 3,
};
```

1-1. `Server/Private/Game/GameRoomNav.cpp`

authored navgrid 또는 WMesh 로드 실패 시 전체 셀 walkable fallback을 silent 정상 경로로 사용하지 않는다. Debug/server smoke에서는 실패를 명확히 기록하고 match start를 실패시키며, explicit lab flag에서만 fallback을 허용한다.

```text
[ServerNav] authored navgrid loaded path=... walkable=247348 blocked=14796
```

이 로그와 count가 없으면 NavMesh 관련 runtime 검증은 실패로 간주한다.

1-2. `Server/Private/Game/GameRoomUnitAI.cpp`

미니언 target 후보는 거리 계산 전에 `RequireClearSegment`를 통과해야 한다. chase 대상은 direct segment가 막혀도 path가 존재하면 유지할 수 있지만, Attack 진입과 windup impact tick에는 다시 clear segment를 검사한다.

```cpp
const bool_t bClearAttackSegment =
    m_navGrid.SegmentWalkable(
        attackerPosition,
        targetPosition,
        attackerRadius);
```

막혔으면 damage/projectile을 만들지 않고 Chase/repath로 돌아간다.

1-3. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

champion basic attack의 최종 서버 accept와 impact 양쪽에서 `tc.pWalkable->SegmentWalkableXZ`를 검사한다. AI가 잘못된 command를 만들더라도 executor가 벽 너머 공격을 확정하지 않는다.

```cpp
if (tc.pWalkable &&
    !tc.pWalkable->SegmentWalkableXZ(sourcePos, targetPos, sourceRadius))
{
    StartAttackChase(world, tc, cmd, effectiveRange);
    return;
}
```

impact tick에도 target이 벽 뒤로 이동했는지 재검사한다.

1-4. `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

AI 후보 필터에도 같은 policy query를 사용해 실패 command를 반복 발행하지 않게 한다. 다만 이것은 품질 필터이며 최종 truth는 CommandExecutor/CombatActionSystem이다.

1-5. `Server/Private/Game/GameRoomProjectiles.cpp`

미니언 ranged projectile는 `StopAtFirstBlock` 정책으로 이전 위치에서 다음 위치까지 segment를 검사한다. 막힌 tick에 projectile을 제거하고 damage를 만들지 않는다. 벽을 무시하도록 설계된 skill projectile은 `IgnoreTerrain`을 명시한다.

1-6. dash/blink 정책

```text
일반 이동 / 기본 공격 / 미니언 projectile -> RequireClearSegment 또는 StopAtFirstBlock
Viego R, 일반 dash -> 기존 TryClampMoveSegmentXZ + 도착 walkable snap
명시적 wall-cross dash -> IgnoreTerrain을 data/skill 정책으로 선언
teleport/blink -> BlinkToReachable, 목적지를 nearest walkable로 resolve
```

2. 검증

Stage1 deterministic wall fixture:

```text
A = (94.75, 0, -67.25)
B = (95.75, 0, -67.25)
중간 blocked cell = (237, 121), B endpoint cell = (238, 121)
Distance(A,B) = 1.0
SegmentWalkable(A,B) == false
```

필수 assert:

```text
미니언 melee target/impact A->B 거절
미니언 ranged projectile A->B 첫 blocked segment에서 제거, damage 0
champion basic attack A->B accept 또는 impact 거절
AI가 blocked target command를 매 tick 반복하지 않음
일반 move segment는 blocked cell을 통과하지 않음
명시 IgnoreTerrain skill만 기존 동작 유지
authored navgrid load 실패는 일반 match start 실패
```

검증 명령:

```text
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
MSBuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Tools/Bin/Debug/SimLab.exe 1800 42
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
git diff --check
```
