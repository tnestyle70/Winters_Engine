# 09 -- UE5-Style Editor Framework via ImGui

> Winters Engine API Modernization -- Stage 9
> Date: 2026-05-02
> Depends on: CImGuiLayer (existing), CAssetRegistry (Stage 8), ECS CWorld

---

## 1. Architecture Overview

The current editor code is **scattered ImGui calls** in `Scene_InGame.cpp` (~3000 lines):
champion tuner sliders, camera debug, FPS display, etc. There is no panel framework,
no docking workspace, no reusable property editor.

This plan introduces a **UE5-style editor framework** built entirely on ImGui:

| Component | Class | Role |
|---|---|---|
| Editor App | `CEditorApp` | Editor application mode. Extends engine loop with panel management. |
| Panel Base | `CEditorPanel` | Abstract dockable panel. Title, visibility, focus. |
| Details Panel | `CDetailsPanel` | Auto-generated property inspector from reflection metadata. |
| Content Browser | `CContentBrowser` | Asset directory browsing with thumbnails. |
| World Outliner | `CWorldOutliner` | Entity hierarchy tree view. |
| Viewport | `CViewport` | 3D render viewport with gizmo overlay. |
| Property Editor | `CPropertyEditor` | Type-dispatched property widgets (f32, Vec3, Color, Enum, etc.). |
| Asset Importer | `CAssetImporter` | Drag-drop file import pipeline. |

### Panel Lifecycle

```
CEditorApp::OnImGui()
  |-- ImGui::DockSpaceOverViewport()       // main docking workspace
  |-- for each CEditorPanel:
      |-- if (m_bVisible) panel->OnImGui()  // ImGui::Begin/End handled by base
  |-- CEditorApp renders menu bar (File/Edit/View/Tools)
```

---

## 2. File Structure

```
Engine/
  Public/Editor/
    CEditorPanel.h           -- abstract panel base
    CDetailsPanel.h          -- property inspector
    CContentBrowser.h        -- asset browser
    CWorldOutliner.h         -- entity tree
    CViewport.h              -- 3D viewport
    CPropertyEditor.h        -- type-dispatched widgets
    CAssetImporter.h         -- drag-drop import
    CEditorApp.h             -- editor application controller
    EditorReflection.h       -- WPROPERTY macro + property metadata
  Private/Editor/
    CEditorPanel.cpp
    CDetailsPanel.cpp
    CContentBrowser.cpp
    CWorldOutliner.cpp
    CViewport.cpp
    CPropertyEditor.cpp
    CAssetImporter.cpp
    CEditorApp.cpp
```

---

## 3. Full Code

### 3.1 EditorReflection.h

```cpp
// Engine/Public/Editor/EditorReflection.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <vector>
#include <functional>
#include <any>

// ---------------------------------------------------------------
//  ePropertyType -- supported property widget types
// ---------------------------------------------------------------
enum class ePropertyType : u8_t
{
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    UInt,
    Bool,
    String,
    WString,
    Color3,        // RGB float3 with color picker
    Color4,        // RGBA float4 with color picker
    Enum,
    Slider,        // float with min/max
    AssetPath,     // string with browse button
};

// ---------------------------------------------------------------
//  PropertyMeta -- metadata for a single reflected property
// ---------------------------------------------------------------
struct PropertyMeta
{
    std::string     strName;
    std::string     strCategory;      // group header (e.g. "Transform", "Material")
    ePropertyType   eType;
    void*           pData      = nullptr;   // pointer to the member variable
    f32_t           fMin       = 0.f;
    f32_t           fMax       = 1.f;
    f32_t           fStep      = 0.01f;
    std::vector<std::string> vecEnumNames; // for ePropertyType::Enum

    // Optional callback when value changes
    std::function<void()> fnOnChanged;
};

// ---------------------------------------------------------------
//  CPropertyList -- collection of properties for one object
//
//  Built manually or via WPROPERTY macros. Passed to CDetailsPanel
//  which auto-generates the ImGui UI.
// ---------------------------------------------------------------
class WINTERS_ENGINE CPropertyList
{
public:
    CPropertyList() = default;
    ~CPropertyList() = default;

    void AddFloat(const std::string& name, const std::string& category,
                  f32_t* pValue, f32_t fMin = 0.f, f32_t fMax = 1.f,
                  f32_t fStep = 0.01f, std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::Slider;
        m.pData       = pValue;
        m.fMin        = fMin;
        m.fMax        = fMax;
        m.fStep       = fStep;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddFloat3(const std::string& name, const std::string& category,
                   Vec3* pValue, std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::Float3;
        m.pData       = pValue;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddColor4(const std::string& name, const std::string& category,
                   Vec4* pValue, std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::Color4;
        m.pData       = pValue;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddBool(const std::string& name, const std::string& category,
                 bool* pValue, std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::Bool;
        m.pData       = pValue;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddInt(const std::string& name, const std::string& category,
                i32_t* pValue, f32_t fMin = 0.f, f32_t fMax = 100.f,
                std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::Int;
        m.pData       = pValue;
        m.fMin        = fMin;
        m.fMax        = fMax;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddEnum(const std::string& name, const std::string& category,
                 i32_t* pValue, const std::vector<std::string>& names,
                 std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName      = name;
        m.strCategory  = category;
        m.eType        = ePropertyType::Enum;
        m.pData        = pValue;
        m.vecEnumNames = names;
        m.fnOnChanged  = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    void AddString(const std::string& name, const std::string& category,
                   std::string* pValue, std::function<void()> onChanged = nullptr)
    {
        PropertyMeta m;
        m.strName     = name;
        m.strCategory = category;
        m.eType       = ePropertyType::String;
        m.pData       = pValue;
        m.fnOnChanged = onChanged;
        m_vecProperties.push_back(std::move(m));
    }

    const std::vector<PropertyMeta>& GetProperties() const { return m_vecProperties; }
    void Clear() { m_vecProperties.clear(); }

private:
    std::vector<PropertyMeta> m_vecProperties;
};
```

### 3.2 CPropertyEditor.h

```cpp
// Engine/Public/Editor/CPropertyEditor.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Editor/EditorReflection.h"

// ---------------------------------------------------------------
//  CPropertyEditor -- type-dispatched ImGui property widgets
//
//  Given a CPropertyList, renders appropriate ImGui widgets for
//  each property type (slider, color picker, checkbox, etc.).
//
//  Stateless utility: all functions are static.
// ---------------------------------------------------------------
class WINTERS_ENGINE CPropertyEditor
{
public:
    // Render all properties in the list, grouped by category
    static void RenderPropertyList(CPropertyList& propList);

    // Render a single property (called internally)
    static bool RenderProperty(PropertyMeta& meta);

private:
    CPropertyEditor() = delete;

    static bool RenderFloat(PropertyMeta& meta);
    static bool RenderFloat3(PropertyMeta& meta);
    static bool RenderFloat4(PropertyMeta& meta);
    static bool RenderColor3(PropertyMeta& meta);
    static bool RenderColor4(PropertyMeta& meta);
    static bool RenderBool(PropertyMeta& meta);
    static bool RenderInt(PropertyMeta& meta);
    static bool RenderSlider(PropertyMeta& meta);
    static bool RenderEnum(PropertyMeta& meta);
    static bool RenderString(PropertyMeta& meta);
};
```

### 3.3 CPropertyEditor.cpp

```cpp
// Engine/Private/Editor/CPropertyEditor.cpp
#include "Editor/CPropertyEditor.h"

#ifdef _DEBUG
#undef new
#endif
#include <imgui.h>
#ifdef _DEBUG
#define new DBG_NEW
#endif

#include <string>
#include <unordered_set>

void CPropertyEditor::RenderPropertyList(CPropertyList& propList)
{
    const auto& props = propList.GetProperties();
    if (props.empty()) return;

    // Group by category
    std::string currentCategory;

    for (auto& meta : const_cast<std::vector<PropertyMeta>&>(propList.GetProperties()))
    {
        if (meta.strCategory != currentCategory)
        {
            currentCategory = meta.strCategory;
            if (!currentCategory.empty())
            {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                    "%s", currentCategory.c_str());
            }
        }

        bool changed = RenderProperty(meta);
        if (changed && meta.fnOnChanged)
            meta.fnOnChanged();
    }
}

bool CPropertyEditor::RenderProperty(PropertyMeta& meta)
{
    switch (meta.eType)
    {
    case ePropertyType::Float:   return RenderFloat(meta);
    case ePropertyType::Float3:  return RenderFloat3(meta);
    case ePropertyType::Float4:  return RenderFloat4(meta);
    case ePropertyType::Color3:  return RenderColor3(meta);
    case ePropertyType::Color4:  return RenderColor4(meta);
    case ePropertyType::Bool:    return RenderBool(meta);
    case ePropertyType::Int:     return RenderInt(meta);
    case ePropertyType::Slider:  return RenderSlider(meta);
    case ePropertyType::Enum:    return RenderEnum(meta);
    case ePropertyType::String:  return RenderString(meta);
    default: return false;
    }
}

bool CPropertyEditor::RenderFloat(PropertyMeta& meta)
{
    f32_t* pVal = static_cast<f32_t*>(meta.pData);
    return ImGui::DragFloat(meta.strName.c_str(), pVal, meta.fStep);
}

bool CPropertyEditor::RenderFloat3(PropertyMeta& meta)
{
    Vec3* pVal = static_cast<Vec3*>(meta.pData);
    return ImGui::DragFloat3(meta.strName.c_str(), &pVal->x, 0.01f);
}

bool CPropertyEditor::RenderFloat4(PropertyMeta& meta)
{
    Vec4* pVal = static_cast<Vec4*>(meta.pData);
    return ImGui::DragFloat4(meta.strName.c_str(), &pVal->x, 0.01f);
}

bool CPropertyEditor::RenderColor3(PropertyMeta& meta)
{
    Vec3* pVal = static_cast<Vec3*>(meta.pData);
    return ImGui::ColorEdit3(meta.strName.c_str(), &pVal->x);
}

bool CPropertyEditor::RenderColor4(PropertyMeta& meta)
{
    Vec4* pVal = static_cast<Vec4*>(meta.pData);
    return ImGui::ColorEdit4(meta.strName.c_str(), &pVal->x);
}

bool CPropertyEditor::RenderBool(PropertyMeta& meta)
{
    bool* pVal = static_cast<bool*>(meta.pData);
    return ImGui::Checkbox(meta.strName.c_str(), pVal);
}

bool CPropertyEditor::RenderInt(PropertyMeta& meta)
{
    i32_t* pVal = static_cast<i32_t*>(meta.pData);
    return ImGui::DragInt(meta.strName.c_str(), pVal, 1.f,
        static_cast<i32_t>(meta.fMin), static_cast<i32_t>(meta.fMax));
}

bool CPropertyEditor::RenderSlider(PropertyMeta& meta)
{
    f32_t* pVal = static_cast<f32_t*>(meta.pData);
    return ImGui::SliderFloat(meta.strName.c_str(), pVal, meta.fMin, meta.fMax);
}

bool CPropertyEditor::RenderEnum(PropertyMeta& meta)
{
    i32_t* pVal = static_cast<i32_t*>(meta.pData);
    if (meta.vecEnumNames.empty()) return false;

    const char* preview = (*pVal >= 0 && *pVal < static_cast<i32_t>(meta.vecEnumNames.size()))
        ? meta.vecEnumNames[*pVal].c_str() : "???";

    bool changed = false;
    if (ImGui::BeginCombo(meta.strName.c_str(), preview))
    {
        for (i32_t i = 0; i < static_cast<i32_t>(meta.vecEnumNames.size()); ++i)
        {
            bool selected = (*pVal == i);
            if (ImGui::Selectable(meta.vecEnumNames[i].c_str(), selected))
            {
                *pVal = i;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool CPropertyEditor::RenderString(PropertyMeta& meta)
{
    std::string* pVal = static_cast<std::string*>(meta.pData);
    char buf[256]{};
    size_t len = pVal->size();
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    std::memcpy(buf, pVal->c_str(), len);

    if (ImGui::InputText(meta.strName.c_str(), buf, sizeof(buf)))
    {
        *pVal = buf;
        return true;
    }
    return false;
}
```

### 3.4 CEditorPanel.h

```cpp
// Engine/Public/Editor/CEditorPanel.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <string>

// ---------------------------------------------------------------
//  CEditorPanel -- abstract base for all dockable editor panels
//
//  Subclasses implement OnImGuiContent() with their specific UI.
//  The base handles ImGui::Begin/End, visibility toggle, and
//  docking configuration.
//
//  Convention: panel title = window title for ImGui docking.
// ---------------------------------------------------------------
class WINTERS_ENGINE CEditorPanel
{
public:
    virtual ~CEditorPanel() = default;

    // Called by CEditorApp each frame. Handles Begin/End.
    void OnImGui();

    // --- Accessors ---
    const std::string& GetTitle()   const { return m_strTitle; }
    bool IsVisible()                const { return m_bVisible; }
    void SetVisible(bool bVisible)        { m_bVisible = bVisible; }
    void ToggleVisible()                  { m_bVisible = !m_bVisible; }

protected:
    CEditorPanel(const std::string& strTitle)
        : m_strTitle(strTitle) {}

    // Override this to draw panel content (between Begin/End)
    virtual void OnImGuiContent() = 0;

    // Optional: override to customize window flags
    virtual int GetWindowFlags() const { return 0; }

    std::string m_strTitle;
    bool        m_bVisible = true;
};
```

### 3.5 CEditorPanel.cpp

```cpp
// Engine/Private/Editor/CEditorPanel.cpp
#include "Editor/CEditorPanel.h"

#ifdef _DEBUG
#undef new
#endif
#include <imgui.h>
#ifdef _DEBUG
#define new DBG_NEW
#endif

void CEditorPanel::OnImGui()
{
    if (!m_bVisible) return;

    ImGuiWindowFlags flags = static_cast<ImGuiWindowFlags>(GetWindowFlags());

    if (ImGui::Begin(m_strTitle.c_str(), &m_bVisible, flags))
    {
        OnImGuiContent();
    }
    ImGui::End();
}
```

### 3.6 CDetailsPanel.h

```cpp
// Engine/Public/Editor/CDetailsPanel.h
#pragma once

#include "WintersAPI.h"
#include "Editor/CEditorPanel.h"
#include "Editor/EditorReflection.h"
#include "Editor/CPropertyEditor.h"
#include <functional>

// ---------------------------------------------------------------
//  CDetailsPanel -- auto-generated property inspector
//
//  Displays properties for the currently selected object.
//  The caller provides a CPropertyList (built from WPROPERTY
//  metadata or manually) via SetTarget().
//
//  Usage:
//    auto pDetails = std::make_unique<CDetailsPanel>();
//    pDetails->SetTarget("Irelia", propList);
//    // panel auto-renders sliders/pickers based on property types
// ---------------------------------------------------------------
class WINTERS_ENGINE CDetailsPanel : public CEditorPanel
{
public:
    CDetailsPanel() : CEditorPanel("Details") {}
    ~CDetailsPanel() override = default;

    void SetTarget(const std::string& strName, CPropertyList* pProps)
    {
        m_strTargetName = strName;
        m_pProperties   = pProps;
    }

    void ClearTarget()
    {
        m_strTargetName.clear();
        m_pProperties = nullptr;
    }

    bool HasTarget() const { return m_pProperties != nullptr; }

protected:
    void OnImGuiContent() override;

private:
    std::string     m_strTargetName;
    CPropertyList*  m_pProperties = nullptr;
};
```

### 3.7 CDetailsPanel.cpp

```cpp
// Engine/Private/Editor/CDetailsPanel.cpp
#include "Editor/CDetailsPanel.h"

#ifdef _DEBUG
#undef new
#endif
#include <imgui.h>
#ifdef _DEBUG
#define new DBG_NEW
#endif

void CDetailsPanel::OnImGuiContent()
{
    if (!m_pProperties)
    {
        ImGui::TextDisabled("No object selected");
        return;
    }

    // Header with target name
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", m_strTargetName.c_str());
    ImGui::Separator();

    // Render all properties via the type-dispatched editor
    CPropertyEditor::RenderPropertyList(*m_pProperties);
}
```

### 3.8 CWorldOutliner.h

```cpp
// Engine/Public/Editor/CWorldOutliner.h
#pragma once

#include "WintersAPI.h"
#include "Editor/CEditorPanel.h"
#include "ECS/Entity.h"
#include <functional>
#include <string>
#include <vector>

class CWorld;

// ---------------------------------------------------------------
//  EntityDisplayInfo -- cached display data for outliner tree
// ---------------------------------------------------------------
struct EntityDisplayInfo
{
    EntityID    id       = NULL_ENTITY;
    std::string strName  = {};
    std::string strType  = {};     // "Champion", "Minion", "FX", etc.
    bool        bAlive   = false;
};

// ---------------------------------------------------------------
//  CWorldOutliner -- entity hierarchy tree view
//
//  Displays all entities in the CWorld as a flat or grouped list.
//  Clicking an entity fires the OnEntitySelected callback, which
//  CEditorApp uses to populate CDetailsPanel.
//
//  Refreshes entity list periodically (not every frame).
// ---------------------------------------------------------------
class WINTERS_ENGINE CWorldOutliner : public CEditorPanel
{
public:
    CWorldOutliner() : CEditorPanel("World Outliner") {}
    ~CWorldOutliner() override = default;

    // Bind to the active world (called on scene change)
    void BindWorld(CWorld* pWorld) { m_pWorld = pWorld; }

    // Set callback when an entity is selected
    void SetOnEntitySelected(std::function<void(EntityID)> fn)
    {
        m_fnOnSelected = std::move(fn);
    }

    // Currently selected entity
    EntityID GetSelectedEntity() const { return m_SelectedEntity; }

    // Force refresh the entity list
    void RefreshEntityList();

protected:
    void OnImGuiContent() override;

private:
    CWorld*                              m_pWorld = nullptr;
    EntityID                             m_SelectedEntity = NULL_ENTITY;
    std::function<void(EntityID)>        m_fnOnSelected;
    std::vector<EntityDisplayInfo>       m_vecEntities;
    f32_t                                m_fRefreshTimer = 0.f;
    char                                 m_szFilter[128] = {};
};
```

### 3.9 CWorldOutliner.cpp

```cpp
// Engine/Private/Editor/CWorldOutliner.cpp
#include "Editor/CWorldOutliner.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#ifdef _DEBUG
#undef new
#endif
#include <imgui.h>
#ifdef _DEBUG
#define new DBG_NEW
#endif

void CWorldOutliner::RefreshEntityList()
{
    m_vecEntities.clear();
    if (!m_pWorld) return;

    // Iterate all alive entities in the world
    // CWorld exposes ForEach<TransformComponent> -- we use that to find all entities
    m_pWorld->ForEach<TransformComponent>(
        std::function<void(EntityID, TransformComponent&)>(
            [this](EntityID id, TransformComponent& /*tc*/)
            {
                EntityDisplayInfo info;
                info.id      = id;
                info.strName = "Entity_" + std::to_string(id);
                info.bAlive  = true;

                // Attempt to get a name component if available
                // (future: CNameComponent with string)
                info.strType = "GameObject";

                m_vecEntities.push_back(std::move(info));
            }));
}

void CWorldOutliner::OnImGuiContent()
{
    if (!m_pWorld)
    {
        ImGui::TextDisabled("No world bound");
        return;
    }

    // Refresh every 0.5 seconds (not every frame)
    // Timer is incremented externally; for simplicity we refresh on button
    if (ImGui::Button("Refresh"))
        RefreshEntityList();

    ImGui::SameLine();
    ImGui::Text("Entities: %u", static_cast<u32_t>(m_vecEntities.size()));

    // Filter
    ImGui::InputText("Filter", m_szFilter, sizeof(m_szFilter));
    std::string filter(m_szFilter);

    ImGui::Separator();

    // Entity list
    if (ImGui::BeginChild("EntityList", ImVec2(0, 0), true))
    {
        for (auto& info : m_vecEntities)
        {
            if (!filter.empty() && info.strName.find(filter) == std::string::npos)
                continue;

            bool bSelected = (info.id == m_SelectedEntity);
            std::string label = info.strName + " [" + info.strType + "]"
                + "##" + std::to_string(info.id);

            if (ImGui::Selectable(label.c_str(), bSelected))
            {
                m_SelectedEntity = info.id;
                if (m_fnOnSelected)
                    m_fnOnSelected(info.id);
            }
        }
    }
    ImGui::EndChild();
}
```

### 3.10 CEditorApp.h

```cpp
// Engine/Public/Editor/CEditorApp.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Editor/CEditorPanel.h"
#include "Editor/CDetailsPanel.h"
#include "Editor/CWorldOutliner.h"
#include "Editor/EditorReflection.h"
#include <memory>
#include <vector>
#include <string>

class CWorld;

// ---------------------------------------------------------------
//  CEditorApp -- editor application controller
//
//  Manages all editor panels, docking workspace layout, and
//  menu bar. Created by CEngineApp when WINTERS_EDITOR is defined.
//
//  Usage:
//    // In CEngineApp::Initialize():
//    m_pEditor = CEditorApp::Create();
//    m_pEditor->AddPanel(std::make_unique<CWorldOutliner>());
//    m_pEditor->AddPanel(std::make_unique<CDetailsPanel>());
//
//    // In CEngineApp render loop:
//    m_pEditor->OnImGui();
// ---------------------------------------------------------------
class WINTERS_ENGINE CEditorApp
{
public:
    ~CEditorApp() = default;
    CEditorApp(const CEditorApp&) = delete;
    CEditorApp& operator=(const CEditorApp&) = delete;

    static std::unique_ptr<CEditorApp> Create();

    // Add a panel (takes ownership)
    void AddPanel(std::unique_ptr<CEditorPanel> pPanel);

    // Find panel by title
    CEditorPanel* FindPanel(const std::string& strTitle);

    // Bind world for outliner and viewport
    void BindWorld(CWorld* pWorld);

    // Main ImGui render call (called between ImGui BeginFrame/EndFrame)
    void OnImGui();

    // Toggle editor visibility
    void SetVisible(bool bVisible) { m_bVisible = bVisible; }
    bool IsVisible() const { return m_bVisible; }
    void Toggle() { m_bVisible = !m_bVisible; }

    // Access typed panels
    CDetailsPanel*   GetDetailsPanel();
    CWorldOutliner*  GetWorldOutliner();

private:
    CEditorApp() = default;

    void RenderMenuBar();
    void SetupDockspace();

    bool m_bVisible         = true;
    bool m_bDockspaceSetup  = false;

    std::vector<std::unique_ptr<CEditorPanel>> m_vecPanels;
    CWorld* m_pWorld = nullptr;
};
```

### 3.11 CEditorApp.cpp

```cpp
// Engine/Private/Editor/CEditorApp.cpp
#include "Editor/CEditorApp.h"
#include "ECS/World.h"

#ifdef _DEBUG
#undef new
#endif
#include <imgui.h>
#ifdef _DEBUG
#define new DBG_NEW
#endif

std::unique_ptr<CEditorApp> CEditorApp::Create()
{
    auto p = std::unique_ptr<CEditorApp>(new CEditorApp());

    // Create default panels
    auto pOutliner = std::make_unique<CWorldOutliner>();
    auto pDetails  = std::make_unique<CDetailsPanel>();

    // Wire outliner selection -> details panel
    CDetailsPanel* pDetailsRaw = pDetails.get();
    pOutliner->SetOnEntitySelected([pDetailsRaw](EntityID id) {
        // When an entity is selected, build a property list for it
        // This is a simplified example -- in production, the entity's
        // components would be introspected to build the property list.
        (void)id;
        (void)pDetailsRaw;
    });

    p->AddPanel(std::move(pOutliner));
    p->AddPanel(std::move(pDetails));

    return p;
}

void CEditorApp::AddPanel(std::unique_ptr<CEditorPanel> pPanel)
{
    if (pPanel)
        m_vecPanels.push_back(std::move(pPanel));
}

CEditorPanel* CEditorApp::FindPanel(const std::string& strTitle)
{
    for (auto& p : m_vecPanels)
    {
        if (p->GetTitle() == strTitle)
            return p.get();
    }
    return nullptr;
}

void CEditorApp::BindWorld(CWorld* pWorld)
{
    m_pWorld = pWorld;

    // Bind to outliner
    auto* pOutliner = GetWorldOutliner();
    if (pOutliner)
        pOutliner->BindWorld(pWorld);
}

CDetailsPanel* CEditorApp::GetDetailsPanel()
{
    return dynamic_cast<CDetailsPanel*>(FindPanel("Details"));
}

CWorldOutliner* CEditorApp::GetWorldOutliner()
{
    return dynamic_cast<CWorldOutliner*>(FindPanel("World Outliner"));
}

void CEditorApp::SetupDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("WintersEditorDockspace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // Create dockspace
    ImGuiID dockspaceID = ImGui::GetID("WintersDockspace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    // First-time layout setup
    if (!m_bDockspaceSetup)
    {
        m_bDockspaceSetup = true;

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

        // Split: left (outliner) | center (viewport) | right (details)
        ImGuiID dockLeft, dockCenter, dockRight;
        ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.2f,
                                     &dockLeft, &dockCenter);
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f,
                                     &dockRight, &dockCenter);

        ImGui::DockBuilderDockWindow("World Outliner", dockLeft);
        ImGui::DockBuilderDockWindow("Details", dockRight);
        // Center is for the 3D viewport (future CViewport panel)

        ImGui::DockBuilderFinish(dockspaceID);
    }

    // Menu bar
    RenderMenuBar();

    ImGui::End(); // WintersEditorDockspace
}

void CEditorApp::RenderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))  { /* TODO */ }
        if (ImGui::MenuItem("Load Scene", "Ctrl+O"))  { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))                   { /* TODO */ }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z"))  { /* TODO: Undo stack */ }
        if (ImGui::MenuItem("Redo", "Ctrl+Y"))  { /* TODO: Redo stack */ }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        for (auto& panel : m_vecPanels)
        {
            bool bVisible = panel->IsVisible();
            if (ImGui::MenuItem(panel->GetTitle().c_str(), nullptr, &bVisible))
                panel->SetVisible(bVisible);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools"))
    {
        if (ImGui::MenuItem("Asset Browser"))
        {
            auto* p = FindPanel("Content Browser");
            if (p) p->SetVisible(true);
        }
        if (ImGui::MenuItem("Profiler"))      { /* TODO */ }
        if (ImGui::MenuItem("Shader Reload")) { /* TODO */ }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void CEditorApp::OnImGui()
{
    if (!m_bVisible) return;

    // Setup dockspace (includes menu bar)
    SetupDockspace();

    // Render all panels
    for (auto& panel : m_vecPanels)
    {
        panel->OnImGui();
    }
}
```

---

## 4. Usage Examples

### 4.1 Migration: Before (scattered ImGui in Scene_InGame)

```cpp
// Scene_InGame.cpp OnImGui() -- current pattern (scattered, not reusable)
void CScene_InGame::OnImGui()
{
    if (ImGui::Begin("Camera"))
    {
        Vec3 eye = m_pCamera->GetEye();
        ImGui::Text("Eye: (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        bool bFollow = m_pCamera->IsFollowing();
        if (ImGui::Checkbox("Follow Mode (F2)", &bFollow))
            m_pCamera->SetFollowing(bFollow);
    }
    ImGui::End();

    // 50+ more lines of manual ImGui code for champion tuning...
    if (ImGui::Begin("Champion Tuner"))
    {
        ImGui::SliderFloat("Attack Speed", &m_fAS, 0.5f, 2.5f);
        ImGui::SliderFloat("Move Speed", &m_fMS, 200.f, 600.f);
        // ... 30 more sliders ...
    }
    ImGui::End();
}
```

### 4.2 Migration: After (framework-driven)

```cpp
// Scene_InGame.cpp -- new pattern with editor framework
void CScene_InGame::OnEnter()
{
    // Build property list for champion tuner (once)
    m_ChampionProps.AddFloat("Attack Speed", "Combat", &m_fAS, 0.5f, 2.5f);
    m_ChampionProps.AddFloat("Move Speed", "Combat", &m_fMS, 200.f, 600.f);
    m_ChampionProps.AddFloat3("Spawn Position", "Transform", &m_vSpawnPos);
    m_ChampionProps.AddColor4("Tint Color", "Visual", &m_vTintColor);
    m_ChampionProps.AddBool("God Mode", "Debug", &m_bGodMode);
    m_ChampionProps.AddEnum("Team", "Info", &m_iTeam,
        { "Blue", "Red", "Neutral" });

    // Register with editor
    auto* pEditor = CGameInstance::Get()->GetEditor();
    if (pEditor)
    {
        pEditor->BindWorld(&m_World);
        pEditor->GetDetailsPanel()->SetTarget("Champion Tuner", &m_ChampionProps);
    }
}

void CScene_InGame::OnImGui()
{
    // All editor UI is handled by CEditorApp::OnImGui()
    // Scene only needs game-specific overlays (minimap, HUD) that are NOT editor panels
}
```

### 4.3 Custom Panel Example: Material Editor

```cpp
// Client-side custom panel
class CMaterialEditorPanel : public CEditorPanel
{
public:
    CMaterialEditorPanel()
        : CEditorPanel("Material Editor") {}

    void SetMaterial(Engine::CMaterialPBR* pMat)
    {
        m_pMaterial = pMat;
        RebuildProps();
    }

protected:
    void OnImGuiContent() override
    {
        if (!m_pMaterial)
        {
            ImGui::TextDisabled("No material selected");
            return;
        }
        CPropertyEditor::RenderPropertyList(m_Props);
    }

private:
    void RebuildProps()
    {
        m_Props.Clear();
        // Get mutable reference to constants for slider binding
        // (In production, CMaterialPBR would expose mutable getters)
        m_Props.AddFloat("Metallic",  "PBR", &m_fMetallic,  0.f, 1.f, 0.01f,
            [this]() { m_pMaterial->SetMetallic(m_fMetallic); });
        m_Props.AddFloat("Roughness", "PBR", &m_fRoughness, 0.f, 1.f, 0.01f,
            [this]() { m_pMaterial->SetRoughness(m_fRoughness); });
        m_Props.AddFloat("AO",        "PBR", &m_fAO,        0.f, 1.f, 0.01f,
            [this]() { m_pMaterial->SetAmbientOcclusion(m_fAO); });
        m_Props.AddFloat("Emissive",  "PBR", &m_fEmissive,  0.f, 10.f, 0.1f,
            [this]() { m_pMaterial->SetEmissiveIntensity(m_fEmissive); });
    }

    Engine::CMaterialPBR* m_pMaterial = nullptr;
    CPropertyList m_Props;
    f32_t m_fMetallic  = 0.f;
    f32_t m_fRoughness = 0.5f;
    f32_t m_fAO        = 1.f;
    f32_t m_fEmissive  = 0.f;
};
```

### 4.4 Integration with CEngineApp

```cpp
// Engine/Private/Framework/CEngineApp.cpp -- adding editor support

bool CEngineApp::Initialize(IWintersApp* pGameApp, const EngineConfig& config)
{
    // ... existing initialization ...

#ifdef WINTERS_EDITOR
    m_pEditor = CEditorApp::Create();

    // Add custom panels
    m_pEditor->AddPanel(std::make_unique<CMaterialEditorPanel>());

    // Toggle with F10
    // (handled in Update: if key pressed, m_pEditor->Toggle())
#endif

    return true;
}

void CEngineApp::Render()
{
    m_pDevice->BeginFrame();
    m_ImGui.BeginFrame();

    // ... game rendering ...

#ifdef WINTERS_EDITOR
    if (m_pEditor && m_pEditor->IsVisible())
        m_pEditor->OnImGui();
#endif

    m_ImGui.EndFrame();
    m_pDevice->EndFrame();
}
```

---

## 5. Docking Workspace Layout

Default layout when editor opens:

```
+-------------------+-------------------------------+-------------------+
| World Outliner    |                               | Details           |
|                   |        3D Viewport            |                   |
| Entity_1 [Champ]  |        (CViewport)            | -- Transform --   |
| Entity_2 [Minion] |                               | Position: ...     |
| Entity_3 [FX]     |                               | Rotation: ...     |
| ...               |                               | -- Material --    |
|                   |                               | Metallic: [==]    |
|                   |                               | Roughness: [==]   |
+-------------------+-------------------------------+-------------------+
|              Content Browser / Asset Registry / Console               |
+-----------------------------------------------------------------------+
```

ImGui docking IDs are set up in `CEditorApp::SetupDockspace()` on first frame.
Users can drag panels to rearrange. Layout persists in `imgui.ini`.

---

## 6. Verification Checklist

| # | Check | Pass Criteria |
|---|---|---|
| 1 | `CEditorApp::Create()` succeeds | Non-null, default panels registered |
| 2 | Dockspace renders | Full-window dock area visible with menu bar |
| 3 | World Outliner shows entities | Entity list populated after `RefreshEntityList()` |
| 4 | Entity selection fires callback | Click entity -> `CDetailsPanel` updates |
| 5 | `CPropertyEditor` renders all types | Float, Vec3, Color4, Bool, Enum widgets visible |
| 6 | Slider changes modify data | Drag metallic slider -> material updates in real-time |
| 7 | Panel visibility toggle works | View menu checkboxes show/hide panels |
| 8 | `WINTERS_EDITOR` ifdef compiles clean | Debug builds include editor, Release excludes |
| 9 | No ImGui ID conflicts | Multiple panels open simultaneously without widget crosstalk |
| 10 | Existing Scene_InGame ImGui unchanged | Old code still works alongside new framework |

---

## 7. Migration Strategy

### Phase 1: Framework Installation (this plan)
- Add `CEditorPanel`, `CPropertyEditor`, `CEditorApp` to Engine
- Wire into `CEngineApp` render loop under `WINTERS_EDITOR`
- Existing scattered ImGui code continues to work untouched

### Phase 2: Panel Extraction
- Extract `ChampionTuner` ImGui block from Scene_InGame -> `CChampionTunerPanel`
- Extract `Camera` debug -> `CCameraPanel`
- Extract FPS/profiler overlay -> `CProfilerPanel` (already partially exists)

### Phase 3: Details Panel + Reflection
- Add `CNameComponent` to ECS for meaningful entity names
- Implement component introspection (iterate entity's components, build CPropertyList)
- Auto-generate Details panel content from any selected entity

### Phase 4: Content Browser + Asset Import
- `CContentBrowser` panel shows `CAssetRegistry` contents (depends on Stage 8)
- Drag-drop `.fbx` / `.png` triggers `CAssetImporter` pipeline
- Thumbnails rendered via offscreen render-to-texture

### Phase 5: Viewport Gizmos
- `CViewport` panel with translate/rotate/scale gizmos (ImGuizmo integration)
- Entity picking via mouse ray-cast
- Multi-select with box selection
