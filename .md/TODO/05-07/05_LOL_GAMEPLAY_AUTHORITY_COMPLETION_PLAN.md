# LoL Gameplay 서버 권위 완성 계획

작성일: 2026-05-07  
대상: Winters LoL 서버-클라 Gameplay 완성  
검증 반영: 런타임에서 Animation / Projectile / Effect 동기화 미완성 확인  
결론: **UDP 이주는 전송 계층 작업이 아니라, 모든 Gameplay 결과를 Server authoritative로 올리는 작업과 함께 진행한다.**

---

## 1. 목표

최종 목표는 실제 LoL과 같은 분리 구조다.

```text
Client
  - 입력 수집
  - 로컬 예측
  - 렌더링 / 사운드 / VFX 재생
  - UI / 카메라 / 조작감

Server
  - 이동 판정
  - 스킬 판정
  - 투사체 생성/이동/충돌
  - HP / Mana / Shield / Buff / Debuff
  - 미니언 wave / AI / 공격
  - 타워 타겟팅 / 투사체 / 데미지
  - 정글몹 / 구조물 / 넥서스 / 게임 종료
  - authoritative snapshot/event 발행
```

클라이언트가 먼저 재생할 수 있는 것은 **예측 가능한 표현**뿐이다. 실제 gameplay 결과는 서버 snapshot/event로 확정한다.

---

## 2. 현재 상태 판정

### 2.1 이미 있는 기반

현재 기반은 꽤 좋다.

```text
Shared/Schemas/Command.fbs
  Move / CastSkill / BasicAttack / LevelSkill 등 command enum 있음

Shared/Schemas/Snapshot.fbs
  EntitySnapshot에 netId, championId, team, hp, mana, pos, yaw,
  animId, cooldown, skillRank, buffMask, statHash 있음

Shared/Schemas/Event.fbs
  Damage / BuffApply / ProjectileSpawn / ProjectileHit / SkillCast 등 event enum 있음

Server/GameRoom
  fixed tick
  PendingCommand drain
  stable_sort
  CommandExecutor
  SnapshotBuilder
  Champion spawn / netId 발급

Shared/GameSim
  MoveSystem
  StatSystem
  BuffSystem
  SkillCooldownSystem
  DeathSystem
  DamagePipeline
```

### 2.2 미완성 또는 미연결

```text
Animation sync
  Snapshot에는 animId/animPhaseFrame이 있으나,
  스킬 애니메이션/평타/투사체/타워/미니언 애니 상태를 서버가 확정하지 않음.

Projectile sync
  Event.fbs에는 ProjectileSpawnEvent가 있으나,
  서버가 projectile entity를 만들고 snapshot/event로 동기화하는 루프가 없음.
  Yasuo/Kalista/Ezreal 등 projectile은 대부분 Client 파일에 있음.

Effect sync
  VisualHook/FxSystem은 Client 로컬 중심.
  서버 authoritative SkillCast/ProjectileHit/Damage event에 의해 VFX가 재생되는 구조가 아님.

Skill authority
  CastSkill command enum은 있으나 Client serializer는 Move만 보냄.
  Server CommandExecutor의 CastSkill은 cooldown 6초 stub.
  BasicAttack은 비어 있음.

Damage authority
  DamagePipeline은 있으나 DamageQueueSystem::Execute가 비어 있음.

Minion authority
  Engine CMinionAISystem은 구현되어 있으나 Server GameRoom tick에 연결되지 않음.
  Client CMinion_Manager가 local spawn/tick/render를 담당 중.

Tower authority
  Engine CTurretAISystem / CTurretProjectileSystem은 구현되어 있으나
  Server GameRoom에 stage structure spawn과 system registration이 연결되지 않음.

Snapshot entity model
  EntitySnapshot이 champion 중심이라 minion/tower/projectile/effect를 타입별로 복원하기 어려움.
```

---

## 3. 서버 권위 원칙

### 3.1 Server가 반드시 소유할 것

```text
Entity identity
  netId, entity kind, owner, team

Movement
  final position, yaw, speed, nav/path result

Combat
  attack target, skill cast accept/reject, hit/miss, cooldown, resource cost

Health
  hp, maxHp, shield, death, respawn

Projectile
  spawn tick, owner, kind, start position, direction, speed,
  current position, hit target, destroy reason

Minion
  wave spawn, lane, waypoint, target, attack windup/hit frame, death

Tower
  activation, target selection, aggro, projectile, damage

Animation state
  gameplay animation id, start tick, phase, one-shot/recovery transition

Effect trigger
  skill accepted, cast frame, projectile spawn, projectile hit,
  damage, death, buff apply/expire
```

### 3.2 Client가 소유해도 되는 것

```text
Camera
Mouse hover
Input buffering
Local predicted animation start
Local predicted VFX start
Screen shake
Sound playback
Particle lifetime after authoritative trigger
UI animation
Debug ImGui
```

### 3.3 Client가 절대 최종 결정하면 안 되는 것

```text
Hit 판정
Damage amount
HP 감소
Death
Cooldown accept
Skill range accept
Projectile collision
Tower aggro
Minion target
Gold/XP
Game end
```

---

## 4. Entity Snapshot 확장

현재 `EntitySnapshot`은 champion 중심이다. LoL식 완성을 위해 `entityKind` 중심 generic snapshot으로 바꾼다.

### 4.1 추가해야 할 enum

```fbs
enum EntityKind : ubyte {
  None = 0,
  Champion = 1,
  Minion = 2,
  Turret = 3,
  Inhibitor = 4,
  Nexus = 5,
  JungleMonster = 6,
  Projectile = 7,
  Ward = 8,
  EffectAnchor = 9
}

enum EntityStateFlags : uint {
  None = 0,
  Dead = 1,
  Untargetable = 2,
  Invisible = 4,
  Casting = 8,
  Moving = 16,
  Stunned = 32,
  Slowed = 64,
  Disarmed = 128
}
```

### 4.2 EntitySnapshot v2 후보

```fbs
table EntitySnapshot {
  netId:uint;
  kind:EntityKind;
  ownerNet:uint;
  team:ubyte;
  subtype:ushort;
  level:ubyte;

  hp:float;
  maxHp:float;
  mana:float;
  maxMana:float;
  shield:float;

  posX:float;
  posY:float;
  posZ:float;
  yaw:float;
  moveSpeed:float;

  animId:ushort;
  animStartTick:ulong;
  animPhaseFrame:ushort;
  animFlags:ushort;

  projectileKind:ushort;
  projectileOwnerNet:uint;
  projectileTargetNet:uint;
  projectileSpeed:float;
  projectileRadius:float;
  projectileMaxDist:float;

  skillCooldowns:[float];
  skillRanks:[ubyte];
  buffMask:uint;
  statHash:uint;
  stateFlags:uint;
}
```

M1에서는 모든 필드를 한 번에 넣지 않아도 된다. 하지만 `kind`, `maxHp`, `stateFlags`, `ownerNet`은 초기에 넣어야 한다.

---

## 5. Command 확장

현재 `Command.fbs`에는 필요한 enum은 대부분 있다. 문제는 client serializer와 server executor가 비어 있다는 점이다.

### 5.1 Client serializer

추가:

```cpp
SendCastSkill(slot, targetNet, groundPos, direction)
SendBasicAttack(targetNet)
SendLevelSkill(slot)
SendRecall()
```

중요:

- `targetEntityId`를 보내면 안 된다.
- Client local entity id는 서버에서 의미가 없다.
- 반드시 `targetNet`으로 보낸다.

### 5.2 Server executor

`CDefaultCommandExecutor`를 stub에서 실제 검증기로 바꾼다.

```text
Move:
  dead/stun/root 검증
  이동 목표 설정

BasicAttack:
  targetNet -> entity lookup
  targetable/team/range/dead 검증
  attack windup state 시작
  animation state = BA
  castFrame tick에 DamageRequest enqueue

CastSkill:
  champion id / slot / rank / cooldown / resource / range 검증
  active cast 생성
  animation state = skill anim
  castFrame tick에 gameplay hook 실행

LevelSkill:
  skill point 검증
  rank 증가
```

---

## 6. Skill Authority Layer

현재 Client에는 세 종류 hook이 섞여 있다.

```text
GameplayHookRegistry
VisualHookRegistry
SkillHookRegistry legacy
```

최종 구조:

```text
Server:
  GameplayHookRegistry만 실행

Client:
  VisualHookRegistry 실행
  legacy SkillHookRegistry는 제거 또는 visual-only로 축소
```

### 6.1 서버 skill 실행 흐름

```text
CommandBatch(CastSkill)
  -> ValidateSkillCast
  -> ActiveCastComponent 생성
  -> NetAnimationComponent = skill anim
  -> Event(SkillCastAccepted)
  -> castFrame tick 도달
  -> GameplayHookRegistry dispatch
  -> DamageRequest / ProjectileSpawn / BuffApply 생성
  -> Event(Damage/ProjectileSpawn/BuffApply)
  -> Snapshot에 HP/cooldown/anim 반영
```

### 6.2 Client skill 흐름

```text
Input Q/W/E/R
  -> CommandBatch(CastSkill) UDP 송신
  -> optional predicted animation/VFX
  -> Server SkillCastAccepted 수신
  -> authoritative animation/VFX align
  -> Snapshot correction
```

---

## 7. Damage / HP / Death Authority

### 7.1 DamageQueueSystem 완성

현재 `DamagePipeline`은 있으나 queue system이 비어 있다. 다음으로 바꾼다.

```text
DamageRequestComponent entity collect sorted
  -> ApplyDamageRequest
  -> DamageEvent 생성
  -> HealthComponent / ChampionComponent / MinionComponent / StructureComponent sync
  -> DeathSystem 후처리
  -> request entity destroy
```

### 7.2 HP 원천 통일

현재 여러 component에 hp가 중복되어 있다.

```text
HealthComponent
ChampionComponent.hp
MinionComponent.hp
StructureComponent.hp
JungleComponent.hp
```

권장:

- 서버 sim의 최종 HP 원천은 `HealthComponent`.
- 나머지는 rendering/UI/legacy compatibility mirror로 유지.
- `DamageQueueSystem`과 `StatSystem`이 mirror를 갱신한다.

### 7.3 Death 이벤트

Death는 snapshot만으로도 표현 가능하지만, VFX/SFX/UI kill feed를 위해 event도 필요하다.

```text
Health <= 0
  -> DeathSystem
  -> stateFlags Dead
  -> NetAnimation Death
  -> Event(Death)
  -> Client death anim/VFX
```

---

## 8. Projectile Authority

투사체는 두 종류로 나눈다.

```text
Gameplay Projectile:
  Ezreal Q/W/R, Yasuo Q tornado, Kalista spear, tower shot 등
  서버 entity로 존재
  충돌/피해 서버 판정

Visual Projectile:
  순수 이펙트 trail, sparkle, afterimage
  서버 event에서 시작하지만 lifecycle은 클라 visual system이 처리 가능
```

### 8.1 Server ProjectileComponent 후보

```cpp
struct ProjectileComponent
{
    eProjectileKind kind;
    EntityID owner;
    EntityID target;
    eTeam ownerTeam;
    Vec3 start;
    Vec3 position;
    Vec3 direction;
    f32_t speed;
    f32_t radius;
    f32_t maxDistance;
    f32_t traveled;
    f32_t damage;
    u16_t skillId;
    bool_t bHoming;
};
```

### 8.2 Server ProjectileSystem

```text
Collect projectile sorted by netId
  -> move by fixed dt
  -> collision query
  -> windwall/block check
  -> hit event
  -> DamageRequest enqueue
  -> destroy projectile
```

### 8.3 Client Projectile sync

M1:

- projectile도 snapshot entity로 보낸다.
- client는 `EntityKind::Projectile`이면 visual projectile renderer를 붙인다.
- server position을 따라간다.

M2:

- `ProjectileSpawnEvent`로 visual을 즉시 시작한다.
- snapshot은 correction/late join 용도로 유지한다.

---

## 9. Minion Authority

현재 client `CMinion_Manager`가 spawn/tick을 한다. 최종적으로는 서버가 wave를 만든다.

### 9.1 Server MinionWaveSystem

```text
GameStart 이후 N초마다 wave spawn
lane top/mid/bot
team blue/red
type melee/ranged/siege/super
netId 발급
Snapshot에 EntityKind::Minion
```

### 9.2 Server MinionAI

Engine `CMinionAISystem`을 서버 tick에 연결한다.

필요 조건:

- Server world에 `MinionComponent`, `MinionStateComponent`, `HealthComponent`, `TransformComponent`, `NavAgentComponent` 존재
- Server nav/path 또는 lane waypoint movement 존재
- Damage application이 server queue로 연결됨

### 9.3 Client Minion_Manager 역할 변경

기존:

```text
spawn + tick + animation + render
```

목표:

```text
server snapshot으로 생성된 minion visual binding
animation playback
render only
```

---

## 10. Tower Authority

Engine에 `CTurretAISystem`, `CTurretProjectileSystem`이 이미 있다. 이것을 서버로 올린다.

### 10.1 Server Structure Spawn

StageData에서 tower/inhib/nexus를 서버 world에 생성한다.

```text
StructureComponent
TurretComponent
TurretAIComponent
HealthComponent
TransformComponent
TargetableTag
NetEntityIdComponent
```

### 10.2 Server Tower Tick

```text
TurretAI
  -> target select
  -> aggro lock
  -> tower projectile spawn

TurretProjectile
  -> homing movement
  -> hit
  -> DamageRequest
```

### 10.3 Client Tower

Client는 tower target/projectile/damage를 결정하지 않는다.

Client는 다음만 한다.

- tower mesh render
- tower attack animation/VFX
- projectile visual
- HP bar
- hit/death effect

---

## 11. Animation Authority

애니메이션은 rendering처럼 보이지만 gameplay와 붙어 있다. LoL에서는 평타/스킬 castFrame, recovery, death, CC 상태가 애니메이션과 연결된다.

따라서 서버는 최소한 **gameplay animation state**를 권위로 내려야 한다.

### 11.1 NetAnimationComponent 확장

현재:

```text
animId
animPhaseFrame
```

확장 후보:

```text
animId
animStartTick
animPhaseFrame
animPlaybackRateQ8
animFlags
actionSeq
```

`actionSeq`가 중요하다. 같은 `animId`가 반복될 때 client가 재시작해야 하는지 판단할 수 있다.

### 11.2 Animation event policy

```text
Movement loop:
  snapshot animId로 동기화

BasicAttack / Skill one-shot:
  Event(AnimationStart or SkillCastAccepted) + snapshot anim state

Death:
  Event(Death) + snapshot stateFlags Dead

CC:
  BuffApply/Stun event + snapshot stateFlags
```

---

## 12. Effect Authority

Effect 자체는 client visual이다. 하지만 **어떤 effect를 언제 시작하는가**는 서버 authoritative event에서 와야 한다.

### 12.1 EffectTriggerEvent 후보

```fbs
table EffectTriggerEvent {
  effectId:uint;
  sourceNet:uint;
  targetNet:uint;
  attachBone:ushort;
  pos:Vec3;
  dir:Vec3;
  startTick:ulong;
  durationMs:ushort;
  flags:ushort;
}
```

### 12.2 Effect 분류

```text
Predicted local effect:
  내 입력 직후 바로 보여도 되는 cast start flash

Authoritative gameplay effect:
  damage hit, projectile hit, CC apply, death, tower shot, skill accepted

Cosmetic-only effect:
  trail, particles, ambient glow
```

Client prediction으로 먼저 튼 effect는 server event가 오면 정렬하거나, reject되면 fade-out한다.

---

## 13. 완성 기준

다음이 모두 되면 LoL식 Gameplay Authority 1차 완료다.

- Client가 Move/CastSkill/BasicAttack을 UDP로 보낸다.
- Server가 모든 command를 검증한다.
- Server가 HP/damage/death/cooldown/buff를 결정한다.
- Server가 projectile spawn/move/hit를 결정한다.
- Server가 minion wave/AI/damage/death를 결정한다.
- Server가 tower target/projectile/damage를 결정한다.
- Server가 animation state와 action event를 내려준다.
- Client는 snapshot/event로 animation/projectile/effect를 재생한다.
- Client local damage write가 gameplay path에서 제거된다.
- TCP는 BanPick/Backend/control에만 남는다.
- UDP는 Gameplay command/snapshot/event에만 쓰인다.
