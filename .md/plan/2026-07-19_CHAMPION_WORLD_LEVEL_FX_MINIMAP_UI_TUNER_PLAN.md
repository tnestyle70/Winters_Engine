Session - 모든 챔피언 월드 체력바 왼쪽에 레벨을 표시하고 아군 레벨업 FX와 미니맵·레벨 UI 튜너를 F8 흐름에 연결한다.
좌표: 없음 · 축: C4 수명은 선언된다, C7 권위와 정합성
관련: 2026-07-14_MINIMAP_F9_SCALING_FAILURE_AND_FIX_PLAN.md, 2026-07-19_MINION_WAVE_PROGRESS_FACING_RECALL_SCALE_VIEGO_R_RESET_PLAN.md / RESULT.md

## 1. 결정 기록

① 문제·제약: 권위 레벨은 snapshot 즉시 갱신되지만 월드 바 전달 필드가 0개이고, 미니맵 기본 비율은 0.37이며 기존 튜너 호출도 0회다. 레벨업 FX는 정확히 1.0초여야 한다.
② 순진한 해법의 실패: Engine UI가 Client 미니맵을 직접 소유하면 의존 방향이 뒤집히고, 패널 확대만으로 정규화 좌표 오차가 고쳐진다고 단정할 수도 없다.
③ 메커니즘: 레벨은 Client snapshot→Engine presentation DTO로 전달하고, FX는 최초 관측을 baseline으로 둔 뒤 보이는 아군의 증가 delta에만 지면 고정 `Recall.Channel`을 재생한다.
④ 대조: 별도 LevelUp 서버 이벤트를 추가하지 않고 이미 권위적인 snapshot delta를 시각 전용 트리거로 쓴다. F8은 Engine UI 창과 Client 소유 미니맵 창을 함께 연다.
⑤ 대가: 런타임 슬라이더 값은 세션 종료 후 저장되지 않고 compiled default만 유지된다. 0.39 확대는 사용자가 명시적으로 선택한 “좌표 오차를 감수하고 패널 크기만 확대” 종결안이다. 아이콘/PNG의 상대 정규화 좌표는 그대로라 수학적 정렬 개선을 합격 조건으로 주장하지 않는다.
- 예산: 구현·빌드 70%, normal F5에서 레벨/FX/미니맵을 캡처하는 시각 검증 30%를 분리한다.

## UI 작업 계약

| 항목 | 범위 | 소유권 |
|---|---|---|
| 월드 레벨 | 모든 엄격 가시 챔피언 체력바 왼쪽, 기본 X=-24px/Y=1px/scale=0.85 | Client visibility bridge → Engine UI presentation |
| 레벨업 FX | snapshot 최초 관측은 무음, 이후 증가 시 보이는 아군만, 지면 고정 1.0초 | Client visual bridge |
| 미니맵 | 기본 화면 높이 비율 0.39, 기존 layout/projection 튜너 재연결 | Client `CMinimapPanel` |
| 비범위 | 서버 레벨 계산, 미니맵 PNG/UV 재제작, 적 FoW 정보 노출, 튜너 영구 저장 | 변경 없음 |

```text
F8
┌ UI Manager ──────────────────────────────┐
│ [HUD] [Health Bars] [Cursor]             │
│ Target: Champion                         │
│ Width / Height / Y Offset                │
│ ─ Champion Level ─                       │
│ Level X / Level Y / Level Font Scale     │
│ [Reset Champion Level]                   │
└──────────────────────────────────────────┘

┌ UI Manager - Minimap ────────────────────┐
│ Viewport Height Ratio  0.39              │
│ Right / Bottom Padding                   │
│ Icon / Champion Scale                    │
│ World Extent (FoW 동기화 가능 시)         │
│ [Reset S020 Projection] [Reset Panel]    │
└──────────────────────────────────────────┘
```

- 행동 예산: F8 1회로 두 소유 창을 열고, 각 값은 slider drag 1회에 즉시 preview한다. 취소/복구는 각 Reset 버튼 1회다.
- 상태 수명: 패널/레벨 튜닝은 session-local draft, 0.39와 레벨 기본값은 compiled default다. 파일 저장 동작은 이번 범위에 추가하지 않는다.
- 피드백: 레벨은 동일 프레임 월드 바에, 미니맵은 동일 프레임 패널에 반영한다. FX delta 감지는 snapshot entity 적용이 끝난 같은 프레임 `OnLateUpdate`에서 1회 수행한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/WorldHealthBarState.h

기존 코드:

```cpp
        f32_t fWidthScale = 1.f;
        u8_t iTeam = 255u;
        bool_t bDead = false;
```

아래로 교체:

```cpp
        f32_t fWidthScale = 1.f;
        u8_t iLevel = 0u;
        u8_t iTeam = 255u;
        bool_t bDead = false;
```

### 2-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`SyncWorldHealthBarsToEngineUI`의 기존 시작 코드:

```cpp
    std::vector<Engine::UIWorldHealthBarDesc> Bars;
    Bars.reserve(128u);

    auto ApplyHealthOverride = [this](
```

아래로 교체:

```cpp
    std::vector<Engine::UIWorldHealthBarDesc> Bars;
    Bars.reserve(128u);

    u8_t iLocalTeam = ToLoLUITeamId(m_PlayerTeam);
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        iLocalTeam = ToLoLUITeamId(
            m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
    }

    auto ApplyHealthOverride = [this](
```

챔피언 lambda의 기존 코드:

```cpp
            if (UI::IsKalistaCarried(m_World, Entity) ||
                m_World.HasComponent<YoneSoulPresentationTag>(Entity))
                return;
```

아래로 교체:

```cpp
            if (UI::IsKalistaCarried(m_World, Entity) ||
                m_World.HasComponent<YoneSoulPresentationTag>(Entity) ||
                !UI::IsRenderableForLocal(
                    m_World,
                    Entity,
                    iLocalTeam,
                    false,
                    false))
            {
                return;
            }
```

`SyncWorldHealthBarsToEngineUI` 챔피언 bar 구성의 기존 코드:

```cpp
            Bar.fManaCurrent = Champion.mana;
            Bar.fManaMaximum = Champion.maxMana;
```

아래로 교체:

```cpp
            Bar.fManaCurrent = Champion.mana;
            Bar.fManaMaximum = Champion.maxMana;
            Bar.iLevel = Champion.level;
```

함수 끝의 기존 local team 재계산 block은 삭제하고 이미 계산된 `iLocalTeam`을 그대로 `UI_Set_WorldHealthBars`에 전달한다.

삭제할 코드:

```cpp
    u8_t iLocalTeam = ToLoLUITeamId(m_PlayerTeam);
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        iLocalTeam = ToLoLUITeamId(m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
    }
```

### 2-2a. C:/Users/user/Desktop/Winters/Client/Public/Scene/RenderVisibilityFilter.h

기존 signature:

```cpp
    inline bool_t IsRenderableForLocal(
        CWorld& world,
        EntityID entity,
        u8_t localTeam,
        bool_t bIgnoreFogOfWar = false)
```

아래로 교체:

```cpp
    inline bool_t IsRenderableForLocal(
        CWorld& world,
        EntityID entity,
        u8_t localTeam,
        bool_t bIgnoreFogOfWar = false,
        bool_t bAllowDebugReveal = true)
```

기존 Debug reveal block:

```cpp
#if defined(_DEBUG) && !defined(WINTERS_DISABLE_AI_VISIBILITY_DEBUG)
        return true;
#else
```

아래로 교체:

```cpp
#if defined(_DEBUG) && !defined(WINTERS_DISABLE_AI_VISIBILITY_DEBUG)
        if (bAllowDebugReveal)
            return true;
#endif
```

그리고 기존 block 끝의 `#endif`를 삭제한다. 이하 `VisibilityComponent` mask 검사는 Debug/Release 모두 공통 실행한다.

기존 코드:

```cpp
        if (!world.HasComponent<VisibilityComponent>(entity))
            return true;
```

아래로 교체:

```cpp
        if (!world.HasComponent<VisibilityComponent>(entity))
            return bAllowDebugReveal;
```

월드 체력바가 넘기는 `bAllowDebugReveal=false`는 snapshot 조립 중 visibility가 아직 없는 적도 fail-closed로 숨긴다. 기존 호출의 기본값 `true`는 기존 fail-open 동작을 유지한다.

### 2-3. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
    f32_t   m_fHPBarWidth = 104.f;    // 화면 픽셀
    f32_t   m_fHPBarHeight = 20.f;
    f32_t   m_fHPBarYOffset = 2.75f;    // 월드 좌표 머리 위 높이 (m)
```

아래로 교체:

```cpp
    f32_t   m_fHPBarWidth = 104.f;    // 화면 픽셀
    f32_t   m_fHPBarHeight = 20.f;
    f32_t   m_fHPBarYOffset = 2.75f;    // 월드 좌표 머리 위 높이 (m)
    f32_t   m_fChampionLevelOffsetX = -24.f;
    f32_t   m_fChampionLevelOffsetY = 1.f;
    f32_t   m_fChampionLevelFontScale = 0.85f;
```

### 2-4. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`BuildHealthBarScreenRects` 아래에 추가:

```cpp
static void DrawChampionLevelText(
    ImDrawList* pDraw,
    ImFont* pFont,
    const HealthBarScreenRects& rects,
    u8_t iLevel,
    f32_t fOffsetX,
    f32_t fOffsetY,
    f32_t fFontScale)
{
    if (!pDraw || !pFont || iLevel == 0u)
        return;

    char levelText[4]{};
    sprintf_s(levelText, "%u", static_cast<u32_t>(iLevel));
    const f32_t fontSize = (std::max)(8.f, pFont->LegacySize * fFontScale);
    const ImVec2 position(
        rects.BarMin.x + fOffsetX,
        rects.BarMin.y + fOffsetY);
    UI_DrawOutlinedText(
        pDraw,
        pFont,
        fontSize,
        position,
        IM_COL32(245, 245, 238, 255),
        levelText);
}
```

`DrawHealthBars`의 기존 코드:

```cpp
    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
```

아래로 교체:

```cpp
    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
    ImFont* pLevelFont = FindUIFont("hud");
```

`DrawHealthBars`의 기존 마지막 두 draw 아래에 추가:

```cpp
        DrawChampionLevelText(
            pDraw,
            pLevelFont,
            rects,
            Bar.iLevel,
            m_fChampionLevelOffsetX,
            m_fChampionLevelOffsetY,
            m_fChampionLevelFontScale);
```

`DrawHealthBarBarcodeOverlay`의 기존 코드:

```cpp
    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
```

아래로 교체:

```cpp
    const f32_t w = m_fHPBarWidth;
    const f32_t h = m_fHPBarHeight;
    const f32_t yOff = m_fHPBarYOffset;
    ImFont* pLevelFont = FindUIFont("hud");
```

`DrawHealthBarBarcodeOverlay`의 기존 마지막 두 draw 아래에 추가:

```cpp
        DrawChampionLevelText(
            pDraw,
            pLevelFont,
            rects,
            Bar.iLevel,
            m_fChampionLevelOffsetX,
            m_fChampionLevelOffsetY,
            m_fChampionLevelFontScale);
```

`OnImGui_Tuner` Health Bars 탭의 구조물 전용 block 아래에 추가:

```cpp
            if (s_SelectedBarType == 0)
            {
                ImGui::SeparatorText("Champion Level");
                ImGui::SliderFloat(
                    "Level X", &m_fChampionLevelOffsetX,
                    -80.f, 20.f, "%+.0f px");
                ImGui::SliderFloat(
                    "Level Y", &m_fChampionLevelOffsetY,
                    -30.f, 30.f, "%+.0f px");
                ImGui::SliderFloat(
                    "Level Font Scale", &m_fChampionLevelFontScale,
                    0.5f, 2.f, "%.2f");
                if (ImGui::Button("Reset Champion Level"))
                {
                    m_fChampionLevelOffsetX = -24.f;
                    m_fChampionLevelOffsetY = 1.f;
                    m_fChampionLevelFontScale = 0.85f;
                }
            }
```

### 2-5. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    std::unordered_map<EntityID, NetworkActionAnimationState> m_NetworkActionAnimStates{};
    std::unordered_map<EntityID, EntityHandle> m_NetworkRecallFxHandles{};
```

아래로 교체:

```cpp
    std::unordered_map<EntityID, NetworkActionAnimationState> m_NetworkActionAnimStates{};
    std::unordered_map<EntityID, EntityHandle> m_NetworkRecallFxHandles{};
    std::unordered_map<EntityID, u8_t> m_NetworkChampionLevels{};
    u64_t m_uNetworkChampionLevelSnapshotTick = 0u;
```

기존 코드:

```cpp
    void StartNetworkRecallFx(EntityID entity);
    void StopNetworkRecallFx(EntityID entity);
    void ClearNetworkRecallFx();
```

아래로 교체:

```cpp
    void StartNetworkRecallFx(EntityID entity);
    void StopNetworkRecallFx(EntityID entity);
    void ClearNetworkRecallFx();
    void SyncNetworkChampionLevelUpFx();
```

### 2-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

`StartNetworkRecallFx` 바로 위에 추가:

```cpp
void CScene_InGame::SyncNetworkChampionLevelUpFx()
{
    u8_t localTeam = static_cast<u8_t>(m_PlayerTeam);
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.IsAlive(m_PlayerEntity) &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        localTeam = static_cast<u8_t>(
            m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
    }

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID entity,
            ChampionComponent& champion,
            TransformComponent& transform)
        {
            const auto [it, bInserted] =
                m_NetworkChampionLevels.try_emplace(entity, champion.level);
            if (bInserted)
                return;

            const u8_t previousLevel = it->second;
            it->second = champion.level;
            if (champion.level <= previousLevel)
                return;

            const bool_t bVisibleAlly =
                static_cast<u8_t>(champion.team) == localTeam &&
                UI::IsRenderableForLocal(
                    m_World,
                    entity,
                    localTeam,
                    false);
            if (!bVisibleAlly ||
                UI::IsKalistaCarried(m_World, entity) ||
                m_World.HasComponent<YoneSoulPresentationTag>(entity))
            {
                return;
            }

            if (m_World.HasComponent<HealthComponent>(entity))
            {
                const HealthComponent& health =
                    m_World.GetComponent<HealthComponent>(entity);
                if (health.bIsDead || health.fCurrent <= 0.f)
                    return;
            }

            FxCueContext cue{};
            cue.attachTo = NULL_ENTITY;
            cue.vWorldPos = transform.GetPosition();
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = 1.f;

            const eChampion championId = champion.id;
            const StatComponent fallbackStat =
                BuildDefaultChampionStat(championId);
            const f32_t recallRadius =
                GameplayQuery::ResolveAttackRangePreviewRadius(
                    m_World,
                    entity,
                    championId,
                    fallbackStat.attackRange,
                    m_bNetworkAuthoritativeGameplay);
            const f32_t diameter = recallRadius * 2.f * kRecallVisualScale;
            cue.bOverrideSize = true;
            cue.fWidthOverride = diameter;
            cue.fHeightOverride = diameter;
            const EntityID fxEntity =
                CFxCuePlayer::Play(m_World, "Recall.Channel", cue);

#if defined(_DEBUG)
            static u32_t s_levelUpFxTraceCount = 0u;
            if (s_levelUpFxTraceCount < 32u)
            {
                char message[224]{};
                sprintf_s(
                    message,
                    "[LevelUpFx] owner=%u previous=%u current=%u fx=%u fixed=1 lifetime=1.00 diameter=%.2f\n",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(previousLevel),
                    static_cast<u32_t>(champion.level),
                    static_cast<u32_t>(fxEntity),
                    diameter);
                OutputDebugStringA(message);
                ++s_levelUpFxTraceCount;
            }
#else
            (void)fxEntity;
#endif
        });
}
```

snapshot remove callback의 기존 코드:

```cpp
                StopNetworkRecallFx(entity);
                m_NetworkActionAnimStates.erase(entity);
```

아래로 교체:

```cpp
                StopNetworkRecallFx(entity);
                m_NetworkActionAnimStates.erase(entity);
                m_NetworkChampionLevels.erase(entity);
```

### 2-6a. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

snapshot entity 적용보다 먼저 호출되는 `OnAuthoritativeSnapshot`에는 레벨 FX 감지를 추가하지 않는다. 기존 LateUpdate:

```cpp
void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}
```

아래로 교체:

```cpp
void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
    if (!m_bNetworkAuthoritativeGameplay || !m_pSnapshotApplier)
        return;

    const u64_t snapshotTick =
        m_pSnapshotApplier->GetLastAppliedSnapshotTick();
    if (snapshotTick == 0u ||
        snapshotTick == m_uNetworkChampionLevelSnapshotTick)
    {
        return;
    }

    m_uNetworkChampionLevelSnapshotTick = snapshotTick;
    SyncNetworkChampionLevelUpFx();
}
```

이 위치는 해당 프레임의 network pump/snapshot entity apply 뒤다. `GetLastAppliedServerTick()`은 Hello에도 갱신될 수 있으므로 쓰지 않고 full snapshot 전용 `GetLastAppliedSnapshotTick()`만 쓴다. 최초 post-apply 관측과 timeline rebase 직후 최초 관측은 map baseline만 만들고 FX를 재생하지 않는다.

`RebaseNetworkTimeline`의 기존 코드:

```cpp
    m_NetworkActorInterpStates.clear();
    m_uNetworkActorInterpSnapshotTick = serverTick;
```

아래로 교체:

```cpp
    m_NetworkActorInterpStates.clear();
    m_NetworkChampionLevels.clear();
    m_uNetworkChampionLevelSnapshotTick = 0u;
    m_uNetworkActorInterpSnapshotTick = serverTick;
```

### 2-7. C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp

기존 코드:

```cpp
        f32_t ViewportHeightRatio = 0.37f;
```

아래로 교체:

```cpp
        f32_t ViewportHeightRatio = 0.39f;
```

기존 코드:

```cpp
        if (!ImGui::Begin("Minimap Layout"))
```

아래로 교체:

```cpp
        if (!ImGui::Begin("UI Manager - Minimap"))
```

### 2-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

기존 include:

```cpp
#include "Core/CInput.h"
#include "GameInstance.h"
```

아래로 교체:

```cpp
#include "Core/CInput.h"
#include "ECS/Systems/VisionSystem.h"
#include "GameInstance.h"
```

기존 include:

```cpp
#include "UI/MapTunerPanel.h"
#include "UI/RenderDebug.h"
```

아래로 교체:

```cpp
#include "UI/MapTunerPanel.h"
#include "UI/MinimapPanel.h"
#include "UI/RenderDebug.h"
```

F8 tuner의 기존 코드:

```cpp
        WINTERS_PROFILE_SCOPE("UI::Tuner");
        CGameInstance::Get()->UI_OnImGui_Tuner();
```

아래로 교체:

```cpp
        WINTERS_PROFILE_SCOPE("UI::Tuner");
        CGameInstance::Get()->UI_OnImGui_Tuner();
        UI::MinimapProjection appliedProjection{};
        if (UI::CMinimapPanel::DrawTunerImGui(
                m_pVisionSystem != nullptr,
                appliedProjection) &&
            m_pVisionSystem)
        {
            Engine::CVisionSystem::FowProjection fowProjection{};
            fowProjection.vWorldAtUv00 = appliedProjection.vWorldAtUv00;
            fowProjection.vWorldAtUv10 = appliedProjection.vWorldAtUv10;
            fowProjection.vWorldAtUv01 = appliedProjection.vWorldAtUv01;
            m_pVisionSystem->SetFowProjection(fowProjection);
        }
```

## 3. 검증

예측:
- Client snapshot 적용이 끝난 프레임에 엄격 가시 챔피언의 `UIWorldHealthBarDesc.iLevel`이 채워지고 RHI/ImGui 두 경로 모두 체력바 왼쪽에 같은 숫자를 한 번만 그린다. Debug reveal이 켜져도 FoW/은신 적 월드 바는 전달하지 않는다.
- 최초 snapshot은 FX를 만들지 않고, 이후 보이는 아군 레벨 증가 1회당 `[LevelUpFx] ... fixed=1 lifetime=1.00`이 1회 출력된다. 적·비가시·carried/soul은 재생하지 않는다.
- F8을 누르면 `UI Manager`와 `UI Manager - Minimap`이 함께 열리고 ratio 기본값 0.39 및 레벨 X/Y/font slider가 동일 프레임에 반영된다.
- 미니맵 panel은 기본 0.39로 커진다. 아이콘과 배경의 상대 정규화 좌표는 변하지 않으므로 탑/바텀 구조물의 수학적 좌표 정렬은 이번 합격 조건이 아니다.
- Engine→Client 의존은 추가하지 않는다. Client scene이 Engine UI와 Client minimap owner를 조율한다.

검증 명령:

```powershell
git diff --check

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsroot 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Winters.sln '/t:Build' /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
```

후속 동기화:
- Engine public header 변경은 Engine PostBuild/Client PreBuild가 루트 `UpdateLib.bat`으로 `EngineSDK/inc`를 자동 동기화한다. 생성물을 수동 편집하지 않는다.

## 수동 시각 QA 계약

- 실행 파일: `C:/Users/user/Desktop/Winters/Client/Bin/Debug/WintersGame.exe`
- 장면/흐름: 서버 권위 normal F5 → 로그인 → 메인 메뉴 → 5v5 InGame. 단축키는 `F8`이다.
- 최소 해상도/DPI: `CONFIRM_NEEDED` — 프로젝트의 공식 최소 해상도/DPI가 문서화되어 있지 않다. 이번 RESULT에는 실제 실행 창 해상도와 Windows 배율을 기록하고 그 환경에서 겹침/클리핑을 판정한다.
- 성공 상태: 가시 아군/적의 월드 체력바 왼쪽 레벨, F8 두 창과 즉시 slider 반영, 기본 0.39 패널, 아군 레벨 증가 시 지면에 고정되어 정확히 1초 종료하는 축소 `Recall.Channel`을 같은 플레이에서 확인한다.
- 실패/empty 상태: FoW/은신 적 또는 월드 바 대상이 없는 화면에서 레벨/FX가 나타나지 않고, VisionSystem 미연결이면 minimap projection 조작이 비활성/무변경인지 확인한다.
- 성공 캡처 산출물: `C:/Users/user/Desktop/Winters/.md/build/2026-07-19_CHAMPION_WORLD_LEVEL_MINIMAP_SUCCESS.png`
- 실패/empty 캡처 산출물: `C:/Users/user/Desktop/Winters/.md/build/2026-07-19_CHAMPION_WORLD_LEVEL_MINIMAP_EMPTY.png`
- FX 수명 산출물: `C:/Users/user/Desktop/Winters/.md/build/2026-07-19_CHAMPION_LEVEL_UP_FX_FIXED_1S.mp4` 또는 동등한 연속 프레임 캡처. 산출물을 만들 수 없는 실행 환경이면 RESULT에 `NOT_RUN`과 이유를 기록하고 자동 검증과 혼동하지 않는다.

미검증:
- 계획 시점에는 위 normal F5 수동 QA와 산출물 생성이 아직 수행되지 않았다.

확인 필요:
- 없음. 소스 적용 전 독립 비평 게이트만 남았다.

## 협업·변경 차선

- 구현 직전 관련 파일별 `git diff`와 계획 anchor를 다시 확인한다. 기존 승리/경제 작업과 진행 중인 정글 오브젝트 세션의 변경은 사용자 소유 변경으로 보존한다.
- `Scene_InGame.cpp`, `Scene_InGameNetwork.cpp`, `Scene_InGameImGui.cpp`, `UI_Manager.cpp`처럼 이미 dirty인 파일은 primary agent 한 명이 exact anchor의 최소 `apply_patch`만 적용한다. 관련 anchor가 달라졌거나 작업 중 파일이 다시 변하면 즉시 중단하고 최신 delta를 재검토한다.
- Engine public header는 원본만 수정하고 `EngineSDK` 생성물을 직접 수정하지 않는다. 전체 검증은 nav/게임심/UI를 합친 단일 MSBuild 차선에서 `/m:1 /nr:false`로 실행한다.
- 0.39는 panel enlargement acceptance다. 캡처에서 구조물 아이콘의 normalized offset이 남아도 이번 slice 실패로 판정하지 않으며, rect 확대·튜너 노출·클리핑 없음만 판정한다.

## 서브 에이전트 비평

- 1차 비평 주체: `/root/critique_nav_level_ui_plans` (read-only)
- 1차 판정: P0 0, P1 4, P2 3, 게이트 실패.
- 처분:
  - `OnAuthoritativeSnapshot`이 entity level 반영보다 빠르다는 P1을 수용했다. 감지를 post-apply `OnLateUpdate`로 옮기고 최초/rebase 관측은 baseline-only로 명시했다.
  - 0.39 확대가 normalized 좌표를 고치지 못한다는 P1은 사실로 수용하되, 사용자가 이미 “좌표 오차 감수 + 패널만 확대”를 선택했으므로 합격 조건을 panel enlargement로 명확히 낮췄다.
  - 실행 파일·장면·F8·성공/empty 캡처·산출물과 공식 최소 해상도/DPI의 `CONFIRM_NEEDED`를 수동 QA 계약에 추가해 P1을 수용했다.
  - dirty target 충돌 전략이 없다는 P1을 수용해 위 협업·변경 차선과 단일 빌드 차선을 추가했다.
  - 2차 비평의 “Hello 뒤 첫 full snapshot 전 baseline 생성” P1을 수용했다. full snapshot 전용 tick이 새로 적용된 LateUpdate에서만 동기화하고 rebase 때 map/tick을 함께 초기화한다.
- 2차 판정: P0 0, P1 1, 게이트 실패. 위 처분을 반영했다.
- 최종 재비평: residual P0 0, P1 0. full snapshot 전용 tick gate와 rebase map/tick 초기화를 확인했다.
- 엄격 가시성 fail-closed 델타 재비평: `bAllowDebugReveal=false`인 월드 체력바만 visibility component 누락 적을 숨기고 기본 호출은 기존 동작을 유지함을 확인했다. residual P0 0, P1 0.
- 상태: 독립 비평 게이트 PASS, 소스 적용 가능.
