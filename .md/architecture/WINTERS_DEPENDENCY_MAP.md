# Winters Dependency Map (실측 기반)

작성일: 2026-07-09 (커밋 f9d4d5c 기준 전수 감사). 규칙의 의도는 `WINTERS_CODEBASE_COMPASS.md`가 소유하고, 이 문서는 **실측 상태**(무엇이 지켜지고 무엇이 위반인지, 파일:라인 근거)를 소유한다. 코드가 바뀌면 이 문서가 스테일해질 수 있다 — 재검증 후 인용할 것.

## 1. 빌드 그래프 (Winters.sln, Debug|x64 / Release|x64)

```text
Engine (WintersEngine.dll)            Engine/Include/Engine.vcxproj
  PostBuild: UpdateLib.bat  ->  EngineSDK/inc,lib,bin  (SDK 배포 허브)

GameSim (WintersGameSim.lib, static)  Shared/GameSim/Include/GameSim.vcxproj
  <- Client, Server, SimLab 이 ProjectReference로 소비 (소스 중복 컴파일 아님)
  PreBuild: UpdateLib.bat / BeforeCompile: run_codegen.bat (flatc)

Client (WintersGame.exe)              Client/Include/Client.vcxproj
  링크: EngineSDK/lib의 WintersEngine.lib  ★ Engine ProjectReference 없음 — sln 의존성만
  ProjectReference: GameSim
  PostBuild: Shaders 복사 + EngineSDK dll/pdb 복사

Server (WintersServer.exe)            Server/Include/Server.vcxproj
  ProjectReference: Engine + GameSim / 링크: ws2_32, Mswsock

SimLab (Tools), WintersAssetConverter (Tools), EldenRingClient (WintersElden.exe)
  ※ AssetConverter는 Engine .cpp 14개를 WINTERS_STATIC_BUILD로 재컴파일 (의도된 설계)
  ※ EldenRingEditor는 CMake 전용 — sln 빌드로는 절대 컴파일되지 않음 (rot 위험)

Services (Go)                          Services/Makefile + docker-compose (MSBuild 외부)
```

주의(빌드 그래프 함정 — 감사 확정):
- **flatc 동시 실행 레이스**: 동일한 FlatcCodegen 타깃이 GameSim/Client/Server 세 프로젝트에서 Inputs/Outputs 없이 매 빌드 실행됨. `msbuild /m`에서 Client와 Server가 병렬로 같은 `Shared/Schemas/Generated/cpp/*_generated.h`를 재작성할 수 있다.
- **UpdateLib.bat 호출처 6곳** (Engine PostBuild x2, Client/Server/GameSim/EldenRingClient PreBuild, CMake POST_BUILD) — 병렬 빌드 시 EngineSDK 파일 레이스 가능. 스크립트 자체가 잠긴 파일 위험을 주석으로 인정 (전체 purge는 `WINTERS_SDK_PURGE=1`일 때만).
- **Client/EldenRingClient는 Engine ProjectReference가 없다** — vcxproj 단독 빌드 시 EngineSDK/lib의 stale lib에 링크된다. 반드시 sln 경유로 빌드.
- **CMake WintersEngine은 플래그가 다르다**: `IMGUI_API=__declspec(dllexport)` 누락 — CMake로 빌드한 DLL이 EngineSDK/bin을 덮으면 vcxproj 클라이언트에서 ImGui 심볼 문제 발생 가능.
- 체크인된 generated .cpp 3종(LoLGameplayDefinitions/LoLVisualDefinitions/ChampionGameData)은 빌드가 재생성하지 않음 — 원본 데이터와의 drift는 python 생성기를 수동 재실행할 때까지 침묵.

## 2. 계층 규칙 실측 결과 (2026-07-09 grep 전수)

| 규칙 | 상태 | 근거 |
|---|---|---|
| Engine → Client/Server/Shared include 금지 | ✅ 위반 0건 | Engine 626개 파일 grep 0건; include 경로에 제품 코드 없음 |
| Server → Client include 금지 | ✅ 위반 0건 | 주석 언급 1건뿐 (ServerMinionTuning.h:6) |
| Client/Public ID3D11 노출 금지 | ✅ 위반 0건 | 164개 파일 0건; 네이티브 핸들은 void*로만 통과 |
| Engine/Public ID3D11 노출 | ✅ 0건 | DX11 concrete는 Private/RHI/DX11에 격리 |
| EngineSDK/inc 수작업 drift | ✅ 없음 | Engine/Public·Include와 diff 일치 (17건 차이는 전부 UpdateLib.bat 규칙) |
| **Shared/GameSim → Engine include 금지** | ⚠️ **어댑터 경유로 축소 (2026-07-09)** | 직접 include 0건 — 전부 Phase 7F 어댑터(`Core/Ecs/*`, `Core/World/World.h`) 경유. 백엔드는 여전히 Engine CWorld (아래 §3) |
| Shared → DX/ImGui 직접 include 금지 | ✅ 직접 0건 / ⚠️ 전이 오염 절단됨 | Engine_Defines 체인(dinput.h/using-namespace) 절단 완료 (아래 §3); DirectXMath는 WintersMath 경유로 잔존 (math-only 허용) |
| **경계 강제 장치** | ✅ lint 가동 (2026-07-09) | `Tools/Harness/Check-SharedBoundary.ps1` — GameSim PreBuild에서 실행, 직접 ECS/Engine_Defines/DX/imgui include 발견 시 빌드 실패 |
| Engine에 LoL 명사 금지 | ⚠️ 그레이존 | StatusPanelState.h(Dragons/Barons), UI_Manager.cpp "Baron" 킬피드 라벨 — view-state 메커니즘 자체는 허용, 어휘가 제품 종속 |
| Engine/Public에 imgui.h 금지 | ⚠️ 1건 | UI_Manager.h:19 (SDK 배포 시 *_Manager.h purge로 완화되나 리포 내 노출) |
| Client/Public 위생 | ⚠️ | Scene_InGame.h/Scene_Editor.h/Scene_Loading.h에 imgui.h; ClientNetwork.h에 WinSock2.h + pragma lib; Defines.h 말미 `using namespace Client` |

## 3. 최대 위반 클러스터: Shared/GameSim → Engine (Phase 7F 진행 중)

2026-07-09 슬라이스 1·2 적용 후 상태:

1. ~~Windows 헤더 오염~~ **절단 완료**: `TransformComponent.h`/`VisionComponents.h`/`NavAgentComponent.h`에서 `Engine_Defines.h` include 제거 — `<dinput.h>`, `using namespace DirectX/std`, `#define new`, OutputDebugStringA 매크로 재정의가 더 이상 Shared TU로 전이되지 않는다. 이 체인에 의존하던 TU들은 명시 include로 전환 (Shared 5개 + Server SnapshotBuilder는 Shared 소유 `Core/Debug/SimDebugOutput.h` 사용 — Engine_Defines와 같은 가드 공유).
2. ~~직접 include 80개 파일~~ **어댑터 경유로 재라우팅 완료**: `Shared/GameSim/Core/Ecs/` 어댑터 9종(Entity/ISystem/SpatialIndex/TransformComponent/SpatialAgentComponent/VisionComponents/CoreComponents/NavAgentComponent/NavigationThrottleComponent) 신설, Shared 78+1개 파일의 `ECS/*` 직접 include를 어댑터/`Core/World/World.h` 경유로 치환. 직접 include 잔존 0건 — `Check-SharedBoundary.ps1`이 GameSim PreBuild에서 강제.
3. **잔존 (마지막 단계, 설계 필요)**: 월드가 여전히 Engine 타입 — `Core/World/World.h:11` `using World = ::CWorld;` (WINTERS_ENGINE dllexport → 링크 의존). `GameSim.vcxproj`의 `EngineSDK\inc` include 경로도 어댑터·WintersTypes/WintersMath 해석을 위해 유지 중. Shared 소유 결정론 ECS 백엔드를 만들어 어댑터를 repoint해야 EngineSDK/inc 제거와 링크 의존 소멸이 가능하다 (WINTERS_ENGINE export 경계 + vcxproj 재작업 포함 — 별도 세션).

부가 잔존: WintersTypes.h(105)/WintersMath.h(34) 직접 include (foundation 모듈 분리 시 함께 이동), GameSim 2개 cpp의 ProfilerAPI.h 사용, TurretAISystem.cpp의 Engine 시스템 사용.

## 4. 런타임 데이터 흐름 (요약)

```text
[Client 입력] → CCommandSerializer → TCP(PacketEnvelope+FlatBuffers CommandBatch)
  → Server IOCP worker → CFrameParser → CPacketDispatcher → CGameRoom::OnCommandBatch
  → CCommandIngress (seq 게이트, Move 병합, ingress mutex)
  → [30Hz tick 스레드, m_stateMutex] DrainCommands → ServerBotAI(Command 생산만)
  → ICommandExecutor::ExecuteCommand → Shared 시스템들 + 15개 챔피언 GameSim::Tick
  → 서버 전용 페이즈(미니언/터렛/투사체/사망·리스폰) → LagComp 기록
  → Snapshot/Event FlatBuffers → 전 세션 broadcast (+CReplayRecorder)
  → Client CSnapshotApplier/CEventApplier → ECS visual 적용 → 보간/예측/yaw 보호
```

- Bot AI는 GameCommand 생산자다. truth 컴포넌트를 직접 쓰지 않는다 (챔피언 봇 경로는 검증 완료; 미니언/터렛/사망·리스폰은 서버 권위 코드가 의도적으로 직접 mutate — 챔피언 yaw 컨벤션과 수동 동기화 필요, gotcha 2026-05-20).
- 네트워크 스레드는 tick 락을 잡지 않는다 (ingress mutex까지만). OnSessionJoin/Leave는 m_stateMutex를 잡으므로 느린 tick이 accept를 지연시킬 수 있음.

## 5. 이 지도의 유지 규칙

- 경계 규칙을 바꾸면 compass를, 실측 상태가 바뀌면 이 문서를 갱신한다.
- 새 위반을 발견하면 파일:라인 근거와 함께 §2 표에 추가하고, 해소되면 상태를 갱신한다 (스테일 위반 주장 인용 금지 — WINTERS_ENGINE_INTEGRATION_REVIEW.md의 Engine→GameSim UI 위반 주장은 f9d4d5c에서 이미 해소된 선례).
