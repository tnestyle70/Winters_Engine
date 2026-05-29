# 06. Network Replication -- UE5-Style CNetDriver / FRepLayout / WRPC / Delta Serialization

> **UE5 대응**: `UNetDriver`, `FRepLayout`, `UFUNCTION(Server)`, `UFUNCTION(Client)`, `UFUNCTION(NetMulticast)`, `FObjectReplicator`, Actor Relevancy
> **현재 Winters**: FlatBuffers 수동 직렬화 (Snapshot.fbs / Command.fbs / Event.fbs), CGameRoom 30Hz 서버 틱, CSnapshotApplier 수동 필드 매핑, 모든 엔티티 매 프레임 전체 전송
> **목표**: WPROPERTY(Replicated) 자동 직렬화, WRPC 매크로 기반 RPC, delta-only 전송, Actor relevancy

---

## 1. Architecture Overview

### 1.1 UE5 Network Replication 핵심

```
서버:
  AActor::PreReplication()
    → FRepLayout 이 UPROPERTY(Replicated) 목록 순회
    → 이전 스냅샷과 비교 (delta)
    → 변경된 프로퍼티만 직렬화 → FBitWriter
    → Actor Relevancy 필터링 → 관련 클라이언트에만 전송

클라이언트:
  FRepLayout::ReceiveProperties()
    → delta 프로퍼티 역직렬화
    → 멤버 변수에 직접 쓰기 (offset 기반)
    → RepNotify 콜백 호출

RPC:
  UFUNCTION(Server, Reliable)
  void ServerMoveCommand(FVector dest);
    → 클라이언트 호출 → 직렬화 → 서버 실행
  UFUNCTION(Client, Reliable)
  void ClientPlayEffect(FName effectId);
    → 서버 호출 → 직렬화 → 특정 클라이언트 실행
  UFUNCTION(NetMulticast, Unreliable)
  void MulticastPlaySound(FName soundId);
    → 서버 호출 → 모든 클라이언트 실행
```

### 1.2 현재 Winters 네트워크 문제

```
Server: CGameRoom::Phase_BroadcastSnapshot()
  → CSnapshotBuilder 가 모든 엔티티 전체 필드를 FlatBuffers 로 직렬화
  → 매 틱(30Hz) 모든 엔티티 × 모든 필드 전송
  → EntitySnapshot { netId, championId, team, level, hp, mana, posX, posY, posZ,
                     yaw, moveSpeed, animId, animPhaseFrame, skillCooldowns[],
                     skillRanks[], buffMask, statHash }

Client: CSnapshotApplier::Apply(snapshot)
  → 수동 필드 매핑:
     auto* entitySnap = snapshot->entities()->Get(i);
     auto& health = world.GetComponent<HealthComponent>(entity);
     health.fCurrent = entitySnap->hp();
     // 각 필드를 하나하나 수동으로 매핑 (30줄 per entity type)

RPC: 없음. CommandPacket 으로 클라 → 서버 일방향.
  서버 → 클라 이벤트 = EventPacket (EventKind enum + union table)
  → 새 이벤트 추가 = .fbs 수정 + flatc 재생성 + 클라/서버 양쪽 핸들러 수동 추가

문제:
  1. 전체 전송: HP 1만 바뀌어도 20+ 필드 전부 보냄 → 대역폭 낭비
  2. 수동 매핑: 필드 추가 = .fbs + SnapshotBuilder + SnapshotApplier 3곳 수정
  3. RPC 없음: 서버→클라 이벤트 = EventKind enum 확장 + 수동 dispatch
  4. 타입 안전성 없음: FlatBuffers 는 런타임 필드 이름 = 문자열 매칭 없음
  5. Relevancy 없음: 모든 엔티티를 모든 클라에 전송
```

### 1.3 Winters Network Replication 설계

```
WPROPERTY(Replicated) → FRepLayout 자동 등록
  → 서버: 이전 값과 비교 (memcmp) → 변경된 프로퍼티만 직렬화
  → 클라: offset 기반 역직렬화 → 멤버 변수 직접 쓰기

WRPC(Server, Reliable) → 매크로가 dispatch 테이블 자동 생성
  → 클라 호출 → 인자 직렬화 → 서버 실행
WRPC(Client, Reliable) → 서버 호출 → 특정 클라 실행
WRPC(Multicast, Unreliable) → 서버 호출 → 모든 클라 실행

FlatBuffers 호환:
  → Snapshot.fbs 는 "전체 스냅샷 + delta 마스크" 로 확장
  → 기존 FlatBuffers 인프라 위에 WPROPERTY 자동화 레이어
  → 점진 마이그: 기존 수동 코드와 새 자동 코드 병존
```

---

## 2. 파일 구조

```
Engine/
├── Public/Network/
│   ├── CNetDriver.h              -- 네트워크 드라이버 (연결/채널 관리)
│   ├── FRepLayout.h              -- 리플리케이션 레이아웃 (프로퍼티 자동 직렬화)
│   ├── NetMacros.h               -- WRPC, WREPLICATED 매크로
│   ├── CNetChannel.h             -- 액터 채널 (per-actor 직렬화 스트림)
│   └── RelevancyFilter.h         -- Actor relevancy 필터
├── Private/Network/
│   ├── CNetDriver.cpp
│   ├── FRepLayout.cpp
│   ├── CNetChannel.cpp
│   └── RelevancyFilter.cpp
```

---

## 3. 코드 전문

### `Engine/Public/Network/NetMacros.h`

```cpp
#pragma once

#include "WintersTypes.h"

/// RPC 타입 분류
enum class eRPCType : u8_t
{
    Server,      // 클라 → 서버
    Client,      // 서버 → 특정 클라
    Multicast,   // 서버 → 모든 클라
};

/// RPC 신뢰성
enum class eRPCReliability : u8_t
{
    Reliable,
    Unreliable,
};

/// WRPC 매크로 -- 함수 선언 앞에 배치
/// 향후 코드 생성기가 dispatch 테이블 자동 생성.
/// 현재는 marker 역할 + RegisterRPC 에서 수동 등록.
///
/// 사용법:
///   WRPC(Server, Reliable)
///   void Server_MoveCommand(Vec3 dest);
///
///   WRPC(Client, Reliable)
///   void Client_PlayEffect(u32_t effectId);
///
///   WRPC(Multicast, Unreliable)
///   void Multicast_PlaySound(u32_t soundId);
#define WRPC(...)

/// RPC 등록 헬퍼 (RegisterRPCs 구현에서 사용)
/// RPCName: 함수 이름 문자열
/// Fn: 멤버 함수 포인터
/// Type: eRPCType
/// Reliability: eRPCReliability
#define REGISTER_RPC(ClassName, FuncName, Type, Reliability)                    \
    do {                                                                        \
        RPCDescriptor desc;                                                     \
        desc.name = #FuncName;                                                  \
        desc.rpcType = Type;                                                    \
        desc.reliability = Reliability;                                         \
        desc.classTypeName = #ClassName;                                        \
        desc.invoker = [](WObject* obj, CNetBitReader& reader) {               \
            auto* self = static_cast<ClassName*>(obj);                          \
            ClassName::FuncName##_NetReceive(self, reader);                     \
        };                                                                      \
        pLayout->RegisterRPC(desc);                                             \
    } while(0)
```

### `Engine/Public/Network/FRepLayout.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/WProperty.h"

#include <vector>
#include <functional>
#include <string>
#include <cstring>

class WObject;
class WClass;

/// 비트 리더/라이터 (간이 직렬화)
/// 향후 비트 레벨 패킹으로 교체 가능
class CNetBitWriter
{
public:
    void WriteBytes(const void* data, u32_t size)
    {
        auto* bytes = static_cast<const u8_t*>(data);
        m_Buffer.insert(m_Buffer.end(), bytes, bytes + size);
    }

    template<typename T>
    void Write(const T& val) { WriteBytes(&val, sizeof(T)); }

    void WriteU8(u8_t val) { m_Buffer.push_back(val); }
    void WriteU16(u16_t val) { WriteBytes(&val, 2); }
    void WriteU32(u32_t val) { WriteBytes(&val, 4); }
    void WriteF32(f32_t val) { WriteBytes(&val, 4); }

    const u8_t* Data() const { return m_Buffer.data(); }
    u32_t Size() const { return static_cast<u32_t>(m_Buffer.size()); }
    void Clear() { m_Buffer.clear(); }

private:
    std::vector<u8_t> m_Buffer;
};

class CNetBitReader
{
public:
    CNetBitReader(const u8_t* data, u32_t size)
        : m_pData(data), m_Size(size), m_Offset(0) {}

    bool ReadBytes(void* dest, u32_t size)
    {
        if (m_Offset + size > m_Size) return false;
        memcpy(dest, m_pData + m_Offset, size);
        m_Offset += size;
        return true;
    }

    template<typename T>
    bool Read(T& val) { return ReadBytes(&val, sizeof(T)); }

    u8_t ReadU8() { u8_t v = 0; ReadBytes(&v, 1); return v; }
    u16_t ReadU16() { u16_t v = 0; ReadBytes(&v, 2); return v; }
    u32_t ReadU32() { u32_t v = 0; ReadBytes(&v, 4); return v; }
    f32_t ReadF32() { f32_t v = 0; ReadBytes(&v, 4); return v; }

    bool IsEOF() const { return m_Offset >= m_Size; }
    u32_t Remaining() const { return m_Size - m_Offset; }

private:
    const u8_t* m_pData;
    u32_t       m_Size;
    u32_t       m_Offset;
};

/// RPC 디스크립터
struct RPCDescriptor
{
    const char*     name = nullptr;
    const char*     classTypeName = nullptr;
    eRPCType        rpcType = eRPCType::Server;
    eRPCReliability reliability = eRPCReliability::Reliable;

    /// RPC 수신 시 호출. reader 에서 인자를 역직렬화 + 함수 실행.
    std::function<void(WObject* obj, CNetBitReader& reader)> invoker;
};

/// 리플리케이션 프로퍼티 엔트리
struct FRepPropertyEntry
{
    const WPropertyMeta* pMeta = nullptr;   // 프로퍼티 메타데이터
    u32_t                index = 0;          // 프로퍼티 인덱스 (bitmask 위치)
};

/// UE5 FRepLayout 대응
/// WPROPERTY(Replicated) 프로퍼티의 자동 delta 직렬화.
///
/// 동작:
///   1. BuildFromClass(WClass*) → Replicated 프로퍼티 목록 수집
///   2. CompareAndSerialize(curObj, prevSnapshot) → 변경된 프로퍼티만 직렬화
///   3. ReceiveProperties(obj, reader) → 역직렬화 → 멤버 변수 직접 쓰기
class WINTERS_API FRepLayout
{
public:
    FRepLayout();
    ~FRepLayout();

    /// WClass 에서 Replicated 프로퍼티 수집하여 레이아웃 빌드
    void BuildFromClass(WClass* pClass);

    /// delta 비교 + 직렬화
    /// prevSnapshot: 이전 프레임의 오브젝트 상태 복사 (바이트 배열)
    /// curObj: 현재 오브젝트
    /// outWriter: 변경된 프로퍼티만 직렬화
    /// @return 변경된 프로퍼티 수 (0 = 변경 없음)
    u32_t CompareAndSerialize(
        const void* prevSnapshot,
        const WObject* curObj,
        CNetBitWriter& outWriter) const;

    /// 역직렬화 → 멤버 변수 쓰기
    /// @return 수신된 프로퍼티 수
    u32_t ReceiveProperties(
        WObject* obj,
        CNetBitReader& reader) const;

    /// 현재 상태를 스냅샷으로 복사 (다음 프레임 delta 비교용)
    void TakeSnapshot(const WObject* obj, std::vector<u8_t>& outSnapshot) const;

    /// 리플리케이트 프로퍼티 수
    u32_t GetPropertyCount() const
    {
        return static_cast<u32_t>(m_Properties.size());
    }

    /// RPC 등록
    void RegisterRPC(const RPCDescriptor& desc);

    /// RPC 이름으로 조회
    const RPCDescriptor* FindRPC(const char* name) const;

    /// RPC 수
    u32_t GetRPCCount() const
    {
        return static_cast<u32_t>(m_RPCs.size());
    }

    /// 전체 스냅샷 크기
    u32_t GetSnapshotSize() const { return m_SnapshotSize; }

private:
    std::vector<FRepPropertyEntry> m_Properties;
    std::vector<RPCDescriptor>     m_RPCs;
    u32_t                          m_SnapshotSize = 0;
};
```

### `Engine/Private/Network/FRepLayout.cpp`

```cpp
#include "Network/FRepLayout.h"
#include "Object/WClass.h"
#include "Object/WObject.h"

#include <cstring>

#ifdef _DEBUG
#define REPLAYOUT_LOG(fmt, ...) do {                               \
    char _buf[512];                                                \
    snprintf(_buf, sizeof(_buf), "[RepLayout] " fmt "\n",          \
             ##__VA_ARGS__);                                       \
    OutputDebugStringA(_buf);                                      \
} while(0)
#else
#define REPLAYOUT_LOG(fmt, ...) ((void)0)
#endif

FRepLayout::FRepLayout()
{
}

FRepLayout::~FRepLayout()
{
}

void FRepLayout::BuildFromClass(WClass* pClass)
{
    m_Properties.clear();
    m_SnapshotSize = 0;

    if (!pClass) return;

    auto allProps = pClass->GetAllProperties();
    u32_t index = 0;

    for (auto* prop : allProps)
    {
        if (!HasFlag(prop->flags, ePropertyFlags::Replicated))
            continue;

        FRepPropertyEntry entry;
        entry.pMeta = prop;
        entry.index = index++;
        m_Properties.push_back(entry);

        m_SnapshotSize += prop->size;
    }

    REPLAYOUT_LOG("Built layout for '%s': %u replicated properties, snapshot %u bytes",
                  pClass->GetName(), index, m_SnapshotSize);
}

u32_t FRepLayout::CompareAndSerialize(
    const void* prevSnapshot,
    const WObject* curObj,
    CNetBitWriter& outWriter) const
{
    if (!curObj || m_Properties.empty()) return 0;

    u32_t changedCount = 0;
    u32_t snapshotOffset = 0;

    // 헤더: 변경 bitmask 예약 위치 (최대 64 프로퍼티)
    // 간이 구현: 프로퍼티 인덱스 + 값 쌍 나열, 종료 = 0xFF
    const u8_t* prevBytes = static_cast<const u8_t*>(prevSnapshot);

    for (auto& entry : m_Properties)
    {
        const void* curValue = entry.pMeta->GetValuePtr(curObj);
        const void* prevValue = prevSnapshot
            ? (prevBytes + snapshotOffset)
            : nullptr;

        bool changed = true;
        if (prevValue)
        {
            changed = (memcmp(curValue, prevValue, entry.pMeta->size) != 0);
        }

        if (changed)
        {
            // 프로퍼티 인덱스 (1바이트, 최대 255 프로퍼티)
            outWriter.WriteU8(static_cast<u8_t>(entry.index));
            // 프로퍼티 값 (raw bytes)
            outWriter.WriteBytes(curValue, entry.pMeta->size);
            ++changedCount;
        }

        snapshotOffset += entry.pMeta->size;
    }

    // 종료 마커
    outWriter.WriteU8(0xFF);

    return changedCount;
}

u32_t FRepLayout::ReceiveProperties(
    WObject* obj,
    CNetBitReader& reader) const
{
    if (!obj) return 0;

    u32_t receivedCount = 0;

    while (!reader.IsEOF())
    {
        u8_t propIndex = reader.ReadU8();
        if (propIndex == 0xFF) break;  // 종료 마커

        if (propIndex >= m_Properties.size())
        {
            REPLAYOUT_LOG("ERROR: Invalid property index %u (max %u)",
                          propIndex, static_cast<u32_t>(m_Properties.size()));
            break;
        }

        auto& entry = m_Properties[propIndex];
        void* destValue = entry.pMeta->GetValuePtr(obj);

        if (!reader.ReadBytes(destValue, entry.pMeta->size))
        {
            REPLAYOUT_LOG("ERROR: Failed to read property '%s' (%u bytes)",
                          entry.pMeta->name, entry.pMeta->size);
            break;
        }

        ++receivedCount;
    }

    return receivedCount;
}

void FRepLayout::TakeSnapshot(const WObject* obj,
                               std::vector<u8_t>& outSnapshot) const
{
    if (!obj)
    {
        outSnapshot.clear();
        return;
    }

    outSnapshot.resize(m_SnapshotSize);
    u32_t offset = 0;

    for (auto& entry : m_Properties)
    {
        const void* srcValue = entry.pMeta->GetValuePtr(obj);
        memcpy(outSnapshot.data() + offset, srcValue, entry.pMeta->size);
        offset += entry.pMeta->size;
    }
}

void FRepLayout::RegisterRPC(const RPCDescriptor& desc)
{
    m_RPCs.push_back(desc);
    REPLAYOUT_LOG("Registered RPC: %s::%s (type=%d, reliable=%d)",
                  desc.classTypeName, desc.name,
                  static_cast<int>(desc.rpcType),
                  static_cast<int>(desc.reliability));
}

const RPCDescriptor* FRepLayout::FindRPC(const char* name) const
{
    for (auto& rpc : m_RPCs)
    {
        if (strcmp(rpc.name, name) == 0)
            return &rpc;
    }
    return nullptr;
}
```

### `Engine/Public/Network/CNetDriver.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Network/FRepLayout.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

class WActor;
class WWorld;

/// 연결 상태
enum class eNetConnectionState : u8_t
{
    Disconnected,
    Connecting,
    Connected,
    Closing,
};

/// 연결 정보
struct NetConnection
{
    u32_t               sessionId = 0;
    eNetConnectionState state = eNetConnectionState::Disconnected;
    u32_t               controlledActorNetId = 0;  // 이 연결이 제어하는 액터
};

/// 액터 채널: per-actor 리플리케이션 상태
struct FActorChannel
{
    WActor*             pActor = nullptr;
    FRepLayout          repLayout;
    std::vector<u8_t>   prevSnapshot;   // 이전 프레임 스냅샷 (delta 비교용)
    bool                bInitialSent = false;
};

/// UE5 UNetDriver 대응
/// 네트워크 연결 관리 + 리플리케이션 오케스트레이션.
///
/// 서버:
///   CNetDriver::ReplicateActors(world)
///     → 각 replicating 액터의 FRepLayout::CompareAndSerialize
///     → Relevancy 필터링
///     → 연결별 전송
///
/// 클라이언트:
///   CNetDriver::ReceiveReplication(data)
///     → FRepLayout::ReceiveProperties
///     → RPC dispatch
///
/// FlatBuffers 호환:
///   기존 Snapshot.fbs/Command.fbs/Event.fbs 는 "전송 포맷" 으로 유지.
///   FRepLayout 이 delta 데이터를 Snapshot 안에 embed 하는 방식.
///   전환 기간 중 기존 전체 스냅샷 + 신규 delta 채널 병존.
class WINTERS_API CNetDriver
{
public:
    CNetDriver();
    ~CNetDriver();

    static std::unique_ptr<CNetDriver> Create();

    // ---- 서버 모드 ----

    /// 새 연결 등록
    void AddConnection(u32_t sessionId);

    /// 연결 제거
    void RemoveConnection(u32_t sessionId);

    /// 모든 replicating 액터의 delta 직렬화 + 전송
    /// @return 전송된 바이트 수
    u32_t ReplicateActors(WWorld* pWorld);

    /// RPC 수신 처리 (클라 → 서버)
    void ProcessIncomingRPC(u32_t sessionId, const u8_t* data, u32_t size);

    // ---- 클라이언트 모드 ----

    /// 서버에서 수신한 리플리케이션 데이터 적용
    void ReceiveReplication(WWorld* pWorld, const u8_t* data, u32_t size);

    /// RPC 전송 (클라 → 서버)
    void SendRPC(const char* rpcName, u32_t actorNetId,
                 CNetBitWriter& argWriter);

    // ---- 연결 관리 ----

    const std::vector<NetConnection>& GetConnections() const
    {
        return m_Connections;
    }

    NetConnection* FindConnection(u32_t sessionId);

    // ---- 채널 관리 ----

    /// 액터의 리플리케이션 채널 생성/획득
    FActorChannel* GetOrCreateChannel(WActor* pActor);

    /// 액터 채널 제거
    void RemoveChannel(WActor* pActor);

    // ---- Relevancy ----

    /// Relevancy 필터 설정
    using RelevancyFilterFn = std::function<bool(
        const WActor* actor, const NetConnection& conn)>;
    void SetRelevancyFilter(RelevancyFilterFn filter);

    // ---- 통계 ----

    u32_t GetTotalBytesSent() const { return m_TotalBytesSent; }
    u32_t GetTotalBytesReceived() const { return m_TotalBytesReceived; }
    u32_t GetDeltaPropertiesSent() const { return m_DeltaPropertiesSent; }

    // ---- 전송 콜백 (실제 소켓 전송은 외부에서) ----

    using SendFn = std::function<void(u32_t sessionId, const u8_t* data, u32_t size)>;
    void SetSendCallback(SendFn fn) { m_SendFn = std::move(fn); }

private:
    std::vector<NetConnection> m_Connections;
    std::unordered_map<u32_t, FActorChannel> m_ActorChannels;  // key: actor netID

    RelevancyFilterFn m_RelevancyFilter;
    SendFn m_SendFn;

    // 통계
    u32_t m_TotalBytesSent = 0;
    u32_t m_TotalBytesReceived = 0;
    u32_t m_DeltaPropertiesSent = 0;
};
```

### `Engine/Private/Network/CNetDriver.cpp`

```cpp
#include "Network/CNetDriver.h"
#include "Actor/WActor.h"
#include "World/WWorld.h"
#include "Object/WClass.h"

#ifdef _DEBUG
#define NET_LOG(fmt, ...) do {                                     \
    char _buf[512];                                                \
    snprintf(_buf, sizeof(_buf), "[NetDriver] " fmt "\n",          \
             ##__VA_ARGS__);                                       \
    OutputDebugStringA(_buf);                                      \
} while(0)
#else
#define NET_LOG(fmt, ...) ((void)0)
#endif

CNetDriver::CNetDriver()
{
}

CNetDriver::~CNetDriver()
{
}

std::unique_ptr<CNetDriver> CNetDriver::Create()
{
    return std::make_unique<CNetDriver>();
}

void CNetDriver::AddConnection(u32_t sessionId)
{
    NetConnection conn;
    conn.sessionId = sessionId;
    conn.state = eNetConnectionState::Connected;
    m_Connections.push_back(conn);
    NET_LOG("Connection added: session=%u", sessionId);
}

void CNetDriver::RemoveConnection(u32_t sessionId)
{
    m_Connections.erase(
        std::remove_if(m_Connections.begin(), m_Connections.end(),
            [sessionId](const NetConnection& c) { return c.sessionId == sessionId; }),
        m_Connections.end());
    NET_LOG("Connection removed: session=%u", sessionId);
}

u32_t CNetDriver::ReplicateActors(WWorld* pWorld)
{
    if (!pWorld || m_Connections.empty()) return 0;

    u32_t totalBytes = 0;

    pWorld->ForEachActor([&](WActor* actor)
    {
        if (!actor->m_bReplicates) return;

        u32_t netId = actor->GetNetID();
        if (netId == 0) return;

        auto* channel = GetOrCreateChannel(actor);
        if (!channel) return;

        // delta 비교 + 직렬화
        CNetBitWriter writer;

        // 패킷 헤더: netId
        writer.WriteU32(netId);

        u32_t changedCount = channel->repLayout.CompareAndSerialize(
            channel->prevSnapshot.empty() ? nullptr : channel->prevSnapshot.data(),
            actor,
            writer);

        // 스냅샷 갱신
        channel->repLayout.TakeSnapshot(actor, channel->prevSnapshot);

        if (changedCount == 0) return;  // 변경 없음 = 전송 안 함

        m_DeltaPropertiesSent += changedCount;

        // Relevancy 필터링 + 전송
        for (auto& conn : m_Connections)
        {
            if (conn.state != eNetConnectionState::Connected) continue;

            // Relevancy 확인
            if (m_RelevancyFilter && !m_RelevancyFilter(actor, conn))
                continue;

            if (m_SendFn)
                m_SendFn(conn.sessionId, writer.Data(), writer.Size());

            totalBytes += writer.Size();
        }
    });

    m_TotalBytesSent += totalBytes;
    return totalBytes;
}

void CNetDriver::ProcessIncomingRPC(u32_t sessionId, const u8_t* data, u32_t size)
{
    if (!data || size < 6) return;  // 최소: netId(4) + rpcNameLen(1) + rpcName(1+)

    CNetBitReader reader(data, size);

    u32_t actorNetId = reader.ReadU32();
    u8_t rpcNameLen = reader.ReadU8();

    // RPC 이름 읽기
    char rpcName[256] = {};
    if (rpcNameLen >= sizeof(rpcName)) return;
    reader.ReadBytes(rpcName, rpcNameLen);
    rpcName[rpcNameLen] = '\0';

    // 액터 채널에서 RPC 검색
    auto it = m_ActorChannels.find(actorNetId);
    if (it == m_ActorChannels.end()) return;

    auto* rpcDesc = it->second.repLayout.FindRPC(rpcName);
    if (!rpcDesc) return;

    // 서버 RPC 인지 확인
    if (rpcDesc->rpcType != eRPCType::Server) return;

    // RPC 실행
    if (rpcDesc->invoker && it->second.pActor)
        rpcDesc->invoker(it->second.pActor, reader);

    m_TotalBytesReceived += size;
    NET_LOG("RPC received: %s on actor %u from session %u",
            rpcName, actorNetId, sessionId);
}

void CNetDriver::ReceiveReplication(WWorld* pWorld, const u8_t* data, u32_t size)
{
    if (!pWorld || !data || size < 5) return;

    CNetBitReader reader(data, size);

    u32_t actorNetId = reader.ReadU32();

    // 액터 찾기
    WActor* pActor = pWorld->FindActorByNetID(actorNetId);
    if (!pActor) return;

    auto* channel = GetOrCreateChannel(pActor);
    if (!channel) return;

    u32_t received = channel->repLayout.ReceiveProperties(pActor, reader);

    m_TotalBytesReceived += size;

    if (received > 0)
        NET_LOG("Received %u properties for actor %u", received, actorNetId);
}

void CNetDriver::SendRPC(const char* rpcName, u32_t actorNetId,
                          CNetBitWriter& argWriter)
{
    CNetBitWriter packet;

    // actorNetId
    packet.WriteU32(actorNetId);

    // RPC name (length-prefixed)
    u8_t nameLen = static_cast<u8_t>(strlen(rpcName));
    packet.WriteU8(nameLen);
    packet.WriteBytes(rpcName, nameLen);

    // RPC arguments
    packet.WriteBytes(argWriter.Data(), argWriter.Size());

    // 전송 (서버에)
    if (m_SendFn)
        m_SendFn(0 /* server */, packet.Data(), packet.Size());

    m_TotalBytesSent += packet.Size();
}

NetConnection* CNetDriver::FindConnection(u32_t sessionId)
{
    for (auto& conn : m_Connections)
    {
        if (conn.sessionId == sessionId)
            return &conn;
    }
    return nullptr;
}

FActorChannel* CNetDriver::GetOrCreateChannel(WActor* pActor)
{
    if (!pActor) return nullptr;

    u32_t netId = pActor->GetNetID();
    auto it = m_ActorChannels.find(netId);
    if (it != m_ActorChannels.end())
        return &it->second;

    FActorChannel channel;
    channel.pActor = pActor;
    channel.repLayout.BuildFromClass(pActor->GetClass());

    m_ActorChannels[netId] = std::move(channel);
    NET_LOG("Channel created for actor %u (class: %s)",
            netId, pActor->GetClass()->GetName());

    return &m_ActorChannels[netId];
}

void CNetDriver::RemoveChannel(WActor* pActor)
{
    if (!pActor) return;
    m_ActorChannels.erase(pActor->GetNetID());
}

void CNetDriver::SetRelevancyFilter(RelevancyFilterFn filter)
{
    m_RelevancyFilter = std::move(filter);
}
```

### `Engine/Public/Network/RelevancyFilter.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "Network/CNetDriver.h"

class WActor;
class WWorld;

/// Actor relevancy 판정 유틸리티
/// UE5 의 IsNetRelevantFor / IsAlwaysRelevant 대응
namespace NetRelevancy
{
    /// 거리 기반 relevancy (MOBA 기본: 시야 거리 이내)
    /// controllerPos: 연결의 컨트롤러 위치
    /// actor: 판정 대상 액터
    /// maxDistance: 최대 관측 거리
    inline bool IsRelevantByDistance(
        const Vec3& controllerPos,
        const WActor* actor,
        f32_t maxDistance = 50.f)
    {
        if (!actor) return false;

        Vec3 actorPos = actor->GetActorPosition();
        f32_t dx = actorPos.x - controllerPos.x;
        f32_t dz = actorPos.z - controllerPos.z;
        f32_t distSq = dx * dx + dz * dz;

        return distSq <= (maxDistance * maxDistance);
    }

    /// MOBA 기본 relevancy 필터 팩토리
    /// 플레이어 챔피언 + 아군은 항상 relevant, 적은 거리 기반
    inline CNetDriver::RelevancyFilterFn CreateMOBAFilter(
        WWorld* pWorld,
        f32_t fVisionRange = 50.f)
    {
        return [pWorld, fVisionRange](const WActor* actor, const NetConnection& conn) -> bool
        {
            // 자기 자신의 액터는 항상 relevant
            if (actor->GetNetID() == conn.controlledActorNetId)
                return true;

            // 향후: 팀 판정 + 시야 시스템 연동
            // 현재: 거리 기반
            // 컨트롤러 액터 위치 필요 → pWorld->FindActorByNetID(conn.controlledActorNetId)
            WActor* controllerActor = pWorld->FindActorByNetID(conn.controlledActorNetId);
            if (!controllerActor)
                return true;  // 컨트롤러 없으면 전체 전송 (안전)

            return IsRelevantByDistance(
                controllerActor->GetActorPosition(),
                actor,
                fVisionRange);
        };
    }
}
```

---

## 4. 사용 예시

### 4.1 Before: 수동 FlatBuffers 직렬화

```cpp
// Server: CSnapshotBuilder (수동, 모든 필드 매 틱 전송)
flatbuffers::Offset<EntitySnapshot> CSnapshotBuilder::BuildEntity(
    flatbuffers::FlatBufferBuilder& fbb,
    EntityID entity, CWorld& world, EntityIdMap& idMap)
{
    auto& health = world.GetComponent<HealthComponent>(entity);
    auto& champ = world.GetComponent<ChampionComponent>(entity);
    auto& tf = world.GetComponent<TransformComponent>(entity);
    auto& stat = world.GetComponent<StatComponent>(entity);

    return CreateEntitySnapshot(fbb,
        idMap.ToNet(entity),
        static_cast<uint8_t>(champ.eChampId),
        static_cast<uint8_t>(champ.eTeam),
        stat.iLevel,
        health.fCurrent,
        stat.fMana,
        tf.position.x, tf.position.y, tf.position.z,
        tf.yaw,
        stat.fMoveSpeed,
        /* animId */ 0, /* animFrame */ 0,
        fbb.CreateVector(cooldowns),
        fbb.CreateVector(ranks),
        /* buffMask */ 0,
        /* statHash */ 0);
}
// → 매 틱 모든 엔티티 × 모든 필드 전송 (변경 없어도)

// Client: CSnapshotApplier (수동 필드 매핑)
void CSnapshotApplier::Apply(const Snapshot* snap, CWorld& world, EntityIdMap& idMap)
{
    for (u32_t i = 0; i < snap->entities()->size(); ++i)
    {
        auto* es = snap->entities()->Get(i);
        EntityID entity = idMap.ToLocal(es->netId());

        auto& health = world.GetComponent<HealthComponent>(entity);
        health.fCurrent = es->hp();

        auto& tf = world.GetComponent<TransformComponent>(entity);
        tf.position = { es->posX(), es->posY(), es->posZ() };
        tf.yaw = es->yaw();
        // ... 15줄 더 per entity
    }
}
```

### 4.2 After: WPROPERTY(Replicated) 자동 리플리케이션

```cpp
// WChampionActor.h — 프로퍼티 선언만으로 자동 리플리케이션
WCLASS()
class WChampionActor : public WActor
{
    using Super = WActor;
    WINTERS_GENERATED_BODY(WChampionActor)

public:
    // 이 필드들은 WPROPERTY(Replicated) → FRepLayout 이 자동 수집
    // 서버에서 값 변경 → 다음 틱에 delta 만 전송 → 클라 자동 적용

    WPROPERTY(Replicated, EditAnywhere, Category = "Stats")
    f32_t m_fHealth = 1500.f;

    WPROPERTY(Replicated, EditAnywhere, Category = "Stats")
    f32_t m_fMana = 500.f;

    WPROPERTY(Replicated, VisibleAnywhere, Category = "Movement")
    Vec3 m_vPosition = { 0.f, 0.f, 0.f };

    WPROPERTY(Replicated, VisibleAnywhere, Category = "Movement")
    f32_t m_fYaw = 0.f;

    WPROPERTY(Replicated, VisibleAnywhere, Category = "Movement")
    f32_t m_fMoveSpeed = 340.f;

    WPROPERTY(Replicated, VisibleAnywhere, Category = "Stats")
    u8_t m_iLevel = 1;

    // RPC: 클라 → 서버 이동 명령
    WRPC(Server, Reliable)
    void Server_MoveCommand(Vec3 dest);

    // RPC: 서버 → 클라 이펙트 재생
    WRPC(Client, Reliable)
    void Client_PlayEffect(u32_t effectId, Vec3 position);

    // RPC: 서버 → 모든 클라 사운드 재생
    WRPC(Multicast, Unreliable)
    void Multicast_PlaySound(u32_t soundId);

private:
    WChampionActor();
};

// WChampionActor.cpp — RegisterProperties
void WChampionActor::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(WChampionActor, m_fHealth,
                      ePropertyFlags::Replicated | ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY(WChampionActor, m_fMana,
                      ePropertyFlags::Replicated | ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY(WChampionActor, m_vPosition,
                      ePropertyFlags::Replicated | ePropertyFlags::VisibleAnywhere);
    REGISTER_PROPERTY(WChampionActor, m_fYaw,
                      ePropertyFlags::Replicated | ePropertyFlags::VisibleAnywhere);
    REGISTER_PROPERTY(WChampionActor, m_fMoveSpeed,
                      ePropertyFlags::Replicated | ePropertyFlags::VisibleAnywhere);
    REGISTER_PROPERTY(WChampionActor, m_iLevel,
                      ePropertyFlags::Replicated | ePropertyFlags::VisibleAnywhere);
}

// RPC 구현
void WChampionActor::Server_MoveCommand(Vec3 dest)
{
    // 서버에서 실행: pathfinding → 이동 시작
    m_vPosition = dest;  // 간이 구현
}

// RPC 수신 핸들러 (매크로가 생성, 현재 수동)
void WChampionActor::Server_MoveCommand_NetReceive(
    WChampionActor* self, CNetBitReader& reader)
{
    Vec3 dest;
    reader.Read(dest);
    self->Server_MoveCommand(dest);
}
```

```cpp
// Server: CGameRoom 에서 자동 리플리케이션
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    // 기존: CSnapshotBuilder 수동 전체 직렬화 (유지, 과도기)
    // 신규: CNetDriver delta 리플리케이션 (병존)
    if (m_pNetDriver)
    {
        u32_t bytesSent = m_pNetDriver->ReplicateActors(m_pWorld.get());
        // delta 전송 완료 → 변경된 프로퍼티만 전송됨
    }
}
```

### 4.3 FlatBuffers 통합 전략

```cpp
// Phase 1 (과도기): 기존 Snapshot + 신규 delta 채널 병존
// Snapshot.fbs 에 deltaPayload 필드 추가:
//   table Snapshot {
//     ...existing fields...
//     deltaPayload:[ubyte];   // FRepLayout delta 바이너리
//   }
//
// 서버: 기존 전체 스냅샷 + delta payload 동시 전송
// 클라: 기존 apply + delta apply 양쪽 실행
// 결과: 점진 마이그레이션 (기존 코드 안전)

// Phase 2: delta 검증 완료 후 기존 전체 필드 → delta only 전환
// Snapshot.fbs 의 개별 필드(hp, posX, ...) 제거
// deltaPayload 만으로 동기화

// Phase 3: 기존 SnapshotBuilder / SnapshotApplier 제거
```

---

## 5. Verification Checklist

```
[ ] FRepLayout::BuildFromClass → Replicated 프로퍼티만 수집
[ ] FRepLayout::CompareAndSerialize → 값 변경 시에만 직렬화 (memcmp)
[ ] FRepLayout::CompareAndSerialize → 값 미변경 시 0 반환 (전송 안 함)
[ ] FRepLayout::ReceiveProperties → offset 기반 멤버 변수 직접 쓰기
[ ] FRepLayout::TakeSnapshot → 현재 상태 바이트 복사
[ ] CNetBitWriter/Reader → 왕복 직렬화 (Write → Read → 원본 동일)
[ ] CNetDriver::ReplicateActors → 모든 replicating 액터 순회
[ ] CNetDriver::ReplicateActors → Relevancy 필터 적용
[ ] CNetDriver::ReceiveReplication → netId 로 액터 검색 + 프로퍼티 적용
[ ] CNetDriver::ProcessIncomingRPC → RPC 이름 매칭 + invoker 실행
[ ] CNetDriver::SendRPC → actorNetId + rpcName + args 직렬화
[ ] RPCDescriptor::invoker → 정적 함수로 인자 역직렬화 + 멤버 함수 호출
[ ] NetRelevancy::IsRelevantByDistance → 거리 기반 필터링
[ ] 종료 마커 0xFF → ReceiveProperties 정상 종료
[ ] 기존 FlatBuffers Snapshot/Command/Event 무변경 (병존)
[ ] LoL 빌드 통과 (신규 파일 추가만, 기존 코드 무변경)
```

---

## 6. Migration Strategy

### Phase 1: 인프라 추가 (기존 코드 무변경)
- FRepLayout, CNetDriver, CNetBitWriter/Reader, NetMacros 엔진에 추가
- 빌드 통과 확인

### Phase 2: WChampionActor 에 WPROPERTY(Replicated) 적용
- WChampionActor 의 Replicated 프로퍼티로 delta 직렬화 테스트
- 단일 프로세스 내 Server→Client 루프백 검증

### Phase 3: CGameRoom 에 CNetDriver 통합
- Phase_BroadcastSnapshot 에서 기존 + delta 동시 전송
- 클라에서 기존 apply + delta apply 동시 실행
- 대역폭 비교 로깅 (기존 전체 vs delta only)

### Phase 4: 기존 수동 코드 점진 제거
- delta 검증 완료 후 SnapshotBuilder 의 개별 필드 제거
- Snapshot.fbs 간소화
- SnapshotApplier 에서 delta-only 경로 활성화

### Phase 5: RPC 도입
- 기존 CommandPacket (Move/CastSkill/BasicAttack) → WRPC(Server) 매크로로 교체
- 기존 EventPacket (Damage/BuffApply/SkillCast) → WRPC(Client/Multicast) 로 교체
- Command.fbs / Event.fbs 는 fallback 으로 유지
