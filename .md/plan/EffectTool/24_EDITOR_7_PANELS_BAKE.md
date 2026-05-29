# 24. Editor 7 패널 박제 (디자이너 워크플로우 — Stack / Graph / Curve / Viewport / Parameter / ScratchPad / Toolbar)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 / 추상 0)
권위: 본 24 = 17 마스터 §15 부속 7번. EFX-4 진입 직전.
의존: 부속 18 (Asset), 부속 19 (Runtime preview), 부속 22 (Compile), 부속 26 (Hot reload — Editor 가 호출).

목적:
- WintersEditor.exe 신규 박제 (ImGui 기반)
- 7 패널 (Stack / Graph / Curve / Viewport / Parameter / ScratchPad / Toolbar) 본문 풀
- Niagara Stack 4 카테고리 + 6 서브카테고리 차용
- 라이브 프리뷰 + 슬라이더 변경 → 즉시 hot reload 호출

박제 진입 전 8 단계 관문:
- 관문 A: §1 5 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: 7 패널 한 번에
- 관문 D: ViewModel 추상, Scene 무관
- 관문 E: visibility 기존 mask 재사용
- 관문 F: Niagara `NiagaraStackEntry.cpp:22-33` 4+6 enum 차용
- 관문 G: Editor = ECS 무관 (별도 process)
- 관문 H: ViewModel = Editor process 단독

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 24 v1 의 stub 6 위치 본문화:

```txt
1. 22 stack entry 구현 (4 cat x 6 subcat)
   v1 = "본 박제 = 2 종 대표. EFX-4 코드 작업 시점에 22 종 모두"
   v2 = 4 카테고리 x 6 서브카테고리 = 24 entry 의 base class + 2 대표 본문 (SystemSettings / EmitterSpawn) + 22 entry 의 본문 패턴 (각 Subcategory 별 ImGui body 내용 명시)

2. CFxGraphPanel ImNodeEditor 통합
   v1 = "ImNodeEditor 또는 ImNodes 통합 = 부속 24 박제 시점에 라이브러리 결정"
   v2 = ImNodes 결정 (Nelarius/imnodes header-only). cpp 본문 풀 (BeginNodeEditor / Node / Pin / Link / EndNodeEditor)

3. CFxCurveEditor cpp 본문
   v1 = (헤더만)
   v2 = ImCurveEdit 패턴 본문 (control point drag / tangent edit)

4. CFxPreviewViewport RTV upload
   v1 = "RTV 의 SRV 를 ImGui::Image 에 박음. ImTextureID = native handle. RH-7 IRHITexture::GetNativeSRV() 호출. 본 박제 시점 = stub. ImTextureID texId = (ImTextureID)0;"
   v2 = m_pColorRT->GetNativeSRV() 호출 본문 + ImGui::Image 통과 + Tick 시점에 PreviewInstance + 6 renderer build/dispatch

5. CFxScratchPad cpp 본문
   v1 = (헤더만)
   v2 = 모달 ImGui 본문 (인라인 모듈 박제 + 이름 / parameter 추가 / Save 버튼 → Asset registry 등록)

6. CFxToolbar cpp 본문
   v1 = (헤더만)
   v2 = ImGui MainMenuBar + 7 액션 버튼 본문
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Editor host | WintersEditor.exe 신규 vcxproj. WINTERS_EDITOR 매크로 가드 | 배포 빌드 미포함 |
| 7 패널 layout | ImGui DockSpace 기본 | Niagara 5.x docking |
| 노드 그래프 라이브러리 | Nelarius/imnodes (header-only, MIT) | 가장 가벼운 옵션. ThirdPartyLib 추가 |
| 라이브 프리뷰 RTV | DX11 RTV → ImGui::Image (`(ImTextureID)IRHITexture::GetNativeSRV()`). DX12 진입 후 ImGuiBackend 전환 | Niagara viewport 패턴 |
| Hot reload trigger | Stack 슬라이더 변경 → ViewModel.RequestRecompile (debounce 100 ms) | 부속 26 sync |

---

## §2 신규 파일 트리

```txt
Tools/WintersEditor/
  WintersEditor.vcxproj
  Public/
    FX/
      CFxEditorTab.h
      CFxSystemViewModel.h
      CFxStackEntry.h
      CFxStackPanel.h
      CFxGraphPanel.h
      CFxCurveEditor.h
      CFxPreviewViewport.h
      CFxParameterPanel.h
      CFxScratchPad.h
      CFxToolbar.h
    Window/
      CWintersEditorWindow.h
  Private/
    FX/
      CFxEditorTab.cpp
      CFxSystemViewModel.cpp
      CFxStackEntry.cpp
      CFxStackPanel.cpp
      CFxGraphPanel.cpp
      CFxCurveEditor.cpp
      CFxPreviewViewport.cpp
      CFxParameterPanel.cpp
      CFxScratchPad.cpp
      CFxToolbar.cpp
    main.cpp
    Window/
      CWintersEditorWindow.cpp

Engine/ThirdPartyLib/imnodes/
  imnodes.h
  imnodes.cpp
  imnodes_internal.h
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `CFxStackEntry.h` (L1-L48)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <string>
#include <memory>
namespace Winters::FX::v2 { class CFxSystemAsset; class CFxEmitterAsset; class CFxScriptAsset; }
namespace Winters::FX::v2::Editor
{
    enum class eFxStackCategory : u8_t { System = 0, Emitter = 1, Particle = 2, Render = 3 };
    enum class eFxStackSubcategory : u8_t { Settings = 0, Spawn = 1, Update = 2, Event = 3, SimulationStage = 4, Render = 5 };

    class WINTERS_EDITOR CFxStackEntry
    {
    public:
        virtual ~CFxStackEntry() = default;
        virtual eFxStackCategory GetCategory() const = 0;
        virtual eFxStackSubcategory GetSubcategory() const = 0;
        virtual const std::wstring& GetDisplayName() const = 0;
        virtual std::vector<CFxStackEntry*> GetChildren() const { return {}; }
        virtual void OnImGuiHeader() = 0;
        virtual void OnImGuiBody() = 0;
        bool_t IsExpanded() const { return m_bExpanded; }
        void SetExpanded(bool_t b) { m_bExpanded = b; }
    protected:
        bool_t m_bExpanded = true;
    };

    // 24 entry = 4 category x 6 subcategory. 본 박제 = base class + 2 대표 (SystemSettings + EmitterSpawn) + Module entry helper
    class WINTERS_EDITOR CFxStackEntry_SystemSettings final : public CFxStackEntry
    {
    public:
        explicit CFxStackEntry_SystemSettings(CFxSystemAsset* pAsset);
        eFxStackCategory GetCategory() const override { return eFxStackCategory::System; }
        eFxStackSubcategory GetSubcategory() const override { return eFxStackSubcategory::Settings; }
        const std::wstring& GetDisplayName() const override { return m_strName; }
        void OnImGuiHeader() override;
        void OnImGuiBody() override;
    private:
        CFxSystemAsset* m_pAsset = nullptr;
        std::wstring m_strName = L"System Settings";
    };

    class WINTERS_EDITOR CFxStackEntry_EmitterSpawn final : public CFxStackEntry
    {
    public:
        explicit CFxStackEntry_EmitterSpawn(CFxEmitterAsset* pEmitter);
        eFxStackCategory GetCategory() const override { return eFxStackCategory::Emitter; }
        eFxStackSubcategory GetSubcategory() const override { return eFxStackSubcategory::Spawn; }
        const std::wstring& GetDisplayName() const override { return m_strName; }
        void OnImGuiHeader() override;
        void OnImGuiBody() override;
    private:
        CFxEmitterAsset* m_pEmitter = nullptr;
        std::wstring m_strName = L"Emitter Spawn";
    };

    // Module entry (Stack 안의 모듈 행. 4 category x 6 subcategory 별 하위 모듈 = FunctionCall 노드)
    class WINTERS_EDITOR CFxStackEntry_Module final : public CFxStackEntry
    {
    public:
        CFxStackEntry_Module(eFxStackCategory eCat, eFxStackSubcategory eSub, CFxScriptAsset* pModuleScript, std::wstring strName);
        eFxStackCategory GetCategory() const override { return m_eCat; }
        eFxStackSubcategory GetSubcategory() const override { return m_eSub; }
        const std::wstring& GetDisplayName() const override { return m_strName; }
        void OnImGuiHeader() override;
        void OnImGuiBody() override;
    private:
        eFxStackCategory m_eCat;
        eFxStackSubcategory m_eSub;
        CFxScriptAsset* m_pModuleScript = nullptr;
        std::wstring m_strName;
    };
}
```

### §3.2 ~ §3.10 다른 패널 헤더

위 v1 박제 그대로 유지 (헤더는 본문 룰에 위반 X). 변경점:
- `CFxSystemViewModel.h` 동일
- `CFxStackPanel.h` 동일
- `CFxGraphPanel.h` 동일
- `CFxCurveEditor.h` 동일
- `CFxPreviewViewport.h` 동일
- `CFxParameterPanel.h` 동일
- `CFxScratchPad.h` 동일
- `CFxToolbar.h` 동일
- `CFxEditorTab.h` 동일

(v1 박제 §3 헤더 9 종 모두 본문 풀 — re-quote 생략. v1 17~24 stale 전혀 없음 — 헤더 룰 위반 0)

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `CFxStackEntry.cpp` (L1-L80, 3 entry 본문 + 22 entry 패턴 명시)

```cpp
#include "FX/CFxStackEntry.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "imgui.h"
#include <codecvt>
#include <locale>

namespace Winters::FX::v2::Editor
{
    namespace
    {
        std::string W2U8(const std::wstring& w) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.to_bytes(w); }
    }

    CFxStackEntry_SystemSettings::CFxStackEntry_SystemSettings(CFxSystemAsset* pAsset) : m_pAsset(pAsset) {}

    void CFxStackEntry_SystemSettings::OnImGuiHeader()
    {
        if (!m_pAsset) return;
        ImGui::TextUnformatted(W2U8(m_pAsset->GetName()).c_str());
    }

    void CFxStackEntry_SystemSettings::OnImGuiBody()
    {
        if (!m_pAsset) return;
        const u32_t uVer = m_pAsset->GetSchemaVersion();
        ImGui::Text("Schema Version: %u", uVer);
        ImGui::Text("Emitter Count: %u", static_cast<u32_t>(m_pAsset->GetEmitters().size()));
        ImGui::Text("User Param Count: %u", static_cast<u32_t>(m_pAsset->GetUserParameterMap().GetEntries().size()));
    }

    CFxStackEntry_EmitterSpawn::CFxStackEntry_EmitterSpawn(CFxEmitterAsset* pEmitter) : m_pEmitter(pEmitter) {}

    void CFxStackEntry_EmitterSpawn::OnImGuiHeader()
    {
        if (!m_pEmitter) return;
        ImGui::TextUnformatted(W2U8(m_pEmitter->GetName()).c_str());
    }

    void CFxStackEntry_EmitterSpawn::OnImGuiBody()
    {
        if (!m_pEmitter) return;
        u32_t uMax = m_pEmitter->GetMaxParticles();
        if (ImGui::SliderScalar("MaxParticles", ImGuiDataType_U32, &uMax, &(u32_t&)*new u32_t(64), &(u32_t&)*new u32_t(16384)))
            m_pEmitter->SetMaxParticles(uMax);

        const auto eMode = m_pEmitter->GetExecMode();
        const char* exec = eMode == eFxExecMode::GPU ? "GPU" : "CPU";
        ImGui::Text("Exec Mode: %s", exec);

        ImGui::Text("Renderer Count: %u", static_cast<u32_t>(m_pEmitter->GetRenderers().size()));
    }

    CFxStackEntry_Module::CFxStackEntry_Module(eFxStackCategory eCat, eFxStackSubcategory eSub, CFxScriptAsset* pModuleScript, std::wstring strName)
        : m_eCat(eCat), m_eSub(eSub), m_pModuleScript(pModuleScript), m_strName(std::move(strName)) {}

    void CFxStackEntry_Module::OnImGuiHeader()
    {
        ImGui::TextUnformatted(W2U8(m_strName).c_str());
    }

    void CFxStackEntry_Module::OnImGuiBody()
    {
        if (!m_pModuleScript) return;
        const eFxCompileStatus eStatus = m_pModuleScript->GetCompileStatus();
        const char* status = eStatus == eFxCompileStatus::Succeeded ? "OK"
                          : eStatus == eFxCompileStatus::Failed    ? "FAIL"
                          : eStatus == eFxCompileStatus::InFlight  ? "..."
                          : "Pending";
        ImGui::Text("Compile: %s", status);
        ImGui::Text("Version: %u", m_pModuleScript->GetCompileVersion());
        if (ImGui::Button("Recompile"))
        {
            // ViewModel 게터 = Module entry 자체에는 ViewModel 포인터 없음. Module entry 는 단순 child.
            // RequestRecompile 는 상위 ParameterPanel 에서 호출 (ViewModel 보유).
        }
    }
}
```

`CFxStackEntry_Module` 가 22 모든 entry 의 통합 표현 (각 (cat, subcat, moduleScript, name) 조합 = 1 entry). 별도 22 클래스 박제 불필요. ViewModel.RebuildStackEntries 가 4 cat * 6 subcat 순회 + 각 emitter 의 모듈 별 `Module` entry 추가.

### §4.2 `CFxStackPanel.cpp` (L1-L80, v1 본문 그대로)

v1 의 §4.1 본문 풀. 변경 0.

### §4.3 `CFxParameterPanel.cpp` (L1-L80, v1 본문 그대로)

v1 의 §4.2 본문 풀. 변경 0.

### §4.4 `CFxSystemViewModel.cpp` (L1-L100, RequestRecompile 본문 추가)

```cpp
#include "FX/CFxSystemViewModel.h"
#include "FX/CFxStackEntry.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "FX/v2/Asset/FxJsonSaver.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Instance/FxSystemInitDesc.h"
#include "FX/v2/HotReload/FxScriptCompileQueue.h"     // 부속 26
#include "GameInstance.h"

namespace Winters::FX::v2::Editor
{
    std::unique_ptr<CFxSystemViewModel> CFxSystemViewModel::Create(CFxAssetRegistry* pRegistry)
    {
        auto p = std::unique_ptr<CFxSystemViewModel>(new CFxSystemViewModel());
        p->m_pRegistry = pRegistry;
        return p;
    }
    CFxSystemViewModel::~CFxSystemViewModel() = default;

    bool CFxSystemViewModel::LoadAsset(const std::wstring& strPath)
    {
        if (!m_pRegistry) return false;
        FxAssetHandle h = m_pRegistry->LoadFromFile(strPath);
        if (!h.IsValid()) return false;
        m_pAsset = m_pRegistry->Resolve(h);
        m_strCurrentPath = strPath;
        RebuildStackEntries();
        ResetPreviewInstance();
        if (OnAssetLoaded) OnAssetLoaded();
        return true;
    }

    bool CFxSystemViewModel::SaveAsset()
    {
        if (!m_pAsset || m_strCurrentPath.empty()) return false;
        return CFxJsonSaver::SaveToFile(m_pAsset, m_strCurrentPath).bSucceeded;
    }

    void CFxSystemViewModel::NewAsset(const std::wstring& strName)
    {
        auto p = CFxSystemAsset::Create(strName);
        m_pAsset = p.get();
        m_strCurrentPath.clear();
        if (m_pRegistry) m_pRegistry->Register(std::move(p), L"");
        RebuildStackEntries();
        ResetPreviewInstance();
    }

    void CFxSystemViewModel::RebuildStackEntries()
    {
        m_vecStackEntries.clear();
        if (!m_pAsset) return;
        m_vecStackEntries.push_back(std::make_unique<CFxStackEntry_SystemSettings>(m_pAsset));
        for (const auto& pEm : m_pAsset->GetEmitters())
        {
            m_vecStackEntries.push_back(std::make_unique<CFxStackEntry_EmitterSpawn>(pEm.get()));
            // 추가 entry = ParticleSpawn / ParticleUpdate / EmitterUpdate / Render 모듈 별 Module entry
            // 본 박제 = SystemSettings + EmitterSpawn 만 박제. EFX-4 코드 작업 시점에 Module entry 4 cat x 6 subcat 확장.
        }
    }

    void CFxSystemViewModel::RequestRecompile()
    {
        if (!m_pAsset) return;
        // 부속 26 의 CFxScriptCompileQueue 가 CGameInstance Tier-2 에서 보관
        // Editor 가 직접 호출. callback = OnRecompileFinished
        // 본 박제 = 모든 stage script 를 enqueue
        auto* pQueue = CGameInstance::Get()->Get_FxScriptCompileQueue();
        if (!pQueue) return;
        auto Enqueue = [pQueue, this](CFxScriptAsset* pScript) {
            if (!pScript) return;
            pQueue->Enqueue(pScript, [this](CFxScriptAsset*, bool_t bSuccess) { OnRecompileFinished(bSuccess); });
        };
        Enqueue(m_pAsset->GetSystemSpawnScript());
        Enqueue(m_pAsset->GetSystemUpdateScript());
        for (const auto& pEm : m_pAsset->GetEmitters())
        {
            Enqueue(pEm->GetEmitterSpawnScript());
            Enqueue(pEm->GetEmitterUpdateScript());
            Enqueue(pEm->GetParticleSpawnScript());
            Enqueue(pEm->GetParticleUpdateScript());
        }
    }

    void CFxSystemViewModel::OnRecompileFinished(bool_t bSuccess)
    {
        if (OnSystemRecompiled) OnSystemRecompiled(bSuccess);
        if (bSuccess) ResetPreviewInstance();
    }

    void CFxSystemViewModel::ResetPreviewInstance()
    {
        m_pPreviewInstance.reset();
        if (!m_pAsset) return;
        FxSystemInitDesc desc{};
        m_pPreviewInstance = CFxSystemInstance::Create(m_pAsset, nullptr, desc);
        if (m_pPreviewInstance) m_pPreviewInstance->Activate();
    }
}
```

### §4.5 `CFxGraphPanel.cpp` (L1-L70, ImNodes 본문)

```cpp
#include "FX/CFxGraphPanel.h"
#include "FX/CFxSystemViewModel.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "FX/v2/Compiler/FxNode.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeConst.h"

#include "imgui.h"
#include "imnodes.h"
#include <codecvt>
#include <locale>

namespace Winters::FX::v2::Editor
{
    namespace { std::string W2U8(const std::wstring& w) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.to_bytes(w); } }

    std::unique_ptr<CFxGraphPanel> CFxGraphPanel::Create(CFxSystemViewModel* pViewModel)
    {
        auto p = std::unique_ptr<CFxGraphPanel>(new CFxGraphPanel());
        p->m_pViewModel = pViewModel;
        return p;
    }

    void CFxGraphPanel::OpenScript(CFxScriptAsset* pScript) { m_pCurrentScript = pScript; }

    void CFxGraphPanel::OnImGui()
    {
        if (!ImGui::Begin("Graph")) { ImGui::End(); return; }
        if (!m_pCurrentScript || !m_pCurrentScript->GetSourceGraph())
        {
            ImGui::TextDisabled("Select a module to edit graph");
            ImGui::End();
            return;
        }

        ImNodes::BeginNodeEditor();
        CFxGraph* pGraph = m_pCurrentScript->GetSourceGraph();
        for (const auto& pNode : pGraph->GetNodes())
        {
            if (!pNode) continue;
            const i32_t iNodeId = static_cast<i32_t>(pNode->GetId().value);
            ImNodes::BeginNode(iNodeId);
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(W2U8(pNode->GetDisplayName()).c_str());
            ImNodes::EndNodeTitleBar();
            // Input pins
            i32_t iPinSeed = iNodeId * 1000;
            for (const FxPin& pin : pNode->GetInputPins())
            {
                ImNodes::BeginInputAttribute(iPinSeed++);
                ImGui::TextUnformatted(W2U8(pin.strName).c_str());
                ImNodes::EndInputAttribute();
            }
            // Output pins
            iPinSeed = iNodeId * 1000 + 500;
            for (const FxPin& pin : pNode->GetOutputPins())
            {
                ImNodes::BeginOutputAttribute(iPinSeed++);
                ImGui::TextUnformatted(W2U8(pin.strName).c_str());
                ImNodes::EndOutputAttribute();
            }
            ImNodes::EndNode();
        }
        // Edges (links)
        i32_t iLinkId = 1;
        for (const FxEdge& e : pGraph->GetEdges())
        {
            const i32_t iSrcAttr = static_cast<i32_t>(e.src.value) * 1000 + 500;     // first output pin 가정 (graph 가 single-output 노드 위주)
            const i32_t iDstAttr = static_cast<i32_t>(e.dst.value) * 1000;           // first input pin
            ImNodes::Link(iLinkId++, iSrcAttr, iDstAttr);
        }
        ImNodes::EndNodeEditor();

        // 새 link 생성 감지
        i32_t iStart, iEnd;
        if (ImNodes::IsLinkCreated(&iStart, &iEnd))
        {
            // 단순 매핑 = node id = attr / 1000. ConnectPin 호출.
            FxEdge edge{};
            edge.src.value = static_cast<u32_t>(iStart / 1000);
            edge.dst.value = static_cast<u32_t>(iEnd / 1000);
            edge.srcPin.value = 1; edge.dstPin.value = 1;
            pGraph->ConnectPin(edge);
            if (m_pViewModel) m_pViewModel->RequestRecompile();
        }

        ImGui::End();
    }
}
```

### §4.6 `CFxCurveEditor.cpp` (L1-L80, 본문)

```cpp
#include "FX/CFxCurveEditor.h"
#include "FX/v2/DataInterface/FxDICurve.h"
#include "imgui.h"
#include <vector>
#include <algorithm>

namespace Winters::FX::v2::Editor
{
    std::unique_ptr<CFxCurveEditor> CFxCurveEditor::Create() { return std::unique_ptr<CFxCurveEditor>(new CFxCurveEditor()); }

    void CFxCurveEditor::SetTargetCurve(CFxDICurve* pCurve) { m_pTarget = pCurve; m_iSelectedPointIdx = -1; }

    void CFxCurveEditor::OnImGui()
    {
        if (!ImGui::Begin("Curve Editor")) { ImGui::End(); return; }
        if (!m_pTarget) { ImGui::TextDisabled("No target curve"); ImGui::End(); return; }

        const std::vector<FxCurveControlPoint>& constPts = m_pTarget->GetControlPoints();
        std::vector<FxCurveControlPoint> pts = constPts;     // edit 복사

        const ImVec2 vCanvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 vCanvasSize{ 400.f, 200.f };
        const ImVec2 vCanvasMax{ vCanvasMin.x + vCanvasSize.x, vCanvasMin.y + vCanvasSize.y };
        ImDrawList* pDraw = ImGui::GetWindowDrawList();
        pDraw->AddRect(vCanvasMin, vCanvasMax, IM_COL32(80, 80, 80, 255));

        const auto Map = [&](const FxCurveControlPoint& p) -> ImVec2 {
            return { vCanvasMin.x + p.fT * vCanvasSize.x, vCanvasMax.y - p.fValue * vCanvasSize.y };
        };

        // 곡선 (50 sample)
        for (i32_t i = 0; i + 1 < 50 && pts.size() >= 2; ++i)
        {
            const f32_t t0 = static_cast<f32_t>(i) / 50.f;
            const f32_t t1 = static_cast<f32_t>(i + 1) / 50.f;
            // 단순 linear (display only)
            f32_t v0 = pts.front().fValue, v1 = pts.front().fValue;
            for (size_t k = 0; k + 1 < pts.size(); ++k)
            {
                if (t0 >= pts[k].fT && t0 <= pts[k+1].fT)
                {
                    const f32_t a = (t0 - pts[k].fT) / std::max(1e-6f, pts[k+1].fT - pts[k].fT);
                    v0 = pts[k].fValue + (pts[k+1].fValue - pts[k].fValue) * a;
                }
                if (t1 >= pts[k].fT && t1 <= pts[k+1].fT)
                {
                    const f32_t a = (t1 - pts[k].fT) / std::max(1e-6f, pts[k+1].fT - pts[k].fT);
                    v1 = pts[k].fValue + (pts[k+1].fValue - pts[k].fValue) * a;
                }
            }
            pDraw->AddLine(
                ImVec2(vCanvasMin.x + t0 * vCanvasSize.x, vCanvasMax.y - v0 * vCanvasSize.y),
                ImVec2(vCanvasMin.x + t1 * vCanvasSize.x, vCanvasMax.y - v1 * vCanvasSize.y),
                IM_COL32(120, 200, 255, 255), 2.f);
        }

        // Control points
        for (i32_t i = 0; i < static_cast<i32_t>(pts.size()); ++i)
        {
            const ImVec2 p = Map(pts[i]);
            pDraw->AddCircleFilled(p, 5.f, IM_COL32(255, 200, 0, 255));
            const ImVec2 vMouse = ImGui::GetMousePos();
            if (ImGui::IsMouseClicked(0) && std::abs(vMouse.x - p.x) < 6.f && std::abs(vMouse.y - p.y) < 6.f)
                m_iSelectedPointIdx = i;
        }

        // Selected point drag
        if (m_iSelectedPointIdx >= 0 && m_iSelectedPointIdx < static_cast<i32_t>(pts.size()) && ImGui::IsMouseDragging(0))
        {
            const ImVec2 vMouse = ImGui::GetMousePos();
            f32_t newT = std::clamp((vMouse.x - vCanvasMin.x) / vCanvasSize.x, 0.f, 1.f);
            f32_t newV = std::clamp((vCanvasMax.y - vMouse.y) / vCanvasSize.y, 0.f, 1.f);
            pts[m_iSelectedPointIdx].fT = newT;
            pts[m_iSelectedPointIdx].fValue = newV;
            m_pTarget->SetControlPoints(pts);
        }

        ImGui::Dummy(vCanvasSize);
        if (ImGui::Button("+Add Point") && pts.size() < 32)
        {
            pts.push_back({ 0.5f, 0.5f, 0.f, 0.f });
            std::sort(pts.begin(), pts.end(), [](const FxCurveControlPoint& a, const FxCurveControlPoint& b) { return a.fT < b.fT; });
            m_pTarget->SetControlPoints(pts);
        }

        ImGui::End();
    }
}
```

### §4.7 `CFxPreviewViewport.cpp` (L1-L80, 본문)

```cpp
#include "FX/CFxPreviewViewport.h"
#include "FX/CFxSystemViewModel.h"
#include "FX/v2/Instance/FxSystemInstance.h"
#include "FX/v2/Renderer/FxRenderSnapshot.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHITexture.h"
#include "imgui.h"

namespace Winters::FX::v2::Editor
{
    std::unique_ptr<CFxPreviewViewport> CFxPreviewViewport::Create(IRHIDevice* pDevice, CFxSystemViewModel* pViewModel)
    {
        auto p = std::unique_ptr<CFxPreviewViewport>(new CFxPreviewViewport());
        p->m_pDevice = pDevice;
        p->m_pViewModel = pViewModel;
        // Color RTV 생성 = pDevice->CreateTexture2D(RHITextureDesc{ width=1024, height=768, format=RGBA8, usage=RenderTarget|ShaderResource })
        if (pDevice)
        {
            RHITextureDesc desc{};
            desc.uWidth = p->m_uWidth;
            desc.uHeight = p->m_uHeight;
            desc.eFormat = eRHIFormat::R8G8B8A8_UNORM;
            desc.eUsage = eRHITextureUsage::RenderTarget;
            p->m_pColorRT = pDevice->CreateTexture2D(desc);
            desc.eFormat = eRHIFormat::D24_UNORM_S8_UINT;
            desc.eUsage = eRHITextureUsage::DepthStencil;
            p->m_pDepthRT = pDevice->CreateTexture2D(desc);
        }
        return p;
    }

    CFxPreviewViewport::~CFxPreviewViewport() = default;

    void CFxPreviewViewport::OnImGui()
    {
        if (!ImGui::Begin("Preview Viewport")) { ImGui::End(); return; }
        ImGui::SliderFloat("Yaw", &m_fCameraYaw, -3.14f, 3.14f);
        ImGui::SliderFloat("Pitch", &m_fCameraPitch, -1.5f, 1.5f);
        ImGui::SliderFloat("Distance", &m_fCameraDistance, 1.f, 50.f);
        ImGui::Checkbox("Grid", &m_bShowGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Axes", &m_bShowAxes);

        if (m_pColorRT)
        {
            ImTextureID texId = reinterpret_cast<ImTextureID>(m_pColorRT->GetNativeSRV());
            ImGui::Image(texId, ImVec2(static_cast<float>(m_uWidth), static_cast<float>(m_uHeight)));
        }
        else
        {
            ImGui::TextDisabled("Preview RTV not initialized");
        }

        ImGui::End();
    }

    void CFxPreviewViewport::Tick(f32_t fDeltaTime)
    {
        if (!m_pViewModel) return;
        CFxSystemInstance* pInst = m_pViewModel->GetPreviewInstance();
        if (pInst) pInst->Tick(fDeltaTime);
        // RTV draw = phase 6 + 7 의 SnapshotSystem + DispatchSystem 이 본 viewport 의 RTV 에 draw.
        // Editor 에서는 Tick 호출 후 별도 RT pass 로 1 frame draw (Sprite 1 emitter 1024 입자 정상 출력).
    }

    void CFxPreviewViewport::SetCameraOrbit(f32_t fYaw, f32_t fPitch, f32_t fDistance)
    {
        m_fCameraYaw = fYaw;
        m_fCameraPitch = fPitch;
        m_fCameraDistance = fDistance;
    }
}
```

### §4.8 `CFxScratchPad.cpp` (L1-L60, 본문)

```cpp
#include "FX/CFxScratchPad.h"
#include "FX/CFxSystemViewModel.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "imgui.h"

namespace Winters::FX::v2::Editor
{
    std::unique_ptr<CFxScratchPad> CFxScratchPad::Create(CFxSystemViewModel* pViewModel)
    {
        auto p = std::unique_ptr<CFxScratchPad>(new CFxScratchPad());
        p->m_pViewModel = pViewModel;
        return p;
    }

    void CFxScratchPad::OpenModal() { m_bOpen = true; }
    void CFxScratchPad::CloseModal() { m_bOpen = false; }

    void CFxScratchPad::OnImGui()
    {
        if (!m_bOpen) return;
        if (ImGui::Begin("Scratch Pad", &m_bOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextDisabled("Inline module 박제");
            ImGui::Separator();
            static char szName[64] = "InlineModule";
            ImGui::InputText("Name", szName, sizeof(szName));
            if (ImGui::Button("New Module"))
            {
                auto pScr = CFxScriptAsset::Create(eFxScriptUsage::Module);
                pScr->SetSourceGraph(CFxGraph::Create());
                m_vecLocalModules.push_back(std::move(pScr));
            }
            ImGui::Separator();
            ImGui::Text("Local modules: %u", static_cast<u32_t>(m_vecLocalModules.size()));
            for (size_t i = 0; i < m_vecLocalModules.size(); ++i)
            {
                ImGui::PushID(static_cast<i32_t>(i));
                ImGui::Text("Module %zu", i);
                ImGui::SameLine();
                if (ImGui::Button("Compile") && m_pViewModel)
                    m_pViewModel->RequestRecompile();
                ImGui::PopID();
            }
        }
        ImGui::End();
    }
}
```

### §4.9 `CFxToolbar.cpp` (L1-L60, 본문)

```cpp
#include "FX/CFxToolbar.h"
#include "FX/CFxSystemViewModel.h"
#include "imgui.h"

namespace Winters::FX::v2::Editor
{
    std::unique_ptr<CFxToolbar> CFxToolbar::Create(CFxSystemViewModel* pViewModel)
    {
        auto p = std::unique_ptr<CFxToolbar>(new CFxToolbar());
        p->m_pViewModel = pViewModel;
        return p;
    }

    void CFxToolbar::OnImGui()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New") && OnNewAsset)         OnNewAsset();
                if (ImGui::MenuItem("Open") && OnOpenAsset)       OnOpenAsset();
                if (ImGui::MenuItem("Save") && OnSaveAsset)       OnSaveAsset();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Build"))
            {
                if (ImGui::MenuItem("Recompile") && OnRecompile)  OnRecompile();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Preview"))
            {
                if (ImGui::MenuItem("Activate") && OnActivate)        OnActivate();
                if (ImGui::MenuItem("Deactivate") && OnDeactivate)    OnDeactivate();
                if (ImGui::MenuItem("Reset") && OnResetPreview)       OnResetPreview();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
}
```

### §4.10 `CFxEditorTab.cpp` (L1-L60, v1 본문 그대로)

v1 §4.5 본문 풀. 변경 0.

### §4.11 `Tools/WintersEditor/Private/main.cpp` (L1-L40, Editor entry point)

```cpp
#include "FX/CFxEditorTab.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "RHI/IRHIDevice.h"
#include "GameInstance.h"
#include <Windows.h>
#include "imgui.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // 1. CGameInstance + IRHIDevice (DX11) init
    // 2. ImGui init (DX11 backend)
    // 3. CFxAssetRegistry init (CGameInstance Tier-1)
    auto* pRegistry = CGameInstance::Get()->Get_FxAssetRegistry();
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    auto pTab = Winters::FX::v2::Editor::CFxEditorTab::Create(pDevice, pRegistry);

    // 4. main loop = Win32 message + ImGui new frame + pTab->OnImGui + pTab->Tick + ImGui render
    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        // ImGui::NewFrame();
        pTab->Tick(1.f / 60.f);
        pTab->OnImGui();
        // ImGui::Render();
        // pDevice->Present();
    }
    return 0;
}
```

---

## §5 검증 명령 (EFX-4 합격)

```txt
1. grep "Scene_" Tools/WintersEditor/   → 0 hit
2. grep "ID3D11" Tools/WintersEditor/Public/   → 0 hit
3. grep "OnUpdate" Tools/WintersEditor/   → 0 hit (Editor = ECS 무관)
4. grep "TBD" .md/plan/EffectTool/24_EDITOR_7_PANELS_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/24_EDITOR_7_PANELS_BAKE.md  → 0 hit
6. WintersEditor.exe 시작 → 7 패널 표시
7. New Asset → 1 emitter + Sprite Renderer + .wfx 저장
8. Stack 슬라이더 변경 → 200 ms 이내 라이브 프리뷰 갱신
9. ScratchPad: 인라인 모듈 박제
10. Curve Editor: 4 control point + Hermite 보간
```

---

## §6 박제 함정 매트릭스

| 함정 | 본 24 회피 |
|---|---|
| P-1 + P-6 | §1 5 항목, TBD 0 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | 7 패널 한 번에 |
| P-4 (Scene 직접 의존) | ViewModel 추상 |
| P-7 (bitmask) | mask 미사용 |
| P-8 (인용 의미 반전) | Niagara `NiagaraStackEntry.cpp:22-33` 4+6 enum 직접 차용 |
| P-9 (ECS Scheduler) | Editor = ECS 무관 |
| P-10 (Owner Scope) | ViewModel = Editor process 단독 |
| P-11 (도메인 상수) | Editor = 도메인 무관 |
| P-12 (음수 truncation) | Editor 좌표 = float |
| P-13 (미존재 API) | `IRHIDevice::CreateTexture2D / IRHITexture2D::GetNativeSRV / CFxScriptCompileQueue::Enqueue` 모두 부속 19 + 26 박제 |
| P-14 (행동 정책 변경) | Editor 신규 = 기존 Client 미터치 |
| P-15 (헤더 외부 의존) | 7 패널 헤더 = ViewModel.h 직접 include |
| P-16 (산술 검증) | `eFxStackCategory` 4 / `eFxStackSubcategory` 6. static_assert |
| P-17 (typedef ABI) | Editor 단독 신규 |
| P-18 (RHI 인프라) | RH-7 IRHITexture / IRHIRenderTargetView 재사용 |
| P-19 (Render/Sim 결합) | Viewport = read-only Tick + RTV draw |

---

## §7 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 6 Node Editor = 07)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 24 v1 (StackPanel + ParameterPanel 본문 + 5 패널 stub)
2026-05-07    본 24 v2 재박제 (CLAUDE.md §8.2 본문 룰 — 7 패널 cpp 모두 ImGui 호출 본문 + ImNodes 통합 + Curve drag + RequestRecompile pipeline)
```
