Session - F9 미니맵 조정값을 S020 단일 projection extent로 연결해 렌더·클릭·카메라 박스·FoW가 함께 움직이게 한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Public/UI/MinimapPanel.h

기존 코드:

```cpp
    const MinimapProjection& GetDefaultMinimapProjection();
```

아래로 교체:

```cpp
    // Legacy-named accessor for the currently applied runtime projection.
    // Startup/reset uses the S020 canonical uniform basis declared above.
    const MinimapProjection& GetDefaultMinimapProjection();
```

`class CMinimapPanel`의 public 영역에서 기존 코드:

```cpp
        static bool_t PrewarmChampionPortrait(eChampion champion);
        static void PrewarmChampionPortraits();
        static void DrawTunerImGui();
```

아래로 교체:

```cpp
        static bool_t PrewarmChampionPortrait(eChampion champion);
        static void PrewarmChampionPortraits();
        static bool_t DrawTunerImGui(
            bool_t bProjectionSyncAvailable,
            MinimapProjection& OutAppliedProjection);
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp

anonymous namespace의 `kUVFull` 바로 아래 기존 projection/layout 상수와 상태 블록을 아래로 교체:

```cpp
    constexpr f32_t kCanonicalProjectionCenterX = 104.50f;
    constexpr f32_t kCanonicalProjectionCenterZ = 0.f;
    constexpr f32_t kCanonicalProjectionExtent = 94.385f;
    constexpr f32_t kMinProjectionExtent = 70.f;
    constexpr f32_t kMaxProjectionExtent = 160.f;

    struct MinimapRuntimeLayout
    {
        f32_t ViewportHeightRatio = 0.35f;
        f32_t RightPadding = 12.f;
        f32_t BottomPadding = 12.f;
        f32_t IconScale = 1.f;
        f32_t ChampionScale = 2.f;
    };

    std::unique_ptr<Engine::CTexture> s_pMinimapBaseTexture;
    const UI::MinimapProjection kDefaultProjection{};
    UI::MinimapProjection s_RuntimeProjection = kDefaultProjection;
    f32_t s_fRuntimeProjectionExtent = kCanonicalProjectionExtent;
    u32_t s_iRuntimeProjectionRevision = 0u;
    MinimapRuntimeLayout s_MinimapLayout{};
    u64_t s_iPortraitLoadFailuresSinceLastRender = 0u;

    UI::MinimapProjection BuildUniformMinimapProjection(f32_t fExtent)
    {
        const f32_t fClampedExtent = std::clamp(
            fExtent, kMinProjectionExtent, kMaxProjectionExtent);
        UI::MinimapProjection Projection{};
        Projection.vWorldAtUv00 = {
            kCanonicalProjectionCenterX,
            kCanonicalProjectionCenterZ + fClampedExtent };
        Projection.vWorldAtUv10 = {
            kCanonicalProjectionCenterX + fClampedExtent,
            kCanonicalProjectionCenterZ };
        Projection.vWorldAtUv01 = {
            kCanonicalProjectionCenterX - fClampedExtent,
            kCanonicalProjectionCenterZ };
        return Projection;
    }

    void ApplyRuntimeMinimapProjection()
    {
        s_fRuntimeProjectionExtent = std::clamp(
            s_fRuntimeProjectionExtent,
            kMinProjectionExtent,
            kMaxProjectionExtent);
        s_RuntimeProjection = BuildUniformMinimapProjection(
            s_fRuntimeProjectionExtent);
        ++s_iRuntimeProjectionRevision;
    }
```

삭제할 범위:
`ResolveChampionLaneScale` 함수 정의 전체를 삭제한다.

기존 코드:

```cpp
    const MinimapProjection& GetDefaultMinimapProjection()
    {
        return kDefaultProjection;
    }
```

아래로 교체:

```cpp
    const MinimapProjection& GetDefaultMinimapProjection()
    {
        return s_RuntimeProjection;
    }
```

`CMinimapPanel::DrawTunerImGui` 정의 전체를 아래로 교체:

```cpp
    bool_t CMinimapPanel::DrawTunerImGui(
        bool_t bProjectionSyncAvailable,
        MinimapProjection& OutAppliedProjection)
    {
        bool_t bProjectionChanged = false;
        OutAppliedProjection = s_RuntimeProjection;

        ImGui::SetNextWindowSize(ImVec2(380.f, 440.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(340.f, 280.f),
            ImVec2(560.f, 720.f));
        if (!ImGui::Begin("Minimap Layout"))
        {
            ImGui::End();
            return false;
        }

        ImGui::PushID("MinimapLayout");
        ImGui::PushItemWidth(-155.f);
        ImGui::SliderFloat(
            "Viewport Height Ratio",
            &s_MinimapLayout.ViewportHeightRatio,
            0.18f,
            0.55f,
            "%.2f");
        ImGui::SliderFloat(
            "Right Padding",
            &s_MinimapLayout.RightPadding,
            0.f,
            64.f,
            "%.0f px");
        ImGui::SliderFloat(
            "Bottom Padding",
            &s_MinimapLayout.BottomPadding,
            0.f,
            64.f,
            "%.0f px");
        ImGui::SliderFloat(
            "Icon Scale",
            &s_MinimapLayout.IconScale,
            0.65f,
            2.f,
            "%.2f");
        ImGui::SliderFloat(
            "Champion Scale",
            &s_MinimapLayout.ChampionScale,
            0.75f,
            3.f,
            "%.2f");
        ImGui::SeparatorText("World Projection (Top / Bottom)");
        ImGui::TextWrapped(
            "S020 uniform basis: one extent moves render, click, camera bounds, and FoW together.");

        ImGui::BeginDisabled(!bProjectionSyncAvailable);
        f32_t fEditedExtent = s_fRuntimeProjectionExtent;
        if (ImGui::SliderFloat(
                "World Extent",
                &fEditedExtent,
                kMinProjectionExtent,
                kMaxProjectionExtent,
                "%.3f world"))
        {
            s_fRuntimeProjectionExtent = fEditedExtent;
            ApplyRuntimeMinimapProjection();
            OutAppliedProjection = s_RuntimeProjection;
            bProjectionChanged = true;
        }
        ImGui::TextDisabled(
            "smaller = outward, larger = inward | revision %u",
            s_iRuntimeProjectionRevision);
        ImGui::PopItemWidth();

        if (ImGui::Button("Reset S020 Projection"))
        {
            s_fRuntimeProjectionExtent = kCanonicalProjectionExtent;
            ApplyRuntimeMinimapProjection();
            OutAppliedProjection = s_RuntimeProjection;
            bProjectionChanged = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Reset Panel / Icons"))
            s_MinimapLayout = {};
        if (!bProjectionSyncAvailable)
        {
            ImGui::TextColored(
                ImVec4(1.f, 0.45f, 0.25f, 1.f),
                "Projection/FoW sync unavailable");
        }
        ImGui::PopID();
        ImGui::End();
        return bProjectionChanged;
    }
```

기존 `ResolveIconRadius`와 `DrawIcon`에서는 `fMinimapU`, `fMinimapV`, top/bottom lane scale 계산을 삭제하고, 반지름을 `fPanelSide`, `IconScale`, `ChampionScale`만으로 계산한다. 모든 `DrawIcon` 호출은 마지막 인자로 `fSide`만 전달한다.

### 1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

기존 코드:

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

`CScene_InGame::OnImGui`의 `m_bShowUITuner` 블록에서 기존 코드:

```cpp
        CGameInstance::Get()->UI_OnImGui_Tuner();
        CGameInstance::Get()->UI_OnImGui_StatusPanelLayoutTuner();
        UI::CMinimapPanel::DrawTunerImGui();
```

아래로 교체:

```cpp
        CGameInstance::Get()->UI_OnImGui_Tuner();
        CGameInstance::Get()->UI_OnImGui_StatusPanelLayoutTuner();
        UI::MinimapProjection AppliedProjection{};
        if (UI::CMinimapPanel::DrawTunerImGui(
                m_pVisionSystem != nullptr,
                AppliedProjection) &&
            m_pVisionSystem)
        {
            Engine::CVisionSystem::FowProjection FowProjection{};
            FowProjection.vWorldAtUv00 = AppliedProjection.vWorldAtUv00;
            FowProjection.vWorldAtUv10 = AppliedProjection.vWorldAtUv10;
            FowProjection.vWorldAtUv01 = AppliedProjection.vWorldAtUv01;
            m_pVisionSystem->SetFowProjection(FowProjection);
        }
```

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp

`CVisionSystem::SetFowProjection`에서 기존 코드:

```cpp
    m_FowProjection = Projection;
    m_bForceRebuild = true;
    m_bFowTextureDirty = true;
```

아래로 교체:

```cpp
    const bool_t bProjectionChanged =
        m_FowProjection.vWorldAtUv00.x != Projection.vWorldAtUv00.x ||
        m_FowProjection.vWorldAtUv00.y != Projection.vWorldAtUv00.y ||
        m_FowProjection.vWorldAtUv10.x != Projection.vWorldAtUv10.x ||
        m_FowProjection.vWorldAtUv10.y != Projection.vWorldAtUv10.y ||
        m_FowProjection.vWorldAtUv01.x != Projection.vWorldAtUv01.x ||
        m_FowProjection.vWorldAtUv01.y != Projection.vWorldAtUv01.y;

    m_FowProjection = Projection;
    if (bProjectionChanged)
    {
        // Explored texels are projection-space history and cannot be reused
        // after the world-to-UV basis changes without a reprojection pass.
        std::fill(m_vecFowTexture.begin(), m_vecFowTexture.end(), 0u);
    }
    m_bForceRebuild = true;
    m_bFowTextureDirty = true;
```

## 2. 검증

1. `git diff --check -- Client/Private/UI/MinimapPanel.cpp Client/Public/UI/MinimapPanel.h Client/Private/Scene/Scene_InGameImGui.cpp Engine/Private/ECS/Systems/VisionSystem.cpp .md/plan/2026-07-14_MINIMAP_F9_SCALING_FAILURE_AND_FIX_PLAN.md`가 오류 없이 끝나는지 확인한다.
2. Client의 `MinimapPanel.cpp`, `Scene_InGameImGui.cpp`와 Engine의 `VisionSystem.cpp`를 Debug x64 define/include 환경에서 `cl.exe /Zs`로 검사한다.
3. extent 70, 94.385, 160 각각에서 두 projection basis 길이가 같고 dot product가 0이며 world→UV→world 오차가 `1e-4` 이하인지 확인한다.
4. 새 Client Debug 빌드로 같은 경기 장면에 접속해 F9 `World Extent`를 70→94.385→160으로 바꾼다. top/bottom 포함 전체 아이콘, 클릭 이동점, 카메라 박스, FoW 경계가 같은 방향과 비율로 이동해야 한다.
5. `Reset S020 Projection`을 누르면 extent 94.385와 세 canonical anchor로 복구되어야 한다.
6. 실행 중인 서버는 유지하고 Client만 새 빌드로 재접속한다. 런타임 A/B 캡처 전에는 최종 PASS로 판정하지 않는다.
