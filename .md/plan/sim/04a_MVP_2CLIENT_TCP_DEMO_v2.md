# Phase 04a v2 — MVP 2-Client TCP Demo (Codex 보정 + A' Transport-Neutral Boundary)

**작성일**: 2026-04-30
**v1 → v2 변경 사유**: Codex 리뷰 5+3건 + 사용자 A' 권장 (transport-neutral core 박제). 큰 축 (TCP MVP → UDP 마이그) 유지. 안전성을 위해 Shared sim 실행부 박제 + MVP 합격 기준 layer 분리 + transport boundary 명시.
**위치**: v1.2 `04_IOCP_GAMEROOM.md` 의 본격 박제 (TCP) + Sim-10 v2 의 M1 (UDP) prerequisite
**한 줄 명제**: **Transport-neutral gameplay sync 검증 = Layer 1 (Move/HP sync, ~58h) + Layer 2 선택 (Cast event echo, +15-25h). 5 core 클래스 (GameRoom/Snapshot×2/Serializer/Executor) 는 transport 무관 — UDP 마이그 시 그대로 재사용. TCP transport 코드는 UDP M1 에서 폐기 가능.**

---

## 0. v1 → v2 변경 정리 (Codex 5+3건 + A' 채택)

| # | v1 | v2 |
|---|---|---|
| **C1** | Shared sim 실행부 ready 가정 (`CMoveSystem`/`CSkillCooldownSystem`/`CDeathSystem`/`CProjectileSystem` + `ICommandExecutor::ExecuteCommand` cpp) | **D-0B 신규 — ServerSimSubset 최소 박제**. Move/Cooldown/Damage/Death 4개 server 측 minimal 구현. `CPendingHitSystem` 은 Client/Yasuo 의존 — 서버 포팅 X (Layer 2 에서 별도) |
| **C2** | "B 스킬 → A FX 반영" full snapshot 만으로 가능 가정 | **MVP 합격 기준 Layer 분리**. Layer 1 = Move/HP sync (snapshot only). Layer 2 (선택) = Cast event echo (Event 또는 RemoteCommandEcho) |
| **C3** | `SnapshotBuilder::Build(const CWorld&, ...)` const 시그니처 | **`Build(CWorld&, ...)` non-const** — `World.h:82` GetComponent/ForEach 가 non-const, `DeterministicEntityIterator` 도 non-const 받음 |
| **C4** | `CSnapshotApplier` 가 RenderComponent 까지 직접 생성 | **`OnNewEntity(championId, netId)` 콜백 → Scene 이 `CreateECSChampionFromDef` 호출**. Applier 는 Transform/Health/Stat 만 갱신 |
| **C5** | D-1A 는 Engine project reference 만 | **D-0A 신규 — Server.vcxproj 에 Shared cpp 명시 편입** (`ChampionStatsRegistry.cpp`, `SkillScalingRegistry.cpp`, `BuffSystem.cpp`, `DamagePipeline.cpp`, `StatSystem.cpp` 등) 또는 Shared static lib 신설 |
| **C6 (small)** | `steady_clock` sim 외부 사용 명시 부재 | **GameRoom::TickThread 의 steady_clock 허용 + sim logic 은 wall-clock 미수신 규칙 명시** |
| **C7 (small)** | `CClientNetwork.h/.cpp` 파일명 | **`ClientNetwork.h/.cpp`** (파일명 C 없이, WINTERS_ENGINE_CONVENTIONS §1.1 준수). 클래스명 `CClientNetwork` 유지 |
| **C8 (small)** | Hello payload 처리 미명시 | **FlatBuffers `Hello.fbs` 추가** (또는 `#pragma pack(push,1)` + `static_assert` POD struct) |
| **C9 (repo 실측)** | `SkillStateComponent.cooldowns[]` / `activeSkillSlot` 필드 가정 | **현재 구조는 `SkillStateComponent.slots[5].cooldownRemaining/currentStage/stageWindow`**. Layer 1 은 cooldown tick 만, Layer 2 cast timing 은 별도 `ActiveCastComponent` 또는 `CastRuntimeComponent` 로 분리 |
| **C10 (repo 실측)** | `MoveTargetComponent` 존재 가정 | **D-0B 에서 `Shared/GameSim/Components/MoveTargetComponent.h` 신규 추가**. Move command 는 이 컴포넌트에 target 을 기록 |
| **C11 (repo 실측)** | `Event.fbs` 신규 박제 가정 | **`Shared/Schemas/Event.fbs` 는 이미 존재**. Layer 2 는 root/type 을 갈아엎지 말고 기존 `EventPacket` / `SkillCastEvent` 를 확장 |
| **A' 채택** | 단순 "TCP MVP → UDP 마이그 102h" | **A' 압축 — Transport boundary 명시 + Layer 분리 + 5 core 재사용**. Layer 1 ~58h. Layer 2 +15-25h. 합치면 v1 의 52h 보다 늘지만 UDP 마이그 시 transport 만 갈아끼움 |

---

## 1. 핵심 — Transport Boundary (★ A')

### 1.1 명제

> **TCP MVP의 목표는 production network 가 아니라 transport-neutral gameplay sync 검증이다. TCP 구현은 UDP M1 에서 폐기될 수 있으므로 GameRoom/Serializer/Snapshot/Applier 와 transport 를 강하게 결합하지 않는다.**

### 1.2 Boundary 매트릭스

| 영역 | 클래스 | UDP 마이그 시 |
|---|---|---|
| **Transport-aware (TCP-specific, M1 폐기 가능)** | `CIOCPCore` | UDP 의 `CUdpCore` 로 교체 |
| | `CFrameParser` | UDP 는 packet boundary 자체가 frame → FrameParser 불필요 |
| | `CSession` (recv/send/queue) | UDP 의 `CUdpSession` (channel 분리) |
| | `CSession_Manager` | UDP 의 `CUdpSessionManager` (sourceAddr 기반) |
| | `CPacketDispatcher` | UDP 의 `CUdpPacketDispatcher` |
| | `CClientNetwork` (TCP) | UDP 의 `CUdpClient` |
| **Transport-neutral (★ 그대로 재사용)** | `CGameRoom` (Tick + Phase chain + stable_sort) | **그대로** |
| | `CDefaultCommandExecutor` | **그대로** |
| | `CSnapshotBuilder` (Build → DetachedBuffer) | **그대로** (delta 는 M3 에서 추가) |
| | `CSnapshotApplier` | **그대로** |
| | `CCommandSerializer` (BuildCommandBatch → bytes) | **그대로** (channel 인자 추가) |
| | `ServerSimSubset` (Move/Cooldown/Damage/Death) | **그대로** |
| | 결정성 가드 (`/fp:precise` + `stable_sort` + allowlist) | **그대로** |

### 1.3 Boundary 강제 메커니즘

**Send/Recv 인터페이스 통일**:
- Server → Session: `void CSession::Send(std::vector<u8_t> packet)` — bytes 만. `ePacketType` 은 envelope header 안에. 
- Server ← Session: `void OnRecvComplete(const u8_t*, u32_t)` — bytes 만.
- Client 측도 동일 — `CClientNetwork::Send/SetFrameCallback` 은 bytes only.

**Transport 종속 X 검증**:
- `grep -rn "WSARecv\|WSASend\|recvfrom\|sendto\|AcceptEx" Server/Public/Game/ Server/Public/Security/ Shared/`
- 결과 0 hit 이면 transport-neutral.

---

## 2. MVP 합격 기준 — Layer 분리 (★ Codex C2)

### Layer 1 — Move/HP Sync (필수, ~58h)

**시각적 합격 기준**:
1. Server 콘솔: "Server listening" → "Client A connected (sid=1, netId=1)" → "Client B connected (sid=2, netId=2)"
2. Client A: 본인 + Client B 챔피언 둘 다 보임. 본인 우클릭 이동 시 본인 캐릭터 이동.
3. Client B: 본인 + Client A 챔피언 둘 다 보임. **Client A 이동이 1-2 frame 지연 후 Client B 화면에 반영**.
4. **HP 동기화** — server 가 HP 강제 변경 (디버그 키) → 양쪽 client snapshot 으로 즉시 반영.
5. Disconnect 시 양쪽 정상 cleanup.

**Layer 1 미포함**:
- Cast event echo (스킬 FX 시각화) → Layer 2
- Damage application (스킬 데미지) → Layer 2
- AOI / Delta / Prediction / LagComp / Encryption → Sim-10 v2 M1+

### Layer 2 — Cast Event Echo (선택, +15-25h)

**시각적 합격 기준**:
6. Client B 가 스킬 (Q/W/E/R) → Client A 화면에 Client B 의 캐스팅 애니 + FX 시각화
7. 데미지 — server 권위 적용 → 양쪽 client HP 갱신 (Layer 1 의 snapshot HP sync 가 reuse)

**Layer 2 추가 작업**:
- `Shared/Schemas/Event.fbs` — `CastEvent { netId, slot, animKey, position, direction, serverTick }` 추가
- Server 가 `CDefaultCommandExecutor::HandleCastSkill` 진입 시 `CGameRoom::EmitEvent(CastEvent)` 호출
- Server 가 매 tick 끝에 pending event 들을 broadcast (`ePacketType::Event`)
- Client `OnEvent` callback → `Visual::OnCastFrame_*_Visual` hook trigger (이즈리얼 FX 등)

**왜 Layer 분리**: snapshot 만으로 "B 스킬 → A FX" 가 안 됨 (Codex C2). 단 Move/HP 동기화는 snapshot 만으로 충분 → Layer 1 부터 시각 검증 가능.

---

## 3. Phase D-0 신규 — ServerSimSubset + Shared cpp link closure (★ Codex C1+C5)

### D-0A — Server.vcxproj 에 Shared cpp 편입 (3h)

**목표**: Server EXE 가 Shared/GameSim 의 sim cpp 들을 link.

**옵션**:
- **(a) 직접 편입** (★ MVP 권장): `Server.vcxproj` `<ItemGroup>` 에 Shared cpp 를 **명시 경로로 개별 추가**. MSBuild wildcard 로 대충 묶지 않는다.
  - 필수 cpp: `ChampionStatsRegistry.cpp`, `SkillScalingRegistry.cpp`, `SkinRegistry.cpp`, `BuffSystem.cpp`, `DamagePipeline.cpp`, `StatSystem.cpp`, `SkillRankSystem.cpp`, `GameplayHookRegistry.cpp` (있는 거 전수)
  - 신규 D-0B 산출물: `CommandExecutor.cpp`, `MoveSystem.cpp`, `SkillCooldownSystem.cpp`, `DamageQueueSystem.cpp`, `DeathSystem.cpp`
- (b) Shared static lib 신설: `SharedSim.vcxproj` 만들고 위 cpp 전부 → static lib. Server/Client 가 link.
  - 더 깔끔하지만 +5h. MVP 후 별도 사이클.

**합격**: `#include "Shared/GameSim/Systems/StatSystem.h"` 후 호출 시 LNK2019 없음.

### D-0B — ServerSimSubset 최소 박제 (8h, ★ Codex C1)

**목표**: GameRoom Phase_SimulationSystems 가 호출하는 system 들의 server 측 minimal 구현.

**박제 대상**:

#### B-1: `Shared/GameSim/Systems/CommandExecutor.cpp` (신규)
- `CDefaultCommandExecutor::Create()` (이미 헤더에 정적 팩토리 선언)
- `ExecuteCommand(world, tc, cmd)` 본체 — switch (cmd.kind):
  - `Move` → `HandleMove` — 신규 `MoveTargetComponent` 에 `cmd.groundPos` 저장
  - `CastSkill` → Layer 1 은 reject/log only. Layer 2 에서 `ActiveCastComponent` 또는 event emit 추가
  - `BasicAttack` → Layer 1 은 reject/log only. Layer 2 에서 PendingHit / DamageQueue 연동
  - `LevelSkill` → `HandleLevelSkill` — `SkillRankComponent.ranks[slot]++`
  - `BuyItem` → 빈 (Layer 2)

#### B-0: `Shared/GameSim/Components/MoveTargetComponent.h` (신규)
```cpp
struct MoveTargetComponent
{
    Vec3 target{};
    f32_t arriveRadius = 0.15f;
    bool_t bHasTarget = false;
};
```

#### B-2: `Shared/GameSim/Systems/MoveSystem.h/.cpp` (신규)
- `CMoveSystem::Execute(world, tc)` — `TransformComponent::SetPosition(...)` 으로 `MoveTargetComponent.target` 방향 이동
- 이동 속도 우선순위: `StatComponent.moveSpeed` → 없으면 `ChampionComponent.moveSpeed` → fallback 8.f
- 도착 시 `MoveTargetComponent.bHasTarget = false`

#### B-3: `Shared/GameSim/Systems/SkillCooldownSystem.h/.cpp` (신규)
- `CSkillCooldownSystem::Execute(world, tc)` — 현재 repo 구조 기준 `SkillStateComponent.slots[i].cooldownRemaining -= tc.fDt`
- `stageWindow > 0` 도 `tc.fDt` 만큼 감소. `currentStage` 리셋 정책은 champion별 Layer 2 에서 확정
- `activeSkillSlot` / `activeSkillElapsed` 필드는 현재 없음. Layer 2 cast timing 은 `ActiveCastComponent` 또는 `CastRuntimeComponent` 신규로 분리

#### B-4: `Shared/GameSim/Systems/DamageQueueSystem.h/.cpp` (Layer 2 — Layer 1 은 stub)
- Layer 1: 빈 함수 (`Execute(world, tc)` no-op)
- Layer 2: `DamageRequestComponent` 큐 → `HealthComponent.fCurrent -= damage`

#### B-5: `Shared/GameSim/Systems/DeathSystem.h/.cpp` (★ Layer 1 필수 — HP 0 처리)
- `CDeathSystem::Execute(world, tc)` — `HealthComponent.fCurrent <= 0` → `bIsDead = true` + Move/Skill 차단
- HP 회복 디버그 키 시 `bIsDead = false`

#### B-6: `CPendingHitSystem` 서버 포팅 — Layer 2 (Codex C1)
- 현재 Client/Yasuo 폴더에 박혀있음 → Layer 1 에서는 미포팅
- Layer 2 에서 `Shared/GameSim/Systems/PendingHitSystem.h/.cpp` 로 이전 + Yasuo 의존 분리

**합격**: `CGameRoom::Phase_SimulationSystems` 가 위 5개 system 호출해서 LNK2019 없음 + 1 client move command 가 server world 의 Transform 갱신.

---

## 4. Phase D-1 Server 본격 (10 작업 — D-1G0 추가)

### D-1A — Server.vcxproj Engine project reference (1h)

(v1 동일)

### D-1B — CIOCPCore 본격 (4h)

(v1 동일 — typo 수정 + `acceptSocket` 필드 + AcceptEx 흐름)

### D-1C — CFrameParser TryPop + Invalid 분리 (2h)

(v1 동일)

### D-1D — CSession 본격 (5h)

(v1 동일 — recv/send/disconnect public API)

### D-1E — CSession_Manager 신규 (3h)

(v1 동일)

### D-1F — CPacketDispatcher 신규 (3h)

(v1 동일)

### D-1G — CGameRoom 본격 (6h ★ v1 8h → 6h. Damage/Cooldown 은 D-0B 에서 박제됨)

**목표**: 30Hz tick + Phase chain + stable_sort drain.

(v1 §3.D-1G 핵심 인터페이스 그대로. 단 Phase_SimulationSystems 호출 대상은 D-0B 산출물:)

```cpp
void CGameRoom::Phase_SimulationSystems(TickContext& tc)
{
    // ★ Layer 1 (Move/HP) — D-0B 박제 후
    CStatSystem::Execute(m_world, tc);
    CBuffSystem::Execute(m_world, tc);
    CSkillCooldownSystem::Execute(m_world, tc);   // D-0B
    CMoveSystem::Execute(m_world, tc);            // D-0B
    CDeathSystem::Execute(m_world, tc);           // D-0B

    // ★ Layer 2 (선택)
    // CPendingHitSystem::Execute(m_world, tc);    // Layer 2 — Yasuo 분리 후
    // CDamageQueueSystem::Execute(m_world, tc);   // Layer 2
}
```

**`Phase_BroadcastSnapshot`** — 모든 session 에게 full snapshot send (AOI X). Layer 1 = Move/HP/Anim 만, Layer 2 = + Skill state.

### D-1G0 — ServerSimSubset 통합 검증 (★ v2 신규, D-0B 흡수 시 0h)

D-0B 와 D-1G 사이 검증 게이트:
- 단일 client connect → Move command → server tick → Transform 위치 변경 검증
- Snapshot 에 Move 결과 반영 검증

### D-1H — CSnapshotBuilder header 분리 + Build 본격 (2h ★ v1 3h → 2h)

**목표**: header 분리 + Layer 1 minimal Build.

**시그니처 보정 (★ Codex C3)**:
```cpp
// Server/Public/Game/SnapshotBuilder.h
class CSnapshotBuilder final
{
public:
    static std::unique_ptr<CSnapshotBuilder> Create();

    // ★ v2 — non-const CWorld& (DeterministicEntityIterator + GetComponent 가 non-const)
    flatbuffers::DetachedBuffer Build(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t rngState,
        u32_t lastAckedSeq,
        NetEntityId yourNetId);
};
```

**Layer 1 EntitySnapshot 직렬화 필드 (★ v2 축소)**:
- `netId`, `championId`, `team`, `level`
- `hp`, `mana`
- `posX/Y/Z`, `yaw`
- `moveSpeed`
- `animId`, `animPhaseFrame`
- (Layer 2 추가) `skillCooldowns[]`, `skillRanks[]`, `buffMask`, `statHash`

**합격**: 5v5 = 10 entity Layer 1 snapshot < 800B.

### D-1I — Server/Private/main.cpp bootstrap (1h)

(v1 동일)

### D-1J — Hello packet 박제 (★ v2 신규, 2h, Codex C8)

**목표**: Server 가 Session connect 시 `Hello` packet 자동 송신 → Client 가 본인 NetEntityId 인지.

**옵션**: **FlatBuffers `Hello.fbs` 추가**.

```fbs
// Shared/Schemas/Hello.fbs (★ v2 신규)
namespace Shared.Schema;

table Hello {
    sessionId:uint;
    yourNetId:uint;
    serverTick:ulong;
    serverTimeMs:ulong;
    championId:ubyte;       // Layer 1 — server 가 자동 할당 (Ezreal 디폴트)
    team:ubyte;             // 0=Blue, 1=Red
}

root_type Hello;
```

**codegen 갱신**:
- `Shared/Schemas/run_codegen.bat` 의 flatc 입력 목록에 `Hello.fbs` 추가
- 생성물: `Shared/Schemas/Generated/cpp/Hello_generated.h`, `go/hello/...`
- Server/Client 에서는 raw struct 대신 `VerifyHelloBuffer` 로 검증 후 apply

**Server flow**:
- `CSession_Manager::OnAccept` → `CGameRoom::OnSessionJoin(sid, entity)` → Server 가 `Hello` flatbuffer 빌드 → `WrapEnvelope(ePacketType::Hello)` → `Send`

**Client flow**:
- `FrameCallback` 의 `ePacketType::Hello` → `CSnapshotApplier::OnHello` 또는 `Scene_InGame::OnHello`
- 본인 NetEntityId 저장 + 본인 Champion entity 시각화 시작

**합격**: 1 client connect → 0.5s 내 본인 챔피언 화면 표시.

---

## 5. Phase D-2 Client 본격 (5 작업 — D-2C 콜백 패턴)

### D-2A — ClientNetwork 신규 (★ v2 파일명 보정, 5h, Codex C7)

**파일**: `Client/Public/Network/ClientNetwork.h` + `Client/Private/Network/ClientNetwork.cpp` (신규, **C 접두사 없는 파일명**)
**클래스**: `CClientNetwork` (C 접두사 유지)

(v1 §4.D-2A API 그대로. 파일명만 변경.)

### D-2B — CCommandSerializer 본격 (2h ★ v1 3h → 2h, Layer 1 은 Move 만)

**Layer 1 API**:
```cpp
class CCommandSerializer final
{
public:
    static std::unique_ptr<CCommandSerializer> Create();

    void SendMove(CClientNetwork& net, const Vec3& groundPos);

    // Layer 2 (별도 사이클)
    // void SendCastSkill(...);
    // void SendBasicAttack(...);

private:
    CCommandSerializer() = default;
    std::vector<u8_t> BuildCommandBatch(const std::vector<GameCommandWire>& wires);
    u32_t m_nextSequenceNum = 1;
    u64_t m_clientTick = 0;
};
```

**합격**: 우클릭 → server log 에 `EnqueueCommand` 호출.

### D-2C — CSnapshotApplier 본격 (★ v2 OnNewEntity 콜백 패턴, 4h, Codex C4)

**v1 의 위험**: SnapshotApplier 가 직접 RenderComponent / ModelRenderer 생성 → Scene 이미 수동 ECS Champion 생성 경로와 충돌.

**v2 해결 — 콜백 패턴**:

```cpp
// Client/Public/Network/SnapshotApplier.h
class CSnapshotApplier final
{
public:
    static std::unique_ptr<CSnapshotApplier> Create();

    // ★ v2 — Scene 이 등록. SnapshotApplier 가 새 NetId 발견 시 호출
    using OnNewEntityFn = std::function<EntityID(
        u32_t netId, u8_t championId, u8_t team)>;
    void SetOnNewEntityCallback(OnNewEntityFn fn);

    // FrameCallback → 호출
    void OnHello(CWorld& world, EntityIdMap& entityMap,
                 const u8_t* payload, u32_t len);

    void OnSnapshot(CWorld& world, EntityIdMap& entityMap,
                    const u8_t* payload, u32_t len);

    u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }

private:
    CSnapshotApplier() = default;

    // 새 NetId → callback 호출 (Scene 이 CreateECSChampion)
    EntityID OnNewEntity(u32_t netId, u8_t championId, u8_t team);

    // 기존 entity 갱신 (Layer 1 = Transform/Health/Mana/Anim 만)
    void ApplyToExisting(CWorld& world, EntityID e,
                         const Shared::Schema::EntitySnapshot* es);

    OnNewEntityFn m_onNewEntity;
    u64_t m_lastServerTick = 0;
    std::unordered_set<u32_t> m_seenNetIds;   // allowlist (sim 외 cache)
};
```

**Scene_InGame 에서 callback 등록**:
```cpp
m_pSnapshotApplier->SetOnNewEntityCallback(
    [this](u32_t netId, u8_t championId, u8_t team) -> EntityID {
        // Scene 의 기존 ECS Champion 생성 경로 활용
        eChampion champ = static_cast<eChampion>(championId);
        EntityID e = CreateECSChampion(champ, static_cast<eTeam>(team));
        m_pEntityIdMap->Bind(netId, e);
        return e;
    });
```

**합격**:
- 새 NetId snapshot 수신 → Scene 이 ECS Champion 생성 (RenderComponent + ModelRenderer 포함)
- 기존 entity → Transform/Health/Mana/Anim 갱신만

### D-2D — Scene_InGame 통합 (3h)

(v1 §4.D-2D 동일 + `OnNewEntityCallback` 등록 추가)

### D-2E — InputSystem 마이그 (2h)

(v1 §4.D-2E 동일 — Layer 1 은 Move 만 server-authoritative)

---

## 6. Phase D-3 검증 (3 작업, Layer 1)

### D-3A — 1 client localhost smoke (1h)

(v1 동일 — Hello → 본인 챔피언 표시 + Move 동작)

### D-3B — 2 client localhost Move sync smoke (2h ★ v2 — Move/HP 만)

**Layer 1 시나리오**:
1. Server 실행
2. Client A → Hello → 본인 챔피언 + (Client B 없으면 본인만)
3. Client B → Hello → 본인 + Client A 챔피언 둘 다 보임
4. Client A 우클릭 이동 → Client B 화면에 Client A 캐릭터 이동 (snapshot Move sync)
5. 디버그 키 (server 콘솔에서 HP set) → 양쪽 client HP UI 즉시 갱신
6. **스킬 시각화는 Layer 2** — Layer 1 검증 시 스킬 입력 무시 또는 server log 만

### D-3C — Tick jitter 측정 (1h)

(v1 동일)

---

## 7. Phase D-4 (선택) — Layer 2 Cast Event Echo (+15-25h)

### D-4A — `Shared/Schemas/Event.fbs` 확장 (3h)

현재 `Event.fbs` 는 이미 존재하며 `root_type EventPacket` 이다. Layer 2 에서는 root 를 갈아엎지 말고 기존 구조를 확장한다.

**현 구조 유지**:
- `EventKind::SkillCast` / `EventKind::Damage` / `EventKind::Death` 이미 존재
- `EventPacket.serverTick` 이 이미 있음
- `SkillCastEvent` 에 현재 없는 시각화 필드만 추가:
  - `groundPos:Vec3`
  - `direction:Vec3`
  - `castFrame:float`
  - 필요 시 `animKey:string` 은 후순위 (MVP 는 slot 기반)

**Batch 는 후순위**: MVP Layer 2 는 event 1개당 `EventPacket` 1개 송신. 대량 event batch 는 Sim-10 M3 이후 대역폭 최적화 때 추가.

### D-4B — Server `CGameRoom::EmitEvent` 박제 (3h)

- `CDefaultCommandExecutor::HandleCastSkill` 진입 시 `EmitEvent(CastEvent)`
- `Phase_BroadcastEvents` 신규 — pending event 들을 broadcast (`ePacketType::Event`)

### D-4C — Client `OnEvent` callback (3h)

- FrameCallback 에서 `ePacketType::Event` → EventApplier
- `EventKind::Cast` → `Visual::OnCastFrame_*_Visual` hook trigger (이즈리얼 FX 등)

### D-4D — `CPendingHitSystem` server 포팅 (5h)

- `Client/Private/GameObject/Champion/Yasuo/PendingHitSystem.cpp` → `Shared/GameSim/Systems/PendingHitSystem.cpp` 이전
- `CYasuoProjectileSystem` 의존 분리 → 일반 `eProjectileKind` 디스패치

### D-4E — `CDamageQueueSystem` 본격 + 검증 (3h)

- `DamageRequestComponent` 큐 → `HealthComponent.fCurrent -= damage` → DeathSystem 으로 cascade
- 검증: B 가 Q → A 화면에 Q FX + A HP 감소

### D-4F — 통합 smoke (3h)

- 2 client 양쪽 다 BA/Q/W/E/R 시각 + 데미지 동기화

---

## 8. 의존성 그래프 (★ v2)

```
Engine.vcxproj (/fp:precise) ✅
        │
        ▼
[D-0] Shared cpp link closure + ServerSimSubset
   ├─ D-0A Server.vcxproj 에 Shared cpp 편입
   └─ D-0B ServerSimSubset 5종 박제
        │
        ▼
Server.vcxproj (D-1A: Engine ref)
        │
        ├── D-1B IOCPCore                ┐
        ├── D-1C FrameParser              │ Transport-aware
        ├── D-1D Session                  │ (UDP M1 에서 폐기 가능)
        ├── D-1E Session_Manager          │
        ├── D-1F PacketDispatcher         ┘
        │
        ├── D-1G GameRoom                 ┐
        ├── D-1G0 ServerSimSubset 통합     │ ★ Transport-neutral
        ├── D-1H SnapshotBuilder          │ (UDP M1 에서 그대로 재사용)
        ├── D-1J Hello packet              │
        └── D-1I main.cpp                  ┘
        │
        ▼
WintersServer.exe ✅
        │
        ▼ (TCP 9000 listen)
        │
Client.vcxproj (/fp:precise) ✅
        │
        ├── D-2A ClientNetwork            ─── Transport-aware
        │
        ├── D-2B CommandSerializer        ┐
        ├── D-2C SnapshotApplier          │ ★ Transport-neutral
        ├── D-2D Scene_InGame 통합         │
        └── D-2E InputSystem 마이그         ┘
        │
        ▼
WintersGame.exe (2 instance) ✅
        │
        ▼
   D-3 Layer 1 검증 (Move/HP sync) → 합격 = MVP
        │
        ▼ (선택)
   [D-4] Layer 2 — Cast Event Echo (FX 시각화)
```

---

## 9. 시간 견적 (★ v2)

### Layer 1 (필수 MVP — Move/HP sync)

| Phase | 작업 | 시간 |
|---|---|---|
| **D-0A** | Server.vcxproj 에 Shared cpp 편입 | 3h |
| **D-0B** | ServerSimSubset 5종 (CommandExecutor/Move/SkillCooldown/DamageQueue stub/Death) | 8h |
| **D-1A** | Server.vcxproj Engine reference | 1h |
| **D-1B** | IOCPCore | 4h |
| **D-1C** | FrameParser TryPop | 2h |
| **D-1D** | Session | 5h |
| **D-1E** | Session_Manager | 3h |
| **D-1F** | PacketDispatcher | 3h |
| **D-1G** | GameRoom (Tick + stable_sort + Move/Death only) | 6h |
| **D-1H** | SnapshotBuilder (Layer 1 필드만, non-const) | 2h |
| **D-1I** | main.cpp bootstrap | 1h |
| **D-1J** | Hello packet (Hello.fbs + Server emit + Client apply) | 2h |
| **D-2A** | ClientNetwork (TCP) | 5h |
| **D-2B** | CommandSerializer (Move 만) | 2h |
| **D-2C** | SnapshotApplier (★ OnNewEntity callback) | 4h |
| **D-2D** | Scene_InGame 통합 + callback 등록 | 3h |
| **D-2E** | InputSystem 마이그 (Move 만 server-authoritative) | 2h |
| **D-3A** | 1 client smoke | 1h |
| **D-3B** | 2 client Move sync smoke | 2h |
| **D-3C** | Jitter 측정 | 1h |
| **Layer 1 합계** | | **~58h** |

### Layer 2 (선택 — Cast Event Echo)

| Phase | 작업 | 시간 |
|---|---|---|
| **D-4A** | 기존 Event.fbs 확장 | 3h |
| **D-4B** | Server EmitEvent + broadcast | 3h |
| **D-4C** | Client EventApplier + Visual hook trigger | 3h |
| **D-4D** | PendingHitSystem server 포팅 (Yasuo 분리) | 5h |
| **D-4E** | DamageQueueSystem 본격 + 검증 | 3h |
| **D-4F** | 통합 smoke | 3h |
| **Layer 2 합계** | | **~20h** |

**Layer 1 + 2 = ~78h**. Layer 2 시각화까지 포함하면 기존 60-70h 감보다 약간 상향해서 보는 편이 안전하다.

---

## 10. 종료 조건 (Layer 1 = MVP)

### Layer 1 합격 (필수)
1. ✅ `WintersServer.exe` listen.
2. ✅ TCP framing — partial / sticky / bad header 처리.
3. ✅ Hello packet 으로 Client 가 본인 NetEntityId 인지.
4. ✅ CommandBatch (Move 만) FlatBuffers verify 후 GameRoom queue.
5. ✅ GameRoom 단일 thread 30Hz tick (jitter < 5ms).
6. ✅ stable_sort drain — 결정성 1차.
7. ✅ ServerSimSubset (Move/SkillCooldown/Death) 동작.
8. ✅ Snapshot full broadcast — Move/HP/Anim 동기화.
9. ✅ Client OnNewEntity callback → Scene 이 ECS Champion 생성.
10. ✅ 2 client Move sync 시각 검증.
11. ✅ HP 디버그 변경 → 양쪽 client UI 갱신.
12. ✅ 5분 무중단 + connect/disconnect leak 0.
13. ✅ `/fp:precise` + `stable_sort` + 회귀 grep allowlist 외 0 hit.
14. ✅ `grep -rn "WSARecv\|WSASend\|recvfrom\|sendto" Server/Public/Game/ Server/Public/Security/ Shared/` = 0 hit (transport-neutral 강제).

### Layer 2 합격 (선택)
15. ✅ B 스킬 → A 화면에 FX 시각화.
16. ✅ Damage 적용 → 양쪽 client HP 동기화.

---

## 11. 합격 후 다음 단계 (★ A' boundary)

| 다음 사이클 | 영역 | 시간 | 재사용 비율 |
|---|---|---|---|
| **Sim-10 v2 M1** | UDP transport 마이그 | 50h | Transport-aware 6개만 갈아끼움. **Transport-neutral 6개는 그대로**. 단 UDP core/reliability 전초 작업 때문에 M1 견적 50h 는 유지 |
| Sim-10 v2 M2 | Reliability 3-channel + Fragment | 30h | |
| Sim-10 v2 M3 | Snapshot Delta + AOI | 50h | Layer 1 의 SnapshotBuilder 에 delta 추가 |
| Sim-10 v2 M5 | Prediction (시각 lag 제거) | 60h | |
| Sim-11 | Encryption | 30h | |

**A' boundary 의 가치**: M1 UDP 마이그 시 GameRoom/Snapshot/Serializer/Applier/Executor/ServerSimSubset 6개 = 약 28h 분량 (D-0B + D-1G + D-1H + D-2B + D-2C) **그대로 재사용**. UDP M1 전체 견적을 없애는 것이 아니라 gameplay-sync 재디버깅을 줄이는 효과다.

---

## 12. 위험 / 롤백 (★ v2 추가)

### 12.1 위험 (v1 + v2 보정)
| # | 위험 | 완화 |
|---|---|---|
| **R1** | Engine.dll link 누락 → LNK2019 | D-1A 에서 Engine project reference |
| **R2** | sessionId race | atomic |
| **R3** | Snapshot send queue overflow | MVP 무한, 운영 시 한도 |
| **R4** | InputSystem local-authoritative 충돌 | D-2E 에서 기존 코드 비활성화 |
| **R5** | Hello → InGame 타이밍 race | Hello 우선 처리 후 InGame 진입 |
| **★ R6** | Shared cpp 누락 → LNK2019 (D-0A 누락 시) | **D-0 부터 시작 — Shared cpp 편입 검증 게이트** |
| **★ R7** | `CPendingHitSystem` Yasuo 의존 → 서버 link 깨짐 | Layer 1 은 미포팅. Layer 2 D-4D 에서 분리 박제 |
| **★ R8** | `SnapshotBuilder` const 시그니처 → `GetComponent` 호출 컴파일 에러 | D-1H 에서 non-const 시그니처 |
| **★ R9** | `SnapshotApplier` 가 RenderComponent 직접 생성 → Scene 수동 경로와 충돌 | D-2C OnNewEntity callback 패턴 |
| **★ R10** | Transport boundary 위반 (Game/Security/Shared 가 WSARecv 호출) | D-3 grep 게이트 — 0 hit 강제 |
| **★ R11** | 현재 `SkillStateComponent` 필드명 불일치 (`cooldowns[]` 없음) | D-0B 는 `slots[i].cooldownRemaining` 기준으로 구현 |
| **★ R12** | `Event.fbs` root 교체 시 기존 generated 코드/API 파손 | Layer 2 는 기존 `EventPacket` root 유지 + 필드 확장만 |

### 12.2 롤백
- 각 Phase 별 commit. D-0 (Shared cpp 편입) 가 가장 큰 vcxproj 변경 — 별도 branch 권장.
- D-1G (GameRoom) 와 D-2C (SnapshotApplier callback) 가 큰 코드 변경 — 별도 branch.

---

## 13. v1 → v2 변경 한 줄

**v1 의 "TCP MVP 52h" → v2 의 "Layer 1 (Move/HP sync) ~58h + Layer 2 (Cast event echo) +20h = 78h". Codex 5+3건 + repo 실측 3건 보정 ① ServerSimSubset 5종 D-0B 박제 ② MVP 합격 기준 layer 분리 (snapshot 만으로 FX 안 됨) ③ SnapshotBuilder non-const 시그니처 ④ SnapshotApplier OnNewEntity callback ⑤ Server.vcxproj Shared cpp 편입 D-0A ⑥ steady_clock 허용 + sim wall-clock 금지 ⑦ ClientNetwork 파일명 ⑧ Hello FlatBuffers ⑨ SkillState 실제 필드명 반영 ⑩ MoveTargetComponent 신규 명시 ⑪ 기존 Event.fbs root 유지. 사용자 A' 채택 — Transport boundary 명시 (6 transport-aware vs 6 transport-neutral) + grep 강제 검증. UDP 마이그 시 transport-neutral 6개 28h 분량 그대로 재사용 → gameplay-sync 재디버깅 감소.**
