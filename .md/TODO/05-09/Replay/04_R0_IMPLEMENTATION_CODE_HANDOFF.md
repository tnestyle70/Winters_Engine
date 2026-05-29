# Replay R0 Implementation Code Handoff

작성일: 2026-05-09

이 문서는 사용자가 직접 반영할 수 있도록 Replay R0 클라이언트 로컬 저장/재생 구현 코드를 순서대로 제시한다.

---

## 0. 먼저 보여줄 진행 순서

```text
R0-1 Shared/Replay/ReplayFormat.h
     리플레이 파일 binary contract 작성

R0-2 Client/Public|Private/Replay/ReplayRecorder.*
     snapshot/event payload를 메모리에 누적하고 .wrpl 파일로 저장

R0-3 Client/Public|Private/Replay/ReplayLibrary.*
     Client/Bin/Replay 경로, 파일명 생성, 로컬 replay 목록 조회

R0-4 Scene_InGame + InGameNetworkBridge
     서버에서 받은 Snapshot/Event payload를 적용 직전에 recorder에 복사

R0-5 Client/Public|Private/Replay/ReplayPlayer.*
     .wrpl 파일을 읽고 tick 기준으로 Snapshot/Event를 재주입

R0-6 Scene_Replay
     Replay 전용 scene에서 CReplayPlayer + SnapshotApplier/EventApplier로 재생

R0-7 Scene_MainMenu + Defines
     MainMenu Replay 버튼과 eSceneID::Replay 진입 연결

R0-8 Client.vcxproj / .filters
     신규 .h/.cpp 등록 후 빌드

R0-9 Smoke
     InGame에서 Save Replay → 클라이언트 종료 → MainMenu Replay에서 재생
```

---

## 1. 방향성

- R0는 **클라이언트 로컬 리플레이**다.
- 서버 authoritative 상태가 이미 만들어내는 `Snapshot` / `Event` flatbuffer payload를 그대로 파일에 저장한다.
- 재생도 payload를 재해석하지 않고 기존 `CSnapshotApplier` / `CEventApplier`에 다시 넣는다.
- 미니언, 타워, 캐릭터 이동, 스킬, 킬 로그는 R0에서 별도 도메인 저장소를 만들지 않는다. 서버 snapshot/event가 이미 single source가 되도록 한다.
- R0 파일 확장자는 `.wrpl` 로 둔다.

---

## 2. 남은 계획

```text
R1 Server Replay Recorder
   Server/GameRoom tick 말단에서 authoritative snapshot/event stream을 저장한다.

R2 Backend Replay Ingest / Download
   match 결과와 replay object storage 경로를 연결한다.

R3 User Replay Library
   메인메뉴에서 matchID, champion, date, duration 기준으로 조회/다운로드/재생한다.
```

---

## 3. Create: Shared/Replay/ReplayFormat.h

```cpp
#pragma once

#include "WintersTypes.h"

#include <cstring>

namespace Winters::Replay
{
    inline constexpr char kReplayMagic[4] = { 'W', 'R', 'P', 'L' };
    inline constexpr u16_t kReplayVersion = 1;
    inline constexpr u16_t kReplayHeaderSize = 48;
    inline constexpr u16_t kReplayRecordHeaderSize = 24;

#pragma pack(push, 1)
    struct ReplayFileHeader
    {
        char magic[4] = { 'W', 'R', 'P', 'L' };
        u16_t version = kReplayVersion;
        u16_t headerSize = kReplayHeaderSize;
        u32_t flags = 0;
        u32_t recordCount = 0;
        u32_t snapshotCount = 0;
        u32_t eventCount = 0;
        u64_t firstTick = 0;
        u64_t lastTick = 0;
        u64_t createdUnixMs = 0;
    };

    enum class eReplayRecordType : u8_t
    {
        Snapshot = 1,
        Event = 2,
    };

    struct ReplayRecordHeader
    {
        u8_t type = 0;
        u8_t reserved0 = 0;
        u16_t headerSize = kReplayRecordHeaderSize;
        u32_t payloadSize = 0;
        u64_t serverTick = 0;
        u32_t sequence = 0;
        u32_t reserved1 = 0;
    };
#pragma pack(pop)

    static_assert(sizeof(ReplayFileHeader) == kReplayHeaderSize);
    static_assert(sizeof(ReplayRecordHeader) == kReplayRecordHeaderSize);

    inline bool_t IsReplayMagic(const ReplayFileHeader& header)
    {
        return std::memcmp(header.magic, kReplayMagic, sizeof(kReplayMagic)) == 0;
    }
}
```

---

## 4. Create: Client/Public/Replay/ReplayRecorder.h

```cpp
#pragma once

#include "Defines.h"
#include "Shared/Replay/ReplayFormat.h"

#include <string>
#include <vector>

class CReplayRecorder final
{
public:
    static std::unique_ptr<CReplayRecorder> Create();

    void Clear();
    void RecordSnapshot(u32_t sequence, const u8_t* payload, u32_t len);
    void RecordEvent(u32_t sequence, const u8_t* payload, u32_t len);

    bool_t SaveToFile(const wstring_t& path, std::string& outError) const;

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

    CReplayRecorder() = default;

    void Record(Winters::Replay::eReplayRecordType type, u32_t sequence, const u8_t* payload, u32_t len);
    static u64_t ExtractServerTick(Winters::Replay::eReplayRecordType type, const u8_t* payload, u32_t len);

    std::vector<Record> m_Records{};
    u32_t m_iSnapshotCount = 0;
    u32_t m_iEventCount = 0;
    u64_t m_iFirstTick = 0;
    u64_t m_iLastTick = 0;
};
```

---

## 5. Create: Client/Private/Replay/ReplayRecorder.cpp

```cpp
#include "Replay/ReplayRecorder.h"

#include "Shared/Schema/event_generated.h"
#include "Shared/Schema/snapshot_generated.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    u64_t NowUnixMs()
    {
        using namespace std::chrono;
        return static_cast<u64_t>(duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count());
    }
}

std::unique_ptr<CReplayRecorder> CReplayRecorder::Create()
{
    return std::unique_ptr<CReplayRecorder>(new CReplayRecorder());
}

void CReplayRecorder::Clear()
{
    m_Records.clear();
    m_iSnapshotCount = 0;
    m_iEventCount = 0;
    m_iFirstTick = 0;
    m_iLastTick = 0;
}

void CReplayRecorder::RecordSnapshot(u32_t sequence, const u8_t* payload, u32_t len)
{
    Record(Winters::Replay::eReplayRecordType::Snapshot, sequence, payload, len);
}

void CReplayRecorder::RecordEvent(u32_t sequence, const u8_t* payload, u32_t len)
{
    Record(Winters::Replay::eReplayRecordType::Event, sequence, payload, len);
}

void CReplayRecorder::Record(
    Winters::Replay::eReplayRecordType type,
    u32_t sequence,
    const u8_t* payload,
    u32_t len)
{
    if (!payload || len == 0)
        return;

    const u64_t serverTick = ExtractServerTick(type, payload, len);
    if (m_Records.empty())
        m_iFirstTick = serverTick;
    m_iLastTick = serverTick;

    Record record{};
    record.header.type = static_cast<u8_t>(type);
    record.header.payloadSize = len;
    record.header.serverTick = serverTick;
    record.header.sequence = sequence;
    record.payload.assign(payload, payload + len);
    m_Records.emplace_back(std::move(record));

    if (type == Winters::Replay::eReplayRecordType::Snapshot)
        ++m_iSnapshotCount;
    else if (type == Winters::Replay::eReplayRecordType::Event)
        ++m_iEventCount;
}

u64_t CReplayRecorder::ExtractServerTick(
    Winters::Replay::eReplayRecordType type,
    const u8_t* payload,
    u32_t len)
{
    if (type == Winters::Replay::eReplayRecordType::Snapshot)
    {
        flatbuffers::Verifier verifier(payload, len);
        if (Shared::Schema::VerifySnapshotBuffer(verifier))
            return Shared::Schema::GetSnapshot(payload)->serverTick();
    }
    else if (type == Winters::Replay::eReplayRecordType::Event)
    {
        flatbuffers::Verifier verifier(payload, len);
        if (Shared::Schema::VerifyEventPacketBuffer(verifier))
            return Shared::Schema::GetEventPacket(payload)->serverTick();
    }

    return m_iLastTick;
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
            out.write(reinterpret_cast<const char*>(record.payload.data()), record.payload.size());
        }

        return out.good();
    }
    catch (const std::exception& e)
    {
        outError = e.what();
        return false;
    }
}
```

---

## 6. Create: Client/Public/Replay/ReplayLibrary.h

```cpp
#pragma once

#include "Defines.h"

#include <string>
#include <vector>

struct ReplayListItem
{
    wstring_t path{};
    std::string displayName{};
    u32_t recordCount = 0;
    u32_t snapshotCount = 0;
    u32_t eventCount = 0;
    u64_t firstTick = 0;
    u64_t lastTick = 0;
};

class CReplayLibrary final
{
public:
    static wstring_t GetReplayDirectory();
    static wstring_t MakeReplayPath(u64_t firstTick, u32_t snapshotCount, u32_t eventCount);
    static std::vector<ReplayListItem> ListLocalReplays();
};
```

---

## 7. Create: Client/Private/Replay/ReplayLibrary.cpp

```cpp
#include "Replay/ReplayLibrary.h"

#include "Shared/Replay/ReplayFormat.h"

#include <filesystem>
#include <fstream>
#include <sstream>

wstring_t CReplayLibrary::GetReplayDirectory()
{
    return L"Client/Bin/Replay";
}

wstring_t CReplayLibrary::MakeReplayPath(u64_t firstTick, u32_t snapshotCount, u32_t eventCount)
{
    std::wstringstream ss;
    ss << GetReplayDirectory()
       << L"/replay_tick" << firstTick
       << L"_s" << snapshotCount
       << L"_e" << eventCount
       << L".wrpl";
    return ss.str();
}

std::vector<ReplayListItem> CReplayLibrary::ListLocalReplays()
{
    std::vector<ReplayListItem> items;
    const std::filesystem::path dir(GetReplayDirectory());
    if (!std::filesystem::exists(dir))
        return items;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != L".wrpl")
            continue;

        std::ifstream in(entry.path(), std::ios::binary);
        Winters::Replay::ReplayFileHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!in || !Winters::Replay::IsReplayMagic(header))
            continue;

        ReplayListItem item{};
        item.path = entry.path().wstring();
        item.displayName = entry.path().filename().string();
        item.recordCount = header.recordCount;
        item.snapshotCount = header.snapshotCount;
        item.eventCount = header.eventCount;
        item.firstTick = header.firstTick;
        item.lastTick = header.lastTick;
        items.emplace_back(std::move(item));
    }

    return items;
}
```

---

## 8. Patch: Client/Public/Scene/InGameNetworkBridge.h

```cpp
class CReplayRecorder;
```

`InGameNetworkBridgeDesc` 끝에 추가:

```cpp
CReplayRecorder* pReplayRecorder = nullptr;
```

---

## 9. Patch: Client/Private/Scene/InGameNetworkBridge.cpp

include 추가:

```cpp
#include "Replay/ReplayRecorder.h"
```

`Initialize`에서 callback capture 직전 추가:

```cpp
auto* pReplayRecorder = desc.pReplayRecorder;
```

lambda capture에 추가:

```cpp
pReplayRecorder
```

`(void)sequence;` 제거 또는 사용 처리.

Snapshot 분기에서 적용 직전:

```cpp
if (pReplayRecorder)
    pReplayRecorder->RecordSnapshot(sequence, payload, len);

snapshotApplier.OnSnapshot(*pWorld, entityMap, payload, len);
```

Event 분기에서 적용 직전:

```cpp
if (pReplayRecorder)
    pReplayRecorder->RecordEvent(sequence, payload, len);

eventApplier.OnEvent(*pWorld, entityMap, payload, len);
```

---

## 10. Patch: Client/Public/Scene/Scene_InGame.h

forward declaration 추가:

```cpp
class CReplayRecorder;
```

private method 추가:

```cpp
void DrawReplayCapturePanel();
void SaveReplayCapture();
```

network member 근처에 추가:

```cpp
std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
std::string m_strReplayStatus{};
```

---

## 11. Patch: Client/Private/Scene/InGameBootstrapBridge.cpp

include 추가:

```cpp
#include "Replay/ReplayRecorder.h"
```

`Enter` 초반 로그 뒤에 추가:

```cpp
scene.m_pReplayRecorder = CReplayRecorder::Create();
scene.m_strReplayStatus = "Replay capture ready";
```

`InGameNetworkBridgeDesc networkDesc{ ... };` 생성 직후, `Initialize` 호출 직전에 추가:

```cpp
networkDesc.pReplayRecorder = scene.m_pReplayRecorder.get();
```

---

## 12. Patch: Client/Private/Scene/Scene_InGame.cpp

include 추가:

```cpp
#include "Replay/ReplayLibrary.h"
#include "Replay/ReplayRecorder.h"
```

`OnImGui`에서 AI panel 뒤, legacy return 전에 호출:

```cpp
if (m_pReplayRecorder)
    DrawReplayCapturePanel();
```

`OnExit` 시작 전에 저장하지 않은 메모리만 해제:

```cpp
m_pReplayRecorder.reset();
m_strReplayStatus.clear();
```

함수 추가:

```cpp
void CScene_InGame::DrawReplayCapturePanel()
{
    ImGui::Begin("Replay Capture");

    if (!m_pReplayRecorder)
    {
        ImGui::TextDisabled("Replay recorder is not ready");
        ImGui::End();
        return;
    }

    ImGui::Text("Records: %u", m_pReplayRecorder->GetRecordCount());
    ImGui::Text("Snapshots: %u", m_pReplayRecorder->GetSnapshotCount());
    ImGui::Text("Events: %u", m_pReplayRecorder->GetEventCount());
    ImGui::Text("Tick: %llu -> %llu",
        static_cast<unsigned long long>(m_pReplayRecorder->GetFirstTick()),
        static_cast<unsigned long long>(m_pReplayRecorder->GetLastTick()));

    const bool_t bCanSave = m_pReplayRecorder->GetRecordCount() > 0;
    if (!bCanSave)
        ImGui::BeginDisabled();

    if (ImGui::Button("Save Replay"))
        SaveReplayCapture();

    if (!bCanSave)
        ImGui::EndDisabled();

    if (!m_strReplayStatus.empty())
        ImGui::TextWrapped("%s", m_strReplayStatus.c_str());

    ImGui::End();
}

void CScene_InGame::SaveReplayCapture()
{
    if (!m_pReplayRecorder)
        return;

    const wstring_t path = CReplayLibrary::MakeReplayPath(
        m_pReplayRecorder->GetFirstTick(),
        m_pReplayRecorder->GetSnapshotCount(),
        m_pReplayRecorder->GetEventCount());

    std::string error;
    if (m_pReplayRecorder->SaveToFile(path, error))
        m_strReplayStatus = "Replay saved to Client/Bin/Replay";
    else
        m_strReplayStatus = "Replay save failed: " + error;
}
```

---

## 13. Create: Client/Public/Replay/ReplayPlayer.h

```cpp
#pragma once

#include "Defines.h"
#include "Shared/Replay/ReplayFormat.h"

#include <string>
#include <vector>

class CEventApplier;
class CSnapshotApplier;
class CWorld;
class EntityIdMap;

class CReplayPlayer final
{
public:
    static std::unique_ptr<CReplayPlayer> Create();

    bool_t LoadFromFile(const wstring_t& path, std::string& outError);
    void ResetPlayback();
    void SetPlaying(bool_t playing) { m_bPlaying = playing; }
    void SetSpeed(f32_t speed) { m_fSpeed = speed < 0.1f ? 0.1f : speed; }
    void Update(f32_t dt, CWorld& world, EntityIdMap& entityMap, CSnapshotApplier& snapshotApplier, CEventApplier& eventApplier);

    bool_t IsLoaded() const { return m_bLoaded; }
    bool_t IsPlaying() const { return m_bPlaying; }
    f32_t GetSpeed() const { return m_fSpeed; }
    u32_t GetCursor() const { return m_iCursor; }
    u32_t GetRecordCount() const { return static_cast<u32_t>(m_Records.size()); }
    u64_t GetCurrentTick() const { return m_iCurrentTick; }

private:
    struct Record
    {
        Winters::Replay::ReplayRecordHeader header{};
        std::vector<u8_t> payload{};
    };

    CReplayPlayer() = default;
    void ApplyRecord(const Record& record, CWorld& world, EntityIdMap& entityMap, CSnapshotApplier& snapshotApplier, CEventApplier& eventApplier);

    std::vector<Record> m_Records{};
    bool_t m_bLoaded = false;
    bool_t m_bPlaying = false;
    f32_t m_fSpeed = 1.f;
    f32_t m_fTickAccumulator = 0.f;
    u32_t m_iCursor = 0;
    u64_t m_iFirstTick = 0;
    u64_t m_iCurrentTick = 0;
};
```

---

## 14. Create: Client/Private/Replay/ReplayPlayer.cpp

```cpp
#include "Replay/ReplayPlayer.h"

#include "Network/Client/EventApplier.h"
#include "Network/Client/SnapshotApplier.h"
#include "Shared/GameSim/EntityIdMap.h"

#include <filesystem>
#include <fstream>

std::unique_ptr<CReplayPlayer> CReplayPlayer::Create()
{
    return std::unique_ptr<CReplayPlayer>(new CReplayPlayer());
}

bool_t CReplayPlayer::LoadFromFile(const wstring_t& path, std::string& outError)
{
    outError.clear();
    m_Records.clear();
    m_bLoaded = false;
    m_bPlaying = false;
    m_iCursor = 0;

    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in)
    {
        outError = "failed to open replay file";
        return false;
    }

    Winters::Replay::ReplayFileHeader fileHeader{};
    in.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (!in || !Winters::Replay::IsReplayMagic(fileHeader) || fileHeader.version != Winters::Replay::kReplayVersion)
    {
        outError = "invalid replay header";
        return false;
    }

    m_Records.reserve(fileHeader.recordCount);
    for (u32_t i = 0; i < fileHeader.recordCount; ++i)
    {
        Record record{};
        in.read(reinterpret_cast<char*>(&record.header), sizeof(record.header));
        if (!in || record.header.payloadSize == 0)
        {
            outError = "invalid replay record";
            return false;
        }

        record.payload.resize(record.header.payloadSize);
        in.read(reinterpret_cast<char*>(record.payload.data()), record.payload.size());
        if (!in)
        {
            outError = "truncated replay payload";
            return false;
        }

        m_Records.emplace_back(std::move(record));
    }

    m_iFirstTick = fileHeader.firstTick;
    m_iCurrentTick = fileHeader.firstTick;
    m_bLoaded = true;
    return true;
}

void CReplayPlayer::ResetPlayback()
{
    m_iCursor = 0;
    m_iCurrentTick = m_iFirstTick;
    m_fTickAccumulator = 0.f;
}

void CReplayPlayer::Update(
    f32_t dt,
    CWorld& world,
    EntityIdMap& entityMap,
    CSnapshotApplier& snapshotApplier,
    CEventApplier& eventApplier)
{
    if (!m_bLoaded || !m_bPlaying || m_iCursor >= m_Records.size())
        return;

    constexpr f32_t kReplayTickRate = 30.f;
    m_fTickAccumulator += dt * m_fSpeed * kReplayTickRate;
    const u64_t ticksToAdvance = static_cast<u64_t>(m_fTickAccumulator);
    if (ticksToAdvance == 0)
        return;

    m_fTickAccumulator -= static_cast<f32_t>(ticksToAdvance);
    m_iCurrentTick += ticksToAdvance;

    while (m_iCursor < m_Records.size() && m_Records[m_iCursor].header.serverTick <= m_iCurrentTick)
    {
        ApplyRecord(m_Records[m_iCursor], world, entityMap, snapshotApplier, eventApplier);
        ++m_iCursor;
    }
}

void CReplayPlayer::ApplyRecord(
    const Record& record,
    CWorld& world,
    EntityIdMap& entityMap,
    CSnapshotApplier& snapshotApplier,
    CEventApplier& eventApplier)
{
    const auto type = static_cast<Winters::Replay::eReplayRecordType>(record.header.type);
    if (type == Winters::Replay::eReplayRecordType::Snapshot)
        snapshotApplier.OnSnapshot(world, entityMap, record.payload.data(), record.header.payloadSize);
    else if (type == Winters::Replay::eReplayRecordType::Event)
        eventApplier.OnEvent(world, entityMap, record.payload.data(), record.header.payloadSize);
}
```

---

## 15. Create: Client/Public/Scene/Scene_Replay.h

```cpp
#pragma once

#include "IScene.h"
#include "ECS/World.h"

#include <memory>
#include <string>

class CEventApplier;
class CReplayPlayer;
class CSnapshotApplier;
class EntityIdMap;

class CScene_Replay final : public IScene
{
public:
    static std::unique_ptr<CScene_Replay> Create(const wstring_t& replayPath);
    ~CScene_Replay() override = default;

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t dt) override {}
    void OnRender() override {}
    void OnImGui() override;

private:
    explicit CScene_Replay(wstring_t replayPath);
    void BackToMainMenu();

    wstring_t m_strReplayPath{};
    std::string m_strStatus{};
    CWorld m_World{};
    std::unique_ptr<EntityIdMap> m_pEntityIdMap{};
    std::unique_ptr<CSnapshotApplier> m_pSnapshotApplier{};
    std::unique_ptr<CEventApplier> m_pEventApplier{};
    std::unique_ptr<CReplayPlayer> m_pReplayPlayer{};
};
```

---

## 16. Create: Client/Private/Scene/Scene_Replay.cpp

```cpp
#include "Scene/Scene_Replay.h"

#include "Defines.h"
#include "GameInstance.h"
#include "Network/Client/EventApplier.h"
#include "Network/Client/SnapshotApplier.h"
#include "Replay/ReplayPlayer.h"
#include "Scene/Scene_MainMenu.h"
#include "Shared/GameSim/EntityIdMap.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

std::unique_ptr<CScene_Replay> CScene_Replay::Create(const wstring_t& replayPath)
{
    return std::unique_ptr<CScene_Replay>(new CScene_Replay(replayPath));
}

CScene_Replay::CScene_Replay(wstring_t replayPath)
    : m_strReplayPath(std::move(replayPath))
{
}

bool CScene_Replay::OnEnter()
{
    m_pEntityIdMap = std::make_unique<EntityIdMap>();
    m_pSnapshotApplier = CSnapshotApplier::Create();
    m_pEventApplier = CEventApplier::Create();
    m_pReplayPlayer = CReplayPlayer::Create();

    if (!m_pSnapshotApplier || !m_pEventApplier || !m_pReplayPlayer)
    {
        m_strStatus = "Replay scene init failed";
        return true;
    }

    m_pSnapshotApplier->SetOnNewEntityCallback(
        [this](u32_t, u8_t, u8_t) -> EntityID
        {
            return m_World.CreateEntity();
        });

    std::string error;
    if (!m_pReplayPlayer->LoadFromFile(m_strReplayPath, error))
    {
        m_strStatus = "Replay load failed: " + error;
        return true;
    }

    m_pReplayPlayer->SetPlaying(true);
    m_strStatus = "Replay loaded";
    return true;
}

void CScene_Replay::OnExit()
{
    m_pReplayPlayer.reset();
    m_pEventApplier.reset();
    m_pSnapshotApplier.reset();
    m_pEntityIdMap.reset();
}

void CScene_Replay::OnUpdate(f32_t dt)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return;

    m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier);
}

void CScene_Replay::OnImGui()
{
    ImGui::Begin("Replay");

    ImGui::TextWrapped("%s", m_strStatus.c_str());
    if (m_pReplayPlayer && m_pReplayPlayer->IsLoaded())
    {
        ImGui::Text("Tick: %llu", static_cast<unsigned long long>(m_pReplayPlayer->GetCurrentTick()));
        ImGui::Text("Record: %u / %u", m_pReplayPlayer->GetCursor(), m_pReplayPlayer->GetRecordCount());

        if (ImGui::Button(m_pReplayPlayer->IsPlaying() ? "Pause" : "Play"))
            m_pReplayPlayer->SetPlaying(!m_pReplayPlayer->IsPlaying());

        ImGui::SameLine();
        if (ImGui::Button("Restart"))
            m_pReplayPlayer->ResetPlayback();

        f32_t speed = m_pReplayPlayer->GetSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 4.f))
            m_pReplayPlayer->SetSpeed(speed);
    }

    if (ImGui::Button("Back"))
        BackToMainMenu();

    ImGui::End();
}

void CScene_Replay::BackToMainMenu()
{
    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MainMenu),
        CScene_MainMenu::Create());
}
```

---

## 17. Patch: Client/Public/Defines.h

`InGame` 뒤에 추가:

```cpp
Replay,
```

최종 일부:

```cpp
InGame,
Replay,
Editor,
Result,
```

---

## 18. Patch: Client/Public/Scene/Scene_MainMenu.h

include는 그대로 두고 private method 추가:

```cpp
void DrawReplayPanel();
void RequestReplay(const wstring_t& path);
```

member 추가:

```cpp
wstring_t m_selectedReplayPath{};
```

`ePanel`에 추가:

```cpp
Replay,
```

---

## 19. Patch: Client/Private/Scene/Scene_MainMenu.cpp

include 추가:

```cpp
#include "Replay/ReplayLibrary.h"
#include "Scene/Scene_Replay.h"
```

switch 추가:

```cpp
case ePanel::Replay:
    DrawReplayPanel();
    break;
```

navigation에 Settings 앞이나 뒤 추가:

```cpp
if (ImGui::Button("Replay"))
    m_ePanel = ePanel::Replay;
ImGui::SameLine();
```

home panel에 추가:

```cpp
if (ImGui::Button("Replay", ImVec2(220.f, 36.f)))
    m_ePanel = ePanel::Replay;
```

함수 추가:

```cpp
void CScene_MainMenu::DrawReplayPanel()
{
    const std::vector<ReplayListItem> items = CReplayLibrary::ListLocalReplays();

    if (items.empty())
    {
        ImGui::TextDisabled("No local replay files");
        return;
    }

    for (const ReplayListItem& item : items)
    {
        const bool_t bSelected = item.path == m_selectedReplayPath;
        if (ImGui::Selectable(item.displayName.c_str(), bSelected))
            m_selectedReplayPath = item.path;

        ImGui::SameLine();
        ImGui::TextDisabled("records=%u snapshots=%u events=%u",
            item.recordCount,
            item.snapshotCount,
            item.eventCount);
    }

    const bool_t bCanPlay = !m_selectedReplayPath.empty();
    if (!bCanPlay)
        ImGui::BeginDisabled();

    if (ImGui::Button("Play Replay", ImVec2(220.f, 42.f)))
        RequestReplay(m_selectedReplayPath);

    if (!bCanPlay)
        ImGui::EndDisabled();
}

void CScene_MainMenu::RequestReplay(const wstring_t& path)
{
    if (path.empty())
        return;

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::Replay),
        CScene_Replay::Create(path));
}
```

---

## 20. Patch: Client/Include/Client.vcxproj

`ClCompile` ItemGroup에 추가:

```xml
<ClCompile Include="..\Private\Replay\ReplayLibrary.cpp" />
<ClCompile Include="..\Private\Replay\ReplayPlayer.cpp" />
<ClCompile Include="..\Private\Replay\ReplayRecorder.cpp" />
<ClCompile Include="..\Private\Scene\Scene_Replay.cpp" />
```

`ClInclude` ItemGroup에 추가:

```xml
<ClInclude Include="..\..\Shared\Replay\ReplayFormat.h" />
<ClInclude Include="..\Public\Replay\ReplayLibrary.h" />
<ClInclude Include="..\Public\Replay\ReplayPlayer.h" />
<ClInclude Include="..\Public\Replay\ReplayRecorder.h" />
<ClInclude Include="..\Public\Scene\Scene_Replay.h" />
```

---

## 21. Patch: Client/Include/Client.vcxproj.filters

필터 추가:

```xml
<Filter Include="12. Replay">
  <UniqueIdentifier>{B5C7F680-9A5B-4B2A-8E3B-8B88F9705001}</UniqueIdentifier>
</Filter>
```

Replay 파일:

```xml
<ClCompile Include="..\Private\Replay\ReplayLibrary.cpp">
  <Filter>12. Replay</Filter>
</ClCompile>
<ClCompile Include="..\Private\Replay\ReplayPlayer.cpp">
  <Filter>12. Replay</Filter>
</ClCompile>
<ClCompile Include="..\Private\Replay\ReplayRecorder.cpp">
  <Filter>12. Replay</Filter>
</ClCompile>
<ClCompile Include="..\Private\Scene\Scene_Replay.cpp">
  <Filter>01. Scene</Filter>
</ClCompile>
<ClInclude Include="..\..\Shared\Replay\ReplayFormat.h">
  <Filter>12. Replay</Filter>
</ClInclude>
<ClInclude Include="..\Public\Replay\ReplayLibrary.h">
  <Filter>12. Replay</Filter>
</ClInclude>
<ClInclude Include="..\Public\Replay\ReplayPlayer.h">
  <Filter>12. Replay</Filter>
</ClInclude>
<ClInclude Include="..\Public\Replay\ReplayRecorder.h">
  <Filter>12. Replay</Filter>
</ClInclude>
<ClInclude Include="..\Public\Scene\Scene_Replay.h">
  <Filter>01. Scene</Filter>
</ClInclude>
```

---

## 22. 검증 순서

```text
1. Client 빌드
2. Server + Client 로 InGame 진입
3. Replay Capture 창에서 Records/Snapshots/Events 증가 확인
4. Save Replay 클릭
5. Client/Bin/Replay/*.wrpl 생성 확인
6. 클라이언트 종료 후 재실행
7. MainMenu > Replay
8. 목록에 저장 파일 표시 확인
9. Play Replay 클릭
10. Scene_Replay에서 tick/record cursor 증가 확인
```

---

## 23. R0 한계

- 이 구현은 replay payload 저장/재주입 인프라 우선이다.
- `CScene_Replay`의 시각 렌더링은 최소 연결만 한다. 실제 인게임과 동일한 카메라/렌더/챔피언 spawn visual parity는 R0 후속 작업으로 둔다.
- R1부터는 클라이언트 저장이 아니라 서버 authoritative 저장을 원천으로 한다.
