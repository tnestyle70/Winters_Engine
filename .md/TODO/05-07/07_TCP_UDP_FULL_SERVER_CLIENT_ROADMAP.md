# TCP / UDP 완성형 서버-클라이언트 로드맵

작성일: 2026-05-07  
대상: 실제 LoL식 Winters 서버-클라 완성  
결론: **TCP는 Control, UDP는 Gameplay. 서버는 모든 gameplay 결과의 단일 원천이다.**

---

## 1. 최종 네트워크 분리

### 1.1 TCP Control Plane

TCP 또는 HTTP에 남길 것:

```text
HTTP Backend:
  Auth
  Profile
  Shop
  Payment
  Matchmaking
  Leaderboard
  Inventory
  Skin ownership

TCP Game Control:
  Login token 검증
  Lobby room join
  BanPick
  LobbyState
  GameStart
  UDP GameplayJoinTicket
  Loading progress
  Surrender / remake / pause 같은 control vote
  PostGame result
  Chat
  Reconnect ticket
```

TCP에 두는 이유:

- 손실되면 안 된다.
- 순서가 중요하다.
- 빈도가 낮다.
- latency보다 신뢰성이 중요하다.

### 1.2 UDP Gameplay Plane

UDP로 보낼 것:

```text
Client -> Server:
  Move
  Stop
  AttackMove
  BasicAttack
  CastSkill
  LevelSkill
  UseItem
  Recall
  Ping
  InputAck

Server -> Client:
  Snapshot
  DeltaSnapshot
  SkillCastAccepted/Rejected
  AnimationStart
  ProjectileSpawn
  ProjectileHit
  Damage
  Heal
  Death
  BuffApply/BuffExpire
  EffectTrigger
  MinionWave
  TowerShot
  GameState
```

UDP에 두는 이유:

- tick 단위로 자주 오간다.
- 오래된 packet보다 최신 packet이 중요하다.
- loss/reorder를 직접 제어해야 한다.
- prediction/reconciliation이 필요하다.

---

## 2. 완성형 서버 tick

최종 `CGameRoom::Tick`은 다음 순서를 목표로 한다.

```text
1. Advance tick
2. Drain UDP command batches
3. Validate commands
4. Execute command intents
5. Active cast / attack windup
6. Skill gameplay hooks
7. Projectile movement / collision
8. Minion wave / minion AI
9. Tower AI / tower projectile
10. Jungle AI
11. Damage queue reduce
12. Buff/status tick
13. Stat recompute
14. Death / respawn / objective state
15. Game end check
16. Build events
17. Build snapshots
18. Send UDP packets
```

중요:

- Snapshot은 결과 state다.
- Event는 결과가 발생한 이유와 visual trigger다.
- Client는 event를 놓쳐도 snapshot으로 최종 state를 복구할 수 있어야 한다.

---

## 3. 완성형 Client frame

최종 client frame은 다음 흐름이다.

```text
1. Poll input
2. Build command
3. Send UDP CommandBatch
4. Local prediction
5. Receive UDP packets
6. Apply events
7. Apply snapshot / reconciliation
8. Interpolate remote entities
9. Play animation
10. Play projectile/effect visuals
11. Render
```

Client의 gameplay write 규칙:

```text
Local predicted component:
  임시 write 가능

Authoritative component:
  server snapshot/event만 write

Render-only component:
  client 자유
```

---

## 4. Milestone 0: 현재 TCP MVP 고정

목표:

- BanPick TCP 유지
- GameStart 유지
- 기존 1-client Move/Snapshot 회귀 방지

완료 기준:

- LobbyState 정상
- GameStart 정상
- TCP snapshot fallback 정상
- 기존 local-only mode 정상

---

## 5. Milestone 1: UDP GameplayJoin + Move Snapshot

목표:

- UDP gameplay session을 열고 Move/Snapshot만 왕복한다.

작업:

```text
GameplayJoin / GameplayJoinAck
CUdpCore
CUdpSession
CUdpGameplayClient
InGameNetworkBridge dual-plane
Move command UDP 전송
Snapshot UDP 수신
```

완료 기준:

- TCP BanPick으로 시작
- UDP Join 성공
- Move command UDP
- Snapshot UDP
- Client position server authoritative

---

## 6. Milestone 2: Snapshot EntityKind v2

목표:

- champion 외 entity도 snapshot으로 복원한다.

작업:

```text
EntityKind enum
stateFlags
maxHp
ownerNet
subtype
projectile fields
SnapshotBuilder v2
SnapshotApplier v2
OnNewEntityCallback kind-aware 변경
```

완료 기준:

- Champion / Minion / Turret / Projectile를 구분해 생성 가능
- HP bar가 entity kind와 관계없이 표시
- unknown kind는 안전하게 무시

---

## 7. Milestone 3: Animation Sync

목표:

- 스킬/평타/죽음 애니메이션이 두 클라이언트에서 동일하게 보인다.

작업:

```text
NetAnimationComponent v2
AnimationStartEvent
actionSeq
animStartTick
CAnimationSyncApplier
SkillCastAccepted event
Death animation event
```

완료 기준:

- Client A Q 사용 -> Client B도 Q anim 재생
- BA 반복 actionSeq 정상
- death anim 1회
- run/idle이 skill one-shot을 덮지 않음

---

## 8. Milestone 4: Damage / HP / Death Authority

목표:

- 모든 HP 변화가 서버에서만 확정된다.

작업:

```text
DamageQueueSystem 구현
DamageEvent broadcast
DeathEvent broadcast
HealthComponent 원천화
Champion/Minion/Structure hp mirror 정리
client local ApplyDamage 제거 또는 predicted-only 격리
```

완료 기준:

- Client local에서 적 HP를 직접 최종 감소시키는 gameplay path 없음
- Server damage event 후 양쪽 HP 동일
- Death snapshot/event 동일

---

## 9. Milestone 5: BasicAttack / Skill Authority

목표:

- 평타와 스킬이 서버 검증 후 실행된다.

작업:

```text
SendBasicAttack
SendCastSkill
Server ValidateBasicAttack
Server ValidateCastSkill
ActiveAttackComponent
ActiveCastComponent
castFrame tick
GameplayHookRegistry server registration
VisualHookRegistry client event path
```

완료 기준:

- 사거리 밖 평타 reject
- 쿨다운 중 스킬 reject
- stun/disarm 상태 command reject
- accepted skill만 animation/effect/damage 발생

---

## 10. Milestone 6: Projectile Authority

목표:

- 투사체가 서버 entity로 생성되고 모든 클라이언트에 보인다.

작업:

```text
ProjectileComponent
ProjectileSystem
ProjectileSpawnEvent
ProjectileHitEvent
projectile snapshot
ProjectileVisualBinder
tower projectile first
Ezreal/Yasuo/Kalista projectile migration
```

완료 기준:

- Projectile netId 존재
- 두 클라이언트에서 같은 projectile 표시
- hit/damage 서버 판정
- projectile destroy 동기화

---

## 11. Milestone 7: Effect Authority

목표:

- gameplay-significant VFX/SFX가 서버 event 기준으로 재생된다.

작업:

```text
EffectTriggerEvent
EffectEventApplier
effect id registry
predicted effect reconciliation
damage number event
hit flash event
buff apply/expire visual event
```

완료 기준:

- Client A 스킬 hit -> Client B도 impact VFX 확인
- tower shot VFX 양쪽 동일
- death/buff effect 1회
- reject된 predicted effect fade-out

---

## 12. Milestone 8: Minion Server Authority

목표:

- 미니언 wave, 이동, 공격, 죽음이 서버 권위가 된다.

작업:

```text
Server StageData load
Server lane waypoint load
Server MinionWaveSystem
Server CMinionAISystem 연결
Minion netId 발급
Minion animation state sync
Client CMinion_Manager render-only 전환
```

완료 기준:

- 두 클라이언트에서 같은 미니언 wave
- 같은 target 공격
- 같은 HP/death
- client local minion spawn/tick 제거

---

## 13. Milestone 9: Tower / Structure Authority

목표:

- 타워/억제기/넥서스가 서버 권위가 된다.

작업:

```text
Server Structure spawn
Server CTurretAISystem 연결
Server CTurretProjectileSystem 연결
Tower aggro event
Structure HP snapshot
Structure death / activation state
Nexus destruction GameEnd
```

완료 기준:

- 타워 target이 모든 클라에서 동일
- tower projectile 동기화
- tower damage 동기화
- tower death 동기화
- nexus death -> GameEnd

---

## 14. Milestone 10: UDP Reliability / Delta / AOI

목표:

- 실전 트래픽으로 확장한다.

작업:

```text
M2 reliability
ack/ackBits
retransmit
fragment/reassembly
M3 delta snapshot
baseline ack
AOI
M4 lag compensation
M5 prediction/reconciliation
M6 replay determinism
```

완료 기준:

- packet loss 환경에서도 중요 event 유지
- snapshot bandwidth 감소
- 멀리 있는 entity 미전송
- skill hit lag compensation 가능
- replay deterministic check 통과

---

## 15. 완성형 Acceptance Matrix

| 도메인 | 완료 기준 |
|---|---|
| TCP Backend | Auth/Profile/Shop/Match/BanPick/PostGame이 TCP/HTTP로 안정 동작 |
| UDP Join | GameStart ticket으로 UDP gameplay bind |
| Movement | 서버 위치가 최종 원천 |
| Animation | skill/BA/death/run/idle이 remote client에 동기화 |
| Skill | cooldown/range/resource/status 검증 서버 권위 |
| Damage | HP 감소/죽음 서버 권위 |
| Projectile | spawn/move/hit/destroy 서버 권위 |
| Effect | gameplay effect trigger 서버 event 기반 |
| Minion | wave/AI/attack/death 서버 권위 |
| Tower | targeting/projectile/damage 서버 권위 |
| Objective | inhib/nexus/game end 서버 권위 |
| Prediction | local feel 유지, server correction 가능 |
| AOI | 필요한 entity만 전송 |
| Replay | 같은 command log로 같은 결과 |

---

## 16. 절대 금지할 최종 구조

```text
Client가 remote HP를 직접 최종 감소
Client가 projectile hit를 최종 결정
Client가 tower target을 결정
Client가 minion wave를 자체 spawn
Client가 remote skill accepted를 자체 판단
Effect가 local-only로만 재생되어 remote에 보이지 않음
Animation one-shot이 snapshot 없이 local에서만 재생
TCP로 gameplay snapshot/command를 계속 전송
UDP로 BanPick/LobbyState를 전송
```

---

## 17. 오늘 기준 다음 실제 작업 추천

가장 먼저 해야 할 구현 순서:

```text
1. CommandSerializer SendCastSkill / SendBasicAttack
2. EntitySnapshot EntityKind v2
3. EventApplier skeleton
4. NetAnimation actionSeq
5. DamageQueueSystem 구현
6. Server BasicAttack authority
7. Tower projectile server authority
8. ProjectileSpawn/Hit event
9. EffectTrigger event
10. Client visual applier 연결
```

이 순서가 런타임에서 바로 확인 가능한 순서다. 특히 지금 확인된 미동기화 문제인 animation/projectile/effect를 빠르게 잡으려면 `AnimationStartEvent`, `ProjectileSpawnEvent`, `EffectTriggerEvent`를 먼저 눈에 보이게 만들어야 한다.
