# 13. UI 파이프라인 — 면접 대비 세션

> 도메인 상태: **working** (정직성 지도 13번 기준)
> 근거 코드: `Engine/Private/Renderer/UIRenderer.cpp`, `Engine/Private/Manager/UI/*`, `Client/Private/Scene/Scene_InGame.cpp`, `Client/Private/UI/*`
> 설계 문서: `.md/architecture/WINTERS_UI_PIPELINE_ARCHITECTURE.md`, `.md/문서/10_Ch10_UI.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: UI 파이프라인은 "게임 시뮬레이션의 truth(HP/쿨다운/위치/팀)를 매 프레임 화면 픽셀로 변환하되, *UI는 truth를 읽기만 하고 절대 쓰지 않는다*"는 단방향 데이터 흐름을, 화면공간 2D 사각형(스프라이트)을 GPU에 배치 드로우하는 렌더러 위에 올린 것이다.

**현재 성숙도(정직하게 혼재)**:
- **working(자체 구현, 매 프레임 가동)**: DX11 기반 배치 스프라이트 렌더러 `CUIRenderer`. 동적 VB, 전용 VS/PS, SRV 변경 시 flush 배칭, DX11 state save/restore, 미니맵용 원형 클립. 체력바/ActorHUD 배경/데미지폰트/커서/오버레이 이미지를 이걸로 그린다.
- **working**: `CUIAtlasManifest` 자체 JSON 파서 + WIC PNG→SRV 로딩. 코드 수정 없이 HUD/상점 에셋 교체.
- **working**: Client(ECS/Snapshot) → Engine UI 단방향 상태 동기화(`ActorHUDState`, `UIWorldHealthBarDesc`).
- **ImGui 의존(자체 아님)**: 텍스트/숫자/상점 그리드/스코어보드/킬피드는 여전히 ImGui `ImDrawList` + ImGui 폰트(`Font_Manager`가 `AddFontFromFileTTF`). **자체 런타임 폰트 렌더러 없음.**
- **prototype**: `LuaUIHost` — Lua 스크립트로 UI를 구성하는 실험 경로.
- **planned-only(문서만)**: retained-mode 위젯 트리(`CUIRoot`/`CUIObject`/`CUIImage`/`CUIPanel`), 레이아웃/스타일 시트 분리, invalidation/dirty 캐시.

> 핵심 정직선: **"스프라이트/이미지 계층은 자체 DX11 렌더러, 텍스트는 ImGui 어댑터. 위젯 트리는 설계만."** 이걸 먼저 말하고 들어간다.

---

## 1. 핵심 개념 (본질)

### 1.1 UI는 "또 하나의 렌더 시스템"이다 (first principles)

게임 UI를 "버튼 라이브러리"로 보면 본질을 놓친다. 근본은:
1. **상태(state)**: HP=320/640, 쿨다운=2.1s, 팀=Blue 같은 게임 truth.
2. **레이아웃(layout)**: 그 상태를 화면 어디에/얼마 크기로 놓을지(픽셀 좌표·앵커·zOrder).
3. **렌더(paint)**: 그 사각형들을 GPU에 텍스처드 쿼드로 제출.
4. **입력(input)**: 마우스/키 → 어떤 위젯이 hit됐는가(hit-test).

내 파이프라인은 이 중 (1)→(3)을 **매 프레임 즉시(immediate)** 다시 계산한다. 즉 retained 위젯 트리가 없고, "이번 프레임의 HUD 상태"를 받아 곧장 드로우 콜로 변환한다.

### 1.2 immediate mode vs retained mode (왜 둘이 갈리나)

- **Immediate mode**(현재 내 HUD, ImGui도 같은 철학): 매 프레임 "지금 HP는 320이니 이 위치에 이 길이 바를 그려라"를 처음부터 다시 호출. 위젯 객체를 유지하지 않음. **장점**: 상태-UI 동기화 버그가 구조적으로 안 생김(매 프레임 truth에서 새로 만드니까). **단점**: 위젯 수 N에 대해 매 프레임 O(N) 풀 재계산 + 풀 재제출. 트리 invalidation(바뀐 것만 다시 그림)이 불가능.
- **Retained mode**(Slate/UMG, 내 planned `CUIRoot`): 위젯 객체 트리를 유지하고, 바뀐 노드만 dirty 표시 후 재페인트(invalidation). **장점**: 정적 UI에서 페인트 비용 절감, 복잡한 레이아웃·애니메이션·접근성에 유리. **단점**: 위젯-상태 동기화 코드(property binding)가 필요하고 그 자체가 버그 원천.

> 면접 포인트: 내가 immediate로 시작한 건 *우연이 아니라 트레이드오프 선택*이다. HUD처럼 매 프레임 거의 모든 값이 변하는 화면(HP/쿨다운/위치 추적 체력바)에서는 invalidation 이득이 작고, immediate가 동기화 버그를 원천 차단한다.

### 1.3 배치(batching)와 드로우 콜이 왜 병목인가

GPU에 사각형 1개를 그리려면 VB 바인딩→셰이더 바인딩→텍스처(SRV) 바인딩→Draw 호출이 필요하다. 사각형마다 이걸 다 하면 CPU가 드라이버 호출(드로우 콜)에 깔린다. 핵심 통찰: **같은 텍스처(SRV)를 쓰는 사각형들은 정점만 한 버퍼에 모아 Draw 한 번으로 묶을 수 있다.**

그래서 내 `CUIRenderer`는:
- `DrawImage`가 즉시 GPU를 때리지 않고 **CPU 정점 배열(`std::vector<UIVertex>`)에 6개 정점(2 삼각형)을 push**한다.
- **SRV가 바뀌는 순간에만 `Flush()`**(현재 모은 정점을 한 Draw로 제출)한다 (`UIRenderer.cpp:500-505`).
- 같은 아틀라스 텍스처를 연속으로 쓰면 드로우 콜 1번으로 수십 개 사각형이 나간다.

이게 "텍스처 아틀라스"가 존재하는 이유다. 여러 스프라이트를 한 PNG에 모으면 SRV가 안 바뀌어 배칭이 깨지지 않는다.

### 1.4 화면공간 좌표 → NDC 변환

UI는 픽셀 좌표(좌상단 0,0, y 아래로 증가)로 사고하지만 GPU는 NDC(-1..+1, y 위로 증가)를 원한다. 변환은 셰이더 VS에서:
```
ndc.x = (px / screenW) * 2 - 1;
ndc.y = 1 - (py / screenH) * 2;   // y 뒤집기
```
(`UIRenderer.cpp:65-67`). 행렬을 cbuffer로 넘기는 대신 screenSize 2개만 넘기고 VS에서 직접 계산 — UI는 항상 직교(orthographic)라 풀 행렬이 불필요.

### 1.5 깊이 없는 알파 블렌딩과 그리기 순서

UI는 깊이 테스트를 끄고(`DepthEnable=FALSE`, `UIRenderer.cpp:349`) **그린 순서가 곧 zOrder**다(painter's algorithm). 알파 블렌딩은 premultiplied가 아니라 일반 src-alpha 블렌딩(`SrcBlend=SRC_ALPHA, DestBlend=INV_SRC_ALPHA`, `:337-338`). 그래서 호출 순서(배경→바→텍스트)가 곧 레이어링이고, 이게 immediate-mode HUD에서 zOrder를 관리하는 가장 단순하고 정직한 방법이다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 + Trade-off

### 2.1 핵심 결정 표

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **자체 DX11 스프라이트 렌더러** (CUIRenderer) | RHI/게임 렌더 패스와 동일 디바이스·텍스처 공유, 배칭/state 완전 제어, 미니맵 원형 클립 같은 커스텀 가능 | 폰트/텍스트 레이아웃을 직접 못 함 | HUD 이미지(체력바·아이콘·오버레이)는 GPU 배칭 이득이 크고 게임 텍스처(FoW SRV 등)를 직접 그려야 함 |
| ImGui 전부 사용 | 즉시 텍스트·위젯·디버그 무료 | 게임 HUD를 ImGui로 그리면 lib에 종속·아틀라스 배칭 제어 불가·릴리즈 룩 한계 | 텍스트/디버그/상점 그리드만 ImGui로 남기고 이미지 계층은 분리 |
| **텍스트도 자체 폰트 아틀라스 렌더러** | ImGui 완전 탈피, 일관 배칭 | SDF/글리프 캐시/커닝/유니코드 폴백을 직접 구현 = 큰 비용 | 신입 1인 범위에서 *지금* 만들 가치 낮음 → ImGui 어댑터로 미룸(planned) |
| **retained 위젯 트리 즉시 구축** | 확장성·invalidation·에디터 친화 | 상태-바인딩 인프라·dirty 전파·hit-test 트리 전부 선구현 필요 | HUD가 매 프레임 거의 다 변하므로 invalidation 이득 적음 → immediate 먼저, 트리는 설계만 |
| **JSON 런타임 아틀라스 매니페스트** | 코드 수정 없이 에셋/rect 교체, 디자이너 협업 | 런타임 파싱·문자열 키 룩업 비용 | UI는 빈도 낮은 로드 1회 경로라 비용 무시 가능, 협업 가치 큼 |
| 수기 JSON 파서 (JsonCursor) | 외부 의존 0, DLL 경계 깔끔 | escape/유니코드/에러복구 취약 | 매니페스트 스키마가 고정·단순(textures/sprites)이라 충분, FX/ECS와 동일 패턴 재사용 |

### 2.2 근본 트레이드오프 정리

1. **immediate vs retained**: "동기화 버그 0(immediate)" vs "페인트 비용 절감·확장성(retained)". HUD 특성상 전자가 합리적. 단, 상점·스코어보드·인벤토리처럼 *대부분 정적인* 화면이 늘면 retained 이득이 커지므로 그쪽부터 트리로 옮기는 게 다음 단계.
2. **자체 렌더러 vs ImGui**: 이미지는 자체(배칭·게임 텍스처 공유), 텍스트는 ImGui(글리프 인프라 무료). **하이브리드가 의도된 분업**이지 미완성의 변명이 아니다.
3. **런타임 JSON vs offline codegen**: ECS Def pack은 매 프레임 경로라 offline codegen으로 string lookup을 없앴지만, UI 아틀라스는 로드 1회라 런타임 JSON이 옳다. *같은 엔진에서도 빈도에 따라 반대 결정을 내린* 사례.

### 2.3 왜 신입 1인 범위에서 합리적인가

- 위젯 트리/SDF 폰트/invalidation을 전부 먼저 만들면 *HUD 하나 그리기까지* 수천 줄을 깔아야 한다. 그건 over-engineering이고 Karpathy 가드레일 위반.
- 대신 "지금 화면에 뜨는 HUD"를 먼저 동작시키고(배치 렌더러+ImGui 텍스트), 확장이 실제로 필요해지는 지점(상점/에디터 UI)에서 트리를 도입하는 게 측정주도·점진 개발이다.

---

## 3. 실제 구현 (코드 근거)

### 3.1 CUIRenderer — 배치 스프라이트 렌더러 (핵심)

파일: `Engine/Private/Renderer/UIRenderer.cpp` (602줄), 헤더 `Engine/Public/Renderer/UIRenderer.h`.

**자료구조**:
- `UIVertex { px, py, u, v, r,g,b,a }` (8 float, pos2/uv2/color4) — `:18-28`.
- CPU 정점 누적 버퍼 `std::vector<UIVertex> vertices` + 현재 바인딩된 SRV `pCurrentSRV` — `:143-144`.
- 동적 VB `kMaxUIVertices=65536` (D3D11_USAGE_DYNAMIC, MAP_WRITE_DISCARD) — `:36, :317-324`.

**셰이더**(인라인 HLSL, `D3DCompile`=FXC로 컴파일, `:38-79`): VS가 픽셀→NDC 변환(y 뒤집기), PS는 `texColor * vColor` (틴트). cbuffer는 screenSize 2 float만.

**데이터 흐름 / 호출 경로**:
1. `Begin(w,h,sampler)` — `SaveState()`로 기존 DX11 파이프라인 상태를 전부 백업 후, UI용 IA/VS/PS/blend/depth/raster/sampler를 세팅, screenSize cbuffer 업로드 (`:418-460`).
2. `DrawImage(srv, x,y,w,h, uvRect, color)` — **SRV가 다르거나 정점 한도 초과면 `Flush()` 후 SRV 교체**, 그다음 6 정점 push (`:486-525`). 이게 배칭의 심장.
3. `DrawImageCircle(...)` — 미니맵용. 중심+세그먼트 fan으로 원형 클립된 텍스처를 그림(세그먼트 12~96 클램프) (`:527-588`).
4. `End()` — 남은 정점 `Flush()` 후 `RestoreState()`로 백업한 DX11 상태 원복 (`:462-471`).

**`Flush()`** (`:165-186`): VB를 MAP_WRITE_DISCARD로 매핑→memcpy→Unmap→`PSSetShaderResources(0,1,srv)`→`Draw(vertexCount,0)`→`vertices.clear()`. **드로우 콜 1번 = 같은 SRV의 모든 사각형.**

**`SaveState()`/`RestoreState()`** (`:188-249`): IA layout/topology/VB, VS/PS, VS cbuffer0, PS SRV0/sampler0, blend/depth/raster를 ComPtr로 Get→Set. **이게 중요한 이유**: UI는 게임 메인 패스 *뒤에* 같은 디바이스 컨텍스트로 끼어들기 때문에, UI가 건드린 상태를 원복하지 않으면 다음 프레임/다른 렌더러가 깨진다. immediate-mode 렌더러를 공유 컨텍스트에 안전하게 끼우는 정석 패턴.

### 3.2 CUIAtlasManifest — JSON 아틀라스 (자체 파서)

파일: `Engine/Private/Manager/UI/UIAtlasManifest.cpp` (410줄), 헤더 `Engine/Public/Manager/UI/UIAtlasManifest.h`.

- 자료구조: `UIAtlasTextureDef{ path, width, height, pSRV }`, `UISpriteDef{ textureID, x,y,w,h }`, 둘 다 `unordered_map<string,...>` (`.h:12-42`).
- `JsonCursor` — 수기 커서 파서(FindKey/ParseString/ParseNumber/SkipValue, 중첩 `{}`/`[]` skip, escape 처리) (`:56-209`). 외부 JSON lib 의존 0.
- `ResolveUVRect(sprite)` — sprite 픽셀 rect를 텍스처 width/height로 나눠 **정규화 UV(0..1)**로 변환 (`:396-409`). 이 UV가 `DrawImage`의 `vUVRect`로 들어가 아틀라스에서 한 스프라이트만 샘플.
- 로딩 경로: `UI_Manager`가 `LoadFromJson`→`ForEachTexture`로 각 텍스처 path를 WIC로 SRV 생성→`SetTextureSRV`로 주입 (`UI_Manager.cpp:772-788`).

### 3.3 Client → Engine 단방향 상태 동기화 (북극성: UI는 truth를 읽기만)

파일: `Client/Private/Scene/Scene_InGame.cpp::SyncActorHUDStateToEngineUI` (`:317-469`).

- ECS 컴포넌트(`ChampionComponent`, `HealthComponent`, `SkillStateComponent`, `ExperienceComponent`, `SkillRankComponent`, `RuneRuntimeComponent`, `FormOverrideComponent`/`SpellbookOverrideComponent`)에서 값을 긁어 **POD 구조체 `Engine::ActorHUDState`** 하나로 추림 → `CGameInstance::Get()->UI_Set_ActorHUDState(&State)`로 Engine에 전달 (`:469`).
- 월드 체력바: `std::vector<Engine::UIWorldHealthBarDesc>`를 만들어(`Scene_InGame.cpp:599~`, Character/Unit/Structure 종류별) Engine UI에 밀어넣음.
- **핵심 설계**: Engine UI는 `EntityID`/HP/팀/위치만 담은 평평한 POD만 받는다(`ActorHUDState.h`, `WorldHealthBarState.h`). Engine은 `CWorld`/`Shared/GameSim`/ChampionComponent를 직접 모른다 → 의존성 방향이 Client→Engine 단방향. 이게 정직성 지도의 "Client(ECS/Snapshot)→Engine UI 단방향 상태 동기화"의 실체.

### 3.4 ImGui 어댑터 계층 (텍스트/상점/패널)

`UI_Manager.cpp`(4160줄)의 절반은 `ImDrawList*` 기반 그리기다: `DrawStatusPanel`/`DrawInGameShop`/`DrawKillFeedBanners`/`DrawDamageFloaters`/`DrawActorHUDOverlay`/`DrawMapPings` 등. 폰트는 `Font_Manager`가 ImGui `AddFontFromFileTTF`로 로드한 `ImFont*`(`Font_Manager.cpp:65-70`). **체력바·커서·오버레이 이미지는 `m_pRHIUIRenderer->DrawImage`(자체), 텍스트/숫자/상점 그리드는 `ImDrawList`(ImGui)** — 한 화면 안에서 두 경로가 공존한다.

### 3.5 LuaUIHost (prototype)

`Engine/Private/Manager/UI/LuaUIHost.cpp` (649줄). Lua 5.4 런타임 위에서 스크립트가 `CUIRenderer`/매니페스트/폰트를 호출해 UI를 구성하는 실험 경로. 라이브 HUD 주 경로는 아니다.

---

## 4. 검증 — 동작을 어떻게 증명했나

> 정직하게: **이 도메인은 자동 골든/스모크 게이트가 없다.** 검증은 계측+토글+수동 시각확인 + 빌드 게이트 수준이다. 과장하지 않는다.

- **로드 실패 가시화**: 매니페스트/WIC 로드 실패 시 `OutputDebugStringW`로 hr·경로·resolved·cwd를 찍어(`UI_Manager.cpp:727-729`, `LoadActorHUDAssets`의 각 fallback 로그 `:791,797`) "에셋이 왜 안 뜨는가"를 즉시 추적. 매니페스트 로드 실패 시 fallback 경로 재시도(`:773-774`).
- **fallback 체인**: 매니페스트/레이아웃 둘 다 실패하면 built-in 하드코드 레이아웃으로 떨어지므로 HUD가 *완전히 사라지지는* 않음(`:794-798`) → "에셋 없음"과 "코드 버그"를 분리 진단.
- **상태 동기화 검증**: `SyncActorHUDStateToEngineUI`가 매 프레임 호출되는지(`Scene_InGame.cpp:1203`)와 HP/쿨다운이 ECS 값과 일치하는지를 인게임에서 시각 대조.
- **상태 누수 검증**: `SaveState/RestoreState`가 빠지면 UI 다음 프레임 게임 렌더가 깨지는 게 즉시 눈에 보임 → 회귀가 시각적으로 드러나는 구조.
- **빌드/경계 게이트**: 협업 하니스(도메인 16)의 의존성 audit이 "Engine UI가 Client/GameSim 타입을 직접 include하지 않는가"를 막아줌 → POD 경계가 코드리뷰가 아닌 게이트로 보장.

**측정 항목(현재)**: 드로우 콜 수(SRV 전환 횟수 = Flush 횟수)는 구조적으로 추적 가능하지만 아직 전용 카운터는 안 붙음 → "측정 예정".

---

## 5. 최적화

### 5.1 실제로 한 것

- **SRV 기반 배칭**: 같은 텍스처 사각형을 한 Draw로 묶음(`UIRenderer.cpp:500-505, 165-186`). 체력바 다수·아틀라스 스프라이트를 아틀라스로 모으면 드로우 콜이 SRV 종류 수로 수렴.
- **MAP_WRITE_DISCARD 동적 VB**: 매 프레임 VB를 DISCARD로 매핑해 GPU와 CPU가 같은 버퍼를 두고 stall하지 않게 함(`:171, :319`).
- **`ReserveQuads`로 정점 vector 사전 reserve**(`:473-484`) + `Begin`에서 capacity 4096 보장(`:426-427`) → 매 프레임 재할당 방지.
- **resize+포인터 채우기**: `DrawImage`가 `push_back` 6번 대신 `resize` 후 포인터로 6 정점을 한 번에 채움(`:516-524`) → 분기·재할당 최소화.
- **직교 전용 경량 셰이더**: 풀 MVP 행렬 cbuffer 대신 screenSize 2 float만 넘겨 VS 연산 최소화.

> 정량 수치: 정직성 지도에 UI 전용 FPS/드로우콜 수치는 없음 → **"측정 예정"**. (전체 엔진은 "작은 씬에서 ~94 드로우콜, CPU 바운드"가 캡처돼 있으나 UI 단독 분해는 안 했다.)

### 5.2 계획 중인 최적화

- **드로우 콜 카운터 추가**: Flush 횟수/정점 수를 프로파일러 스코프(도메인 12 인프라)에 노출해 "아틀라스 분할이 실제로 드로우 콜을 줄였는가"를 *측정으로* 증명.
- **인덱스 버퍼 도입**: 현재 사각형당 6 정점(중복 2개). IB를 쓰면 4 정점+6 인덱스로 정점 대역폭 33% 절감 — 단, UI 정점 수가 병목이 아닐 가능성이 높아 *측정 후* 결정(over-engineering 경계).
- **retained 정적 화면 캐싱**: 상점/스코어보드처럼 대부분 정적인 화면을 위젯 트리+invalidation으로 옮겨 매 프레임 풀 재제출 제거(§6 참조).

---

## 6. 구현 예정 (Planned) — 동일 깊이

### 6.1 Retained-mode 위젯 트리 (`CUIRoot`/`CUIObject`/위젯)

**무엇을**: `.md/architecture/WINTERS_UI_PIPELINE_ARCHITECTURE.md`의 `Engine/UI/Core`(UIRect/UIAnchor/CUIObject/CUIRoot/UIRenderContext) + `Widgets`(CUIImage/CUIText/CUIProgressBar/CUIIcon/CUIPanel)를 실제 코드로 구현. **현재 코드 0줄, 문서만.**

**왜(어떤 문제)**: immediate-mode는 매 프레임 모든 위젯을 풀 재계산·재제출한다. 상점·스코어보드·인벤토리처럼 *프레임 간 거의 안 변하는* 화면이 늘면 이게 낭비다. 또 hit-test(마우스가 어느 위젯 위인가), 레이아웃 중첩(패널 안 패널), zOrder 관리가 immediate에선 손으로 짜야 한다.

**어떻게(설계·자료구조·단계)**:
1. `CUIObject`: 부모/자식 포인터, `UIRect`(로컬 위치/크기), `UIAnchor`(부모 기준 정렬), `zOrder`, `bVisible`, `bDirty`.
2. `CUIRoot`: 트리 루트. `Layout()`(앵커→절대 픽셀 rect 계산, 부모→자식 top-down), `Paint(UIRenderContext&)`(zOrder 정렬 후 `CUIRenderer::DrawImage` 호출로 환원), `HitTest(x,y)`(자식→부모, zOrder 역순).
3. `UIRenderContext`: 기존 `CUIRenderer`를 감싸는 어댑터(문서 §"현재 코드와의 관계"대로 `CUIRenderer`는 이동 안 하고 컨텍스트가 감쌈) → **기존 배치 렌더러를 페인트 백엔드로 그대로 재사용**.
4. invalidation: `bDirty` 노드만 재레이아웃/재페인트. 단, 현재 immediate 경로와 *공존*시키며 정적 화면부터 점진 이관.

**예상 트레이드오프**: 트리 유지·dirty 전파 코드가 늘고, 상태→위젯 property binding이 새 버그 표면이 됨(immediate가 회피했던 바로 그것). → *전 화면을 한 번에 옮기지 않고* 정적 화면(상점)부터 이관해 위험을 격리.

**검증**: (a) 페인트 시 드로우 콜이 immediate와 동일한지(같은 배칭으로 환원되는지) 카운터 비교, (b) hit-test 단위 테스트(좌표→예상 위젯), (c) dirty 한 노드만 바꿨을 때 재페인트 위젯 수가 1인지 로그(Ch10 문서의 "invalidation hit rate" 기대 로그와 동일 컨셉, `.md/문서/10_Ch10_UI.md:333-334`).

### 6.2 자체 런타임 폰트 렌더러 (ImGui 텍스트 탈피)

**무엇을**: 현재 텍스트는 ImGui `ImFont`/`ImDrawList`. 이를 자체 SDF(또는 비트맵) 글리프 아틀라스 + `CUITextRenderer`로 대체.

**왜**: 텍스트가 ImGui에 묶여 있어 (a) HUD 텍스트가 ImGui lib 라이프사이클·룩에 종속, (b) 텍스트와 이미지가 *다른 배치 경로*라 드로우 콜이 섞임, (c) 릴리즈 빌드에서 ImGui 의존 제거 불가.

**어떻게**: 폰트를 글리프 아틀라스 텍스처(SDF면 스케일 독립)로 베이크 → 문자열을 글리프 쿼드 시퀀스로 셰이핑(커닝·줄바꿈) → **기존 `CUIRenderer::DrawImage`로 글리프 쿼드 제출** → 텍스트도 이미지와 같은 배치 경로로 통합. 자료구조: `GlyphInfo{ uvRect, advance, bearing }` 맵 + 아틀라스 SRV.

**트레이드오프**: 유니코드 폴백·복잡 스크립트·이모지·커닝은 ImGui가 공짜로 주던 것 → 직접 하면 비용 큼. 그래서 **한국어/라틴 고정 글리프셋 + SDF**로 범위를 좁혀 시작. 신입 1인 범위에선 *지금 당장은 안 함*이 맞는 결정.

**검증**: 동일 문자열을 ImGui와 자체 렌더러로 그려 픽셀/레이아웃 비교(골든 이미지), 드로우 콜이 이미지 경로와 합쳐졌는지 카운터.

### 6.3 레이아웃/스타일/룰 JSON 분리

**무엇을**: `*_layout.json`(위치/앵커/zOrder), `*_style.json`(색/alpha/폰트/margin), `*_rules.json`(표시 조건/dead overlay)로 분리(문서 §"데이터 책임"). 현재는 atlas_manifest(rect)만 JSON, 레이아웃은 코드/하드코드 fallback.

**왜**: 기획·디자인 값(위치·색·표시 규칙)을 코드에서 빼야 디자이너가 코드 빌드 없이 튜닝. truth는 여전히 Server/GameSim 소유(UI는 표현만).

**어떻게**: 기존 `JsonCursor` 패턴 재사용해 `UILayoutDocument`/`UIStyleSheet` 로더 추가 → `CUIObject` 트리에 주입. hot reload는 후속(파일 watch).

**트레이드오프**: 런타임 파싱 비용(로드 1회라 무시 가능), 스키마 버저닝 부담. → 매니페스트와 동일하게 단순 고정 스키마로 시작.

**검증**: JSON 값 변경→재로드 시 위치/색이 반영되는지 시각 + 누락 키 fallback 로그.

---

## 7. 면접 예상 질문 & 모범 답변 (12개)

**Q1. (기본) UI 렌더링이 게임 3D 렌더링과 뭐가 다른가요?**
A. UI는 거의 다 2D 직교 투영이라 풀 MVP 행렬이 필요 없고 screenSize 2개만으로 픽셀→NDC 변환합니다(VS에서 y를 뒤집죠). 깊이 테스트를 끄고 그린 순서가 곧 zOrder(painter's algorithm)이며, 알파 블렌딩이 항상 켜집니다. 또 UI는 게임 메인 패스 *뒤에* 같은 디바이스 컨텍스트에 끼어들기 때문에 상태 save/restore가 필수입니다. 제 `CUIRenderer`의 `Begin`/`End`가 그 백업·원복을 합니다.

**Q2. (기본) 배칭은 어떻게 구현했나요? 드로우 콜이 왜 중요하죠?**
A. `DrawImage`는 즉시 GPU를 때리지 않고 CPU 정점 벡터에 사각형 6 정점을 쌓습니다. **SRV(텍스처)가 바뀌는 순간에만 Flush**해서 같은 텍스처의 사각형 전부를 Draw 한 번으로 제출합니다(`UIRenderer.cpp:500-505`). 드로우 콜마다 CPU가 드라이버 호출 비용을 내므로, 텍스처를 아틀라스로 모아 SRV 전환을 줄이면 드로우 콜이 텍스처 종류 수로 수렴합니다. 그래서 아틀라스 매니페스트가 존재합니다.

**Q3. (설계) immediate-mode를 택한 이유는? retained가 더 좋지 않나요?**
A. 트레이드오프 선택입니다. HUD는 HP·쿨다운·추적 체력바처럼 매 프레임 거의 모든 값이 변해서 retained의 invalidation(바뀐 것만 다시 그림) 이득이 작습니다. 반대로 immediate는 매 프레임 truth에서 새로 그리니 상태-UI 동기화 버그가 구조적으로 안 생깁니다. 다만 상점·스코어보드처럼 정적인 화면이 늘면 retained 이득이 커지므로, 위젯 트리(`CUIRoot`)는 *그 화면들부터* 도입할 계획입니다. 전 화면을 한 번에 옮기는 건 over-engineering이라 안 합니다.

**Q4. (설계) UI가 게임 상태를 직접 읽으면 안 되는 이유는?**
A. 두 가지입니다. 첫째 의존성: Engine UI가 `ChampionComponent`나 `Shared/GameSim`을 직접 알면 Engine이 Client/게임 룰에 종속돼 재사용이 깨집니다. 그래서 Client가 ECS에서 값을 긁어 `ActorHUDState`/`UIWorldHealthBarDesc` 같은 평평한 POD로 추려 단방향으로 넘기고(`Scene_InGame.cpp:317-469`), Engine UI는 그 POD만 압니다. 둘째 권위: UI는 표현 계층이지 truth가 아니므로, 절대 truth를 쓰지 않습니다. truth는 서버/GameSim 소유입니다.

**Q5. (설계) Save/RestoreState가 왜 그렇게 많은 상태를 백업하나요?**
A. UI 렌더러가 게임 렌더러와 *같은* `ID3D11DeviceContext`를 공유하기 때문입니다. UI가 IA layout·VB·VS/PS·blend·depth·raster·sampler·SRV를 자기 것으로 바꿔놓고 원복하지 않으면, 다음에 그 컨텍스트로 그리는 렌더러가 깨집니다. 그래서 `Begin`에서 `SaveState`로 전부 Get해 백업하고 `End`의 `RestoreState`에서 되돌립니다(`UIRenderer.cpp:188-249`). immediate 렌더러를 공유 컨텍스트에 안전하게 끼우는 정석입니다.

**Q6. (압박/레드플래그) "자체 UI 프레임워크를 만들었다"고 했는데, 텍스트도 직접 그리나요?**
A. 아닙니다. 정확히 말하면 **스프라이트/이미지 계층은 자체 DX11 렌더러, 텍스트는 ImGui 어댑터**입니다. 체력바·커서·HUD 배경·오버레이는 제 `CUIRenderer::DrawImage`로 그리지만, 텍스트·숫자·상점 그리드·스코어보드는 ImGui `ImDrawList`와 ImGui 폰트(`Font_Manager`의 `AddFontFromFileTTF`)에 의존합니다. 자체 런타임 폰트 렌더러는 아직 없고, SDF 글리프 아틀라스로 분리하는 게 계획입니다. 하이브리드는 미완성이 아니라 *의도된 분업*입니다 — 글리프 인프라를 1인 범위에서 지금 만드는 건 우선순위가 낮습니다.

**Q7. (압박/레드플래그) 아키텍처 문서에 `CUIRoot`, `CUIObject`, 위젯 트리가 있던데 그건 구현됐나요?**
A. 아닙니다. **위젯 트리는 설계 문서만 있고 코드는 0줄**입니다. 현재 라이브는 레거시 `UI_Manager`의 immediate 경로입니다. 정직하게 그어두는 이유는, HUD가 매 프레임 거의 다 변해서 트리의 invalidation 이득이 작기 때문입니다. 트리는 상점·스코어보드 같은 정적 화면부터 도입할 거고, 그때 기존 `CUIRenderer`를 `UIRenderContext`로 감싸 *페인트 백엔드로 재사용*합니다. 무엇을·왜·어떻게·어떻게 검증할지는 §6에 설계해 뒀습니다.

**Q8. (압박/레드플래그) Lua로 UI를 만든다고 했는데 실제 게임에서 돌아가나요?**
A. `LuaUIHost`는 **prototype**입니다. Lua 5.4 위에서 스크립트가 `CUIRenderer`·매니페스트·폰트를 호출해 UI를 구성하는 실험 경로이고, 라이브 HUD 주 경로는 아닙니다. 인게임 HUD는 C++ `UI_Manager`가 그립니다. Lua는 "UI를 데이터/스크립트로 외부화할 수 있는가"를 검증하는 PoC로만 봐주시면 됩니다.

**Q9. (압박) UI 자동 테스트나 골든 검증이 있나요?**
A. 솔직히 **자동 UI 골든/스모크 게이트는 없습니다.** 검증은 (1) 로드 실패 시 hr·경로·cwd를 `OutputDebugStringW`로 찍어 추적(`UI_Manager.cpp:727`), (2) 매니페스트·레이아웃 실패 시 built-in fallback으로 HUD가 사라지지 않게 분리 진단, (3) Save/Restore 누락 시 다음 프레임 게임 렌더가 시각적으로 깨져 회귀가 바로 보임, (4) 협업 하니스의 의존성 audit이 Engine UI의 POD 경계를 게이트로 보장 — 이 수준입니다. 다음으로 드로우 콜 카운터를 프로파일러에 붙여 "아틀라스 분할이 드로우 콜을 실제로 줄였는가"를 측정으로 증명하려 합니다.

**Q10. (심화) JSON 파서를 직접 짰는데 외부 라이브러리를 안 쓴 이유는? 견고성은?**
A. 매니페스트 스키마가 textures/sprites로 고정·단순하고, UI는 로드 1회 경로라 외부 의존(빌드·DLL 경계 부담)을 들일 가치가 낮았습니다. ECS·FX에서도 같은 `JsonCursor` 패턴을 재사용합니다. 견고성은 인정합니다 — escape는 처리하지만 완전한 유니코드·에러 복구는 약하고, 잘못된 JSON이면 false 반환 후 fallback 경로로 떨어집니다. 외부 입력이 아니라 *내가 만든 에셋*을 읽는 경로라 이 트레이드오프가 합리적이라고 봅니다. 사용자 입력 JSON이었다면 검증을 강화했을 겁니다.

**Q11. (심화) 같은 엔진인데 ECS는 offline codegen, UI는 런타임 JSON입니다. 왜 반대죠?**
A. *빈도*가 결정 기준이기 때문입니다. ECS Def는 매 프레임 시뮬 경로에서 조회되므로 string lookup을 없애려 offline codegen으로 dense index를 만듭니다. UI 아틀라스는 씬 로드 때 1회만 파싱되므로 런타임 JSON 파싱 비용이 무시 가능하고, 대신 코드 빌드 없이 디자이너가 교체하는 협업 가치가 큽니다. 같은 도구도 *어디서 얼마나 자주 쓰이는가*에 따라 반대 결정을 내리는 게 맞습니다.

**Q12. (확장) 드로우 콜을 더 줄이려면? 다음에 뭘 하겠습니까?**
A. 순서대로입니다. (1) **먼저 측정**: Flush 횟수·정점 수 카운터를 프로파일러에 붙여 UI 드로우 콜이 실제 병목인지 확인 — 측정 없이 최적화하면 over-engineering입니다. (2) 병목이면 **아틀라스 통합**으로 SRV 전환을 줄여 Flush를 합칩니다(이미 배칭 구조는 있음). (3) 정점 대역폭이 문제면 인덱스 버퍼로 사각형당 6→4 정점. (4) 정적 화면은 retained 트리로 옮겨 매 프레임 재제출 자체를 제거. 전체 엔진은 이미 "작은 씬에서도 CPU 바운드(~94 드로우콜)"라고 캡처돼 있어, UI 드로우 콜 절감이 CPU 여유로 직결됩니다.

---

## 8. 30초 엘리베이터 피치

"UI 파이프라인은 게임 truth를 매 프레임 화면 픽셀로 바꾸되, *UI는 truth를 읽기만 하고 절대 쓰지 않는* 단방향 흐름이 핵심입니다. 그래서 DX11 위에 직접 만든 배치 스프라이트 렌더러를 두고, 같은 텍스처 사각형을 드로우 콜 하나로 묶고, 게임 컨텍스트에 끼어들 땐 DX11 상태를 save/restore로 안전하게 원복합니다. 에셋은 JSON 아틀라스 매니페스트로 빼서 코드 빌드 없이 교체하고, Client는 ECS 값을 평평한 POD로 추려 Engine UI에 넘겨 의존성을 단방향으로 끊었습니다. 정직하게 말하면 *이미지 계층은 자체, 텍스트는 아직 ImGui 어댑터*고, retained 위젯 트리와 자체 폰트 렌더러는 정적 화면부터 점진 도입하는 게 설계돼 있습니다. 화려한 위젯 라이브러리보다 *배칭·상태 안전·단방향 데이터 흐름*이라는 렌더 시스템의 본질을 직접 짠 게 제 강점입니다."
