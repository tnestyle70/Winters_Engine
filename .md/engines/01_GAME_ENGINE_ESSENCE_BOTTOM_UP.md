# 게임 엔진의 본질 — 가장 밑바닥에서부터 층층이 쌓아 올리는 설명

작성일: 2026-07-10. Winters(자체 DX11 엔진 + 30Hz 서버 권위 GameSim) 실물 코드를 축으로, 게임 엔진이라는 물건을 L0(플랫폼)부터 L10(배포/백엔드)까지 층 단위로 해부하고 각 층의 UE/Unity 대응물을 매핑한다. 기술면접 답변과 UE/Unity 툴 포트폴리오 설계의 공통 기반 문서다. 이 문서의 모든 Winters 경로는 레포에서 실재 확인을 거쳤다. 외부 검증 사실은 출처 도메인, 일반 공학 지식은 (일반 지식), 이번 세션에서 검증하지 못한 사실은 (미검증)으로 표기한다.

---

## §0. 본질 한 줄 — 엔진이란 무엇인가

> **게임 엔진 = 게임 내용과 무관하게 반복되는 문제들의 재사용 가능한 해답 묶음 + 데이터를 실행 가능한 경험으로 바꾸는 파이프라인.**

두 절반을 나눠 보면:

1. **재사용 가능한 해답 묶음** — 어떤 게임을 만들든 "창을 띄우고, GPU에 말을 걸고, 시간을 전진시키고, 무언가를 그리고, 입력을 받고, 통신한다"는 문제는 반복된다. 이 반복 문제의 해답을 게임 코드에서 분리해 둔 것이 엔진 런타임이다.
2. **데이터 → 경험 파이프라인** — 아티스트/디자이너가 만든 원본 데이터(FBX, PNG, 수치 테이블)를 검증된 런타임 계약으로 변환해 게임에 흘려보내는 경로다. Winters가 UE에서 가져오기로 결정한 것도 개별 툴이 아니라 바로 이 규율이다 — "자산이 검증된 런타임 계약으로만 게임에 들어간다" (`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §0).

**왜 모든 엔진은 같은 층으로 수렴하는가.** 층 구조는 취향이 아니라 문제 공간의 형태다.

- 각 층은 서로 다른 **변화 속도**를 가진 문제를 푼다. OS/GPU API는 몇 년 단위로 바뀌고, 렌더 기법은 분기 단위로, 게임 콘텐츠는 일 단위로 바뀐다. 변화 속도가 다른 것을 한 덩어리로 묶으면 가장 빠른 변화가 전체를 계속 깨뜨리므로, 경계(인터페이스)가 자연 발생한다.
- 두 개의 **비대칭**이 파이프라인 형태를 강제한다. (a) "임포트는 한 번, 로드는 매 실행" — 그래서 오프라인 쿠킹 층(L2)이 생긴다. (b) "저작은 수천 번, 출시는 한 번" — 그래서 에디터/툴 층(L9)이 런타임과 분리된다.
- **권위(authority)의 소재**가 층을 가른다. 무엇이 진실인가(서버 시뮬레이션)와 무엇이 보이는가(클라이언트 프레젠테이션)는 다른 문제고, 이 분리가 L4/L8의 형태를 결정한다.

그래서 Winters, UE, Unity는 구현 언어도 규모도 다르지만 아래 §L0~§L10에서 보듯 층별로 1:1:1 대응물이 존재한다. 대응물이 없는 곳은 "그 엔진이 그 문제를 아직 안 풀었거나, 의도적으로 안 푸는 곳"이며 그 자체가 설계 결정이다.

---

## L0 — 플랫폼 / 메인 루프 / 메모리

**원리.** 가장 밑바닥 문제는 세 개다. (1) OS로부터 창·입력·시간을 얻는다. (2) "게임은 멈추지 않는 시뮬레이션"이므로 이벤트 대기가 아니라 **루프**로 돈다 — 매 반복이 `입력 → 갱신 → 렌더`. (3) 프레임 예산(16.6ms/33ms) 안에서 메모리를 할당·해제해야 하므로 범용 할당자에 다 맡길 수 없고, 소유권/수명 전략이 엔진의 성격을 결정한다.

**Winters 실물.**
- 루프와 창: `Engine/Public/Framework/CEngineApp.h` — `Initialize/Run/Shutdown`과 내부 `Update(f32_t deltaTime)/Render()`가 프레임 루프의 전부다. 창은 `Engine/Public/Platform/CWin32Window.h`(그 위에 `IPlatformWindow.h` 추상화), 시간은 `Engine/Public/Core/CTimer.h`.
- 병렬화: `Engine/Public/Core/JobSystem.h` + `Engine/Private/Core/JobSystem.cpp`. 완성된 heap `WorkItem`의 pointer token만 worker별 고정 4096-slot Chase-Lev deque에 publish하고, 외부 submit/overflow는 global queue로 간다. Submit/Shutdown admission lease가 publish와 deque/fiber 파괴 사이 lifetime race를 닫는다. 실행 모드는 `ThreadOnly`(기본), `FiberShell`, `FiberFull`이고, FiberFull은 worker마다 64개 native fiber(64 KiB commit/256 KiB reserve)를 pool로 만들어 counter wait stack을 origin worker ready ring에서 재개한다. canonical primitive는 `Engine/Public/Core/JobSystem/WorkStealingDeque.h`, `Core/JobCounter.h`, `Core/Fiber/FiberPool.h`다.
- Windows 연결: 실제 병렬 실행은 `std::thread` OS workers가 담당한다. `ConvertThreadToFiberEx/CreateFiberEx/SwitchToFiber`는 같은 worker 위에서 user-mode continuation을 multiplex하고, IOCP의 kernel completion wait를 대체하지 않는다. worker identity는 TLS, sibling fiber마다 달라야 하는 nested Submit/profiler stack은 FLS다. Visual Studio Debug x64 빌드와 `Tools/Harness/RunJobSystemStress.ps1 -Mode all`은 서로 다른 증거다. 전자는 ABI/링크, 후자는 last-item CAS·lifecycle·nested wait·pool saturation correctness를 검증한다.
- 메모리: GC 없음. `unique_ptr` 소유권 + 디버그 CRT 추적(`Engine/Public/Engine_Defines.h`의 `DBG_NEW` 매크로, 39~40행). 이 매크로가 ImGui/Tracy의 placement new와 충돌해 `push_macro/undef/pop_macro` 격리 패턴을 쓰는 것까지가 실전 경험이다(`.md/interview/tool-development.md` §1).

**UE 대응물.** `FEngineLoop::Tick`이 게임 스레드 루프의 심장이고, 메모리는 이중 체제다 — 일반 C++는 커스텀 할당자(FMemory/FMalloc), `UObject` 파생만 리플렉션 기반 mark-and-sweep GC가 수거한다 (일반 지식).

**Unity 대응물.** 네이티브 C/C++ 코어가 플레이어 루프를 돌리고 C# 래퍼를 통해 사용자 코드를 호출한다 (docs.unity3d.com). 관리 힙은 Boehm-Demers-Weiser GC를 **incremental 모드**로 쓰며, 비세대(non-generational)·비압축(non-compacting)이라 힙 단편화가 가능하다 (docs.unity3d.com). 그래서 "프레임당 관리 할당 0바이트"가 Unity 성능 문화의 기본기가 됐고, 그 탈출구로 Burst+Job+NativeContainer가 존재한다. 2026년 로드맵은 Mono/IL2CPP 이원 체제를 CoreCLR로 교체하는 방향이다 (discussions.unity.com, 로드맵이므로 일정은 유동).

**3엔진 메모리 전략 비교.**

| | Winters | UE | Unity |
|---|---|---|---|
| 기본 전략 | 수동 소유권(`unique_ptr`), GC 없음 | 커스텀 할당자 + UObject 한정 GC (일반 지식) | Boehm incremental GC (docs.unity3d.com) |
| 결정론과의 관계 | GC 정지 없음 → 30Hz 고정 틱에 유리 | GC 스파이크를 클러스터링 등으로 관리 (일반 지식) | GC 스파이크 회피가 코딩 컨벤션을 지배 |
| 대가 | 댕글링/이중 해제를 사람이 막아야 함 | UObject 밖 객체는 여전히 수동 | 네이티브 객체는 어차피 수동(Destroy) |

---

## L1 — 그래픽스 디바이스 추상화 (RHI)

**원리.** GPU에 말을 거는 API(DX11/DX12/Vulkan/Metal)는 플랫폼마다 다르고 세대마다 바뀐다. 렌더러가 특정 API 타입(`ID3D11Device*`)에 직접 의존하면 API 교체가 전면 재작성이 된다. 그래서 디바이스/버퍼/텍스처/파이프라인 상태/커맨드 제출을 **백엔드 중립 핸들과 인터페이스**로 감싸는 층이 생긴다. 핵심 설계 판단은 "추상화 수준" — 너무 얇으면 이식성이 없고, 너무 두꺼우면 API별 강점을 못 쓴다.

**Winters 실물.**
- 인터페이스군: `Engine/Public/RHI/IRHIDevice.h`, `IRHICommandList.h`, `IRHISwapChain.h`, `IRHIPipelineState.h`, `IRHIBindGroup.h`, `RHIHandles.h`, `RHIDescriptors.h`, `RHIShaderCompiler.h`.
- DX11 백엔드: `Engine/Private/RHI/DX11/CDX11Device.cpp`. 텍스처 로딩 초크포인트는 `Engine/Public/RHI/RHITextureLoader.h`(실패를 스테이지+파일+원인으로 보고하는 패턴의 기준 사례).
- 경계 규칙이 문서로 강제된다: "Client/Public 또는 Shared에 `ID3D11Device`, `ID3D11ShaderResourceView` 노출을 늘리지 않는다" (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` RHI 섹션). DX11이 기본 생존 경로이고 DX12/Vulkan은 이 RHI를 통해 점진 도입한다.

**UE 대응물.** 같은 이름의 층이 그대로 있다 — RHI 모듈이 D3D12/Vulkan/Metal 백엔드를 감싸고, 그 위에 RDG(Render Dependency Graph)가 패스/리소스 수명을 관리한다 (일반 지식).

**Unity 대응물.** 디바이스 추상화(내부 GfxDevice 계층)는 네이티브 코어 안에 숨겨져 있어 사용자에게 노출되지 않는다 (일반 지식). 사용자에게 열린 것은 그 위층인 SRP다(→ L6). 즉 Unity는 "L1은 봉인, L6은 개방", Winters/UE는 둘 다 자기 소유라는 차이가 있다.

---

## L2 — 에셋 파이프라인 (import → cook → zero-copy load)

**원리.** 원본 포맷(FBX/glTF/PNG)은 저작 도구를 위한 포맷이지 런타임을 위한 포맷이 아니다. "임포트는 한 번, 로드는 매 실행"이라는 비대칭 때문에, 비용이 큰 해석(파싱, 탄젠트 생성, 본 가중치 정규화)을 오프라인으로 옮기고 런타임은 GPU 업로드 형태와 일치하는 바이너리를 거의 `memcpy`로 읽게 만든다. 부수 효과가 본질만큼 중요하다 — 검증 시점이 런타임의 "조용한 실패"에서 빌드 타임의 "시끄러운 실패"로 당겨진다.

**Winters 실물.**
- 포맷: `Engine/Public/AssetFormat/` 아래 `Mesh/WMeshFormat.h`, `Anim/WAnimFormat.h`, `Anim/WSkelFormat.h`, `Material/WMaterialFormat.h`, 공통 `Common/WintersFileHeader.h`(magic "WINT" + major/minor 버전 + flags + content_size, `pack(1)`+`static_assert`로 레이아웃 고정).
- 쿠커: `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`(진입점은 `Engine/Private/Tools/AssetConverter/main.cpp`, Assimp 링크) + 일괄 배치 `Tools/convert_all_assets.bat`. 27개 FBX/GLB 전수 변환 FAIL=0 실적.
- zero-copy 로드: `Engine/Public/AssetFormat/Mesh/WMeshLoader.h` — 파일 블롭을 통째로 읽고 정점/인덱스 블롭 포인터가 원본 버퍼 내부를 가리켜 파싱 없이 GPU로 직행한다.
- 스테이지 데이터도 같은 원리: `Shared/GameSim/Definitions/MapDataFormats.h`의 `STAGE_MAGIC`/`STAGE_VERSION=5`/`STAGE_VERSION_MIN_COMPAT=3` — 포맷 진화(v2→v5)를 실제로 겪고 하위 호환 창을 유지한 실물이다. 산출물은 `Data/Stage1.dat`, `Data/Stage1.navgrid`.
- Elden 계열은 별도 파이썬 파이프라인 `Tools/EldenAssetPipeline/elden_pipeline.py`가 있다.
- 알려진 약점(자기비판): `WMeshFormat.h`의 메시 메타 헤더에 버전 필드가 없고 저장이 원자적이지 않다 — 에디터 코어 로드맵 선행 과제로 등록됨 (`.md/interview/tool-development.md` §4).

**UE 대응물.** Import→`.uasset`→Cook 파이프라인과 DDC(Derived Data Cache) (일반 지식). 쿠킹 결과물이 (원본, 설정, 컨버터 버전)의 순수 함수라는 성질이 DDC 캐시 키의 근거다. 참고로 Fab 마켓의 코드 플러그인도 "소스 제출 → Epic이 엔진 버전별로 컴파일" 구조라, 파이프라인 규율이 마켓 정책에까지 이어진다 (dev.epicgames.com, forums.unrealengine.com).

**Unity 대응물.** AssetDatabase가 임포터를 돌려 결과를 `Library/` 캐시 아티팩트로 저장하고, 원본+임포트 설정+프로젝트 설정에서 언제든 재생성 가능하다 (docs.unity3d.com). 자산 정체성은 `.meta` 파일의 GUID + 파일 내 fileID 쌍이고, `.meta`를 잃으면 참조가 조용히 전멸한다는 것까지가 계약의 일부다 (docs.unity3d.com). 커스텀 포맷은 `ScriptedImporter`(버전 넘버 bump = 전체 재임포트)로 1급 시민이 된다 (docs.unity3d.com) — Winters `.wmesh` 파이프라인의 직접 등가물이다.

---

## L3 — 오브젝트 / 월드 모델

**원리.** "게임 속의 것"을 코드로 어떻게 표현하는가. 세 가지 검증된 답이 있다 — 상속 트리(고전 OOP), 조립(컴포지션), 데이터 지향(ECS). 그리고 어느 답을 고르든 반드시 따라오는 부속 문제가 **정체성과 직렬화**다: 이 객체를 파일/네트워크/저장에서 무엇으로 식별하고, 어떻게 바이트로 눕히는가.

**Winters 실물.**
- 자체 ECS: 엔진 측 프리미티브는 `Engine/Public/ECS/`(컴포넌트/시스템/`SystemScheduler.h`), 시뮬레이션 측은 어댑터를 경유한 `Shared/GameSim/Core/Ecs/`(`Entity.h`, `ISystem.h`, `TransformComponent.h` 등)와 `Shared/GameSim/Core/World/World.h`. 컴포넌트는 trivially-copyable을 유지해 스냅샷 복제와 충돌하지 않게 한다 (`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §5).
- 템플릿/스폰: `Engine/Public/ECS/Systems/EntityBlueprint.h` — 상속 대신 Installer 함수 조립로 엔티티 템플릿을 정의하고, 리소스 I/O는 1회·스폰은 순수 메모리 조립이라는 분리를 지킨다.
- 정체성 계약이 명문화되어 있다: `DefinitionKey`는 네트워크/저장/manifest용 안정 식별자, `ChampionDefId` 같은 dense ID는 pack-local, `EntityHandle`은 프로세스-로컬이며 JSON/네트워크/저장 경계를 절대 넘지 않는다 (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` DataDriven 섹션). — "ID마다 유효 범위가 다르다"를 계약으로 박은 것이 이 층의 핵심 성취다.
- Shared/GameSim가 Engine 헤더를 직접 include하지 못하게 `Tools/Harness/Check-SharedBoundary.ps1`이 빌드에서 기계 강제한다.

**UE 대응물.** `UObject` 리플렉션(UPROPERTY/UCLASS + UnrealHeaderTool 코드젠) 위의 Actor/Component 모델, 직렬화는 바이너리 `.uasset` (일반 지식). 순수 데이터 자산은 `UDataAsset`/`UPrimaryDataAsset`으로 분리된다 (dev.epicgames.com). Winters는 UObject/GC/리플렉션 전면 도입을 **의도적으로 비채택**했다 — trivially-copyable 컴포넌트 + 스냅샷 복제 모델과 충돌하기 때문 (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §5).

**Unity 대응물.** GameObject = ID + Transform + 평평한 컴포넌트 리스트, 상속 계층 없음. 수명은 네이티브가 쥐고 MonoBehaviour 생명주기(Awake→OnEnable→Start→FixedUpdate→Update→LateUpdate→OnDestroy)를 호출한다 (docs.unity3d.com). 직렬화는 텍스트 YAML + GUID/fileID로, diff/merge/외부 툴 생성이 가능하다는 점이 Unity 툴링 생태계의 토대다 (docs.unity3d.com). 데이터 자산은 `ScriptableObject`(.asset, 빌드에서는 읽기 전용) (docs.unity3d.com). ECS는 DOTS(Entities)로 별도 존재하다가 Unity 6.4부터 코어 패키지로 편입 중이고, GameObject와 Entity를 `EntityId`로 통합하는 "ECS for All" 방향이 공식화됐다 (discussions.unity.com). 아키타입/청크 기반이라는 점에서 Winters의 자체 ECS와 개념 축이 같다.

---

## L4 — 시뮬레이션 루프 (시간, 결정론, 물리)

**원리.** 시간 전진 방식이 게임의 물리적 성격을 결정한다. **가변 틱**(렌더 프레임과 동기)은 부드럽지만 결과가 프레임레이트에 종속되고, **고정 틱**은 같은 입력에서 같은 결과를 재현할 수 있다. 서버 권위 + 재현 가능성(리플레이/봇 학습/디버깅)이 필요하면 고정 틱 + 결정론이 사실상 강제된다. 결정론의 적은 세 가지다 — 부동소수점 환경 차이, 순회 순서 비결정성, 숨은 전역 상태(난수/시계).

**Winters 실물.**
- 고정 30Hz: `Shared/GameSim/Core/Determinism/DeterministicTime.h` — `kFixedDt = 1.f/30.f`, `kTicksPerSecond = 30`. 난수는 `DeterministicRng.h`.
- 결정론을 **테스트로 증명**한다: `Tools/SimLab/main.cpp` — "같은 seed + 같은 커맨드 스트림 ⇒ 틱별 상태 해시 동일"을 검사하는 headless 러너, exit code 0/1. 이것이 Blueprint 같은 인터프리터를 시뮬레이션에 넣지 않는 이유의 실증 장치다(§5 비채택 근거: "SimLab 해시 검증 체계가 무너진다").
- 서버 틱 본체: `Server/Private/Game/GameRoomTick.cpp`. 시뮬레이션 시스템들은 `Shared/GameSim/Systems/`(CommandExecutor, AttackChase, ChampionAI 등).
- 물리: 범용 물리 엔진 없이 도메인 특화 판정으로 해결한다 — `Engine/Public/Physics3D/HitVolume.h`(히트 볼륨), 이동은 NavGrid 기반(`Engine/Public/Manager/Navigation/NavGrid.h`, `Pathfinder.h`). MOBA는 강체 역학이 아니라 격자 보행성+판정 볼륨이 진실이라는 도메인 판단이다.
- 클라이언트는 이 루프의 소비자다: 예측/보간은 하되 truth를 만들지 않는다(compass Client 규칙).

**UE 대응물.** 가변 프레임 틱 + 물리 고정 서브스텝이 기본이고, 물리는 Chaos (일반 지식). 네트워크 게임의 결정론은 엔진이 보장해 주지 않아 프로젝트별로 록스텝/스냅샷 전략을 직접 세운다 (일반 지식).

**Unity 대응물.** `Update`(가변) / `FixedUpdate`(고정) 이원 루프이며 물리 시뮬레이션 타이밍은 SimulationMode로 조정 가능하다 (docs.unity3d.com). 기본 3D 물리는 PhysX (일반 지식). DOTS 스택의 Unity Physics는 stateless 설계로 결정론 지향이며 Netcode for Entities와 짝을 이룬다 (unity.com).

---

## L5 — 애니메이션 (스켈레톤 / 스키닝 / 리타겟)

**원리.** 캐릭터는 본 트리(스켈레톤)이고, 애니메이션은 본별 트랜스폼의 시간 곡선이며, 스키닝은 정점을 본 행렬(보통 최대 4개 가중치)로 변형하는 GPU 작업이다. 이 층의 진짜 어려운 문제는 재생이 아니라 **리타겟** — 스켈레톤이 다른 캐릭터 간에 애니메이션을 재사용하는 것 — 과 "애니메이션 타이밍이 게임플레이 판정과 어떻게 엮이는가"다.

**Winters 실물.**
- 런타임: `Engine/Public/Resource/Skeleton.h`, `Bone.h`, `Animation.h`, `Animator.h`, `Model.h`. 쿠킹 포맷은 `WSkelFormat.h`/`WAnimFormat.h`(L2 참조).
- 판정과 연출의 분리가 스키마에 박혀 있다: `Shared/GameSim/Definitions/SkillDef.h`의 `visualCastFrame/visualRecoveryFrame`(클라이언트 연출 프레임)과 서버 권위 windup tick(`Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h`의 `ResolveBasicAttackWindupTicks`)이 서로 다른 축이다 — "타이밍"이 visual-frame(Client)과 sim-tick(Shared)으로 이원화된 구조.
- 성능 실전: 정적 엔티티의 스키닝 갱신을 `bAnimated=false`로 스킵해 프레임 17.8ms→9ms 복구(`.md/interview/tool-development.md` §9의 프로파일러 서사).
- 리타겟은 **미구현이며 의도적으로 선결 과제로 지정**되어 있다 — `.wskel`에 본 매핑+리스트 포즈 보정 프로필을 추가하고 cook 시점 오프라인 베이크로 해결하는 설계 (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-2). 런타임 IK보다 cook-time 베이크가 결정론/성능 원칙에 맞는다는 판단까지가 설계다.

**UE 대응물.** Skeleton 자산 + AnimBlueprint 상태머신 + IK Rig/IK Retargeter, 절차적 제어는 Control Rig (일반 지식; Control Rig의 "스켈레톤 차이를 데이터로 흡수"라는 계약 단위 분해는 `WINTERS_UE_FAB_TOOL_ADOPTION.md` §1 표).

**Unity 대응물.** Mecanim — Animator Controller 상태머신 + Avatar 시스템, Humanoid 릭은 아바타 매핑을 통한 리타겟을 기본 제공한다 (일반 지식).

---

## L6 — 렌더 파이프라인 (컬링 / 패스 / 드로우 제출)

**원리.** L1이 "GPU에 말 거는 법"이라면 L6은 "한 프레임을 어떤 순서로 그릴 것인가"다. 골격은 어느 엔진이나 같다: 보이는 것 추리기(컬링) → 패스 구성(섀도/노멀/불투명/반투명/포스트) → 상태 변경을 최소화하는 정렬로 드로우 제출. 현대적 형태는 패스와 리소스 의존성을 그래프로 선언하는 프레임 그래프다.

**Winters 실물.**
- 드로우 제출/컬링: `Engine/Public/Renderer/ModelRenderer.h` — `RenderFrustumCulled(matViewProj)`, `RenderWithVisibility(const VisibilityMask&)`가 실물이다. 컬링과 가시성 마스크(팀 시야/FOW)가 렌더러 API에 분리되어 있다.
- 패스: `Engine/Public/Renderer/NormalPass.h`, `SSAOPass.h`, `FogOfWarRenderer.h`, PBR 머티리얼 `CMaterialPBR.h`.
- 백엔드 중립 방향: `Engine/Public/Renderer/RenderWorldSnapshot.h` + `RHISceneRenderer.h` — "LoL과 Elden은 렌더러 클래스 계층을 복제하지 않고 같은 RHI 렌더러에 서로 다른 render snapshot을 공급한다"는 compass 규칙의 코드 착지점. 렌더러에 주는 입력을 '스냅샷 데이터'로 정규화한 것이 이 층의 Winters식 데이터 계약이다.

**UE 대응물.** 단일 통합 C++ 디퍼드 렌더러 + 렌더 스레드 분리, 패스 구성은 RDG로 선언 (일반 지식). 사용자 확장은 엔진 소스 수정이나 SceneViewExtension 수준으로 제한적이다 (일반 지식).

**Unity 대응물.** SRP — 렌더 파이프라인 오케스트레이션(컬링, 패스 스케줄, 제출)을 C#으로 열어 둔 프레임워크 위에 URP(모바일~콘솔 범용)와 HDRP(하이엔드)가 서 있고, Unity 6에서 URP에 RenderGraph가 기본 활성화됐다 (docs.unity3d.com). Winters의 RenderWorldSnapshot 방향과 같은 "프레임 그래프로의 수렴"이 3엔진 공통 흐름이다.

---

## L7 — UI (툴 UI와 제품 HUD의 분리)

**원리.** UI는 하나의 문제가 아니라 두 개다. (1) **개발자/디자이너용 툴 UI** — 오늘 만들어 내일 쓰는 물건, 즉시모드(ImGui)가 표준. (2) **플레이어 대면 HUD/메뉴** — 로컬라이즈/게임패드/애니메이션이 필요한 제품, 보존모드가 표준. 이 둘을 한 기술로 통일하려는 시도가 보통 실패한다는 것 자체가 업계 경험칙이다 (일반 지식).

**Winters 실물.**
- 경계가 규칙이다: "ImGui는 툴/튜너 전용이고 런타임 HUD가 아니다" (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §2). ImGui 통합은 `Engine/Public/Editor/ImGuiLayer.h`.
- 제품 HUD는 자체 경로: `Engine/Public/Manager/UI/UI_Manager.h`(`CUI_Manager::Load_TextureSRV` + `CUIAtlasManifest` 재사용이 신규 UI 텍스처 로딩의 기본 검토 순서 — compass LoL DX11 섹션), 렌더는 `Engine/Public/Renderer/UIRenderer.h`, 월드 부착 HUD는 `Engine/Private/Manager/UI/ActorHUDPanel.cpp`.
- UI 로직 스크립팅은 Lua로 한정: `Engine/Public/Manager/UI/LuaUIHost.h`, `Engine/Public/Scripting/LuaRuntime.h` — "분기 로직은 훅 레지스트리, UI 연출만 Lua"라는 비채택 결정(§5)의 절반이다.
- 아키텍처 규칙: "UI panel은 render만 한다. 월드/FOW/스냅샷 판정은 Client bridge가 view data로 만든다" (compass) — UI가 진실 소유자를 직접 조회하지 못하게 하는 의존 차단.

**UE 대응물.** Slate(에디터 전체가 이걸로 만들어진 C++ 선언형 보존모드) + UMG(Slate의 UObject 래퍼, 제품 UI/Editor Utility Widget 공용) (dev.epicgames.com). 툴/제품이 같은 위젯 스택을 공유하는 대신 Slate 자체가 거대한 유지보수 대상이 된 구조다.

**Unity 대응물.** 세 세대 공존 — 레거시 IMGUI(`OnGUI`), 런타임 UGUI, 그리고 UXML/USS 기반 UI Toolkit. Unity 6 공식 문서가 "복잡한 에디터 툴에는 UI Toolkit 권장"을 명시한다 (docs.unity3d.com).

---

## L8 — 네트워킹 (서버 권위 복제)

**원리.** 멀티플레이어의 근본 문제는 "여러 기계가 서로 다른 시점에 같은 세계를 봐야 한다"이고, 검증된 답은 **서버 권위**다: 클라이언트는 의도(입력)만 보내고, 서버가 진실을 계산하며, 클라이언트는 결과(스냅샷/이벤트)를 받아 연출한다. 파생 문제로 지연 은폐(예측/보간), 대역폭(델타/관심 영역), 스키마 진화(버전 핸드셰이크)가 따라온다.

**Winters 실물 — 파이프라인 전체가 파일로 존재한다.**

```text
Client Input
  → GameCommand 직렬화       Client/Private/Network/Client/CommandSerializer.cpp, ClientInputBuffer.cpp
  → 스키마                    Shared/Schemas/Command.fbs (FlatBuffers, run_codegen.bat)
  → transport adapter         TCP: PacketEnvelope/IOCPCore/Session
                              UDP: UdpClient ↔ UdpIocpCore (v3 lane/ACK/fragment/reassembly)
  → logical session/tick handoff
                              ServerSessionHub → CommandIngress
  → Server GameSim 30Hz 틱    Server/Private/Game/GameRoomTick.cpp + Shared/GameSim/Systems/*
  → Snapshot/Event 생성       Server/Private/Game/SnapshotBuilder.cpp, ReplicationEmitter.cpp (Snapshot.fbs / Event.fbs)
  → transport-neutral send    ServerSessionHub::SendFrame → TCP envelope 또는 UDP lane
  → 클라 적용(연출로 소비)    Client/Private/Network/Client/SnapshotApplier.cpp, EventApplier.cpp
```

- **TCP→UDP의 본질은 GameSim 교체가 아니라 transport contract 교체다.** TCP baseline은 16B length-prefixed stream framing과 kernel reliability/order를 쓴다. UDP v3는 40B header, 1200B datagram, 16B fragment header/1144B data, 64 KiB cap을 명시하고 Control/Lobby/Command/Event는 reliable-ordered, Snapshot/Heartbeat는 unreliable-sequenced로 분리한다. `CServerSessionHub`가 공통 logical session ID, command sequence/suspicion, outbound `SendFrame`을 소유하므로 GameRoom은 socket 종류를 알 필요가 없다.
- **Windows IO와 authority의 경계**: `CUdpIocpCore`의 `WSARecvFrom`/`WSASendTo` completion worker는 cookie/connection/generation, ACK, ordered receive, reassembly까지만 처리한다. callback은 bounded owned-frame queue에 넣고, `CGameRoom::Tick`이 mutex 전에 drain한 뒤에만 world를 바꾼다. IOCP wait를 fiber화하지 않는 이유도 같다. kernel completion과 stackful CPU continuation은 다른 문제다.
- **상태 판정**: `Server/Private/main.cpp`는 `tcp|udp|dual` transport CLI와 `thread|fiber-shell|fiber-full` lifecycle을 연결했고 TCP·UDP·dual F5 smoke를 통과했다. TCP fallback은 계속 기본값이며 UDP를 production cutover 완료로 표기하지 않는다. post-handshake MAC/AEAD, RTT/pacing/congestion, IPv6/NAT rebinding/PMTU, graceful close, delta/AOI는 남은 production gates다.
- **불변식: Bot AI도 GameCommand 생산자다.** 봇이 Transform/HP/cooldown 같은 truth 컴포넌트를 직접 고치는 것은 금지이고 command를 생산한다 (compass Server 섹션). 실물이 `Server/Private/Game/ServerAICommandProducer.cpp`다 — AI가 사람과 같은 입구(커맨드 큐)로만 세계에 개입하므로, 치트 검증·리플레이·SimLab 결정론이 봇 유무와 무관하게 성립한다.
- 버전/드리프트 방어: TCP는 `Shared/Network/PacketEnvelope.h`의 version, UDP는 `UdpPacketHeader` v3, 데이터 계층은 `Shared/Schemas/Hello.fbs`의 `dataBuildHash`로 서로 다른 wire/schema/data build를 접속 경계에서 구분한다.
- 조용한 실패 금지: FlatBuffers verify 실패를 bare return으로 삼키면 스키마 드리프트가 네트워크 멈춤과 구분 불가 → 유계 트레이스/카운터 필수 (`.claude/gotchas.md` 2026-07-09).
- 예측의 경계: 로컬 클릭 yaw 예측은 서버 yaw가 따라잡을 때까지만 보호하는 등, "예측은 연출이고 진실은 스냅샷"이라는 규율이 gotchas 다수 항목으로 축적되어 있다.

**UE 대응물.** Actor 프로퍼티 복제 + RPC + NetDriver, 서버 권위 기본 모델 (일반 지식). Iris 복제 시스템 등 세부는 이번 세션 미검증 (미검증).

**Unity 대응물.** Netcode for GameObjects(고수준)와 DOTS 스택의 Netcode for Entities — 후자는 Entities/Physics와 함께 2026년 코어 패키지 편입 예정이 공식화되어 있다 (discussions.unity.com). NGO의 세부 모델은 (일반 지식).

---

## L9 — 툴 / 에디터 (에디터 = 데이터 생산자)

**원리.** 에디터는 게임의 일부가 아니라 **데이터의 생산자**이고 런타임은 **소비자**다. 이 층의 품질은 기능 개수가 아니라 세 가지 계약으로 결정된다: (1) 에디터 산출물과 런타임 로드가 **같은 경로/검증**을 탄다(이중 경로 금지), (2) **Validator가 게이트**다 — 불량 데이터는 조용히 폴백되지 않고 사유와 함께 가시화된다, (3) 편집은 **트랜잭션**(undo/redo)이고 크래시가 사용자 작업물을 파괴하지 않는다.

**Winters 실물.**
- LoL 맵 에디터: `Client/Private/Scene/Scene_Editor.cpp` — `Save_CurrentStage()`(923행), NavGrid 오버레이 `RenderNavGridOverlay()`(872행). 인게임 씬과 같은 `IScene`의 형제 씬으로, "에디터가 만든 데이터는 런타임과 같은 로드 경로"(compass Editor 규칙)를 지킨다.
- UE 스타일 에디터 셸: `EldenRingEditor/Public/EldenRingEditorScene.h` — Viewport/ContentBrowser/WorldCell/Outliner/Details/Transaction/FxGraph/Sequencer/WorldPartition/BossTesting/Log 11패널.
- Undo/Redo: `EldenRingEditor/Public/World/EditorTransaction.h` — `IEditorCommand(Do/Undo/Name)` + `CEditorTransaction` + 구체 커맨드 3종. 선택 상태 복원, 삽입 위치 보존, 드래그 병합(커밋 시 커맨드 1개) 같은 실무 디테일 포함.
- Validator 게이트: `Engine/Public/FX/Graph/FxGraphValidator.h` — `Validate(FxEmitterGraph)`가 노드별 이슈와 위상 정렬 순서를 함께 반환하고, `Engine/Private/FX/Exec/FxExecPlan.cpp`가 검증된 그래프를 실행 플랜으로 컴파일한다. "그래프 자산 → 검증 → 컴파일된 플랜 실행"이라는 Niagara와 동형 구조.
- 라이브 튜닝과 박제의 분리: `Client/Public/UI/EffectTuner.h`, `Client/Private/UI/SkillTimingPanel.cpp`(ImGui 슬라이더로 `SkillDef` 타이밍 실험) — 슬라이더 값은 실험값이고, 영속화는 authoring 소스 역기록 후 재cook만 인정한다 (`WINTERS_UE_FAB_TOOL_ADOPTION.md` §2·§4). 시퀀서 골격은 `Engine/Public/Cinematic/CSequenceAsset.h`/`CSequencePlayer.h`, 월드 스트리밍은 `Engine/Public/World/WorldPartitionSystem.h`.
- 관측 툴: `Engine/Include/ProfilerAPI.h`(RAII 스코프 + 컴파일 타임 게이트 + 유계 버퍼 + Tracy 통합).

**UE 대응물.** 이 층이 UE의 최강 층이다. 사다리 구조가 명확하다 — Editor Utility Widget(분 단위 노코드) → Python 에디터 스크립팅 → 플러그인 템플릿 7종 → Slate → 커스텀 자산 타입(UObject+UFactory+AssetDefinition+`FAssetEditorToolkit` — Niagara/BT/Material 에디터 전부 이 패턴) → IDetailCustomization → UEditorSubsystem/UToolMenus (dev.epicgames.com). Winters의 어빌리티 타임라인 에디터 UE 이식이 정확히 이 커스텀 자산 패턴을 밟는다.

**Unity 대응물.** `[MenuItem]` 한 줄 → `EditorWindow`(UI Toolkit 권장) → `CustomEditor`/`PropertyDrawer` → `ScriptableObject` 데이터 백본 → `ScriptedImporter` → UPM 패키지 배포, 그리고 `SerializedObject/SerializedProperty`를 경유하면 undo/멀티 편집/프리팹 오버라이드가 공짜로 따라온다 (docs.unity3d.com).

---

## L10 — 빌드 / 배포 / 백엔드

**원리.** 게임은 실행 파일 하나가 아니라 **함대**다: 클라이언트 빌드, dedicated server, 매치 밖 상태(계정/매칭/상점)를 소유하는 백엔드 서비스, 그리고 이들 사이의 버전 계약. 이 층의 본질 문제는 "서로 다른 시점에 빌드된 조각들이 만나도 안전한가"이며, 답은 버전 스탬프/핸드셰이크와 재현 가능한 빌드다.

**Winters 실물.**
- dedicated server: `Server/Private/main.cpp` 진입의 콘솔 서버(IOCP), 클라이언트와 별도 실행 파일.
- HTTP 백엔드: `Services/` — Go 마이크로서비스 6종(`Services/cmd/auth`, `leaderboard`, `matchmaking`, `payment`, `profile`, `shop` + 대응 `internal/`), SQL 마이그레이션(`Services/migrations`), 실행은 `Services/Makefile`(`go run ./cmd/<svc>`). 소유권 문서 `Services/README.md`가 "in-match gameplay truth는 소유하지 않는다"를 명시 — L8의 게임 서버와 L10의 백엔드가 다른 진실을 소유한다는 경계다.
- docker: `Services/docker-compose.yml` — postgres 16-alpine(5433), redis 7-alpine, apache/kafka 3.7.0(KRaft)을 인프라로 띄운다. Go 서비스 자체의 Dockerfile은 없다(호스트에서 `go run`) — 컨테이너화는 인프라까지만이 현재 상태.
- 클라이언트 쪽 소비자: `Client/Private/Network/Backend/CHttpClient.cpp` + 타입별 클라이언트(`AuthClient.cpp`, `MatchClient.cpp`, `ProfileClient.cpp`, `PaymentClient.cpp`, `CShopClient.cpp`). CHttpClient의 std::async future 수명 함정은 gotchas에 박제되어 있다(2026-07-09).
- 버전 계약: L8의 `kPacketVersion` + `Hello.dataBuildHash`가 프로세스 간 버전 핸드셰이크의 현재 실물이고, 에셋 쪽은 `WintersFileHeader`의 major/minor가 담당한다. 바이너리 핸드셰이크의 전면 강화는 UE5.7 감사에서 HIGH 갭으로 등록된 로드맵 항목이다.
- 빌드: 레거시 3대장(Engine/Client/Server)은 `.vcxproj` 소유, CMake `WintersWorkspaceMap`은 브라우징 맵일 뿐이라는 소유권 구분(gotchas 2026-05-28), Engine 공개 헤더 변경 시 `UpdateLib.bat`로 `EngineSDK/inc` 동기화. CI 부재가 확인된 최대 약점이다(감사 HIGH).

**UE 대응물.** UBT/UAT 기반 빌드·쿡·패키징, Dedicated Server 타깃 분리 (일반 지식). 플러그인 배포는 `RunUAT BuildPlugin -Rocket`으로 설치형 엔진에 대해 검증하며, 이것이 Fab 제출 전 검증 절차와 동일하다 (dev.epicgames.com). 백엔드는 엔진 밖(EOS 등 별도 서비스) (일반 지식).

**Unity 대응물.** 플레이어 빌드는 Mono(JIT)/IL2CPP(IL→C++ AOT) 백엔드 선택의 문제이고, iOS/콘솔은 AOT가 강제된다 (docs.unity3d.com). 백엔드는 UGS(Unity Gaming Services)가 상응 (일반 지식).

---

## §11. Winters 도메인 ↔ UE ↔ Unity 매핑 총괄표

| Winters 도메인 | 역할 (compass 기준) | UE 대응물 | Unity 대응물 | 한 줄 차이 |
|---|---|---|---|---|
| `Client/` | 제품별 씬/입력/카메라/프레젠테이션, Snapshot을 visual로 소비 | 게임 프로젝트의 Game 모듈 + 클라이언트 타깃 (일반 지식) | 프로젝트의 Assets/ 게임 코드 | Winters는 "클라는 truth를 만들지 않는다"가 문서+리뷰로 강제되는 명시 계약 |
| `Server/` | GameCommand 수신→GameSim 실행→Snapshot/Event 송신하는 권위 | Dedicated Server 타깃 + 리플리케이션 (일반 지식) | Netcode 서버 빌드 (일반 지식) | Winters는 서버가 별도 코드베이스 도메인이라 권위 경계가 폴더 경계와 일치 |
| `Engine/` | 창/루프/RHI/렌더러/리소스/ECS 프리미티브/공통 서비스 | UE Engine 소스 (Runtime 모듈군) | 네이티브 C++ 코어 (닫힘) (docs.unity3d.com) | Unity는 이 층이 봉인, UE는 소스 공개, Winters는 직접 소유 — 학습·개조 자유도의 원천 |
| `EngineSDK/` | Engine 공개 헤더의 배포 사본 (`UpdateLib.bat` 동기화) | 설치형(Launcher) 엔진의 헤더+바이너리 배포 (dev.epicgames.com) | UnityEngine/UnityEditor 어셈블리 API 표면 | "엔진 내부와 소비자 표면의 분리"를 1인 규모에서 수동 재현한 것 — 자동화 부재가 알려진 리스크 |
| `Shared/GameSim/` | 결정론 시뮬레이션 계약: GameCommand/Snapshot 스키마, gameplay 컴포넌트 | 정확한 등가물 없음 — GAS+리플리케이션이 부분 대응 (일반 지식) | 정확한 등가물 없음 — Netcode for Entities의 sim 어셈블리가 근접 (discussions.unity.com) | 클라/서버가 같은 sim 코드를 링크하는 구조는 상용 엔진이 기본 제공하지 않는 Winters의 차별 지점 |
| `Tools/` | 쿠커(AssetConverter)/검증 하네스/SimLab/파이프라인 스크립트 | UAT/커맨드릿 + DDC 파이프라인 (일반 지식) | AssetPostprocessor/ScriptedImporter + CI 스크립트 (docs.unity3d.com) | Winters는 CLI-first(배치·exit code 규약)라 CI 편입이 남은 마지막 단계 |
| `Services/` | 매치 밖 backend state(계정/매칭/상점/리더보드/결제) — Go | EOS/백엔드 서비스 (엔진 외부) (일반 지식) | UGS (일반 지식) | 상용 엔진은 SaaS로 사는 층을 Winters는 Go+Postgres+Redis+Kafka로 직접 소유 |
| docker (`Services/docker-compose.yml`) | 백엔드 인프라(postgres/redis/kafka) 로컬 기동 | 컨테이너화된 dedicated server 운영 관행 (일반 지식) | UGS 호스팅 관행 (일반 지식) | Winters는 인프라만 컨테이너, 게임 서버 컨테이너화는 미착수 |
| `EldenRingEditor/` (+`EldenRingClient/`) | Engine SDK 소비자로 서는 별도 에디터 실행 파일 + 제2 클라이언트 | UnrealEd(에디터 타깃) + 멀티 게임 프로젝트 (일반 지식) | Unity 에디터 자체 + 멀티 프로젝트 | "하나의 엔진 DLL, 복수 제품 exe" 방향의 증거물 — 에디터가 sln에 편입 안 된 상태가 알려진 부채 |

---

## §12. 이 탑에서 가장 본질적인 것 — 데이터 계약과 루프

층을 다 쌓고 나서 "무엇이 엔진의 본질인가"를 다시 물으면, 답은 특정 층이 아니다. **탑을 관통하는 두 개의 축 — 데이터 계약(포맷/검증/소유권)과 루프(시간) — 이 본질이고, 각 층의 구현은 교체 가능하다.** Winters 자신이 그 증거다.

**증거 1 — 구현은 교체되는 중인데 계약은 살아남는다.** 렌더 백엔드는 DX11 concrete에서 RHI 인터페이스로 이관 중이지만(`IRHIDevice.h`), `.wmesh` 포맷과 `WintersFileHeader`의 magic/version 계약은 그대로다. LoL 클라이언트와 Elden 클라이언트는 씬/카메라/전투가 전혀 다르지만 같은 `.w*` 계약과 같은 `RenderWorldSnapshot` 입력 규약을 소비한다(compass: "필요한 것은 Elden식 RenderWorldSnapshot 작성"). 층의 안쪽(구현)은 갈아끼워도 층 사이의 표면(계약)은 유지된다 — 엔진의 자산은 코드 줄 수가 아니라 이 표면들이다.

**증거 2 — 포맷의 시간축이 실제로 시험됐다.** `MapDataFormats.h`의 스테이지 포맷은 v2→v5로 진화하면서 `STAGE_VERSION_MIN_COMPAT=3`으로 구파일을 계속 읽는다. `Hello.fbs`의 `dataBuildHash`는 "서로 다른 시점에 빌드된 클라와 서버가 만나도 되는가"를 접속 시점에 판정한다. 데이터 계약 설계의 핵심이 데이터 자체가 아니라 시간축이라는 것(`.md/interview/tool-development.md` §4)의 실물들이다.

**증거 3 — 소유권이 곧 아키텍처다.** Winters에서 가장 공들여 문서화·기계화된 것은 렌더 기법이 아니라 소유권 규칙이다: `DefinitionKey`/`EntityHandle`의 유효 범위, "ServerPrivate 수치는 Server에만 컴파일", "bot AI는 command 생산자", "Shared는 Engine을 include하지 않는다"(`Tools/Harness/Check-SharedBoundary.ps1`이 빌드 강제). 같은 판단이 UE에도 적용된다 — UE의 힘도 Slate나 Nanite 개별 기능이 아니라 "모든 자산은 카탈로그에 등록되고, 원본→엔진 포맷 변환은 재현 가능하고, 게임 수치는 데이터"라는 계약들의 집합이다(`WINTERS_UE_FAB_TOOL_ADOPTION.md` §1의 계약 단위 분해).

**증거 4 — 루프(시간)는 유일하게 협상 불가능한 축이다.** `DeterministicTime.h`의 `1/30`초는 상수 하나지만, 그 위에 서버 권위(L8), 스킬 타이밍의 tick/visual-frame 이원화(L5), SimLab 해시 검증(L4), Blueprint 비채택(§5 결정)까지가 전부 정렬되어 있다. 반대로 렌더러는 프레임레이트가 변해도, UI가 ImGui에서 무엇으로 바뀌어도 게임의 정체성은 유지된다. "시간을 어떻게 전진시키는가"에 대한 결정만은 탑 전체를 관통하므로 가장 먼저 고정해야 하고 가장 나중까지 살아남는다.

그래서 면접 한 줄 요약은 이렇다: **엔진을 만든다는 것은 렌더러를 만드는 일이 아니라, 시간의 전진 규칙과 데이터의 통행 규칙을 정하고 — 그 규칙이 사람의 선의가 아니라 포맷/Validator/빌드 게이트로 강제되게 만드는 일이다.** 렌더러·물리·UI는 그 규칙 위에 올라가는 교체 가능한 세입자다. UE/Unity로 무대를 옮겨도 이 관점은 그대로 이식된다 — UE의 uasset/DDC/쿠킹과 Unity의 GUID/meta/Library 캐시는 결국 같은 두 축의 다른 구현이기 때문이다.

## Canonical code pointers — concurrency/network

- Job/Fiber runtime: `Engine/Public/Core/JobSystem.h`, `Engine/Private/Core/JobSystem.cpp`, `Engine/Public/Core/JobSystem/WorkStealingDeque.h`, `Engine/Public/Core/Fiber/FiberPool.h`
- Job correctness gate: `Tools/Harness/JobSystemStress.cpp`, `Tools/Harness/RunJobSystemStress.ps1`
- TCP baseline: `Shared/Network/PacketEnvelope.h`, `Server/Private/Network/IOCPCore.cpp`, `Session.cpp`, `FrameParser.cpp`
- UDP v3: `Shared/Network/UdpPacketHeader.h`, `UdpFragmentHeader.h`, `PacketSemantics.h`, `UdpReliabilityChannel.*`, `UdpReassemblyBuffer.h`
- UDP endpoints/session boundary: `Server/Private/Network/UdpIocpCore.cpp`, `Server/Private/Network/ServerSessionHub.cpp`, `Client/Private/Network/Client/UdpClient.cpp`
- Authority path: `Server/Private/Game/GameRoomTick.cpp`, `CommandIngress.cpp`, `SnapshotBuilder.cpp`, `Shared/GameSim/Systems/*`
