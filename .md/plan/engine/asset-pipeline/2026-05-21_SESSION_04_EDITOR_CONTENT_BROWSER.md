Session - Scene_Editor에 AssetDatabase 기반 Content Browser 패널을 붙인다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_Editor.h

기존 코드:

```cpp
#include "Renderer/ModelRenderer.h"
#include "Core/CTransform.h"
```

아래로 교체:

```cpp
#include "Renderer/ModelRenderer.h"
#include "Asset/AssetDatabase.h"
#include "Core/CTransform.h"
```

기존 코드:

```cpp
    // 마우스 좌클릭 에지 감지 (GetAsyncKeyState 직접 사용)
    unique_ptr<Engine::CNavGrid> m_pEditorNavGrid;
    int32_t m_iNavBrushRadiusCells = 2;
    bool_t m_bShowNavGridOverlay = true;

    // ImGui 패널
    void Render_MenuBar();
    void Render_Palette();
    void Render_Hierarchy();
    void Render_Inspector();
```

아래로 교체:

```cpp
    // 마우스 좌클릭 에지 감지 (GetAsyncKeyState 직접 사용)
    unique_ptr<Engine::CNavGrid> m_pEditorNavGrid;
    int32_t m_iNavBrushRadiusCells = 2;
    bool_t m_bShowNavGridOverlay = true;

    // 에셋 브라우저
    Engine::CAssetDatabase m_EditorAssetDatabase{};
    char m_szAssetFilter[128]{};
    int32_t m_iSelectedAssetIndex = -1;

    // ImGui 패널
    void Render_MenuBar();
    void Render_Palette();
    void Render_Hierarchy();
    void Render_Inspector();
    void RenderAssetBrowser();
    void RefreshAssetDatabase();
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Editor.cpp

기존 코드:

```cpp
#include "WintersPaths.h"
#include "GameInstance.h"
#include "Map/MapDataIO.h"

using namespace Winters::Map;
```

아래로 교체:

```cpp
#include "WintersPaths.h"
#include "GameInstance.h"
#include "Map/MapDataIO.h"
#include "Asset/AssetPath.h"
#include "Asset/AssetManifest.h"

using namespace Winters::Map;

namespace
{
    std::string WideToUtf8ForEditor(const std::wstring& text)
    {
        if (text.empty())
            return {};

        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
            return {};

        std::string out(static_cast<size_t>(count - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), count, nullptr, nullptr);
        return out;
    }
}
```

기존 코드:

```cpp
    // 싱글턴 Manager 는 CGameApp 에서 이미 Initialize 됨 → 현재 Stage 자동 로드
    Load_CurrentStage();
    LoadCurrentNavGrid();

    // 에디터는 WP 편집 전용 — 스폰 비활성
```

아래로 교체:

```cpp
    // 싱글턴 Manager 는 CGameApp 에서 이미 Initialize 됨 → 현재 Stage 자동 로드
    Load_CurrentStage();
    LoadCurrentNavGrid();
    RefreshAssetDatabase();

    // 에디터는 WP 편집 전용 — 스폰 비활성
```

기존 코드:

```cpp
void CScene_Editor::OnImGui()
{
    Render_MenuBar();
    Render_Palette();
    Render_Hierarchy();
    Render_Inspector();
}
```

아래로 교체:

```cpp
void CScene_Editor::OnImGui()
{
    Render_MenuBar();
    Render_Palette();
    Render_Hierarchy();
    Render_Inspector();
    RenderAssetBrowser();
}
```

기존 코드:

```cpp
        if (ImGui::MenuItem("Load Stage + NavGrid"))
        {
            Load_CurrentStage();
            LoadCurrentNavGrid();
        }
        ImGui::Separator();
```

아래로 교체:

```cpp
        if (ImGui::MenuItem("Load Stage + NavGrid"))
        {
            Load_CurrentStage();
            LoadCurrentNavGrid();
        }
        if (ImGui::MenuItem("Refresh Assets"))
        {
            RefreshAssetDatabase();
        }
        ImGui::Separator();
```

기존 코드:

```cpp
void CScene_Editor::Handle_MousePlacement()
{
```

아래에 추가:

```cpp
void CScene_Editor::RefreshAssetDatabase()
{
    m_EditorAssetDatabase.Clear();
    m_EditorAssetDatabase.ScanManifestDirectory(L"Data");
    m_EditorAssetDatabase.ScanManifestDirectory(L"Client/Bin/Resource");
    m_iSelectedAssetIndex = -1;
}

void CScene_Editor::RenderAssetBrowser()
{
    ImGui::SetNextWindowPos(ImVec2(260, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Content Browser"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh"))
        RefreshAssetDatabase();

    ImGui::SameLine();
    ImGui::Text("Assets: %u", m_EditorAssetDatabase.GetRecordCount());
    ImGui::InputText("Filter", m_szAssetFilter, sizeof(m_szAssetFilter));

    const std::string filter = Engine::CAssetPath::ToLowerAscii(m_szAssetFilter);
    const auto& records = m_EditorAssetDatabase.GetRecords();

    if (ImGui::BeginChild("AssetList", ImVec2(0, 120), true))
    {
        for (size_t i = 0; i < records.size(); ++i)
        {
            const Engine::AssetRecord& record = records[i];
            const std::string virtualPath = Engine::CAssetPath::ToLowerAscii(record.strVirtualPath);
            if (!filter.empty() && virtualPath.find(filter) == std::string::npos)
                continue;

            const bool selected = m_iSelectedAssetIndex == static_cast<int32_t>(i);
            const std::string label = record.strVirtualPath + "##asset" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), selected))
                m_iSelectedAssetIndex = static_cast<int32_t>(i);
        }
    }
    ImGui::EndChild();

    if (m_iSelectedAssetIndex >= 0 &&
        m_iSelectedAssetIndex < static_cast<int32_t>(records.size()))
    {
        const Engine::AssetRecord& record = records[static_cast<size_t>(m_iSelectedAssetIndex)];
        ImGui::Separator();
        ImGui::Text("Kind: %s", Engine::CAssetManifest::ToString(record.eKind));
        ImGui::Text("Status: %s", Engine::CAssetManifest::ToString(record.eImportStatus));
        ImGui::TextWrapped("Source: %s", WideToUtf8ForEditor(record.strSourcePath).c_str());
        ImGui::TextWrapped("Cooked: %s", WideToUtf8ForEditor(record.strCookedPath).c_str());
        if (!record.strLastError.empty())
            ImGui::TextColored(ImVec4(1.f, 0.35f, 0.25f, 1.f), "%s", record.strLastError.c_str());

        ImGui::BeginDisabled();
        ImGui::Button("Reimport");
        ImGui::SameLine();
        ImGui::Button("Load Preview");
        ImGui::EndDisabled();
    }

    ImGui::End();
}

void CScene_Editor::Handle_MousePlacement()
{
```

2. 검증

미검증:
- 에디터 Content Browser 렌더링 미검증
- `.wasset`가 없는 기존 resource 폴더에서 빈 목록으로 조용히 동작하는지 미검증

검증 명령:
- git diff --check
- msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64

수동 확인:
- F5로 Editor scene 진입 후 `Content Browser` 패널이 열린다.
- `File > Refresh Assets`를 눌러도 크래시가 없다.
- Session 02에서 생성한 `.wasset`가 목록에 표시된다.

확인 필요:
- `Reimport`와 `Load Preview` 버튼은 Session 05에서 실제 job queue와 preview loader에 연결한다.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
