Session - 미니언 밀림, 타게팅, 공격 타임라인, 정/역재생 애니메이션을 실제 LoL에 가까운 안정된 흐름으로 정리한다.

> SUPERSEDED(2026-07-11): 한 공격에서 forward 전체 clip 뒤 reverse 전체 clip을 재생하는 설계는 이중 공격 모션 회귀를 만들었으므로 S006에서 폐기했다. 현재 계약은 action sequence당 정방향 또는 역방향 traversal 정확히 1회다.

1. 반영해야 하는 코드

현재 서버 미니언은 이동 중 유닛 회피를 제외한 뒤 전역 depenetration에서 공격/대기 중인 미니언까지 순차적으로 밀고, 보정 방향으로 body yaw도 덮어쓴다. 공격은 animation windup 전에 피해/투사체를 즉시 생성한다. 클라이언트에는 같은 attack clip의 reverse playback이 이미 있으나 replicated Run/Idle pose가 다음 tick에 Attack/Recover를 취소한다.

성공 기준은 다음이다.

```text
Target acquire
-> approach/path refresh
-> range hysteresis
-> windup 동안 target facing 고정
-> windup 시점에 정확히 1회 melee impact 또는 ranged projectile
-> 짧은 reverse recovery
-> replicated idle/run 복귀
```

1-1. `Server/Private/Game/ServerMinionTuning.h`

기존 0.50초 target scan과 0.60초 chase path refresh를 줄이고 attack range hysteresis와 windup/recovery를 명시한다.

```cpp
constexpr f32_t kTargetScanIntervalSec = 0.15f;
constexpr f32_t kChasePathRefreshSec = 0.20f;
constexpr f32_t kChaseTargetMoveThreshold = 0.35f;
constexpr f32_t kAttackExitRangePadding = 0.18f;
constexpr f32_t kMeleeAttackWindupSec = 0.22f;
constexpr f32_t kRangedAttackWindupSec = 0.28f;
constexpr f32_t kAttackRecoverySec = 0.22f;
constexpr f32_t kAttackRootCorrectionScale = 0.10f;
```

1-2. `Server/Private/Game/GameRoomUnitAI.cpp` - target lifecycle

dead/invalid target은 다음 0.50초 scan을 기다리지 않고 즉시 해제하고 같은 tick에 재탐색한다. lane id는 영구 배제 조건이 아니라 같은 lane 후보의 우선순위로 사용한다. 공격에 진입한 뒤에는 `attackRange + exitPadding`까지 target을 유지해 경계에서 Chase/Attack이 매 tick 왕복하지 않게 한다.

후보 정렬은 다음 tuple로 고정한다.

```text
1. 현재 자신을 공격 중인 유효 적
2. 같은 lane의 가장 가까운 적 미니언
3. aggro 범위 안 적 챔피언
4. 구조물
5. distanceSq
6. EntityID deterministic tie-break
```

1-3. `Server/Private/Game/GameRoomUnitAI.cpp` - 공격 타임라인

`MinionStateComponent.attackTimer`, `attackWindupSec`, `attackRecoverySec`, `bAttackHitApplied`를 실제 서버 권위 상태로 사용한다.

Attack 진입 시 아래 상태만 시작하고 즉시 damage/projectile을 만들지 않는다.

```cpp
minion.current = MinionStateComponent::Attack;
minion.attackTimer = 0.f;
minion.attackWindupSec = IsRangedMinion(minion)
    ? ServerMinionTuning::kRangedAttackWindupSec
    : ServerMinionTuning::kMeleeAttackWindupSec;
minion.attackRecoverySec = ServerMinionTuning::kAttackRecoverySec;
minion.bAttackHitApplied = false;
StartActionState(world, entity, eActionStateId::BasicAttack, serverTick, 1u);
```

매 tick `attackTimer += dt` 후 windup 경계에서 target alive/range/지형을 다시 검사한다. 성공한 tick에만 melee damage 또는 ranged projectile을 정확히 한 번 생성하고 `bAttackHitApplied = true`로 바꾼다. `windup + recovery`가 끝나면 cooldown을 시작하고 Idle/Chase로 전이한다.

1-4. `Server/Private/Game/GameRoomUnitAI.cpp` - 밀림/회피

전역 depenetration은 마지막 안전장치로만 남긴다.

```text
Attack/Dead -> body position correction 0
Idle -> correction 10%
LaneMove/Chase -> correction 35%
보정 벡터로 target-facing yaw를 덮어쓰지 않음
동일 pair 보정은 양쪽에 반씩 대칭 적용
모든 candidate segment는 NavGrid clamp 후 적용
```

공격 중에는 target direction이 yaw owner이며, avoidance/depenetration은 yaw를 쓰지 않는다. 이동 방향 후보의 고정 각도 점프는 점수 기반 blended steering과 yaw slew로 완화한다.

1-5. `Client/Private/Manager/Minion_Manager.cpp`

새 reverse animation asset은 만들지 않는다. Engine의 `PlayAnimationByNameAdvanced(name, false, true, speed)`를 사용해 같은 attack clip을 끝에서 시작으로 재생한다.

현재 Attack/Recover 중 아래 replicated locomotion preemption을 제거한다.

```cpp
if ((anim.phase == eMinionAttackAnimPhase::Attack ||
        anim.phase == eMinionAttackAnimPhase::Recover) &&
    !bDeathRequested &&
    !bNewerAttackSequence)
{
    // Run/Idle pose snapshot은 authoritative attack animation을 중단하지 않는다.
    return;
}
```

forward animation 종료 뒤 0.22초 reverse recovery를 재생하고, 그 뒤 최신 replicated Run/Idle을 적용한다. Death와 더 최신 attack action sequence만 즉시 중단할 수 있다. Melee/Range가 같은 상태 기계를 사용한다.

1-6. 디버그 관측

기존 debug overlay에 다음 값을 노출한다.

```text
entity / target entity
state (LaneMove/Chase/Attack)
attack phase / timer / windup / recovery / hitApplied
distance / enter range / exit range
path waypoint / correction distance / stuck reason
target switch reason
```

2. 검증

결정론 smoke:

```text
공격 시작 tick T -> T 이전 damage/projectile 0
T + windup -> damage 또는 projectile 정확히 1
recovery 종료까지 target-facing yaw 유지
죽은 target -> 다음 scan interval을 기다리지 않고 즉시 재탐색
range 경계 왕복 -> hysteresis 안에서 target/state 유지
Attack 중 depenetration correction/yaw overwrite 없음
같은 seed 2회 target sequence와 impact tick 동일
```

클라이언트 animation:

```text
Melee: forward attack -> reverse 0.22초 -> Idle/Run
Range: forward attack -> reverse 0.22초 -> Idle/Run
중간 Run/Idle snapshot이 Attack/Recover를 끊지 않음
Death와 newer attack sequence는 즉시 interrupt
```

빌드/런타임:

```text
git diff --check
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
서버 1대 + 클라이언트 2대에서 60초 동일 wave capture
```

수집 지표는 초당 target switch, dead-target idle gap, tick당 correction 거리, stuck/repath 횟수, attack windup 오차다.
