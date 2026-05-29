# 상태 제어 아키텍처

## 목표

Winters의 챔피언 구현은 일회성 챔피언 컴포넌트를 넘어 확장 가능해야 한다.

목표 구조는 다음과 같다.

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

Server GameSim이 게임플레이 진실을 소유한다. Client는 렌더링, 가벼운 예측, FX/UI 표현을 담당할 수 있지만, 지속되는 게임플레이 상태를 결정하면 안 된다.

이 문서는 비에고, 사일러스, 애쉬, 제드, 잭스, 칼리스타, 그리고 이후 추가될 챔피언을 위해 상태 이상, 가시성, 타겟 가능 여부, 스펠북 오버라이드, 챔피언별 기능의 아키텍처 방향을 고정한다.

## Riot식 운영 모델

공개된 Riot 엔지니어링 글을 기준으로 보면 대략 다음과 같은 계층 구조를 추정할 수 있다.

- C++ 엔진 코드는 공통적이고 성능이 중요한 gameplay primitive를 소유한다.
- 스크립트/데이터는 챔피언별 조합과 빠른 반복 작업을 담당한다.
- Buff/status는 gameplay mechanic을 만드는 핵심 도구다.
- 챔피언 스크립트가 같은 저수준 로직을 반복하기 시작하면 재사용 가능한 새 시스템을 만든다.
- 서버 결정성과 리플레이 가능성이 중요하므로 상태는 fixed tick 기반으로, 같은 입력에서 재현 가능해야 한다.

참고:
- Riot Swarm 기술 글: https://www.riotgames.com/en/news/the-tech-behind-swarm
  - C++ 계층과 scripting 계층, gameplay 도구로서의 buff, data-driven wave 구조.
- Riot Future of League's Engine: https://www.riotgames.com/en/news/future-leagues-engine
  - 저수준 query/action 복잡도를 엔진 레벨의 noun/verb로 끌어올리는 방향.
- Riot Hwei spell casting 글: https://technology.riotgames.com/node/135
  - 서버 권위 spell flow, 재사용을 염두에 둔 spellbook override.
- Riot determinism 관련 글:
  - 고정 입력은 고정 결과를 만들어야 하며, divergence tooling/replay validation이 중요하다.

## 핵심 규칙

챔피언 코드는 전역 상태 의미를 직접 구현하면 안 된다.

나쁜 예:

```text
if ViegoSimComponent.bMistActive:
    hide from render
    skip AI
    reject targeting
```

좋은 예:

```text
Viego E -> ApplyStatusEffect(ViegoMist)
StatusEffectSystem -> GameplayState 파생
GameplayStateQuery -> CanBeSeenBy / CanBeTargetedBy / CanMove / CanAttack / CanCast
SnapshotBuilder -> 파생된 generic state 직렬화
Client -> snapshot/event 기준으로만 렌더링과 표현 처리
```

## 계층

```text
Shared/GameSim
  StatusEffectComponent
  GameplayStateComponent
  StatusEffectSystem
  GameplayStateQuery
  SpellbookOverrideComponent
  FormOverrideComponent
  AreaEffectComponent
  Projectile hit effect payload

Server
  Command validation
  Bot AI query 사용
  Turret AI query 사용
  Snapshot/Event 직렬화

Client
  Snapshot 적용
  Local selection filtering
  Render visibility filtering
  Animation/FX/UI 표현
```

현재 repo 제약:

```text
TurretAISystem처럼 서버에서 쓰는 일부 시스템이 아직 Engine 아래에 있다.
1차 구현에서는 의존 방향 때문에 필요하면 component 선언은 Engine/Public ECS header에 둘 수 있다.
그래도 권위 있는 상태 부여/판정 로직은 Shared/GameSim에 둔다.
장기적으로는 full state semantics가 필요한 서버 gameplay system을 Shared/GameSim 쪽으로 옮긴다.
```

## 상태 효과 모델

`StatusEffectComponent`는 현재 활성화된 effect instance 목록을 저장한다.

각 effect instance는 다음 정보를 가진다.

```text
effectId
sourceEntity
stackGroup
stackPolicy
remainingTicks or remainingSec
stateFlags
visibilityFlags
targetingFlags
actionBlockFlags
magnitude values
```

`effectId`는 identity/debug/UI용 key다. `effectId` 자체가 gameplay 의미가 되면 안 된다.

실제 gameplay 의미는 tag/flag에서 온다.

```text
Stunned
Slowed
Disarmed
Invisible
Camouflaged
Revealed
TrueSightRevealed
Untargetable
Unselectable
Invulnerable
CannotMove
CannotAttack
CannotCast
Dashing
Suppressed
```

## 파생 Gameplay State

`GameplayStateComponent`는 active effect들로부터 매 tick 재계산된다.

저장해야 하는 compact derived state 예시는 다음과 같다.

```text
stateFlags
moveSpeedMul
incomingDamageMul
outgoingDamageMul
visibilityMask or visibility flags
targeting flags
```

챔피언 스킬은 이 컴포넌트를 직접 쓰면 안 된다. 항상 `ApplyStatusEffect()`를 호출하고, status system이 결과 상태를 파생해야 한다.

## Query 계층

모든 시스템은 챔피언 컴포넌트나 raw flag를 직접 들여다보지 말고 shared query를 호출해야 한다.

필수 query surface:

```text
HasState(world, entity, flag)
CanMove(world, entity)
CanAttack(world, entity)
CanCast(world, entity)
CanBeSeenBy(world, observer, target)
CanBeTargetedBy(world, observer, target)
CanBeSelectedBy(world, observer, target)
CanReceiveProjectileHit(world, source, target)
GetMoveSpeedMultiplier(world, entity)
```

사용 대상:

```text
CommandExecutor
MoveSystem
AttackChaseSystem
BotLaneAISystem
TurretAISystem
Projectile hit scan
Damage pipeline
SnapshotBuilder
Client GameplayQuery
Client RenderVisibilityFilter
```

## 가시성과 타겟팅

단일 `bInvisible`은 사용하지 않는다.

가시성은 반드시 effect list 기반이어야 한다. 여러 상태가 겹칠 수 있기 때문이다.

```text
ViegoMist -> Invisible or Camouflaged
SennaMist -> team/aura camouflage
Akali-like shroud -> local area concealment
Oracle/TrueSight -> reveal
Damage reveal -> temporary reveal
```

최종 가시성은 query 결과다.

```text
CanBeSeenBy(observer, target)
```

타겟 가능 여부는 가시성과 분리한다.

```text
Invisible: 적에게 보이지 않고 AI 획득이 막히지만, projectile collision 정책에 따라 hit될 수 있다.
Camouflaged: 적이 가까이 있거나 reveal이 있으면 보인다.
Untargetable: 선택, basic attack, targeted spell, targeted projectile이 막힌다.
Invulnerable: 타겟팅은 가능하지만 damage가 무효화되거나 감소한다.
Unselectable: client selection은 막히지만 server scripted effect는 허용될 수 있다.
```

`TargetableTag`는 기본 객체 분류로 유지한다. 임시 상태를 표현하기 위해 이 tag를 add/remove하지 않는다.

## Snapshot과 Client

Server snapshot state flag는 전송 결과물이지 gameplay truth가 아니다.

Server:

```text
StatusEffectComponent -> GameplayStateComponent -> SnapshotStateFlags
```

Client:

```text
SnapshotStateFlags -> ReplicatedStateComponent -> render/selection/UI 표현
```

Client는 적이 진짜 invisible인지 결정하면 안 된다. Client는 서버 상태를 표현만 한다.

## 투사체와 Hit Effect

Projectile component는 effect payload를 들고 있을 수 있어야 한다.

```text
damage
damageType
onHitStatusEffect
onHitStatusFlags
onHitStatusDuration
onHitMagnitude
```

예시:

```text
Ashe W projectile -> damage + Slow
Ashe R projectile -> damage + Stun
Yasuo Tornado -> damage + Airborne/Stun
```

Projectile system은 `CanReceiveProjectileHit()`를 통해 hit 유효성을 판정한다.

## Spellbook과 Form Override

비에고 패시브와 사일러스 R은 status effect만으로 구현하면 안 된다.

두 기능에는 재사용 가능한 override system이 필요하다.

```text
SpellbookOverrideComponent
  slot overrides
  source champion/spell
  expire rule
  restore rule

FormOverrideComponent
  originalChampionId
  overrideChampionId
  copied/borrowed stats policy
  visual skin/model policy
  expire rule
```

챔피언 예시:

```text
Viego passive:
  dead champion -> SoulEntity
  right click/use -> FormOverride + SpellbookOverride for limited duration

Sylas R:
  target champion ult -> SpellbookOverride(R) with stolen spell payload

Jayce/Nidalee/Elise-like future:
  Form/Spellbook swap, not bespoke per champion command code
```

## 챔피언 매핑

```text
Viego E
  ApplyStatusEffect(ViegoMist, Invisible/Camouflage flags)
  Spawn mist area FX/event

Viego Passive
  SoulEntity + FormOverride + SpellbookOverride

Sylas R
  Spell theft payload + SpellbookOverride

Ashe W
  Projectile volley + on-hit Slow

Ashe R
  Global projectile + on-hit Stun

Zed
  Shadow entity + shadow spell mirror/return rules

Jax E
  Defensive status + stun area on release

Kalista R
  Ally Untargetable + ForcedMove/Attach + release throw state
```

## 피해야 할 패턴

피해야 할 것:

```text
SnapshotBuilder checking ViegoSimComponent
Bot AI checking Viego-specific mist bool
Render filter checking champion-specific components
Temporary removal of TargetableTag for status behavior
Client-only status truth
Status logic inside FX systems
One bool per status
effectId-only behavior branching
```

선호하는 것:

```text
ApplyStatusEffect()
GameplayStateQuery
Snapshot from derived generic state
Server event/cue single source for visuals
Data/spec-driven champion behavior where possible
```

## 첫 번째 Vertical Slice

1. 상태 truth를 `Shared/GameSim`으로 옮긴다.
2. `StatusEffectComponent`와 `GameplayStateComponent`를 추가한다.
3. `StatusEffectSystem`과 `GameplayStateQuery`를 추가한다.
4. 기존 `StunComponent`, `SlowComponent`, `DisarmComponent`는 legacy compatibility로 흡수한다.
5. 비에고 E의 `bMistActive`를 `ApplyStatusEffect(ViegoMist)`로 교체한다.
6. Snapshot은 invisible/untargetable/stun/slow/disarm을 generic gameplay state에서 직렬화한다.
7. CommandExecutor, BotLaneAI, TurretAI, Projectile hit scan, MoveSystem, AttackChaseSystem이 query를 사용하게 한다.
8. Client selection/render는 replicated snapshot flag를 사용하게 한다.
9. 이후 애쉬 W slow와 애쉬 R stun을 projectile hit status payload로 구현한다.
10. 이후 비에고 패시브와 사일러스 R을 Spellbook/Form override system으로 구현한다.

## 성공 기준

- 새 stun/slow/invisible/untargetable 챔피언을 추가할 때 AI, 렌더링, 타겟팅, SnapshotBuilder, command validation을 각각 수정하지 않아도 된다.
- 챔피언 코드는 대부분 status/spell/projectile/area/form instruction을 발행하는 형태가 된다.
- Server가 gameplay truth의 유일한 source다.
- Client는 snapshot/event를 받아 표현만 한다.
- 고정 command input으로 replay/determinism 검증이 가능해야 한다.
