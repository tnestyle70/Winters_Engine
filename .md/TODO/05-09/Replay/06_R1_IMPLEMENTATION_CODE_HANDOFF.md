# Replay R1 Implementation Code Handoff

작성일: 2026-05-09

이 문서는 R1 Server Replay Recorder를 사용자가 직접 반영할 수 있도록 파일별 코드와 삽입 위치를 제시한다.

---

## 0. 먼저 보여줄 진행 순서

```text
R1-1 Shared/Replay/ReplayFormat.h 확인
     R0에서 만든 .wrpl binary contract를 Server도 include 한다.

R1-2 Server/Public/Game/ReplayRecorder.h 생성
     서버 recorder public API 작성.

R1-3 Server/Private/Game/ReplayRecorder.cpp 생성
     snapshot/event record, local .wrpl save 구현.

R1-4 Server/Public/Game/GameRoom.h 수정
     recorder forward declaration, finalize method, member 추가.

R1-5 Server/Private/Game/GameRoom.cpp 수정
     Create/Stop/Event/Snapshot hook 연결.

R1-6 Server.vcxproj / .filters 수정
     신규 파일 등록.

R1-7 Smoke
     InGame 후 서버 종료 시 Replay/*.wrpl 생성 확인.
```

---

## 1. 전제

R1은 R0의 이 파일을 전제로 한다.

```text
Shared/Replay/ReplayFormat.h
```

아직 실제 코드베이스에 없다면 먼저 R0 문서의 `ReplayFormat.h`를 추가한다.

---

## 2. Create: Server/Public/Game/ReplayRecorder.h

```cpp
#pragma once

#include "WintersTypes.h"
#include "Shared/Replay/ReplayFormat.h"

#include <memory>
#include <string>
#include <vector>

class CReplayRecorder final
{
public:
    static std::unique_ptr<CReplayRecorder> Create(u32_t roomID, u32_t tickRate);

    void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
    void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);

    bool_t SaveToFile(const wstring_t& path, std::string& outError) const;
    wstring_t MakeDefaultPath() const;

    bool_t IsEmpty() const { return m_Records.empty(); }
    u32_t GetRoomID() const { return m_iRoomID; }
    u32_t GetTickRate() const { return m_iTickRate; }
    u32_t GetRecordCount() const { return static_cast<u32_t>(m_Records.size()); }
    u32_t GetSnapshotCount() const { return m_iSnapshotCount; }
    u32_t GetEventCount() const { return m_iEventCount; }
    u64_t GetFirstTick() const { return m_iFirstTick; }
    u64_t GetLastTick() const { return m_iLastTick; }

private:
    struct Record
    {
        Winters::Replay::ReplayRecordHeader header{};
        std::vector<u8_t> payload{};
    };

    CReplayRecorder(u32_t roomID, u32_t tickRate);

    void Record(Winters::Replay::eReplayRecordType type, u64_t tick, const u8_t* bytes, u32_t len);

    u32_t m_iRoomID = 0;
    u32_t m_iTickRate = 0;
    std::vector<Record> m_Records{};
    u32_t m_iSnapshotCount = 0;
    u32_t m_iEventCount = 0;
    u64_t m_iFirstTick = 0;
    u64_t m_iLastTick = 0;
};
```

---

## 3. Create: Server/Private/Game/ReplayRecorder.cpp

```cpp
#include "Game/ReplayRecorder.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    u64_t NowUnixMs()
    {
        using namespace std::chrono;
        return static_cast<u64_t>(duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count());
    }
}

std::unique_ptr<CReplayRecorder> CReplayRecorder::Create(u32_t roomID, u32_t tickRate)
{
    return std::unique_ptr<CReplayRecorder>(new CReplayRecorder(roomID, tickRate));
}

CReplayRecorder::CReplayRecorder(u32_t roomID, u32_t tickRate)
    : m_iRoomID(roomID)
    , m_iTickRate(tickRate)
{
}

void CReplayRecorder::RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len)
{
    Record(Winters::Replay::eReplayRecordType::Snapshot, tick, bytes, len);
}

void CReplayRecorder::RecordEvent(u64_t tick, const u8_t* bytes, u32_t len)
{
    Record(Winters::Replay::eReplayRecordType::Event, tick, bytes, len);
}

void CReplayRecorder::Record(
    Winters::Replay::eReplayRecordType type,
    u64_t tick,
    const u8_t* bytes,
    u32_t len)
{
    if (!bytes || len == 0)
        return;

    if (m_Records.empty())
        m_iFirstTick = tick;
    m_iLastTick = tick;

    Record record{};
    record.header.type = static_cast<u8_t>(type);
    record.header.payloadSize = len;
    record.header.serverTick = tick;
    record.header.sequence = static_cast<u32_t>(tick & 0xFFFFFFFFull);
    record.payload.assign(bytes, bytes + len);
    m_Records.emplace_back(std::move(record));

    if (type == Winters::Replay::eReplayRecordType::Snapshot)
        ++m_iSnapshotCount;
    else if (type == Winters::Replay::eReplayRecordType::Event)
        ++m_iEventCount;
}

wstring_t CReplayRecorder::MakeDefaultPath() const
{
    std::wstringstream ss;
    ss << L"Replay/room" << m_iRoomID
       << L"_tick" << m_iFirstTick
       << L"_" << m_iLastTick
       << L".wrpl";
    return ss.str();
}

bool_t CReplayRecorder::SaveToFile(const wstring_t& path, std::string& outError) const
{
    outError.clear();
    if (m_Records.empty())
    {
        outError = "no replay records";
        return false;
    }

    try
    {
        const std::filesystem::path fsPath(path);
        if (fsPath.has_parent_path())
            std::filesystem::create_directories(fsPath.parent_path());

        std::ofstream out(fsPath, std::ios::binary);
        if (!out)
        {
            outError = "failed to open replay file";
            return false;
        }

        Winters::Replay::ReplayFileHeader header{};
        header.recordCount = static_cast<u32_t>(m_Records.size());
        header.snapshotCount = m_iSnapshotCount;
        header.eventCount = m_iEventCount;
        header.firstTick = m_iFirstTick;
        header.lastTick = m_iLastTick;
        header.createdUnixMs = NowUnixMs();

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        for (const Record& record : m_Records)
        {
            out.write(reinterpret_cast<const char*>(&record.header), sizeof(record.header));
            out.write(
                reinterpret_cast<const char*>(record.payload.data()),
                static_cast<std::streamsize>(record.payload.size()));
        }

        if (!out.good())
        {
            outError = "failed to write replay file";
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        outError = e.what();
        return false;
    }
}
```

---

## 4. Patch: Server/Public/Game/GameRoom.h

forward declaration 추가:

```cpp
class CReplayRecorder;
```

private method 추가:

```cpp
void FinalizeReplayRecorder();
```

member 추가 위치: `m_pSnapBuilder` 근처.

```cpp
std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
bool_t m_bReplayFinalized = false;
```

예시:

```cpp
std::unique_ptr<ICommandExecutor> m_pExecutor;
std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;
std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
bool_t m_bReplayFinalized = false;
std::unique_ptr<Engine::CSpatialHashSystem> m_pSpatialSystem;
```

---

## 5. Patch: Server/Private/Game/GameRoom.cpp

include 추가:

```cpp
#include "Game/ReplayRecorder.h"
```

### 5.1 CGameRoom::Create

`m_pSnapBuilder` 생성 직후 추가:

```cpp
room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
```

예시:

```cpp
std::unique_ptr<CGameRoom> CGameRoom::Create(u32_t roomId)
{
    auto room = std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}
```

### 5.2 CGameRoom::Stop

기존 `Stop()` 교체:

```cpp
void CGameRoom::Stop()
{
    const bool_t bWasRunning = m_bRunning.exchange(false);
    if (bWasRunning && m_tickThread.joinable())
        m_tickThread.join();

    FinalizeReplayRecorder();
}
```

함수 추가:

```cpp
void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder || m_pReplayRecorder->IsEmpty())
        return;

    const wstring_t path = m_pReplayRecorder->MakeDefaultPath();
    std::string error;
    if (m_pReplayRecorder->SaveToFile(path, error))
    {
        std::wcout << L"[Replay] saved " << path
            << L" records=" << m_pReplayRecorder->GetRecordCount()
            << L" snapshots=" << m_pReplayRecorder->GetSnapshotCount()
            << L" events=" << m_pReplayRecorder->GetEventCount()
            << L"\n";
    }
    else
    {
        std::cerr << "[Replay] save failed: " << error << "\n";
    }

    m_bReplayFinalized = true;
}
```

### 5.3 CGameRoom::BroadcastEventPayload

payload null check 직후, session loop 전에 추가:

```cpp
if (m_pReplayRecorder)
    m_pReplayRecorder->RecordEvent(sequence, payload, payloadSize);
```

최종 형태:

```cpp
void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
{
    if (!payload || payloadSize == 0)
        return;

    if (m_pReplayRecorder)
        m_pReplayRecorder->RecordEvent(sequence, payload, payloadSize);

    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession)
            continue;

        auto packet = WrapEnvelope(ePacketType::Event, sequence, payload, payloadSize);
        pSession->Send(std::move(packet));
    }
}
```

### 5.4 CGameRoom::Phase_BroadcastSnapshot

함수 시작 부분에 spectator snapshot record 추가:

```cpp
if (m_pReplayRecorder && m_pSnapBuilder)
{
    auto replaySnapshot = m_pSnapBuilder->Build(
        m_world,
        m_entityMap,
        tc.tickIndex,
        m_rng.GetState(),
        0,
        NULL_NET_ENTITY);

    m_pReplayRecorder->RecordSnapshot(
        tc.tickIndex,
        replaySnapshot.data(),
        static_cast<u32_t>(replaySnapshot.size()));
}
```

최종 일부:

```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    if (m_pReplayRecorder && m_pSnapBuilder)
    {
        auto replaySnapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            m_rng.GetState(),
            0,
            NULL_NET_ENTITY);

        m_pReplayRecorder->RecordSnapshot(
            tc.tickIndex,
            replaySnapshot.data(),
            static_cast<u32_t>(replaySnapshot.size()));
    }

    u32_t sentCount = 0;
    for (u32_t sid : m_sessionIds)
    {
        // existing per-session snapshot send path 유지
    }
}
```

---

## 6. Patch: Server/Include/Server.vcxproj

`ClCompile` ItemGroup에 추가:

```xml
<ClCompile Include="..\Private\Game\ReplayRecorder.cpp" />
```

`ClInclude` ItemGroup에 추가:

```xml
<ClInclude Include="..\Public\Game\ReplayRecorder.h" />
<ClInclude Include="..\..\Shared\Replay\ReplayFormat.h" />
```

---

## 7. Patch: Server/Include/Server.vcxproj.filters

Filter ItemGroup에 추가:

```xml
<Filter Include="02. Game\07. Replay">
  <UniqueIdentifier>{67B9E61B-24FB-4C17-A452-B9448239B1E1}</UniqueIdentifier>
</Filter>
```

`ClCompile` ItemGroup에 추가:

```xml
<ClCompile Include="..\Private\Game\ReplayRecorder.cpp">
  <Filter>02. Game\07. Replay</Filter>
</ClCompile>
```

`ClInclude` ItemGroup에 추가:

```xml
<ClInclude Include="..\Public\Game\ReplayRecorder.h">
  <Filter>02. Game\07. Replay</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\Replay\ReplayFormat.h">
  <Filter>04. Shared</Filter>
</ClInclude>
```

---

## 8. Smoke

1. `Server` 빌드.
2. `Client` 1개 이상 접속.
3. Lobby/BanPick 흐름으로 InGame 진입.
4. 미니언/챔피언 이동, 스킬, 피해 event가 발생하도록 10초 이상 진행.
5. 서버 콘솔에서 `q` 입력.
6. 서버 로그 확인:

```text
[Replay] saved Replay/room1_tick1_1234.wrpl records=... snapshots=... events=...
```

7. 파일 확인:

```text
Server/Bin/Debug/Replay/*.wrpl
```

8. R0 `Scene_Replay`가 이미 반영되어 있으면 해당 `.wrpl` 파일을 `Client/Bin/Replay`로 복사해서 재생 smoke.

---

## 9. R1 Gotchas

- `BroadcastEventPayload`는 session loop 안에서 record하면 event가 접속자 수만큼 중복 저장된다. 반드시 loop 전에 한 번만 기록한다.
- `Phase_BroadcastSnapshot`에서 session-specific snapshot을 저장하면 `yourNetId`가 클라이언트마다 달라진다. replay 저장은 `NULL_NET_ENTITY` spectator snapshot으로 고정한다.
- `Stop()`에서 tick thread join 전 파일 저장을 하면 tick thread가 recorder에 쓰는 중일 수 있다. join 이후 finalize한다.
- R1은 파일 헤더에 `roomID/tickRate`를 추가하지 않는다. R0 player와 같은 `.wrpl` v1 contract를 유지한다.
- 장기 경기에서는 in-memory recorder가 커질 수 있다. R2 전후로 streaming writer 또는 chunked temp file 방식으로 교체한다.
