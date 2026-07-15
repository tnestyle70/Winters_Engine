Session - skill lockDuration과 이동 정책을 분리하고 서버 승인 기반 입력 큐로 Viego W/Jax E/Garen E 회귀를 제거한다.

1. 반영해야 하는 코드

현재 서버와 클라이언트는 action id만 보고 Q/W/E/R을 모두 이동 불가로 간주한다. 클라이언트는 command 승인 전에 legacy/data lock 시간을 `max()`로 누적하고, 서버는 stolen action도 base champion으로 lock을 계산한다. 성공 기준은 action 시작 시 source champion/slot/stage/policy/end tick을 서버가 박제하고 클라이언트는 승인된 ActionStart/Snapshot만 신뢰하는 것이다.

1-1. `Shared/GameSim/Components/ActionStateComponent.h`

이동 정책 enum과 action 시작 identity를 추가한다.

```cpp
enum class eSkillActionMovePolicy : u8_t
{
    Allow = 0,
    QueueUntilUnlock = 1,
    StationaryChannel = 2,
    ForcedMotion = 3,
};

struct ActionStateComponent
{
    u16_t actionId = static_cast<u16_t>(eActionStateId::None);
    u64_t startTick = 0;
    u64_t lockEndTick = 0;
    u32_t sequence = 0;
    u32_t commandSequence = 0;
    eChampion sourceChampion = eChampion::NONE;
    u8_t sourceSlot = 0;
    u8_t stage = 1;
    eSkillActionMovePolicy movePolicy = eSkillActionMovePolicy::Allow;
    bool_t bHasQueuedMove = false;
    Vec3 queuedMoveTarget{};
    Vec3 queuedMoveDirection{};
};
```

1-2. `Shared/GameSim/Definitions/GameplayDefinitionQuery.h/.cpp`

`ResolveSkillActionLockTicks`는 시간만 반환한다. 별도 `ResolveSkillActionMovePolicy(champion, slot, stage)`를 추가한다.

초기 정책은 다음으로 고정한다.

```text
Viego W stage1 -> Allow
Viego W stage2 -> ForcedMotion
Viego R -> ForcedMotion
Jax E stage1/2 -> Allow
Garen E -> Allow
ViegoConsumeSoul -> StationaryChannel
그 외 일반 cast -> QueueUntilUnlock
```

정책 resolver는 시간을 변경하지 않는다.

1-3. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

서버가 cast를 실제 승인한 뒤에만 `StartCommandActionState`를 호출한다. 이 함수는 resolved source champion/slot/stage와 command sequence를 저장하고, 현재 tick에서 새 stage lock end를 다시 계산해 대입한다.

```cpp
action.startTick = tc.tickIndex;
action.lockEndTick = tc.tickIndex + lockTicks;
action.commandSequence = commandSequence;
action.sourceChampion = sourceChampion;
action.sourceSlot = sourceSlot;
action.stage = sanitizedStage;
action.movePolicy = movePolicy;
```

W2는 이전 `lockEndTick`과 `max()`하지 않는다. 새 action sequence와 새 `startTick/lockEndTick`으로 교체한다.

잠금 중 Move command는 버리지 않고 가장 마지막 target/direction을 `ActionStateComponent`에 덮어쓴다.

```cpp
action.bHasQueuedMove = true;
action.queuedMoveTarget = cmd.groundPos;
action.queuedMoveDirection = cmd.direction;
```

`Allow`는 즉시 일반 Move 처리로 통과하고 나머지 세 정책은 simulation owner가 끝날 때까지 queued intent로 보관한다.

1-4. `Shared/GameSim/Systems/Move/MoveSystem.cpp`

이동 잠금 판정은 더 이상 `StatComponent.championId + actionId`로 definition을 재조회하지 않는다.

```cpp
return action.movePolicy != eSkillActionMovePolicy::Allow &&
    tc.tickIndex < action.lockEndTick;
```

unlock tick에 `bHasQueuedMove`가 있으면 walkable path를 다시 계산해 `MoveTargetComponent`에 적용하고 flag를 지운다. 따라서 lock 중 마지막 우클릭만 실행되며 NavMesh 검증을 우회하지 않는다.

1-5. `Shared/Schemas/Snapshot.fbs`, `Shared/Schemas/Event.fbs`

호환성을 위해 table 끝에 다음 필드를 추가하고 codegen한다.

```fbs
actionSourceChampionId:ubyte;
actionSourceSlot:ubyte;
actionMovePolicy:ubyte;
actionLockEndTick:ulong;
actionCommandSeq:uint;
```

`Server/Private/Game/SnapshotBuilder.cpp`와 `ReplicatedEventSerializer.cpp`가 동일 필드를 채운다. 클라이언트 Event/Snapshot applier는 `ReplicatedActionComponent`에 그대로 복원한다.

1-6. `Client/Private/Scene/Scene_InGameLocalSkills.cpp`

network authority 경로에서는 `ArmNetworkMoveInputLock`을 호출하지 않는다. cast command 전송만 하고, 실제 lock은 ActionStart/Snapshot이 가진 `actionMovePolicy + actionLockEndTick + actionCommandSeq`로 시작한다. 따라서 서버가 cooldown/target/state 때문에 거절한 cast는 client lock을 만들지 않는다.

`IssuePlayerMoveTarget`은 authoritative lock 중에도 Move command를 서버로 보낸다. 이때 local destination/yaw/run prediction은 하지 않고 queue indicator만 표시한다. 서버 unlock snapshot이 도착하면 정상 보간을 재개한다.

1-7. `Client/Private/Scene/Scene_InGameNetwork.cpp`

network action animation의 source champion은 `ChampionComponent.id`가 아니라 `ActionStateComponent.sourceChampion`을 우선한다. base idle/run은 `FormOverride.visualChampion`을 사용한다. lock 종료는 animation duration이 아니라 replicated `lockEndTick`으로 판단한다.

2. 검증

서버 단위/smoke:

```text
Viego W1 policy == Allow
Viego W2 policy == ForcedMotion, W2 lockEndTick은 W1 end와 max하지 않음
lock 중 Move A, Move B 입력 -> B만 저장, unlock 후 NavMesh path B 실행
Jax E1/E2 policy == Allow
Garen E policy == Allow
stolen Jax E sourceChampion == JAX, base champion == VIEGO
cooldown/target 거절 cast -> ActionState.sequence/commandSequence 변화 없음
```

클라이언트/네트워크:

```text
거절된 skill command 뒤 우클릭이 즉시 전송되고 이동 가능
승인된 QueueUntilUnlock action 중 마지막 우클릭이 unlock 직후 실행
W1 이동 가능, W2 forced motion 종료 뒤 queued move 실행
Viego consume -> R -> stolen Jax E -> Garen E 순서에서 lock/animation source 일치
```

공통 검증:

```text
Shared/Schemas/run_codegen.bat
git diff --check
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```
