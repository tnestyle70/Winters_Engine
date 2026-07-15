# 06. 씬 시스템 · 게임 루프 (Scene System & Game Loop)

> 면접 대본 겸 지식 베이스. 가상 함수/인터페이스 문법은 `.md/interview/cpp/06_polymorphism_virtual.md`,
> unique_ptr 소유권은 `.md/interview/cpp/04_pointers_smart_pointers.md`가 담당하고,
> 이 챕터는 "Winters의 프레임과 씬 수명을 누가, 어떤 순서로, 왜 그렇게 돌리는가"를 다룬다.
> 모든 인용은 repo 실측 기준 (경로는 repo-relative).

---

## ① 도메인 한 줄 정의

"Winters의 씬 시스템은 **Engine이 제품 무지(product-agnostic)의 `IScene` 계약과 이중 슬롯
`CScene_Manager`(영속 static + 교체형 current)를 제공하고, Client가 로그인→로비→밴픽→로딩→인게임→에디터라는
제품 씬 그래프를 소유하는 구조**입니다. 게임 루프는 `CEngineApp::Run`이 메시지 펌프→시간 갱신→Update→Render→입력
프레임 마감→프레임 리미터를 단일 메인 스레드에서 돌리고, 씬 전환은 'OnExit→리소스 정리→명시적 파괴→OnEnter'라는
결정적(deterministic) 순서로 처리합니다."

첫 문장 뒤에 바로 붙일 차별점: **씬 전환 실패를 조용히 삼키지 않고 E_FAIL로 fail-fast화한 에러 경계 리팩터링**과,
**584줄 헤더 하나를 유지한 채 구현을 총 9개 TU(책임별 분할 TU 7개 + 원본 Scene_InGame.cpp + 공유 헬퍼 Internal.cpp)로
기계 분할한 인게임 씬** — 이 두 가지가 이 도메인에서 내가 실제로 내린 의사결정이다.

---

## ② 구조와 데이터 흐름

### IScene 계약 — Engine이 아는 것은 6개 훅뿐

`Engine/Include/IScene.h` (전문 17줄):

```cpp
class WINTERS_ENGINE IScene abstract
{
public:
    virtual ~IScene() = default;

    virtual bool OnEnter() = 0;
    virtual void OnExit() = 0;
    virtual void OnUpdate(f32_t dt) = 0;
    virtual void OnLateUpdate(f32_t dt){}
    virtual void OnRender() = 0;
    virtual void OnImGui() {}
};
```

- `OnEnter`만 `bool`을 반환한다 — 부트스트랩은 실패할 수 있는 작업이고, 실패를 타입으로 표현했다.
- `OnLateUpdate`/`OnImGui`는 기본 구현이 빈 몸체 — 모든 씬이 구현할 필요가 없는 훅은 순수 가상으로 강제하지 않았다.
- Engine은 champion/lane 같은 제품 명사를 모른다(02장 계층 규칙). 구체 씬(로그인, 인게임, 에디터)은 전부 Client 소유.

### CScene_Manager — 이중 슬롯과 단일 소유

`Engine/Public/Scene/Scene_Manager.h`:

- `unique_ptr<IScene> m_pCurrentScene` — 교체되는 씬. `Change_Scene`으로만 바뀐다.
- `unique_ptr<IScene> m_pStaticScene` — "Player 계정 정보 UI 오버레이 영속!" 주석(line 20)이 달린 영속 슬롯.
  매 프레임 Update/LateUpdate/Render/ImGui가 **static→current 순서**로 호출돼 오버레이가 항상 함께 돈다.
- 정직하게 말하면: `Set_StaticScene`은 Engine API로 존재하지만 현재 Client 호출부는 0건이다(grep 실측).
  계정 오버레이용으로 **예약해 둔 확장점**이지 아직 쓰는 기능은 아니다 — 면접에서 물으면 이렇게 답한다.

`Change_Scene`의 결정적 전환 순서 (`Engine/Private/Scene/Scene_Manager.cpp:15-39`):

```cpp
if (m_pCurrentScene)
{
    m_pCurrentScene->OnExit();                              // 1. 이전 씬 정리
    CGameInstance::Get()->Clear_Resources(m_iCurrentSceneID); // 2. 이전 sceneID 리소스 정리
    Safe_Reset(m_pCurrentScene);                            // 3. 명시적 파괴
}
m_pCurrentScene = std::move(pScene);                        // 4. 소유권 이동
m_iCurrentSceneID = iNextSceneID;
if (m_pCurrentScene && !m_pCurrentScene->OnEnter())         // 5. 진입, 실패 시 fail-fast
{
    // "[Scene_Manager] Change_Scene OnEnter FAILED sceneID=%u" 로그 후
    return E_FAIL;
}
```

### Client 씬 그래프

`Client/Public/Defines.h:16`의 `eSceneID`: GameSelect / Login / MainMenu / CustomMode / BanPick / Shop /
MatchLoading / InGame / Editor / Result / SceneLoading.

실측 전환 호출부:
- 부팅: `Client/Private/GameApp.cpp:35` — `CGameApp::OnInit`이 Login 씬으로 시작.
- 매치 진입: `Client/Private/Scene/Scene_MatchLoading.cpp:91` — 로더 완료 + 최소 노출시간 경과 후 InGame으로.
- 인게임→에디터: `Client/Private/Scene/Scene_InGameImGui.cpp:42` — M 키로 `CScene_Editor` 생성 후 즉시 전환.
- 에디터→인게임: `Client/Private/Scene/Scene_Editor.cpp:986-1003` — dirty면 MessageBox로 저장 확인 후 복귀.
- 종료: `GameApp.cpp:44` — `Change_Scene(eSceneID::End, nullptr)`로 현재 씬만 정리하고 빈 상태로.

### 게임 루프 — 한 프레임의 전체 구조

`Engine/Private/Framework/CEngineApp.cpp:274` `Run()` 기준, 메인 스레드 단일 루프:

```text
┌─ frame start (FrameClock::now) ─────────────────────────────────────┐
│ Profiler_Begin / WINTERS_PROFILE_SCOPE("Frame")                     │
│ 1. m_Window.PumpMessages()        ← Win32 메시지 → CInput 상태 갱신 │
│ 2. 엔진 레벨 핫키: F3 프로파일러 토글, F4 JSON 캡처, F11 리미터 토글│
│ 3. Update_TimeDelta("Timer_Default") → deltaTime 확정               │
│ 4. Tick_Engine()                  ← 사운드 매니저 tick               │
│ 5. Update(dt)                                                       │
│     └ SceneManager.Update(dt) → LateUpdate(dt)   (static→current)   │
│ 6. Render()                                                         │
│     └ Device BeginFrame(clear)   ← WINTERS_CLEAR_RGB 환경변수 지원  │
│     └ ImGui.BeginFrame                                              │
│     └ SceneManager.Render()      ← 씬 3D 렌더                       │
│     └ SceneManager.ImGui()       ← 씬 ImGui (디버그/HUD 패널)       │
│     └ App::OnImGui / DebugRender / ProfilerOverlay                  │
│     └ ImGui.EndFrame → UI_Render_Cursor → Device EndFrame(Present)  │
│ Profiler_End / FRAME_MARK                                           │
│ 7. CInput::Get().EndFrame()       ← 에지(Pressed) 상태 프레임 마감   │
│ 8. SleepUntilFrameTarget(frameStart + targetFrameDuration)          │
└─────────────────────────────────────────────────────────────────────┘
```

순서에서 중요한 것 세 가지:

1. **입력은 프레임 맨 앞에서 확정, 맨 뒤에서 마감** — PumpMessages가 프레임 시작에 입력 상태를 갱신하고,
   `CInput::EndFrame()`(line 343)은 렌더까지 끝난 뒤 호출된다. 덕분에 Update/Render/ImGui 어디서 읽어도
   같은 프레임의 에지(IsKeyPressed) 상태가 일관된다.
2. **Update와 Render는 프로파일 스코프로 분리 계측** — "Update"/"Render" 스코프(line 318-323)가 프레임 예산
   분석의 1차 단위다. 최적화 주장은 이 JSON 캡처(F4)로 증명한다.
3. **프레임 리미터는 옵션** — `m_uTargetFPS > 0`일 때만 `targetFrameDuration`을 계산하고
   `CScopedFrameTimerResolution`으로 타이머 해상도를 올린 뒤 `SleepUntilFrameTarget`으로 맞춘다(line 276-281, 348-349).
   F11로 런타임에 캡을 풀어 무제한 측정 모드로 전환할 수 있다 — 벤치마크용으로 내가 의도한 토글이다.

또 하나, `Render()`는 `m_bDX11RuntimeEnabled` / `m_bSceneRuntimeEnabled` / `m_bImGuiRuntimeEnabled` 플래그로
3단 분기한다(line 403-499) — 풀 클라이언트, 씬만 있는 구성, GameApp 콜백만 있는 구성을 같은 루프가 소화한다.

### 씬 로딩 데이터 흐름 — CLoader의 두 가지 모드

`Client/Private/Scene/Loader.cpp:44-66`의 `CLoader::Create`가 목적지 씬에 따라 로드 전략을 가른다:

- **InGame행**: `PrepareMainThreadInGameLoad()` — FxDirectory/Model/Texture `LoadStep` 목록을 만들어 두고,
  로딩 씬의 매 `OnUpdate`에서 `TickMainThreadLoad()`가 **프레임당 1 스텝씩 메인 스레드에서** 소비한다
  (`Loader.cpp:82-116`). 진행률은 `atomic<f32_t>`로 노출되고 `Scene_Loading::OnImGui`가 ImGui ProgressBar로 그린다.
- **그 외(MainMenu/BanPick)**: `CJobSystem::Submit`으로 백그라운드 잡에 `RunLoadJob()`을 태운다.

왜 InGame만 메인 스레드인가 — 인게임 프리로드는 모델/텍스처의 **D3D 디바이스 리소스 생성**을 동반하는데,
Winters의 텍스처 경로는 immediate context 사용을 렌더/메인 스레드 계약으로 묶어 두었기 때문이다
(예: `Engine/Private/Resource/Texture.cpp`의 지연 밉 생성이 "Bind(렌더 스레드)에서만" 계약).
그래서 스레드로 빼는 대신 **스텝화(stepwise)로 잘라 프레임을 살리는** 쪽을 택했다.
전환 자체는 `Scene_Loading::OnUpdate`(`Client/Private/Scene/Scene_Loading.cpp:25-42`)가
`IsFinished()` 확인 후 `Build_NextScene()` → `Change_Scene` 호출로 완료한다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1. 씬 스택이 아니라 "교체형 current + 영속 static" 이중 슬롯

- **왜**: LoL 클라의 화면 흐름은 로그인→로비→인게임처럼 대부분 완전 교체이고, 유일하게 "겹쳐서 계속 떠야 하는 것"은
  계정/UI 오버레이 계열뿐이었다.
- **대안**: 일반화된 씬 스택(push/pop) 또는 레이어 리스트.
- **선택 이유**: 스택은 "지금 어떤 씬 조합이 살아 있는가"를 런타임 상태로 만들어 수명 추론을 어렵게 한다.
  슬롯 2개면 소유가 `unique_ptr` 2개로 고정되고, 호출 순서(static→current)도 코드에 박제된다
  (`Scene_Manager.cpp:41-71`). 씬 수명 버그의 탐색 공간 자체를 줄였다.
- **감수한 비용**: 오버레이가 2종 이상 필요해지면 구조 확장이 필요하다. 또 static 슬롯은 아직 실사용 0건 —
  "필요해 보여서 만든 확장점"이라는 비판을 받으면 인정하고, 대신 스택을 안 만든 절제를 방어 포인트로 쓴다.

### 결정 2. Change_Scene의 결정적 파괴 순서 + OnEnter fail-fast

- **왜**: 씬 전환은 "이전 씬의 리소스가 언제 죽는가"가 가장 사고 나기 쉬운 지점이다. 파괴 시점이
  스마트 포인터의 암묵적 재대입에 묻히면 디버깅이 안 된다.
- **대안**: `m_pCurrentScene = std::move(pScene)` 한 줄로 이전 씬을 암묵 파괴 / OnEnter 실패 무시(과거 방식).
- **선택 이유**: OnExit→`Clear_Resources(이전 sceneID)`→`Safe_Reset`을 **명시적 3단계**로 쪼개 각 단계에
  브레이크포인트를 걸 수 있게 했다. 그리고 2026-07-09 에러 경계 리팩터링에서 OnEnter 반환값을 무시하던
  기존 코드를 sceneID 로그 + `E_FAIL` 반환으로 바꿨다(`Scene_Manager.cpp:30-36`).
  근거 문서 `.md/plan/2026-07-09_ERROR_BOUNDARY_REFACTORING.md:32` — "실패한 씬이 이전 씬 파괴 후
  반쯤 초기화된 채 계속 구동됨"이 기존 증상이었다. 같은 파일의 `Set_StaticScene`은 이미 검사하고 있었는데
  `Change_Scene`만 삼키고 있던 **비대칭**을 감사에서 잡아낸 것.
- **감수한 비용**: E_FAIL을 반환해도 아직 "실패 씬을 뭘로 대체하는가"(폴백 씬 정책)는 미구현이다 —
  현재는 실패가 가시화될 뿐, 복구는 다음 슬라이스로 명시해 뒀다. 미완이라고 정직하게 말한다.

### 결정 3. 인게임 씬을 상속 분해가 아닌 "한 헤더 + 책임별 TU"로 기계 분할

- **왜**: `CScene_InGame`이 수천 줄 god-class가 됐다. 네트워크 적용, 로컬 예측, 렌더, 입력, 부트스트랩이
  한 파일에 섞여 리뷰와 병렬 작업이 불가능해졌다.
- **대안**: (a) 기능별 서브클래스/컴포지션으로 클래스 자체를 쪼개기, (b) 파일만 쪼개기.
- **선택 이유**: (a)는 시그니처와 호출 순서가 바뀌는 **행동 변경 리스크**를 동반한다. 나는 Stage 1로
  "동작/시그니처/호출순서 불변"의 verbatim 이동만 먼저 했다. 현재 실측:
  `Client/Public/Scene/Scene_InGame.h` 584줄 헤더 하나에, 구현이
  Lifecycle(1012줄)/Network(1194줄)/LocalSkills(2486줄)/Render(690줄)/Input(791줄)/ImGui(210줄)/MapNav(608줄)/
  원본 Scene_InGame.cpp(1217줄, OnUpdate 오케스트레이션 잔류)/공유 헬퍼 Scene_InGameInternal.cpp(44줄)로 나뉜다
  — 분할 TU 7개 + 원본 + Internal.cpp = 총 9개 TU. 각 TU 상단에
  "Stage 1 (mechanical split) ... 동작/시그니처/호출순서 불변"과 설계 문서
  (`.md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md`) 링크를 박았다.
  특히 `Scene_InGameNetwork.cpp:3`과 `Scene_InGameLocalSkills.cpp:3`에 **"local-only prediction과
  snapshot-apply는 절대 같은 파일에 두지 않는다"**는 규칙을 주석으로 강제했다 — 예측 코드가 서버 truth 적용
  코드에 스며드는 것을 파일 경계로 막는 장치다.
- **감수한 비용**: 헤더는 여전히 584줄 한 덩어리라 컴파일 의존이 크고, 클래스 응집도 문제 자체는 남아 있다.
  2개 이상 TU가 쓰는 헬퍼는 `Scene_InGameInternal.h`(22줄) + `Scene_InGameInternal.cpp`(44줄)로 단일화해
  중복 정의 드리프트만 먼저 잡았다.
  진짜 구조 개선(Installer 패턴)은 Stage 2로 미뤄 뒀다 — 리팩터링을 "행동 보존 단계"와 "구조 변경 단계"로
  나눈 것이 이 결정의 핵심이다.

### 결정 4. 씬 수명주기에서 실행 모드를 확정 — OnEnter의 시스템 등록 게이팅

- **왜**: 같은 인게임 씬이 로컬 시뮬 모드와 서버 권위(network-authoritative) 모드를 다 소화해야 한다.
- **대안**: 시스템은 다 등록해 두고 각 시스템 내부에서 매 tick마다 모드를 검사.
- **선택 이유**: `Scene_InGameLifecycle.cpp:377-395`에서 OnEnter 첫머리에
  `m_bNetworkAuthoritativeGameplay = m_bUsingSharedNetwork || m_bReplayPlaybackMode`로 모드를 확정하고,
  이후 Navigation/LocalUnitAI/NavigationThrottle/TurretAI/StructureProjectile/BehaviorTree/MCTS 등록을
  전부 `if (!m_bNetworkAuthoritativeGameplay)`로 건너뛴다(line 529-581). Transform/SpatialHash/StatusEffect/
  Vision/YoneSoul만 항상 등록. 시스템이 **등록조차 안 되면** 매 프레임 분기 비용도, "실수로 돌아버리는"
  사고도 원천 차단된다. 실제로 클라 `CNavigationSystem`이 스냅샷이 적용한 챔피언 yaw를 덮어써 이동이
  계단식으로 튀던 사고(gotcha 2026-05-22)의 구조적 해법이 이 게이팅이었다.
- **감수한 비용**: 모드가 씬 진입 시점에 고정된다 — 인게임 도중 로컬↔네트워크 전환은 불가능하다.
  우리 제품 흐름(밴픽에서 모드 확정)에서는 문제가 아니라고 판단했다.

### 결정 5. 에디터를 인게임 위 패널이 아닌 별도 씬으로

- **왜**: 맵 오브젝트 배치/NavGrid 편집은 인게임과 카메라·입력·월드 수명이 전부 다르다.
- **대안**: 인게임 씬에 에디터 모드 플래그를 두고 ImGui 패널로 편집.
- **선택 이유**: `Client/Public/Scene/Scene_Editor.h:20-28` 주석대로 CScene_Editor는 "맵 오브젝트 배치 전용 씬" —
  탑다운 FreeCam, Palette/Hierarchy/Inspector, Save/Load/Stage 전환을 갖고 자기만의 `CWorld`를 소유한다.
  대신 **데이터 소스는 공유한다**: Structure/Jungle/Minion/Bush 싱글턴 매니저와 `CMapDataIO`의 Stage .dat를
  인게임과 동일하게 쓰기 때문에, 에디터에서 저장하면 인게임 씬이 OnEnter의 `Load_Stage`에서 그대로 읽는다.
  전환도 씬 전환일 뿐이다: 인게임에서 M 키(`Scene_InGameImGui.cpp:38-44`) ↔ 에디터 Esc/Back
  (`Scene_Editor.cpp:986-1003`, dirty면 저장 여부 MessageBox). 인게임 모드 플래그 방식이었다면
  "에디터 중에 미니언 AI가 도는가?" 같은 상태 조합 문제를 계속 껴안았을 것이다.
- **감수한 비용**: 에디터↔인게임 왕복마다 인게임 부트스트랩(맵 로드, ECS 재조립) 전체 비용을 다시 낸다.
  편집 반복 루프의 속도가 씬 전환 비용에 묶인다 — 도구로서는 감수, 대신 상태 격리를 얻었다.

### 결정 6. 인게임 OnUpdate의 프레임 오케스트레이션 순서를 명시적으로 고정

`Client/Private/Scene/Scene_InGame.cpp:739`의 OnUpdate가 네트워크 권위 모드에서 지키는 순서(실측):

```text
(1) CaptureNetworkActorInterpolationStarts   ← 스냅샷 "적용 전" 위치 캡처
(2) PumpNetwork / UpdateReplayPlayback       ← 스냅샷·이벤트 적용 (콜백)
(3) 새 serverTick이면 BeginNetworkActorInterpolationForSnapshot
(4) SyncPlayerEntityTransformFromECS
(5) VisionSystem FOW 로컬팀 세팅
(6) Scheduler.Execute(m_World, dt)           ← 등록된 ECS 시스템 실행
(7) FOW 텍스처 dirty면 업로드
(8) ApplyNetworkActorInterpolation(dt)
(9) SyncPlayerEntityTransformFromECS 재호출
(10) UpdateNetworkChampionLocomotion         ← 애니 상태 합성
(11) ProjectGameplayActorsToMapSurface
(12) UpdateTargeting / UpdateCombatInput     ← 입력·전투 (사망 시 입력 락)
```

- **왜 이 순서인가**: (1)이 (2)보다 앞서는 것이 핵심이다. 보간의 start는 "스냅샷이 덮어쓰기 직전의 위치"여야
  하는데, 순서를 바꾸면 start==target이 되어 보간이 무효화되고 30Hz 스냅샷 그대로 뚝뚝 끊긴다.
- **대안**: 각 하위 시스템이 알아서 이전 위치를 기억하게 하기.
- **선택 이유**: 순서 의존을 한 함수(OnUpdate)에 펼쳐 놓으면 "프레임에서 무엇이 먼저인가"라는 질문의 답이
  코드 한 곳에 있다. 분산시키면 각 시스템의 캐시 타이밍 버그로 변한다.
- **감수한 비용**: OnUpdate가 길다(오케스트레이터 역할). Stage 2에서 페이즈 객체로 추출할 후보다.

---

## ④ 어려웠던 점과 해결

### 1. 씬 전환 도중 자기 자신이 파괴되는 문제 (self-destruction during transition)

씬이 **자기 OnUpdate 안에서** `Change_Scene`을 부르면, `Change_Scene`의 `Safe_Reset(m_pCurrentScene)`이
지금 멤버 함수를 실행 중인 바로 그 객체를 파괴한다. `this`가 죽은 뒤 멤버에 접근하면 UB다.
`Client/Private/Scene/Scene_Loading.cpp:37-41`에 이 계약을 박제했다:

```cpp
CGameInstance::Get()->Change_Scene(
    static_cast<uint32_t>(m_pLoader->Get_NextSceneID()),
    std::move(pNext));
// 이 지점 이후 this 는 파괴될 수 있음 — 멤버 접근 금지
```

해결 방식은 "전환 요청을 큐잉해 프레임 끝에 처리"하는 지연 전환이 정석이지만, 나는 MVP에서
**호출 후 즉시 return + 주석 계약**으로 규율화했고, `m_bTransitioned` 플래그로 재진입도 막았다.
지연 전환 큐는 개선 후보로 인지하고 있다(⑤ 참고). 면접에서는 "UB 가능성을 어디서 인지했고
어떤 규율로 막았는가"를 말할 수 있는 것이 핵심이다.

### 2. OnEnter 실패가 조용히 삼켜지던 기간

전수 감사 전까지 `Change_Scene`은 OnEnter의 bool을 버리고 무조건 S_OK를 반환했다. 증상은 "씬이 반쯤
초기화된 채 돌아가는데 어디서 실패했는지 모름". `Set_StaticScene`은 검사하는데 `Change_Scene`은 안 하는
비대칭을 P1(실패 즉시 가시화) 감사에서 발견했고, sceneID를 담은 로그 + E_FAIL로 고쳤다.
교훈: **같은 파일 안의 두 API가 다른 에러 정책을 갖고 있으면 그 자체가 버그 신호**다.

### 3. 씬 종료 시 RHI 핸들 누수 — 명시 파괴 규칙

`OnExit`(`Scene_InGameLifecycle.cpp:942-1012`)는 UI 상태 clear→네트워크 콜백/객체 reset→RHI 렌더러·맵
shutdown→FX/매니저 정리→보간·애니 맵 clear 순으로 OnEnter의 역순 정리를 하는데, 마지막에
`pDevice->DestroyTexture(m_hRHIAttackRangeTex)`를 **직접** 호출한다(line 999-1004).
RHI 핸들은 RAII 래퍼가 아니라 정수형 핸들이라 GC도 소멸자도 없다 — 씬이 만든 RHI 리소스는 씬이
명시적으로 죽인다는 규칙을 OnExit에 뒀다. unique_ptr 멤버(레거시 경로)와 수동 파괴(RHI 경로)가 공존하는
과도기의 수명 관리다.

### 4. 로딩을 어디 스레드에 둘 것인가

MainMenu/BanPick 프리로드는 JobSystem 백그라운드로 돌리지만, InGame 프리로드는 D3D 리소스 생성이 얽혀
메인 스레드 스텝화로 풀었다(② 참고). 부작용으로 `CLoader` 소멸자는 잡 미완료 시 대기해야 하는데, 잡시스템이
없을 때의 폴백이 10ms 스핀 대기다(`Loader.cpp:68-80`) — `Scene_Loading.cpp:21` 주석에 "임시: 스핀 대기"로
부채임을 표시해 뒀다. "비동기 로딩 완성했습니다"가 아니라 "디바이스 계약 때문에 이렇게 갈랐고 여기가
임시입니다"라고 말하는 것이 정직한 답변이다.

### 5. 씬별 리소스 언로드의 미완

`Change_Scene`이 부르는 `Clear_Resources`(`Engine/Private/GameInstance.cpp:202-208`)는 현재
BlueprintRegistry의 씬 스코프만 정리한다. 주석에 "추후 ResourceCache::Unload_Scene 연계 예정"이라고
박아 둔 대로, 모델/텍스처 캐시는 씬 단위 언로드가 없고 앱 종료 시 일괄 Clear에 의존한다.
LoL처럼 인게임 한 판이 세션의 대부분인 제품에서는 치명적이지 않다고 판단해 우선순위를 미뤘지만,
씬을 오래 오가면 캐시가 단조 증가하는 알려진 부채다.

---

## ⑤ 향후 개선 방향

1. **씬 전환 지연 큐**: OnUpdate 중 즉시 파괴 대신 "전환 요청 → 프레임 끝 처리"로 바꿔
   self-destruction 계약(주석 규율)을 구조로 대체.
2. **OnEnter 실패 폴백 정책**: 지금은 E_FAIL 가시화까지. 실패 씬을 에러 씬/이전 씬으로 대체하는
   복구 정책이 에러 경계 리팩터링 문서에 "다음 슬라이스"로 명시돼 있다.
3. **ResourceCache::Unload_Scene 연계**: `Clear_Resources`가 Blueprint 외에 씬 스코프 모델/텍스처까지
   내리도록 — 리소스 챕터의 CName 해싱 계획과 함께 진행.
4. **인게임 씬 Stage 2 (Installer 패턴)**: Lifecycle TU 주석에 예고된 대로, OnEnter 부트스트랩을
   선언적 Installer 목록으로 바꿔 등록 순서를 데이터화.
5. **Loader 스핀 대기 제거**: JobCounter 기반 대기로 일원화.
6. **static scene 슬롯 실사용**: 계정/알림 오버레이를 올리거나, 끝까지 안 쓰면 제거 — 죽은 확장점을
   방치하지 않는 것도 결정이다.

---

## ⑥ 면접 Q&A

**Q1. 씬 전환 시 리소스는 어떻게 처리하나요?**
- 골격: 순서가 계약이다 — OnExit(씬 자체 정리) → Clear_Resources(이전 sceneID 스코프) → Safe_Reset(명시 파괴)
  → 새 씬 move → OnEnter. 씬은 unique_ptr 단일 소유라 파괴 시점이 Change_Scene 한 곳에 고정된다.
  인게임 OnExit는 OnEnter의 역순 정리 + RHI 핸들 명시 DestroyTexture까지 수행.
- 꼬리질문 대비: "Clear_Resources가 실제로 다 내리나요?" → 정직하게: 현재 Blueprint 씬 스코프만.
  ResourceCache 씬 언로드는 주석으로 예약된 부채고, 세션 구조상(인게임 1판 중심) 우선순위를 미뤘다.

**Q2. 게임 루프에서 입력/업데이트/렌더 순서를 설명해 주세요.**
- 골격: PumpMessages(입력 확정) → deltaTime 갱신 → Update(씬 Update→LateUpdate) → Render(BeginFrame→씬
  Render→씬 ImGui→오버레이→Present) → CInput::EndFrame(에지 마감) → 프레임 리미터. 입력 확정이 맨 앞,
  에지 마감이 맨 뒤라 한 프레임 안에서는 어디서 읽어도 입력 뷰가 일관된다.
- 꼬리질문: "가변 timestep인데 물리는요?" → 클라는 프레젠테이션이라 가변 dt로 충분하고, gameplay truth는
  서버 30Hz 고정 tick이 소유한다(네트워크 챕터로 연결). 클라에서 고정 스텝이 필요한 시뮬을 안 돌리는 것이
  구조적 답이다.

**Q3. 씬이 자기 Update 중에 Change_Scene을 부르면 무슨 일이 생기나요?**
- 골격: Change_Scene의 Safe_Reset이 호출자 자신을 파괴한다 — 이후 멤버 접근은 UB. Scene_Loading에
  "이 지점 이후 this 파괴 가능, 멤버 접근 금지" 주석 계약 + 즉시 return + 재진입 플래그로 규율화했다.
- 꼬리질문: "더 안전한 방법은?" → 전환 요청 큐잉 후 프레임 끝 처리. 개선 후보로 인지하고 있고, MVP에서는
  호출부가 2~3곳으로 통제 가능해 규율을 택했다고 답한다.

**Q4. 로딩 씬은 어떻게 구현했나요? 비동기인가요?**
- 골격: 목적지에 따라 이원화 — 메뉴/밴픽 프리로드는 JobSystem 백그라운드, 인게임 프리로드는 D3D 리소스
  생성이 메인 스레드 계약이라 LoadStep 목록을 프레임당 1개씩 소비하는 메인 스레드 스텝화. 진행률은 atomic으로
  노출해 ImGui ProgressBar로 표시.
- 꼬리질문: "D3D11은 리소스 생성이 free-threaded 아닌가요?" → 디바이스 생성 자체는 스레드 세이프지만
  Winters는 immediate context를 쓰는 지연 밉 생성 등 컨텍스트 종속 작업이 로드 경로에 얽혀 있어, 계약을
  단순하게 유지하려고 메인 스레드로 묶었다. 컨텍스트 작업 분리 + 로더 스레드 승격이 다음 단계.

**Q5. 인게임 씬이 거대해졌을 때 어떻게 관리했나요?**
- 골격: Stage 1은 기계 분할 — 한 헤더(584줄)를 유지한 채 구현을 Lifecycle/Network/LocalSkills/Render/Input/
  ImGui/MapNav 7개 TU로 verbatim 이동(원본 Scene_InGame.cpp + 공유 헬퍼 Internal.cpp 포함 총 9개 TU),
  "동작/시그니처/호출순서 불변"을 TU 주석으로 보장해 리뷰 리스크를 0에
  가깝게. 공유 헬퍼는 Internal.h/.cpp로 단일화. 예측과 스냅샷 적용은 절대 같은 파일에 두지 않는다는 규칙을 명문화.
- 꼬리질문: "그건 파일만 쪼갠 것 아닌가요?" → 맞다, 의도적으로. 행동 보존 단계와 구조 변경 단계(Installer,
  Stage 2)를 분리한 것이며, 파일 경계 자체도 '예측/권위 분리' 같은 아키텍처 규칙의 집행 도구로 쓰고 있다.

**Q6. 씬 OnEnter가 실패하면 어떻게 되나요?**
- 골격: 과거에는 반환값이 무시돼 반쯤 초기화된 씬이 계속 돌았다. 에러 경계 감사에서 같은 파일의
  Set_StaticScene과의 정책 비대칭을 발견, sceneID 로그 + E_FAIL 반환으로 fail-fast화했다.
- 꼬리질문: "실패하면 유저는 뭘 보나요?" → 현재는 빈 씬 상태 + 로그. 폴백 씬 대체가 다음 슬라이스라고
  문서에 명시했다 — 가시화와 복구를 단계로 나눈 것을 설명한다.

**Q7. 에디터를 왜 별도 씬으로 만들었나요?**
- 골격: 카메라/입력/월드 수명이 인게임과 전부 달라서. 별도 씬이면 상태 조합(에디터 중 AI가 도는가 등)을
  고민할 필요가 없다. 대신 Structure/Jungle/Minion/Bush 매니저와 Stage .dat 포맷을 공유해 "에디터 저장 →
  인게임 OnEnter 로드"로 데이터가 이어진다. M 키/Esc로 씬 전환하며, dirty면 저장 확인.
- 꼬리질문: "왕복 비용은?" → 인게임 부트스트랩 전체를 다시 낸다. 도구 반복 속도보다 상태 격리를 우선한
  트레이드오프고, 에디터 코어를 독립시키는 방향(Elden 에디터 계획)과도 이어진다.

**Q8. 서버 권위 모드와 로컬 모드를 같은 씬이 어떻게 소화하나요?**
- 골격: OnEnter 첫머리에 모드를 확정하고, 시뮬레이션 계열 시스템(Navigation/AI/Turret/BT/MCTS)의 **등록
  자체를** 게이팅한다. 등록 안 된 시스템은 돌 수 없으므로 매 tick 분기도, 클라가 truth를 덮어쓰는 사고도
  구조적으로 차단된다. 계기는 클라 내비게이션이 스냅샷 yaw를 덮어쓰던 실제 버그.
- 꼬리질문: "런타임 모드 전환은?" → 불가능하고, 제품 흐름상 필요 없다고 판단해 씬 진입 시 고정했다.

**Q9. 프레임 레이트 제한은 어떻게 구현했나요?**
- 골격: targetFPS 설정 시 목표 프레임 duration을 계산, 스코프 기반으로 타이머 해상도를 올리고
  SleepUntilFrameTarget으로 다음 프레임 시각까지 대기. F11로 런타임 토글해 무제한 측정 모드 지원.
  프레임 자체는 Profiler Begin/End + FRAME_MARK로 감싸 F4로 JSON 캡처.
- 꼬리질문: "Sleep 정밀도는?" → 타이머 해상도 스코프(CScopedFrameTimerResolution)로 완화하지만 OS Sleep의
  한계는 있다. 마지막 1~2ms 스핀 보정은 개선 여지로 인지.

**Q10. Update와 LateUpdate를 왜 나눴나요?**
- 골격: 계약상 "모든 씬 Update가 끝난 뒤 실행되는 후처리 훅"(카메라 추적 등 순서 의존 작업용)이고
  SceneManager가 Update 직후 LateUpdate를 호출한다. 다만 정직하게 — 현재 인게임 씬의 OnLateUpdate는
  빈 몸체이고, 순서 의존 후처리는 OnUpdate 내부 오케스트레이션(보간 캡처→적용 순서)으로 풀고 있다.
  인터페이스 훅과 실사용의 간극을 인지하고 있다고 말하는 편이 오히려 신뢰를 준다.

---

## ⑦ 다른 챕터와의 연결

- **02_architecture_layers**: IScene/CScene_Manager는 Engine(제품 무지), 구체 씬 그래프는 Client 소유 —
  "Engine은 champion을 모른다" 규칙이 씬 시스템에서 어떻게 지켜지는지의 실례. 씬 렌더가 DX11 legacy와
  RHI RenderWorldSnapshot을 병행하는 것(`Scene_InGameRender.cpp:328` OnRender의 backend 분기)도
  레이어 챕터의 RHI 이관 전략과 같은 축이다.
- **cpp/03_memory_lifetime_raii · cpp/04_pointers_smart_pointers**: 씬 unique_ptr 단일 소유,
  Safe_Reset 명시 파괴, RHI 핸들의 비-RAII 수동 파괴 — 수명 관리 문법의 도메인 적용 사례.
- **cpp/06_polymorphism_virtual**: IScene abstract 인터페이스, DLL 경계(WINTERS_ENGINE)를 넘는 가상 호출.
- **cpp/09_concurrency**: CLoader의 JobSystem 백그라운드 로드 vs 메인 스레드 스텝화, atomic 진행률,
  소멸자 대기 — 동시성 챕터의 실전 지점.
- **cpp/10_error_handling**: Change_Scene fail-fast화는 P1(실패 즉시 가시화) 에러 정책의 씬 도메인 적용.
- **리소스/에셋 챕터**: Clear_Resources ↔ ResourceCache::Unload_Scene 미연계 부채, 로딩 씬의 프리로드가
  채우는 캐시의 수명 정책.
- **클라 네트워크 챕터**: OnUpdate 오케스트레이션의 (1)~(3) 단계(보간 캡처→스냅샷 적용→보간 시작)와
  서버 권위 시스템 게이팅은 스냅샷 적용/보간/예측 파이프라인의 프레임 배치 문제다.
