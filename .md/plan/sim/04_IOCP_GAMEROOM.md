# Phase Sim-4 — IOCP GameRoom v1.2

**작성일**: 2026-04-29  
**코드베이스 보정**: 2026-04-30  
**목적**: IOCP 기반 서버 프로세스에서 30Hz 권위 시뮬 GameRoom 을 1차로 세운다. 클라는 Command 만 보내고, 서버는 `sessionId -> EntityID` 로 issuer 를 결정한 뒤 Snapshot/Event 를 내려보낸다.

---

## 0. 이번 문서의 정리 기준

v1.1 문서는 구현 코드가 너무 많이 들어가 있어 실제 소스와 금방 어긋난다. v1.2 에서는 **계획서는 작업 순서와 불변식만 가진다**. 실제 구현의 정본은 `Server/Public`, `Server/Private`, `Shared/GameSim`, `Shared/Network`, `Shared/Schemas` 다.

현재 코드베이스 상태는 “Sim-4 완료”가 아니라 “프로젝트와 일부 스텁/Sim-3 산출물 존재”다. 따라서 이번 사이클의 목표는 새 아키텍처를 더 늘리는 것이 아니라, **이미 생긴 스텁을 컴파일 가능한 최소 권위 서버로 수렴**시키는 것이다.

---

## 1. 코드베이스 현재 상태

| 영역 | 현재 상태 | Sim-4 처리 |
|---|---|---|
| `Server/Include/Server.vcxproj` | 존재. `FlatcCodegen` 있음. `ws2_32.lib` 만 링크. `/fp:precise`, `Mswsock.lib`, Engine link/reference 없음 | 4A 에서 프로젝트 설정 보정 |
| `Server/Private/main.cpp` | placeholder 콘솔. IOCP/GameRoom 진입 없음 | 4A 에서 서버 bootstrap 으로 교체 |
| `CIOCPCore` | 헤더/소스 존재. `workter` 오타, `acceptSocket` 없음, 구현 대부분 false/stub | 4B 에서 실제 IOCP listen/worker/accept 로 교체 |
| `CFrameParser` | 존재. `TryExtract(ParsedFrame&, bool&)` 최소형. frame consume/clear 정책 미완성 | 4B-1 최우선 수정 |
| `CSession` | 헤더 초안만 있고 public API 거의 없음. cpp 0줄 | 4B 에서 수명/recv/send 구현 |
| `CSession_Manager` | h/cpp 0줄 | 4B 에서 `Get()->Find()` 패턴 구현 |
| `CPacketDispatcher` | h/cpp 0줄 | 4C 에서 FrameParser drain + FlatBuffers verify |
| `CGameRoom` | 헤더가 의사코드 상태. raw session 순회, 오타, 누락 include 다수 | 4D 에서 새로 정리 |
| `CGameLogic` | h/cpp 0줄 | 이번 사이클 active path 에서 제외 |
| `CServerWorld` | h/cpp 0줄 | 이번 사이클 active path 에서 제외 |
| `CSnapshotBuilder` | cpp 내부 class 선언만 있음. 헤더 없음 | 4F 에서 헤더 분리 + 구현 |
| `CCommandDispatcher` | cpp 내부 class 선언만 있음. 헤더 없음 | 이번 사이클 active path 에서 제외 |
| `CAOI` | h/cpp 0줄 | 4F 에서 구현 |
| `CAntiCheatServer` | h/cpp 0줄 | 4G 에서 command 검증만 구현 |
| `CLagCompensation` | h/cpp 0줄 | 4G 에서 history recorder 최소 구현 |
| `Shared/GameSim` | Deterministic/RNG/EntityIdMap/일부 component/system 존재 | 4E 에서 부족분만 추가 |
| `Shared/Schemas` | Command/Snapshot/Event fbs + cpp/go generated 존재 | 그대로 사용 |

---

## 2. 이번 사이클에서 과감히 빼는 것

아래 항목은 좋은 생각이지만 Sim-4 첫 완주에는 불필요하거나 현재 코드와 중복된다.

| 항목 | 결정 | 이유 |
|---|---|---|
| `CGameLogic` | 보류 | `CGameRoom::Tick()` 이 Shared systems 를 직접 호출하면 충분하다. 중간 wrapper 는 아직 이득이 없다 |
| `CServerWorld` | 보류 | 현재 `CWorld` 가 이미 있고 `Shared/GameSim/World.h` 도 alias 성격이다 |
| `CCommandDispatcher` 별도 계층 | 보류 | `CPacketDispatcher` 가 frame drain, verify, room queue 까지 맡는 편이 단순하다 |
| `ValidateDamage()` 함수 | 삭제 | Command schema 에 damage command 가 없다. “클라가 damage 를 보낼 채널 없음”이 검증이다 |
| full C++ 코드 덤프 | 삭제 | 문서와 소스가 서로 다른 정본이 되는 문제를 막는다 |
| 100 동접/10룸 부하 | Sim-4H 이후 | 첫 목표는 10 session 1 room + 30Hz jitter 검증이다 |
| 완성형 LagComp hit 판정 | 최소화 | Sim-4 에서는 history 기록과 200ms old-check 까지만. 스킬별 rewind 판정은 스킬/예측 단계에서 붙인다 |
| `GameSessionConfig` 본격 연동 | Sim-6 | 이번엔 local `RoomConfig` 또는 seed/player placeholder 로 충분하다 |

---

## 3. 절대 불변식

1. **서버가 진실**: HP, 위치, cooldown, damage 는 서버 `CWorld` 만 변경한다.
2. **issuer spoof 방지**: wire command 에 issuer/source 필드를 추가하지 않는다. issuer 는 항상 `sessionId -> controlled EntityID` 로 서버가 결정한다.
3. **IO thread 와 sim thread 분리**: IOCP worker 는 packet 을 검증하고 queue 에 넣는다. `CGameRoom::Tick()` 은 단일 thread 에서 queue 를 drain 한다.
4. **명령 정렬**: tick 진입 전 `(serverAcceptedTick, sessionId, sequenceNum)` 으로 정렬한다. `clientTick` 은 lag 추정 참고값일 뿐 신뢰하지 않는다.
5. **TCP framing 필수**: raw recv bytes 를 FlatBuffers verifier 에 넣지 않는다. session 별 `CFrameParser` 가 완전한 envelope 를 만든 뒤 verify 한다.
6. **GameRoom 은 raw `CSession*` 보유 금지**: `sessionId` 만 들고, 송신 직전에 `CSession_Manager::Get()->Find(sid)` 한다.
7. **결정성 순회**: system/AOI/snapshot 대상은 `DeterministicEntityIterator` 또는 NetEntityId 정렬을 거친다.
8. **Snapshot bandwidth**: `animKey:string` 금지. schema 처럼 `animId:u16`, `animPhaseFrame:u16` 유지.

---

## 4. Active 클래스 목록

이번 사이클에서 실제로 완성할 클래스만 active 로 둔다.

| 클래스 | 파일 | 책임 |
|---|---|---|
| `CIOCPCore` | `Server/Public/Network/IOCPCore.h`, `Server/Private/Network/IOCPCore.cpp` | listen socket, completion port, worker threads, AcceptEx |
| `CFrameParser` | `Server/Public/Network/FrameParser.h`, `Server/Private/Network/FrameParser.cpp` | session 별 recv buffer + PacketEnvelope 추출 |
| `CSession` | `Server/Public/Network/Session.h`, `Server/Private/Network/Session.cpp` | socket, recv/send overlapped, send queue, sequence guard |
| `CSession_Manager` | `Server/Public/Network/Session_Manager.h`, `Server/Private/Network/Session_Manager.cpp` | `sessionId -> shared_ptr<CSession>`, closing session 수명 유지 |
| `CPacketDispatcher` | `Server/Public/Network/PacketDispatcher.h`, `Server/Private/Network/PacketDispatcher.cpp` | frame drain, FlatBuffers verify, room route |
| `CGameRoom` | `Server/Public/Game/GameRoom.h`, `Server/Private/Game/GameRoom.cpp` | 30Hz tick, pending command drain, Shared sim chain, snapshot send |
| `CSnapshotBuilder` | `Server/Public/Game/SnapshotBuilder.h`, `Server/Private/Game/SnapshotBuilder.cpp` | visible entities 를 Snapshot flatbuffer 로 직렬화 |
| `CAOI` | `Server/Public/Game/AOI.h`, `Server/Private/Game/AOI.cpp` | 50m grid + visibleSet |
| `CAntiCheatServer` | `Server/Public/Security/AntiCheatServer.h`, `Server/Private/Security/AntiCheatServer.cpp` | move/cooldown/range/targetMode 검증 |
| `CLagCompensation` | `Server/Public/Security/LagCompensation.h`, `Server/Private/Security/LagCompensation.cpp` | position history 기록 + 200ms old-check |
| `CDefaultCommandExecutor` | `Shared/GameSim/Systems/CommandExecutor.cpp` | `GameCommand` 를 서버 `CWorld` 에 반영 |
| `CSkillRegistry` | `Shared/GameSim/Registries/SkillRegistry.h/.cpp` | 서버/클라 공통 SkillDef lookup |

보류 파일은 프로젝트에 남아 있어도 되지만, active path 에서 include/call 하지 않는다: `GameLogic`, `ServerWorld`, `CommandDispatcher`.

---

## 5. 4A — 프로젝트/부트스트랩 보정

**목표**: `WintersServer.exe` 가 placeholder 가 아니라 서버 bootstrap 을 가진다.

작업:
- `Server.vcxproj` Debug/Release 모두 `/fp:precise` 고정.
- `AdditionalDependencies` 에 `ws2_32.lib;mswsock.lib` 명시.
- GameRoom 이 `CWorld`/Engine ECS 를 실제로 링크하기 시작하는 시점에 `WintersEngine.lib` 또는 Engine project reference 추가.
- `Shared/Schemas/run_codegen.bat` pre-build 는 유지.
- `main.cpp` 는 port/workerCount 설정 후 `CIOCPCore::Create(...)->Start()` 로 진입.

합격:
- `Server.vcxproj` 가 Debug x64 로 빌드 대상 파일을 모두 포함한다.
- 실행 시 placeholder 문구가 아니라 listen 준비 로그가 나온다.

---

## 6. 4B — IOCP + Session

**목표**: TCP client 가 connect/disconnect 할 수 있고, outstanding OVERLAPPED 수명이 안전하다.

작업:
- `CIOCPCore` 의 `workter` 오타를 `worker` 로 정리.
- `IOContext` 에 `acceptSocket` 을 추가한다. AcceptEx socket 은 completion 전까지 context 가 소유한다.
- accept 완료 순서는 `SO_UPDATE_ACCEPT_CONTEXT -> CSession_Manager::OnAccept -> BindIOCP -> PostInitialRecv`.
- `CSession` 은 `shared_ptr` factory, `PostRecv`, `OnRecvComplete`, `Send`, `OnSendComplete`, `OnDisconnect` 를 가진다.
- `WSASend` buffer 는 send queue 가 소유한다. caller-owned pointer 금지.
- `CSession_Manager` 는 `static CSession_Manager* Get()` 과 `Find(sessionId)` 를 제공한다.
- closing session 은 `pendingIoCount == 0` 이 될 때까지 `m_closingSessions` 에 보관한다.

합격:
- 1 client connect/disconnect 반복 시 crash 없음.
- disconnect 직후 completion 이 늦게 와도 session 내부 `IOContext` use-after-free 없음.

---

## 7. 4B-1 — FrameParser 정리

**목표**: partial/sticky packet 을 안전하게 처리한다.

현재 `CFrameParser::TryExtract(ParsedFrame&, bool&)` 는 완전 frame 을 찾아도 buffer consume 이 없다. 이 상태로는 sticky packet 2개를 처리할 수 없고 같은 frame 을 반복 추출할 위험이 있다.

권장 API:

```cpp
enum class eFrameParseResult : u8_t
{
    NeedMore = 0,
    Complete = 1,
    Invalid = 2,
};

struct ParsedFrameOwned
{
    ePacketType type = ePacketType::None;
    u32_t sequence = 0;
    std::vector<u8_t> payload;
};
```

정책:
- `Append(nullptr, 0)` 은 no-op.
- magic/version mismatch, payload > 64KB, accumulated buffer > 256KB 는 `Invalid`.
- `Complete` 시 payload 는 owned copy 로 반환하고 consumed bytes 를 buffer 에서 제거한다.
- `Invalid` 시 buffer 를 clear 하고 caller 가 disconnect 한다.

합격:
- envelope 를 1 byte 씩 나눠 보내도 1 frame 으로 복원된다.
- envelope 2개를 한 chunk 로 보내면 2 frame 이 순서대로 나온다.
- bad magic/version/payloadSize 는 disconnect 로 이어진다.

---

## 8. 4C — PacketDispatcher

**목표**: 완전한 PacketEnvelope 만 FlatBuffers verifier 로 들어간다.

작업:
- `CPacketDispatcher::DrainFrames(sessionId, CFrameParser&)` 구현.
- `ePacketType::CommandBatch` 만 `VerifyCommandBatchBuffer` 후 room queue 로 보낸다.
- Heartbeat 는 1차 no-op.
- 알 수 없는 type 은 suspicion count 만 증가.
- route table 은 `sessionId -> roomId`, `roomId -> CGameRoom*`.

주의:
- `OnRecv(bytes)` 래퍼와 `CSession::OnRecvComplete()` 가 같은 bytes 를 동시에 append 하지 않게 한다. 실사용 경로는 `CSession::OnRecvComplete -> DrainFrames` 하나로 고정한다.

합격:
- dummy CommandBatch payload verify 성공.
- 깨진 FlatBuffer payload 는 command queue 에 들어가지 않는다.

---

## 9. 4D — GameRoom 30Hz tick

**목표**: IOCP worker N개와 독립된 단일 sim tick 을 만든다.

`CGameRoom.h` 는 현재 의사코드 상태라 부분 수정하지 말고 구조를 새로 잡는다.

필수 멤버:
- `CWorld m_world`
- `EntityIdMap m_entityMap`
- `DeterministicRng m_rng`
- `u64_t m_tickIndex`
- `std::atomic<u64_t> m_visibleTickIndex`
- `std::mutex m_pendingMutex`
- `std::vector<PendingCommand> m_pendingCommands`
- `std::vector<GameCommand> m_pendingExecCommands`
- `std::vector<u32_t> m_sessionIds`
- `std::unordered_map<u32_t, EntityID> m_sessionToEntity`

`PendingCommand` 필드:
- `sessionId`
- `sequenceNum`
- `GameCommandWire wire`
- `serverAcceptedTick`
- `recvTimeMs`
- `clientTickHint`

tick 단계:
1. `Phase_DrainCommands`: queue swap, sort, AntiCheat, `BuildServerCommand`
2. `Phase_ExecuteCommands`: `CDefaultCommandExecutor::ExecuteCommand`
3. `Phase_SimulationSystems`: Shared systems 고정 순서 실행
4. `Phase_AOIUpdate`: session 별 visibleSet 계산
5. `Phase_BuildAndSendSnapshots`: envelope 로 감싸서 send
6. `Phase_EmitEvents`: 1차는 broadcast, 이후 visibleSet routing

합격:
- 30Hz tick 이 5분 이상 유지된다.
- worker thread 가 command 를 밀어 넣어도 world mutation 은 tick thread 에서만 발생한다.
- drain 정렬 결과가 같은 input 에 대해 항상 같다.

---

## 10. 4E — Shared GameSim 연결

**목표**: GameRoom 이 실제 gameplay command 를 처리할 수 있다.

현재 있는 것:
- `ICommandExecutor.h`
- `BuildServerCommand(...)` 선언
- `DeterministicRng`, `DeterministicTime`, `EntityIdMap`
- `StatSystem`, `BuffSystem`, `DamagePipeline`, `PendingHitSystem` 일부
- `ChampionStatsRegistry`, `SkillScalingRegistry`, `SkinRegistry`

부족한 것:
- `Shared/GameSim/Systems/CommandExecutor.cpp`
- `Shared/GameSim/Registries/SkillRegistry.h/.cpp`
- `Shared/GameSim/Systems/SkillCooldownSystem.h/.cpp`
- `Shared/GameSim/Systems/MoveSystem.h/.cpp`
- `Shared/GameSim/Systems/DamageQueueSystem.h/.cpp`
- `Shared/GameSim/Systems/DeathSystem.h/.cpp`

정책:
- Client 의 `GamePlay/SkillRegistry` 를 서버가 include 하지 않는다.
- Shared `CSkillRegistry` 를 정본으로 만들고, Client registry 는 wrapper 나 re-export 로 정리한다.
- `CDefaultCommandExecutor` 는 Visual hook 을 호출하지 않는다. `GameplayHookRegistry` 만 사용한다.
- `Move` command 는 목적지 의도만 저장한다. 위치 적분은 서버 `MoveSystem` 이 `StatComponent::moveSpeed` 로 한다.

합격:
- Move command 가 서버 world 의 movement state 로 들어간다.
- CastSkill 이 cooldown/mana/range 검증 후 accepted 상태로 들어간다.
- BasicAttack 또는 최소 DamageRequest 경로 하나가 `ApplyDamageRequest` 까지 연결된다.

---

## 11. 4F — AOI + SnapshotBuilder

**목표**: session 별 visible entity 만 Snapshot 으로 내려보낸다.

작업:
- `Server/Public/Game/SnapshotBuilder.h` 를 만든다. cpp 내부 class 선언은 제거한다.
- SnapshotBuilder 는 `visibleSet` 을 NetEntityId 오름차순으로 정렬한 뒤 flatbuffer 를 만든다.
- `CAOI` 는 50m cell grid 로 1차 구현한다.
- Transform 은 현재 Engine ECS 의 `TransformComponent` 를 사용한다.
- 1차 AOI 는 거리/grid 만 본다. Fog of War, wall occlusion, bush/ward 는 다음 단계다.

합격:
- 20 entity 기준 Snapshot 이 2KB 안쪽이다.
- 다른 cell 의 entity 가 visibleSet 에 들어오지 않는다.
- 같은 world/same visibleSet 에서 snapshot entity 순서가 항상 같다.

---

## 12. 4G — AntiCheat + Lag history

**목표**: command channel 에서 바로 막을 수 있는 것만 막는다.

AntiCheat 1차:
- sequence gap/duplicate: `CSession::TryAcceptSequence`
- Move: finite + map bounds + walkable 예정. far destination 은 reject 하지 않는다
- CastSkill: cooldown + range + targetMode
- issuer spoof: wire 에 issuer/source 필드가 없고 서버가 `sessionId -> entity` 를 결정하므로 구조적으로 차단

targetMode 정책:
- `Self`: target 이 없거나 issuer 만 허용
- `UnitTarget`: targetNet 이 valid 여야 한다. ally/enemy 여부는 SkillDef 플래그가 생기면 그때 검증
- `GroundTarget`: `groundPos` finite/bounds 검증
- `Direction`: direction finite/normalized 검증
- `Conditional`: hook 검증으로 넘긴다

LagCompensation 1차:
- 매 tick 끝에 `(tickIndex, EntityID -> Vec3)` history 를 기록한다.
- 보존 길이는 6 frame, 즉 30Hz 기준 약 200ms.
- `IsTooOld(pastTick, currentTick)` 만 제공한다.
- 스킬별 rewind hit 판정은 Sim-5/스킬 서버화 단계에서 붙인다.

합격:
- cooldown 중 skill command reject.
- range 초과 target skill reject.
- map 밖 move reject.
- 250ms 초과 rewind request 는 suspicious/reject 경로로 들어간다.

---

## 13. 4H — 결정성/회귀

**목표**: 같은 seed + 같은 ordered command stream 이 같은 결과를 만든다.

검증:
- FrameParser unit smoke: 1 byte chunk, sticky 2 packet, bad magic, payload > 64KB.
- PacketDispatcher smoke: valid CommandBatch 만 room queue 로 들어간다.
- GameRoom smoke: 10 fake session, 5분 30Hz tick, jitter < 5ms.
- Determinism smoke: 같은 seed/input 으로 room 2개를 5분 돌려 hp/pos/rngState/snapshot hash 비교.
- Snapshot smoke: entity 정렬과 size 확인.

부하 목표:
- Sim-4 종료 기준은 **10 session / 1 room** 이다.
- 100 동접 / 10 room 은 Sim-4 통과 후 별도 load cycle 로 넘긴다.

---

## 14. 종료 조건

Sim-4 는 아래가 모두 통과하면 완료다.

- `WintersServer.exe` 가 placeholder 가 아니라 IOCP 서버로 실행된다.
- TCP framing 이 partial/sticky/bad header 를 처리한다.
- CommandBatch 가 FlatBuffers verify 후 GameRoom queue 로 들어간다.
- GameRoom 은 단일 tick thread 에서 30Hz 로 world 를 갱신한다.
- issuer 는 `sessionId -> EntityID` 로만 결정된다.
- Snapshot 은 PacketEnvelope 로 감싸져 session 별로 송신된다.
- AntiCheat 1차(move/cooldown/range/targetMode)가 동작한다.
- Lag history recorder 가 6 frame 보존한다.
- 2 room deterministic smoke 가 통과한다.

---

## 15. Gotcha 메모

사이클 완료 후 `CLAUDE.md` 에 반영할 항목:

- G-Sim16: TCP recv 는 임의 chunk. PacketHeader + per-session FrameParser 필수.
- G-Sim17: GameRoom 은 raw `CSession*` 보유 금지. sessionId 만 보관.
- G-Sim18: 명령 정렬은 `(serverAcceptedTick, sessionId, sequenceNum)`.
- G-Sim19: issuer spoof 방어는 wire 에 issuer 필드가 없는 구조 자체다.
- G-Sim20: Snapshot 은 `animId`/`animPhaseFrame` 사용. `animKey:string` 금지.
- G-Sim21: AcceptEx socket 은 completion 까지 `IOContext::acceptSocket` 이 소유한다.
- G-Sim22: `Shared/GameSim` 소스는 Client include 금지. SkillRegistry 는 Shared 정본.
- G-Sim23: Move packet 은 목적지 의도다. 서버가 moveSpeed 로 위치를 적분한다.
- G-Sim24: LagCompensation 1차 한계는 200ms. 그 이상은 reject/suspicious.

---

## 한 줄

Sim-4 v1.2 는 “새 클래스 14개를 한 번에 늘리는 계획”이 아니라, 이미 만들어진 Server/Shared 스텁을 **IOCP 수신 -> FrameParser -> FlatBuffers verify -> GameRoom queue -> 30Hz authority tick -> AOI Snapshot** 으로 수렴시키는 사이클이다. Active path 는 작게 유지하고, 중복 계층은 다음 단계로 미룬다.
