# PvP, Co-op, Raid Network And Server Authoritative Simulation

## 목표

LoL에서 UDP 기반 네트워크와 서버 권위 시뮬레이션을 먼저 안정화한다. 그 후 EldenRing 클라이언트는 같은 네트워크 기반을 액션 RPG에 맞게 확장한다.

```
LoL UDP stabilization
  -> common transport/session/snapshot infra
  -> Elden PvP
  -> Elden co-op field
  -> Elden multi-party boss raid
```

## 프로토콜 역할

| 프로토콜 | 용도 |
|---|---|
| TCP | 로그인, 인증, 로비, 파티, 매칭, 채팅, 인벤토리, 보상 |
| UDP | 입력, 이동, 회전, 공격, 회피, snapshot, boss/raid state |
| Reliable UDP/KCP 후보 | 중요한 gameplay event, 재전송 필요한 action ack |

## 서버 권위 원칙

클라이언트는 요청한다. 서버가 판정한다.

```
Client:
  InputCommand(seq, buttons, move, camera)
  local prediction

Server:
  validate input
  simulate movement/action/hit/boss
  broadcast Snapshot

Client:
  reconcile own player
  interpolate others
  play visual-only FX
```

## 룸 타입

```cpp
enum class eEldenRoomType : uint8_t
{
    Duel1v1,
    Duel2v2,
    CoopField,
    RaidBoss
};
```

| Room | 인원 | Tick | 특징 |
|---|---:|---:|---|
| Duel1v1 | 2 | 30~60 TPS | 정밀 PvP 판정 |
| Duel2v2 | 4 | 30~60 TPS | lock-on/target sync |
| CoopField | 2~4 | 20~30 TPS | field AI + exploration |
| RaidBoss | 4~8+ | 20~30 TPS | boss phase authority, telegraph sync |

## 서버 구조

```
Server/
├── Network/
│   ├── IOCPCore
│   ├── TcpSession
│   ├── UdpPeer
│   └── ReliableUdpChannel
├── Rooms/
│   ├── CEldenRoom
│   ├── CDuelRoom
│   ├── CCoopFieldRoom
│   └── CRaidBossRoom
├── Sim/
│   ├── CEldenServerWorld
│   ├── CCharacterSimSystem
│   ├── CActionSimSystem
│   ├── CHitValidationSystem
│   ├── CBossSimSystem
│   └── CReplicationSystem
└── Interest/
    ├── CAOIGrid
    ├── CWorldCellInterest
    └── CRaidInterestSet
```

## Tick 모델

LoL과 달리 Elden은 액션 판정이 더 촘촘하다.

권장:

| 모드 | 서버 Tick |
|---|---:|
| Field co-op | 20 또는 30 TPS |
| Raid | 30 TPS |
| PvP duel | 60 TPS 후보 |

초기 구현은 30 TPS로 통일한다.

```
Tick N:
  1. Drain network input queue
  2. Validate sequence/time
  3. Simulate actions
  4. Simulate movement
  5. Evaluate hitbox timeline
  6. Simulate boss AI/phase
  7. Build snapshots
  8. Send to interest set
```

## InputCommand

```cpp
struct EldenInputCommand
{
    u32_t clientSeq = 0;
    u32_t clientTick = 0;
    u32_t buttonsPressed = 0;
    u32_t buttonsHeld = 0;
    i16_t moveX = 0;
    i16_t moveY = 0;
    i16_t cameraYaw = 0;
    i16_t cameraPitch = 0;
    u32_t targetEntityNetId = 0;
};
```

버튼 예:

```cpp
enum EldenInputBits : u32_t
{
    Move        = 1 << 0,
    Sprint      = 1 << 1,
    Dodge       = 1 << 2,
    LightAttack = 1 << 3,
    HeavyAttack = 1 << 4,
    Guard       = 1 << 5,
    Parry       = 1 << 6,
    LockOn      = 1 << 7,
    UseItem     = 1 << 8
};
```

## Snapshot

```cpp
struct EldenEntitySnapshot
{
    u32_t netId = 0;
    u16_t archetypeId = 0;
    u16_t flags = 0;
    Vec3 position;
    Vec3 velocity;
    f32_t yaw = 0.f;
    u16_t actionState = 0;
    u16_t animId = 0;
    f32_t animTime = 0.f;
    f32_t hp = 0.f;
    f32_t stamina = 0.f;
};

struct EldenSnapshotPacket
{
    u32_t serverTick = 0;
    u32_t lastProcessedInputSeq = 0;
    u16_t entityCount = 0;
    EldenEntitySnapshot entities[MAX_SNAPSHOT_ENTITIES];
};
```

## Hit Validation

서버는 hitbox/hurtbox를 권위적으로 판정한다.

```
ActionState + AnimTime
  -> active HitboxTimeline entries
  -> overlap with target Hurtbox
  -> guard/parry/iframe check
  -> DamageEvent
  -> HitReactState
```

검증 순서:

1. 공격자가 실제로 공격 state인가
2. 현재 anim time이 hitbox window 안인가
3. stamina/cooldown/action lock이 유효한가
4. 대상이 iframe/parry/guard 상태인가
5. 거리/각도/높이 차가 허용 범위인가
6. 같은 hitbox가 같은 대상에게 중복 hit하지 않았는가

## PvP 특수 처리

PvP는 체감 반응성이 중요하다.

정책:

1. 자기 이동은 local prediction
2. 상대는 interpolation
3. 공격 입력은 즉시 애니 재생 가능하나 damage는 서버 결과 대기
4. parry/dodge iframe은 서버 tick 기준
5. lag compensation은 제한적으로만 허용

Lag compensation 후보:

```
server stores last N ticks of hurtbox transforms
when attack command arrives:
  rewind target hurtboxes by attacker RTT/2 capped at maxRewindMs
  validate
```

`maxRewindMs`는 PvP에서 과도하면 불쾌하므로 100ms 이하부터 시작한다.

## Raid Boss Authority

레이드는 서버가 절대 권위다.

서버 보유:

1. boss phase
2. boss action state
3. boss AI blackboard
4. boss hitbox timeline
5. arena hazards
6. raid wipe/win conditions

클라 보유:

1. telegraph FX
2. camera shake
3. sound
4. UI warning
5. animation interpolation

## Boss Phase Graph

```cpp
struct BossPhaseNode
{
    u32_t phaseId = 0;
    f32_t hpEnterBelow = 1.f;
    u32_t allowedActionMask = 0;
    u32_t nextPhaseId = 0;
};

struct BossActionDef
{
    u32_t actionId = 0;
    const char* animKey = nullptr;
    f32_t durationSec = 0.f;
    f32_t cooldownSec = 0.f;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    u32_t hitboxTimelineId = 0;
    u32_t telegraphFxId = 0;
};
```

## Interest Management

LoL AOI는 grid 중심이고, Elden Raid는 room-wide interest도 필요하다.

| 모드 | Interest |
|---|---|
| Duel | room full interest |
| CoopField | world cell + AOI |
| RaidBoss | arena full interest + spectators optional |

Raid에서는 모든 플레이어가 보스와 서로를 봐야 하므로 AOI를 너무 공격적으로 줄이지 않는다.

## TCP 흐름

```
Login
  -> lobby
  -> party
  -> matchmaking
  -> room allocation
  -> UDP endpoint handshake
  -> enter game
```

TCP 패킷:

| Packet | 역할 |
|---|---|
| C2S_Login | 인증 |
| S2C_LoginOk | 세션 발급 |
| C2S_CreateParty | 파티 |
| C2S_QueueRaid | 레이드 큐 |
| S2C_MatchFound | 룸 정보 |
| C2S_EnterRoom | 입장 |

## UDP 흐름

```
C2S_UdpHello(sessionToken)
S2C_UdpHelloAck(peerId, serverTick)
C2S_InputCommand(seq...)
S2C_Snapshot(tick...)
S2C_Event(reliable optional)
```

## 구현 순서

| 단계 | 내용 | 기준 |
|---|---|---|
| N0 | LoL UDP stable | input/snapshot/reconciliation |
| N1 | Elden packet schema | InputCommand/Snapshot |
| N2 | local simulation test | client-only action sim |
| N3 | server room skeleton | 1 room, 1 player |
| N4 | movement authority | 서버 위치 보정 |
| N5 | action authority | 공격/회피 state 서버 판정 |
| N6 | PvP duel | 2 clients |
| N7 | co-op dummy enemy | 2 clients vs AI |
| N8 | raid boss room | 4 clients vs boss |
| N9 | lag compensation | 제한적 rewind |

## 검증

필수 테스트:

1. packet loss 5%에서 이동 보정
2. latency 100ms에서 dodge/attack 체감 확인
3. PvP에서 speed hack input 거부
4. attack range hack 거부
5. raid boss phase가 모든 클라에서 동일
6. snapshot drop 시 animation interpolation 안정성
7. disconnect/reconnect policy
