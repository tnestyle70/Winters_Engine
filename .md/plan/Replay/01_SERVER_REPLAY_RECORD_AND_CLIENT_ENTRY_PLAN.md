Session - InGame 중 서버가 관전자 기준 full snapshot/event를 WRPL로 자동 녹화하고, Stop에서 저장 결과를 로그로 남기며, 클라가 WRPL을 읽어 기존 applier로 재생할 진입 토대를 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`CGameRoom::BroadcastEventPayload`는 내부에 session fan-out 루프를 가지므로, event당 1번만 기록하려면 루프 진입 전 최상단에서 RecordEvent를 호출한다.

기존 코드:

```cpp
void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
{
    if (!payload || payloadSize == 0)
        return;

    for (u32_t sid : m_sessionIds)
```

아래로 교체:

```cpp
void CGameRoom::BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence)
{
    if (!payload || payloadSize == 0)
        return;

    if (m_pReplayRecorder && !m_bReplayFinalized)
        m_pReplayRecorder->RecordEvent(sequence, payload, payloadSize);

    for (u32_t sid : m_sessionIds)
```

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`CGameRoom::Phase_BroadcastSnapshot`는 세션별 snapshot을 보낸다. replay용은 세션과 무관하게 tick마다 관전자 기준(`lastAckedSeq = 0`, `yourNetId = NULL_NET_ENTITY`) full snapshot 1개를 만들어 기록한다. 접속자 0명이어도 기록되어야 하므로 session 루프 바깥에 둔다.

기존 코드:

```cpp
        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
    }
}
```

아래로 교체:

```cpp
        auto packet = WrapEnvelope(
            ePacketType::Snapshot,
            static_cast<u32_t>(tc.tickIndex),
            snapshot.data(),
            static_cast<u32_t>(snapshot.size()));
        pSession->Send(std::move(packet));
    }

    if (m_pReplayRecorder && !m_bReplayFinalized)
    {
        auto spectatorSnapshot = m_pSnapBuilder->Build(
            m_world,
            m_entityMap,
            tc.tickIndex,
            ResolveServerGameTimeMs(tc.tickIndex),
            m_rng.GetState(),
            0u,
            NULL_NET_ENTITY);

        m_pReplayRecorder->RecordSnapshot(
            tc.tickIndex,
            spectatorSnapshot.data(),
            static_cast<u32_t>(spectatorSnapshot.size()));
    }
}
```

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`CGameRoom::FinalizeReplayRecorder`는 이미 존재하지만 저장 성공/실패/empty 로그가 없다. 저장 결과를 항상 보이도록 직접 `OutputDebugStringA/W`로 남긴다. (`WintersOutputAIDebugStringA`는 `WINTERS_ENABLE_AI_DEBUG_STRING` 게이트라 replay 로그에는 쓰지 않는다.)

기존 코드:

```cpp
void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder)
        return;

    m_bReplayFinalized = true;
    if (m_pReplayRecorder->IsEmpty())
        return;

    std::string error;
    const wstring_t path = m_pReplayRecorder->MakeDefaultPath();
    (void)m_pReplayRecorder->SaveToFile(path, error);
}
```

아래로 교체:

```cpp
void CGameRoom::FinalizeReplayRecorder()
{
    if (m_bReplayFinalized || !m_pReplayRecorder)
        return;

    m_bReplayFinalized = true;

    if (m_pReplayRecorder->IsEmpty())
    {
        char msg[160]{};
        sprintf_s(msg, "[Replay] room=%u finalize skipped: no records\n", m_roomId);
        ::OutputDebugStringA(msg);
        return;
    }

    std::string error;
    const wstring_t path = m_pReplayRecorder->MakeDefaultPath();
    const bool_t bSaved = m_pReplayRecorder->SaveToFile(path, error);

    char msg[256]{};
    if (bSaved)
    {
        sprintf_s(msg,
            "[Replay] room=%u saved records=%u snapshots=%u events=%u ticks=%llu..%llu\n",
            m_roomId,
            m_pReplayRecorder->GetRecordCount(),
            m_pReplayRecorder->GetSnapshotCount(),
            m_pReplayRecorder->GetEventCount(),
            static_cast<unsigned long long>(m_pReplayRecorder->GetFirstTick()),
            static_cast<unsigned long long>(m_pReplayRecorder->GetLastTick()));
    }
    else
    {
        sprintf_s(msg, "[Replay] room=%u save FAILED: %s\n",
            m_roomId, error.c_str());
    }
    ::OutputDebugStringA(msg);

    ::OutputDebugStringW(L"[Replay] path=");
    ::OutputDebugStringW(path.c_str());
    ::OutputDebugStringW(L"\n");
}
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Replay/ReplayReader.h

새 파일:

```cpp
#pragma once

#include "Shared/Replay/ReplayFormat.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

class CReplayReader final
{
public:
    struct Record
    {
        Winters::Replay::eReplayRecordType type =
            Winters::Replay::eReplayRecordType::Snapshot;
        u64_t serverTick = 0;
        u32_t sequence = 0;
        std::vector<u8_t> payload{};
    };

    bool_t LoadFromFile(const wstring_t& path, std::string& outError);

    bool_t IsLoaded() const { return m_bLoaded; }
    const Winters::Replay::ReplayFileHeader& Header() const { return m_header; }
    const std::vector<Record>& Records() const { return m_records; }
    u64_t FirstTick() const { return m_header.firstTick; }
    u64_t LastTick() const { return m_header.lastTick; }

private:
    bool_t m_bLoaded = false;
    Winters::Replay::ReplayFileHeader m_header{};
    std::vector<Record> m_records{};
};
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Replay/ReplayReader.cpp

새 파일 (ReplayRecorder::SaveToFile의 직렬화 순서 ReplayFileHeader -> [ReplayRecordHeader + payload]* 를 그대로 역으로 읽는다):

```cpp
#include "Network/Replay/ReplayReader.h"

#include <fstream>

bool_t CReplayReader::LoadFromFile(const wstring_t& path, std::string& outError)
{
    outError.clear();
    m_bLoaded = false;
    m_records.clear();
    m_header = Winters::Replay::ReplayFileHeader{};

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        outError = "failed to open replay file";
        return false;
    }

    Winters::Replay::ReplayFileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || !Winters::Replay::IsReplayMagic(header))
    {
        outError = "invalid replay header";
        return false;
    }
    if (header.version != Winters::Replay::kReplayVersion)
    {
        outError = "unsupported replay version";
        return false;
    }

    std::vector<Record> records;
    records.reserve(header.recordCount);

    for (u32_t i = 0; i < header.recordCount; ++i)
    {
        Winters::Replay::ReplayRecordHeader recordHeader{};
        in.read(reinterpret_cast<char*>(&recordHeader), sizeof(recordHeader));
        if (!in)
        {
            outError = "truncated replay record header";
            return false;
        }

        Record record{};
        record.type = static_cast<Winters::Replay::eReplayRecordType>(recordHeader.type);
        record.serverTick = recordHeader.serverTick;
        record.sequence = recordHeader.sequence;
        record.payload.resize(recordHeader.payloadSize);
        if (recordHeader.payloadSize > 0)
        {
            in.read(reinterpret_cast<char*>(record.payload.data()),
                static_cast<std::streamsize>(recordHeader.payloadSize));
            if (!in)
            {
                outError = "truncated replay record payload";
                return false;
            }
        }

        records.emplace_back(std::move(record));
    }

    m_header = header;
    m_records = std::move(records);
    m_bLoaded = true;
    return true;
}
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Public/Defines.h

기존 코드:

```cpp
enum class eSceneID : int
{
	GameSelect,
	Login,
	MainMenu,
	CustomMode,
	BanPick,
	Shop,
	MatchLoading,
	InGame,
	Editor,
	Result,
	SceneLoading,
	End
};
```

아래로 교체:

```cpp
enum class eSceneID : int
{
	GameSelect,
	Login,
	MainMenu,
	CustomMode,
	BanPick,
	Shop,
	MatchLoading,
	InGame,
	Replay,
	Editor,
	Result,
	SceneLoading,
	End
};
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Replay/ReplayPlayer.h

새 파일. world+entityMap+applier를 소유하고 WRPL record를 tick 순서대로 applier에 먹이는 코어다. 시각 엔티티 생성은 applier가 콜백으로 scene에 위임하므로(아래 확인 필요), scene이 콜백을 주입할 수 있게 applier 포인터를 노출한다.

```cpp
#pragma once

#include "Network/Client/EventApplier.h"
#include "Network/Client/SnapshotApplier.h"
#include "Network/Replay/ReplayReader.h"

#include "ECS/World.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "WintersTypes.h"

#include <memory>
#include <string>

class CReplayPlayer final
{
public:
    bool_t LoadFromFile(const wstring_t& path, std::string& outError);

    CSnapshotApplier* GetSnapshotApplier() { return m_pSnapshotApplier.get(); }
    CEventApplier* GetEventApplier() { return m_pEventApplier.get(); }
    CWorld& GetWorld() { return m_world; }
    EntityIdMap& GetEntityMap() { return m_entityMap; }

    bool_t IsLoaded() const { return m_reader.IsLoaded(); }
    u64_t FirstTick() const { return m_reader.FirstTick(); }
    u64_t LastTick() const { return m_reader.LastTick(); }
    u64_t CurrentTick() const { return m_currentTick; }

    // 전방 재생: targetTick 이하의 모든 record를 순서대로 applier에 먹인다.
    void StepToTick(u64_t targetTick);

private:
    CReplayReader m_reader;
    CWorld m_world;
    EntityIdMap m_entityMap;
    std::unique_ptr<CSnapshotApplier> m_pSnapshotApplier = CSnapshotApplier::Create();
    std::unique_ptr<CEventApplier> m_pEventApplier = CEventApplier::Create();
    std::size_t m_nextRecordIndex = 0;
    u64_t m_currentTick = 0;
};
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Replay/ReplayPlayer.cpp

새 파일. `LoadFromFile`과 전방 `StepToTick`은 검증된 시그니처(OnSnapshot/OnEvent가 raw payload를 받음)로 확정할 수 있다.

```cpp
#include "Network/Replay/ReplayPlayer.h"

bool_t CReplayPlayer::LoadFromFile(const wstring_t& path, std::string& outError)
{
    m_nextRecordIndex = 0;
    m_currentTick = 0;
    return m_reader.LoadFromFile(path, outError);
}

void CReplayPlayer::StepToTick(u64_t targetTick)
{
    if (!m_reader.IsLoaded())
        return;

    const std::vector<CReplayReader::Record>& records = m_reader.Records();

    while (m_nextRecordIndex < records.size())
    {
        const CReplayReader::Record& record = records[m_nextRecordIndex];
        if (record.serverTick > targetTick)
            break;

        if (record.type == Winters::Replay::eReplayRecordType::Snapshot)
        {
            m_pSnapshotApplier->OnSnapshot(
                m_world, m_entityMap,
                record.payload.data(),
                static_cast<u32_t>(record.payload.size()));
        }
        else if (record.type == Winters::Replay::eReplayRecordType::Event)
        {
            m_pEventApplier->OnEvent(
                m_world, m_entityMap,
                record.payload.data(),
                static_cast<u32_t>(record.payload.size()));
        }

        ++m_nextRecordIndex;
        m_currentTick = record.serverTick;
    }
}
```

확인 필요 (이 파일 완성 전 선행 조사):

```text
- 시각 엔티티 콜백 배선: CSnapshotApplier는 새 netId에 대해 m_onNewEntity(OnNewEntityFn)로 챔피언/미니언 비주얼 생성을 scene에 위임하고, CEventApplier는 SetFxMeshRenderer로 FX 렌더러를 받는다. live 배선이 어디서 이뤄지는지 먼저 읽어야 한다:
    C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp
    C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameNetworkBridge.cpp
  여기서 SetOnNewEntityCallback / SetOnRemoveEntityCallback / SetOnChampionVisualChangedCallback / SetFxMeshRenderer가 어떻게 설정되는지 확인한 뒤, Scene_Replay가 동일 콜백을 CReplayPlayer의 applier에 주입한다. (콜백 없이 OnSnapshot만 먹이면 EntityIdMap만 갱신되고 화면에 아무것도 안 보일 수 있다.)
- 뒤로 감기(seek back)/재시작: 현재 StepToTick은 전방 전용이다. 되감기는 world 전체 클리어 + applier/EntityIdMap 재생성이 필요하다. CWorld가 안전하게 클리어/재구성 가능한지(대입 가능 여부, 전체 DestroyEntity 경로) 확인 전에는 Reset을 고정하지 않는다.
```

1-9. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_Replay.h, Scene_Replay.cpp

확인 필요:

```text
- Scene_Replay는 Scene_InGame의 월드 렌더 경로를 미러링해야 한다. Scene_InGame이 CWorld를 어떤 렌더 시스템/카메라/뷰로 그리는지(InGameRenderBridge.cpp), scene 생성/등록이 어디서 이뤄지는지 먼저 읽은 뒤 작성한다.
- CReplayPlayer를 소유하고, 매 프레임 재생 tick을 진행시켜 StepToTick(currentPlaybackTick) 호출 -> GetWorld() 렌더. 재생/일시정지/배속 컨트롤은 ImGui로 노출.
- 진입점: Scene_CustomMode(C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_CustomMode.cpp)의 match loading 진입 부근에 Replay 목록/버튼을 붙여 eSceneID::Replay로 전환. (Replay 디렉터리의 .wrpl 목록 열거 -> 선택 -> Scene_Replay 로드.)
```

1-10. 클라 InGame "Stop" 버튼 -> 서버 finalize 명령

확인 필요:

```text
- 현재 CommandKind(Command_generated.h: Move/CastSkill/BasicAttack/LevelSkill/BuyItem/UseItem/Recall/RecallCancel/AIDebugControl)와 LobbyCommandKind(LobbyTypes_generated.h: JoinSlot..SetBotLane)에 Stop/EndGame/FinalizeReplay가 없다. 둘 다 flatbuffers 생성물이라 임의 추가 금지.
- 추가하려면 원본 .fbs 스키마(Command 또는 LobbyCommand)에 host/debug 전용 StopMatch(or FinalizeReplay) 종류를 넣고 flatc로 재생성한 뒤, 서버 dispatch(OnCommandBatch / OnLobbyCommand)에서 host 검증 후 room->Stop() 1회만 호출하도록 연결한다.
- 클라가 직접 WRPL 파일을 닫는 구조는 server-authority와 어긋나므로 만들지 않는다. 녹화 종료/저장은 항상 서버 Stop 경로에서만 일어난다.
- 당장은 서버 콘솔 q(이미 room->Stop()까지 연결됨)로 finalize가 동작하므로, 클라 Stop 버튼은 후속 단계로 분리한다.
```

2. 검증

미검증:
- 빌드 미검증.
- 런타임에서 InGame 시작 후 WRPL이 실제로 생성되는지 미검증.
- WRPL을 CReplayReader로 다시 읽어 record 수가 서버 로그의 records/snapshots/events와 일치하는지 미검증.

검증 명령:
- git diff --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 (또는 기존 확인된 빌드 절차)

확인 필요:
- 1-1/1-2/1-3에서 sprintf_s, OutputDebugStringA/W가 GameRoomReplication.cpp에서 PCH로 이미 가용한지(GameRoom.cpp는 동일 패턴을 별도 include 없이 사용). 아니면 <cstdio>, <Windows.h> 추가.
- 1-4/1-5/1-7/1-8 신규 파일이 Client 빌드 프로젝트(.vcxproj)에 ClInclude/ClCompile로 포함되는지.
- 1-2의 spectator snapshot은 접속자 0명이어도 매 InGame tick마다 1개씩 생성된다(전체 매치 캡처 목적). tick rate 30 기준 기록량/메모리 증가가 허용 범위인지.
- 1-6에서 eSceneID에 Replay를 중간 삽입하면 Editor/Result/SceneLoading/End의 정수값이 1씩 밀린다. eSceneID를 정수로 직렬화하거나 하드코딩 인덱스로 참조하는 곳이 없는지 확인(없으면 무해).

설계 DoD:
- 녹화 권위: WRPL에는 transport(TCP/UDP) 패킷이 아니라 GameSim authoritative Snapshot/Event payload만 들어간다. 저장은 서버 Stop 경로에서만 일어난다.
- 재생 권위: 재생은 GameSim 재시뮬레이션이 아니라, 저장된 payload를 기존 CSnapshotApplier::OnSnapshot / CEventApplier::OnEvent에 순서대로 먹이는 방식이다.
- UDP 무관: replay record hook은 transport 아래가 아니라 GameRoom replication 위쪽(Phase_BroadcastEvents/Phase_BroadcastSnapshot)에 있으므로 TCP/UDP 어느 쪽이든 동일 payload를 저장한다.

후속 동기화:
- 1-1~1-3은 Server private cpp만 수정하므로 EngineSDK 동기화 불필요.
