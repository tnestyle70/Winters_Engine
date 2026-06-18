Session - LoL과 Elden이 함께 쓰는 런타임 UI 루트와 CUIObject 계층을 세우고 ImGui는 튜너/디버그로 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UIRenderTypes.h

새 파일:

```cpp
#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <string>

NS_BEGIN(Engine)

enum class eUIAnchor : u8_t
{
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

struct UIRect
{
    f32_t fX = 0.f;
    f32_t fY = 0.f;
    f32_t fW = 0.f;
    f32_t fH = 0.f;

    f32_t Left() const { return fX; }
    f32_t Top() const { return fY; }
    f32_t Right() const { return fX + fW; }
    f32_t Bottom() const { return fY + fH; }
    bool_t IsValid() const { return fW > 0.f && fH > 0.f; }
};

struct UIPadding
{
    f32_t fLeft = 0.f;
    f32_t fTop = 0.f;
    f32_t fRight = 0.f;
    f32_t fBottom = 0.f;
};

struct UITransform2D
{
    UIRect Rect{};
    eUIAnchor Anchor = eUIAnchor::TopLeft;
    Vec2 Pivot{ 0.f, 0.f };
    f32_t fOpacity = 1.f;
    i32_t iZOrder = 0;
};

struct UISpriteRef
{
    std::string strSpriteID;
    void* pTextureSRV = nullptr;
    Vec4 vUV = Vec4(0.f, 0.f, 1.f, 1.f);
    Vec4 vColor = Vec4(1.f, 1.f, 1.f, 1.f);
    bool_t bCircle = false;
};

struct UITextDrawDesc
{
    const char* pText = nullptr;
    void* pFont = nullptr;
    Vec2 vPosition{};
    f32_t fFontSize = 16.f;
    Vec4 vColor = Vec4(1.f, 1.f, 1.f, 1.f);
    Vec4 vOutlineColor = Vec4(0.f, 0.f, 0.f, 0.75f);
    bool_t bOutline = true;
};

NS_END
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UITextRenderer.h

새 파일:

```cpp
#pragma once

#include "Manager/UI/UIRenderTypes.h"

#include <memory>

NS_BEGIN(Engine)

class CUITextRenderer
{
public:
    virtual ~CUITextRenderer() = default;

    virtual Vec2 MeasureText(const UITextDrawDesc& Desc) const = 0;
    virtual void DrawText(const UITextDrawDesc& Desc) = 0;
};

std::unique_ptr<CUITextRenderer> CreateImGuiTextRenderer();

NS_END
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UIRenderContext.h

새 파일:

```cpp
#pragma once

#include "Manager/UI/UITextRenderer.h"
#include "Renderer/UIRenderer.h"

NS_BEGIN(Engine)

struct UIRenderContext
{
    CUIRenderer* pRenderer = nullptr;
    CUITextRenderer* pTextRenderer = nullptr;
    u32_t iScreenWidth = 0;
    u32_t iScreenHeight = 0;

    bool_t IsValid() const
    {
        return pRenderer && pRenderer->IsReady() && iScreenWidth > 0 && iScreenHeight > 0;
    }

    void DrawImage(const UISpriteRef& Sprite, const UIRect& Rect) const
    {
        if (!IsValid() || !Rect.IsValid())
            return;

        if (Sprite.bCircle)
        {
            pRenderer->DrawImageCircle(
                Sprite.pTextureSRV,
                Rect.fX,
                Rect.fY,
                Rect.fW,
                Rect.fH,
                Sprite.vUV,
                Sprite.vColor);
            return;
        }

        pRenderer->DrawImage(
            Sprite.pTextureSRV,
            Rect.fX,
            Rect.fY,
            Rect.fW,
            Rect.fH,
            Sprite.vUV,
            Sprite.vColor);
    }

    void DrawRectFilled(const UIRect& Rect, const Vec4& Color) const
    {
        if (!IsValid() || !Rect.IsValid())
            return;

        pRenderer->DrawImage(
            nullptr,
            Rect.fX,
            Rect.fY,
            Rect.fW,
            Rect.fH,
            Vec4(0.f, 0.f, 1.f, 1.f),
            Color);
    }

    void DrawText(const UITextDrawDesc& Desc) const
    {
        if (pTextRenderer)
            pTextRenderer->DrawText(Desc);
    }
};

NS_END
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/ImGuiTextRenderer.cpp

새 파일:

```cpp
#include "Manager/UI/UITextRenderer.h"

#include <algorithm>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

NS_BEGIN(Engine)

namespace
{
    ImU32 ToImU32(const Vec4& Color)
    {
        const f32_t r = std::clamp(Color.x, 0.f, 1.f);
        const f32_t g = std::clamp(Color.y, 0.f, 1.f);
        const f32_t b = std::clamp(Color.z, 0.f, 1.f);
        const f32_t a = std::clamp(Color.w, 0.f, 1.f);
        return IM_COL32(
            static_cast<i32_t>(r * 255.f),
            static_cast<i32_t>(g * 255.f),
            static_cast<i32_t>(b * 255.f),
            static_cast<i32_t>(a * 255.f));
    }

    ImFont* ResolveFont(void* pFont)
    {
        if (pFont)
            return static_cast<ImFont*>(pFont);
        return ImGui::GetFont();
    }
}

class CImGuiTextRenderer final : public CUITextRenderer
{
public:
    Vec2 MeasureText(const UITextDrawDesc& Desc) const override
    {
        if (!ImGui::GetCurrentContext() || !Desc.pText || Desc.pText[0] == '\0')
            return {};

        ImFont* pFont = ResolveFont(Desc.pFont);
        if (!pFont)
            return {};

        const f32_t fFontSize = Desc.fFontSize > 0.f ? Desc.fFontSize : ImGui::GetFontSize();
        const ImVec2 Size = pFont->CalcTextSizeA(fFontSize, FLT_MAX, 0.f, Desc.pText);
        return Vec2(Size.x, Size.y);
    }

    void DrawText(const UITextDrawDesc& Desc) override
    {
        if (!ImGui::GetCurrentContext() || !Desc.pText || Desc.pText[0] == '\0')
            return;

        ImDrawList* pDraw = ImGui::GetForegroundDrawList();
        ImFont* pFont = ResolveFont(Desc.pFont);
        if (!pDraw || !pFont)
            return;

        const f32_t fFontSize = Desc.fFontSize > 0.f ? Desc.fFontSize : ImGui::GetFontSize();
        const ImVec2 Pos(Desc.vPosition.x, Desc.vPosition.y);
        const ImU32 TextColor = ToImU32(Desc.vColor);
        const ImU32 OutlineColor = ToImU32(Desc.vOutlineColor);

        if (Desc.bOutline)
        {
            constexpr f32_t fOutline = 1.f;
            pDraw->AddText(pFont, fFontSize, ImVec2(Pos.x - fOutline, Pos.y), OutlineColor, Desc.pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(Pos.x + fOutline, Pos.y), OutlineColor, Desc.pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(Pos.x, Pos.y - fOutline), OutlineColor, Desc.pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(Pos.x, Pos.y + fOutline), OutlineColor, Desc.pText);
        }

        pDraw->AddText(pFont, fFontSize, Pos, TextColor, Desc.pText);
    }
};

std::unique_ptr<CUITextRenderer> CreateImGuiTextRenderer()
{
    return std::make_unique<CImGuiTextRenderer>();
}

NS_END
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UIObject.h

새 파일:

```cpp
#pragma once

#include "Manager/UI/UIRenderContext.h"

#include <memory>
#include <string>
#include <vector>

NS_BEGIN(Engine)

class CUIObject
{
public:
    virtual ~CUIObject() = default;

    void SetID(const std::string& strID) { m_strID = strID; }
    const std::string& GetID() const { return m_strID; }

    void SetVisible(bool_t bVisible) { m_bVisible = bVisible; }
    bool_t IsVisible() const { return m_bVisible; }

    void SetTransform(const UITransform2D& Transform) { m_Transform = Transform; }
    const UITransform2D& GetTransform() const { return m_Transform; }
    UITransform2D& GetTransform() { return m_Transform; }

    void SetRect(const UIRect& Rect) { m_Transform.Rect = Rect; }
    const UIRect& GetRect() const { return m_Transform.Rect; }

    void SetZOrder(i32_t iZOrder) { m_Transform.iZOrder = iZOrder; }
    i32_t GetZOrder() const { return m_Transform.iZOrder; }

    CUIObject* GetParent() const { return m_pParent; }
    void AddChild(std::unique_ptr<CUIObject> pChild);
    void ClearChildren();
    CUIObject* FindByID(const std::string& strID);

    virtual void Initialize() {}
    virtual void Update(f32_t fDeltaTime);
    void RenderTree(UIRenderContext& Context);

protected:
    virtual void OnUpdate(f32_t fDeltaTime) { (void)fDeltaTime; }
    virtual void OnRender(UIRenderContext& Context) { (void)Context; }

private:
    std::string m_strID;
    UITransform2D m_Transform{};
    CUIObject* m_pParent = nullptr;
    std::vector<std::unique_ptr<CUIObject>> m_Children;
    bool_t m_bVisible = true;
};

NS_END
```

1-6. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UIObject.cpp

새 파일:

```cpp
#include "Manager/UI/UIObject.h"

#include <algorithm>

NS_BEGIN(Engine)

void CUIObject::AddChild(std::unique_ptr<CUIObject> pChild)
{
    if (!pChild)
        return;

    pChild->m_pParent = this;
    pChild->Initialize();
    m_Children.push_back(std::move(pChild));
}

void CUIObject::ClearChildren()
{
    m_Children.clear();
}

CUIObject* CUIObject::FindByID(const std::string& strID)
{
    if (m_strID == strID)
        return this;

    for (const std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (!pChild)
            continue;

        if (CUIObject* pFound = pChild->FindByID(strID))
            return pFound;
    }

    return nullptr;
}

void CUIObject::Update(f32_t fDeltaTime)
{
    OnUpdate(fDeltaTime);

    for (std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (pChild)
            pChild->Update(fDeltaTime);
    }
}

void CUIObject::RenderTree(UIRenderContext& Context)
{
    if (!m_bVisible)
        return;

    OnRender(Context);

    std::stable_sort(
        m_Children.begin(),
        m_Children.end(),
        [](const std::unique_ptr<CUIObject>& pLhs, const std::unique_ptr<CUIObject>& pRhs)
        {
            const i32_t iLhs = pLhs ? pLhs->GetZOrder() : 0;
            const i32_t iRhs = pRhs ? pRhs->GetZOrder() : 0;
            return iLhs < iRhs;
        });

    for (std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (pChild)
            pChild->RenderTree(Context);
    }
}

NS_END
```

1-7. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UIRoot.h

새 파일:

```cpp
#pragma once

#include "Manager/UI/UIObject.h"

#include <memory>
#include <vector>

NS_BEGIN(Engine)

class CUIRoot final
{
public:
    void SetScreenSize(u32_t iScreenWidth, u32_t iScreenHeight);
    u32_t GetScreenWidth() const { return m_iScreenWidth; }
    u32_t GetScreenHeight() const { return m_iScreenHeight; }

    void AddChild(std::unique_ptr<CUIObject> pChild);
    void Clear();
    CUIObject* FindByID(const std::string& strID);

    void Update(f32_t fDeltaTime);
    void Render(UIRenderContext& Context);

private:
    std::vector<std::unique_ptr<CUIObject>> m_Children;
    u32_t m_iScreenWidth = 0;
    u32_t m_iScreenHeight = 0;
};

NS_END
```

1-8. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UIRoot.cpp

새 파일:

```cpp
#include "Manager/UI/UIRoot.h"

#include <algorithm>

NS_BEGIN(Engine)

void CUIRoot::SetScreenSize(u32_t iScreenWidth, u32_t iScreenHeight)
{
    m_iScreenWidth = iScreenWidth;
    m_iScreenHeight = iScreenHeight;
}

void CUIRoot::AddChild(std::unique_ptr<CUIObject> pChild)
{
    if (!pChild)
        return;

    pChild->Initialize();
    m_Children.push_back(std::move(pChild));
}

void CUIRoot::Clear()
{
    m_Children.clear();
}

CUIObject* CUIRoot::FindByID(const std::string& strID)
{
    for (const std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (!pChild)
            continue;

        if (CUIObject* pFound = pChild->FindByID(strID))
            return pFound;
    }

    return nullptr;
}

void CUIRoot::Update(f32_t fDeltaTime)
{
    for (std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (pChild)
            pChild->Update(fDeltaTime);
    }
}

void CUIRoot::Render(UIRenderContext& Context)
{
    SetScreenSize(Context.iScreenWidth, Context.iScreenHeight);

    std::stable_sort(
        m_Children.begin(),
        m_Children.end(),
        [](const std::unique_ptr<CUIObject>& pLhs, const std::unique_ptr<CUIObject>& pRhs)
        {
            const i32_t iLhs = pLhs ? pLhs->GetZOrder() : 0;
            const i32_t iRhs = pRhs ? pRhs->GetZOrder() : 0;
            return iLhs < iRhs;
        });

    for (std::unique_ptr<CUIObject>& pChild : m_Children)
    {
        if (pChild)
            pChild->RenderTree(Context);
    }
}

NS_END
```

1-9. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
#include "Manager/UI/UIAtlasManifest.h"
#include "Renderer/UIRenderer.h"
```

아래에 추가:

```cpp
#include "Manager/UI/UIRoot.h"
#include "Manager/UI/UITextRenderer.h"
```

기존 코드:

```cpp
    void DrawStatusPanel(ImDrawList* pDraw);
```

아래에 추가:

```cpp
    void    DrawRuntimeUIRoot(f32_t fDeltaTime);
```

기존 코드:

```cpp
    std::unique_ptr<CUIRenderer> m_pRHIUIRenderer;
    std::unique_ptr<CFont_Manager> m_pFontManager;
```

아래로 교체:

```cpp
    std::unique_ptr<CUIRenderer> m_pRHIUIRenderer;
    std::unique_ptr<CUITextRenderer> m_pTextRenderer;
    std::unique_ptr<CFont_Manager> m_pFontManager;
    CUIRoot m_RuntimeUIRoot;
```

기존 코드:

```cpp
    bool_t m_bUseLuaUI = true;
```

아래에 추가:

```cpp
    bool_t m_bUseRuntimeUIRoot = true;
```

1-10. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
#include "Manager/UI/LuaUIHost.h"
```

아래에 추가:

```cpp
#include "Manager/UI/UIRenderContext.h"
#include "Manager/UI/UITextRenderer.h"
```

기존 코드:

```cpp
    m_pFontManager->AddFont("fallback", kPathFontFallback, 17.f);
    m_pFontManager->AddFont("hud", kPathFontHud, 17.f);
    m_pFontManager->AddFont("shop", kPathFontShop, 18.f);
    m_pFontManager->AddFont("shop.body", kPathFontShopBody, 15.f);
```

아래에 추가:

```cpp
    m_pTextRenderer = CreateImGuiTextRenderer();
```

기존 코드:

```cpp
    ReleaseChampionHUDAssets();
    m_pRHIUIRenderer.reset();
```

아래로 교체:

```cpp
    ReleaseChampionHUDAssets();
    m_RuntimeUIRoot.Clear();
    m_pTextRenderer.reset();
    m_pRHIUIRenderer.reset();
```

`void CUI_Manager::Render_Overlay(const Mat4& matVP)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        m_pRHIUIRenderer->Begin(m_iWinSizeX, m_iWinSizeY);
        m_pRHIUIRenderer->ReserveQuads(768u);
```

아래에 추가:

```cpp
        if (m_bUseRuntimeUIRoot)
        {
            WINTERS_PROFILE_SCOPE("UI::RuntimeRoot");
            DrawRuntimeUIRoot(fUIDt);
        }
```

`void CUI_Manager::Render_Cursor()` 기존 코드 위에 추가:

```cpp
void CUI_Manager::DrawRuntimeUIRoot(f32_t fDeltaTime)
{
    if (!m_pRHIUIRenderer || !m_pRHIUIRenderer->IsReady())
        return;

    m_RuntimeUIRoot.Update(fDeltaTime);

    UIRenderContext Context{};
    Context.pRenderer = m_pRHIUIRenderer.get();
    Context.pTextRenderer = m_pTextRenderer.get();
    Context.iScreenWidth = m_iWinSizeX;
    Context.iScreenHeight = m_iWinSizeY;
    m_RuntimeUIRoot.Render(Context);
}

```

`void CUI_Manager::OnImGui_Tuner()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    ImGui::Checkbox("Use Lua UI", &m_bUseLuaUI);
```

아래에 추가:

```cpp
    ImGui::Checkbox("Use Runtime UI Root", &m_bUseRuntimeUIRoot);
    ImGui::TextDisabled("Runtime HUD widgets must render through CUIRoot/CUIObject. ImGui remains for tuner/debug only.");
```

1-11. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

기존 코드:

```text
## LoL DX11 현재 기준
```

아래에 추가:

```text
## UI Runtime 기준

- Runtime HUD는 `CUIRoot -> CUIObject -> UIRenderContext -> CUIRenderer` 경로로 추가한다.
- `CUIObject`는 수업식 UI object lifecycle을 따르되, `CGameObject`와 DX11 device/context를 직접 소유하지 않는다.
- ImGui는 tuner, editor, debug overlay, 임시 migration adapter에만 사용한다.
- 새 gameplay HUD feature는 `ImDrawList`에 직접 추가하지 말고 runtime UI root widget으로 만든다.
- 텍스트는 `CUITextRenderer` 인터페이스를 통해 호출한다. 현재 구현은 ImGui font adapter지만 widget code는 ImGui type을 직접 include하지 않는다.
```

2. 검증

미검증:
- 빌드 미검증
- 런타임에서 기존 HUD/상점/상태창/커서가 기존처럼 보이는지 미검증
- 새 `CUIRoot`에 아직 실제 미니맵 widget은 등록하지 않음

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64`
- `rg -n "ImDrawList\\*|ImGui::" Engine/Public/Manager/UI/UIObject.h Engine/Public/Manager/UI/UIRoot.h Engine/Public/Manager/UI/UIRenderContext.h Engine/Public/Manager/UI/UIRenderTypes.h Engine/Public/Manager/UI/UITextRenderer.h`
- `rg -n "CUIRoot|CUIObject|CreateImGuiTextRenderer|DrawRuntimeUIRoot" Engine/Public/Manager/UI Engine/Private/Manager/UI`

확인 필요:
- 새로 추가한 `Engine/Public/Manager/UI/*.h`와 `Engine/Private/Manager/UI/*.cpp` 파일이 `Engine.vcxproj`, `Engine.vcxproj.filters`, `cmake/WintersEngine.cmake`에 포함되는지 확인.
- 기존 `ChampionHUDPanel`, `InGameShop`, `StatusPanel`, `GameContextHUD`, `KillFeed`, damage floater는 후속 세션에서 순서대로 `CUIObject` widget으로 이관한다.
- 미니맵은 이 계획 반영 뒤 `CMinimapWidget : CUIObject`로 작성한다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
