# 28. DX12 이주 시 FX 통합 박제 (RH-7 + DX12 backend + GPU compute 활성)

작성일: 2026-05-07
권위: 본 28 = 17 마스터 §15 부속 11번 (5/7 신규). 사용자 DX12 이주 계획서 진행 중 + EffectTool 박제 정합 박제.
의존:
- 부속 18~27 v2 박제 (RH-7 추상 + DX11 가동 가정)
- Track 2 RH-7 박제 (`Engine/Public/RHI/{IRHIDevice, IRHIPipelineState, IRHIBindGroup, IRHICommandList, RHICapabilities}.h`)
- Track 2 W7-9 DX12 Bootstrap (`Engine/Private/RHI/DX12/` 28 파일, `Debug-DX12` config, D3D12MA, CDX12Device::Initialize)
- CLAUDE.md §1 Track 2 (`.md/plan/rhi/00_RHI_MIGRATION_MASTER.md` + `.md/plan/rhi/DX12_BOOTSTRAP_PLAN.md`)

목적:
- DX12 이주 시 FX 부속별 영향 매트릭스 박제
- RHI 백엔드 결정값 갱신 (RH-7 추상 통과 + DX12 우선)
- GPU compute path 활성 (DX12 SM 5.1+ 필수, DX11 fallback = legacy maintenance only)
- D3D12MA 통합 (FX 의 Particle SoA / Parameter UAV 할당)
- ImGui DX12 backend (Editor)
- DXC 컴파일 파이프라인 (HLSL → DXIL, master material 3 종 + 6 셰이더 + 6 .ush)
- 부속별 cpp 의 DX12 specific 분기 매트릭스

---

## §0.1 DX12 이주 결정 영향 분석

```txt
사용자 보고: DX12 이주 계획서 작성 중. EffectTool 박제 (부속 18~27) 가 RH-7 추상 통과를 가정했지만,
DX12 native 활용은 부속별로 다른 정정 필요.

영향 범위 (부속별):
  부속 18 Asset             영향 0 (RHI 무관)
  부속 19 Runtime           영향 1 (TickGPU 의 CFxGpuComputeDispatch 호출 = DX12 활성)
  부속 20 Renderer          영향 3 (IRHIPipelineState DX12 변형, RTV / depth-stencil heap, root signature)
  부속 21 VM + GPU compute  영향 4 (DX12 compute primary, DX11 fallback maintenance, D3D12MA, RDG-style barrier)
  부속 22 Compile           영향 1 (DXC 사용 = HLSL → DXIL, FXC 사용 = HLSL → DXBC, 양 경로)
  부속 23 DataInterface     영향 2 (StructuredBuffer SRV / Texture2D SRV = DX12 root signature 통합, .ush register space0 명시)
  부속 24 Editor            영향 2 (ImGui DX12 backend 전환, Preview RTV = DX12 RTV heap)
  부속 25 Domain            영향 0 (RHI 무관)
  부속 26 Hot reload        영향 1 (CFxScriptCompileQueue 의 HLSL 컴파일 = DXC 우선, FXC = legacy)
  부속 27 Master Material   영향 2 (3 master material PSO = DX12 PipelineStateDesc + RootSignature, DXIL 컴파일)

총 영향 = 부속 19/20/21/22/23/24/26/27 = 8 부속 (부속 18/25 무관)
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| RHI 백엔드 우선순위 | DX12 = primary. DX11 = legacy maintenance only (Editor + Client 1차 가동까지) | 사용자 DX12 이주 결정. Track 2 W7-9 Scaffold 박제 |
| GPU compute path | DX12 SM 5.1+ 필수. `RHICapabilities::bSupportsCompute` = DX12 backend 시 항상 true | 부속 21 의 DX11 fallback 은 legacy 만 |
| HLSL 컴파일러 | DXC (HLSL → DXIL) primary. FXC (HLSL → DXBC) = legacy DX11 빌드 시점 | DXC = SM 6.0+ + SPIR-V 호환. FXC 의 SM 5.0 한정 |
| Memory allocator | D3D12MA (Track 2 W7-9 박제). FX 의 Particle SoA / Parameter UAV / RTV 모두 D3D12MA 통과 | Niagara 의 `D3D12RHI::CommittedResource` 차용 |
| Resource binding model | Bindless (DX12 SM 5.1+ unbounded array) 보다 root signature 명시. master material 3 종 = 별 root signature | 호환성 우선 (DX11 → DX12 마이그 단계) |
| ImGui backend | DX12 (ImGui_ImplDX12). Editor 진입 시 init | Track 2 W7-9 합격 후 |
| 셰이더 register space | 모든 FX 셰이더 = `space0` 명시 (부속 21 / 23 / 27 박제 정합) | DX12 SM 5.1+ + Vulkan SPIR-V 호환 |
| Hot reload 시점 | DXC 컴파일 200ms 이내 (FXC 350ms 보다 빠름) | EFX-8 합격 |

---

## §2 부속별 영향 매트릭스 + 정정 (8 부속)

### §2.1 부속 19 Runtime (TickGPU 영향)

```txt
영향 위치: Engine/Private/FX/v2/Instance/FxEmitterInstance.cpp 의 TickGPU
v2 박제: bGpuSupported false 가정 (DX11 한정) → TickCPU fallback 강제

DX12 정정:
  - bGpuSupported = CFxGpuComputeShim::QuerySupportsCompute(pDevice) = DX12 시 true
  - TickGPU 본문 = CFxGpuComputeDispatch::Enqueue(tick) 활성 (CPU fallback X)
  - phase 5 끝에 CFxGpuComputeDispatch::Dispatch(pCmdList) 호출 = FxTickSystem 가 책임
  - 결과 SoA buffer = 부속 20 Renderer 가 다음 frame 의 Previous read

코드 정정 (FxEmitterInstance.cpp 의 TickGPU 본문):
```

```cpp
// DX12 활성 시
void CFxEmitterInstance::TickGPU(f32_t fDeltaTime)
{
    if (!m_pAsset) return;
    auto* pDispatch = CGameInstance::Get()->Get_FxGpuComputeDispatch();
    if (!pDispatch || !pDispatch->IsGpuPathSupported())
    {
        // DX11 fallback (legacy maintenance)
        TickCPU(fDeltaTime);
        return;
    }

    FxGpuSystemTick tick;
    tick.pEmitter = this;
    tick.eStage = eFxGpuStage::ParticleUpdate;
    tick.uNumInstances = m_DataSet.GetNumInstances();
    tick.uMaxInstances = m_DataSet.GetMaxInstances();
    pDispatch->Enqueue(tick);

    // EmitterUpdate / ParticleSpawn / KillFlag = phase 5 끝의 Dispatch 가 처리
    ++m_uTickCount;
}
```

### §2.2 부속 20 Renderer (IRHIPipelineState DX12 변형)

```txt
영향 위치: 6 renderer cpp 의 Initialize / Render
v2 박제: pCmdList->SetPipelineState(m_pPipeline) 등 RH-7 추상 통과

DX12 정정:
  - m_pPipeline 의 desc = DX12 PipelineStateDesc (root signature + input layout + RTV format + DSV format)
  - master material 3 종 (부속 27) 별 root signature 1 종씩 = 3 root signature 박제
  - IRHIPipelineState::Initialize 가 backend 별로 DX11 PSO blob / DX12 ID3D12PipelineState 생성

신규 root signature 3 종 (부속 27 의 master material 별):
  - RootSig_M_VFX_Particle:
      Slot 0: CBV b0 (CBPerFrame)
      Slot 1: CBV b1 (CBPerEmitter)
      Slot 2: CBV b2 (CB_M_VFX_Particle = 4 핵심 + 26 advanced)
      Slot 3: SRV t0 (g_DiffuseMap)
      Slot 4: SRV t1 (g_NoiseTex)
      Slot 5: SRV t2 (g_DistortionTex)
      Slot 6: SRV t3 (instance buffer)
      Slot 7: Sampler s0
  - RootSig_M_VFX_Trail: 동일 + g_NoiseTex + diffuse
  - RootSig_M_VFX_Volumetric: 동일 + g_Noise3D (Texture3D)
```

### §2.3 부속 21 VM + GPU compute (DX12 primary)

```txt
영향 위치: Engine/Private/FX/v2/GPU/FxGpuComputeDispatch.cpp 의 Dispatch
v2 박제: pCmdList->SetComputePipelineState / SetComputeBindGroup / Dispatch 호출 본문

DX12 정정:
  - DX12 SM 5.1+ compute root signature = Particle UAV + Parameter UAV + Per-emitter CBV
  - m_pPipelineParticleUpdate = ID3D12PipelineState (compute) = DXC HLSL → DXIL 산출
  - D3D12MA 가 Particle SoA buffer (RWStructuredBuffer<float>) 할당
  - RDG-style barrier (D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) 명시 = UAV barrier between phase 5 enqueue / Dispatch / phase 7 Renderer SRV read
  - DX11 fallback = m_bGpuSupported false → 1 회 warning + CPU 강제 (legacy maintenance, 박제 보존)

신규 cpp 정정 (FxGpuComputeDispatch.cpp 의 Dispatch 본문 추가):
```

```cpp
void CFxGpuComputeDispatch::Dispatch(IRHICommandList* pCmdList)
{
    if (!m_bGpuSupported || !pCmdList) return;
    if (m_vecPending.empty()) return;
    if (!m_pPipelineParticleUpdate || !m_pBindGroupLayout) return;

    // DX12 RDG-style barrier (UAV → UAV between dispatches)
    pCmdList->ResourceBarrier_UAVToUAV(m_pParticleSrvUav.get());

    for (const FxGpuSystemTick& tick : m_vecPending)
    {
        pCmdList->SetComputePipelineState(m_pPipelineParticleUpdate);
        pCmdList->SetComputeBindGroup(0, m_pBindGroupLayout,
            /*srv*/ nullptr,
            /*uav*/ m_pParticleSrvUav.get(),
            /*cb*/  m_pParameterUav.get());
        const u32_t uGroups = (tick.uNumInstances + 63u) / 64u;
        pCmdList->Dispatch(uGroups, 1, 1);
    }

    // Phase 6/7 Renderer 가 read 하기 전에 UAV → SRV transition
    pCmdList->ResourceBarrier_UAVToSRV(m_pParticleSrvUav.get());
}
```

### §2.4 부속 22 Compile (DXC primary)

```txt
영향 위치: Engine/Private/FX/v2/Compiler/FxHlslTranslator.cpp 의 Translate
v2 박제: HLSL string 산출 (utf-8 bytes)

DX12 정정:
  - HLSL string → DXC 컴파일 호출 추가 = `dxcompiler.dll` 의 IDxcCompiler3::Compile
  - 산출 = DXIL bytecode (vector<u8_t>)
  - FxCompileResult.hlslBytes = "DXIL bytecode" 의미로 변경 (DX11 빌드 시 = DXBC bytecode)
  - DXC arguments: -T cs_6_0 / vs_6_0 / ps_6_0, -E main, -spirv (Vulkan 옵션, RH-6 진입 후)

cpp 정정 (FxHlslTranslator.cpp 의 Translate 끝부분 추가):
```

```cpp
// HLSL string 산출 직후 (utf8 bytes 직전)
#if defined(WINTERS_RHI_BACKEND_DX12)
    // DXC 컴파일
    Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
    DxcBuffer src{};
    src.Ptr = utf8.data();
    src.Size = utf8.size();
    src.Encoding = DXC_CP_UTF8;
    LPCWSTR args[] = { L"-T", L"cs_6_0", L"-E", L"main" };     // compute. ps_6_0 / vs_6_0 도 동일 패턴
    Microsoft::WRL::ComPtr<IDxcResult> pResult;
    pCompiler->Compile(&src, args, _countof(args), nullptr, IID_PPV_ARGS(&pResult));
    // pResult->GetOutput(DXC_OUT_OBJECT, ...) → DXIL bytecode
    Microsoft::WRL::ComPtr<IDxcBlob> pDxil;
    pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pDxil), nullptr);
    if (pDxil)
    {
        result.hlslBytes.assign((u8_t*)pDxil->GetBufferPointer(), (u8_t*)pDxil->GetBufferPointer() + pDxil->GetBufferSize());
    }
    else
    {
        result.hlslBytes.assign(utf8.begin(), utf8.end());     // fallback = HLSL string (debug)
    }
#else
    result.hlslBytes.assign(utf8.begin(), utf8.end());
#endif
```

### §2.5 부속 23 DataInterface (root signature 통합)

```txt
영향 위치: 6 .ush 파일의 register binding
v2 박제: register(b2, space0) ~ register(t13, space0) 등 명시

DX12 정정:
  - 모든 register space0 명시 = OK (이미 v2 박제 정합)
  - Root signature = master material 별. 6 DI 의 SRV / CBV slot = master material 의 root signature 안에 포함
  - Bindless 미사용 (호환성 우선)

추가 검증: grep "register\\(.*space0\\)" Shaders/FX/v2/   → 모든 register 명시
```

### §2.6 부속 24 Editor (ImGui DX12 backend)

```txt
영향 위치: Tools/WintersEditor/Private/main.cpp 의 ImGui init
v2 박제: ImGui DX11 backend 가정

DX12 정정:
  - ImGui_ImplDX12_Init(pDevice, NUM_FRAMES, RTV_FORMAT, srvDescHeap, srvDescHeapStart) 호출
  - srvDescHeap = ImGui 의 font / image SRV 용 별도 DescriptorHeap 박제
  - CFxPreviewViewport 의 m_pColorRT = DX12 ID3D12Resource (RTV heap 의 entry)
  - GetNativeSRV() 가 DX12 시 = D3D12_GPU_DESCRIPTOR_HANDLE 반환 (SRV heap 의 GPU handle, ImTextureID)

main.cpp 정정:
```

```cpp
// Tools/WintersEditor/Private/main.cpp DX12 분기
#if defined(WINTERS_RHI_BACKEND_DX12)
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(pDX12Device, kNumFrames, DXGI_FORMAT_R8G8B8A8_UNORM, pSrvHeap,
        pSrvHeap->GetCPUDescriptorHandleForHeapStart(),
        pSrvHeap->GetGPUDescriptorHandleForHeapStart());
#else
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(pDX11Device, pDX11Context);
#endif
```

### §2.7 부속 26 Hot reload (DXC 컴파일 latency)

```txt
영향 위치: CFxScriptCompileQueue 의 worker thread 컴파일
v2 박제: HlslTranslator + VMTranslator 호출

DX12 정정:
  - HlslTranslator 호출 = DXC 컴파일 포함 (위 §2.4)
  - DXC latency = 50~150ms (FXC 100~300ms) → recompile 200ms 합격 더 쉬움
  - DXC 의 -O0 (debug) / -O3 (release) 옵션 분기 = WINTERS_DEBUG vs Release
```

### §2.8 부속 27 Master Material (3 PSO)

```txt
영향 위치: Engine/Private/FX/v2/Material/FxMasterMaterialRegistry.cpp 의 BuildStandardMasters
v2 박제: master material descriptor 만 (HLSL path, parameter map)

DX12 정정:
  - BuildStandardMasters 호출 후 즉시 3 PSO 생성:
    1. M_VFX_Particle PSO = ID3D12PipelineState (graphics, blend = additive, depth = read-only)
    2. M_VFX_Trail PSO = 동일 + topology = TriangleStrip
    3. M_VFX_Volumetric PSO = 동일 + blend = AlphaBlend, depth = none
  - Root signature 3 종 박제 (위 §2.2)
  - PSO 컴파일 = Engine init 1 회 (cooked .wfxbin 의 DXIL bytes 사용 또는 DXC 호출)

cpp 정정 (FxMasterMaterialRegistry.cpp 의 BuildStandardMasters 끝부분):
```

```cpp
void CFxMasterMaterialRegistry::BuildStandardMasters()
{
    // (descriptor 빌드 = 부속 27 v2 박제 그대로)
    m_Particle.eType = eFxMasterMaterial::Particle_Generic;
    // ... 기존 박제 ...

#if defined(WINTERS_RHI_BACKEND_DX12)
    // DX12 PSO 즉시 컴파일
    auto* pDevice = CGameInstance::Get()->Get_RHIDevice();
    if (pDevice)
    {
        BuildPSOForMaster(pDevice, m_Particle);
        BuildPSOForMaster(pDevice, m_Trail);
        BuildPSOForMaster(pDevice, m_Volumetric);
    }
#endif
}

#if defined(WINTERS_RHI_BACKEND_DX12)
void CFxMasterMaterialRegistry::BuildPSOForMaster(IRHIDevice* pDevice, FxMasterMaterial& master)
{
    // 1. Root signature 빌드
    RHIRootSignatureDesc rsDesc{};
    rsDesc.AddCBV(0, 0);     // b0 PerFrame
    rsDesc.AddCBV(1, 0);     // b1 PerEmitter
    rsDesc.AddCBV(2, 0);     // b2 master material params
    rsDesc.AddSRVRange(0, 4);     // t0~t3
    rsDesc.AddSampler(0);     // s0
    master.pRootSig = pDevice->CreateRootSignature(rsDesc);

    // 2. Shader compile (DXC HLSL → DXIL)
    std::vector<u8_t> vsBytes = CompileHlslToDxil(master.strVsHlslPath, "vs_6_0");
    std::vector<u8_t> psBytes = CompileHlslToDxil(master.strPsHlslPath, "ps_6_0");

    // 3. PSO desc
    RHIGraphicsPipelineDesc psoDesc{};
    psoDesc.pRootSig = master.pRootSig;
    psoDesc.vsBytes = std::move(vsBytes);
    psoDesc.psBytes = std::move(psBytes);
    psoDesc.eBlend = master.eBlend;
    psoDesc.eRtFormat = eRHIFormat::R8G8B8A8_UNORM;
    psoDesc.eDsFormat = eRHIFormat::D24_UNORM_S8_UINT;
    master.pPipeline = pDevice->CreateGraphicsPipelineState(psoDesc);
}
#endif
```

---

## §3 RHI 백엔드 결정값 갱신 (17 마스터 §1)

```txt
17 §1 사전 결정 표의 "RHI 백엔드" 행 갱신:

이전 (5/7 v4 마스터):
  | RHI 백엔드 | RH-7 추상 통과 (IRHIDevice / IRHIPipelineState / IRHIBindGroup) |
              | Track 2 W7-9 Scaffold 와 정합. Renderer 본체 = DX11 native handle 추출 금지 |

본 28 갱신:
  | RHI 백엔드 | DX12 = primary. DX11 = legacy maintenance only. RH-7 추상 통과 (8 인터페이스) |
              | Track 2 W7-9 Scaffold 박제 후 진입. DXC + D3D12MA + bindless 미사용 + root signature 명시 |

영향:
  - 부속 19 의 TickGPU = DX12 활성 (DX11 fallback = legacy)
  - 부속 21 의 CFxGpuComputeDispatch = DX12 SM 5.1+ 필수
  - 부속 22 의 HlslTranslator = DXC primary (FXC = legacy)
  - 부속 24 의 Editor ImGui = DX12 backend
  - 부속 27 의 Master Material PSO = DX12 PipelineStateDesc + RootSignature 명시
```

---

## §4 신규 RHI 인터페이스 (Track 2 RH-7 추가 박제 의존)

```txt
부속 28 박제 시점에 다음 RH-7 인터페이스가 필요. Track 2 W7-9 의 추가 박제로 진입.

신규 (Track 2 W7-9 의 후속 박제):
  IRHIRootSignature           DX12 root signature + DX11 호환 layer (DX11 = bindings descriptor table 시뮬)
  IRHIDescriptorHeap          DX12 SRV / RTV / DSV / Sampler heap. DX11 = no-op (legacy)
  IRHICommandList::ResourceBarrier_UAVToUAV(pBuffer)
  IRHICommandList::ResourceBarrier_UAVToSRV(pBuffer)
  IRHIDevice::CreateRootSignature(const RHIRootSignatureDesc&) → IRHIRootSignature*
  IRHIDevice::CreateGraphicsPipelineState(const RHIGraphicsPipelineDesc&) → IRHIPipelineState*

이미 박제 (Track 2 W7-9):
  IRHIDevice / IRHIPipelineState / IRHIBindGroup / IRHICommandList / IRHIQueue /
  IRHIRenderPass / IRHIBindGroupLayout / IRHISwapChain (8 인터페이스)
  RHICapabilities (bSupportsCompute / bSupportsStructuredBufferUAV)
  D3D12MA (ThirdPartyLib)

본 28 박제 = 위 신규 6 메서드를 Track 2 RH-7 의 후속 박제로 명시. EFX-3 진입 전 박제 완료 필수.
```

---

## §5 Track 2 일정 정합 (DX12 W7-9 합격 후 본 28 진입)

```txt
17 §13 EFX 일정 + Track 2 W7-9 정합:

EFX-0 (Legacy Bridge)         RHI 무관
EFX-1 (.wfx JSON)              RHI 무관
EFX-2 (Runtime SoA)            RH-7 추상 (DX11 가동) 통과
EFX-3 (Renderer 6 종 / 3 master)  ★ Track 2 W7-9 합격 후 진입 권장
                                  - DX11 가동 가능하지만 master material PSO = DX12 우선 박제
                                  - 본 28 의 §2.2 root signature 3 종 박제 필수
EFX-4 (Editor MVP)             ★ 본 28 §2.6 ImGui DX12 backend 박제 후 진입
EFX-5 (Compile Graph→VM/HLSL)  ★ 본 28 §2.4 DXC 컴파일 박제 후 진입
EFX-6 (DataInterface 6 종)     RH-7 추상 (DX12 root signature 통합)
EFX-7 (GPU compute)            ★ Track 2 W7-9 합격 + Track 2 W10-13 (DX12 visual parity) 합격 후
                                  - DX11 fallback 비표준 (legacy maintenance only)
EFX-8 (Hot reload)             DXC latency 200ms 이내 (FXC 보다 빠름)
EFX-9 (.wfxbin cooked)         DXIL bytecode 직렬화 (FXC DXBC 는 legacy)

권장 진입 순서:
  Track 2 W7-9 (DX12 Bootstrap) 합격 → 본 28 의 신규 RHI 인터페이스 6 메서드 박제 →
  부속 18~22 v2 박제 코드화 → 부속 27 v2 박제 코드화 (master material 3 종) →
  본 28 의 §2 부속별 정정 적용 → EFX-3 진입
```

---

## §6 검증 명령

```txt
1. grep "WINTERS_RHI_BACKEND_DX12" Engine/{Public,Private}/FX/v2/   → 부속 19/20/21/22/23/24/27 모두 가드 명시
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/   → 0 hit (DX12 primary, RH-7 추상만)
3. grep "ID3D12" Engine/{Public,Private}/FX/v2/   → 0 hit (RH-7 추상만, DX12 native = Engine/Private/RHI/DX12/ 한정)
4. grep "register\\(.*space0\\)" Shaders/FX/v2/   → 모든 register 명시
5. grep "TBD" .md/plan/EffectTool/28_DX12_MIGRATION_FX_INTEGRATION_BAKE.md  → 0 hit
6. grep "stub|scaffold|본 박제 시점.*채움" 본 28  → 0 hit
7. CFxMasterMaterialRegistry::BuildPSOForMaster (DX12) → 3 PSO 생성 검증
8. CFxGpuComputeDispatch::Dispatch (DX12) → ResourceBarrier_UAVToUAV / UAVToSRV 호출 검증
9. CFxHlslTranslator::Translate (DX12) → DXIL bytecode 산출 검증 (FxCompileResult.hlslBytes 의 첫 4 byte = DXIL magic 'DXBC' or 'DXIL')
10. WintersEditor.exe DX12 backend → ImGui_ImplDX12_Init 호출 검증
11. EFX-7 GPU compute: DX12 path 8192 입자 / 64 emitter / 1 frame 1.5ms 이내 (Track 2 W10-13 합격 의존)
12. EFX-8 Hot reload: DXC 컴파일 200ms 이내 (master material recompile)
```

---

## §7 박제 함정 매트릭스

| 함정 | 본 28 회피 |
|---|---|
| P-1 + P-6 | §1 8 항목, TBD 0 |
| P-2 (PIMPL 추측) | DX12 cpp 분기 = 헤더 + cpp 동시 |
| P-3 (모든 path) | 8 부속 영향 한 번에 + DX12 / DX11 fallback 양 경로 |
| P-4 (Scene 직접 의존) | RHI 호출 = IRHIDevice 만, Scene 무관 |
| P-7 (bitmask) | mask 미사용 |
| P-8 (인용 의미 반전) | DX12 SDK `D3D12.h` / `dxcompiler.h` API 직접 참조 |
| P-9 (ECS Scheduler) | GPU dispatch = phase 5 끝 (FxTickSystem). 이미 박제 |
| P-10 (Owner Scope) | RootSignature / PSO = `CFxMasterMaterialRegistry` (Tier-1) owned |
| P-11 (도메인 상수) | DX12 = RHI 결정. 도메인 무관 |
| P-12 (음수 truncation) | UAV barrier 인덱스 양수 |
| P-13 (미존재 API) | 신규 RHI 인터페이스 6 메서드 = §4 의 Track 2 RH-7 후속 박제 명시 (Track 2 진입 전 박제 의무) |
| P-14 (행동 정책 변경) | DX12 활성 = 신규 path. DX11 fallback 보존 |
| P-15 (헤더 외부 의존) | `dxcompiler.h` 등 DX12 SDK = `WINTERS_RHI_BACKEND_DX12` 가드 안에서만 include |
| P-16 (산술 검증) | DXIL magic / FXC DXBC magic 검증 |
| P-17 (typedef ABI) | 신규 RHI 인터페이스 = forward declare (헤더 ABI 유지) |
| P-18 (RHI 인프라) | RH-7 8 인터페이스 + 본 28 신규 6 메서드 = Track 2 의존 |
| P-19 (Render/Sim 결합) | UAV → SRV barrier = phase 5 → phase 6 분리 강제 |

---

## §8 변경 이력

```txt
2026-04-21    Phase G 초안
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 28 = 부속 11번 박제 (5/7 신규)
              - 사용자 DX12 이주 계획서 진행 중 + EffectTool 박제 정합
              - RHI 백엔드 결정값 DX12 우선 갱신
              - 8 부속 (19/20/21/22/23/24/26/27) 영향 매트릭스 박제
              - 신규 RHI 인터페이스 6 메서드 = Track 2 W7-9 후속 박제 명시
              - DXC + D3D12MA + Root Signature 3 종 + ImGui DX12 backend
              - EFX 일정 + Track 2 W7-9 / W10-13 정합
```

---

## §9 부속 27 + 28 박제 완료 후 EFX 일정 갱신 (17 §13 권위)

```txt
EFX-0    Legacy Bridge        2~3 일       11 챔프 hook manifest. RHI 무관.
EFX-1    .wfx JSON            3~4 일       structured JSON. RHI 무관.
EFX-2    Runtime SoA          1 주          RH-7 추상 (DX11 가동) 통과.
                                            ★ Track 2 W7-9 (DX12 Bootstrap) 합격 후 EFX-3 진입 권장
EFX-3    Master Material 3 + Renderer 6  1~2 주
                                            (구) 6 renderer InGame
                                            (신) M_VFX_Particle/Trail/Volumetric 인스턴스 9개 + 4 핵심 노브 디자이너 슬라이더
                                            DX12 PSO 박제 (본 28 §2.2)
EFX-4    Editor MVP           2~3 주        7 패널 (Graph 후순위)
                                            ImGui DX12 backend (본 28 §2.6)
EFX-5    Compile (Graph→VM)   1~2 주        2차 트랙. 1차 = master material 정적 HLSL
                                            DXC 컴파일 (본 28 §2.4)
EFX-6    DataInterface 6 종   1~2 주        DX12 root signature 통합 (본 28 §2.5)
EFX-7    GPU compute          3~4 주        ★ Track 2 W10-13 합격 후. DX11 fallback legacy.
EFX-8    Hot reload           3~5 일        DXC 200ms (본 28 §2.7)
EFX-9    .wfxbin cooked       3~5 일        DXIL bytecode 직렬화

LoL runtime MVP        4~6 주
디자이너 Editor MVP    8~12 주 (master material 3 + 4 핵심 노브 + Curve)
DX12 Full + Elden      16~22 주 (GPU compute + 6 renderer 모두 + boss telegraph)
```
