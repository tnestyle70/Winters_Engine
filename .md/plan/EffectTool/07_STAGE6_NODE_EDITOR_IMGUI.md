# Stage 6 — ImGui 노드 에디터 + 프리뷰 뷰포트 + 파라미터 인스펙터

## 목표

**Niagara / FXR 에디터** 의 축소판. 프로그래머가 JSON 을 쓰지 않고 **GUI** 로 그래프를 구축한다.
Scene_FxNodeEditor 로 독립 씬 전환 또는 Scene_Editor 안의 탭 중 하나.

## 왜 Phase 1 MVP 뒤로 미루는가

- JSON 직접 편집으로도 이펙트 제작 가능 (Stage 1~5 완료 시점)
- 노드 에디터는 **개발 생산성 도구** — 제품이 아님. 선순위는 게임 기능
- 노드 에디터 없이도 "챔피언 스킬별 에셋" 은 JSON 으로 확보 가능
- 혼자 개발 중이면 JSON / C++ 코드로 생성 가능 → 에디터는 아티스트 합류 또는 수량 증가 시 필요

## imgui-node-editor 통합

Thedmd / **imgui-node-editor** 가 표준 선택지 (CC0 or MIT).
Winters 의 **ThirdPartyLib 자립 원칙** (.md/build/THIRDPARTY_INTEGRATION_GUIDE.md) 에 따라 편입:

```
Engine/ThirdPartyLib/imgui-node-editor/
├── Inc/imgui_node_editor/
│   ├── imgui_node_editor.h
│   ├── imgui_node_editor_internal.h
│   └── ...
├── Lib/Debug/   — 사용 안 함 (헤더 + .cpp 직접 빌드)
└── Src/         — .cpp 는 Engine 프로젝트가 직접 컴파일
```

정적 라이브러리화 대신 **.cpp 를 Engine.vcxproj 에 소스로 편입** (크기 작음, ImGui 와 동일 패턴).

## 에디터 화면 레이아웃

```
┌──────────────────────────────────────────────────────────────────┐
│ File  Edit  View  Compile  Help                                  │
├──────────────┬───────────────────────────────┬───────────────────┤
│              │                               │                   │
│  [Palette]   │                               │  [Inspector]      │
│  ─────────   │                               │  ────────         │
│  Spawn       │         Node Graph Canvas     │  Selected:        │
│    Burst     │                               │   SpawnBurst #1   │
│    Rate      │     ┌───SpawnBurst───┐        │                   │
│  Init        │     │count = 30      │        │  count [30    ]   │
│    Pos       │     └───┬────────────┘        │                   │
│    Vel       │         │                     │                   │
│    Color     │     ┌───▼InitPosSphere┐       │  [Apply]          │
│    Size      │     │radius = 0.2    │        │                   │
│    Life      │     └───┬────────────┘        │                   │
│  Update      │         │                     │                   │
│    Gravity   │         ▼                     │                   │
│    Drag      │                               │                   │
│    Color     │                               │                   │
│    Age/Kill  │                               │                   │
│  Expression  │                               │                   │
│    Const     │                               │                   │
│    Attr      │                               │                   │
│    BinOp     │                               │                   │
│              │                               │                   │
├──────────────┴───────────────────────────────┼───────────────────┤
│  [Preview Viewport]          [Timeline]      │  [Asset Browser]  │
│   ┌──────────────────┐       ◀ 0.3s / 2.0s   │  FX/              │
│   │                  │       ▶ 1.0x          │  └ Champions/     │
│   │   [불꽃 프리뷰]   │       ⟲ Restart       │    ├ Irelia_Q.fxg │
│   │                  │                       │    └ Yasuo_R.fxg  │
│   └──────────────────┘                       │                   │
└──────────────────────────────────────────────┴───────────────────┘
```

## FxNodeEditorPanel

```cpp
// Engine/Public/FX/Editor/FxNodeEditorPanel.h
#pragma once
#include "FxGraph.h"

namespace ax::NodeEditor { struct EditorContext; using Config = struct Config; }

namespace Engine::FX::Editor {

class CFxNodeEditorPanel
{
public:
    static std::unique_ptr<CFxNodeEditorPanel> Create();
    ~CFxNodeEditorPanel();

    void OnImGui();                 // 탭 전체 렌더

    // 현재 편집 중인 에셋
    void  SetAsset(class CFxAsset* asset);
    CFxAsset* GetAsset() { return m_pAsset; }

    // 프리뷰용 이미터 핸들
    CEmitterInstance* GetPreviewEmitter() { return m_pPreviewEmitter.get(); }

    // 저장 / 로드
    void SaveCurrentAsset();
    void LoadAsset(const std::wstring& path);

private:
    CFxNodeEditorPanel() = default;

    // 서브 섹션
    void DrawPalette();             // 좌측 노드 팔레트
    void DrawCanvas();              // 중앙 노드 그래프
    void DrawInspector();           // 우측 선택 노드 파라미터
    void DrawPreview();             // 하단 프리뷰 뷰포트
    void DrawAssetBrowser();        // 하단 우측 파일 브라우저

    // 에디터 상태
    ax::NodeEditor::EditorContext* m_pNodeEditor = nullptr;
    CFxAsset*                      m_pAsset      = nullptr;
    NodeId                         m_selectedNode = NULL_NODE;
    PinId                          m_selectedPin  = NULL_PIN;

    // 프리뷰
    std::unique_ptr<CEmitterInstance> m_pPreviewEmitter;
    f32_t                          m_previewTime      = 0.f;
    f32_t                          m_previewTimeScale = 1.f;
    bool_t                         m_previewPaused    = false;

    // 저장 상태
    std::wstring                   m_assetPath;
    bool_t                         m_bDirty = false;
};

} // namespace Engine::FX::Editor
```

## Palette

노드 종류 목록. 드래그해서 캔버스에 드롭하면 해당 타입 노드 생성.

```cpp
void CFxNodeEditorPanel::DrawPalette()
{
    ImGui::BeginChild("Palette", ImVec2(220, 0), true);

    if (ImGui::TreeNodeEx("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawPaletteEntry("Burst",  eNodeKind::SpawnBurst, eStage::Spawn);
        DrawPaletteEntry("Rate",   eNodeKind::SpawnRate,  eStage::Spawn);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Initialize", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawPaletteEntry("Position (Sphere)", eNodeKind::InitPositionSphere, eStage::Spawn);
        DrawPaletteEntry("Position (Box)",    eNodeKind::InitPositionBox,    eStage::Spawn);
        DrawPaletteEntry("Velocity (Cone)",   eNodeKind::InitVelocityCone,   eStage::Spawn);
        DrawPaletteEntry("Color",             eNodeKind::InitColor,          eStage::Spawn);
        DrawPaletteEntry("Size",              eNodeKind::InitSize,           eStage::Spawn);
        DrawPaletteEntry("Lifetime",          eNodeKind::InitLifetime,       eStage::Spawn);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Update", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawPaletteEntry("Gravity",           eNodeKind::UpdateGravity,           eStage::Update);
        DrawPaletteEntry("Drag",              eNodeKind::UpdateDrag,              eStage::Update);
        DrawPaletteEntry("Curl Noise",        eNodeKind::UpdateCurlNoise,         eStage::Update);
        DrawPaletteEntry("Color over Life",   eNodeKind::UpdateColorOverLife,     eStage::Update);
        DrawPaletteEntry("Size over Life",    eNodeKind::UpdateSizeOverLife,      eStage::Update);
        DrawPaletteEntry("Integrate Pos",     eNodeKind::UpdateIntegratePosition, eStage::Update);
        DrawPaletteEntry("Age + Kill",        eNodeKind::UpdateAgeAndKill,        eStage::Update);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Expression"))
    {
        DrawPaletteEntry("Const",       eNodeKind::ExprConst,     eStage::Update);
        DrawPaletteEntry("Attr Read",   eNodeKind::ExprAttrRead,  eStage::Update);
        DrawPaletteEntry("BinOp",       eNodeKind::ExprBinOp,     eStage::Update);
        DrawPaletteEntry("Unary",       eNodeKind::ExprUnaryOp,   eStage::Update);
        DrawPaletteEntry("Lerp",        eNodeKind::ExprLerp,      eStage::Update);
        DrawPaletteEntry("Attr Write",  eNodeKind::ExprAttrWrite, eStage::Update);
        ImGui::TreePop();
    }

    ImGui::EndChild();
}

void CFxNodeEditorPanel::DrawPaletteEntry(const char* label, eNodeKind kind, eStage stage)
{
    if (ImGui::Selectable(label))
    {
        // 캔버스 중앙에 노드 추가
        if (m_pAsset)
        {
            NodeId id = m_pAsset->GetGraphMutable().AddNode(kind, stage);
            m_pAsset->GetGraphMutable().GetMutable(id).editorPos = {200, 200};
            m_bDirty = true;
        }
    }
}
```

## Canvas

imgui-node-editor API 사용. 핵심 호출:

```cpp
void CFxNodeEditorPanel::DrawCanvas()
{
    namespace NED = ax::NodeEditor;
    NED::SetCurrentEditor(m_pNodeEditor);
    NED::Begin("FxGraph Canvas");

    if (!m_pAsset) { NED::End(); return; }
    auto& g = m_pAsset->GetGraphMutable();

    // 노드 그리기
    for (auto& [id, node] : g.AllNodes())
    {
        NED::BeginNode(NED::NodeId(id));
            ImGui::TextUnformatted(NodeKindToDisplay(node.kind));

            // 입력 핀
            for (const auto& pin : node.inputs) {
                NED::BeginPin(NED::PinId(pin.id), NED::PinKind::Input);
                    ImGui::Text("  > %s", pin.name.c_str());
                NED::EndPin();
            }
            // 출력 핀
            for (const auto& pin : node.outputs) {
                NED::BeginPin(NED::PinId(pin.id), NED::PinKind::Output);
                    ImGui::Text("%s >", pin.name.c_str());
                NED::EndPin();
            }
        NED::EndNode();
    }

    // 엣지 그리기
    for (const auto& e : g.AllEdges()) {
        // 각 엣지의 link id 는 hash (uint64) 로 구성
        std::uint64_t lid = (std::uint64_t(e.fromNode) << 32) | e.toNode;
        NED::Link(NED::LinkId(lid), NED::PinId(e.fromPin), NED::PinId(e.toPin));
    }

    // 새 엣지 생성 이벤트 처리
    if (NED::BeginCreate()) {
        NED::PinId from, to;
        if (NED::QueryNewLink(&from, &to)) {
            if (NED::AcceptNewItem()) {
                // pin → node 매핑 찾아서 g.Connect 호출
                NodeId fromNode = FindNodeByPin(g, from.Get());
                NodeId toNode   = FindNodeByPin(g, to.Get());
                if (fromNode && toNode) {
                    g.Connect(fromNode, from.Get(), toNode, to.Get());
                    m_bDirty = true;
                }
            }
        }
    }
    NED::EndCreate();

    // 삭제 이벤트
    if (NED::BeginDelete()) {
        NED::LinkId lid;
        while (NED::QueryDeletedLink(&lid)) {
            if (NED::AcceptDeletedItem()) {
                // lid 를 엣지로 역변환 후 g.Disconnect
                // (link id 는 from/to node hash — 저장 구조 맞게 처리)
            }
        }
        NED::NodeId nid;
        while (NED::QueryDeletedNode(&nid)) {
            if (NED::AcceptDeletedItem()) {
                g.RemoveNode(NodeId(nid.Get()));
                m_bDirty = true;
            }
        }
    }
    NED::EndDelete();

    // 선택
    if (NED::GetSelectedObjectCount() > 0) {
        std::vector<NED::NodeId> sel(1);
        NED::GetSelectedNodes(sel.data(), 1);
        m_selectedNode = NodeId(sel[0].Get());
    }

    NED::End();
    NED::SetCurrentEditor(nullptr);
}
```

## Inspector — 선택 노드 파라미터

각 `eNodeKind` 별로 UI 템플릿이 다름. switch 로 푸는 게 가장 단순:

```cpp
void CFxNodeEditorPanel::DrawInspector()
{
    ImGui::BeginChild("Inspector", ImVec2(280, 0), true);
    if (!m_pAsset || m_selectedNode == NULL_NODE) {
        ImGui::TextDisabled("No node selected");
        ImGui::EndChild();
        return;
    }

    auto& g = m_pAsset->GetGraphMutable();
    if (!g.Exists(m_selectedNode)) { ImGui::EndChild(); return; }
    Node& n = g.GetMutable(m_selectedNode);

    ImGui::Text("Node #%u  [%s]", n.id, NodeKindToDisplay(n.kind));
    ImGui::Separator();

    switch (n.kind)
    {
    case eNodeKind::SpawnBurst: {
        std::int32_t count = std::get<std::int32_t>(n.params.at("count"));
        if (ImGui::SliderInt("count", &count, 1, 500)) {
            n.params["count"] = count;  m_bDirty = true;
        }
    } break;

    case eNodeKind::SpawnRate: {
        f32_t rate = std::get<f32_t>(n.params.at("rate"));
        if (ImGui::SliderFloat("rate (p/s)", &rate, 0.f, 1000.f)) {
            n.params["rate"] = rate;  m_bDirty = true;
        }
    } break;

    case eNodeKind::InitPositionSphere: {
        f32_t r = std::get<f32_t>(n.params.at("radius"));
        if (ImGui::SliderFloat("radius", &r, 0.f, 10.f)) {
            n.params["radius"] = r;  m_bDirty = true;
        }
    } break;

    case eNodeKind::InitVelocityCone: {
        Vec3 dir = std::get<Vec3>(n.params.at("direction"));
        f32_t half = std::get<f32_t>(n.params.at("coneAngleRad"));
        f32_t smn  = std::get<f32_t>(n.params.at("speedMin"));
        f32_t smx  = std::get<f32_t>(n.params.at("speedMax"));
        float d[3] = { dir.x, dir.y, dir.z };
        if (ImGui::SliderFloat3("direction", d, -1.f, 1.f)) {
            n.params["direction"] = Vec3{ d[0], d[1], d[2] };  m_bDirty = true;
        }
        if (ImGui::SliderAngle("cone half-angle", &half, 0.f, 180.f)) {
            n.params["coneAngleRad"] = half;  m_bDirty = true;
        }
        if (ImGui::SliderFloat("speed min", &smn, 0.f, 30.f)) {
            n.params["speedMin"] = smn;  m_bDirty = true;
        }
        if (ImGui::SliderFloat("speed max", &smx, 0.f, 30.f)) {
            n.params["speedMax"] = smx;  m_bDirty = true;
        }
    } break;

    case eNodeKind::InitColor: {
        Vec4 c = std::get<Vec4>(n.params.at("color"));
        float col[4] = { c.x, c.y, c.z, c.w };
        if (ImGui::ColorEdit4("color", col)) {
            n.params["color"] = Vec4{ col[0], col[1], col[2], col[3] };
            m_bDirty = true;
        }
    } break;

    case eNodeKind::InitLifetime:
    case eNodeKind::InitSize: {
        f32_t mn = std::get<f32_t>(n.params.at("min"));
        f32_t mx = std::get<f32_t>(n.params.at("max"));
        if (ImGui::SliderFloat("min", &mn, 0.f, 10.f)) { n.params["min"] = mn; m_bDirty = true; }
        if (ImGui::SliderFloat("max", &mx, 0.f, 10.f)) { n.params["max"] = mx; m_bDirty = true; }
    } break;

    case eNodeKind::UpdateGravity: {
        Vec3 g3 = std::get<Vec3>(n.params.at("gravity"));
        float v[3] = { g3.x, g3.y, g3.z };
        if (ImGui::SliderFloat3("gravity", v, -30.f, 30.f)) {
            n.params["gravity"] = Vec3{ v[0], v[1], v[2] };  m_bDirty = true;
        }
    } break;

    case eNodeKind::UpdateDrag: {
        f32_t k = std::get<f32_t>(n.params.at("drag"));
        if (ImGui::SliderFloat("drag (1/s)", &k, 0.f, 20.f)) {
            n.params["drag"] = k;  m_bDirty = true;
        }
    } break;

    case eNodeKind::UpdateColorOverLife: {
        DrawColorKeyframesEditor(n);   // 키프레임 타임라인 별도 위젯
    } break;

    // ... 기타 노드 ...

    default:
        ImGui::TextDisabled("(no parameters)");
        break;
    }

    ImGui::Separator();
    if (ImGui::Button("Delete Node")) {
        g.RemoveNode(m_selectedNode);
        m_selectedNode = NULL_NODE;
        m_bDirty = true;
    }

    ImGui::EndChild();
}
```

## Preview Viewport

별도 RenderTarget 에 FX 만 그려서 ImGui::Image 로 표시.

```cpp
void CFxNodeEditorPanel::DrawPreview()
{
    ImGui::BeginChild("Preview", ImVec2(0, 220), true);

    ImGui::Text("Preview");
    ImGui::SameLine();
    if (ImGui::Button(m_previewPaused ? "▶" : "⏸")) m_previewPaused = !m_previewPaused;
    ImGui::SameLine();
    if (ImGui::Button("⟲")) RestartPreview();
    ImGui::SameLine();
    ImGui::SliderFloat("speed", &m_previewTimeScale, 0.1f, 3.f);
    ImGui::SameLine();
    ImGui::Text("%.2fs", m_previewTime);

    // 렌더타겟 텍스처 → ImGui::Image
    if (m_pPreviewRTV_SRV) {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_pPreviewRTV_SRV), ImVec2(400, 180));
    }

    ImGui::EndChild();
}

void CFxNodeEditorPanel::TickPreview(f32_t dt)
{
    if (m_previewPaused || !m_pPreviewEmitter) return;
    m_previewTime += dt * m_previewTimeScale;
    m_pPreviewEmitter->Tick(dt * m_previewTimeScale);
}

void CFxNodeEditorPanel::RestartPreview()
{
    if (!m_pAsset) return;

    // 현재 그래프로 새 이미터 재생성
    m_pPreviewEmitter = CEmitterInstance::Create(m_pAsset->GetGraph(), 4096, 0xCAFEBABEu, false);
    m_previewTime = 0.f;
}
```

### Preview RT 를 만드는 것

`CEngineApp::Render` 플로우에 **preview 전용 RT** 추가:
1. RT 스위치 → Clear (투명/검정)
2. CFxRenderSystem 에 preview 카메라 주입
3. Emitter 1개만 렌더
4. 메인 backbuffer 로 복귀

ImGui 는 `ID3D11ShaderResourceView*` 를 `ImTextureID` 로 캐스팅해 표시 가능.

## Asset Browser

```cpp
void CFxNodeEditorPanel::DrawAssetBrowser()
{
    ImGui::BeginChild("AssetBrowser", ImVec2(0, 220), true);
    ImGui::Text("FX Assets");
    ImGui::Separator();

    // "Client/Bin/Resource/FX/" 이하 재귀 파일 리스트
    DrawFolder(L"Resource/FX");

    ImGui::EndChild();
}

void CFxNodeEditorPanel::DrawFolder(const std::wstring& relPath)
{
    // WintersPaths 로 full path 해결
    // WIN32 FindFirstFileW 로 *.fxg 파일 열거
    // 각 파일에 대해 Selectable → LoadAsset(path)
    // 하위 폴더는 TreeNode 로 재귀
}
```

## 저장 / 로드

```cpp
void CFxNodeEditorPanel::SaveCurrentAsset()
{
    if (!m_pAsset || m_assetPath.empty()) return;
    m_pAsset->SaveToFile(m_assetPath);
    m_bDirty = false;
}

void CFxNodeEditorPanel::LoadAsset(const std::wstring& path)
{
    auto asset = CFxAsset::LoadFromFile(path);
    if (!asset) return;
    m_pAsset    = asset.get();      // 소유권은 AssetLibrary
    m_assetPath = path;
    m_bDirty    = false;
    RestartPreview();
}
```

Ctrl+S 단축키 (ImGui 이벤트 후킹) 로 저장.

## Scene_FxNodeEditor

```cpp
// Client/Public/Scene/Scene_FxNodeEditor.h
#pragma once
#include "IScene.h"

class Scene_FxNodeEditor : public IScene
{
public:
    static std::unique_ptr<Scene_FxNodeEditor> Create();

    HRESULT OnEnter() override;
    void    OnUpdate(f32_t dt) override;
    void    OnRender() override;
    void    OnImGui() override;
    void    OnExit()  override;

private:
    Scene_FxNodeEditor() = default;

    std::unique_ptr<Engine::FX::Editor::CFxNodeEditorPanel> m_pPanel;

    // 현재 편집 중인 에셋 소유권
    std::unique_ptr<Engine::FX::CFxAsset> m_pCurrentAsset;
};
```

**진입**: 메인 메뉴 또는 `Scene_InGame` 에서 `Ctrl+Shift+F` 로 진입 (인게임 FX 튜닝용).
`Change_Scene` 직후 `return` 필수 (CLAUDE.md Gotcha: Scene 즉시 self-destruct).

## 연습모드 통합 — 인게임 에디팅

Phase B-5 "에디터가 곧 게임 모드" 철학.

```
Practice Mode (Scene_InGame 확장)
  ├── 챔피언 + 맵 표시
  ├── ImGui 에 "FX Editor Panel" 탭
  └── 선택된 스킬의 FX 를 실시간 편집 → 즉시 반영
```

즉 Scene 전환 없이 `Scene_InGame::OnImGui` 안에서 `CFxNodeEditorPanel::OnImGui()` 호출.
게임 화면과 에디터가 한 화면.

## Keyframe Timeline — 색/크기 커브 편집

```cpp
void CFxNodeEditorPanel::DrawColorKeyframesEditor(Node& n)
{
    // params["keyframes"] 가 JSON 기반 raw → 파싱된 vector<ColorKey> 캐시 필요
    auto& keys = *std::any_cast<std::vector<ColorKey>>(&n.params.at("keyframes_parsed"));

    ImGui::Text("Color Keyframes");
    if (ImGui::Button("+ Add")) {
        keys.push_back({ 0.5f, Vec4{1,1,1,1} });
        std::sort(keys.begin(), keys.end(), [](auto& a, auto& b){ return a.t < b.t; });
        m_bDirty = true;
    }

    // 타임라인 바 (ImDrawList 로 커스텀 그림)
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImVec2(ImGui::GetContentRegionAvail().x, 40);
    ImGui::Dummy(sz);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(40, 40, 40, 255));

    for (auto& k : keys) {
        f32_t x = p0.x + sz.x * k.t;
        ImU32 c = IM_COL32(
            int(k.color.x * 255), int(k.color.y * 255),
            int(k.color.z * 255), int(k.color.w * 255));
        dl->AddCircleFilled({x, p0.y + sz.y * 0.5f}, 6, c);
    }

    // 각 키프레임 상세 편집 (간단)
    for (std::size_t i = 0; i < keys.size(); ++i) {
        ImGui::PushID(int(i));
        ImGui::SliderFloat("t", &keys[i].t, 0.f, 1.f);
        float col[4] = { keys[i].color.x, keys[i].color.y, keys[i].color.z, keys[i].color.w };
        if (ImGui::ColorEdit4("color", col)) {
            keys[i].color = { col[0], col[1], col[2], col[3] };
            m_bDirty = true;
        }
        ImGui::PopID();
    }
}
```

## 유효성 오버레이

`CFxGraphValidator` 결과를 캔버스 위에 표시:

```cpp
auto issues = CFxGraphValidator::Validate(g);
for (const auto& iss : issues) {
    if (iss.level == ValidationIssue::Level::Error)
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "ERROR: %s", iss.message.c_str());
    else if (iss.level == ValidationIssue::Level::Warning)
        ImGui::TextColored(ImVec4(1,1,0.3f,1), "WARN: %s", iss.message.c_str());
}
```

## Gotchas

- **imgui-node-editor 초기화 순서**: `NED::CreateEditor()` 는 ImGui 컨텍스트 있는 상태에서 호출. `Scene_FxNodeEditor::OnEnter` 에서 수행
- **pin id / node id 충돌**: imgui-node-editor 는 pin / node / link id 를 전역 unique 로 요구. `NodeId = 32bit`, `PinId = 32bit` 겹칠 수 있음 → 상위 1 bit 를 namespace 로 사용 (`0x8000_0000 | pinId`)
- **`std::any_cast` 예외**: params 에 컴파일된 바이트코드 / 파싱된 keyframe 을 any 로 저장하면 타입 맞추기 까다로움. 차라리 `CompiledExpr / std::vector<ColorKey>` 를 Node 멤버로 직접 추가
- **에디터 상태 퍼시스턴스**: 노드 위치 / 줌 / 선택 은 세션 간 유지하고 싶을 수도. imgui-node-editor 는 `.imgui_node_editor.json` 자동 저장 — 프로젝트 루트에 생기지 않게 WintersPaths 의 사용자 설정 폴더로 리다이렉트
- **프리뷰 RT 크기 동적 변경**: ImGui 창 크기 리사이즈 시 RT 재할당. 프레임마다 새로 만들면 누수 → `EnsureSize` 패턴
- **Scene 전환 시 리소스 반환**: OnExit 에서 프리뷰 RT / NodeEditor context 명시적 파괴. ComPtr 이어도 명시적 nullptr 으로 참조 끊기 권장
- **Ctrl+Z undo**: Phase 1 MVP 에서는 구현 생략 (너무 복잡). 대신 JSON 자동 저장 (30 초 주기) 로 복구 가능
- **한글 노드 이름**: Asset 의 `displayName` 은 한글 가능 (UTF-8). ImGui::Text 는 UTF-8 OK. 폰트는 한글 글리프 포함해야 (기존 Phase C-2 Font 추가 시점과 맞물림)

## 단축키

| 키 | 기능 |
|---|---|
| Ctrl+S | 현재 에셋 저장 |
| Ctrl+Shift+S | 다른 이름으로 저장 |
| Ctrl+N | 빈 에셋 생성 |
| Ctrl+O | 에셋 로드 다이얼로그 |
| Delete | 선택 노드 / 엣지 삭제 |
| Space | 프리뷰 재생 / 일시정지 |
| R | 프리뷰 재시작 |
| F | 선택 노드 프레이밍 (캔버스 센터) |
| Tab | 노드 검색 (퀵 애드) |

## 실행 흐름 (한 프레임)

```
Scene_FxNodeEditor::OnUpdate(dt)
    m_pPanel->TickPreview(dt)
        m_pPreviewEmitter->Tick(dt)     (Stage 3 Executor)

Scene_FxNodeEditor::OnRender()
    [RT = preview]
    FxRenderSystem (preview camera)     (Stage 5)
    [RT = backbuffer]
    // 메인 화면 clear

Scene_FxNodeEditor::OnImGui()
    m_pPanel->OnImGui()
        DrawPalette()
        DrawCanvas()            ← imgui-node-editor
        DrawInspector()
        DrawPreview()           ← preview RT SRV 표시
        DrawAssetBrowser()
```

## 구현 순서

1. imgui-node-editor ThirdPartyLib 편입 + Engine.vcxproj 에 .cpp 편입
2. `FxNodeEditorPanel.h` / `.cpp` 뼈대 — 빈 캔버스만 먼저
3. Palette 드래그 앤 드롭 → 노드 생성
4. Canvas 노드 그리기 (BeginNode/EndNode) + 엣지 그리기
5. 엣지 생성 / 삭제 이벤트 연동
6. 선택 노드 Inspector 스위치문 (우선 5 종만)
7. 저장 / 로드 (Ctrl+S)
8. Preview RT 구성 + `ImGui::Image` 표시
9. 프리뷰 Tick / Pause / Restart / 속도 슬라이더
10. AssetBrowser (FindFirstFileW 로 `.fxg` 열거)
11. 키프레임 에디터 위젯 (Color/Size)
12. Scene_FxNodeEditor 진입 (Ctrl+Shift+F)
13. Scene_InGame 연습모드 탭으로 통합

## 다음 Stage

Stage 7 — GPU Compute 백엔드. 현재는 CPU 루프. 100K+ 파티클이 필요하면 Compute Shader 에서 같은 그래프를 실행.
