Session - 실제 롤 미니언 체력바 이미지로 미니언 HP바를 띄우고 체력 비율만큼 줄어들게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

`class CUI_Manager final`의 private 렌더 함수 선언부에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void    Draw_HealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    Draw_HealthBars_RHI(const DirectX::XMMATRIX& mVP);
    void    DrawHealthBarBarcodeOverlay(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

아래에 추가:

```cpp
    void    Draw_MinionHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
    void    Draw_MinionHealthBars_RHI(const DirectX::XMMATRIX& mVP);
```

`Champion-attached HP bar resources.` 블록에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    void* m_pSRV_HPBarGreen = nullptr;
    void* m_pSRV_HPBarRed = nullptr;
```

아래에 추가:

```cpp
    void* m_pSRV_MinionBlueHPBar = nullptr;
    void* m_pSRV_MinionRedHPBar = nullptr;
```

월드 HP바 튜닝 필드에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    f32_t   m_fHPBarWidth = 104.f;    // 화면 픽셀
    f32_t   m_fHPBarHeight = 20.f;
    f32_t   m_fHPBarYOffset = 2.75f;    // 월드 좌표 머리 위 높이 (m)
```

아래에 추가:

```cpp
    f32_t   m_fMinionHPBarWidth = 46.f;
    f32_t   m_fMinionHPBarHeight = 6.f;
    f32_t   m_fMinionHPBarYOffset = 1.65f;
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

익명 namespace의 UI 텍스처 경로 상수에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    constexpr const wchar_t* kPathHPBarGreen = L"Resource/Texture/UI/HealthBarGreen.png";
    constexpr const wchar_t* kPathHPBarRed = L"Resource/Texture/UI/HealthBarRed.png";
```

아래에 추가:

```cpp
    constexpr const wchar_t* kPathMinionBlueHPBar = L"Resource/Texture/UI/MinionBlueHPBar.png";
    constexpr const wchar_t* kPathMinionRedHPBar = L"Resource/Texture/UI/MinionRedHPBar.png";
```

`CUI_Manager::Initialize`에서 챔피언 HP바 로드 블록 바로 아래에 추가:

기존 코드:

```cpp
    if (FAILED(Load_TextureSRV(kPathHPBarRed, &m_pSRV_HPBarRed)))
    {
        OutputDebugStringA("[UI_Manager] HealthBarRed.png load failed - enemy HP bars use fallback fill\n");
        m_pSRV_HPBarRed = nullptr;
    }
```

아래에 추가:

```cpp
    if (FAILED(Load_TextureSRV(kPathMinionBlueHPBar, &m_pSRV_MinionBlueHPBar)))
    {
        OutputDebugStringA("[UI_Manager] MinionBlueHPBar.png load failed - blue minion HP bars use fallback fill\n");
        m_pSRV_MinionBlueHPBar = nullptr;
    }
    if (FAILED(Load_TextureSRV(kPathMinionRedHPBar, &m_pSRV_MinionRedHPBar)))
    {
        OutputDebugStringA("[UI_Manager] MinionRedHPBar.png load failed - red minion HP bars use fallback fill\n");
        m_pSRV_MinionRedHPBar = nullptr;
    }
```

`CUI_Manager::Shutdown`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ReleaseSRV(m_pSRV_HPBarGreen);
    ReleaseSRV(m_pSRV_HPBarRed);
```

아래에 추가:

```cpp
    ReleaseSRV(m_pSRV_MinionBlueHPBar);
    ReleaseSRV(m_pSRV_MinionRedHPBar);
```

`CUI_Manager::Render_Overlay`의 RHI HP바 호출부 교체:

기존 코드:

```cpp
        if (m_bShowHealthBars)
            Draw_HealthBars_RHI(mVP);
```

아래로 교체:

```cpp
        if (m_bShowHealthBars)
        {
            Draw_HealthBars_RHI(mVP);
            Draw_MinionHealthBars_RHI(mVP);
        }
```

`CUI_Manager::Render_Overlay`의 ImGui fallback HP바 호출부 교체:

기존 코드:

```cpp
        else
            Draw_HealthBars(pDraw, mVP);
```

아래로 교체:

```cpp
        else
        {
            Draw_HealthBars(pDraw, mVP);
            Draw_MinionHealthBars(pDraw, mVP);
        }
```

`CUI_Manager::Draw_HealthBars_RHI` 함수 끝과 `CUI_Manager::DrawHealthBarBarcodeOverlay` 함수 사이에 아래 함수 추가:

아래에 추가:

```cpp
void CUI_Manager::Draw_MinionHealthBars(ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !pDraw)
        return;

    const f32_t w = m_fMinionHPBarWidth;
    const f32_t h = m_fMinionHPBarHeight;
    const f32_t yOff = m_fMinionHPBarYOffset;

    m_pWorld->ForEach<MinionComponent, TransformComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf)
        {
            f32_t hpCurrent = minion.hp;
            f32_t hpMaximum = minion.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const ImVec2 barMin(screen.x - w * 0.5f, screen.y - h * 0.5f);
            const ImVec2 barMax(screen.x + w * 0.5f, screen.y + h * 0.5f);
            const f32_t fillW = w * clamped;
            const ImVec2 fillMax(barMin.x + fillW, barMax.y);

            const bool_t bBlueTeam = minion.team == eTeam::Blue;
            void* pBarSRV = bBlueTeam ? m_pSRV_MinionBlueHPBar : m_pSRV_MinionRedHPBar;

            pDraw->AddRectFilled(barMin, barMax, IM_COL32(14, 14, 14, 232));
            if (fillW > 0.5f)
            {
                if (pBarSRV)
                {
                    pDraw->AddImage(
                        reinterpret_cast<ImTextureID>(pBarSRV),
                        barMin,
                        fillMax,
                        ImVec2(0.f, 0.f),
                        ImVec2(clamped, 1.f),
                        IM_COL32(255, 255, 255, 255));
                }
                else
                {
                    pDraw->AddRectFilled(
                        barMin,
                        fillMax,
                        bBlueTeam ? IM_COL32(55, 150, 255, 255) : IM_COL32(230, 54, 54, 255));
                }
            }
        });
}

void CUI_Manager::Draw_MinionHealthBars_RHI(const DirectX::XMMATRIX& mVP)
{
    if (!m_pWorld || !m_pRHIUIRenderer)
        return;

    const f32_t w = m_fMinionHPBarWidth;
    const f32_t h = m_fMinionHPBarHeight;
    const f32_t yOff = m_fMinionHPBarYOffset;
    const Vec4 uvFull(0.f, 0.f, 1.f, 1.f);

    m_pWorld->ForEach<MinionComponent, TransformComponent>(
        [&](EntityID id, MinionComponent& minion, TransformComponent& tf)
        {
            f32_t hpCurrent = minion.hp;
            f32_t hpMaximum = minion.maxHp;
            bool_t bDead = (hpCurrent <= 0.f);
            if (m_pWorld->HasComponent<HealthComponent>(id))
            {
                const HealthComponent& hp = m_pWorld->GetComponent<HealthComponent>(id);
                hpCurrent = hp.fCurrent;
                if (hp.fMaximum > 0.f)
                    hpMaximum = hp.fMaximum;
                bDead = hp.bIsDead || hpCurrent <= 0.f;
            }
            if (bDead || hpMaximum <= 0.f)
                return;

            const Vec3 p = tf.GetPosition();
            ImVec2 screen;
            if (!UI_WorldToScreen(mVP, { p.x, p.y + yOff, p.z }, screen,
                m_iWinSizeX, m_iWinSizeY))
                return;

            const f32_t clamped = std::clamp(hpCurrent / hpMaximum, 0.f, 1.f);
            const f32_t x = screen.x - w * 0.5f;
            const f32_t y = screen.y - h * 0.5f;
            const f32_t fillW = w * clamped;

            const bool_t bBlueTeam = minion.team == eTeam::Blue;
            void* pBarSRV = bBlueTeam ? m_pSRV_MinionBlueHPBar : m_pSRV_MinionRedHPBar;

            m_pRHIUIRenderer->DrawImage(
                nullptr,
                x,
                y,
                w,
                h,
                uvFull,
                Vec4(0.055f, 0.055f, 0.055f, 0.91f));

            if (fillW <= 0.5f)
                return;

            if (pBarSRV)
            {
                m_pRHIUIRenderer->DrawImage(
                    pBarSRV,
                    x,
                    y,
                    fillW,
                    h,
                    Vec4(0.f, 0.f, clamped, 1.f),
                    UI_WhiteVec());
            }
            else
            {
                m_pRHIUIRenderer->DrawImage(
                    nullptr,
                    x,
                    y,
                    fillW,
                    h,
                    uvFull,
                    bBlueTeam ? Vec4(0.22f, 0.58f, 1.0f, 1.0f) : Vec4(0.90f, 0.21f, 0.21f, 1.0f));
            }
        });
}
```

`CUI_Manager::OnImGui_Tuner`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ImGui::Text("HP Green: %s", m_pSRV_HPBarGreen ? "loaded" : "FALLBACK");
    ImGui::Text("HP Red: %s", m_pSRV_HPBarRed ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("World HP bars: champion only, ally green / enemy red");
    ImGui::SliderFloat("Bar Width (px)",   &m_fHPBarWidth,   20.f, 200.f);
    ImGui::SliderFloat("Bar Height (px)",  &m_fHPBarHeight,   3.f,  32.f);
    ImGui::SliderFloat("Y Offset (m)",     &m_fHPBarYOffset, 0.5f,  6.f);
```

아래에 추가:

```cpp
    ImGui::Text("Minion Blue HP: %s", m_pSRV_MinionBlueHPBar ? "loaded" : "FALLBACK");
    ImGui::Text("Minion Red HP: %s", m_pSRV_MinionRedHPBar ? "loaded" : "FALLBACK");
    ImGui::TextDisabled("Minion HP bars: team blue/red texture, dark depleted backing");
    ImGui::SliderFloat("Minion Bar Width (px)", &m_fMinionHPBarWidth, 20.f, 100.f);
    ImGui::SliderFloat("Minion Bar Height (px)", &m_fMinionHPBarHeight, 3.f, 16.f);
    ImGui::SliderFloat("Minion Y Offset (m)", &m_fMinionHPBarYOffset, 0.5f, 3.f);
```

1-3. C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj

변경 없음. 기존 항목이 이미 포함되어 있다.

```xml
    <ClCompile Include="..\Private\Manager\UI\UI_Manager.cpp" />
    <ClInclude Include="..\Public\Manager\UI\UI_Manager.h" />
```

1-4. C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj.filters

변경 없음. 기존 항목이 이미 포함되어 있다.

```xml
    <ClInclude Include="..\Public\Manager\UI\UI_Manager.h">
    <ClCompile Include="..\Private\Manager\UI\UI_Manager.cpp">
```

2. 검증

- `git diff --check -- Engine/Public/Manager/UI/UI_Manager.h Engine/Private/Manager/UI/UI_Manager.cpp .md/TODO/05-19/MINION_HEALTH_BAR_RHI_PLAN.md`
- `Engine/Include/Engine.vcxproj` Debug x64 빌드
- `Client/Include/Client.vcxproj` Debug x64 빌드
- 런타임에서 `UI Manager` 튜너가 `Minion Blue HP`, `Minion Red HP`를 loaded로 표시하는지 확인
- 미니언 피격 시 `MinionComponent.hp/maxHp` 또는 `HealthComponent` 값 기준으로 체력바 foreground가 왼쪽부터 비율만큼 줄어드는지 확인
- HP가 줄어든 영역 뒤에는 같은 크기의 짙은 회색 바가 보이는지 확인
