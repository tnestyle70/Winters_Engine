Session - UI/ImGui/RHI HUD 비용 절감

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Renderer/UIRenderer.h

기존 코드:
```cpp
    void Begin(u32_t iScreenWidth, u32_t iScreenHeight,
        eUISamplerMode eSamplerMode = eUISamplerMode::LinearClamp);
    void End();
```

아래에 추가:
```cpp
    void ReserveQuads(u32_t iQuadCount);
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Renderer/UIRenderer.cpp

기존 코드:
```cpp
void CUIRenderer::Begin(u32_t iScreenWidth, u32_t iScreenHeight, eUISamplerMode eSamplerMode)
{
    if (!IsReady() || m_pImpl->bInFrame)
        return;

    m_pImpl->bInFrame = true;
    m_pImpl->pCurrentSRV = nullptr;
    m_pImpl->vertices.clear();
    m_pImpl->SaveState();
```

아래에 추가:
```cpp
    if (m_pImpl->vertices.capacity() < 4096u)
        m_pImpl->vertices.reserve(4096u);
```

기존 코드:
```cpp
void CUIRenderer::End()
{
    if (!IsReady() || !m_pImpl->bInFrame)
        return;

    m_pImpl->Flush();
    m_pImpl->pCurrentSRV = nullptr;
    m_pImpl->bInFrame = false;
    m_pImpl->RestoreState();
}
```

아래에 추가:
```cpp
void CUIRenderer::ReserveQuads(u32_t iQuadCount)
{
    if (!IsReady())
        return;

    u32_t iVertexCount = iQuadCount * 6u;
    if (iVertexCount > kMaxUIVertices)
        iVertexCount = kMaxUIVertices;

    if (m_pImpl->vertices.capacity() < iVertexCount)
        m_pImpl->vertices.reserve(iVertexCount);
}
```

기존 코드:
```cpp
    const UIVertex vtx[6] = {
        { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w },
        { x1, y0, u1, v0, vColor.x, vColor.y, vColor.z, vColor.w },
        { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w },
        { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w },
        { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w },
        { x0, y1, u0, v1, vColor.x, vColor.y, vColor.z, vColor.w },
    };

    m_pImpl->vertices.insert(m_pImpl->vertices.end(), vtx, vtx + 6);
```

아래로 교체:
```cpp
    const size_t base = m_pImpl->vertices.size();
    m_pImpl->vertices.resize(base + 6u);
    UIVertex* pVtx = m_pImpl->vertices.data() + base;
    pVtx[0] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[1] = { x1, y0, u1, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[2] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[3] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[4] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[5] = { x0, y1, u0, v1, vColor.x, vColor.y, vColor.z, vColor.w };
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:
```cpp
    if (bUseRHI)
    {
        WINTERS_PROFILE_SCOPE("UI::RHIOverlay");
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        if (m_bShowHealthBars)
```

아래에 추가:
```cpp
        m_pRHIUIRenderer->ReserveQuads(768u);
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

기존 코드:
```cpp
void CProfilerOverlay::Draw()
```

아래에 추가:
```text
CONFIRM_NEEDED
프로파일러 자체 비용은 이미 Sample/Freeze로 많이 줄었지만, Raw events와 stable rows가 열린 상태에서는 ImGui::Text 호출이 많다.
구현 직전 Draw_ScopeSummary, Draw_Counters, Draw_FrameTree의 접힘 기본값과 표시 row cap을 다시 확인한다.
목표는 ProfilerOverlay::Render가 Freeze 상태에서 0.5ms 아래, Live 상태에서 1ms 아래로 머무는 것이다.
```

2. 검증

```text
빌드:
- git diff --check
- msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64
- Client Debug x64 빌드 또는 UI_Manager.cpp 단위 컴파일

프로파일러:
- UIOverlay::Render, UI::RHIOverlay, UI::RHIHealthBars, UI::RHIChampionHUD, ProfilerOverlay::Render 확인
- Profiler 닫힘 기준 UIOverlay::Render가 1ms 아래로 내려가는지 확인
- Profiler Freeze 기준 ProfilerOverlay::Render가 조작 불가 수준으로 튀지 않는지 확인

시각 QA:
- 챔피언/미니언/포탑 체력바가 누락되지 않는지 확인
- RHI HUD와 ImGui overlay가 겹쳐서 흔들리지 않는지 확인
- 마우스 커서 렌더가 UI batch 변경 이후에도 정상인지 확인
```
