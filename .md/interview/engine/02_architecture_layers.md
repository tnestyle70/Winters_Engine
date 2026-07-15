# 02. 레이어 아키텍처 · 의존성 경계 (Layer Architecture & Dependency Boundaries)

> 면접 대본 겸 지식 베이스. 코드 문법(C++ DLL/링크 메커니즘)은 `.md/interview/cpp/02_compile_link_dll.md`가 담당하고,
> 이 챕터는 "Winters를 왜 이렇게 나눴고, 경계를 어떻게 지키는가"라는 도메인 의사결정을 다룬다.
> 모든 근거는 repo 실측 기준 — `.md/architecture/WINTERS_DEPENDENCY_MAP.md`(2026-07-09 커밋 f9d4d5c 전수 감사)와 실제 파일을 인용한다.

---

## ① 도메인 한 줄 정의

"Winters는 **'이 코드가 gameplay truth를 만드는가, presentation을 만드는가'** 라는 단일 판정 질문으로
Shared/GameSim · Server · Engine · Client · Tools 5계층을 나눴고, 그 경계를 문서가 아니라
**vcxproj include 경로, ProjectReference 유무, SDK 헤더 배포 스크립트, PreBuild lint**로 빌드 수준에서 강제한 구조입니다."

면접에서 첫 문장으로 이걸 말하고, 바로 "경계가 규칙이 아니라 빌드 실패로 강제된다"는 점을 차별점으로 잡는다.

---

## ② 구조와 데이터 흐름

### 5계층의 책임과 허용 의존 방향

`WINTERS_CODEBASE_COMPASS.md` "계층 책임" 절이 규칙의 원본이다. 요약하면:

| 계층 | 산출물 | 책임 | 허용 의존 | 금지 |
|---|---|---|---|---|
| **Shared/GameSim** | WintersGameSim.lib (static) | 서버 권위 gameplay truth의 데이터·결정론 시뮬레이션 계약. GameCommand, Snapshot/Event schema, gameplay component, 챔피언 GameSim | (원칙적으로) 없음 — 최하층 | Engine/Client/Renderer/UI/ImGui/DX11 include 금지 |
| **Server** | WintersServer.exe | GameCommand 수신→30Hz truth tick→Snapshot/Event 송신. 위치/HP/쿨타임/피해/승패의 권위 | Engine + GameSim (ProjectReference) | Client include 금지, 서버 로그만으로 클라 비주얼 성공 판정 금지 |
| **Engine** | WintersEngine.dll | 창, frame loop, RHI, renderer, resource, ECS primitive, UI rendering — "제품 무지(product-agnostic)의 실행 능력" | 제품 코드 의존 0 | champion/skill/lane 같은 제품 명사를 모른다. Engine UI panel이 CWorld/GameSim/제품 매니저 직접 조회 금지 |
| **Client** | WintersGame.exe (LoL), WintersElden.exe | 입력→intent(Command), Snapshot→view state→화면. 보간/약한 예측/애니·FX 재생 | EngineSDK + Shared schema/component | authoritative truth 생산 금지, Client/Public에 ID3D11* 노출 확장 금지 |
| **Tools/Editor** | SimLab, WintersAssetConverter, EldenRingEditor | 런타임 계약(.wmesh/.wskel/.wanim 등 Winters binary) 생산·검증 | 용도별 | Editor 기능이 normal F5 runtime을 우회/은폐 금지 |

핵심은 방향이 한쪽으로만 흐른다는 것:

```text
                 (진실 방향)                          (표현 방향)
 Shared/GameSim ──> Server ──(Snapshot/Event)──> Client ──> 화면
       ▲              │                            │
       │              └── Engine (ProjectReference)└── EngineSDK (배포된 lib/헤더만)
       │
  Engine은 이 어디에도 의존하지 않는다 (제품 무지)
```

실측(DEPENDENCY_MAP §2, grep 전수): Engine→제품코드 include 위반 0건(626개 파일),
Server→Client 0건, Client/Public ID3D11 노출 0건. 유일한 대형 위반이었던
Shared→Engine은 아래 ④에서 다룬다.

### 빌드 그래프 — 경계가 링크 구조로 표현된다

`WINTERS_DEPENDENCY_MAP.md` §1의 실측 빌드 그래프:

```text
Engine (WintersEngine.dll)          Engine/Include/Engine.vcxproj
  PostBuild: UpdateLib.bat → EngineSDK/inc,lib,bin   (SDK 배포 허브)

GameSim (WintersGameSim.lib, static) Shared/GameSim/Include/GameSim.vcxproj
  ← Client, Server, SimLab이 ProjectReference로 공유 (소스 중복 컴파일 아님)
  PreBuild: UpdateLib.bat + Check-SharedBoundary.ps1 (경계 lint)

Client (WintersGame.exe)            Client/Include/Client.vcxproj
  링크: EngineSDK/lib의 WintersEngine.lib  ★ Engine ProjectReference 없음
  ProjectReference: GameSim만

Server (WintersServer.exe)          Server/Include/Server.vcxproj
  ProjectReference: Engine + GameSim / 링크: ws2_32, Mswsock
```

여기서 비대칭이 의도적이다. Client.vcxproj를 열어보면 `AdditionalIncludeDirectories`에
`EngineSDK\inc`, `AdditionalDependencies`에 `WintersEngine.lib`이 있고 ProjectReference는
GameSim 하나뿐이다(Client/Include/Client.vcxproj:57,69,435). 반면 Server.vcxproj는
Engine과 GameSim을 모두 ProjectReference한다(Server/Include/Server.vcxproj:168,176).
**Client는 "배포된 SDK를 소비하는 외부 소비자"로, Server는 "엔진과 함께 사는 권위 실행기"로
취급한 것이 링크 구조 자체에 새겨져 있다.**

### EngineSDK 헤더 배포 구조 — UpdateLib.bat

Engine 빌드 PostBuild가 `UpdateLib.bat`을 실행해 SDK를 미러링한다 (UpdateLib.bat 실측):

1. `Engine/Include/*.h`(공개 API)와 `Engine/Public/**`(내부 헤더, 하위폴더 유지)를 `EngineSDK/inc/`로 xcopy.
2. **복사 직후 `del /Q /S EngineSDK\inc\*_Manager.h`** — 내부 매니저 헤더를 SDK에서 물리적으로 제거한다(UpdateLib.bat:39). Client는 매니저를 `CGameInstance` 게이트웨이로만 접근할 수 있다. "매니저 직접 include 금지"가 규칙이 아니라 **파일 부재**로 강제된다.
3. `WintersEngine.lib/dll/pdb`를 `EngineSDK/lib,bin`의 Debug/Release로 복사, ThirdParty 런타임 DLL(Assimp+transitive, DirectXTK, FMOD)도 함께 배포.
4. 전체 purge(`rd /S`)는 `WINTERS_SDK_PURGE=1`일 때만 — 병렬 빌드 중 파일 잠김으로 SDK 트리가 반쯤 비어 Client 컴파일이 깨진 사고 이후 조건부로 바꿨다(스크립트 주석에 사고 경위가 남아 있다, UpdateLib.bat:19-21).

### DLL 경계의 의미

Engine이 DLL인 것은 단순 배포 형태가 아니라 **API 표면을 좁히는 장치**다:

- 신규 export는 `CGameInstance` 게이트웨이 하나로 제한하고, 내부 매니저(CTimer_Manager 등)는 export 마크 없이 CGameInstance가 unique_ptr로 소유한다(WINTERS_ENGINE_CONVENTIONS.md §3.1~3.2).
- 핫패스는 게이트웨이 포워딩을 못 견디므로 Tier2(JobSystem/ECS World/RHI draw)는 순수 가상 인터페이스(`IJobSystem`/`IWorld`) Getter를 한 번만 받고 포인터 캐시로 직접 호출한다. 구현체 `CJobSystem`/`CWorld`는 export 마크 없이 DLL 내부에 남는다(§3.3) — **DLL 경계는 인터페이스만 통과한다.**
- DLL 경계에서 STL 타입 값 반환 금지(std::vector<T> → out-param/opaque handle), dllexport 클래스의 unique_ptr 멤버는 copy ctor/assign 명시 delete(§3.4, gotcha 2026-04-23).
- 의도된 예외: `WintersAssetConverter`는 독립 EXE라 Engine .cpp 14개를 `WINTERS_STATIC_BUILD` 정의로 재컴파일해 dllexport/dllimport를 비활성화한다(Tools/WintersAssetConverter/WintersAssetConverter.vcxproj:52-53). 같은 Engine 소스가 DLL과 static EXE 두 방식으로 컴파일되고, WINTERS_ENGINE 매크로가 그 스위치다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1. truth/presentation 이분법으로 계층을 나눴다

- **왜**: 서버 권위(server-authoritative) 게임에서 최악의 버그는 "클라가 몰래 truth를 만드는" 종류다. 치트 표면이 되고, 재현이 안 되고, 결정론이 깨진다.
- **대안**: 기능별 분할(렌더링/오디오/네트워크 모듈), 혹은 단일 exe 안의 논리적 네임스페이스 분리.
- **선택 이유**: 기능별 분할은 "이 값이 권위인가"라는 질문에 답을 못 준다. cross-module 작업 전 체크리스트 1번이 "gameplay truth인가, presentation인가?"(compass 작업 전 체크)일 정도로 이 질문 하나가 모든 세부 규칙(Engine은 제품 명사를 모른다, Server는 클라 비주얼을 판정하지 않는다, Bot AI는 command만 생산한다)의 상위 제약이다. 설계 철학 P2도 "계층 간 오염 방지의 최전선은 의존성 방향"이라고 명시한다(WINTERS_DESIGN_PHILOSOPHY.md P2).
- **감수한 비용**: 같은 개념이 두 번 존재한다. 예: 애니메이션 수치가 게임플레이 타이밍(ServerPrivate)과 비주얼 재생(ClientPublic)으로 이원화되고, 그 사이 drift 버그(공속-애니 미반영)가 실제로 났다(WINTERS_DATA_ARCHITECTURE.md D-4 감사). 이원화는 경계의 비용이고, 그 비용을 감사로 관리한다.

### 결정 2. Client에는 Engine ProjectReference를 일부러 안 걸었다

- **왜**: "Client는 엔진 소스와 함께 사는 코드가 아니라 SDK 소비자"라는 관계를 빌드 그래프에 새기고 싶었다. Engine 내부 헤더 전체가 아니라 배포된 `EngineSDK/inc`만 보이게 하려는 목적.
- **대안**: Server처럼 ProjectReference를 걸면 빌드 순서가 자동 보장되고 stale lib 문제가 사라진다.
- **선택 이유**: ProjectReference를 걸면 include 경로도 Engine 소스 트리로 자연히 열리고, "SDK로 배포된 것만 쓴다"는 경계가 형식화되지 않는다. Engine을 나중에 별도 배포물(진짜 SDK)로 뗄 수 있는 구조를 유지하는 쪽을 택했다.
- **감수한 비용**: **Client 단독 빌드 시 EngineSDK/lib의 stale lib에 링크되는 함정**이 생겼다(DEPENDENCY_MAP §1 주의 목록). 반드시 sln 경유로 빌드해야 하고, 이 함정 자체를 의존성 지도에 명시해 관리한다. "재빌드하면 낫는" 종류의 사고라 원인 추적이 어렵다는 것도 알고 감수한 비용이다.

### 결정 3. SDK 배포를 스크립트로 하되 `*_Manager.h`를 purge한다

- **왜**: "Client는 CGameInstance로만 매니저에 접근한다"는 규칙을 리뷰로 지키는 건 오래 못 간다.
- **대안**: (a) 문서에만 적기, (b) 매니저 헤더를 애초에 Public에서 빼기.
- **선택 이유**: (b)는 Engine 내부 TU들이 서로 매니저를 include해야 해서 불가. 그래서 **복사 후 삭제** — SDK 트리에서 `*_Manager.h`가 아예 존재하지 않으니 Client가 include하면 컴파일 에러다. 규칙이 파일 시스템 수준의 사실이 된다(UpdateLib.bat:38-39).
- **감수한 비용**: UpdateLib.bat 호출처가 6곳(Engine PostBuild x2, 각 프로젝트 PreBuild, CMake)이라 병렬 빌드 시 EngineSDK 파일 레이스가 가능하다(DEPENDENCY_MAP §1). 미러링 스크립트라는 간접층 자체가 새 함정(파일 잠김, stale 복사)을 만든다.

### 결정 4. 컴파일러가 못 잡는 경계는 PreBuild lint로 강제한다

- **왜**: Shared/GameSim의 include 경로에는 어댑터·WintersTypes 해석을 위해 `EngineSDK\inc`가 열려 있다(GameSim.vcxproj:56). 즉 **Shared가 Engine 헤더를 include해도 컴파일은 된다.** 컴파일러로는 이 규칙을 강제할 수 없다.
- **대안**: (a) 문서+리뷰 습관, (b) include 경로 자체를 제거(현재는 백엔드가 Engine CWorld라 불가능), (c) 텍스트 lint.
- **선택 이유**: `Tools/Harness/Check-SharedBoundary.ps1`이 Shared 하위 .h/.cpp/.hpp/.inl 전수를 `#include "(ECS/|Engine_Defines.h|Client/|Server/|d3d11|dxgi|imgui)` 패턴으로 스캔하고, Phase 7F 어댑터(`Shared/GameSim/Core/Ecs/*`, `Core/World/World.h`)만 ECS/ include를 화이트리스트하며, 위반 시 file:line 출력 + exit 1로 **GameSim 빌드 자체를 실패**시킨다. 이 스크립트가 GameSim.vcxproj PreBuildEvent에 UpdateLib.bat 직후로 걸려 있다(GameSim.vcxproj:61-64, Debug/Release 양쪽). 의존성 규칙을 '리뷰 습관'에서 '컴파일 게이트'로 승격한 것.
- **감수한 비용**: 정규식 lint라 위양성/우회(문자열 조작된 include 등) 가능성이 이론상 있고, 매 빌드 PowerShell 기동 비용이 붙는다. 대신 규칙이 절대 조용히 무너지지 않는다.

### 결정 5. Shared→Engine 절단을 빅뱅이 아니라 어댑터 슬라이스로 했다

- **왜**: 2026-07-09 감사에서 Shared/GameSim이 ECS/* 헤더를 80개 파일에서 직접 include하는 최대 위반 클러스터가 확정됐다(DEPENDENCY_MAP §3).
- **대안**: (a) Shared 소유 결정론 ECS를 즉시 새로 만들어 한 번에 이관(빅뱅), (b) 위반을 그대로 두고 문서만 남기기.
- **선택 이유**: 빅뱅은 80개 파일 + 링크 경계 + vcxproj 재작업이 한 변경에 몰려 검증 불가능해진다. 대신 3단계 슬라이스: (1) 헤더 오염 절단 — Engine_Defines.h include를 제거해 `<dinput.h>`, `using namespace DirectX/std`, `#define new`, OutputDebugStringA 매크로 재정의의 전이 오염부터 끊음, (2) **어댑터 간접층 삽입** — `Shared/GameSim/Core/Ecs/` 어댑터 9종(Entity/ISystem/SpatialIndex/TransformComponent/SpatialAgentComponent/VisionComponents/CoreComponents/NavAgentComponent/NavigationThrottleComponent, 실파일 확인)을 신설해 78+1개 파일의 직접 include를 재라우팅, 직접 include 0건 달성 후 lint로 고정, (3) 백엔드 교체는 별도 세션으로 미룸. 어댑터는 이렇게 생겼다 — "백엔드 교체 시 이 파일만 바꾼다"가 주석으로 박혀 있다:

```cpp
// Shared/GameSim/Core/Ecs/TransformComponent.h (전문)
#pragma once
// Phase 7F adapter: Shared/GameSim 코드는 Engine ECS 헤더를 직접 include하지 않고
// 이 어댑터를 경유한다. 백엔드 교체 시 이 파일만 바꾼다 (WINTERS_DEPENDENCY_MAP.md §3).
#include "ECS/Components/TransformComponent.h"
```

- **감수한 비용**: 잔존 부채가 명시적으로 남는다 — `Core/World/World.h:11`의 `using World = ::CWorld;`는 여전히 Engine dllexport 타입이라 **링크 의존이 살아 있고**, GameSim include 경로의 `EngineSDK\inc`도 그래서 못 뺀다. "직접 include 0건"과 "의존 0"은 다르다는 걸 지도에 정직하게 기록했다.

### 결정 6. 규칙(의도)과 실측(감사)을 다른 문서가 소유한다

- **왜**: 아키텍처 문서는 코드가 바뀌면 스테일해지고, 스테일 문서를 근거로 잘못된 수정이 나온다.
- **선택**: 의도는 `WINTERS_CODEBASE_COMPASS.md`, 실측 상태(무엇이 지켜지고 무엇이 위반인지 file:line 근거)는 `WINTERS_DEPENDENCY_MAP.md`가 소유. 실측 문서는 스스로 "코드가 바뀌면 스테일해질 수 있다 — 재검증 후 인용"을 명시하고, 이미 해소된 위반 주장 재인용을 금지한다(DEPENDENCY_MAP 서두·§5). 최상위 규칙은 code wins over docs.
- **감수한 비용**: 문서 두 개를 유지해야 하고, 경계 규칙 변경 시 양쪽 갱신 의무가 생긴다.

---

## ④ 어려웠던 점과 해결

### (1) Shared→Engine 전이 오염 — include 하나가 계층 전체를 오염시킨다

가장 아팠던 위반. Shared 컴포넌트 헤더들이 `Engine_Defines.h`를 include하고 있었는데,
이 체인은 `<dinput.h>` 같은 Windows 헤더, `using namespace DirectX/std`, `#define new`,
OutputDebugStringA 매크로 재정의까지 통째로 끌고 온다. 즉 **Shared TU 하나가 Engine의
전처리 환경 전체에 오염**됐고, "결정론 시뮬레이션 계층"이 사실상 Engine 빌드 환경의 부속물이었다.
해결은 위 결정 5의 3단계 슬라이스. 교훈: 위반의 위험은 "include 한 줄"이 아니라
그 헤더가 끌고 오는 **매크로/using/전역 정의의 전이(transitive) 오염**이다. 이걸 면접에서
"왜 레이어 위반이 위험한가"의 구체 답으로 쓴다.

### (2) 빌드 그래프의 세 가지 지뢰 (감사로 확정, DEPENDENCY_MAP §1)

- **flatc 코드젠 레이스**: 동일 FlatcCodegen 타깃(`BeforeTargets="ClCompile"`, GameSim.vcxproj:142)이 GameSim/Client/Server 세 프로젝트에 Inputs/Outputs 선언 없이 걸려 있어 `msbuild /m` 병렬 빌드에서 Client와 Server가 같은 `*_generated.h`를 동시에 재작성할 수 있다.
- **UpdateLib.bat 6곳 호출 레이스**: 병렬 빌드 시 EngineSDK 파일 레이스. 실제로 purge 중 파일 잠김으로 SDK 트리가 반쯤 비어 Client 컴파일이 깨진 사고가 있었고, 그래서 전체 purge를 `WINTERS_SDK_PURGE=1` 옵트인으로 바꿨다.
- **stale lib 링크**: Client에 Engine ProjectReference가 없어 단독 빌드 시 낡은 WintersEngine.lib에 링크된다. "재빌드하면 사라지는" 증상이라 가장 추적하기 어려운 부류 — sln 경유 빌드를 규칙화하고 지도에 명시했다.

이 셋의 공통점은 **비결정적이고 재현이 안 되는 빌드 사고**라는 것. 코드 버그보다 신뢰를 더 깎기 때문에 감사 문서에 못박아 두는 것 자체가 해결의 절반이다.

### (3) "경계 문서"가 또 하나의 거짓 소스가 되는 문제

WINTERS_ENGINE_INTEGRATION_REVIEW.md의 "Engine→GameSim UI 위반" 주장이 이미 해소됐는데도
재인용될 뻔한 선례가 있었다(DEPENDENCY_MAP §5). 해결이 결정 6 — 실측 문서에 감사 기준
커밋(f9d4d5c)을 박고, 스테일 위반 주장 인용을 명시적으로 금지했다.

---

## ⑤ 향후 개선 방향

1. **Phase 7F 마지막 단계 — Shared 소유 결정론 ECS 백엔드**: `using World = ::CWorld;`를 Shared 소유 구현으로 repoint하면 어댑터 9종은 그대로 두고 backing만 바뀐다. 그때 비로소 GameSim의 `EngineSDK\inc` include 경로와 WINTERS_ENGINE 링크 의존을 제거할 수 있고, Check-SharedBoundary lint가 아니라 **include 경로 부재**로 경계가 강제되는 최종 형태가 된다(DEPENDENCY_MAP §3).
2. **FlatcCodegen에 Inputs/Outputs 선언**: 매 빌드 실행과 병렬 레이스를 동시에 없앤다. 코드젠 소유 프로젝트를 GameSim 하나로 줄이는 방향도 검토.
3. **Engine의 LoL 명사 그레이존 정리**: StatusPanelState.h의 Dragons/Barons 같은 어휘가 제품 종속(⚠️ 그레이존, DEPENDENCY_MAP §2). view-state 메커니즘은 유지하되 어휘를 제품 중립으로.
4. **경계 lint의 확장**: 현재 lint는 Shared만 커버한다. Client/Public ID3D11 노출 같은 다른 경계 audit은 Run-S17RhiValidation.ps1 하네스에 있는데, PreBuild 수준으로 내리는 것을 검토(2026-07-10 감사의 "규칙은 있는데 기계 강제가 없다" 메타패턴 해소 방향).

---

## ⑥ 면접 Q&A

### Q1. "엔진 아키텍처를 어떤 기준으로 나눴나요?"

**답변 골격**: 기능(렌더링/오디오)이 아니라 **소유권** 기준입니다. 판정 질문은 하나 — "이 코드가 gameplay truth를 만드는가, presentation을 만드는가". Shared/GameSim이 결정론 규칙과 데이터, Server가 권위 실행(30Hz tick→Snapshot), Engine이 제품 무지의 실행 능력(창/RHI/ECS primitive), Client가 제품별 presentation, Tools가 런타임 계약 생산·검증. 각 계층에 금지사항이 명시돼 있습니다 — Engine은 champion/skill 같은 제품 명사를 모르고, Server는 클라 비주얼 성공을 판정하지 않습니다.

**꼬리질문 대비**: "왜 그 질문이 기준인가?" → 서버 권위 게임에서 치트·비결정론·재현 불가 버그의 뿌리가 전부 "presentation 계층이 truth를 만드는" 위반이기 때문. Bot AI조차 truth 컴포넌트를 직접 못 고치고 GameCommand만 생산하게 한 것도 같은 축이다(예외인 미니언/터렛 직접 mutate는 문서에 명시된 트레이드오프).

### Q2. "아키텍처 규칙이 시간이 지나면 무너지는데, 어떻게 지켰나요?"

**답변 골격**: 세 겹으로 강제했습니다. (1) **링크 구조** — Client는 Engine ProjectReference 없이 EngineSDK lib만 링크, Server만 직접 참조. (2) **파일 부재** — SDK 배포 스크립트가 내부 매니저 헤더(*_Manager.h)를 복사 후 삭제해서 Client가 include 자체를 못 함. (3) **PreBuild lint** — 컴파일러가 못 잡는 Shared→Engine include 규칙은 Check-SharedBoundary.ps1이 GameSim PreBuild에서 스캔해 위반 시 빌드를 실패시킵니다. 규칙을 문서에서 컴파일 게이트로 승격한 게 핵심입니다.

**꼬리질문 대비**: "lint를 우회하면?" → 어댑터 2개 경로만 화이트리스트된 정규식 스캔이라 정상적 include는 전부 걸린다. 근본 해결은 백엔드 교체 후 include 경로 자체를 제거하는 것이고, lint는 그때까지의 교량이라고 답한다.

### Q3. "레이어 위반이 실제로 있었나요? 어떻게 갚았나요?"

**답변 골격**: 최대 위반은 Shared/GameSim→Engine이었습니다. 80개 파일이 ECS 헤더를 직접 include했고, Engine_Defines 체인이 dinput.h, using namespace, #define new, 로그 매크로 재정의까지 Shared TU로 전이 오염시켰습니다. 빅뱅 대신 3단계 슬라이스로: 헤더 오염 절단 → Core/Ecs 어댑터 9종으로 재라우팅(직접 include 0건, lint로 고정) → Shared 소유 ECS 백엔드 교체(잔존 과제). 어댑터 파일마다 "백엔드 교체 시 이 파일만 바꾼다"가 주석으로 박혀 있습니다.

**꼬리질문 대비**: "그럼 지금 의존이 0인가?" → 아니다. `using World = ::CWorld;` 한 줄이 Engine dllexport 타입이라 링크 의존이 남아 있고, 이걸 의존성 지도에 정직하게 '잔존'으로 기록했다. "직접 include 0건"과 "의존 0"을 구분해서 말하는 게 신뢰를 준다.

### Q4. "왜 Client는 Engine을 ProjectReference로 안 걸었나요?"

**답변 골격**: Client를 "SDK 소비자"로 취급하기 위해서입니다. ProjectReference를 걸면 Engine 소스 트리 전체가 사실상 열리는데, EngineSDK/inc·lib만 링크하게 하면 "배포된 표면만 쓴다"가 빌드 그래프에 새겨집니다. Server는 반대로 권위 실행기라 Engine을 직접 참조합니다 — 이 비대칭이 계층 관계의 표현입니다.

**꼬리질문 대비**: "부작용은?" → 단독 빌드 시 stale lib 링크 함정. sln 경유 빌드를 규칙화했고 의존성 지도에 함정으로 명시했다. 함정을 숨기지 않고 문서화한 것까지가 설계라고 답한다.

### Q5. "DLL 경계에서 조심한 것들은?"

**답변 골격**: 표면 최소화와 ABI 안전 두 축입니다. 신규 export는 CGameInstance 게이트웨이 하나로 제한하고 내부 매니저는 export 마크 없이 unique_ptr 소유. 핫패스(JobSystem/ECS/RHI draw)는 게이트웨이 포워딩 비용을 못 견뎌서 순수 가상 인터페이스 Getter를 한 번만 받아 포인터 캐시로 직접 호출 — DLL 경계는 인터페이스만 통과합니다. ABI 쪽은 STL 값 반환 금지(C4251), dllexport 클래스의 unique_ptr 멤버는 copy 명시 delete, private ctor + Create() 팩토리.

**꼬리질문 대비**: "왜 STL 값 반환이 문제인가?" → cpp 챕터(02_compile_link_dll) 영역이지만 한 줄로: DLL과 EXE가 다른 CRT/STL 레이아웃을 가질 수 있어 경계에서 STL 객체 소유권이 넘어가면 힙/레이아웃 불일치 위험. 그래서 out-param과 opaque handle만 통과.

### Q6. "include 경로가 열려 있으면 컴파일러가 경계를 못 잡는데, 그건 어떻게 했나요?"

**답변 골격**: 정확히 그 문제를 겪었습니다. GameSim은 어댑터와 수학 헤더 해석 때문에 EngineSDK/inc가 include 경로에 있어서, Shared가 Engine 헤더를 include해도 컴파일이 됩니다. 그래서 경계 검사를 컴파일러 밖으로 꺼내 텍스트 lint(Check-SharedBoundary.ps1)로 만들고 PreBuild에 물려 빌드 실패로 변환했습니다. 최종형은 백엔드 교체 후 include 경로 자체를 제거하는 것 — lint는 그때까지 경계가 후퇴하지 않게 잠그는 래칫(ratchet)입니다.

**꼬리질문 대비**: "vcxproj include path 수준에서 경계를 본다는 게 무슨 뜻인가?" → 어떤 프로젝트의 include 경로에 어떤 트리가 열려 있는지가 그 계층의 "볼 수 있는 세계"를 정의한다. EngineSDK/inc가 비Engine 프로젝트에 열려 있으면 레이어 규칙이 컴파일-강제 불가능해진다는 걸 gotcha로 박제했다(2026-07-09).

### Q7. "멀티 프로젝트 빌드에서 겪은 어려움은?"

**답변 골격**: 비결정적 사고 세 가지입니다. flatc 코드젠 타깃이 3개 프로젝트에서 Inputs/Outputs 없이 병렬 실행돼 같은 generated 헤더를 동시 재작성할 수 있는 레이스, SDK 미러링 스크립트가 6곳에서 호출돼 병렬 빌드 시 파일 레이스(실제로 purge 중 파일 잠김으로 SDK 트리가 반쯤 비는 사고 → 전체 purge를 환경변수 옵트인으로 변경), Client 단독 빌드 시 stale lib 링크. 공통점은 "재빌드하면 낫는" 증상이라 원인 추적이 최악이라는 것 — 전수 감사로 지도에 못박고 운영 규칙(sln 경유, purge 옵트인)으로 봉쇄했습니다.

**꼬리질문 대비**: "왜 바로 안 고쳤나?" → Inputs/Outputs 선언은 로드맵에 있고, 수정보다 먼저 한 게 '감사로 확정해 문서에 박기'였다. 비재현 사고는 존재 인지가 절반이기 때문.

### Q8. "Engine 하나로 LoL과 Elden 두 게임을 올린다는데, 경계는 어떻게 되나요?"

**답변 골격**: WintersEngine.dll 위에 WintersLOL.exe와 WintersElden.exe를 별도 제품 클라이언트로 올립니다. Elden은 LoL의 Scene/champion registry를 재사용하지 않고, 렌더러 계층도 복제하지 않습니다 — 같은 RHI renderer에 서로 다른 RenderWorldSnapshot을 공급하는 방식입니다. Engine이 제품 명사를 모르게 유지한 것이 이 멀티 제품 구조의 전제조건입니다. Engine에 "Baron" 같은 LoL 어휘가 스민 그레이존이 감사에 잡혀 있고, 정리 대상입니다.

**꼬리질문 대비**: "그게 왜 전제조건인가?" → Engine이 champion을 알면 Elden 빌드에 LoL 개념이 링크되고, 두 제품의 변경이 서로를 리컴파일시킨다. 제품 무지는 취향이 아니라 두 번째 제품이 생기는 순간의 생존 조건.

### Q9. "이 구조에서 지금 가장 부끄러운 부분은?" (압박 질문 대비)

**답변 골격**: 두 개를 정직하게 말합니다. (1) Shared의 World가 여전히 Engine CWorld의 alias라 "결정론 시뮬레이션 계층"이 Engine DLL에 링크 의존한다는 것 — 어댑터까지 갔지만 백엔드 교체가 남았습니다. (2) 경계 강제가 계층마다 불균등하다는 것 — Shared 경계는 PreBuild lint로 매 빌드 강제되지만 Client/Public의 그래픽스 노출 경계는 push 전 하네스 audit 수준입니다. "규칙은 있는데 기계 강제가 없다"가 최근 감사에서 뽑은 메타패턴이고, 강제 장치의 균질화가 다음 단계입니다.

**꼬리질문 대비**: 약점을 물으면 로드맵과 세트로 답한다. 부채 목록(의존성 지도 §3, 에러정책 §4)이 이미 '해소/잔존' 상태로 관리되고 있음을 근거로.

---

## ⑦ 다른 챕터와의 연결

- **cpp/02_compile_link_dll.md**: 이 챕터의 "DLL 경계" 결정들의 언어/링커 메커니즘(dllexport/dllimport, C4251, ODR, static vs shared) — WINTERS_STATIC_BUILD 스위치와 stale lib 링크 함정의 원리 설명은 그쪽.
- **cpp/11_architecture_ecs.md**: Engine ECS primitive와 Shared가 어댑터로 소비하는 컴포넌트들의 내부 구조.
- **cpp/10_error_handling.md**: 계층별 로그 채널 분리(Server=std::cerr, Shared=WintersOutputAIDebugStringA)가 필요한 이유인 Engine_Defines 매크로 게이트 — 경계 위반(전이 오염)이 로깅에까지 미친 사례.
- **(engine) 서버 권위 · 네트워크 챕터**: 이 챕터의 truth/presentation 이분법이 런타임에서 실제로 흐르는 경로(Command→30Hz tick→Snapshot→SnapshotApplier)와 스레드 경계(IOCP는 ingress mutex까지).
- **(engine) 데이터 아키텍처 챕터**: 계층 경계의 데이터 버전 — ServerPrivate 값은 Server에만, ClientPublic 값은 Client에만 컴파일되는 "데이터 경계 = 링크 경계" 설계.
- **(engine) 빌드/검증 파이프라인 챕터**: Check-SharedBoundary 외의 게이트들(Run-S17RhiValidation 하네스, gotchas 운영, 이중 빌드 시스템 sln/CMake의 소유권 분리와 EldenRingEditor rot 위험).
