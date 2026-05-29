# 26. Hot Reload + .wfxbin Cooked Binary 박제 (async compile pipeline + Editor → Cooked 분리)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 / 추상 0)
권위: 본 26 = 17 마스터 §15 부속 9번 (마지막). EFX-8 / EFX-9 진입 직전.
의존: 부속 18 (Asset 직렬화), 부속 19 (`CFxSystemInstance::Reset`), 부속 22 (Translator), 부속 24 (`CFxSystemViewModel::OnRecompileFinished`).

목적:
- `CFxScriptCompileQueue` async pipeline 본문 (CJobSystem worker thread + Game thread pump)
- `.wfxbin` cooked binary 본문 (Graph 제거, VMData + HLSL bytes)
- Renderer properties 직렬화 본문 (6 type, BinaryWriter / BinaryLoader)
- Hot reload 200ms 합격 sequence

박제 진입 전 8 단계 관문:
- 관문 A: §1 5 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: Hot reload + Cooked 양쪽
- 관문 D: Compile = Asset 만
- 관문 E: bitmask 미사용
- 관문 F: Niagara `NiagaraScript::RequestCompile` async pattern 차용
- 관문 G: Compile = worker. PumpCompletedTasks = phase 5 단독
- 관문 H: Queue = `CGameInstance` Tier-2

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 26 v1 의 stub 3 위치 본문화:

```txt
1. CFxScriptCompileQueue::StartTaskOnWorker 의 CJobSystem::Submit
   v1 = "CJobSystem 정식 path = Submit 패턴. 본 박제 시점 = stub. (void)callback;"
   v2 = pJS->Submit([pTask, this]() { ... Translate ... }) 호출 본문 풀

2. CFxBinaryWriter / CFxBinaryLoader 의 renderer 직렬화
   v1 = "renderer properties = EFX-9 코드 작업 시점에 deserialize / 본 박제 = stub. WriteU32(out, 0); 0 renderers"
   v2 = 6 renderer type 별 SerializeRenderer / DeserializeRenderer 본문 풀

3. Tools/WintersAssetConverter/Commands/CookFxAssets.cpp 본문
   v1 = "EFX-9 코드 시점 박제"
   v2 = manifest .wfx → .wfxbin 일괄 cook CLI 명령 본문 풀
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Async pipeline | CJobSystem 사용. Worker thread 가 Translate, Game thread 가 PumpCompletedTasks | Engine 의 기존 CJobSystem 재사용 |
| Recompile latency 목표 | 200ms 이내 | EFX-8 합격 |
| Cooked format | `.wfxbin` = header + sections (SystemMetadata + UserParams + Emitters + Scripts + Renderers) | Niagara cooked 패턴 |
| Cooking 트리거 | Editor Toolbar > Cook 또는 빌드 PostBuild | 빌드 분리 |
| Hot reload 시점 | Editor Stack 슬라이더 변경 → ViewModel.RequestRecompile (debounce 100ms) | 입력 폭발 방지 |

---

## §2 신규 파일 트리 (v1 그대로)

```txt
Engine/Public/FX/v2/HotReload/
  FxScriptCompileQueue.h
  FxCompileTask.h
  FxRecompileNotification.h

Engine/Public/FX/v2/Cooking/
  FxBinaryFormat.h
  FxBinaryWriter.h
  FxBinaryLoader.h
  FxRendererSerialization.h     본 v2 신규

Engine/Private/FX/v2/HotReload/
  FxScriptCompileQueue.cpp

Engine/Private/FX/v2/Cooking/
  FxBinaryWriter.cpp
  FxBinaryLoader.cpp
  FxRendererSerialization.cpp   본 v2 신규

Tools/WintersAssetConverter/Commands/
  CookFxAssets.cpp
```

---

## §3 헤더 박제 (v1 그대로 + RendererSerialization.h 신설)

v1 §3 5 헤더 (`FxCompileTask.h / FxRecompileNotification.h / FxScriptCompileQueue.h / FxBinaryFormat.h / FxBinaryWriter.h / FxBinaryLoader.h`) 모두 본문 풀. 변경 0. re-quote 생략.

신규: `Engine/Public/FX/v2/Cooking/FxRendererSerialization.h` (L1-L20)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <vector>
#include <memory>
namespace Winters::FX::v2
{
    struct FxRendererProperties;
    class WINTERS_ENGINE CFxRendererSerialization
    {
    public:
        // 6 type 별 직렬화 / 역직렬화. Renderer 의 type 은 첫 byte 에 enum 으로 박힘.
        static void Serialize(const FxRendererProperties* pProps, std::vector<u8_t>& outBytes);
        static std::unique_ptr<FxRendererProperties> Deserialize(const u8_t* pBytes, u64_t uByteSize, u64_t& outConsumed);
    };
}
```

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `Engine/Private/FX/v2/HotReload/FxScriptCompileQueue.cpp` (L1-L130, CJobSystem::Submit 본문)

```cpp
#include "FX/v2/HotReload/FxScriptCompileQueue.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "FX/v2/Compiler/FxHlslTranslator.h"
#include "FX/v2/Compiler/FxVMTranslator.h"
#include "FX/v2/Compiler/FxCompileResult.h"
#include "FX/v2/Compiler/FxJsonGraphLoader.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "GameInstance.h"
#include "Core/JobSystem/JobSystem.h"

namespace Winters::FX::v2
{
    std::unique_ptr<CFxScriptCompileQueue> CFxScriptCompileQueue::Create(CFxAssetRegistry* pRegistry)
    {
        auto p = std::unique_ptr<CFxScriptCompileQueue>(new CFxScriptCompileQueue());
        p->m_pRegistry = pRegistry;
        p->m_pHlslTranslator = CFxHlslTranslator::Create(pRegistry);
        p->m_pVMTranslator = CFxVMTranslator::Create(pRegistry);
        return p;
    }

    CFxScriptCompileQueue::~CFxScriptCompileQueue() = default;

    void CFxScriptCompileQueue::Enqueue(CFxScriptAsset* pScript, FxRecompileFn callback)
    {
        if (!pScript) { if (callback) callback(nullptr, false); return; }

        auto pTask = std::make_shared<FxCompileTask>();
        pTask->pScript = pScript;
        pTask->uRequestVersion = pScript->GetCompileVersion() + 1;
        pTask->eStatus.store(eFxCompileTaskStatus::Pending, std::memory_order_release);

        pScript->SetCompileStatus(eFxCompileStatus::InFlight);

        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            m_vecPending.push_back(pTask);
            m_vecPendingCallbacks.push_back(std::move(callback));
        }

        StartTaskOnWorker(pTask, m_vecPendingCallbacks.back());
    }

    void CFxScriptCompileQueue::StartTaskOnWorker(std::shared_ptr<FxCompileTask> pTask, FxRecompileFn /*callback*/)
    {
        CJobSystem* pJS = CGameInstance::Get()->Get_JobSystem();
        CFxHlslTranslator* pHlsl = m_pHlslTranslator.get();
        CFxVMTranslator* pVM = m_pVMTranslator.get();

        auto Compile = [pTask, pHlsl, pVM]() {
            pTask->eStatus.store(eFxCompileTaskStatus::InFlight, std::memory_order_release);

            // Graph json raw → CFxGraph parse (부속 22 의 JsonGraphLoader)
            CFxScriptAsset* pScript = pTask->pScript;
            if (pScript && !pScript->GetSourceGraph() && !pScript->GetSourceGraphJsonRaw().empty())
            {
                FxJsonGraphLoadResult parsed = CFxJsonGraphLoader::Parse(pScript->GetSourceGraphJsonRaw());
                if (parsed.bSucceeded) pScript->SetSourceGraph(std::move(parsed.pGraph));
            }

            FxCompileResult resVM = pVM->Translate(pScript);
            FxCompileResult resHlsl = pHlsl->Translate(pScript);

            pTask->pResult = std::make_unique<FxCompileResult>();
            pTask->pResult->pVMData = std::move(resVM.pVMData);
            pTask->pResult->hlslBytes = std::move(resHlsl.hlslBytes);
            pTask->pResult->vecErrors.insert(pTask->pResult->vecErrors.end(), resVM.vecErrors.begin(), resVM.vecErrors.end());
            pTask->pResult->vecErrors.insert(pTask->pResult->vecErrors.end(), resHlsl.vecErrors.begin(), resHlsl.vecErrors.end());
            pTask->pResult->bSucceeded = resVM.bSucceeded && resHlsl.bSucceeded;

            pTask->eStatus.store(
                pTask->pResult->bSucceeded ? eFxCompileTaskStatus::Succeeded : eFxCompileTaskStatus::Failed,
                std::memory_order_release);
        };

        if (pJS)
        {
            // CJobSystem::Submit(std::function<void()>) 패턴 (Engine 의 기존 인터페이스)
            pJS->Submit(std::move(Compile));
        }
        else
        {
            // fallback = inline (Editor 단독 실행 / 테스트)
            Compile();
        }
    }

    void CFxScriptCompileQueue::PumpCompletedTasks()
    {
        std::vector<std::shared_ptr<FxCompileTask>> vecDone;
        std::vector<FxRecompileFn> vecDoneCB;

        {
            std::lock_guard<std::mutex> lk(m_Mutex);
            for (size_t i = 0; i < m_vecPending.size(); )
            {
                const eFxCompileTaskStatus s = m_vecPending[i]->eStatus.load(std::memory_order_acquire);
                if (s == eFxCompileTaskStatus::Succeeded
                 || s == eFxCompileTaskStatus::Failed
                 || s == eFxCompileTaskStatus::Cancelled)
                {
                    vecDone.push_back(m_vecPending[i]);
                    vecDoneCB.push_back(std::move(m_vecPendingCallbacks[i]));
                    m_vecPending.erase(m_vecPending.begin() + i);
                    m_vecPendingCallbacks.erase(m_vecPendingCallbacks.begin() + i);
                    continue;
                }
                ++i;
            }
        }

        for (size_t i = 0; i < vecDone.size(); ++i)
        {
            const std::shared_ptr<FxCompileTask>& pTask = vecDone[i];
            FxRecompileFn& cb = vecDoneCB[i];
            const bool_t bSuccess = pTask->eStatus.load(std::memory_order_acquire) == eFxCompileTaskStatus::Succeeded;

            if (bSuccess && pTask->pResult)
            {
                if (pTask->pScript)
                {
                    pTask->pScript->SetVMData(std::move(pTask->pResult->pVMData));
                    pTask->pScript->SetHlslBytes(std::move(pTask->pResult->hlslBytes));
                    pTask->pScript->SetCompileStatus(eFxCompileStatus::Succeeded);
                    pTask->pScript->SetCompileVersion(pTask->uRequestVersion);
                }
            }
            else
            {
                if (pTask->pScript) pTask->pScript->SetCompileStatus(eFxCompileStatus::Failed);
            }
            if (cb) cb(pTask->pScript, bSuccess);
        }
    }

    u32_t CFxScriptCompileQueue::GetPendingCount() const
    {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(m_Mutex));
        return static_cast<u32_t>(m_vecPending.size());
    }

    u32_t CFxScriptCompileQueue::GetInFlightCount() const
    {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(m_Mutex));
        u32_t uCount = 0;
        for (const auto& pTask : m_vecPending)
            if (pTask->eStatus.load(std::memory_order_acquire) == eFxCompileTaskStatus::InFlight) ++uCount;
        return uCount;
    }
}
```

P-13 회피: `CJobSystem::Submit(std::function<void()>)` = Engine 의 기존 박제 (5/6 commit `Engine/Public/Core/JobSystem/JobSystem.h` 의 `Submit` 메서드 정합).

### §4.2 `Engine/Private/FX/v2/Cooking/FxRendererSerialization.cpp` (L1-L150, 6 renderer 본문)

```cpp
#include "FX/v2/Cooking/FxRendererSerialization.h"
#include "FX/v2/Renderer/FxRendererProperties.h"
#include "FX/v2/Renderer/FxSpriteRendererProperties.h"
#include "FX/v2/Renderer/FxMeshRendererProperties.h"
#include "FX/v2/Renderer/FxRibbonRendererProperties.h"
#include "FX/v2/Renderer/FxBeamRendererProperties.h"
#include "FX/v2/Renderer/FxLightRendererProperties.h"
#include "FX/v2/Renderer/FxDecalRendererProperties.h"
#include <cstring>
#include <codecvt>
#include <locale>

namespace Winters::FX::v2
{
    namespace
    {
        std::string W2U8(const std::wstring& w) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.to_bytes(w); }
        std::wstring U82W(const std::string& s) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.from_bytes(s); }

        void WriteU32(std::vector<u8_t>& out, u32_t v) { const u8_t* p = reinterpret_cast<const u8_t*>(&v); out.insert(out.end(), p, p + 4); }
        void WriteF32(std::vector<u8_t>& out, f32_t v) { const u8_t* p = reinterpret_cast<const u8_t*>(&v); out.insert(out.end(), p, p + 4); }
        void WriteString(std::vector<u8_t>& out, const std::wstring& w)
        {
            const std::string s = W2U8(w);
            WriteU32(out, static_cast<u32_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
        }
        void WriteCommonProps(std::vector<u8_t>& out, const FxRendererProperties* p)
        {
            WriteString(out, p->strMaterialPath);
            WriteU32(out, static_cast<u32_t>(p->eBlend));
            WriteU32(out, p->bDepthWrite ? 1u : 0u);
            WriteU32(out, p->bMotionBlur ? 1u : 0u);
        }

        struct R { const u8_t* p; const u8_t* end; u64_t consumed = 0;
            bool ReadU32(u32_t& v) { if (p + 4 > end) return false; std::memcpy(&v, p, 4); p += 4; consumed += 4; return true; }
            bool ReadF32(f32_t& v) { if (p + 4 > end) return false; std::memcpy(&v, p, 4); p += 4; consumed += 4; return true; }
            bool ReadString(std::wstring& v) { u32_t n; if (!ReadU32(n)) return false; if (p + n > end) return false; v = U82W(std::string(reinterpret_cast<const char*>(p), n)); p += n; consumed += n; return true; }
        };

        void ReadCommonProps(R& r, FxRendererProperties* p)
        {
            r.ReadString(p->strMaterialPath);
            u32_t b = 0; r.ReadU32(b); p->eBlend = static_cast<eFxBlendMode>(b);
            u32_t dw = 0, mb = 0;
            r.ReadU32(dw); p->bDepthWrite = dw != 0;
            r.ReadU32(mb); p->bMotionBlur = mb != 0;
        }
    }

    void CFxRendererSerialization::Serialize(const FxRendererProperties* pProps, std::vector<u8_t>& outBytes)
    {
        if (!pProps) { WriteU32(outBytes, 0xFFFFFFFFu); return; }
        const u32_t uType = static_cast<u32_t>(pProps->GetRenderType());
        WriteU32(outBytes, uType);
        WriteCommonProps(outBytes, pProps);

        switch (pProps->GetRenderType())
        {
        case eFxRenderType::Billboard:
        {
            const auto* p = static_cast<const FxSpriteRendererProperties*>(pProps);
            WriteU32(outBytes, static_cast<u32_t>(p->eFacing));
            WriteU32(outBytes, p->uAtlasCols);
            WriteU32(outBytes, p->uAtlasRows);
            WriteF32(outBytes, p->fAtlasFps);
            break;
        }
        case eFxRenderType::Mesh:
        {
            const auto* p = static_cast<const FxMeshRendererProperties*>(pProps);
            WriteString(outBytes, p->strMeshPath);
            WriteU32(outBytes, static_cast<u32_t>(p->vecHiddenSubmeshIndices.size()));
            for (u32_t i : p->vecHiddenSubmeshIndices) WriteU32(outBytes, i);
            WriteU32(outBytes, p->bUseLODs ? 1u : 0u);
            break;
        }
        case eFxRenderType::Ribbon:
        {
            const auto* p = static_cast<const FxRibbonRendererProperties*>(pProps);
            WriteU32(outBytes, static_cast<u32_t>(p->eTess));
            WriteU32(outBytes, p->uMaxTessellationFactor);
            WriteF32(outBytes, p->fUVScrollSpeed);
            break;
        }
        case eFxRenderType::Beam:
        {
            const auto* p = static_cast<const FxBeamRendererProperties*>(pProps);
            WriteU32(outBytes, p->uSegments);
            WriteF32(outBytes, p->fNoiseAmplitude);
            WriteF32(outBytes, p->fUVScrollSpeed);
            break;
        }
        case eFxRenderType::Light:
        {
            const auto* p = static_cast<const FxLightRendererProperties*>(pProps);
            WriteF32(outBytes, p->fRadiusScale);
            WriteF32(outBytes, p->fIntensityScale);
            WriteU32(outBytes, p->bAffectsTranslucency ? 1u : 0u);
            WriteU32(outBytes, p->bCastShadows ? 1u : 0u);
            break;
        }
        case eFxRenderType::Decal:
        {
            const auto* p = static_cast<const FxDecalRendererProperties*>(pProps);
            WriteF32(outBytes, p->fProjectionDepth);
            WriteF32(outBytes, p->fFadeStartDistance);
            WriteF32(outBytes, p->fFadeEndDistance);
            WriteU32(outBytes, p->bClampToTerrain ? 1u : 0u);
            break;
        }
        }
    }

    std::unique_ptr<FxRendererProperties> CFxRendererSerialization::Deserialize(const u8_t* pBytes, u64_t uByteSize, u64_t& outConsumed)
    {
        outConsumed = 0;
        if (!pBytes || uByteSize < 4) return nullptr;
        R r{ pBytes, pBytes + uByteSize };
        u32_t uType = 0;
        if (!r.ReadU32(uType)) return nullptr;
        if (uType == 0xFFFFFFFFu) { outConsumed = r.consumed; return nullptr; }

        std::unique_ptr<FxRendererProperties> pOut;
        switch (static_cast<eFxRenderType>(uType))
        {
        case eFxRenderType::Billboard:
        {
            auto p = std::make_unique<FxSpriteRendererProperties>();
            ReadCommonProps(r, p.get());
            u32_t facing = 0; r.ReadU32(facing); p->eFacing = static_cast<eFxSpriteFacingMode>(facing);
            r.ReadU32(p->uAtlasCols);
            r.ReadU32(p->uAtlasRows);
            r.ReadF32(p->fAtlasFps);
            pOut = std::move(p);
            break;
        }
        case eFxRenderType::Mesh:
        {
            auto p = std::make_unique<FxMeshRendererProperties>();
            ReadCommonProps(r, p.get());
            r.ReadString(p->strMeshPath);
            u32_t n = 0; r.ReadU32(n);
            p->vecHiddenSubmeshIndices.resize(n);
            for (u32_t i = 0; i < n; ++i) r.ReadU32(p->vecHiddenSubmeshIndices[i]);
            u32_t lod = 0; r.ReadU32(lod); p->bUseLODs = lod != 0;
            pOut = std::move(p);
            break;
        }
        case eFxRenderType::Ribbon:
        {
            auto p = std::make_unique<FxRibbonRendererProperties>();
            ReadCommonProps(r, p.get());
            u32_t tess = 0; r.ReadU32(tess); p->eTess = static_cast<eFxRibbonTessellationMode>(tess);
            r.ReadU32(p->uMaxTessellationFactor);
            r.ReadF32(p->fUVScrollSpeed);
            pOut = std::move(p);
            break;
        }
        case eFxRenderType::Beam:
        {
            auto p = std::make_unique<FxBeamRendererProperties>();
            ReadCommonProps(r, p.get());
            r.ReadU32(p->uSegments);
            r.ReadF32(p->fNoiseAmplitude);
            r.ReadF32(p->fUVScrollSpeed);
            pOut = std::move(p);
            break;
        }
        case eFxRenderType::Light:
        {
            auto p = std::make_unique<FxLightRendererProperties>();
            ReadCommonProps(r, p.get());
            r.ReadF32(p->fRadiusScale);
            r.ReadF32(p->fIntensityScale);
            u32_t at = 0, cs = 0;
            r.ReadU32(at); p->bAffectsTranslucency = at != 0;
            r.ReadU32(cs); p->bCastShadows = cs != 0;
            pOut = std::move(p);
            break;
        }
        case eFxRenderType::Decal:
        {
            auto p = std::make_unique<FxDecalRendererProperties>();
            ReadCommonProps(r, p.get());
            r.ReadF32(p->fProjectionDepth);
            r.ReadF32(p->fFadeStartDistance);
            r.ReadF32(p->fFadeEndDistance);
            u32_t ct = 0; r.ReadU32(ct); p->bClampToTerrain = ct != 0;
            pOut = std::move(p);
            break;
        }
        }
        outConsumed = r.consumed;
        return pOut;
    }
}
```

### §4.3 `Engine/Private/FX/v2/Cooking/FxBinaryWriter.cpp` (L1-L120, renderer 통합)

```cpp
#include "FX/v2/Cooking/FxBinaryWriter.h"
#include "FX/v2/Cooking/FxBinaryFormat.h"
#include "FX/v2/Cooking/FxRendererSerialization.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Renderer/FxRendererProperties.h"
#include "FX/v2/VM/FxVMExecutableData.h"
#include <fstream>
#include <codecvt>
#include <locale>
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        std::string W2U8(const std::wstring& w) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.to_bytes(w); }
        void WB(std::vector<u8_t>& o, const void* p, u64_t n) { const u8_t* s = static_cast<const u8_t*>(p); o.insert(o.end(), s, s + n); }
        void WU32(std::vector<u8_t>& o, u32_t v) { WB(o, &v, 4); }
        void WString(std::vector<u8_t>& o, const std::wstring& w) { const std::string s = W2U8(w); WU32(o, static_cast<u32_t>(s.size())); WB(o, s.data(), s.size()); }
        void WriteScript(std::vector<u8_t>& out, const CFxScriptAsset* pScript)
        {
            if (!pScript) { WU32(out, 0); WU32(out, 0); WU32(out, 0); WU32(out, 0); return; }
            const FxVMExecutableData* pVM = pScript->GetVMData();
            if (pVM)
            {
                WU32(out, static_cast<u32_t>(pVM->vecInstructions.size()));
                WB(out, pVM->vecInstructions.data(), pVM->vecInstructions.size() * sizeof(FxVMInstruction));
                WU32(out, static_cast<u32_t>(pVM->vecConstants.size()));
                WB(out, pVM->vecConstants.data(), pVM->vecConstants.size() * sizeof(f32_t));
                WU32(out, pVM->uNumRegisters);
            }
            else { WU32(out, 0); WU32(out, 0); WU32(out, 0); }
            const std::vector<u8_t>& hlsl = pScript->GetHlslBytes();
            WU32(out, static_cast<u32_t>(hlsl.size()));
            WB(out, hlsl.data(), hlsl.size());
        }
    }

    FxCookResult CFxBinaryWriter::CookToFile(const CFxSystemAsset* pAsset, const std::wstring& strOutputPath)
    {
        FxCookResult result;
        if (!pAsset) { result.vecErrors.push_back(L"null asset"); return result; }

        std::vector<u8_t> body;
        WString(body, pAsset->GetName());
        WU32(body, pAsset->GetSchemaVersion());

        WriteScript(body, pAsset->GetSystemSpawnScript());
        WriteScript(body, pAsset->GetSystemUpdateScript());

        WU32(body, static_cast<u32_t>(pAsset->GetEmitters().size()));
        for (const auto& pEm : pAsset->GetEmitters())
        {
            WString(body, pEm->GetName());
            WU32(body, static_cast<u32_t>(pEm->GetExecMode()));
            WU32(body, pEm->GetMaxParticles());
            WriteScript(body, pEm->GetEmitterSpawnScript());
            WriteScript(body, pEm->GetEmitterUpdateScript());
            WriteScript(body, pEm->GetParticleSpawnScript());
            WriteScript(body, pEm->GetParticleUpdateScript());

            WU32(body, static_cast<u32_t>(pEm->GetRenderers().size()));
            for (const auto& pProps : pEm->GetRenderers())
                CFxRendererSerialization::Serialize(pProps.get(), body);
        }

        FxBinaryHeader header;
        header.uTotalByteSize = sizeof(FxBinaryHeader) + body.size();

        std::ofstream f(W2U8(strOutputPath), std::ios::binary);
        if (!f.is_open()) { result.vecErrors.push_back(L"파일 쓰기 실패"); return result; }
        f.write(reinterpret_cast<const char*>(&header), sizeof(header));
        f.write(reinterpret_cast<const char*>(body.data()), body.size());
        result.uOutputByteSize = header.uTotalByteSize;
        result.bSucceeded = true;
        return result;
    }
}
```

### §4.4 `Engine/Private/FX/v2/Cooking/FxBinaryLoader.cpp` (L1-L100, renderer 통합)

```cpp
#include "FX/v2/Cooking/FxBinaryLoader.h"
#include "FX/v2/Cooking/FxBinaryFormat.h"
#include "FX/v2/Cooking/FxRendererSerialization.h"
#include "FX/v2/Asset/FxSystemAsset.h"
#include "FX/v2/Asset/FxEmitterAsset.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Renderer/FxRendererProperties.h"
#include "FX/v2/VM/FxVMExecutableData.h"
#include <fstream>
#include <codecvt>
#include <locale>
#include <cstring>

namespace Winters::FX::v2
{
    namespace
    {
        std::wstring U82W(const std::string& s) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.from_bytes(s); }
        std::string W2U8(const std::wstring& w) { std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c; return c.to_bytes(w); }
        struct ByteReader
        {
            const u8_t* p = nullptr; const u8_t* end = nullptr;
            bool ReadBytes(void* dst, u64_t n) { if (p + n > end) return false; std::memcpy(dst, p, n); p += n; return true; }
            bool ReadU32(u32_t& v) { return ReadBytes(&v, 4); }
            bool ReadString(std::wstring& v) { u32_t n; if (!ReadU32(n)) return false; if (p + n > end) return false; v = U82W(std::string(reinterpret_cast<const char*>(p), n)); p += n; return true; }
            std::unique_ptr<CFxScriptAsset> ReadScript(eFxScriptUsage usage)
            {
                auto pScript = CFxScriptAsset::Create(usage);
                u32_t nIns = 0; ReadU32(nIns);
                auto pVM = std::make_unique<FxVMExecutableData>();
                pVM->vecInstructions.resize(nIns);
                if (nIns > 0) ReadBytes(pVM->vecInstructions.data(), nIns * sizeof(FxVMInstruction));
                u32_t nC = 0; ReadU32(nC);
                pVM->vecConstants.resize(nC);
                if (nC > 0) ReadBytes(pVM->vecConstants.data(), nC * sizeof(f32_t));
                ReadU32(pVM->uNumRegisters);
                pScript->SetVMData(std::move(pVM));
                pScript->SetCompileStatus(eFxCompileStatus::Succeeded);
                u32_t nH = 0; ReadU32(nH);
                std::vector<u8_t> hlsl(nH);
                if (nH > 0) ReadBytes(hlsl.data(), nH);
                pScript->SetHlslBytes(std::move(hlsl));
                return pScript;
            }
        };
    }

    FxBinaryLoadResult CFxBinaryLoader::LoadFromFile(const std::wstring& strPath)
    {
        FxBinaryLoadResult result;
        std::ifstream f(W2U8(strPath), std::ios::binary | std::ios::ate);
        if (!f.is_open()) { result.vecErrors.push_back(L"파일 열기 실패"); return result; }
        const std::streamsize uSize = f.tellg();
        if (uSize < (std::streamsize)sizeof(FxBinaryHeader)) { result.vecErrors.push_back(L"too small"); return result; }
        f.seekg(0, std::ios::beg);
        std::vector<u8_t> bytes(static_cast<size_t>(uSize));
        f.read(reinterpret_cast<char*>(bytes.data()), uSize);

        FxBinaryHeader header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.uMagic != 0x46584249) { result.vecErrors.push_back(L"잘못된 magic"); return result; }
        if (header.uVersion != 1) { result.vecErrors.push_back(L"unsupported version"); return result; }

        ByteReader r{ bytes.data() + sizeof(header), bytes.data() + bytes.size() };

        std::wstring strName;
        u32_t uSchema = 0;
        if (!r.ReadString(strName) || !r.ReadU32(uSchema)) { result.vecErrors.push_back(L"metadata"); return result; }

        auto pSystem = CFxSystemAsset::Create(strName);
        pSystem->SetSchemaVersion(uSchema);
        pSystem->SetSystemSpawnScript(r.ReadScript(eFxScriptUsage::SystemSpawn));
        pSystem->SetSystemUpdateScript(r.ReadScript(eFxScriptUsage::SystemUpdate));

        u32_t nEm = 0; r.ReadU32(nEm);
        for (u32_t i = 0; i < nEm; ++i)
        {
            std::wstring n; r.ReadString(n);
            u32_t mode = 0, mp = 0;
            r.ReadU32(mode); r.ReadU32(mp);
            auto pEm = CFxEmitterAsset::Create(n);
            pEm->SetExecMode(static_cast<eFxExecMode>(mode));
            pEm->SetMaxParticles(mp);
            pEm->SetEmitterSpawnScript(r.ReadScript(eFxScriptUsage::EmitterSpawn));
            pEm->SetEmitterUpdateScript(r.ReadScript(eFxScriptUsage::EmitterUpdate));
            pEm->SetParticleSpawnScript(r.ReadScript(eFxScriptUsage::ParticleSpawn));
            pEm->SetParticleUpdateScript(r.ReadScript(eFxScriptUsage::ParticleUpdate));

            u32_t nR = 0; r.ReadU32(nR);
            for (u32_t k = 0; k < nR; ++k)
            {
                u64_t consumed = 0;
                auto pProps = CFxRendererSerialization::Deserialize(r.p, static_cast<u64_t>(r.end - r.p), consumed);
                r.p += consumed;
                if (pProps) pEm->AddRenderer(std::move(pProps));
            }
            pSystem->AddEmitter(std::move(pEm));
        }
        result.pAsset = std::move(pSystem);
        result.bSucceeded = true;
        return result;
    }
}
```

### §4.5 `Tools/WintersAssetConverter/Commands/CookFxAssets.cpp` (L1-L50, CLI 본문)

```cpp
#include "FX/v2/Asset/FxJsonLoader.h"
#include "FX/v2/Cooking/FxBinaryWriter.h"
#include <iostream>
#include <vector>
#include <string>

using namespace Winters::FX::v2;

static const std::wstring g_ManifestWfx[] = {
    L"Resource/FX/Annie/Q_Fireball.wfx",
    L"Resource/FX/Ashe/Q_VolleyOpening.wfx",
    L"Resource/FX/Fiora/E_Stab.wfx",
    L"Resource/FX/Garen/E_JudgmentSpin.wfx",
    L"Resource/FX/Irelia/Q_Stab.wfx",
    L"Resource/FX/Jax/Q_LeapStrike.wfx",
    L"Resource/FX/Kalista/Q_PiercingSpear.wfx",
    L"Resource/FX/Riven/Q_BrokenWings.wfx",
    L"Resource/FX/Yasuo/Q_Straight.wfx",
    L"Resource/FX/Yone/Q_MortalSteel.wfx",
    L"Resource/FX/Zed/Q_RazorShuriken.wfx",
};

int RunCookFxAssets()
{
    int nSuccess = 0, nFail = 0;
    for (const std::wstring& strWfx : g_ManifestWfx)
    {
        FxLoadResult lr = CFxJsonLoader::LoadFromFile(strWfx);
        if (!lr.bSucceeded || !lr.pAsset) { ++nFail; std::wcerr << L"[FAIL load] " << strWfx << std::endl; continue; }
        std::wstring strBin = strWfx;
        const size_t pos = strBin.find_last_of(L'.');
        if (pos != std::wstring::npos) strBin.replace(pos, std::wstring::npos, L".wfxbin");
        else strBin += L".wfxbin";
        FxCookResult cr = CFxBinaryWriter::CookToFile(lr.pAsset.get(), strBin);
        if (!cr.bSucceeded) { ++nFail; std::wcerr << L"[FAIL cook] " << strBin << std::endl; continue; }
        ++nSuccess;
        std::wcout << L"[OK] " << strBin << L" (" << cr.uOutputByteSize << L" bytes)" << std::endl;
    }
    std::wcout << L"cook: " << nSuccess << L" / " << (nSuccess + nFail) << std::endl;
    return nFail == 0 ? 0 : 1;
}
```

---

## §5 Hot Reload sequence (200ms 합격)

```txt
1. 디자이너 = Stack 의 Gravity 슬라이더 9.81 → 5.0
2. CFxParameterPanel::RenderFloatSlider → m_pViewModel->RequestRecompile()
3. ViewModel = 모든 stage script 를 CFxScriptCompileQueue::Enqueue
4. Queue = CJobSystem::Submit (worker thread)
5. Worker = JsonGraphLoader::Parse + VMTranslator::Translate + HlslTranslator::Translate
6. Worker = pTask->eStatus.store(Succeeded)
7. Game thread (다음 frame, phase 5) = CFxTickSystem::Execute 진입 직전 + queue->PumpCompletedTasks()
8. PumpCompletedTasks = Script 의 SetVMData + SetHlslBytes + callback 호출
9. callback = ViewModel::OnRecompileFinished(true) → ResetPreviewInstance
10. CFxSystemInstance::Reset = 모든 입자 kill + parameter store rebind + Activate
11. 다음 frame 부터 새 gravity 반영

목표 latency: 액션 → frame N+1 = 200 ms 이내 (CJobSystem worker 1 frame + Game thread pump)
```

---

## §6 검증 명령 (EFX-8 + EFX-9 합격)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/{HotReload,Cooking}/   → 0 hit
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/{HotReload,Cooking}/   → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/{HotReload,Cooking}/  → 0 hit
4. grep "TBD" .md/plan/EffectTool/26_HOT_RELOAD_AND_COOKED_BINARY_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/26_HOT_RELOAD_AND_COOKED_BINARY_BAKE.md  → 0 hit
6. Editor 슬라이더 → 200 ms 이내 라이브 프리뷰 갱신
7. WintersAssetConverter --cook-fx 실행 → manifest .wfx → .wfxbin 산출
8. .wfxbin 사이즈 = .wfx 의 50% 이하 (Graph 제거 검증)
9. CFxBinaryLoader 의미 비교 (emitter / particle / VM instruction count 동일)
10. Compile queue 100 회 stress: race / leak 0
11. Renderer 6 type 모두 Serialize → Deserialize → 의미 비교 (struct field 동일)
```

---

## §7 박제 함정 매트릭스

| 함정 | 본 26 회피 |
|---|---|
| P-1 + P-6 | §1 5 항목, TBD 0. CJobSystem::Submit / 6 renderer 직렬화 모두 본문 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | Hot reload + Cooked + Renderer 직렬화 한 번에 |
| P-4 (Scene 직접 의존) | Asset 만 |
| P-7 (bitmask) | binary `uFlags = u32_t` |
| P-8 (인용 의미 반전) | Niagara `NiagaraScript::RequestCompile` async + Cooked vs Editor 분리 차용 |
| P-9 (ECS Scheduler) | Compile = worker. PumpCompletedTasks = phase 5 단독 |
| P-10 (Owner Scope) | Queue = `CGameInstance` Tier-2 |
| P-11 (도메인 상수) | 본 26 = 도메인 무관 |
| P-12 (음수 truncation) | 정수 size = u32 / u64 양수 |
| P-13 (미존재 API) | `CJobSystem::Submit / CFxJsonGraphLoader::Parse / 6 renderer Properties type` 모두 부속 18 / 22 / 20 박제 |
| P-14 (행동 정책 변경) | 본 26 = 신규 |
| P-15 (헤더 외부 의존) | `FxBinaryLoader.h` = `CFxSystemAsset.h` 직접 include |
| P-16 (산술 검증) | `FxBinaryHeader` sizeof 24, `FxBinarySectionHeader` sizeof 8 static_assert |
| P-17 (typedef ABI) | 신규 |
| P-18 (RHI 인프라) | RHI 무관 |
| P-19 (Render/Sim 결합) | Compile = worker. Reset = phase 5. Render = phase 7 |

---

## §8 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 9 통합)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 26 v1 (CJobSystem::Submit stub + renderer 직렬화 stub)
2026-05-07    본 26 v2 재박제 (CLAUDE.md §8.2 본문 룰 — CJobSystem::Submit 호출 본문 + 6 renderer Serialize/Deserialize 본문 + CookFxAssets CLI 본문)
```

---

## §9 부속 18~26 v2 재박제 완료 보고

CLAUDE.md §8.2 본문 룰 (stub 0 / 라인 번호 / 추상 0) 적용 완료 9 부속:

```txt
18  Asset Layer                   v2 = phased 박제 명시 (m_strSourceGraphJsonRaw / m_vecRendererJsonBlobs raw 보관 멤버)
19  Runtime Layer                 v2 = CFxVM::Execute 호출 본문 + ConsumeSpawnCount + KillFlag swap-back + RNG seed XOR
20  Renderer 6 종                 v2 = Sprite cpp 본문 풀 + 5 cpp 패턴 명시 + ECS Snapshot/Dispatch 본문 + 6 셰이더 본문
21  VM + GPU compute              v2 = 28 opcode scalar 풀 본문 + AVX2 13 op 본문 + EXTERNAL DI 호출 본문 + GPU dispatch 본문
22  Compile (Graph → HLSL/VM)     v2 = 7 노드 ctor 본문 + Translator 양 경로 본문 + 9 모듈 Graph 빌드 본문 + JsonGraphLoader 본문
23  DataInterface 6 종            v2 = 6 DI CPU 함수 본문 + 6 .ush 본문 + IRHIBindGroup::SetUniformBuffer 호출 본문
24  Editor 7 패널                 v2 = 7 패널 cpp ImGui 호출 본문 + ImNodes 통합 + Curve drag + RequestRecompile pipeline
25  LoL/Elden 도메인              v2 = 11 챔프 hook 모두 본문 (Yasuo 8 + 10 챔프 5~6 hook each)
26  Hot reload + .wfxbin          v2 = CJobSystem::Submit 호출 본문 + 6 renderer Serialize/Deserialize 본문 + CookFxAssets CLI 본문
```

8 GATE 통과:
- 관문 A (TBD 0): 9 부속 모두 §1 결정값 풀
- 관문 B (PIMPL 미사용): 헤더 + cpp 동시 박제
- 관문 C (모든 path 동시): 6 renderer / 6 DI / 7 패널 / 9 모듈 한 번에
- 관문 D (Scene 직접 의존 0): grep `Scene_` 0 hit 강제
- 관문 E (bitmask 폭): 기존 `MeshGroupVisibilityComponent::VisibilityMask` (2048) 재사용
- 관문 F (인용 의미): Niagara 인용 시 직접 인용 블록 동반
- 관문 G (ECS Scheduler): Phase 표 1 곳 (FxSpawnRequest=0, FxTick=5, FxRenderSnapshot=6, FxRenderDispatch=7)
- 관문 H (Owner Scope): Storage = CWorld / Registry+Queue = CGameInstance Tier-1+2 / Renderer = DispatchSystem owned

박제 함정 P-1 ~ P-19 회피 매트릭스: 9 부속 모두 §7 또는 §8 에 표 박제.

다음 단계 = EFX-0 코드 작업 진입:
1. `Engine/Public/FX/v2/` + `Engine/Private/FX/v2/` 디렉토리 신설
2. `Engine/ThirdPartyLib/{nlohmann_json, imnodes}/` 추가
3. 부속 18 부터 헤더 + cpp 박제분 그대로 코드화
4. 11 챔프 priority hook = `BuildFxAsset_X()` 빌더 함수 (부속 18 §6) 그대로 코드화
5. WintersAssetConverter --dump-fx-manifest 실행 → 11 .wfx 산출
6. EFX-1 → EFX-2 → ... → EFX-9 순차 진입

마스터 17 §13 일정 표 (LoL MVP 4~6주 / Editor MVP 8~12주 / Elden+GPU 16~22주) 참조.
