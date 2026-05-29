# Replay Verification Checklist

작성일: 2026-05-09

---

## 1. Build Gates

```powershell
where.exe MSBuild
MSBuild Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

필수 확인:

- `Shared/Replay/ReplayFormat.h` include 경로 문제 없음.
- `Client/Public/Replay/*.h` 가 `Defines.h` 기준으로 컴파일됨.
- `Client/Private/Replay/*.cpp` 가 FlatBuffers generated headers를 정상 include함.
- `Client.vcxproj.filters` 누락으로 IDE 표시만 깨지는 일 없음.

---

## 2. InGame Capture Smoke

전제:

- 서버 실행.
- 클라이언트가 BanPick/MatchLoading/InGame network authoritative 흐름으로 진입.

절차:

1. InGame 진입.
2. 5초 이상 움직임/스킬/미니언/타워 event가 있는 상태를 만든다.
3. InGame Replay Capture 창 확인.
4. `Record`, `Snapshot`, `Event` count가 증가하는지 확인.
5. `Save Replay` 버튼 클릭.
6. 상태 문구에 `.wrpl` 경로가 출력되는지 확인.
7. 파일 존재 확인.

PowerShell 확인:

```powershell
Get-ChildItem .\Client\Bin\Replay\*.wrpl | Sort-Object LastWriteTime -Descending | Select-Object -First 5
```

통과 기준:

- `.wrpl` 파일 생성.
- 파일 크기 0 아님.
- Snapshot count 1 이상.
- Event count는 전투/스킬을 했다면 1 이상.

---

## 3. Client Restart Persistence

절차:

1. 클라이언트 종료.
2. 다시 실행.
3. MainMenu 진입.
4. Replay 버튼 클릭.
5. 방금 저장한 파일이 목록에 표시되는지 확인.

통과 기준:

- `Client/Bin/Replay` 파일이 삭제되지 않음.
- MainMenu Replay panel에서 최근 파일이 보임.

---

## 4. Playback Smoke

절차:

1. MainMenu Replay panel에서 파일 선택.
2. Play 클릭.
3. `Scene_Replay` 진입 확인.
4. Play/Pause 버튼 동작 확인.
5. record slider 이동 확인.
6. Back to MainMenu 확인.

통과 기준:

- crash 없이 scene 전환.
- snapshot을 적용해 entity가 생성됨.
- 챔피언/미니언/타워 위치가 시간에 따라 변함.
- event 기반 Fx/animation/damage가 가능한 범위에서 보임.

---

## 5. Data Validation

간단한 WRPL dump helper를 만들면 확인할 것:

```text
magic = WRPL
version = 1
recordCount = snapshotCount + eventCount
firstTick <= lastTick
payloadSize > 0 for every record
all Snapshot payloads pass VerifySnapshotBuffer
all Event payloads pass VerifyEventPacketBuffer
```

---

## 6. Known Failure Checks

### F1. Save 버튼이 항상 disabled

확인:

- network authoritative InGame인지 확인.
- `InGameNetworkBridge` frame handler가 snapshot/event를 받고 있는지 확인.
- `CReplayRecorder* pReplayRecorder`가 desc/capture에 전달됐는지 확인.

### F2. 파일은 생기는데 replay 목록에 안 보임

확인:

- 저장 경로와 목록 조회 경로가 둘 다 `Client/Bin/Replay`인지 확인.
- Windows 경로 문자열에 `\U`, `\u` 이스케이프를 쓰지 않았는지 확인.
- 확장자 필터가 `.wrpl`인지 확인.

### F3. replay scene에서 entity가 안 나옴

확인:

- `CSnapshotApplier::SetOnNewEntityCallback`이 Scene_Replay에서도 설정됐는지 확인.
- InGame의 champion spawn helper를 그대로 못 쓰는 경우 최소 marker entity fallback이 있는지 확인.
- 첫 record가 Event이고 Snapshot이 늦게 오는 경우, Snapshot까지 진행했는지 확인.

### F4. 애니메이션/Fx가 두 번 보임

확인:

- 현재 본문제일 가능성이 높은 원인은 Snapshot/Event 양쪽 animation 재생이다.
- Replay만의 문제가 아니라 `ServerAICompletion.md` S5 대상이다.
- R0에서는 "현재 네트워크 플레이에서 보인 결과를 재현"하는 것까지를 통과 기준으로 둔다.

### F5. 긴 경기에서 메모리 과다

확인:

- recorder 누적 payload 총량을 UI에 표시한다.
- R0 제한값을 둔다. 예: 512MB 또는 30분.
- 장기 해결은 temp file streaming writer.

---

## 7. Done Criteria

R0 완료 판정:

```text
[ ] InGame Replay Capture count가 증가한다.
[ ] Save Replay 버튼으로 .wrpl 파일이 생성된다.
[ ] Client 종료 후 MainMenu Replay panel에서 파일이 보인다.
[ ] Scene_Replay 로 전환된다.
[ ] Snapshot 기반 entity/HP/위치가 재생된다.
[ ] Event 기반 animation/Fx/damage가 가능한 범위에서 재생된다.
[ ] Back to MainMenu 가 crash 없이 동작한다.
```

R0 이후 바로 이어갈 것:

```text
R0-2: WRPL dump/validator tool
R1: Server-side spectator replay recorder
R2: Go Replay Service ingest/download
R3: userID 기반 personal replay library
```
