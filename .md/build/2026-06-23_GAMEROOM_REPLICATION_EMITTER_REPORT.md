# 2026-06-23 GameRoom ReplicationEmitter 진행 보고서

## 범위

- 기준 문서: `.md/plan/refactor/14_CODEX_PROMPT_ATOMIC_DECOMPOSITION.md`
- ROLE: `SERVER_ATOMIZE`
- 대상: GameRoom Stage 2 `ReplicationEmitter` owner 추출
- 이번 변경은 서버 replication event 직렬화 경계만 다뤘다.

## 변경 요약

### 1. ReplicationEmitter owner 추가

- 새 파일:
  - `Server/Public/Game/ReplicationEmitter.h`
  - `Server/Private/Game/ReplicationEmitter.cpp`
- `CReplicationEmitter`가 담당하는 것:
  - `ReplicatedActionComponent` action-start 이벤트 수집
  - action-start 이벤트 직렬화
  - `ReplicatedEventComponent` entity 정렬 수집
  - replicated event 직렬화

### 2. GameRoomReplication adapter 축소

- `GameRoomReplication.cpp`에서 직접 하던 action/event 수집과 serializer 호출을 `CReplicationEmitter`로 이동했다.
- `CGameRoom`에 남긴 것:
  - replay event 기록
  - session lookup
  - packet envelope wrapping
  - session send
  - projectile net unbind
  - consumed replicated-event entity destroy
- `CGameRoom::BroadcastReplicatedEvent` 멤버 선언/구현은 제거했다.

## 경계 확인

- `ReplicationEmitter`는 packet/session/network/replay transport를 참조하지 않는다.
- event payload를 보내는 side effect는 계속 `CGameRoom::BroadcastEventPayload`에 남아 있다.
- projectile net unbind와 event entity destroy도 `CGameRoom` adapter에 남겼다.
- 기존 per-event 순서를 보존하기 위해 replicated event는 entity별로 build -> broadcast -> optional unbind -> destroy 순서를 유지했다.

## 검증

### 정적 검증

- `git diff --check` 통과
  - LF -> CRLF 안내만 있음
- owner side-effect rg 통과
  - `CSession`, packet, `WrapEnvelope`, `Session_Manager`, `ReplayRecorder`, `Send(` 참조 없음
- `BroadcastReplicatedEvent` 낡은 선언/구현 제거 확인

### 빌드

- 명령:
  - `MSBuild Server\Include\Server.vcxproj /m:1 /nodeReuse:false /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:IntDir=C:\Users\tnest\Desktop\Winters\Server\Bin\Intermediate\CodexDebug\`
- 결과:
  - Server Debug x64 빌드 성공
  - 오류 0개
- 경고:
  - 기존 EngineSDK DLL interface 경고
  - 기존 `GameRoomMinionAI.cpp` UTF-8 문자 경고
  - Codex 전용 `IntDir` 분리에 따른 MSB8028 안내

### 스모크

- 명령:
  - `.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10`
- 결과:
  - 정상 종료
  - champion sim hooks 등록 확인
  - port 9000 listen 확인
  - replay 기록 없음으로 skip save

## 변경 파일

- `Server/Public/Game/ReplicationEmitter.h`
- `Server/Private/Game/ReplicationEmitter.cpp`
- `Server/Private/Game/GameRoomReplication.cpp`
- `Server/Public/Game/GameRoom.h`
- `Server/Include/Server.vcxproj`
- `Server/Include/Server.vcxproj.filters`

## 다음 후보

1. `ServerProjectileAuthority`
   - `GameRoomProjectiles.cpp`의 turret projectile/skill projectile 판정과 replicated event 생성 분리.
2. `ReplicationEmitter` 후속
   - snapshot emission 쪽은 session별 controlled entity, ack, replay snapshot이 섞여 있어 별도 작은 slice로 진행해야 한다.
