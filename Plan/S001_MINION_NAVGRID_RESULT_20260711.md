Session - 미니언 타게팅/공격/역재생 애니메이션과 NavGrid 전투 경계를 구현·검증한 결과를 박제한다.

> SUPERSEDED(2026-07-11): 한 공격에서 forward 전체 clip 뒤 reverse 전체 clip을 재생하는 설계는 이중 공격 모션 회귀를 만들었으므로 S006에서 폐기했다. 현재 계약은 action sequence당 정방향 또는 역방향 traversal 정확히 1회다.

1. 반영한 코드

미니언 target lifecycle은 0.15초 scan, 0.20초 chase path refresh와 deterministic EntityID tie-break를 사용한다. lane은 hard exclusion이 아니라 우선순위로만 사용한다. 공격 windup 중에는 `attackTargetId`를 잠가 기존 대상이 죽어도 같은 action이 새 대상에게 전이되지 않는다. recovery가 끝난 다음에만 새 대상을 scan한다.

공격은 즉시 피해를 주지 않는다. melee는 0.22초, ranged는 0.28초 windup 뒤 target alive/range/NavGrid segment를 재검증한 한 tick에만 impact 또는 projectile을 만든다. 이어 0.22초 recovery를 거친다. 동일 target의 cooldown 구간에는 0.18 range hysteresis를 사용해 경계 떨림을 줄이되, cooldown이 끝난 뒤 base range 밖이면 다시 chase한다.

depenetration은 LaneMove/Chase에만 적용한다. Attack/Idle/Dead 위치와 target-facing yaw를 separation correction이 덮지 않는다. 현재 solver는 기존 sequential pair correction을 유지하므로 완전 대칭 solver는 후속 최적화 항목이다.

클라이언트는 새 animation asset을 만들지 않고 기존 `attack` clip을 코드로 역재생한다.

```text
server ActionStart
-> clipSeconds / replicatedWindupSeconds 속도로 forward 전체 clip 재생
-> replicatedRecoverySeconds 동안 동일 clip reverse 재생
-> 최신 Idle/Run pose 복귀
```

실제 WANIM 길이인 melee 약 0.900초, ranged 약 1.633초를 각각 서버 windup에 맞춰 압축하므로 ranged가 다음 공격 sequence에 계속 덮여 reverse를 못 하던 문제를 제거했다. `Snapshot.fbs`에 `minionAttackWindupSec/minionAttackRecoverySec`를 추가해 Client가 Server 튜닝과 같은 시간을 사용한다. stale BasicAttack actionId는 현재 상태 truth로 사용하지 않고 새 action sequence의 애니메이션 trigger로만 사용한다.

NavGrid 전투 경계는 다음 위치에 적용했다.

```text
minion Attack 진입
minion windup impact
minion ranged projectile step와 근접 hit 직전
champion BasicAttack accept
champion CombatAction impact
AttackChase의 in-range 발사/섭취 판정
```

사거리 안이어도 direct segment가 막히면 미니언과 챔피언은 매 tick 공격을 재명령하지 않고 stop range 0의 Nav path로 clear firing position까지 우회한다. 미니언 projectile은 첫 blocked segment에서 target 없는 hit event를 만들고 제거되며 피해를 주지 않는다.

`GameRoomNav.cpp`에서 authored NavGrid와 WMesh bake가 모두 실패한 경우 더 이상 전체 cell을 walkable로 만들지 않는다. fail-closed grid를 사용해 이동/전투 segment를 모두 막고 명확한 서버 로그를 남긴다. 정상 match는 `Data/Stage1.navgrid`를 사용한다.

```text
[ServerNav] authored navgrid loaded ...
walkable=247348
blocked=14796
hash=69CE27FE
```

2. 검증

`Tools/SimLab` authored NavGrid 프로브 결과:

```text
A=(94.75,0,-67.25)
B=(95.75,0,-67.25)
intermediate blocked cell=(237,121)
B endpoint cell=(238,121)
SegmentWalkable(A,B)==false

[SimLab][NavGrid] PASS: authored cells=247348 blocked=14796 hash=69CE27FE blocked=(237,121) endpoint=(238,121)
```

최종 빌드/실행:

```text
Tools/SimLab Debug x64 -> PASS
SimLab.exe 1800 42 -> PASS, same-seed deterministic
Server Debug x64 -> PASS
Client Debug x64 -> PASS
SharedBoundary -> PASS
git diff --check -> PASS
```

수동 플레이 검증에서는 다음 순서로 캡처한다.

```text
1. melee/ranged가 같은 target을 windup 동안 유지하는지
2. impact가 forward 종료 tick에 한 번만 발생하는지
3. forward -> reverse -> Idle/Run이 매 attack cycle에 보이는지
4. 사거리 경계에서 Attack/Chase pose가 매 tick 떨리지 않는지
5. 얇은 벽 반대편 target에 damage 0, 우회 path 생성인지
6. ranged projectile이 벽 앞에서 제거되고 target HP가 유지되는지
```

현재 남은 구조 개선은 sequential separation을 대칭 pair solver로 바꾸는 작업과 lane tangent/center pull을 실제 nav-cost integration field로 바꾸는 작업이다. 이번 세션의 버그 수정 범위인 target 전이, 즉시 impact, 역재생 취소, stale Attack 상태, 벽 너머 공격과 all-walkable fallback은 반영 완료했다.
