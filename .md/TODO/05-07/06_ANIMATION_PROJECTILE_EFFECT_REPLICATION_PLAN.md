# Animation / Projectile / Effect 동기화 상세 계획

작성일: 2026-05-07  
대상: 런타임 검증에서 미동기화 확인된 Animation, Projectile, Effect  
결론: **애니메이션은 snapshot state + event, 투사체는 server entity + event, 이펙트는 authoritative trigger event로 동기화한다.**

---

## 1. 현재 런타임 검증 결과

검증 결과:

```text
Animation sync  : 안 됨
Projectile sync : 안 됨
Effect sync     : 안 됨
```

현재 client에서는 스킬 입력 시 `ApplyLocalPrediction`이 애니메이션을 직접 재생하고, castFrame에서 gameplay hook, visual hook, legacy skill hook을 모두 실행한다. 이 구조는 싱글플레이/로컬 테스트에는 빠르지만 네트워크에서는 문제가 된다.

문제:

- 다른 클라이언트가 내 스킬 애니메이션 시작을 authoritative하게 받지 못한다.
- projectile entity가 server netId로 생성되지 않는다.
- effect가 server event에서 시작되지 않는다.
- damage와 hit effect가 client local 판정에 묶여 있다.
- 같은 skill을 두 클라이언트가 보면 castFrame 타이밍이 다를 수 있다.

---

## 2. 최종 동기화 원칙

### 2.1 Animation

```text
Server owns:
  animId
  actionSeq
  animStartTick
  playback rate
  casting / moving / dead state

Client owns:
  실제 ModelRenderer 재생
  animation blending
  local smoothing
  visual-only transition
```

### 2.2 Projectile

```text
Server owns:
  projectile spawn
  projectile netId
  movement
  collision
  block/windwall
  hit/destroy
  damage

Client owns:
  projectile mesh/trail rendering
  interpolation
  impact visual playback
```

### 2.3 Effect

```text
Server owns:
  effect trigger timing for gameplay-significant events

Client owns:
  particle simulation
  billboard animation
  mesh FX rendering
  sound playback
```

---

## 3. Animation Sync 계획

### 3.1 NetAnimationComponent v2

현재 snapshot에는 `animId`, `animPhaseFrame`만 있다. 이것만으로는 one-shot 재시작, 평타 반복, 스킬 cast/recovery를 안정적으로 맞추기 어렵다.

확장:

```cpp
struct NetAnimationComponent
{
    u16_t animId = 0;
    u16_t animPhaseFrame = 0;
    u64_t animStartTick = 0;
    u32_t actionSeq = 0;
    u16_t playbackRateQ8 = 256;
    u16_t flags = 0;
};
```

flags 후보:

```text
Loop
OneShot
Casting
Recovery
Interrupted
Death
HardCC
```

### 3.2 AnimationStartEvent

Snapshot은 늦게 도착할 수 있다. one-shot 애니메이션은 event가 필요하다.

추가 후보:

```fbs
table AnimationStartEvent {
  netId:uint;
  animId:ushort;
  actionSeq:uint;
  startTick:ulong;
  playbackRateQ8:ushort;
  flags:ushort;
}
```

전송:

```text
UDP ReliableOrdered 또는 ReliableUnordered
```

M1에서는 `Event` packet으로 보내고, M2 reliability 이후 채널을 나눈다.

### 3.3 Server animation 결정 지점

```text
Move command accepted
  -> Run / Idle 결정

BasicAttack command accepted
  -> BA windup animation
  -> actionSeq++

CastSkill accepted
  -> skill animId
  -> actionSeq++

DeathSystem
  -> death animId
  -> actionSeq++

CC apply
  -> stun/airborne anim if needed
```

### 3.4 Client animation 적용

Client `SnapshotApplier`는 다음 규칙을 따른다.

```text
if incoming.actionSeq > local.actionSeq:
  PlayAnimation(animId, flags)
  set local.actionSeq
  align phase by serverTick - animStartTick

else if same actionSeq:
  small phase correction only

movement loop:
  run/idle은 smoothing 가능

one-shot:
  중간 snapshot만 보고 강제 재시작하지 않음
```

### 3.5 기존 로컬 애니메이션 처리 변경

현재 `ApplyLocalPrediction`은 입력 즉시 스킬 애니메이션을 재생한다.

목표:

```text
Local player:
  predicted animation start 가능
  단, server reject 시 cancel/fade-out
  server accept event 수신 시 actionSeq에 맞춰 align

Remote player:
  local prediction 없음
  server AnimationStartEvent / snapshot만 사용
```

---

## 4. Projectile Sync 계획

### 4.1 Projectile을 server entity로 만든다

현재 Yasuo/Kalista/Ezreal projectile은 client champion 폴더 중심이다. 서버 권위에서는 `Shared/GameSim` 또는 `Server/Game`에서 돌아야 한다.

공통 component:

```cpp
struct ProjectileComponent
{
    eProjectileKind kind = eProjectileKind::Generic;
    EntityID owner = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    eTeam ownerTeam = eTeam::Neutral;
    Vec3 position{};
    Vec3 direction{};
    f32_t speed = 0.f;
    f32_t radius = 0.5f;
    f32_t maxDistance = 0.f;
    f32_t traveled = 0.f;
    f32_t damage = 0.f;
    u16_t skillId = 0;
    bool_t bHoming = false;
    bool_t bPiercing = false;
};
```

### 4.2 ProjectileSpawn flow

```text
Server Skill castFrame
  -> Spawn projectile entity
  -> issue netId
  -> add TransformComponent
  -> add ProjectileComponent
  -> Event(ProjectileSpawn)
  -> Snapshot includes projectile entity
```

### 4.3 Projectile movement

Server:

```text
ProjectileSystem::Execute(fixedDt)
  -> sorted by netId
  -> move
  -> query hit candidates
  -> check team/targetable/dead
  -> check block rules
  -> on hit: DamageRequest + ProjectileHitEvent
  -> destroy projectile
```

Client:

```text
ProjectileSpawnEvent
  -> create visual entity immediately

Snapshot projectile
  -> bind/correct position

ProjectileHitEvent
  -> impact VFX/SFX
  -> remove visual or play fade
```

### 4.4 ProjectileSnapshot fields

EntitySnapshot v2에 다음이 필요하다.

```text
kind = Projectile
subtype = projectileKind
ownerNet
targetNet
pos
yaw
projectileSpeed
projectileRadius
stateFlags
```

### 4.5 Champion별 이전

우선순위:

```text
1. Tower projectile
2. BasicAttack projectile for ranged champions
3. Ezreal Q/W/R
4. Yasuo Q tornado / wind
5. Kalista spear / Rend stack
6. Irelia R wave / E blade pair
```

이유:

- Tower projectile은 게임 규칙 핵심이고 구현이 이미 Engine에 있다.
- Ezreal/Yasuo/Kalista는 projectile architecture를 검증하기 좋다.
- Irelia E/R은 blade/wave/effect가 섞여 있어 일반 projectile 이후가 안전하다.

---

## 5. Effect Sync 계획

### 5.1 Effect는 snapshot이 아니라 event 중심

Effect는 매 tick snapshot으로 보내면 bandwidth가 터진다. 대부분은 시작 시점만 맞추면 된다.

```text
EffectTriggerEvent
  -> Client visual system creates FX
  -> 이후 lifetime은 client local
```

### 5.2 EffectTriggerEvent 후보

```fbs
enum EffectKind : ushort {
  None = 0,
  SkillCastStart = 1,
  SkillCastFrame = 2,
  ProjectileTrail = 3,
  ProjectileHit = 4,
  DamageHit = 5,
  Death = 6,
  BuffApply = 7,
  BuffExpire = 8,
  TowerShot = 9,
  MinionAttack = 10
}

table EffectTriggerEvent {
  effectId:uint;
  kind:EffectKind;
  sourceNet:uint;
  targetNet:uint;
  attachBone:ushort;
  posX:float;
  posY:float;
  posZ:float;
  dirX:float;
  dirY:float;
  dirZ:float;
  startTick:ulong;
  durationMs:ushort;
  flags:ushort;
}
```

### 5.3 Predicted effect policy

Local player는 입력 직후 cast flash 같은 것을 먼저 보여줄 수 있다.

```text
Predicted effect:
  local only
  temporary predictedActionSeq

Server accept:
  match predictedActionSeq
  keep / align

Server reject:
  cancel or fade out
```

Remote player는 predicted effect가 없다. 서버 event만 본다.

### 5.4 VisualHookRegistry 변경

현재 visual hook은 client skill dispatch에서 바로 호출된다.

목표:

```text
Client input path:
  optional predicted visual hook only

Server event path:
  authoritative visual hook
```

즉 `VisualHookRegistry` 호출 지점을 `EffectEventApplier`로 옮긴다.

---

## 6. Event Channel 설계

### 6.1 M1

M1에서는 UDP reliability가 없으므로 snapshot으로 보정 가능한 것과 event-only를 구분한다.

```text
Snapshot으로 보정 가능한 것:
  position, hp, death, projectile current position, animation current state

Event가 없으면 아쉬운 것:
  impact VFX, cast flash, sound
```

M1에서는 event loss를 감수하되, 중요한 gameplay 결과는 snapshot에 반드시 남긴다.

### 6.2 M2 이후

```text
ReliableOrdered:
  SkillCastAccepted
  AnimationStart
  Death
  GameEnd

ReliableUnordered:
  Damage
  BuffApply
  BuffExpire

Unreliable:
  cosmetic effect
  projectile trail

Snapshot channel:
  latest world state
```

---

## 7. Client 적용기 분리

현재 `CSnapshotApplier`만 있다. 추가로 event applier가 필요하다.

```text
CSnapshotApplier
  Entity state apply

CEventApplier
  Damage/Death/Buff/Projectile/Animation/Effect event apply

CProjectileVisualBinder
  projectile net entity -> renderer/fx 연결

CAnimationSyncApplier
  NetAnimationComponent -> ModelRenderer 재생

CEffectEventApplier
  EffectTriggerEvent -> CFxSystem/CFxMeshSystem/CSound_Manager
```

---

## 8. Server event queue

Server GameRoom에 event queue를 둔다.

```cpp
struct GameplayEvent
{
    eEventKind kind;
    u64_t serverTick;
    NetEntityId sourceNet;
    NetEntityId targetNet;
    // payload union or flatbuffer builder input
};
```

Tick 흐름:

```text
DrainCommands
ExecuteCommands
SimulationSystems
CollectEvents
BroadcastSnapshot
BroadcastEvents
ClearEvents
```

주의:

- Event도 deterministic order로 정렬한다.
- key는 `serverTick`, `kind`, `sourceNet`, `targetNet`, `eventSeq`.

---

## 9. Acceptance Test

### 9.1 Animation

2 clients:

```text
Client A uses Q
Client B sees Q animation start within 1 snapshot/event window
Client B does not play wrong idle/run over skill animation
Repeated BA restarts correctly through actionSeq
Death animation plays once
```

### 9.2 Projectile

```text
Client A fires Ezreal Q
Server spawns projectile netId
Client A and B both see same projectile
Projectile hit is server-decided
HP decreases only after server event/snapshot
Projectile disappears on both clients
```

### 9.3 Effect

```text
SkillCastAccepted event triggers cast VFX on both clients
ProjectileHit event triggers impact VFX on both clients
Damage event triggers hit flash / damage number
Death event triggers death VFX once
Predicted local VFX is corrected on server reject
```

### 9.4 Tower / Minion

```text
Tower target selected on server
Tower projectile visible on both clients
Tower damage HP syncs on both clients
Minion wave spawn appears on both clients
Minion attacks damage server HP
Minion death visible on both clients
```

---

## 10. 구현 순서

```text
A. Snapshot EntityKind 추가
B. EventApplier skeleton 추가
C. AnimationStartEvent 추가
D. ProjectileComponent / ProjectileSystem server 추가
E. Tower projectile를 첫 server projectile로 연결
F. SendCastSkill / SendBasicAttack 추가
G. Server ActiveCast + castFrame 구현
H. DamageQueueSystem 완성
I. EffectTriggerEvent + EffectEventApplier 추가
J. Champion projectile 이전
K. Minion/Tower server authority 연결
```

이 순서로 가면 가장 먼저 "런타임에서 안 보였던 애니/투사체/이펙트 동기화"를 눈으로 확인할 수 있다.
