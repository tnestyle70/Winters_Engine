# 2026-06-23 GameRoom SERVER_ATOMIZE 진행 보고서

## 범위

- 기준 문서: `.md/plan/refactor/14_CODEX_PROMPT_ATOMIC_DECOMPOSITION.md`
- ROLE: `SERVER_ATOMIZE`
- 대상: GameRoom Stage 2 원자 owner 추출
- 이번 변경은 GameRoom 서버 경계만 다뤘고, Client/Shared/Engine ROLE은 건드리지 않았다.

## 변경 요약

### 1. WorldBootstrap owner 추가

- 새 파일:
  - `Server/Public/Game/WorldBootstrap.h`
  - `Server/Private/Game/WorldBootstrap.cpp`
- `GameRoomSpawn.cpp`에 있던 stage/fallback bootstrap 판단을 request 생성으로 분리했다.
- `CWorldBootstrap`이 담당하는 것:
  - fallback structure spawn request 생성
  - stage structure entry -> structure spawn request 변환
  - stage jungle entry -> jungle spawn request 변환
- `CGameRoom`에 남긴 것:
  - `m_world` entity 생성
  - component 부착
  - nav/grid/flow field 갱신
  - session binding

### 2. ServerAICommandProducer owner 추가

- 새 파일:
  - `Server/Public/Game/ServerAICommandProducer.h`
  - `Server/Private/Game/ServerAICommandProducer.cpp`
- `CGameRoom::Phase_ServerBotAI`는 인게임 phase gate 이후 producer 호출만 남겼다.
- 봇 초기 lane 결정 로직을 GameRoom 멤버에서 `CServerAICommandProducer::ResolveInitialBotLane`으로 이동했다.
- `GameRoomSpawn.cpp`의 champion AI component 생성은 producer의 lane 결정 결과만 사용한다.

## 경계 확인

- `WorldBootstrap`은 packet/session/network/replay/world/entity-map side effect를 직접 참조하지 않는다.
- `ServerAICommandProducer`는 command 생산 owner로 한정하고 packet/session/network transport를 참조하지 않는다.
- 실제 gameplay truth는 계속 Server/GameSim 흐름에 남겼다.

## 검증

### 정적 검증

- `git diff --check` 통과
  - LF -> CRLF 안내만 있음
- 금지 dependency rg 통과
- owner side-effect rg 통과

### 빌드

- 1차 기본 Debug 빌드는 실패:
  - 원인: `Server/Bin/Intermediate/Debug/vc143.pdb` 공유 잠금
  - 동시에 남아 있던 `MSBuild/cl` 프로세스가 Debug PDB를 잡고 있었다.
- 재검증:
  - `IntDir=C:\Users\tnest\Desktop\Winters\Server\Bin\Intermediate\CodexDebug\`
  - `OutDir`는 기본 Debug 유지
  - 결과: Server Debug x64 빌드 성공, 오류 0개
- 남은 경고:
  - 기존 `EngineSDK` DLL interface 경고 2개
  - `IntDir` 임시 분리에 따른 MSB8028 안내 2개

### 스모크

- 명령:
  - `.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=10`
- 결과:
  - 정상 종료
  - champion sim hooks 등록 확인
  - port 9000 listen 확인
  - replay 기록 없음으로 skip save

## 변경 파일

- `Server/Public/Game/WorldBootstrap.h`
- `Server/Private/Game/WorldBootstrap.cpp`
- `Server/Public/Game/ServerAICommandProducer.h`
- `Server/Private/Game/ServerAICommandProducer.cpp`
- `Server/Private/Game/GameRoomSpawn.cpp`
- `Server/Private/Game/GameRoomChampionAI.cpp`
- `Server/Public/Game/GameRoom.h`
- `Server/Include/Server.vcxproj`
- `Server/Include/Server.vcxproj.filters`

## 다음 후보

1. `ReplicationEmitter`
   - `GameRoomReplication.cpp`의 event serialization/broadcast adapter를 result 기반 owner로 줄이는 방향.
2. `ServerProjectileAuthority`
   - `GameRoomProjectiles.cpp`의 turret projectile/skill projectile 판단을 owner로 분리.
3. `WorldBootstrap` 후속
   - champion spawn loadout request까지 분리할 수 있으나 session binding과 AI component 초기화가 섞여 있어 더 작은 단계로 나눠야 한다.
