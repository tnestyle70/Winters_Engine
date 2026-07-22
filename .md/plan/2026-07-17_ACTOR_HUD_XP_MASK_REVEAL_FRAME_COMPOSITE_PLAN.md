Session - authored XP arc를 Progress mask로 reveal하고 portrait frame을 마지막에 합성한다
좌표: 신규 좌표 후보 · 축: C1 · C8
관련: 2026-07-17_ACTOR_HUD_PROCEDURAL_XP_ARC_PLAN.md, 2026-07-17_ACTOR_HUD_PROCEDURAL_XP_ARC_RESULT.md

## 1. 결정 기록

① 문제·제약: frame source `202×168`을 destination `128×128`로 그리므로 배율은 `(0.633663, 0.761905)`다. XP source `48×120`에 같은 배율을 적용한 목적지는 `30.42×91.43`이어야 하나 기존은 `36×90`이었다.
① 문제·제약: procedural 48분할 원호는 규칙적인 타원에는 맞지만 authored XP alpha silhouette과 frame border/level orb 가림을 재사용하지 못한다.
② 순진한 해법의 실패: XP를 균일 `0.75`배하고 frame보다 나중에 그리면 X축이 약 18% 넓고 돌출 픽셀이 테두리 위에 남는다. fill/track IoU는 합성 검증이 아니다.
③ 메커니즘: authored track/fill을 동일 destination rect에 그린다. fill quad는 크기를 줄이지 않고 pixel shader가 `localV >= 1-Progress`인 픽셀만 통과시키며 texture alpha가 최종 arc mask가 된다.
③ 메커니즘: draw order를 `portrait face → track → fill reveal → portrait frame`으로 고정하고 procedural `DrawRingArc/ringArc` 경로를 삭제한다.
④ 대조: geometry UV crop도 실루엣은 보존하지만 매 비율마다 quad와 UV가 바뀐다. shader reveal은 authored quad/UV를 고정하고 Progress만 바꾸므로 원본 material 방식과 책임이 가깝다.
⑤ 대가: 공용 UI vertex에 reveal용 float 2개를 추가해 vertex stride가 `32→40 bytes`가 된다. reveal은 현재 bottom-to-top 선형 mask이며, 원본이 각도/거리 threshold mask라면 전용 mask texture가 필요하다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/UIRenderer.h

기존 `DrawRingArc` 선언을 삭제하고 아래 선언을 `DrawImageCircle` 아래에 추가한다.

```cpp
    void DrawImageVerticalReveal(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor,
        f32_t fRevealRatio);
```

### 2-2. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/UIRenderer.cpp

`UIVertex`의 기존 마지막 필드:

```cpp
        f32_t a = 1.f;
```

아래에 추가:

```cpp
        f32_t localV = 0.f;
        f32_t revealRatio = -1.f;
```

embedded HLSL의 `VS_INPUT`, `PS_INPUT`에 아래 필드를 추가하고 `VSMain`에서 전달한다.

```hlsl
    float2 vRevealData : TEXCOORD1;
```

`PSMain`의 texture sample 전에 추가:

```hlsl
    if (input.vRevealData.y >= 0.0f &&
        input.vRevealData.x < 1.0f - saturate(input.vRevealData.y))
    {
        discard;
    }
```

input layout의 COLOR 항목 아래에 추가:

```cpp
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
```

`CUIRenderer::DrawImage` 바로 아래에 추가:

```cpp
void CUIRenderer::DrawImageVerticalReveal(void* pTextureSRV,
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    const Vec4& vUVRect,
    const Vec4& vColor,
    f32_t fRevealRatio)
{
    if (!IsReady() || !m_pImpl->bInFrame ||
        fW <= 0.f || fH <= 0.f || fRevealRatio <= 0.f)
    {
        return;
    }

    ID3D11ShaderResourceView* pSRV = pTextureSRV
        ? static_cast<ID3D11ShaderResourceView*>(pTextureSRV)
        : m_pImpl->pWhiteSRV.Get();
    if (!pSRV)
        return;

    if (m_pImpl->pCurrentSRV != pSRV ||
        m_pImpl->vertices.size() + 6u > kMaxUIVertices)
    {
        m_pImpl->Flush();
        m_pImpl->pCurrentSRV = pSRV;
    }

    const f32_t x0 = fX;
    const f32_t y0 = fY;
    const f32_t x1 = fX + fW;
    const f32_t y1 = fY + fH;
    const f32_t u0 = vUVRect.x;
    const f32_t v0 = vUVRect.y;
    const f32_t u1 = vUVRect.z;
    const f32_t v1 = vUVRect.w;
    const f32_t reveal = WintersMath::Clamp01(fRevealRatio);

    const size_t base = m_pImpl->vertices.size();
    m_pImpl->vertices.resize(base + 6u);
    UIVertex* pVtx = m_pImpl->vertices.data() + base;
    pVtx[0] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w, 0.f, reveal };
    pVtx[1] = { x1, y0, u1, v0, vColor.x, vColor.y, vColor.z, vColor.w, 0.f, reveal };
    pVtx[2] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w, 1.f, reveal };
    pVtx[3] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w, 0.f, reveal };
    pVtx[4] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w, 1.f, reveal };
    pVtx[5] = { x0, y1, u0, v1, vColor.x, vColor.y, vColor.z, vColor.w, 1.f, reveal };
}
```

기존 `CUIRenderer::DrawRingArc` 정의 전체를 삭제한다.

### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.h

`LayoutElement`에서 procedural 전용 `vColor`, `vColorEnd`, `fArcStartDegrees`, `fArcSweepDegrees`, `fArcThickness`, `bHasColor`, `bHasColorEnd`를 삭제한다. XP preview 상태 두 필드는 유지한다.

### 2-4. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.cpp

`ParseLayoutElement`, layout tuner, `BuildLayoutJson`, `DrawElementRHI`에서 `ringArc/arc/color/colorEnd` 전용 코드를 삭제한다.

`UseDefaultLayout`의 portrait/XP 합성을 아래로 교체한다.

```cpp
        addElement("portrait.face", nullptr, "portrait", 167.75f, 61.25f, 82.f, 82.f);
        m_Elements.back().strShape = "circle";
        addElement("portrait.xp.arc.track", "portrait.xp.arc.track", nullptr,
            252.25f, 48.5f, 30.42f, 91.43f);
        addElement("portrait.xp.arc.fill", "portrait.xp.arc.fill", nullptr,
            252.25f, 48.5f, 30.42f, 91.43f, "xpRatio", nullptr, "maskBottomToTop");
        addElement("portrait.frame", "portrait.frame", nullptr,
            159.75f, 48.5f, 128.f, 128.f);
```

`DrawElementRHI`의 ratio 처리에서 `clip == "maskBottomToTop"`은 quad/UV를 자르지 않고 ratio만 보존한다. 최종 draw 직전에 아래를 추가한다.

```cpp
        if (Element.strClip == "maskBottomToTop")
        {
            m_pRenderer->DrawImageVerticalReveal(
                pSRV,
                drawX,
                drawY,
                drawScaledW,
                drawScaledH,
                uv,
                color,
                ratio);
            return;
        }
```

`portrait.xp.arc.fill`이고 preview가 켜졌을 때만 ratio를 preview 값으로 덮어쓴다. gameplay state는 수정하지 않는다.

### 2-5. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_irelia_layout.json

portrait 합성 블록을 아래 순서와 값으로 교체한다.

```json
        {
            "ID": "portrait.face",
            "image": "portrait",
            "shape": "circle",
            "rect": [167.75, 61.25, 82.00, 82.00]
        },
        {
            "ID": "portrait.xp.arc.track",
            "sprite": "portrait.xp.arc.track",
            "rect": [252.25, 48.50, 30.42, 91.43]
        },
        {
            "ID": "portrait.xp.arc.fill",
            "sprite": "portrait.xp.arc.fill",
            "bind": "xpRatio",
            "clip": "maskBottomToTop",
            "rect": [252.25, 48.50, 30.42, 91.43]
        },
        {
            "ID": "portrait.frame",
            "sprite": "portrait.frame",
            "rect": [159.75, 48.50, 128.00, 128.00]
        },
```

### 2-6. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_atlas_manifest.json

`portrait.frame` 뒤에 authored XP sprites를 복구한다.

```json
        "portrait.xp.arc.track": {
            "texture": "hud",
            "x": 729,
            "y": 869,
            "w": 48,
            "h": 120
        },
        "portrait.xp.arc.fill": {
            "texture": "hud",
            "x": 631,
            "y": 869,
            "w": 48,
            "h": 120
        },
```

### 2-7. C:/Users/user/Desktop/Winters/.claude/gotchas.md

기존 HUD XP arc 항목의 예방 규칙에 아래 내용을 합친다.

```text
atlas child rect는 parent frame의 source→destination X/Y 배율을 각각 상속해 계산하고, authored fill은 Progress mask로 reveal한 뒤 frame/border를 마지막에 합성한다.
```

## 3. 검증

예측:
- XP 0%는 track만, 50%는 fill quad 크기·UV 불변 상태에서 하단 절반만, 100%는 authored fill 전체가 보인다.
- frame은 track/fill 뒤에 그려져 gold border와 level orb가 돌출 픽셀을 덮는다.
- 일반 `DrawImage/DrawImageCircle` vertex의 reveal ratio 기본값 `-1` 때문에 기존 HUD·미니맵에는 pixel discard가 적용되지 않는다.
- procedural `DrawRingArc/ringArc` 문자열은 plan/result 외 runtime code와 runtime layout에서 0건이다.

검증 명령:
- `Get-Content Client/Bin/Resource/UI/hud_irelia_layout.json -Raw | ConvertFrom-Json | Out-Null`
- `Get-Content Client/Bin/Resource/UI/hud_atlas_manifest.json -Raw | ConvertFrom-Json | Out-Null`
- `rg -n 'DrawRingArc|ringArc|maskBottomToTop|DrawImageVerticalReveal|portrait\.xp\.arc' Engine Client/Bin/Resource/UI`
- `git diff --check -- Engine/Public/Renderer/UIRenderer.h Engine/Private/Renderer/UIRenderer.cpp Engine/Private/Manager/UI/ActorHUDPanel.h Engine/Private/Manager/UI/ActorHUDPanel.cpp .claude/gotchas.md`
- 후속 동기화: Engine public header 변경 후 루트 `UpdateLib.bat` 실행 필요.

미검증:
- 사용자 요청에 따라 Engine/Client 빌드와 인게임 시각 검증은 수행하지 않는다.
- pixel shader branch의 실제 DX11 컴파일과 XP 0/50/100% 합성은 사용자 빌드에서 확인한다.

확인 필요:
- bottom-to-top 선형 threshold가 authored XP 진행 방향과 다르면 별도 progress-mask texture 또는 angular threshold가 필요하다.

## 4. 검증·인계 예산

- 바닥 70%: procedural 중복 경로 삭제, 동일 transform 계산, JSON/셰이더 정적 검증.
- 천장 30%: 2026-07-18까지 사용자 디버그 빌드에서 XP preview 0/50/100%와 frame 최종 가림을 캡처해 종결 판정한다.
