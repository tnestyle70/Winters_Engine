# Replay Code Handoff Positions

작성일: 2026-05-09

이 문서는 사용자가 직접 코드를 반영할 때 볼 삽입 위치 목록이다.
Codex는 구현 세션에서 아래 순서대로 코드 블록을 출력한다.

---

## 0. Naming / File Rules

- 클래스명은 `C` 접두사: `CReplayRecorder`, `CReplayPlayer`, `CReplayLibrary`, `CScene_Replay`.
- 새 파일명은 가능하면 `C` 접두사 없이 작성:
  - `ReplayRecorder.h`
  - `ReplayPlayer.h`
  - `ReplayLibrary.h`
  - `Scene_Replay.h` 는 기존 Scene 파일명 관례에 맞춰 유지.
- 신규 코드 타입은 `u32_t`, `u64_t`, `f32_t`, `bool_t`, `wstring_t` 계열 사용.
- 신규 문서/코드 명명은 `ID` 표기를 우선한다. 예: `netID`, `recordID`.
- FlatBuffers accessor는 기존 generated 이름을 그대로 사용한다. 예: `netId()`, `yourNetId()`.

---

## 1. Shared Replay Format

### 1.1 Create

```text
Shared/Replay/ReplayFormat.h
```

넣을 내용:

- `kReplayMagic`
- `kReplayVersion`
- `ReplayFileHeader`
- `eReplayRecordType`
- `ReplayRecordHeader`
- `static_assert` for packed sizes

주의:

- 이 파일은 Client와 향후 Server가 같이 쓰는 binary contract다.
- include는 `WintersTypes.h`만으로 충분하게 유지한다.
- FlatBuffers generated header를 여기서 include하지 않는다.

### 1.2 Project include

현재 Client include path는 repo root와 `Shared`를 볼 수 있다.
별도 Shared project 등록은 필요 없지만, `Client.vcxproj.filters`에는 표시용 등록이 필요할 수 있다.

---

## 2. Replay Recorder

### 2.1 Create Header

```text
Client/Public/Replay/ReplayRecorder.h
```

주요 선언:

- `class CReplayRecorder final`
- `RecordSnapshot`
- `RecordEvent`
- `SaveToFile`
- count getter

include 후보:

```cpp
#include "Defines.h"
```

전방 선언:

```cpp
namespace Shared::Schema { struct Snapshot; struct EventPacket; }
```

### 2.2 Create CPP

```text
Client/Private/Replay/ReplayRecorder.cpp
```

include 후보:

```cpp
#include "Replay/ReplayRecorder.h"
#include "Shared/Replay/ReplayFormat.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
```

함수별 역할:

- `RecordSnapshot`
  - null/len guard
  - `VerifySnapshotBuffer`
  - `GetSnapshot(payload)->serverTick()` 추출
  - payload copy
- `RecordEvent`
  - null/len guard
  - `VerifyEventPacketBuffer`
  - `GetEventPacket(payload)->serverTick()` 추출
  - payload copy
- `SaveToFile`
  - directory 생성은 `CReplayLibrary`로 위임하거나 내부에서 `std::filesystem::create_directories`
  - header 작성
  - record header + payload 순서대로 fwrite/ofstream write

---

## 3. Replay Library

### 3.1 Create Header

```text
Client/Public/Replay/ReplayLibrary.h
```

주요 선언:

- `ReplayListItem`
- `CReplayLibrary::GetReplayDirectory`
- `CReplayLibrary::MakeReplayPath`
- `CReplayLibrary::ListLocalReplays`

### 3.2 Create CPP

```text
Client/Private/Replay/ReplayLibrary.cpp
```

권장 저장 위치:

```text
Client/Bin/Replay
```

구현 메모:

- 실행 working directory가 SolutionDir일 수 있으므로 `"Client/Bin/Replay"` 기준을 우선 사용한다.
- C++ 문자열 경로는 `L"Client/Bin/Replay"` 또는 forward slash를 사용한다.
- `std::filesystem` 사용.

---

## 4. InGame Recorder Hook

### 4.1 Modify Header

```text
Client/Public/Scene/InGameNetworkBridge.h
```

삽입 위치:

1. forward declarations 근처:

```cpp
class CReplayRecorder;
```

2. `struct InGameNetworkBridgeDesc` 마지막 근처:

```cpp
CReplayRecorder* pReplayRecorder = nullptr;
```

### 4.2 Modify CPP

```text
Client/Private/Scene/InGameNetworkBridge.cpp
```

include 추가:

```cpp
#include "Replay/ReplayRecorder.h"
```

수정 위치:

```cpp
void CInGameNetworkBridge::Initialize(InGameNetworkBridgeDesc& desc)
```

로컬 포인터 준비:

```cpp
CReplayRecorder* pReplayRecorder = desc.pReplayRecorder;
```

frameHandler capture 목록에 `pReplayRecorder` 추가.

Snapshot 분기:

```cpp
else if (type == ePacketType::Snapshot)
{
    if (pReplayRecorder)
        pReplayRecorder->RecordSnapshot(sequence, payload, len);

    ...
    snapshotApplier.OnSnapshot(*pWorld, entityMap, payload, len);
}
```

Event 분기:

```cpp
else if (type == ePacketType::Event)
{
    if (pReplayRecorder)
        pReplayRecorder->RecordEvent(sequence, payload, len);

    eventApplier.OnEvent(*pWorld, entityMap, payload, len);
}
```

주의:

- `sequence`를 `(void)sequence;`로 버리던 코드는 recorder 사용 후 제거한다.
- recorder 기록은 applier 호출 전이 낫다. applier에서 실패해도 raw 수신 기록은 남는다.

---

## 5. Scene_InGame Save UI

### 5.1 Modify Header

```text
Client/Public/Scene/Scene_InGame.h
```

forward declarations 근처:

```cpp
class CReplayRecorder;
```

private network member 근처:

```cpp
unique_ptr<CReplayRecorder> m_pReplayRecorder;
std::string m_strReplayStatus;
```

private methods 근처:

```cpp
void DrawReplayCapturePanel();
void SaveReplayCapture();
```

### 5.2 Modify CPP

```text
Client/Private/Scene/Scene_InGame.cpp
```

include 추가:

```cpp
#include "Replay/ReplayRecorder.h"
#include "Replay/ReplayLibrary.h"
```

수정 위치:

1. `CScene_InGame::OnEnter()`
   - `m_pReplayRecorder = CReplayRecorder::Create();`
   - status 초기화.

2. `CScene_InGame::OnExit()`
   - `m_pReplayRecorder.reset();`
   - 자동 저장은 R0에서는 하지 않는다. 버튼 클릭 저장만 수행.

3. `InGameNetworkBridgeDesc` 생성 위치
   - desc에 `m_pReplayRecorder.get()` 전달.

4. `CScene_InGame::OnImGui()`
   - AIDebug 처리 후 또는 legacy debug return 전에 `DrawReplayCapturePanel();` 호출.
   - Replay 저장 버튼은 항상 보이게 할지, F10 legacy debug에서만 보이게 할지 결정 필요.
   - 권장: 개발 중에는 항상 작은 창으로 표시.

5. 새 함수 `DrawReplayCapturePanel()`
   - record/snapshot/event count 표시.
   - `Save Replay` 버튼.
   - 마지막 저장 경로/status 표시.

6. 새 함수 `SaveReplayCapture()`
   - `CReplayLibrary::MakeReplayPath(...)`
   - `m_pReplayRecorder->SaveToFile(path, error)`
   - status 문자열 갱신.

---

## 6. Replay Player

### 6.1 Create Header

```text
Client/Public/Replay/ReplayPlayer.h
```

주요 선언:

- `CReplayPlayer`
- `LoadFromFile`
- `Play/Pause`
- `SetPlaySpeed`
- `SeekToRecord`
- `Update`

### 6.2 Create CPP

```text
Client/Private/Replay/ReplayPlayer.cpp
```

역할:

- WRPL header 검증.
- record headers/payload offsets 저장.
- elapsed time을 tick 기준으로 record 진행.
- record type이 Snapshot이면 `CSnapshotApplier::OnSnapshot`.
- record type이 Event면 `CEventApplier::OnEvent`.

MVP timing:

- 단순히 `serverTick` 차이를 `1 / 30.f` 기준으로 환산한다.
- 같은 tick record는 한 update에서 모두 처리한다.

---

## 7. Scene_Replay

### 7.1 Create Header

```text
Client/Public/Scene/Scene_Replay.h
```

생성 함수:

```cpp
static unique_ptr<CScene_Replay> Create(const wstring_t& replayPath);
```

멤버 후보:

- `wstring_t m_replayPath`
- `CWorld m_world`
- `unique_ptr<EntityIdMap> m_pEntityIdMap`
- `unique_ptr<CSnapshotApplier> m_pSnapshotApplier`
- `unique_ptr<CEventApplier> m_pEventApplier`
- `unique_ptr<CReplayPlayer> m_pReplayPlayer`
- camera/render resource members

### 7.2 Create CPP

```text
Client/Private/Scene/Scene_Replay.cpp
```

함수별 역할:

- `OnEnter`
  - replay load
  - applier 생성
  - entity map 생성
  - snapshot applier `SetOnNewEntityCallback` 설정
  - 기본 camera/render 준비
- `OnUpdate`
  - `m_pReplayPlayer->Update(...)`
- `OnRender`
  - replay world render
- `OnImGui`
  - play/pause/speed/slider/back 버튼

주의:

- `Back to MainMenu` 클릭 후 `Change_Scene` 호출하면 즉시 `return`.
- InGame scene 객체를 재사용하지 않는다. Replay는 별도 scene이다.

---

## 8. MainMenu Replay Entry

### 8.1 Modify Scene Enum

```text
Client/Public/Defines.h
```

`eSceneID`에 추가:

```cpp
Replay,
```

권장 위치:

```text
InGame,
Replay,
Editor,
```

### 8.2 Modify Header

```text
Client/Public/Scene/Scene_MainMenu.h
```

`ePanel`에 추가:

```cpp
Replay,
```

메서드 추가:

```cpp
void DrawReplayPanel();
void RequestReplay(const wstring_t& path);
```

멤버 후보:

```cpp
wstring_t m_selectedReplayPath;
```

### 8.3 Modify CPP

```text
Client/Private/Scene/Scene_MainMenu.cpp
```

include 추가:

```cpp
#include "Replay/ReplayLibrary.h"
#include "Scene/Scene_Replay.h"
```

수정 위치:

1. `CScene_MainMenu::OnImGui()`
   - switch에 `ePanel::Replay` 추가.

2. `CScene_MainMenu::DrawNavigation()`
   - `Replay` 버튼 추가.

3. 새 함수 `DrawReplayPanel()`
   - `CReplayLibrary::ListLocalReplays()`
   - 파일 목록 표시.
   - Play 버튼 클릭 시 `RequestReplay(path)`.

4. 새 함수 `RequestReplay(...)`
   - `CGameInstance::Get()->Change_Scene(static_cast<uint32_t>(eSceneID::Replay), CScene_Replay::Create(path));`
   - 호출 직후 `return` 원칙 준수.

---

## 9. Project Files

### 9.1 Client.vcxproj

```text
Client/Include/Client.vcxproj
Client/Include/Client.vcxproj.filters
```

추가할 ClInclude:

```text
..\Public\Replay\ReplayRecorder.h
..\Public\Replay\ReplayLibrary.h
..\Public\Replay\ReplayPlayer.h
..\Public\Scene\Scene_Replay.h
..\..\Shared\Replay\ReplayFormat.h
```

추가할 ClCompile:

```text
..\Private\Replay\ReplayRecorder.cpp
..\Private\Replay\ReplayLibrary.cpp
..\Private\Replay\ReplayPlayer.cpp
..\Private\Scene\Scene_Replay.cpp
```

filters 권장:

```text
01. Scene
04. Network or 07. Replay
```

새 filter를 만들면 `07. Replay` 추천.

---

## 10. Implementation Session Output Order

Codex가 다음 구현 세션에서 출력할 코드 순서:

1. `Shared/Replay/ReplayFormat.h`
2. `Client/Public/Replay/ReplayRecorder.h`
3. `Client/Private/Replay/ReplayRecorder.cpp`
4. `Client/Public/Replay/ReplayLibrary.h`
5. `Client/Private/Replay/ReplayLibrary.cpp`
6. `InGameNetworkBridge.h/.cpp` 수정 블록
7. `Scene_InGame.h/.cpp` 수정 블록
8. `Client/Public/Replay/ReplayPlayer.h`
9. `Client/Private/Replay/ReplayPlayer.cpp`
10. `Scene_Replay.h/.cpp`
11. `Defines.h`, `Scene_MainMenu.h/.cpp` 수정 블록
12. `Client.vcxproj/.filters` 등록 위치
13. 검증 절차
