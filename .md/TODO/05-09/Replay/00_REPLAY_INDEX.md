# Replay Plan Index

작성일: 2026-05-09

목표:

```text
InGame 중 ImGui 버튼 클릭
-> 지금까지 받은 Snapshot/Event 전체를 로컬 .wrpl 파일로 저장
-> Client 종료 후에도 파일 유지
-> Scene_MainMenu 의 Replay 버튼에서 파일 선택
-> Scene_Replay 에서 재생
```

이번 Replay 계획은 기존 `.md/plan/backend/Phase10_ReplayService.md` 보다 앞서는 **Client local replay MVP** 이다.
즉, 지금 당장 사용자가 요구한 흐름은 Go Replay Service, 업로드, DB 저장 없이 로컬 클라이언트 파일만으로 먼저 완성한다.

---

## 1. Read Order

1. `00_REPLAY_INDEX.md`
   - 현재 폴더의 입구.
2. `01_CLIENT_LOCAL_REPLAY_MASTER_PLAN.md`
   - 제품 흐름, 데이터 흐름, 단계별 목표.
3. `02_CODE_HANDOFF_POSITIONS.md`
   - 사용자가 직접 반영할 `.h/.cpp/.vcxproj` 위치 목록.
4. `03_VERIFICATION_CHECKLIST.md`
   - smoke / 수동 검증 / 실패 시 확인 순서.
5. `04_R0_IMPLEMENTATION_CODE_HANDOFF.md`
   - R0 진행 순서, 방향성, 남은 계획, 파일별 코드 블록.
6. `05_R1_SERVER_REPLAY_RECORDER_PLAN.md`
   - R1 서버 authoritative replay recorder 계획.
7. `06_R1_IMPLEMENTATION_CODE_HANDOFF.md`
   - R1 Server/GameRoom 반영용 코드 블록.

참조 문서:

- `.md/plan/backend/Phase10_ReplayService.md`
  - 장기: 서버 녹화 + Go Replay Service + metadata/download.
- `.md/TODO/05-09/ServerAICompletion.md`
  - 서버 권위 구조와 Snapshot/Event 단일화 원칙.

---

## 2. Scope Split

### R0. Client Local Replay MVP

이번 작업 범위.

- Client InGame 이 수신한 `Snapshot` / `Event` raw payload를 메모리에 누적한다.
- InGame ImGui 버튼으로 누적분을 `.wrpl` 파일에 저장한다.
- MainMenu Replay 패널에서 로컬 `.wrpl` 파일을 선택한다.
- `Scene_Replay` 가 파일을 읽고 기존 `CSnapshotApplier` / `CEventApplier` 로 재생한다.

### R1. Server Replay Recorder

후속.

- `CGameRoom` 이 spectator snapshot + event stream 을 서버에서 녹화한다.
- 유저가 중간에 나가도 서버 권위 기준 전체 경기 replay를 남긴다.

### R2. Backend Replay Service

후속.

- Go `Services/cmd/replay` 로 업로드한다.
- PostgreSQL metadata와 local storage, 나중에 S3/MinIO/CDN 확장.

### R3. User Replay Library

후속.

- `sessionID -> backend userID` 브리지 이후 내 리플레이, bookmark, 공유, 권한을 붙인다.

---

## 3. MVP Rule

R0에서는 새 게임 상태 직렬화를 만들지 않는다.

- 위치/HP/미니언/타워/챔피언 상태: 기존 `Shared::Schema::Snapshot` raw payload.
- 이동/스킬/Fx/피격/투사체/애니메이션: 기존 `Shared::Schema::EventPacket` raw payload.
- 킬 로그: MVP는 `DamageEvent.bKilled` 또는 HP 0 전이를 기반으로 표시한다. 장기적으로 `KillFeed`/`Death` event를 서버에서 명시 발행한다.

즉, `.wrpl` 은 새 gameplay schema가 아니라 기존 wire payload를 담는 컨테이너다.

---

## 4. Current Code Anchors

서버/네트워크 수신 흐름:

```text
Client/Private/Scene/InGameNetworkBridge.cpp
  CInGameNetworkBridge::Initialize(...)
    frameHandler(type, sequence, payload, len)
      Snapshot -> CSnapshotApplier::OnSnapshot(...)
      Event    -> CEventApplier::OnEvent(...)
```

클라이언트 적용기:

```text
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/Network/Client/EventApplier.cpp
```

InGame ImGui:

```text
Client/Private/Scene/Scene_InGame.cpp
  CScene_InGame::OnImGui()
```

MainMenu:

```text
Client/Public/Scene/Scene_MainMenu.h
Client/Private/Scene/Scene_MainMenu.cpp
```

Scene enum:

```text
Client/Public/Defines.h
```

---

## 5. Code Progress Rule

이 Replay 작업의 코드 진행 방식:

1. Codex는 먼저 목표와 원리를 설명한다.
2. 수정할 `.h/.cpp/.fbs/.vcxproj` 경로를 정확히 나열한다.
3. 각 파일에서 수정할 함수 또는 삽입 위치를 지정한다.
4. 필요한 코드 블록을 세션에 제공한다.
5. 사용자가 직접 반영한다.
6. 사용자가 검토 요청을 하면 Codex가 실제 코드베이스를 읽고 직접 수정한다.
7. 신규/수정 코드와 문서는 `Id`보다 `ID` 표기를 우선한다. 단, FlatBuffers accessor나 기존 public contract는 유지한다.
