# Client Local Replay Master Plan

작성일: 2026-05-09

---

## 0. 결론

이번 Replay MVP는 **클라이언트가 이미 수신한 authoritative Snapshot/Event를 로컬 파일로 저장하고 다시 재생하는 기능**이다.

중요한 점:

- GameSim을 다시 돌리지 않는다.
- 입력 command를 재시뮬레이션하지 않는다.
- 서버 결정 결과인 Snapshot/Event를 그대로 재생한다.
- 그래서 서버 권위 안정화 중에도 Replay가 디버깅 도구로 바로 쓸모 있다.

---

## 1. User Flow

### 1.1 InGame 저장

```text
Scene_InGame 진입
-> ReplayRecorder 생성
-> InGameNetworkBridge 가 Snapshot/Event 수신 때마다 recorder 에 raw payload 기록
-> 사용자가 InGame ImGui 의 "Save Replay" 버튼 클릭
-> recorder 가 지금까지 누적한 records 를 .wrpl 로 저장
-> 상태 문구에 저장 경로/record 수 표시
```

저장 위치:

```text
Client/Bin/Replay/
  2026-05-09_22-45-12_room-local_snapshot-1842_event-91.wrpl
```

### 1.2 Client 종료 후 재생

```text
Client 재실행
-> Scene_MainMenu
-> Replay 버튼 클릭
-> Client/Bin/Replay/*.wrpl 목록 표시
-> 파일 선택 후 Play
-> Scene_Replay 로 전환
-> .wrpl header/records parse
-> Snapshot/Event payload 를 기존 applier 로 재생
```

---

## 2. What Gets Saved

### Snapshot record

저장 원본:

```text
Shared::Schema::Snapshot raw FlatBuffer payload
```

담기는 것:

- 챔피언 위치/yaw/HP/마나/레벨/스킬 쿨다운.
- 미니언 위치/HP/상태.
- 타워/억제기/넥서스/정글몹 HP와 위치.
- 투사체 entity snapshot.
- `animID`, `actionSeq`, `animStartTick`, `playbackRate`.

### Event record

저장 원본:

```text
Shared::Schema::EventPacket raw FlatBuffer payload
```

담기는 것:

- `AnimationStart`
- `EffectTrigger`
- `Damage`
- `ProjectileSpawn`
- `ProjectileHit`
- `SkillCast`

킬 로그:

- 현재 즉시 사용 가능한 데이터는 `DamageEvent.bKilled`.
- 장기적으로 서버가 `Death` 또는 `KillFeed` event를 발행하면 Replay UI가 더 정확해진다.

---

## 3. WRPL Container

R0에서는 컨테이너만 정의한다.
payload 내부는 기존 FlatBuffers가 담당한다.

### 3.1 Header

```cpp
struct ReplayFileHeader
{
    char   magic[4];          // "WRPL"
    u16_t  version;           // 1
    u16_t  headerSize;
    u32_t  flags;
    u32_t  recordCount;
    u32_t  snapshotCount;
    u32_t  eventCount;
    u64_t  firstTick;
    u64_t  lastTick;
    u64_t  createdUnixMs;
};
```

### 3.2 Record

```cpp
enum class eReplayRecordType : u8_t
{
    Snapshot = 1,
    Event = 2,
};

struct ReplayRecordHeader
{
    u8_t  type;
    u8_t  reserved0;
    u16_t reserved1;
    u32_t payloadSize;
    u64_t serverTick;
    u32_t sequence;
};
```

### 3.3 Record Ordering

기록 순서는 수신 순서를 유지한다.

```text
Snapshot tick 100
Event tick 101 AnimationStart
Event tick 101 EffectTrigger
Snapshot tick 101
Snapshot tick 102
...
```

재생 시에는 record order 그대로 처리한다.
같은 tick 내 event/snapshot 정렬을 새로 하지 않는다.

---

## 4. Architecture

```text
Network frame callback
  -> ReplayRecorder::RecordSnapshot / RecordEvent
  -> SnapshotApplier / EventApplier

InGame ImGui
  -> ReplayRecorder::SaveToFile(...)

MainMenu Replay panel
  -> ReplayLibrary::ListLocalReplays()
  -> CScene_Replay::Create(path)

Scene_Replay
  -> ReplayPlayer::Load(path)
  -> ReplayPlayer::Update(dt)
  -> SnapshotApplier / EventApplier
```

---

## 5. New Runtime Classes

### CReplayRecorder

위치:

```text
Client/Public/Replay/ReplayRecorder.h
Client/Private/Replay/ReplayRecorder.cpp
```

역할:

- Snapshot/Event payload 누적.
- FlatBuffer verifier로 payload 유효성 확인.
- serverTick 추출.
- Save 버튼 클릭 시 `.wrpl` 파일 저장.

권장 인터페이스:

```cpp
class CReplayRecorder final
{
public:
    static unique_ptr<CReplayRecorder> Create();

    void Reset();
    void RecordSnapshot(u32_t sequence, const u8_t* payload, u32_t len);
    void RecordEvent(u32_t sequence, const u8_t* payload, u32_t len);

    bool_t SaveToFile(const wstring_t& path, string& outError);

    u32_t GetRecordCount() const;
    u32_t GetSnapshotCount() const;
    u32_t GetEventCount() const;
    u64_t GetFirstTick() const;
    u64_t GetLastTick() const;
};
```

### CReplayLibrary

위치:

```text
Client/Public/Replay/ReplayLibrary.h
Client/Private/Replay/ReplayLibrary.cpp
```

역할:

- `Client/Bin/Replay` 디렉토리 생성.
- 파일명 생성.
- replay 목록 조회.
- display metadata 제공.

권장 인터페이스:

```cpp
struct ReplayListItem
{
    wstring_t path;
    string displayName;
    u64_t fileSizeBytes = 0;
    u64_t modifiedUnixMs = 0;
};

class CReplayLibrary final
{
public:
    static wstring_t GetReplayDirectory();
    static wstring_t MakeReplayPath(u64_t firstTick, u32_t snapshotCount, u32_t eventCount);
    static vector<ReplayListItem> ListLocalReplays();
};
```

### CReplayPlayer

위치:

```text
Client/Public/Replay/ReplayPlayer.h
Client/Private/Replay/ReplayPlayer.cpp
```

역할:

- `.wrpl` 파일 parse.
- record index 구성.
- 재생/정지/seek/속도 변경.
- record payload를 `CSnapshotApplier` / `CEventApplier` 로 전달.

권장 인터페이스:

```cpp
class CReplayPlayer final
{
public:
    static unique_ptr<CReplayPlayer> Create();

    bool_t LoadFromFile(const wstring_t& path, string& outError);
    void Play();
    void Pause();
    void SetPlaySpeed(f32_t speed);
    void SeekToRecord(u32_t recordIndex);

    void Update(f32_t dt, CWorld& world, EntityIdMap& entityMap,
        CSnapshotApplier& snapshotApplier, CEventApplier& eventApplier);

    u32_t GetRecordCount() const;
    u32_t GetCurrentRecordIndex() const;
};
```

### CScene_Replay

위치:

```text
Client/Public/Scene/Scene_Replay.h
Client/Private/Scene/Scene_Replay.cpp
```

역할:

- replay 전용 world와 camera를 구성한다.
- player/applier/entity map을 소유한다.
- ImGui transport UI를 제공한다.

초기 UI:

- Play/Pause
- Speed x0.5/x1/x2/x4
- Record slider
- Back to MainMenu

---

## 6. MVP Boundaries

R0에서 하는 것:

- 네트워크 authoritative InGame에서 수신한 Snapshot/Event 저장.
- 파일 저장 후 재실행해도 MainMenu에서 목록 조회.
- Snapshot 기반 위치/HP/미니언/타워/챔피언 재생.
- Event 기반 애니메이션/Fx/데미지/투사체 재생.

R0에서 하지 않는 것:

- 서버 전체 경기 자동 녹화.
- local-only mode의 자체 snapshot 생성.
- rewind 후 재시뮬레이션.
- backend upload.
- user별 replay library.
- 정확한 LoL식 타임라인 UI.

---

## 7. Important Risks

### RISK-1. Save 버튼 클릭 전 기록이 없을 수 있음

원인:

- local-only mode에서는 `Snapshot/Event`가 오지 않는다.

대응:

- R0는 network authoritative mode 전용으로 둔다.
- recorder record count가 0이면 Save 버튼 비활성 또는 경고 표시.

### RISK-2. Event/Snapshot animation 중복

현재 `SnapshotApplier`와 `EventApplier`가 모두 animation 재생 가능하다.
Replay에서도 같은 문제가 그대로 보일 수 있다.

대응:

- R0는 현재 동작을 있는 그대로 재현한다.
- `ServerAICompletion.md`의 S5에서 actionSeq dedupe를 별도 해결한다.

### RISK-3. 긴 경기 메모리 증가

메모리에 모든 payload를 누적하면 긴 매치에서 커진다.

대응:

- R0는 개발용 MVP로 허용.
- R1에서 streaming writer로 전환하거나, R0-2에서 recorder가 temp 파일에 append하도록 변경한다.

### RISK-4. Replay scene resource preload

`CSnapshotApplier::EnsureEntity`가 새 entity를 만들 때 champion renderer/Fx resource가 필요하다.

대응:

- `Scene_Replay` 초기 구현은 InGame의 createChampion callback과 최대한 같은 경로를 재사용한다.
- 재생 전 필수 champion/Fx resource preload를 단계적으로 추가한다.

---

## 8. Recommended Implementation Order

1. `ReplayFormat.h`
2. `CReplayRecorder`
3. `InGameNetworkBridge`에 recorder hook
4. `Scene_InGame::OnImGui()` 저장 버튼
5. `CReplayLibrary` 목록/파일명
6. `CReplayPlayer` parser
7. `Scene_Replay`
8. `Scene_MainMenu` Replay 패널
9. vcxproj/filters 등록
10. smoke 검증
