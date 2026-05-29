# 02 우클릭 공격 추격 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08 현재: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08: 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. 인게임 수동 검증이 기준이다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

플레이어가 기본 공격 사거리 밖의 적을 우클릭하면, 챔피언이 사거리 안으로 걸어간 뒤 기본 공격을 수행한다. 네트워크/서버 권위 모드에서는 클라이언트가 여전히 intent만 보내고, 서버가 사거리 검증/이동/최종 공격을 소유한다.

Non-goals

- 클라이언트가 데미지를 직접 적용하지 않는다.
- 사거리 밖 기본 공격을 즉시 성공 처리하지 않는다.
- 스킬 타겟팅은 이 slice에서 바꾸지 않는다.
- orbwalking/kiting 자동화는 아직 넣지 않는다.

Why this order

전장 높이를 잡은 뒤 가장 먼저 체감되는 전투 UX 문제다. 사람이 조작하는 이렐리아의 우클릭 공격이 MOBA답게 동작해야 Bot AI의 "접근 후 공격" 흐름도 같은 서버 경로를 공유할 수 있다.

Current-code evidence

- [InGameCombatInputBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp:273)는 네트워크 공격 target이 local 예측 사거리 밖이면 move command를 보낸다.
- [InGameCombatInputBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp:318)는 local 사거리 통과 후에만 `SendBasicAttack`을 보낸다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:1179)는 서버에서 기본 공격이 사거리 밖이면 `out-of-range`로 reject한다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:894)는 move command에서 `MoveTargetComponent`를 생성/갱신한다.
- [MoveTargetComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/MoveTargetComponent.h:8)는 기존 shared 이동 target 저장소다.

Files touched

- 추가: [AttackChaseComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/AttackChaseComponent.h)
- 추가: [AttackChaseSystem.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.h)
- 추가: [AttackChaseSystem.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.cpp)
- 수정: [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp)
- 필요 시 수정: [ICommandExecutor.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h)
- 수정: [GameRoom.cpp](C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp)
- 수정: [InGameCombatInputBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp)

Insertion/replacement anchors

- `CDefaultCommandExecutor::HandleBasicAttack`: `out-of-range` branch를 단순 reject가 아니라 `StartAttackChase(...)`로 전환한다.
- `CDefaultCommandExecutor::HandleMove`: 플레이어의 명시적 지면 이동 command가 들어오면 `AttackChaseComponent`를 clear한다.
- `CDefaultCommandExecutor::HandleCastSkill`: 스킬 시전 시 공격 추격을 clear한다.
- `GameRoom.cpp::Tick`: 서버 sim 순서에서 이동/nav 이후, command 실행 전후 적절한 위치에 `AttackChaseSystem`을 둔다. 더 안전한 방식은 내부 `BasicAttack` command를 생산하게 하는 것이다.

Full new-file contents

```cpp
// Shared/GameSim/Components/AttackChaseComponent.h
#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

struct AttackChaseComponent
{
    EntityID target = NULL_ENTITY;
    u32_t sequenceNum = 0;
    f32_t repathTimer = 0.f;
    bool_t bActive = false;
};

static_assert(std::is_trivially_copyable_v<AttackChaseComponent>);
```

```cpp
// Shared/GameSim/Systems/AttackChaseSystem.h
#pragma once

class CWorld;
struct TickContext;
class CDefaultCommandExecutor;

class CAttackChaseSystem final
{
public:
    void Execute(CWorld& world, const TickContext& tc, CDefaultCommandExecutor& executor);
};
```

Implementation outline

1. 클라이언트는 적 우클릭 시 target intent를 보낸다.
2. 서버 `HandleBasicAttack`은 target/team/dead/cooldown을 먼저 검증한다.
3. 유효하지만 사거리 밖이면 `AttackChaseComponent`와 `MoveTargetComponent`를 붙인다.
4. `AttackChaseSystem`은 target 위치를 따라가다가 사거리 안에 들어오면 같은 기본 공격 실행 경로를 호출한다.
5. 데미지는 서버 range validation 성공 후 `DamageRequest`/GameSim event로만 흐른다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: 이렐리아로 Ashe를 사거리 밖에서 우클릭하면, 이렐리아가 Ashe에게 걸어가고 사거리 안에서 기본 공격한다.
- 예상 debug log를 추가한다면: `[Command] basic-attack chase start`, 이후 `[Command] basic-attack accept`.

Rollback scope

`AttackChaseComponent`/`AttackChaseSystem`을 삭제하고, `out-of-range` reject branch를 기존처럼 복구한다. move/skill command의 chase clear도 제거한다.

Next slice

우클릭 공격 이동이 자연스러워지면 03/08 B 귀환으로 넘어간다.
