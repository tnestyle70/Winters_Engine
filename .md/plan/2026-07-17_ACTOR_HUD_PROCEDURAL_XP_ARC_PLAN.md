Session - 챔피언 원형 초상화의 빈 XP 홈과 동일 좌표계에서 경험치 원호를 런타임 생성한다
좌표: 신규 좌표 후보 · 축: C1 · C8
관련: 2026-07-14_KALISTA_HUD_IRELIA_POSTFX_COMPLETION_REPORT.md, 2026-07-14_WINTERS_FULL_DATA_STRUCTURE_CUTOVER_ANALYSIS.md

## 1. 결정 기록

① 문제·제약: 같은 합성에서 프레임 `202×168 → 128×128`은 X/Y 배율이 `0.6337/0.7619`, XP 조각 `48×120 → 36×90`은 `0.75/0.75`라 5회 이상 위치·모양 수정이 수렴하지 않았다.
① 문제·제약: 튜너 저장 대상은 `actor_hud_layout.json`, 실제 로드 대상은 `hud_irelia_layout.json`으로 달라 저장 성공 문구 뒤에도 다음 실행에 수정값이 반영되지 않았다.
② 순진한 해법의 실패: fill/track 마스크 IoU `0.9975`는 두 고립 조각끼리만 같다는 증거이며, 프레임과의 합성 위치·비등방 배율은 검증하지 못한다. 단일 `width/height` 보정으로 서로 다른 좌표계를 동시에 맞출 수 없다.
③ 메커니즘: `CUIRenderer::DrawRingArc`가 흰 텍스처와 48분할 타원 링 메시를 생성하고, HUD JSON의 `rect/arc/color/colorEnd`와 `xpRatio`가 위치·두께·방향·그라데이션·채움량을 소유한다.
③ 메커니즘: XP atlas sprite 두 개와 `bottomToTop` 사각 클립을 제거하고, 실제 로드·저장 파일명을 `hud_irelia_layout.json` 하나로 통일한다.
④ 대조: 원본 fill PNG는 authored silhouette 보존에는 유리하지만 프레임과 독립 사각형으로 배치되는 한 부모 변환 불일치가 재발한다. 별도 픽셀 셰이더 마스크는 현재 단일 흰 텍스처 링보다 상태·검증 비용이 크다.
⑤ 대가: XP 원호 한 개당 최대 48구간·288정점을 추가한다. 빈 홈이 타원 호가 아닌 자유곡선임이 인게임 캡처로 확인되면 이 선택은 틀리며 그때는 프레임과 동일 UV 공간의 마스크 셰이더로 교체한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/UIRenderer.h

기존 코드:

```cpp
    void DrawImageCircle(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor,
        u32_t iSegmentCount = 48);
```

아래에 추가:

```cpp
    void DrawRingArc(
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        f32_t fStartRadians, f32_t fSweepRadians, f32_t fThickness,
        const Vec4& vStartColor,
        const Vec4& vEndColor,
        u32_t iSegmentCount = 48);
```

### 2-2. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/UIRenderer.cpp

`CUIRenderer::DrawImageCircle` 함수 바로 아래에 추가:

```cpp
void CUIRenderer::DrawRingArc(
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    f32_t fStartRadians, f32_t fSweepRadians, f32_t fThickness,
    const Vec4& vStartColor,
    const Vec4& vEndColor,
    u32_t iSegmentCount)
{
    if (!IsReady() || !m_pImpl->bInFrame ||
        fW <= 0.f || fH <= 0.f || fThickness <= 0.f ||
        std::abs(fSweepRadians) <= WintersMath::kEpsilon)
    {
        return;
    }

    ID3D11ShaderResourceView* pSRV = m_pImpl->pWhiteSRV.Get();
    if (!pSRV)
        return;

    iSegmentCount = std::clamp(iSegmentCount, 1u, 96u);
    const u32_t vertexCount = iSegmentCount * 6u;
    if (m_pImpl->pCurrentSRV != pSRV ||
        m_pImpl->vertices.size() + vertexCount > kMaxUIVertices)
    {
        m_pImpl->Flush();
        m_pImpl->pCurrentSRV = pSRV;
    }

    const f32_t cx = fX + fW * 0.5f;
    const f32_t cy = fY + fH * 0.5f;
    const f32_t outerRx = fW * 0.5f;
    const f32_t outerRy = fH * 0.5f;
    const f32_t innerRx = std::max(0.f, outerRx - fThickness);
    const f32_t innerRy = std::max(0.f, outerRy - fThickness);

    const auto lerpColor = [&](f32_t t) -> Vec4
    {
        return Vec4(
            WintersMath::Lerp(vStartColor.x, vEndColor.x, t),
            WintersMath::Lerp(vStartColor.y, vEndColor.y, t),
            WintersMath::Lerp(vStartColor.z, vEndColor.z, t),
            WintersMath::Lerp(vStartColor.w, vEndColor.w, t));
    };
    const auto makeVertex = [](f32_t px, f32_t py, const Vec4& color) -> UIVertex
    {
        return { px, py, 0.5f, 0.5f, color.x, color.y, color.z, color.w };
    };

    for (u32_t i = 0; i < iSegmentCount; ++i)
    {
        const f32_t t0 = static_cast<f32_t>(i) / static_cast<f32_t>(iSegmentCount);
        const f32_t t1 = static_cast<f32_t>(i + 1u) / static_cast<f32_t>(iSegmentCount);
        const f32_t a0 = fStartRadians + fSweepRadians * t0;
        const f32_t a1 = fStartRadians + fSweepRadians * t1;
        const f32_t c0 = static_cast<f32_t>(std::cos(a0));
        const f32_t s0 = static_cast<f32_t>(std::sin(a0));
        const f32_t c1 = static_cast<f32_t>(std::cos(a1));
        const f32_t s1 = static_cast<f32_t>(std::sin(a1));
        const Vec4 color0 = lerpColor(t0);
        const Vec4 color1 = lerpColor(t1);

        const UIVertex outer0 = makeVertex(cx + c0 * outerRx, cy + s0 * outerRy, color0);
        const UIVertex outer1 = makeVertex(cx + c1 * outerRx, cy + s1 * outerRy, color1);
        const UIVertex inner0 = makeVertex(cx + c0 * innerRx, cy + s0 * innerRy, color0);
        const UIVertex inner1 = makeVertex(cx + c1 * innerRx, cy + s1 * innerRy, color1);

        m_pImpl->vertices.push_back(outer0);
        m_pImpl->vertices.push_back(outer1);
        m_pImpl->vertices.push_back(inner1);
        m_pImpl->vertices.push_back(outer0);
        m_pImpl->vertices.push_back(inner1);
        m_pImpl->vertices.push_back(inner0);
    }
}
```

### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.h

기존 코드:

```cpp
            Vec4 vUV = Vec4(0.f, 0.f, 1.f, 1.f);
            bool_t bHasUV = false;
```

아래로 교체:

```cpp
            Vec4 vUV = Vec4(0.f, 0.f, 1.f, 1.f);
            Vec4 vColor = Vec4(1.f, 1.f, 1.f, 1.f);
            Vec4 vColorEnd = Vec4(1.f, 1.f, 1.f, 1.f);
            f32_t fArcStartDegrees = 0.f;
            f32_t fArcSweepDegrees = 0.f;
            f32_t fArcThickness = 0.f;
            bool_t bHasUV = false;
            bool_t bHasColor = false;
            bool_t bHasColorEnd = false;
```

`CActorHudPanel` private 상태에 기본 OFF인 `m_bPreviewXpRatio`와 `m_fPreviewXpRatio = 0.5f`를 추가한다. 레이아웃 튜너의 `ringArc` 편집부에서 preview checkbox/slider를 제공하고, `portrait.xp.arc.fill`에만 렌더 비율을 덮어쓴다. gameplay `ActorHUDState.XpRatio`는 수정하지 않는다.

### 2-4. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.cpp

`ResolveSourceLayoutPath`의 기존 코드:

```cpp
            std::wstring candidate = std::wstring(exePath) +
                L"..\\Resource\\UI\\actor_hud_layout.json";
```

아래로 교체:

```cpp
            std::wstring candidate = std::wstring(exePath) +
                L"..\\Resource\\UI\\hud_irelia_layout.json";
```

`ParseLayoutElement`에서 `shape` 처리 바로 아래에 추가:

```cpp
                else if (key == "arc")
                {
                    f32_t values[3] = {};
                    if (!ParseArray(json, values, 3))
                        return false;
                    out.fArcStartDegrees = values[0];
                    out.fArcSweepDegrees = values[1];
                    out.fArcThickness = values[2];
                }
                else if (key == "color")
                {
                    f32_t values[4] = {};
                    if (!ParseArray(json, values, 4))
                        return false;
                    out.vColor = Vec4(values[0], values[1], values[2], values[3]);
                    out.bHasColor = true;
                }
                else if (key == "colorEnd")
                {
                    f32_t values[4] = {};
                    if (!ParseArray(json, values, 4))
                        return false;
                    out.vColorEnd = Vec4(values[0], values[1], values[2], values[3]);
                    out.bHasColorEnd = true;
                }
```

`DrawLayoutTunerImGui`의 element rect 편집 블록 바로 아래에 원호·색 편집을 추가하고, `BuildLayoutJson`은 `shape == "ringArc"`일 때 `arc/color/colorEnd`를 직렬화한다. `SaveLayout` 성공 문구는 실제 단일 파일만 표현하도록 아래로 교체한다.

기존 코드:

```cpp
            m_strLastSaveMessage = "Saved layout JSON to runtime and source resources.";
```

아래로 교체:

```cpp
            m_strLastSaveMessage = "Saved hud_irelia_layout.json.";
```

`UseDefaultLayout`의 기존 XP 두 요소:

```cpp
        addElement("portrait.xp.arc.track", "portrait.xp.arc.track", nullptr, 252.f, 48.5f, 36.f, 90.f);
        addElement("portrait.xp.arc.fill", "portrait.xp.arc.fill", nullptr, 252.f, 48.5f, 36.f, 90.f, "xpRatio", nullptr, "bottomToTop");
```

아래로 교체:

```cpp
        addElement("portrait.face", nullptr, "portrait", 167.75f, 61.25f, 82.f, 82.f);
        m_Elements.back().strShape = "circle";
        addElement("portrait.frame", "portrait.frame", nullptr, 159.75f, 48.5f, 128.f, 128.f);
        LayoutElement xpArc{};
        xpArc.strID = "portrait.xp.arc.fill";
        xpArc.strBind = "xpRatio";
        xpArc.strShape = "ringArc";
        xpArc.Rect = { 155.25f, 37.f, 127.f, 152.f };
        xpArc.fArcStartDegrees = 38.f;
        xpArc.fArcSweepDegrees = -96.f;
        xpArc.fArcThickness = 8.5f;
        xpArc.vColor = Vec4(0.49f, 0.14f, 0.92f, 1.f);
        xpArc.vColorEnd = Vec4(0.82f, 0.44f, 1.f, 1.f);
        xpArc.bHasColor = true;
        xpArc.bHasColorEnd = true;
        m_Elements.push_back(std::move(xpArc));
```

위 교체에서 기존 `addElement("portrait", ...)` 한 줄도 함께 삭제한다. JSON 로드 실패 fallback도 `portrait.face → portrait.frame → procedural XP` 합성 순서를 유지한다.

`DrawElementRHI`에서 texture를 찾기 전에 `shape == "ringArc"`를 분기한다. `xpRatio`를 `Clamp01`하고 sweep에 곱한 뒤 `DrawRingArc`를 호출하며, 0%는 정점을 만들지 않는다. 사각형 `bottomToTop` clip 경로는 HP/MP 등 기존 요소를 위해 유지한다.

### 2-5. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
        OutputDebugStringA("[UI_Manager] actor_hud_layout.json load failed - using built-in HUD layout\n");
```

아래로 교체:

```cpp
        OutputDebugStringA("[UI_Manager] hud_irelia_layout.json load failed - using built-in HUD layout\n");
```

### 2-6. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_irelia_layout.json

기존 코드:

```json
        {
            "ID": "portrait.xp.arc.track",
            "sprite": "portrait.xp.arc.track",
            "rect": [252.00, 48.50, 36.00, 90.00]
        },
        {
            "ID": "portrait.xp.arc.fill",
            "sprite": "portrait.xp.arc.fill",
            "bind": "xpRatio",
            "clip": "bottomToTop",
            "rect": [252.00, 48.50, 36.00, 90.00]
        },
```

아래로 교체:

```json
        {
            "ID": "portrait.xp.arc.fill",
            "shape": "ringArc",
            "bind": "xpRatio",
            "arc": [38.00, -96.00, 8.50],
            "color": [0.49, 0.14, 0.92, 1.00],
            "colorEnd": [0.82, 0.44, 1.00, 1.00],
            "rect": [155.25, 37.00, 127.00, 152.00]
        },
```

### 2-7. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_atlas_manifest.json

`portrait.xp.arc.track`, `portrait.xp.arc.fill` 두 sprite 정의를 삭제한다. XP는 더 이상 atlas UV나 별도 PNG 조각을 읽지 않는다.

### 2-8. C:/Users/user/Desktop/Winters/.claude/gotchas.md

2026-07-17 항목으로 아래 규칙을 추가한다.

```text
- 2026-07-17 - [HUD XP arc] fill/track 마스크 IoU만 통과시키고 서로 다른 프레임/XP 목적지 변환과 실제 load/save 파일명을 확인하지 않아 5회 이상 위치 수정이 회귀했다 -> 합성 기준 프레임과 같은 좌표계에서 검증하고, 비등방 부모 변환을 독립 sprite width/height로 보정하지 않으며, HUD 튜너 저장 경로와 런타임 로드 경로가 같은 파일인지 정적 확인한다.
```

## 3. 검증

예측:
- XP 0%에서는 원호 정점이 생성되지 않아 프레임의 빈 홈만 보이고, 50%에서는 하단 시작각 `38°`부터 sweep 절반, 100%에서는 `-58°`까지 동일한 타원 홈 안에서 채워진다.
- HUD Layout 튜너의 기본 OFF preview를 켜면 서버 경험치 상태를 바꾸지 않고 0/50/100%를 즉시 비교할 수 있다.
- `portrait.xp.arc.fill`은 atlas sprite와 `bottomToTop` clip을 더 이상 참조하지 않는다. HP/MP 사각 clip과 원형 portrait 렌더는 불변이다.
- 게이트 없음: 실제 픽셀 단위 홈 포함 여부는 현재 실행 중인 사용자 디버그 세션의 0/50/100% 캡처 없이는 확정할 수 없다.

검증 명령:
- `Get-Content Client/Bin/Resource/UI/hud_irelia_layout.json -Raw | ConvertFrom-Json | Out-Null`
- `Get-Content Client/Bin/Resource/UI/hud_atlas_manifest.json -Raw | ConvertFrom-Json | Out-Null`
- `rg -n 'portrait\.xp\.arc\.(track|fill)|bottomToTop|ringArc|hud_irelia_layout' Engine Client/Bin/Resource/UI`
- `git diff --check -- Engine/Public/Renderer/UIRenderer.h Engine/Private/Renderer/UIRenderer.cpp Engine/Private/Manager/UI/ActorHUDPanel.h Engine/Private/Manager/UI/ActorHUDPanel.cpp Engine/Private/Manager/UI/UI_Manager.cpp .claude/gotchas.md .md/plan/2026-07-17_ACTOR_HUD_PROCEDURAL_XP_ARC_PLAN.md`
- 후속 동기화: Engine public header 변경 후 루트 `UpdateLib.bat` 실행 필요.

미검증:
- 사용자 요청에 따라 Engine/Client 빌드와 인게임 실행은 이번 세션에서 수행하지 않는다.
- 다양한 해상도에서 홈의 픽셀 경계와 레벨 구슬 아래 가림 순서는 사용자 인게임 캡처로 확인한다.

확인 필요:
- 첫 인게임 캡처에서 홈이 타원 호가 아닌 authored 자유곡선으로 판정되면 procedural ring을 유지하지 않고 동일 UV 공간 mask 방식으로 전환한다.

## 4. 검증·인계 예산

- 바닥 70%: 중복 sprite 경로 제거, 저장/로드 경로 통일, JSON/정적 diff 검증.
- 천장 30%: 2026-07-18까지 사용자 디버그 빌드에서 XP 0/50/100% 세 장을 캡처해 실제 합성 성공 여부를 판정한다.
