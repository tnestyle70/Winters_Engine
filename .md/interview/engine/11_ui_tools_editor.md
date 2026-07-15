# 11. UI · 툴 · 에디터

> 면접 대본 겸 지식 베이스. 코드 문법이 아니라 **구조·설계 결정·문제 해결**을 설명한다.
> 모든 인용은 실제 코드를 열어 검증했다. 경로는 repo-relative.

---

## ① 도메인 한 줄 정의

"Winters의 UI/툴 레이어는 **ImGui를 RHI 뒤로 감싼 즉시모드 기반** 위에, 런타임 HUD(체력바/커서/미니맵), 맵 에디터(Stage 바이너리 왕복), FX/스킬 튜닝 툴, 그리고 '증상을 튜닝하기 전에 관측부터 만든다'는 원칙을 코드로 강제한 디버그 오버레이 군을 올린 계층입니다. UI 패널은 렌더만 하고, truth 판정은 전부 Client 브릿지가 view data로 만들어 넘깁니다."

---

## ② 구조와 데이터 흐름

### 레이어 구성

```text
[Win32 WndProc]
   │ ImGui_ImplWin32_WndProcHandler 가 먼저 소비 (소비 시 early return)
   │ 남은 입력만 CInput 으로                    ← Engine/Private/Platform/CWin32Window.cpp:166
   ▼
[CEngineApp::Render — 한 프레임의 UI 오케스트레이션]
   Device::BeginFrame → ImGui::BeginFrame → Scene::Render → Scene::ImGui
   → App::ImGui → DebugRender → ProfilerOverlay → ImGui::EndFrame
   → UI_Render_Cursor → Device::EndFrame       ← Engine/Private/Framework/CEngineApp.cpp:373-504
   ▼
[CImGuiLayer — RHI 추상화 뒤의 ImGui]
   IRHIDevice::GetNativeHandle(DX11Device/Context | DX12Device/Queue/CommandList)
   backend 별 NewFrame/RenderDrawData 분기      ← Engine/Private/Editor/ImGuiLayer.cpp
   ▼
[소비자 계층]
   런타임 HUD: CUI_Manager(Engine) ← Client 브릿지가 view data 푸시
   디버그 오버레이: DebugDrawSystem (배경 드로우리스트에 3D 투영)
   튜닝 툴: EffectTuner / WfxEffectTool / SkillTimingPanel / AIDebugPanel
   에디터: Scene_Editor (Stage.dat / .navgrid 왕복)
```

### 프레임 오케스트레이션과 런타임 모드 게이팅

`CEngineApp::Render`는 세 가지 런타임 모드로 UI 경로를 게이팅한다(Engine/Private/Framework/CEngineApp.cpp:403-499):

- `m_bDX11RuntimeEnabled` — 풀 경로: ImGui BeginFrame → Scene Render/ImGui → App ImGui → DebugRender → Profiler 오버레이(F3 토글, F4는 JSON 캡처 후 열기) → ImGui EndFrame → UI 커서.
- `m_bSceneRuntimeEnabled` + `m_bImGuiRuntimeEnabled` — DX12 부분지원 경로: ImGui 구간만 플래그로 켜고 끈다.
- 둘 다 아니면 GameApp-only — 헤드리스/서버성 실행에서 UI가 통째로 빠져도 프레임 루프는 동일하게 돈다.

각 구간이 `WINTERS_PROFILE_SCOPE`로 계측되어 있어서 "UI가 프레임을 얼마나 먹는가"가 항상 프로파일러에 잡힌다. UI를 if 하나로 끄고 켤 수 있는 이 구조가 나중에 서버/스모크 실행 경로의 기반이 됐다.

### 핵심 데이터 흐름 — "UI 패널은 렌더만 한다"

컴퍼스 규칙(`.md/architecture/WINTERS_CODEBASE_COMPASS.md`)에 "Engine UI panel은 CWorld/Shared/GameSim을 직접 조회하지 않는다. 필요한 데이터는 view state로 받는다"를 박아두고, 실제로 그렇게 흐른다.

- 체력바: `CScene_InGame::SyncWorldHealthBarsToEngineUI`(Client/Private/Scene/Scene_InGame.cpp:597)가 ECS 월드를 순회해 챔피언/미니언/포탑을 `UIWorldHealthBarDesc`(종류·위치·HP·마나·팀·생사) 배열로 만들고, 로컬 팀 id와 함께 `UI_Set_WorldHealthBars`로 엔진에 넘긴다. 엔진 `CUI_Manager`는 이 desc만 보고 그린다 — HP가 진짜인지, 어느 팀인지의 판정은 UI가 하지 않는다.
- 미니맵: `BuildMinimapFrameState`가 월드 상태를 `MinimapFrameState`(FOW SRV, 아이콘 뷰 배열, 투영 파라미터)로 스냅샷하고 `CMinimapPanel::RenderRuntime`이 렌더만 한다(Client/Public/UI/MinimapPanel.h:82-96). 좌표 변환은 3점 보정 `MinimapProjection`(vWorldAtUv00/10/01)으로 world↔UV 아핀 매핑, 클릭 이동은 `TryResolveMinimapClickWorldPos`.
- HUD 에셋: 커서 3종(default/enemy/attack), 팀별 유닛/구조물 HP바 텍스처, 핑 휠, 어빌리티 아틀라스가 `CUI_Manager` 상단에 경로 상수로 모여 있고, HUD 배치는 `hud_atlas_manifest.json` + `actor_hud_layout.json` 매니페스트로 데이터화되어 있다(Engine/Private/Manager/UI/UI_Manager.cpp:39-64). HUD 폰트도 LoL풍 전용 폰트(beaufort/spiegel) + 한글 폴백(NotoSansCJK)을 경로 상수로 관리한다.
- 체력바 디테일: 캐릭터 체력바는 엔티티별 trail 상태(`m_CharacterHealthBarTrails`, 0.04초 hold 후 수축)를 유지해 피해량이 잠깐 잔상으로 남는 LoL식 연출을 낸다(Engine/Public/Manager/UI/UI_Manager.h:427-437).
- 렌더 경로 이원화: `Render_Cursor`는 RHI UI 렌더러가 준비되어 있으면 그쪽으로, 아니면 ImGui foreground 드로우리스트로 폴백한다(UI_Manager.cpp:1415-1435). 체력바도 `DrawHealthBars`/`DrawHealthBarsRHI` 쌍으로 존재 — DX12 이관기의 병행 구조가 UI까지 관철된다.
- 이미지 주도 씬(밴픽/로딩): `CImageScenePresenter`가 배경 텍스처 + 소스 이미지 서브렉트 렌더와 `ScreenToSource`/`WasSourceRectClicked` 좌표 매핑·클릭 판정을 재사용 가능하게 묶는다. 이것도 RHI 프레임/ImGui 드로우리스트 두 경로를 지원하고, 텍스처는 `unique_ptr` 소유 + 복사 금지(Client/Public/UI/ImageScenePresenter.h:25-64).

### 디버그 패널 조직 — F키 토글 체계

패널이 늘어나면서 토글을 한 곳(`CScene_InGame::OnImGui`, Client/Private/Scene/Scene_InGameImGui.cpp:34-105)에 모았다:

| 키 | 패널 |
|---|---|
| F1 | 렌더 디버그 마스터 (켜면 colliders+champions 자동 on) |
| F6 | 리플레이 컨트롤 |
| F7 | WFX 이펙트 툴 |
| F8 | UI 튜너 |
| F9 | AI 디버그 패널 |
| F10 | legacy 디버그 번들 (Combat/MapTuner/ChampionTuner/EffectTuner/SkillTiming/NetworkTrace) |
| M | 맵 에디터 씬 전환 |

자주 쓰는 패널은 개별 F키, 은퇴 수순인 패널은 F10 번들 뒤로 격리해 기본 화면을 오염시키지 않는다. 각 패널 렌더는 `WINTERS_PROFILE_SCOPE("UI::...")`로 계측된다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1. ImGui를 RHI 인터페이스 뒤로 감싼다

- **왜**: 엔진이 DX11→DX12 병행 이관 중이라, UI 레이어가 구체 디바이스를 알면 백엔드마다 UI 코드가 갈라진다.
- **대안**: (a) ImGui 초기화에 DX11 디바이스를 직접 주입, (b) 백엔드별 ImGuiLayer 파생 클래스.
- **선택**: `CImGuiLayer`는 `IRHIDevice::GetNativeHandle(eNativeHandleType::...)`로 네이티브 핸들만 뽑아 `imgui_impl_dx11/dx12`를 초기화하고, BeginFrame/EndFrame에서 backend를 분기한다. DX12 EndFrame은 `CDX12Device::BeginFrame`이 열어 둔 프레임 커맨드리스트에 직접 기록한다(Engine/Private/Editor/ImGuiLayer.cpp:284-318).
- **감수한 비용**: GetNativeHandle은 사실상 추상화의 탈출구(escape hatch)다. UI 레이어 한 곳에만 허용하고, 나머지 렌더 코드는 RHI 타입만 쓰도록 경계를 유지해야 한다.

이 과정에서 실무 이슈도 하나 겪었다: imgui 1.92+부터 DX12 백엔드가 SRV 디스크립터 할당을 앱 콜백에 위임한다. 그래서 SHADER_VISIBLE CBV_SRV_UAV 힙(capacity 64) 위에 free-index 스택으로 Alloc/Free 하는 `ImGuiDX12SrvHeapAllocator`를 직접 만들어 `SrvDescriptorAllocFn/FreeFn` 람다로 넘겼고, 초기화가 중간에 실패하면 힙 Destroy → Win32 Shutdown → DestroyContext 순으로 되감는 방어적 teardown을 넣었다(ImGuiLayer.cpp:60-115, 237-275).

### 결정 2. 게임 입력과 UI 입력 충돌은 이중 게이팅으로 푼다

- **왜**: 패널 위 클릭이 월드 배치/이동 명령으로 새면 에디터와 디버그 패널이 실사용 불가가 된다.
- **선택**: 1차로 WndProc에서 `ImGui_ImplWin32_WndProcHandler`가 소비하면 early return(CWin32Window.cpp:166-168), 2차로 씬 로직이 배치 전에 `ImGui::GetIO().WantCaptureMouse`를 검사한다(Scene_Editor.cpp:738). 커서는 `NoMouseCursorChange` 플래그로 ImGui에 뺏기지 않고 자체 렌더한다(ImGuiLayer.cpp:202).
- **감수한 비용**: 게이트가 두 곳이라 "왜 클릭이 안 먹지"를 디버깅할 때 두 층을 다 봐야 한다. 대신 어느 한쪽이 빠져도 입력 누수가 안 난다.

### 결정 3. 3D 디버그 오버레이는 전용 라인 렌더러 대신 ImGui 배경 드로우리스트

- **왜**: 와이어박스/실린더/월드 라인/텍스트 라벨이 필요했지만, 디버그용으로 별도 라인 렌더 파이프라인(버텍스 버퍼, 셰이더, 드로우콜 관리)을 만드는 건 과투자였다.
- **선택**: `WorldToScreen`(VP 변환, w≤0.01 클립, NDC→픽셀)으로 직접 투영해 `ImGui::GetBackgroundDrawList()`에 2D 프리미티브로 그린다(Client/Private/UI/DebugDrawSystem.cpp:31-103).
- **감수한 비용**: 반드시 ImGui BeginFrame/EndFrame 사이에서만 호출 가능하고, 깊이 테스트가 없어 항상 위에 그려진다. 디버그 용도로는 오히려 장점이라 수용했다.

### 결정 4. 튜닝 툴의 공통 철학 — tune-then-bake (라이브 튜닝 후 소스로 굽기)

- **왜**: FX 색·크기·수명, 스킬 castFrame/recoveryFrame 같은 값은 "빌드 → 실행 → 확인" 루프로는 수렴이 너무 느리다. 반대로 모든 값을 외부 데이터 파일로 빼면 아직 포맷이 안정되지 않은 시기에 로더/검증 인프라 비용이 먼저 든다.
- **선택**: 중간 단계로, 런타임 구조체에 슬라이더를 직접 바인딩해 실시간 조정하고, 확정값은 소스로 다시 베이킹한다.
  - `EffectTuner`: 프리셋 콤보 + 런타임 `IreliaTuning` 구조체 직결 슬라이더(Q/W/E/R 색·크기·수명·오프셋)로 조정하고, "Spawn Test" 버튼이 플레이어 위치에 해당 프리셋을 즉석 스폰해 확인 루프를 닫는다. "Save Preset (Clipboard)" 버튼은 `IreliaFxPresets.cpp`에 붙여넣을 C++ 코드를 생성해 클립보드에 넣는다. `.wfx` Dump / Load+Spawn으로 데이터 에셋 왕복도 지원(Client/Private/UI/EffectTuner.cpp:440-543).
  - `SkillTimingPanel`: const인 `g_SkillTable`을 `const_cast`로 뚫어 castFrame/recoveryFrame/lockDuration/playSpeed(2스테이지 스킬은 stage2 세트까지)를 슬라이더로 조정한다. 주석에 "디버그 전용 런타임 튜너. 확정 값은 Copy 후 SkillTable.cpp 하드코딩"이라고 사용 규약을 박았다(Client/Private/UI/SkillTimingPanel.cpp:21-22).
- **감수한 비용**: const_cast는 명백한 규칙 위반이고, 클립보드 익스포트는 자동화가 아니라 반자동이다. 디버그 전용 코드에 한정하고 주석으로 규약을 명시하는 조건으로 감수했다. 장기적으로는 `.wfx` 같은 데이터 에셋으로 이동 중이다.

### 결정 5. 이펙트 저작 툴은 노드 그래프가 아니라 document 모델부터

- **왜**: Niagara식 노드 그래프(계획서 11파일까지 작성했던)를 바로 만들면, 데이터 모델이 흔들릴 때마다 그래프 UI까지 같이 갈아엎게 된다.
- **선택**: `WfxEffectToolPanel`은 플랫한 WfxDocument/emitter 모델 + 인스펙터 + FxCuePlayer 라이브 프리뷰로 저작하고, Reference 힌트에 "Tune in .wfx first; Niagara-style graph nodes can be layered on this document model later"라고 스코프 판단을 명시했다(Client/Private/UI/WfxEffectToolPanel.cpp:1010).
- **감수한 비용**: 복잡한 이펙트 조합 표현력은 노드 그래프보다 떨어진다. 대신 데이터 모델(.wfx)이 먼저 안정되고, 그래프는 나중에 그 위의 뷰로 얹을 수 있다.

### 결정 6. 에디터는 게임 런타임 Manager를 그대로 공유한다

- **왜**: 에디터에서 배치한 좌표가 게임에서 어긋나면 에디터의 존재 의미가 없다.
- **대안**: 에디터 전용 배치 데이터 모델을 두고 익스포트 시 변환.
- **선택**: `Scene_Editor`는 Structure/Jungle/Minion/Bush 4개 싱글턴 Manager에 에디터 전용 CWorld를 주입해 그대로 구동하고, 맵 메시도 InGame과 동일한 `.wmesh` + X-미러 스케일(-0.01, 0.01, 0.01) + -135도 회전을 재사용한다(Client/Private/Scene/Scene_Editor.cpp:104-135). 저장/로드는 같은 Manager의 `Save_ToFile/Load_FromFile`을 순차 호출하므로 에디터↔게임 왕복이 정의상 1:1이다.
- **감수한 비용**: 싱글턴 Manager를 두 씬이 공유하므로 씬 전환 시 Initialize/Shutdown 수명 규약을 정확히 지켜야 하고, 에디터 진입 시 미니언 스폰을 꺼야 하는(`Set_Enabled(false)`) 식의 모드 게이팅이 필요하다.

에디터 UX는 4패널 구성이다: MenuBar(Stage 1~8 전환, Ctrl+S가 Stage와 NavGrid를 함께 저장) + Palette(AddMode 선택) + Hierarchy(트리 선택) + Inspector(Position/Rotation/Scale/Visible/Delete). 배치는 지면 평면 레이피킹 + 모드별 동작이다:

- `TryPickGroundPlane`: 마우스 월드 레이를 y=0 평면과 교차(`t = -origin.y / dir.y`), 레이가 거의 수평이거나 t<0이면 거부(Scene_Editor.cpp:846-870).
- 좌클릭 = 구조물/정글/부시 추가 또는 미니언 웨이포인트 append. 편집 중 레인의 웨이포인트는 배경 드로우리스트에 라인스트립 + 점 + 인덱스 라벨로 시각화(Scene_Editor.cpp:192-229).
- NavGrid 모드 = 좌드래그 blocked / 우드래그 walkable의 원형 브러시(`dx²+dy² ≤ r²`, Scene_Editor.cpp:822-844). 막힌 셀은 노란 쿼드 오버레이로 표시.

### 결정 7. NavGrid 데이터: 비트팩 + 리비전 카운터 + 콘텐츠 해시

- **왜**: 워커빌리티는 셀당 1비트면 충분하고, 경로 캐시가 그리드 변경을 감지할 수단이 필요했다.
- **선택**: 파일은 `WNVG` magic + version + origin + byteSize 헤더 뒤 비트 덤프이고, 로드 시 byteSize까지 전부 검증한다(Engine/Private/Manager/Navigation/NavGrid.cpp:367-398). 런타임에는 `SetWalkable`이 값이 실제로 바뀔 때만 `BumpRevision`, `ComputeContentHash`(FNV-1a)로 콘텐츠 지문을 만들어 캐시 무효화 근거를 두 겹으로 제공한다(NavGrid.cpp:297-316). 충돌 반경 대응은 `BuildInflated`가 반경만큼 팽창(민코프스키)한 파생 그리드를 만든다.
- **감수한 비용**: 그리드 크기가 컴파일 타임 고정(kByteSize)이라 맵 크기 가변화가 막혀 있다. 단일 맵(소환사의 협곡) 프로젝트라 지금은 수용.

---

## ④ 어려웠던 점과 해결

### 1. 미니언 stuck 사고 → "관측 먼저" 원칙의 코드화

Chase 상태 미니언이 제자리에서 애니만 도는 stuck 사고 때, 코드 추론만 반복하다 시간을 태웠다. 결론은 "증상을 튜닝하기 전에 관측 가능한 프로브부터 만든다"였고, 이걸 CLAUDE.md 규범("현재 셀, 다음 셀/웨이포인트, 보정 방향, stuck 사유를 노출하라")으로 박은 뒤 `DebugDrawSystem::DrawMinionMovement`로 구현했다: 현재 셀 박스(걷기 가능 여부 색), 전방 프로브 다음 셀, 웨이포인트 라인, 이동 방향 벡터, 그리고 `m%u cell=%d,%d path=%u/%u block=%u` 라벨(BlockedMoveFrames 포함)을 머리 위에 띄운다(Client/Private/UI/DebugDrawSystem.cpp:402-514). 이후 이동 버그는 "라벨을 보면 어느 단계가 깨졌는지 보이는" 문제로 바뀌었다.

이 오버레이 군의 컨트롤 타워가 `RenderDebug` 패널이다: F1 마스터 스위치, 카테고리별 체크박스(NavGrid/PathGrid/Structures/Colliders/Champions/MinionMovement/ChampionAIText/Ranges), NavGrid 컬링 반경 슬라이더, "Reload Authored NavGrid" 버튼, 그리고 "All On"/"Champion AI Only" 프리셋(Client/Private/UI/RenderDebug.cpp:11-142). 렌더 효과 검증도 같은 패널에서 한다 — SSAO는 백엔드 가용성(`IsSSAOAvailable`)으로 `BeginDisabled` 게이팅하고, radius/intensity/thickness 슬라이더 + Soft/Reference/Stress 프리셋에 더해 **SSAO 출력 SRV를 `ImGui::Image`로 인게임에 직접 미리보기** 한다. 중간 버퍼를 눈으로 확인할 수 있으니 "SSAO가 적용됐는데 티가 안 남 vs 아예 안 돔"을 즉시 구분할 수 있다.

같은 철학의 네트워크 버전이 `CNetworkEventTrace`다. "호출은 됐는데 FX가 안 보임" 증상에서 이벤트가 실제 도착했는지/어떤 값인지를 먼저 계측하려고, EventPacket을 verify 후 kind별(Damage/SkillCast/ProjectileSpawn/...)로 파싱해 serverTick·src·tgt·값을 링버퍼에 기록하고 kind별 카운터와 함께 ImGui 표로 보여준다. `m_bEnabled`로 오버헤드를 게이팅한다(Client/Private/Network/Client/NetworkEventTrace.cpp:48-58). 이 도구가 있으면 "이벤트가 안 왔다(네트워크/서버 문제)"와 "이벤트는 왔는데 렌더가 안 됐다(클라 FX 문제)"의 CPU/GPU·클라/서버 경계 분기가 코드 추론 없이 즉시 갈린다.

### 2. 서버 권위 전환이 툴을 부쉈다 → 툴이 경계 이동을 따라가게 재설계

게임플레이가 서버 권위로 이동하면서 클라에서 챔피언 스탯을 직접 만지던 `ChampionTuner`는 무의미해졌다. 이 패널은 지금 기능이 제거되고 "Gameplay tuning is server-authoritative. ... Use server debug override data"라는 안내문만 남은 툼스톤이다(Client/Private/UI/ChampionTuner.cpp:23-27). 대체재가 `AIDebugPanel`인데, 이건 읽기 전용이 아니라 **관찰-조종 폐루프**다:

- 관찰: 서버가 SnapshotBuilder에서 채운 `ChampionAIDebugComponent`를 스냅샷으로 복제받아 봇 목록 테이블(AI/Team/State/Intent/Action/Target/Pos/HP/ActionId)로 렌더하고, 선택한 봇의 프로파일 레인지·HP·오버라이드 상태를 상세 표시한다(Client/Private/UI/AIDebugPanel.cpp:360-479).
- 감사(audit): tick별 결정 트레이스 링버퍼를 최신순 8열 테이블(Tick/State/Intent/Action/Block/Cmd/Target/Score)로 보여준다. blockReason(NoTarget/OutOfRange/Cooldown/ActionLocked/...)이 라벨링되어 있어 "봇이 왜 이 판단을 했는가/왜 안 했는가"를 프레임 단위로 되짚는다(AIDebugPanel.cpp:609-661).
- 조종: `SendAIDebugControl/Tune/ResetTuning` 커맨드를 네트워크로 서버에 되쏜다. 모든 버튼은 연결 여부로 `BeginDisabled` 게이팅(AIDebugPanel.cpp:604-607). 튜닝 슬라이더의 확정값은 서버로 반영된다.
- 자기 설명: 봇은 보이는데 디버그 스냅샷이 없으면 패널이 "No Champion AI snapshot yet. If bots are visible, check server SnapshotBuilder debug flags."라고 파이프라인의 실패 지점을 직접 안내한다(AIDebugPanel.cpp:443-447). 관측 데이터도 `bPresent` 게이트로 유효성을 검사한다.

핵심은 디버그 데이터가 별도 백채널이 아니라 **정규 스냅샷 복제 경로**로 흐른다는 점이다. 봇 AI는 truth를 직접 고치지 않고 command를 생산한다는 컴퍼스 원칙과 정합하고, 디버그 도구가 곧 복제 파이프라인의 통합 테스트가 된다.

### 3. 씬 전환 self-destruct → use-after-free 가드

에디터에서 ESC로 인게임 복귀를 요청하면 씬 전환이 에디터 씬 **자신의 소멸**을 유발한다. `Request_BackToInGame()` 직후 멤버를 만지면 use-after-free다. 해결은 단순하지만 잊기 쉬워서, 호출 지점에 "ESC → Scene 전환은 self-destruct 를 유발 → 즉시 return 필수 (use-after-free 방지)" 주석과 함께 즉시 return을 강제했다(Scene_Editor.cpp:159-164). 미저장 편집은 `m_bDirty`로 추적하고, 복귀 시 dirty면 MessageBox(YES/NO/CANCEL)로 저장/버림/취소를 물어 CANCEL이면 전환 자체를 중단한다(Scene_Editor.cpp:986-1003). 툴에서 데이터를 잃는 경험은 툴 신뢰를 한 번에 무너뜨리기 때문에 dirty 가드는 에디터의 최소 요건으로 봤다.

### 4. 자작 바이너리 포맷의 버전 지옥 → magic + version 범위 + 섹션별 게이트

Stage.dat은 `StageHeader(magic, version)` 뒤에 각 Manager가 순차 직렬화한다. 로드 시 magic 검사, version 범위 검사 `[MIN_COMPAT, VERSION]` 후, 신규 섹션은 버전 게이트로 읽는다: v≥4면 미니언 웨이포인트 로드, 아니면 `LoadDefaults()`; v≥5면 Bush 로드, 아니면 `Clear()`(Client/Private/Map/MapDataIO.cpp:138-178). 구버전 파일이 에러가 아니라 "기본값으로 안전하게 열리는" 상태가 되도록 했다. NavGrid도 같은 패턴: `WNVG` magic + version + byteSize를 검증하는 셀당 1비트 팩 포맷이고, 파일이 아예 없으면 에디터가 배치물 AABB로 경계 그리드를 합성해(`CreateStageBoundsNavGrid`, 전체 walkable) 툴이 죽지 않게 했다(Engine/Private/Manager/Navigation/NavGrid.cpp:344-398, Scene_Editor.cpp:49-101).

단, 여기 **자백할 결함**이 하나 있다: `Save_Stage/Load_Stage`는 실패/성공 메시지를 `sprintf_s`로 로컬 버퍼에 포맷만 하고 어디에도 출력하지 않는다(MapDataIO.cpp:94-95, 147, 153). 팀 에러핸들링 정책으로 "방출되지 않는 진단(dead diagnostic)은 없느니만 못하다"고 박제한 바로 그 안티패턴이 잔존하는 사례다. 올바른 대조 사례는 NavGrid 쪽의 bounded 방출이고, 이건 발견된 정책 위반으로 수정 대상이다.

### 5. 에셋이 런타임에 조용히 깨지는 문제 → 저작 시점 프리플라이트

FX는 텍스처/erode/model 경로 중 하나만 빠져도 런타임에 "안 보임"으로만 나타난다. 그래서 `WfxAssetCatalog::ScanDirectory`가 모든 .wfx를 실제 로드해 보고, 각 이미터의 리소스 경로를 `std::filesystem::exists`로 검증해 Ready/Missing/Load-failed 상태와 누락 목록을 카탈로그에 표시한다(Client/Private/UI/WfxAssetCatalog.cpp:119-190). 깨짐을 런타임 디버깅이 아니라 저작 시점 관측으로 앞당긴 것이다.

### 6. 서드파티 헤더 vs 자체 매크로

엔진의 디버그 `new` 매크로가 ImGui의 placement-new를 깨뜨린다. ImGui를 포함하는 TU마다 `#pragma push_macro("new") / #undef new / #include <imgui.h> / #pragma pop_macro("new")` 관용구로 국소 무력화한다(예: DebugDrawSystem.cpp:17-20, SkillTimingPanel.cpp:6-9). FlatBuffers 헤더는 min/max 매크로에 대해 같은 처리를 한다.

---

## ⑤ 향후 개선 방향

1. **MapDataIO dead-diagnostic 제거** — 포맷만 하고 버리는 진단을 bounded 방출로 교체(§④-4에서 자백한 정책 위반). 팀 에러핸들링 정책이 이미 있으므로 순수 배선 작업이다.
2. **노드 그래프 이펙트 에디터** — 안정화된 WfxDocument 모델 위에 뷰로 얹는다. Phase G 계획서(노드 그래프 설계)가 이미 있고, "document 모델 안정화가 먼저"라는 순서만 지킨다. 툴 코드에 그 유예 결정이 힌트 문구로 박혀 있어 나중에 합류하는 사람도 의도를 안다.
3. **HUD의 RHI 경로 완성** — 체력바/커서가 ImGui 드로우리스트와 RHI UI 렌더러 병행인데, DX12 이관이 끝나면 RHI 경로로 일원화하고 ImGui 폴백은 디버그 전용으로 강등한다.
4. **에디터 코어 선행** — UE5.7 감사에서 잡은 방향대로, 어빌리티 타임라인 툴 등 다음 툴은 재사용 가능한 에디터 코어(도킹, 문서 모델, undo) 위에 올린다. 지금의 F키 토글 패널 군은 그 전 단계의 실용주의다.
5. **tune-then-bake의 데이터화** — 클립보드 C++ 익스포트를 .wfx/데이터 팩 저장으로 대체해 const_cast 튜너 계열을 은퇴시킨다. ChampionTuner 툼스톤처럼, 은퇴한 툴은 흔적을 남기고 정리한다.
6. **에디터 undo/redo** — 현재는 dirty 가드와 Delete만 있고 undo 스택이 없다. 배치 작업량이 늘면 가장 먼저 체감될 부채다.

---

## ⑥ 면접 Q&A

**Q1. 멀티 백엔드 엔진에서 에디터 UI를 어떻게 이식성 있게 붙였나?**
- 답변 골격: CImGuiLayer는 구체 디바이스를 모르고 `IRHIDevice::GetNativeHandle`로 네이티브 핸들만 뽑아 imgui_impl_dx11/dx12를 초기화한다. BeginFrame/EndFrame이 backend 분기, DX12는 디바이스가 열어 둔 프레임 커맨드리스트에 기록.
- 꼬리질문 대비: "GetNativeHandle은 추상화 누수 아닌가?" → 맞다, 의도적 탈출구다. 서드파티(ImGui) 통합 지점 한 곳에만 허용하고 게임 렌더 코드는 RHI 타입만 쓴다. / "DX12에서 뭐가 달랐나?" → imgui 1.92+가 SRV 디스크립터 할당을 앱에 위임해서 free-index 스택 힙 할당자를 직접 구현했다.

**Q2. 게임 입력과 에디터 UI 입력 충돌은 어떻게 막았나?**
- 답변 골격: 이중 게이팅. WndProc에서 ImGui 핸들러가 먼저 소비하면 early return, 씬 로직은 배치 전에 WantCaptureMouse 재검사. 커서는 NoMouseCursorChange로 자체 관리.
- 꼬리질문 대비: "왜 두 겹인가?" → WndProc 게이트는 메시지 단위, WantCaptureMouse는 프레임 상태 단위라 커버리지가 다르다. 드래그 중 포커스 이동 같은 경계 케이스에서 한쪽만으로는 샌다.

**Q3. 인게임 HUD는 어떤 구조인가? UI가 게임 상태를 직접 읽나?**
- 답변 골격: 안 읽는다. 컴퍼스 규칙으로 "UI 패널은 렌더만, 판정은 Client 브릿지가 view data로"를 박았다. 체력바는 Scene_InGame이 ECS를 순회해 UIWorldHealthBarDesc 배열을 만들어 엔진 CUI_Manager에 푸시하고, 미니맵도 BuildMinimapFrameState→RenderRuntime의 build-then-render 분리.
- 꼬리질문 대비: "왜 그렇게까지?" → 서버 권위 게임이라 truth가 스냅샷에 있다. UI가 월드를 직접 읽으면 시야/생사/팀 판정 로직이 UI에 중복되고, 권위 경계가 흐려진다. / "미니맵 좌표계는?" → 3점 보정(UV 00/10/01의 월드 좌표)으로 world↔UV 아핀 변환, 클릭 이동도 같은 투영의 역변환.

**Q4. 디버그 시각화 인프라를 어떻게 저비용으로 만들었나?**
- 답변 골격: 전용 라인 렌더 파이프라인 대신 VP 행렬로 직접 스크린 투영해 ImGui 배경 드로우리스트에 그린다. w≤0.01 클립 처리, 와이어박스/실린더/라벨 헬퍼.
- 꼬리질문 대비: "한계는?" → ImGui 프레임 안에서만 호출 가능, 깊이 테스트 없음. 디버그 용도라 수용. / "성능은?" → 카테고리별 토글 + NavGrid는 카메라 반경 컬링으로 막힌 셀만 그린다.

**Q5. 미니언이 제자리에 멈추는 버그를 어떻게 잡았나?**
- 답변 골격: 코드 추론을 반복하다 실패한 뒤, "증상 튜닝 전에 관측부터"를 팀 규범으로 박고 DrawMinionMovement를 만들었다. 현재 셀/다음 셀 프로브/웨이포인트 선/이동 방향/BlockedMoveFrames 라벨을 머리 위에 띄우니 어느 단계가 깨졌는지가 보였다.
- 꼬리질문 대비: "그 원칙이 다른 데도 적용됐나?" → NetworkEventTrace(이벤트 도착/값 링버퍼 인스펙터), AIDebugPanel(봇 결정 트레이스), WfxAssetCatalog(에셋 프리플라이트) 전부 같은 철학이다.

**Q6. 서버에서 도는 봇 AI를 클라에서 어떻게 디버깅하나?**
- 답변 골격: 서버 SnapshotBuilder가 결정 트레이스(tick/state/intent/action/blockReason/점수) 링버퍼를 스냅샷에 실어 복제하고, 클라 AIDebugPanel이 표로 렌더한다. 역방향으로 오버라이드/튜닝 커맨드를 서버에 되쏘는 폐루프.
- 꼬리질문 대비: "왜 클라 로컬에서 안 하고?" → 게임플레이 truth가 서버에 있어서 클라 튜닝은 의미가 없다. 실제로 클라측 ChampionTuner는 폐기하고 안내문만 남긴 툼스톤이다 — 아키텍처 경계가 이동하면 툴도 따라가야 한다는 사례.

**Q7. 맵 에디터와 게임의 배치가 어긋나지 않게 어떻게 보장했나?**
- 답변 골격: 에디터 전용 데이터 모델을 안 만들고 런타임 싱글턴 Manager 4종을 그대로 공유한다. 맵 메시/X-미러/회전 변환도 InGame과 동일 값 재사용. 저장·로드도 같은 Manager 직렬화 함수라 왕복이 정의상 1:1.
- 꼬리질문 대비: "픽킹은?" → 마우스 월드 레이를 y=0 평면과 교차(t=-origin.y/dir.y), 수평·음수 t 거부. NavGrid는 좌드래그 blocked/우드래그 walkable 원형 브러시(dx²+dy²≤r²). / "수명 함정은?" → ESC 씬 전환이 에디터 자신을 파괴하므로 Request 직후 즉시 return 가드.

**Q8. 자작 바이너리 포맷(Stage.dat)의 버전 관리는?**
- 답변 골격: magic + version 헤더, 로드 시 [MIN_COMPAT, VERSION] 범위 검사, 신규 섹션은 버전 게이트(v≥4 미니언 WP 아니면 LoadDefaults, v≥5 Bush 아니면 Clear). 구버전 파일이 에러가 아니라 기본값으로 열린다.
- 꼬리질문 대비: "실패 로깅은?" → 여기서 dead-diagnostic 안티패턴을 자백한다: 진단을 포맷만 하고 방출하지 않는 코드가 잔존하고, 팀 정책상 수정 대상이다. 결함을 아는 것과 모르는 것의 차이를 보여줄 기회.

**Q9. 값 튜닝(FX, 스킬 타이밍)을 어떻게 반복 가능하게 만들었나?**
- 답변 골격: tune-then-bake. 런타임 구조체에 슬라이더 직결로 실시간 확인하고, 확정값은 클립보드 C++ 코드 생성(EffectTuner) 또는 수동 복사(SkillTimingPanel)로 소스에 굽는다. 빌드 루프가 사라져 FX 수렴 속도가 근본적으로 달라졌다.
- 꼬리질문 대비: "const 테이블을 const_cast로 뚫는 건 괜찮나?" → 디버그 전용 + 주석으로 규약 명시라는 조건부 수용이고, 장기적으로 .wfx 데이터 에셋으로 이동해 은퇴시키는 게 방향이다.

**Q10. 툴에 언제 투자하나? 기준이 있나?**
- 답변 골격: 세 가지 신호. (1) 같은 증상을 두 번째 코드 추론으로 잡으려 할 때 — 관측 프로브가 먼저다(미니언 stuck → DebugDrawSystem). (2) 반복 루프에 빌드가 끼어 있을 때 — 라이브 튜닝 툴(EffectTuner). (3) 실패가 런타임에 조용히 나타날 때 — 저작/접속 시점 검증으로 앞당긴다(WfxAssetCatalog 프리플라이트). 반대로 노드 그래프처럼 데이터 모델이 미확정인 툴은 의도적으로 유예한다.
- 꼬리질문 대비: "툴이 많아지면 관리 비용은?" → F1~F10 토글 체계로 조직하고 legacy 패널은 F10 번들 뒤로 격리, 각 패널 렌더를 프로파일 스코프로 계측한다(Scene_InGameImGui.cpp:34-105). 출하 영향은 no-op 스텁/Enabled 게이팅으로 차단.

---

## 부록 — 이 챕터의 코드 앵커 (전부 실물 검증됨)

| 주제 | 위치 |
|---|---|
| ImGui RHI 래핑 + DX12 SRV 힙 할당자 | Engine/Private/Editor/ImGuiLayer.cpp:60-115, 174-318 |
| 프레임 오케스트레이션 / 런타임 모드 게이팅 | Engine/Private/Framework/CEngineApp.cpp:373-504 |
| WndProc ImGui 우선 소비 | Engine/Private/Platform/CWin32Window.cpp:166-168 |
| 체력바 view-data 브릿지 | Client/Private/Scene/Scene_InGame.cpp:597-676 |
| HUD 에셋 카탈로그 / 커서 RHI-ImGui 이원화 | Engine/Private/Manager/UI/UI_Manager.cpp:39-64, 1415-1435 |
| 미니맵 3점 보정 투영 / build-then-render | Client/Public/UI/MinimapPanel.h:27-96 |
| 이미지 씬 프리젠터 (밴픽/로딩) | Client/Public/UI/ImageScenePresenter.h:25-64 |
| 3D 오버레이 WorldToScreen → 배경 드로우리스트 | Client/Private/UI/DebugDrawSystem.cpp:31-103 |
| 미니언 이동 관측 뷰 (셀/프로브/웨이포인트/block 라벨) | Client/Private/UI/DebugDrawSystem.cpp:402-514 |
| 오버레이 컨트롤 타워 + SSAO 프리뷰 | Client/Private/UI/RenderDebug.cpp:11-142 |
| F키 토글 체계 / legacy 번들 | Client/Private/Scene/Scene_InGameImGui.cpp:34-130 |
| AI 관찰-조종 폐루프 / 결정 트레이스 | Client/Private/UI/AIDebugPanel.cpp:360-479, 604-661 |
| 네트워크 이벤트 링버퍼 인스펙터 | Client/Private/Network/Client/NetworkEventTrace.cpp:48-58 |
| 에디터 부트스트랩 / self-destruct 가드 / dirty 가드 | Client/Private/Scene/Scene_Editor.cpp:104-164, 986-1003 |
| 지면 레이피킹 / NavGrid 브러시 / WP 시각화 | Client/Private/Scene/Scene_Editor.cpp:192-229, 736-921 |
| NavGrid 폴백 합성 (경계 AABB) | Client/Private/Scene/Scene_Editor.cpp:49-101, 974-984 |
| Stage 바이너리 버전 게이트 / dead-diagnostic | Client/Private/Map/MapDataIO.cpp:87-196 |
| NavGrid 비트팩 영속화 / 콘텐츠 해시 / 팽창 | Engine/Private/Manager/Navigation/NavGrid.cpp:297-398 |
| EffectTuner tune-then-bake | Client/Private/UI/EffectTuner.cpp:440-543 |
| SkillTimingPanel const_cast 규약 | Client/Private/UI/SkillTimingPanel.cpp:19-43 |
| ChampionTuner 툼스톤 | Client/Private/UI/ChampionTuner.cpp:22-30 |
| Wfx 노드 그래프 유예 힌트 | Client/Private/UI/WfxEffectToolPanel.cpp:1003-1012 |
| Wfx 에셋 프리플라이트 | Client/Private/UI/WfxAssetCatalog.cpp:119-190 |
| imgui push_macro("new") 관용구 | Client/Private/UI/DebugDrawSystem.cpp:17-20 |

---

## ⑦ 다른 챕터와의 연결

- **RHI/렌더 백엔드 챕터**: CImGuiLayer의 GetNativeHandle 패턴과 HUD의 ImGui/RHI 병행 경로는 DX11→DX12 점진 이관 전략(RenderWorldSnapshot 병행)의 UI측 단면이다.
- **네트워크/서버 권위 챕터**: AIDebugPanel·NetworkEventTrace는 스냅샷/이벤트 복제 경로 위에 세운 관측 도구이고, ChampionTuner 툼스톤은 권위 경계 이동이 툴에 미친 영향의 실물이다. "UI는 렌더만, truth는 스냅샷" 원칙이 양쪽을 잇는다.
- **내비게이션/이동 챕터**: DebugDrawSystem의 미니언 이동 뷰와 NavGrid 비트팩/리비전/콘텐츠 해시는 이동 버그 전쟁(stuck, yaw)에서 나온 관측 인프라다. 에디터의 NavGrid 브러시가 그 데이터의 저작 지점.
- **FX 파이프라인 챕터**: EffectTuner→WfxEffectTool→(유예된) 노드 그래프는 하드코딩 → tune-then-bake → 데이터 주도 저작으로 가는 마이그레이션 서사이고, FxCuePlayer/폴백 카탈로그가 런타임측 짝이다.
- **에러 핸들링 정책 챕터**: MapDataIO dead-diagnostic 잔존 vs NavGrid bounded 방출 대조는 "방출 없는 진단 금지" 정책의 반례/정례 쌍으로 그대로 쓸 수 있다.
