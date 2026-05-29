# Phase 04a v2 — D-2 Sub-plan: Client Transport (TCP)

**작성일**: 2026-04-30 (Codex 1차 검토 보정 2026-04-30)
**상위 문서**: `04a_MVP_2CLIENT_TCP_DEMO_v2.md` §5
**범위**: D-2A~D-2E Client 측 본격 — ClientNetwork (신규) + CommandSerializer (본격) + SnapshotApplier (callback) + Scene_InGame 통합 + UpdateCombatInput 마이그
**합격**: Hello → 본인 챔피언 시각화 + 우클릭 이동 → server tick → 다른 client 화면 반영

**한 줄**: **Client 측 TCP transport (ClientNetwork) + transport-neutral serializer/applier 박제. SnapshotApplier 는 OnNewEntity callback 패턴 — Scene 이 직접 ECS Champion 생성. UDP 마이그 시 ClientNetwork 만 갈아끼움.**

---

## ★ Prerequisite (D-1J 선행 필수)

D-2C 의 SnapshotApplier 는 `Hello_generated.h` 를 include 함. 따라서 **D-1J (Hello.fbs +
run_codegen.bat 확장 + Hello_generated.h 산출)** 가 D-2C 박제 직전까지 완료되어야 함.

D-1J 가 누락된 채 D-2C 빌드 시 `cannot open Hello_generated.h` 에러. D-2 단독 진입 시
이 prereq 를 본 sub-plan §X 에 흡수해야 함 (현재는 D-1J 의존 가정).

---

## ★ Codex 1차 검토 보정 요약 (2026-04-30)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P1-1 | (false positive) "코드블록 주석 먹힘" 주장 | line 67/255/497 | ✅ 검증 결과 정상. 패치 없음 |
| P1-2 | Client.vcxproj 편입 diff 부재 | §1.3 (신규 추가) | ClCompile/ClInclude 명시 |
| P1-3 | Hello.fbs / Hello_generated.h prereq | 본 §Prerequisite + D-1J 참조 | 명시 |
| P1-4 | Scene/API 이름 불일치 (`m_EntityMap` / `CreateECSChampionFromDef` / `current`) | §3.2, §4.2 | 실제 명칭으로 보정 |
| P1-5 | InputSystem.cpp 는 #if 0 비활성 | §5 | 대상을 `Scene_InGame::UpdateCombatInput` + `OnUpdate 이동 블록` 으로 변경 |
| P1-6 | SyncECSTransformsFromLegacy 두 번 호출 + 로컬 이동이 snapshot 덮음 | §5 (확장) | 네트워크 모드 분기 + Legacy 방향 반전 가이드 |
| P2-7 | CClientNetwork 에 WSAStartup/WSACleanup 부재 + header 순서 | §1.1, §1.2 | winsock2.h 먼저 + WSAStartup/WSACleanup 추가 |
| P2-8 | m_seenNetIds spawn 성공 전 insert (실패 시 재시도 차단) | §3.2 | spawn 성공 후 insert/Bind |

---

## 1. D-2A — ClientNetwork 신규 (5h)

### 1.1 `Client/Public/Network/ClientNetwork.h` (★ 신규, C 없는 파일명)

★ **Codex 보정 (P2-7)**: `Defines.h` 가 Windows.h 를 끌어오면 winsock2.h 와 충돌
(WinSock 매크로 / sockaddr 중복 정의). 반드시 `winsock2.h` + `ws2tcpip.h` 를 **최상단**
에 두고, 그 다음에 `Defines.h` / 기타 include.

```cpp
#pragma once

// ★ winsock2.h 는 Windows.h 보다 먼저 — Defines.h 가 Windows.h 를 transit include 시 충돌 방지
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Defines.h"
#include "Shared/Network/PacketEnvelope.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class CClientNetwork final
{
public:
    static std::unique_ptr<CClientNetwork> Create();
    ~CClientNetwork();

    bool Connect(const char* host, u16_t port);
    void Disconnect();

    bool Send(std::vector<u8_t> packet);

    // FrameCallback — 메인 thread 에서 PumpReceivedFrames() 호출 시 invoke
    using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;
    void SetFrameCallback(FrameCallback fn);

    void PumpReceivedFrames();

    bool IsConnected() const { return m_bConnected.load(std::memory_order_relaxed); }

    u32_t GetMyNetEntityId() const            { return m_myNetId; }
    void  SetMyNetEntityId(u32_t id)          { m_myNetId = id; }
    u32_t GetMySessionId() const              { return m_mySessionId; }
    void  SetMySessionId(u32_t sid)           { m_mySessionId = sid; }

private:
    CClientNetwork() = default;

    void RecvThread();

    SOCKET            m_socket = INVALID_SOCKET;
    std::thread       m_recvThread;
    std::atomic<bool> m_bRunning{ false };
    std::atomic<bool> m_bConnected{ false };

    std::vector<u8_t> m_recvAccum;   // recv worker 내부 버퍼

    // ★ recv worker → main thread marshal 큐
    std::mutex                                                         m_pendingMutex;
    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>>     m_pendingFrames;

    FrameCallback m_callback;
    u32_t         m_myNetId = 0;
    u32_t         m_mySessionId = 0;
};
```

### 1.2 `Client/Private/Network/ClientNetwork.cpp` (전문)

★ **Codex 보정 (P2-7)**: WSAStartup / WSACleanup 추가. Server 와 달리 Client 는 별도
WinSock 초기화 위치가 없어 ClientNetwork 가 lifecycle 관리 (간단 ref-count).

```cpp
#include "Network/ClientNetwork.h"
#include <iostream>
#include <cstring>
#include <atomic>

namespace
{
    // ★ WSAStartup ref-count — 동일 process 내 다중 ClientNetwork 가능성 대비
    std::atomic<int> g_WsaRefCount{ 0 };

    bool EnsureWsaInit()
    {
        if (g_WsaRefCount.fetch_add(1, std::memory_order_acq_rel) == 0)
        {
            WSADATA wsa{};
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            {
                g_WsaRefCount.store(0, std::memory_order_release);
                return false;
            }
        }
        return true;
    }

    void ReleaseWsa()
    {
        if (g_WsaRefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            WSACleanup();
    }
}

std::unique_ptr<CClientNetwork> CClientNetwork::Create()
{
    if (!EnsureWsaInit()) return nullptr;
    return std::unique_ptr<CClientNetwork>(new CClientNetwork());
}

CClientNetwork::~CClientNetwork()
{
    Disconnect();
    ReleaseWsa();
}

bool CClientNetwork::Connect(const char* host, u16_t port)
{
    if (m_bConnected) return true;

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    BOOL on = TRUE;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));

    m_bRunning = true;
    m_bConnected = true;
    m_recvThread = std::thread(&CClientNetwork::RecvThread, this);

    std::cout << "[ClientNetwork] connected " << host << ":" << port << "\n";
    return true;
}

void CClientNetwork::Disconnect()
{
    if (!m_bRunning.exchange(false)) return;
    m_bConnected = false;

    if (m_socket != INVALID_SOCKET)
    {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_recvThread.joinable()) m_recvThread.join();
}

bool CClientNetwork::Send(std::vector<u8_t> packet)
{
    if (!m_bConnected || m_socket == INVALID_SOCKET || packet.empty()) return false;

    // 단순 blocking send (MVP — 동시 호출은 InputSystem 단일 thread 가정)
    int total = 0;
    while (total < static_cast<int>(packet.size()))
    {
        int sent = send(m_socket,
            reinterpret_cast<const char*>(packet.data() + total),
            static_cast<int>(packet.size() - total),
            0);
        if (sent == SOCKET_ERROR)
        {
            m_bConnected = false;
            return false;
        }
        total += sent;
    }
    return true;
}

void CClientNetwork::SetFrameCallback(FrameCallback fn)
{
    m_callback = std::move(fn);
}

void CClientNetwork::PumpReceivedFrames()
{
    if (!m_callback) return;

    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> drained;
    {
        std::lock_guard lk(m_pendingMutex);
        drained.swap(m_pendingFrames);
    }

    for (auto& [type, seq, payload] : drained)
        m_callback(type, seq, payload.data(), static_cast<u32_t>(payload.size()));
}

void CClientNetwork::RecvThread()
{
    char buf[8192]{};

    while (m_bRunning)
    {
        int n = recv(m_socket, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            m_bConnected = false;
            break;
        }

        m_recvAccum.insert(m_recvAccum.end(),
            reinterpret_cast<u8_t*>(buf),
            reinterpret_cast<u8_t*>(buf) + n);

        // ★ frame 추출 (Server CFrameParser 와 동일 로직, 간략화)
        for (;;)
        {
            if (m_recvAccum.size() < sizeof(PacketHeader)) break;

            PacketHeader hdr{};
            std::memcpy(&hdr, m_recvAccum.data(), sizeof(PacketHeader));

            if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion ||
                hdr.payloadSize > kMaxPacketPayloadSize)
            {
                m_recvAccum.clear();
                m_bConnected = false;
                break;
            }

            const u32_t total = sizeof(PacketHeader) + hdr.payloadSize;
            if (m_recvAccum.size() < total) break;

            std::vector<u8_t> payload(
                m_recvAccum.begin() + sizeof(PacketHeader),
                m_recvAccum.begin() + total);

            {
                std::lock_guard lk(m_pendingMutex);
                m_pendingFrames.emplace_back(
                    static_cast<ePacketType>(hdr.type), hdr.sequence, std::move(payload));
            }

            m_recvAccum.erase(m_recvAccum.begin(), m_recvAccum.begin() + total);
        }
    }
}
```

---

## 1.3 Client.vcxproj 편입 diff (★ Codex P1-2 신규)

현재 `Client/Include/Client.vcxproj` 에는 `SnapshotApplier.cpp` 만 편입되어 있고
`ClientNetwork.cpp` / `CommandSerializer.cpp` 는 미편입. 새 cpp 만 만들면 빌드 대상이
안 되므로 vcxproj 도 함께 수정해야 함.

### 1.3.1 추가 ClCompile (`<ItemGroup>` ClCompile 그룹 안)

기존 `..\Private\Network\SnapshotApplier.cpp` 항목 근처에 추가:

```xml
<ClCompile Include="..\Private\Network\ClientNetwork.cpp" />
<ClCompile Include="..\Private\Network\CommandSerializer.cpp" />
<!-- 기존 -->
<ClCompile Include="..\Private\Network\SnapshotApplier.cpp" />
```

### 1.3.2 추가 ClInclude (`<ItemGroup>` ClInclude 그룹 안)

```xml
<ClInclude Include="..\Public\Network\ClientNetwork.h" />
<!-- 기존 -->
<ClInclude Include="..\Public\Network\CommandSerializer.h" />
<ClInclude Include="..\Public\Network\SnapshotApplier.h" />
```

### 1.3.3 합격
- ✅ Visual Studio 솔루션 reload 후 Client 프로젝트가 새 cpp/h 인식
- ✅ `Client/Bin/Intermediate/Debug/ClientNetwork.obj` 생성
- ✅ filters 에도 `Network` 카테고리로 분류 (선택, IDE 가독성)

---

## 2. D-2B — CommandSerializer 본격 (2h, Layer 1 = Move 만)

### 2.1 `Client/Public/Network/CommandSerializer.h` (전문, stub → 본격)

```cpp
#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "Shared/GameSim/EntityIdMap.h"

#include <memory>
#include <vector>

class CClientNetwork;
struct GameCommandWire;

class CCommandSerializer final
{
public:
    static std::unique_ptr<CCommandSerializer> Create();

    // ★ Layer 1 — Move 만
    void SendMove(CClientNetwork& net, const Vec3& groundPos);

    // Layer 2 (본격은 D-4 Cast Event Echo 사이클)
    // void SendCastSkill(CClientNetwork& net, u8_t slot, NetEntityId targetNet,
    //                    const Vec3& groundPos, const Vec3& direction);
    // void SendBasicAttack(CClientNetwork& net, NetEntityId targetNet);

private:
    CCommandSerializer() = default;

    std::vector<u8_t> BuildCommandBatch(const std::vector<GameCommandWire>& wires);

    u32_t m_nextSequenceNum = 1;
    u64_t m_clientTick = 0;
};
```

### 2.2 `Client/Private/Network/CommandSerializer.cpp` (전문)

```cpp
#include "Network/CommandSerializer.h"
#include "Network/ClientNetwork.h"

#include "Shared/Network/PacketEnvelope.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"

#include <flatbuffers/flatbuffers.h>

std::unique_ptr<CCommandSerializer> CCommandSerializer::Create()
{
    return std::unique_ptr<CCommandSerializer>(new CCommandSerializer());
}

void CCommandSerializer::SendMove(CClientNetwork& net, const Vec3& groundPos)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::Move;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.clientTick = ++m_clientTick;
    wire.groundPos = groundPos;
    wire.targetNet = NULL_NET_ENTITY;

    std::vector<u8_t> batchPayload = BuildCommandBatch({ wire });
    auto wrapped = WrapEnvelope(
        ePacketType::CommandBatch, wire.sequenceNum,
        batchPayload.data(), static_cast<u32_t>(batchPayload.size()));
    net.Send(std::move(wrapped));
}

std::vector<u8_t> CCommandSerializer::BuildCommandBatch(
    const std::vector<GameCommandWire>& wires)
{
    flatbuffers::FlatBufferBuilder fbb(256);

    std::vector<flatbuffers::Offset<Shared::Schema::CommandPacket>> cmdVec;
    cmdVec.reserve(wires.size());

    for (const auto& w : wires)
    {
        Shared::Schema::Vec3 g{ w.groundPos.x, w.groundPos.y, w.groundPos.z };
        Shared::Schema::Vec3 d{ w.direction.x, w.direction.y, w.direction.z };

        cmdVec.push_back(Shared::Schema::CreateCommandPacket(
            fbb,
            static_cast<Shared::Schema::CommandKind>(w.kind),
            w.sequenceNum,
            w.clientTick,
            w.slot,
            w.targetNet,
            &g, &d,
            w.itemId, /*pad=*/0));
    }

    auto vecOffset = fbb.CreateVector(cmdVec);
    auto batch = Shared::Schema::CreateCommandBatch(fbb, vecOffset, /*timestamp=*/0);
    fbb.Finish(batch);

    auto* data = fbb.GetBufferPointer();
    auto  size = fbb.GetSize();
    return std::vector<u8_t>(data, data + size);
}
```

---

## 3. D-2C — SnapshotApplier 본격 (4h, ★ OnNewEntity callback)

★ **현재 상태 (2026-04-30 시점)**:
- `Client/Public/Network/SnapshotApplier.h` 가 이미 존재 — stub API (`ApplyBytes`, `Apply`)
  로 rng state 만 set. 본격 API 로 **전면 교체** 필요. 기존 caller (`Scene_InGame.cpp:1916`
  의 `m_pSnapshotApplier->ApplyBytes(...)`) 도 D-2D 통합 시 새 API 로 마이그.
- `Client/Public/Network/CommandSerializer.h` 도 stub (`m_LastPayload` 만) — 전면 교체.

### 3.1 `Client/Public/Network/SnapshotApplier.h` (전문, stub → 본격)

```cpp
#pragma once
#include "Defines.h"
#include "ECS/Entity.h"

#include <functional>
#include <memory>
#include <unordered_set>

class CWorld;
class EntityIdMap;

class CSnapshotApplier final
{
public:
    static std::unique_ptr<CSnapshotApplier> Create();

    // ★ Codex C4 — Scene 등록 콜백. 새 NetId 발견 시 호출
    using OnNewEntityFn = std::function<EntityID(
        u32_t netId, u8_t championId, u8_t team)>;
    void SetOnNewEntityCallback(OnNewEntityFn fn) { m_onNewEntity = std::move(fn); }

    // FrameCallback 으로 호출
    void OnHello(CWorld& world, EntityIdMap& entityMap,
                 const u8_t* payload, u32_t len,
                 u32_t* outMyNetId, u32_t* outMySessionId);

    void OnSnapshot(CWorld& world, EntityIdMap& entityMap,
                    const u8_t* payload, u32_t len);

    u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }

private:
    CSnapshotApplier() = default;

    OnNewEntityFn               m_onNewEntity;
    u64_t                       m_lastServerTick = 0;
    std::unordered_set<u32_t>   m_seenNetIds;   // allowlist (sim 외 cache)
};
```

### 3.2 `Client/Private/Network/SnapshotApplier.cpp` (전문)

```cpp
#include "Network/SnapshotApplier.h"

#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <flatbuffers/flatbuffers.h>

std::unique_ptr<CSnapshotApplier> CSnapshotApplier::Create()
{
    return std::unique_ptr<CSnapshotApplier>(new CSnapshotApplier());
}

void CSnapshotApplier::OnHello(CWorld& world, EntityIdMap& entityMap,
    const u8_t* payload, u32_t len,
    u32_t* outMyNetId, u32_t* outMySessionId)
{
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyHelloBuffer(verifier)) return;

    const auto* hello = Shared::Schema::GetHello(payload);
    if (outMyNetId)     *outMyNetId     = hello->yourNetId();
    if (outMySessionId) *outMySessionId = hello->sessionId();

    // ★ Codex 보정 (P2-8): spawn 성공 후 insert/Bind. 실패 시 다음 snapshot 에서
    //   재시도 가능. 기존 코드는 m_seenNetIds.insert 가 spawn 성공 전 실행되어
    //   callback 실패 시 영구 차단되는 사고.
    if (m_onNewEntity && m_seenNetIds.find(hello->yourNetId()) == m_seenNetIds.end())
    {
        EntityID e = m_onNewEntity(
            hello->yourNetId(), hello->championId(), hello->team());
        if (e != NULL_ENTITY)
        {
            entityMap.Bind(hello->yourNetId(), e);
            m_seenNetIds.insert(hello->yourNetId());
        }
    }
}

void CSnapshotApplier::OnSnapshot(CWorld& world, EntityIdMap& entityMap,
    const u8_t* payload, u32_t len)
{
    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifySnapshotBuffer(verifier)) return;

    const auto* snap = Shared::Schema::GetSnapshot(payload);
    m_lastServerTick = snap->serverTick();

    if (!snap->entities()) return;

    for (const auto* es : *snap->entities())
    {
        const u32_t netId = es->netId();
        EntityID e = entityMap.FromNet(netId);

        // 새 entity → callback 으로 Scene 이 ECS Champion 생성
        if (e == NULL_ENTITY)
        {
            if (!m_onNewEntity) continue;
            // ★ Codex 보정 (P2-8): spawn 성공 후 insert. 실패 시 재시도 보장.
            if (m_seenNetIds.find(netId) != m_seenNetIds.end()) continue;

            e = m_onNewEntity(netId, es->championId(), es->team());
            if (e == NULL_ENTITY) continue;
            entityMap.Bind(netId, e);
            m_seenNetIds.insert(netId);
        }

        // 기존 entity 갱신 — Layer 1 = Transform/Health/Anim
        if (world.HasComponent<TransformComponent>(e))
        {
            auto& tf = world.GetComponent<TransformComponent>(e);
            tf.SetPosition({ es->posX(), es->posY(), es->posZ() });
            tf.m_bLocalDirty = true;
            tf.m_bWorldDirty = true;
        }
        if (world.HasComponent<HealthComponent>(e))
        {
            // ★ Codex 보정 (P1-4): 실제 필드명 fCurrent / fMaximum / bIsDead
            auto& hp = world.GetComponent<HealthComponent>(e);
            hp.fCurrent = es->hp();
            if (hp.fCurrent <= 0.f) hp.bIsDead = true;
            else                    hp.bIsDead = false;
        }
        if (world.HasComponent<StatComponent>(e))
        {
            auto& s = world.GetComponent<StatComponent>(e);
            s.moveSpeed = es->moveSpeed();
        }
        // animId / animPhaseFrame — Layer 2 (Animator 통합)
    }
}
```

---

## 4. D-2D — Scene_InGame 통합 (3h)

### 4.1 `Client/Public/Scene/Scene_InGame.h` (멤버 추가)

★ **현재 상태 (2026-04-30 시점)**: `Scene_InGame.h:143-145` 이미 다음 멤버 존재:
- `unique_ptr<EntityIdMap>      m_pEntityIdMap;`         ← **m_EntityMap 이 아님**
- `unique_ptr<CSnapshotApplier> m_pSnapshotApplier;`
- `unique_ptr<CCommandSerializer> m_pCommandSerializer;`

신규 추가만 필요: `m_pNetwork` 한 개.

```cpp
// 기존 멤버 (Scene_InGame.h:143-145) — 그대로 유지:
//   unique_ptr<EntityIdMap>            m_pEntityIdMap;
//   unique_ptr<CSnapshotApplier>       m_pSnapshotApplier;
//   unique_ptr<CCommandSerializer>     m_pCommandSerializer;

// ★ Codex 보정 (P1-4): 신규 추가
private:
    std::unique_ptr<CClientNetwork>    m_pNetwork;
```

### 4.2 `Client/Private/Scene/Scene_InGame.cpp` (★ 신규 코드 블록)

#### `OnEnter()` 끝부분에 추가:

★ **Codex 보정 (P1-4)**: API 이름 보정
- `m_EntityMap` → `*m_pEntityIdMap` (실제 멤버는 unique_ptr)
- `CreateECSChampionFromDef(m_World, champ, tm)` → `CreateECSChampion(champ, tm)` (실제
  시그니처: `Scene_InGame.h:293`, `EntityID CreateECSChampion(eChampion id, eTeam team)`)
- `Scene_InGame.cpp:115-117` 에서 이미 `m_pSnapshotApplier`/`m_pCommandSerializer` 가
  `make_unique<>()` 로 생성됨. 본 plan 의 Create() 패턴은 D-2 박제 시 멤버를 **재생성** —
  기존 line 115-117 은 OnEnter 에서 이 코드로 교체 (주의: factory 패턴 적용).

```cpp
// ★ 04a v2 D-2D — Network 초기화
//   기존 Scene_InGame.cpp:115-117 의 make_unique<>() 호출은 본 코드로 교체.
m_pNetwork           = CClientNetwork::Create();
m_pCommandSerializer = CCommandSerializer::Create();
m_pSnapshotApplier   = CSnapshotApplier::Create();

// OnNewEntity callback 등록
m_pSnapshotApplier->SetOnNewEntityCallback(
    [this](u32_t netId, u8_t championId, u8_t team) -> EntityID
    {
        (void)netId;
        eChampion champ = static_cast<eChampion>(championId);
        eTeam     tm    = static_cast<eTeam>(team);

        // ★ Scene 의 기존 ECS Champion 생성 경로 활용 (Scene_InGame.h:293)
        EntityID e = CreateECSChampion(champ, tm);
        return e;
    });

// FrameCallback — Hello / Snapshot 분기
m_pNetwork->SetFrameCallback(
    [this](ePacketType type, u32_t seq, const u8_t* payload, u32_t len)
    {
        (void)seq;
        switch (type)
        {
            case ePacketType::Hello:
            {
                u32_t myNetId = 0;
                u32_t mySid = 0;
                // ★ Codex P1-4 — m_EntityMap 가 아닌 *m_pEntityIdMap
                m_pSnapshotApplier->OnHello(m_World, *m_pEntityIdMap,
                    payload, len, &myNetId, &mySid);
                m_pNetwork->SetMyNetEntityId(myNetId);
                m_pNetwork->SetMySessionId(mySid);
                break;
            }
            case ePacketType::Snapshot:
                m_pSnapshotApplier->OnSnapshot(m_World, *m_pEntityIdMap, payload, len);
                break;
            default:
                break;
        }
    });

// TCP connect — GUI 앱이라 std::cout 대신 OutputDebugStringA
if (!m_pNetwork->Connect("127.0.0.1", 9000))
    OutputDebugStringA("[Scene_InGame] Server connect failed.\n");
else
    OutputDebugStringA("[Scene_InGame] Server connected (127.0.0.1:9000)\n");
```

#### `OnUpdate(dt)` 시작부에 추가:

```cpp
// ★ 04a v2 D-2D — 매 frame frame pump (main thread 단일 호출 강제 — ECS race 방지)
if (m_pNetwork && m_pNetwork->IsConnected())
    m_pNetwork->PumpReceivedFrames();
```

#### `OnExit()` 에 추가:

```cpp
if (m_pNetwork) m_pNetwork->Disconnect();
m_pSnapshotApplier.reset();
m_pCommandSerializer.reset();
m_pNetwork.reset();
```

---

## 5. D-2E — UpdateCombatInput / OnUpdate 이동 블록 → CommandSerializer hook (2h)

### 5.1 ★ Codex 보정 (P1-5): 대상 파일 변경

**기존 plan**: `Client/Private/GamePlay/InputSystem.cpp` 수정.
**현실**: `InputSystem.cpp` 는 line 1-2 가 `// [Phase T] R-4 비활성 — 전체 감쌈.` + `#if 0`
로 **완전 비활성**. 실제 우클릭 이동은 `Scene_InGame::UpdateCombatInput` (호출자) +
`Scene_InGame::OnUpdate` line 1520-1571 의 m_pPlayerTransform 기반 이동 블록에서 처리.

→ **수정 대상**: `Client/Private/Scene/Scene_InGame.cpp` 의:
1. `UpdateCombatInput(bool& outSkipGroundMove)` — Q/W/E/R 스킬 디스패치 (Layer 2 대상)
2. `OnUpdate(f32_t dt)` 의 line 1525 `if (... input.IsRButtonDown())` 블록 — 우클릭 이동
3. `OnUpdate` 의 line 1544-1571 — 로컬 보간 이동 (network 모드 시 분기 필요)

### 5.2 우클릭 이동 마이그 — `Scene_InGame::OnUpdate` line 1525 블록

**수정 전 (Scene_InGame.cpp:1525-1542)**:
```cpp
if (!bImGuiMouse && !bSkipGroundMove && !bActionLocked && input.IsRButtonDown())
{
    Vec3 ground = input.GetMouseGroundPos(...);
    if (fabsf(ground.x) + fabsf(ground.z) > 0.001f)
        m_vPlayerDest = ground;
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
    {
        auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
        agent.vTarget = ground;
        agent.bHasGoal = true;
        agent.bPathDirty = true;
    }
}
```

**수정 후 (★ server authoritative + 로컬 fallback 보존)**:
```cpp
if (!bImGuiMouse && !bSkipGroundMove && !bActionLocked && input.IsRButtonDown())
{
    Vec3 ground = input.GetMouseGroundPos(...);
    if (fabsf(ground.x) + fabsf(ground.z) > 0.001f)
        m_vPlayerDest = ground;

    // ★ 04a v2 D-2E — network 모드 시 server authoritative
    const bool_t bNetworkActive = (m_pNetwork && m_pNetwork->IsConnected());
    if (bNetworkActive && m_pCommandSerializer)
    {
        m_pCommandSerializer->SendMove(*m_pNetwork, ground);
        // ★ Codex P1-6: 로컬 NavAgent 갱신 스킵 — server snapshot 으로 위치 권위 받음.
    }
    else
    {
        // 오프라인 / 미연결 fallback — 기존 로컬 권위 유지
        if (m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
        {
            auto& agent = m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
            agent.vTarget = ground;
            agent.bHasGoal = true;
            agent.bPathDirty = true;
        }
    }
}
```

### 5.3 ★ Codex 보정 (P1-6): legacy sync 방향 분기

**문제**: `Scene_InGame::OnUpdate` 가 `SyncECSTransformsFromLegacy()` 를 두 번 호출
(line 915, 926). 또한 line 1544-1571 의 로컬 보간 이동이 `m_pPlayerTransform` 을 직접
SetPosition. → snapshot 으로 갱신한 ECS Transform 이 Legacy → ECS sync 에서 덮이거나,
로컬 보간이 다음 frame 에 다시 덮어버림.

**해결**: network active 시 legacy sync 방향을 **반대로** 하거나, 로컬 보간 자체를 스킵.

#### 5.3.1 SyncECSTransformsFromLegacy 호출부 분기 (line 915, 926)

```cpp
{
    WINTERS_PROFILE_SCOPE("SyncECS");
    // ★ 04a v2 D-2E + Codex P1-6
    //   network 모드: ECS 가 권위. Legacy → ECS sync 스킵 (기존 호출 보존하면 server snapshot 덮음)
    //   오프라인 모드: 기존 그대로
    const bool_t bNetworkActive = (m_pNetwork && m_pNetwork->IsConnected());
    if (!bNetworkActive)
        SyncECSTransformsFromLegacy();
    else
        SyncPlayerEntityTransformFromECS();   // ECS → Legacy (반대 방향, 본인 entity 만)
}
```

(line 926 의 두 번째 호출도 동일 패턴 적용)

#### 5.3.2 line 1544-1571 로컬 보간 분기

```cpp
// ★ 04a v2 D-2E — network active 시 로컬 보간 스킵 (server snapshot 만 신뢰)
const bool_t bNetworkActive = (m_pNetwork && m_pNetwork->IsConnected());
if (!bNetworkActive)
{
    // 기존 로컬 보간 코드 (line 1544-1571)
    Vec3 cur = m_pPlayerTransform->GetPosition();
    Vec3 delta = { m_vPlayerDest.x - cur.x, 0.f, m_vPlayerDest.z - cur.z };
    // ... 보간 ...
}
else
{
    // network 모드: SyncPlayerEntityTransformFromECS 가 ECS Transform 을
    // m_pPlayerTransform 으로 push. m_bMoving 은 prev/cur 거리로 추정.
    Vec3 prev = m_pPlayerTransform->GetPosition();
    SyncPlayerEntityTransformFromECS();
    Vec3 cur = m_pPlayerTransform->GetPosition();
    f32_t moved = fabsf(cur.x - prev.x) + fabsf(cur.z - prev.z);
    m_bMoving = (moved > 0.001f);

    // 애니 전환은 m_bMoving 변화 감지 (기존 transition 로직 재사용)
}
```

**Q/W/E/R/좌클릭 (Skill/BA)**: Layer 1 미동작 — `UpdateCombatInput` 안의 스킬 디스패치는
그대로 유지하되 `SendCastSkill` 추가는 Layer 2 (D-4) 에서 본격.

### 5.4 합격 추가 조건 (Codex P1-5/P1-6 반영)

- ✅ 우클릭 → `m_pCommandSerializer->SendMove` 호출 (network 모드)
- ✅ 우클릭 → 로컬 NavAgent 갱신 스킵 (network 모드)
- ✅ snapshot 도착 → 본인 캐릭터 위치 1-2 frame 지연 후 갱신 (legacy sync 가 덮지 않음)
- ✅ 오프라인 모드: 기존 로컬 보간 동작 유지 (regression 0)

---

## 6. 합격 게이트 (D-2 전체)

| Phase | 합격 조건 |
|---|---|
| **D-2A** | `Connect("127.0.0.1", 9000)` → server log 에 "Client connected" 출력. Disconnect 시 정상 cleanup |
| **D-2B** | 우클릭 → server log 에 `EnqueueCommand(Move)` 호출 |
| **D-2C** | OnHello → 본인 NetEntityId 인지 + Scene 의 callback 으로 ECS Champion 생성. OnSnapshot → 다른 client 챔피언 위치 갱신 |
| **D-2D** | Scene 진입 시 자동 connect, 매 frame PumpReceivedFrames, FrameCallback 분기 동작 |
| **D-2E** | 우클릭 → CommandSerializer 호출 + 기존 local nav 코드 비활성화 |

---

## 7. 위험

| # | 위험 | 완화 |
|---|---|---|
| **R1** | RecvThread 가 socket close 시 무한 대기 | `recv` 가 0 또는 SOCKET_ERROR 반환 시 break |
| **R2** | PumpReceivedFrames 가 main thread 외에서 호출 → ECS race | `Scene_InGame::OnUpdate` 단일 호출 강제 |
| **R3** | OnNewEntity callback 이 nullptr → entity 생성 안됨 | `if (!m_onNewEntity) continue;` guard |
| **R4** | Scene 이미 ECS Champion 생성한 경우 (예: BanPick 산출) → 중복 spawn | `m_seenNetIds` set 으로 1회 보장 |
| **R5** | InputSystem 이 Network 포인터 캐시 — Scene_InGame OnExit 후 dangling | OnExit 에서 Network reset 시 InputSystem 도 nullptr 마킹 |

---

## 8. 한 줄

**D-2 = Client 측 5 작업. ClientNetwork (TCP connect + recv worker thread + frame parser mirror + main thread marshal) + CommandSerializer (Move 만, FlatBuffers CommandBatch + envelope wrap) + SnapshotApplier (OnHello + OnSnapshot + OnNewEntity callback 패턴) + Scene_InGame 통합 (connect + PumpReceivedFrames + callback 분기) + InputSystem 마이그 (server authoritative, 기존 local 비활성화). UDP 마이그 시 ClientNetwork 만 갈아끼움 — 나머지 4개 그대로.**
