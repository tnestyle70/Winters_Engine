# Winters UI Pipeline Architecture

작성일: 2026-06-04

목표: LoL HUD, Elden HUD, 에디터/튜너가 같은 UI 기반 위에 올라가도록 `Engine/UI`를 제품 중립 런타임 UI 계층으로 세운다. ImGui는 런타임 HUD의 주 렌더러가 아니라 튜너, 디버그, 에디터, 전환기 adapter로 둔다.

## 최상위 구조

```text
Engine/UI
├── Core
│   ├── UIRect
│   ├── UIAnchor
│   ├── CUIObject
│   ├── CUIRoot
│   └── UIRenderContext
│
├── Render
│   ├── CUIRenderer
│   ├── CUITextRenderer
│   └── CUIFontManager
│
├── Data
│   ├── UIJsonLoader
│   ├── UIAtlasManifest
│   ├── UILayoutDocument
│   └── UIStyleSheet
│
├── Widgets
│   ├── CUIImage
│   ├── CUIText
│   ├── CUIProgressBar
│   ├── CUIIcon
│   └── CUIPanel
│
└── Tools
    ├── UIInspector
    ├── LayoutTuner
    └── Validator
```

게임별 UI는 Engine UI 위에 얹는다.

```text
Client/LoL/UI
├── LoLHudScreen
├── ChampionHudWidget
├── MinimapWidget
├── ScoreboardWidget
├── ShopWidget
└── KillFeedWidget

Client/Elden/UI
├── EldenHudScreen
├── BossHealthWidget
├── LockOnWidget
├── StaminaWidget
└── ItemQuickSlotWidget
```

## 핵심 원칙

- Engine UI는 제품 의미를 모른다. 이미지, 텍스트, 패널, 진행바, 아이콘, 레이아웃, 스타일만 안다.
- LoL 미니맵, 챔피언 HUD, 상점, 스코어보드 같은 의미는 `Client/LoL/UI`가 소유한다.
- Elden 보스 HP, lock-on, stamina, quick slot 같은 의미는 `Client/Elden/UI`가 소유한다.
- `Shared/GameSim`과 Server는 UI를 모른다. gameplay truth를 Snapshot/Event/ViewState로 넘길 뿐이다.
- Runtime HUD는 `CUIRoot -> CUIObject -> Widget -> UIRenderContext -> CUIRenderer`로 그린다.
- ImGui는 tuner/debug/editor와 임시 text adapter에만 사용한다.
- ImGui 도구의 정보 구조, 행동 예산, 권위 적용, 시각 검증은 `WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md`를 따른다.
- 새 gameplay HUD는 `ImDrawList` 직접 호출로 시작하지 않는다.

## 데이터 책임

```text
*_layout.json
= 위치, 크기, anchor, pivot, zOrder

*_style.json
= 색, alpha, font, margin, animation feel

*_rules.json
= 표시 조건, visibility policy, dead overlay, UX rule

*_atlas_manifest.json
= texture path, sprite id, atlas rect
```

기획자와 디자이너가 만지는 값은 JSON/source data에 둔다. 실제 판정 truth는 Server/Shared/GameSim/FOW/Snapshot이 소유한다.

## 세션 분리

### Session 01. Engine UI Core / Render 루트

- `Engine/Public/UI/Core`
- `Engine/Public/UI/Render`
- `CUIRoot`, `CUIObject`, `UIRenderContext`, `CUITextRenderer`
- 기존 `CUI_Manager`에 runtime UI root 연결
- 기존 HUD는 유지하고 새 root만 통과시킨다.

### Session 02. Engine UI Data

- `UIJsonLoader`
- `UILayoutDocument`
- `UIStyleSheet`
- 기존 `UIAtlasManifest`를 `Engine/UI/Data` 방향으로 정리
- `ReadTextFileW`, `JsonCursor`, JSON writer 중복 제거

### Session 03. Engine UI Widgets

- `CUIImage`
- `CUIText`
- `CUIProgressBar`
- `CUIIcon`
- `CUIPanel`
- widget은 `CWorld`, `Shared/GameSim`, `ImGui`, `DX11` concrete type을 직접 알지 않는다.

### Session 04. Engine UI Tools

- `UIInspector`
- `LayoutTuner`
- `Validator`
- JSON save/export/hot reload
- ImGui는 이 세션에서 tool shell로 사용한다.

### Session 05. Product UI Entry

- `Client/LoL/UI/LoLHudScreen`
- `Client/Elden/UI/EldenHudScreen`
- Engine UI root에 제품별 screen을 등록하는 bridge 추가

### Session 06. 기존 LoL HUD 이관

- Champion HUD
- Shop
- Status panel
- Scoreboard
- Kill feed
- Damage/world text

### Session 07. Minimap

- `Client/LoL/UI/MinimapWidget`
- FOW, champion portrait, object, jungle, minion sync를 `MinimapFrameState`로 받고 `CUIObject`로 렌더

## 현재 코드와의 관계

- 현재 `Engine/Public/Manager/UI`는 살아 있는 레거시/전환기 UI manager 위치다.
- 새 제품 중립 UI 기반은 `Engine/Public/UI`와 `Engine/Private/UI`에 둔다.
- 기존 `CUIRenderer`는 이미 screen-space quad renderer 역할을 하므로 Session 01에서는 이동하지 않고 `UIRenderContext`가 감싼다.
- 기존 `CFont_Manager`는 ImGui font 기반이므로 `CUITextRenderer` adapter 뒤로 숨긴다. 완전한 runtime font renderer는 후속 세션에서 분리한다.
- 미니맵은 이 기반이 들어간 뒤 새 widget으로 다시 계획한다.
