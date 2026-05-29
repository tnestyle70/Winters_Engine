# 07 -- UE Niagara-Style VFX System

> Winters Engine API Modernization -- Stage 7
> Date: 2026-05-02
> Depends on: IRHIDevice (RH-0), ECS CWorld, CMaterialPBR (Track 1)

---

## 1. Architecture Overview

The current FX pipeline (`CFxSystem` + `FxBillboardComponent` + `FxMeshComponent`) is fully manual:
each champion skill file constructs POD structs with hardcoded values, spawns them via
`CFxSystem::Spawn()`, and the system iterates every frame to update lifetime/position and render.

This plan replaces that approach with a **data-driven, GPU-accelerated particle system** inspired
by UE5 Niagara. Key concepts:

| Concept | Winters Class | Role |
|---|---|---|
| NiagaraSystem | `CNiagaraSystem` | Top-level effect asset. Contains 1..N emitters. |
| NiagaraEmitter | `CNiagaraEmitter` | Single emitter: spawn rate, modules, renderer type. |
| NiagaraModule | `INiagaraModule` | Pluggable behavior: InitialVelocity, Gravity, ColorOverLife, etc. |
| ParticleBuffer | `CParticleBuffer` | GPU-side SoA structured buffers for particle data. |
| Renderer | `eParticleRenderer` | Billboard / Mesh / Ribbon / Light rendering modes. |

### Data Flow (per frame)

```
1. CNiagaraSystem::Tick(dt)
   |-- for each CNiagaraEmitter:
       |-- Spawn new particles (CPU -> append buffer or CS)
       |-- CS Dispatch: ParticleUpdate.hlsl (apply modules)
       |-- GPU Readback: alive count (indirect args)
2. CNiagaraSystem::Render(matVP)
   |-- for each CNiagaraEmitter:
       |-- Bind SoA SRV
       |-- DrawInstancedIndirect (billboard / mesh / ribbon)
```

---

## 2. File Structure

```
Engine/
  Public/VFX/
    INiagaraModule.h          -- module interface
    CNiagaraSystem.h          -- system (asset)
    CNiagaraEmitter.h         -- emitter definition
    CParticleBuffer.h         -- GPU SoA buffers
    NiagaraModules.h          -- built-in modules (spawn, velocity, gravity, color, size, noise)
  Private/VFX/
    CNiagaraSystem.cpp
    CNiagaraEmitter.cpp
    CParticleBuffer.cpp
    NiagaraModules.cpp
Shaders/
  VFX/
    ParticleUpdate.hlsl       -- compute shader: per-particle update
    ParticleBillboard.hlsl    -- VS/PS for camera-facing quads
    ParticleRibbon.hlsl       -- VS/GS/PS for ribbon trails
```

---

## 3. Full Code

### 3.1 INiagaraModule.h

```cpp
// Engine/Public/VFX/INiagaraModule.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <memory>

// Forward
struct ParticlePayload;

// ---------------------------------------------------------------
//  eNiagaraModulePhase -- when the module executes
// ---------------------------------------------------------------
enum class eNiagaraModulePhase : u8_t
{
    Spawn,      // once when particle is born
    Update,     // every frame while alive
    Render,     // just before draw (optional CPU-side override)
};

// ---------------------------------------------------------------
//  INiagaraModule -- pluggable particle behavior
//
//  Modules are lightweight value objects. They write into a
//  ParticlePayload struct that maps 1:1 to the GPU SoA layout.
//  GPU modules contribute HLSL snippets via GetHLSLSource().
// ---------------------------------------------------------------
class WINTERS_ENGINE INiagaraModule
{
public:
    virtual ~INiagaraModule() = default;

    virtual eNiagaraModulePhase GetPhase() const = 0;
    virtual std::string         GetName()  const = 0;

    // CPU fallback: called per-particle on spawn or update.
    // GPU path ignores this and uses the compute shader instead.
    virtual void Execute(ParticlePayload& particle, f32_t fDeltaTime) = 0;

    // Return HLSL snippet to inject into ParticleUpdate.hlsl.
    // Empty string = CPU-only module.
    virtual std::string GetHLSLSource() const { return {}; }

    virtual std::unique_ptr<INiagaraModule> Clone() const = 0;
};
```

### 3.2 CParticleBuffer.h

```cpp
// Engine/Public/VFX/CParticleBuffer.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <memory>
#include <d3d11.h>
#include <wrl/client.h>

class IRHIDevice;

// ---------------------------------------------------------------
//  ParticlePayload -- mirrors GPU SoA layout (CPU staging)
//  Each field is stored as a separate StructuredBuffer on GPU
//  for cache-friendly CS reads.
// ---------------------------------------------------------------
struct ParticlePayload
{
    Vec3  vPosition     = { 0.f, 0.f, 0.f };
    Vec3  vVelocity     = { 0.f, 0.f, 0.f };
    Vec4  vColor        = { 1.f, 1.f, 1.f, 1.f };
    f32_t fSize         = 1.f;
    f32_t fAge          = 0.f;
    f32_t fLifetime     = 1.f;
    f32_t fRotation     = 0.f;   // radians (billboard Z spin)
    u32_t iAlive        = 1;     // 0 = dead, 1 = alive
    u32_t _pad0         = 0;
    u32_t _pad1         = 0;
    u32_t _pad2         = 0;
};
static_assert(sizeof(ParticlePayload) == 64, "ParticlePayload must be 64 bytes for GPU alignment");

// ---------------------------------------------------------------
//  CParticleBuffer -- GPU-side structured buffer pair (UAV + SRV)
//
//  Holds up to m_iMaxParticles particles in a StructuredBuffer.
//  Double-buffered: CS writes to UAV, VS/PS reads from SRV.
//  IndirectArgs buffer for DrawInstancedIndirect.
// ---------------------------------------------------------------
class WINTERS_ENGINE CParticleBuffer
{
public:
    ~CParticleBuffer() = default;
    CParticleBuffer(const CParticleBuffer&) = delete;
    CParticleBuffer& operator=(const CParticleBuffer&) = delete;

    static std::unique_ptr<CParticleBuffer> Create(IRHIDevice* pDevice, u32_t iMaxParticles);

    // Upload newly spawned particles from CPU staging into append region
    void UploadSpawned(ID3D11DeviceContext* pContext,
                       const ParticlePayload* pParticles, u32_t iCount);

    // Bind as UAV for compute shader update pass
    void BindForCompute(ID3D11DeviceContext* pContext, u32_t iSlot);

    // Bind as SRV for rendering pass
    void BindForRender(ID3D11DeviceContext* pContext, u32_t iSlot);

    // Copy alive count from GPU to indirect args buffer
    void UpdateIndirectArgs(ID3D11DeviceContext* pContext);

    // Getters
    ID3D11Buffer*              GetIndirectArgsBuffer() const { return m_pIndirectArgs.Get(); }
    u32_t                      GetMaxParticles()       const { return m_iMaxParticles; }

    // Readback alive count (for debug / CPU culling decisions)
    u32_t ReadbackAliveCount(ID3D11DeviceContext* pContext);

private:
    CParticleBuffer() = default;

    u32_t m_iMaxParticles = 0;

    // Particle data (structured buffer)
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_pParticleBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pParticleUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pParticleSRV;

    // Alive counter (atomic via AppendStructuredBuffer)
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_pAliveCounter;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pAliveCounterUAV;

    // DrawInstancedIndirect args
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_pIndirectArgs;

    // Staging buffer for CPU readback
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_pStagingBuffer;
};
```

### 3.3 CParticleBuffer.cpp

```cpp
// Engine/Private/VFX/CParticleBuffer.cpp
#include "VFX/CParticleBuffer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"

namespace
{
    ID3D11Device* GetDX11(IRHIDevice* p)
    {
        return static_cast<ID3D11Device*>(
            p->GetNativeHandle(eNativeHandleType::DX11Device));
    }
}

std::unique_ptr<CParticleBuffer> CParticleBuffer::Create(IRHIDevice* pDevice, u32_t iMaxParticles)
{
    if (!pDevice || iMaxParticles == 0) return nullptr;

    auto* pDev = GetDX11(pDevice);
    if (!pDev) return nullptr;

    auto pBuf = std::unique_ptr<CParticleBuffer>(new CParticleBuffer());
    pBuf->m_iMaxParticles = iMaxParticles;

    // --- Particle StructuredBuffer (UAV + SRV) ---
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth           = sizeof(ParticlePayload) * iMaxParticles;
    bd.Usage               = D3D11_USAGE_DEFAULT;
    bd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(ParticlePayload);

    HRESULT hr = pDev->CreateBuffer(&bd, nullptr, pBuf->m_pParticleBuffer.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format              = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements  = iMaxParticles;
    uavDesc.Buffer.Flags        = 0;
    hr = pDev->CreateUnorderedAccessView(pBuf->m_pParticleBuffer.Get(),
        &uavDesc, pBuf->m_pParticleUAV.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format              = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements  = iMaxParticles;
    hr = pDev->CreateShaderResourceView(pBuf->m_pParticleBuffer.Get(),
        &srvDesc, pBuf->m_pParticleSRV.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    // --- Alive counter (1-element structured buffer with atomic) ---
    D3D11_BUFFER_DESC counterDesc{};
    counterDesc.ByteWidth           = sizeof(u32_t);
    counterDesc.Usage               = D3D11_USAGE_DEFAULT;
    counterDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
    counterDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    counterDesc.StructureByteStride = sizeof(u32_t);
    hr = pDev->CreateBuffer(&counterDesc, nullptr, pBuf->m_pAliveCounter.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    D3D11_UNORDERED_ACCESS_VIEW_DESC counterUAV{};
    counterUAV.Format              = DXGI_FORMAT_UNKNOWN;
    counterUAV.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
    counterUAV.Buffer.FirstElement = 0;
    counterUAV.Buffer.NumElements  = 1;
    counterUAV.Buffer.Flags        = D3D11_BUFFER_UAV_FLAG_COUNTER;
    hr = pDev->CreateUnorderedAccessView(pBuf->m_pAliveCounter.Get(),
        &counterUAV, pBuf->m_pAliveCounterUAV.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    // --- Indirect args buffer (DrawInstancedIndirect: 4 x u32) ---
    D3D11_BUFFER_DESC indirectDesc{};
    indirectDesc.ByteWidth = sizeof(u32_t) * 4;
    indirectDesc.Usage     = D3D11_USAGE_DEFAULT;
    indirectDesc.BindFlags = 0;
    indirectDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    hr = pDev->CreateBuffer(&indirectDesc, nullptr, pBuf->m_pIndirectArgs.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    // --- Staging buffer for CPU readback ---
    D3D11_BUFFER_DESC stagingDesc{};
    stagingDesc.ByteWidth  = sizeof(u32_t) * 4;
    stagingDesc.Usage      = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = pDev->CreateBuffer(&stagingDesc, nullptr, pBuf->m_pStagingBuffer.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    return pBuf;
}

void CParticleBuffer::UploadSpawned(ID3D11DeviceContext* pContext,
                                     const ParticlePayload* pParticles, u32_t iCount)
{
    if (!pContext || !pParticles || iCount == 0) return;

    // UpdateSubresource into the particle buffer at the next free slot.
    // In production this would use a staging ring buffer; simplified here.
    D3D11_BOX box{};
    box.left   = 0; // Append logic handled by CS alive list
    box.right  = sizeof(ParticlePayload) * iCount;
    box.top    = 0;
    box.bottom = 1;
    box.front  = 0;
    box.back   = 1;

    pContext->UpdateSubresource(m_pParticleBuffer.Get(), 0, &box,
                                pParticles, 0, 0);
}

void CParticleBuffer::BindForCompute(ID3D11DeviceContext* pContext, u32_t iSlot)
{
    if (!pContext) return;
    ID3D11UnorderedAccessView* uavs[] = { m_pParticleUAV.Get(), m_pAliveCounterUAV.Get() };
    u32_t initCounts[] = { 0xFFFFFFFF, 0 };
    pContext->CSSetUnorderedAccessViews(iSlot, 2, uavs, initCounts);
}

void CParticleBuffer::BindForRender(ID3D11DeviceContext* pContext, u32_t iSlot)
{
    if (!pContext) return;
    ID3D11ShaderResourceView* srvs[] = { m_pParticleSRV.Get() };
    pContext->VSSetShaderResources(iSlot, 1, srvs);
    pContext->PSSetShaderResources(iSlot, 1, srvs);
}

void CParticleBuffer::UpdateIndirectArgs(ID3D11DeviceContext* pContext)
{
    if (!pContext) return;
    // Copy alive count into indirect args buffer:
    // DrawInstancedIndirect expects (VertexCountPerInstance, InstanceCount, StartVertex, StartInstance)
    // For billboard quads: VertexCount=6, InstanceCount=aliveCount
    pContext->CopyStructureCount(m_pIndirectArgs.Get(), 4, m_pAliveCounterUAV.Get());
}

u32_t CParticleBuffer::ReadbackAliveCount(ID3D11DeviceContext* pContext)
{
    if (!pContext) return 0;
    pContext->CopyResource(m_pStagingBuffer.Get(), m_pIndirectArgs.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(pContext->Map(m_pStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        u32_t count = reinterpret_cast<u32_t*>(mapped.pData)[1]; // InstanceCount slot
        pContext->Unmap(m_pStagingBuffer.Get(), 0);
        return count;
    }
    return 0;
}
```

### 3.4 CNiagaraEmitter.h

```cpp
// Engine/Public/VFX/CNiagaraEmitter.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "VFX/INiagaraModule.h"
#include "VFX/CParticleBuffer.h"
#include <memory>
#include <vector>
#include <string>

class IRHIDevice;
struct ID3D11DeviceContext;

namespace Engine { class CTexture; }
class DX11Shader;
class DX11Pipeline;

// ---------------------------------------------------------------
//  eParticleRenderer -- how particles are drawn
// ---------------------------------------------------------------
enum class eParticleRenderer : u8_t
{
    Billboard,      // Camera-facing quads (default)
    Mesh,           // Instanced mesh per particle
    Ribbon,         // Connected trail strip
    Light,          // Point light per particle (deferred only, future)
};

// ---------------------------------------------------------------
//  EmitterDesc -- creation parameters for CNiagaraEmitter
// ---------------------------------------------------------------
struct EmitterDesc
{
    std::string           strName          = "Emitter";
    u32_t                 iMaxParticles    = 1024;
    f32_t                 fSpawnRate       = 100.f;  // particles per second
    f32_t                 fDefaultLifetime = 2.f;
    eParticleRenderer     eRenderer        = eParticleRenderer::Billboard;
    bool                  bLoop            = true;
    f32_t                 fDuration        = 0.f;    // 0 = infinite
};

// ---------------------------------------------------------------
//  CNiagaraEmitter -- a single particle emitter
//
//  Owns a CParticleBuffer and a list of INiagaraModules.
//  Tick() spawns particles + dispatches CS for update.
//  Render() draws via DrawInstancedIndirect.
// ---------------------------------------------------------------
class WINTERS_ENGINE CNiagaraEmitter
{
public:
    ~CNiagaraEmitter() = default;
    CNiagaraEmitter(const CNiagaraEmitter&) = delete;
    CNiagaraEmitter& operator=(const CNiagaraEmitter&) = delete;

    static std::unique_ptr<CNiagaraEmitter> Create(IRHIDevice* pDevice,
                                                    const EmitterDesc& desc);

    // Add a module (takes ownership via move)
    void AddModule(std::unique_ptr<INiagaraModule> pModule);

    // Per-frame update: spawn + CS dispatch
    void Tick(ID3D11DeviceContext* pContext, f32_t fDeltaTime,
              const Vec3& vSystemPos);

    // Render all alive particles
    void Render(ID3D11DeviceContext* pContext, const Mat4& matVP,
                const Vec3& vCamRight, const Vec3& vCamUp);

    // Bind texture for billboard/mesh rendering
    void SetTexture(Engine::CTexture* pTexture) { m_pTexture = pTexture; }

    // State queries
    const std::string& GetName()        const { return m_Desc.strName; }
    bool               IsFinished()     const { return m_bFinished; }
    u32_t              GetAliveCount()  const { return m_iAliveCount; }
    eParticleRenderer  GetRendererType() const { return m_Desc.eRenderer; }

private:
    CNiagaraEmitter() = default;

    void SpawnParticles(f32_t fDeltaTime, const Vec3& vSystemPos);
    void DispatchUpdate(ID3D11DeviceContext* pContext, f32_t fDeltaTime);

    EmitterDesc m_Desc{};

    // Modules by phase
    std::vector<std::unique_ptr<INiagaraModule>> m_vecSpawnModules;
    std::vector<std::unique_ptr<INiagaraModule>> m_vecUpdateModules;

    // GPU resources
    std::unique_ptr<CParticleBuffer> m_pBuffer;
    IRHIDevice*                      m_pDevice   = nullptr;
    Engine::CTexture*                m_pTexture  = nullptr;

    // Spawn accumulator
    f32_t m_fSpawnAccum   = 0.f;
    f32_t m_fElapsedTime  = 0.f;
    u32_t m_iAliveCount   = 0;
    u32_t m_iNextSpawnSlot = 0;
    bool  m_bFinished     = false;

    // CPU staging for newly spawned particles
    std::vector<ParticlePayload> m_vecSpawnStaging;

    // CS for particle update (compiled from module HLSL snippets)
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_pUpdateCS;
};
```

### 3.5 CNiagaraEmitter.cpp

```cpp
// Engine/Private/VFX/CNiagaraEmitter.cpp
#include "VFX/CNiagaraEmitter.h"
#include "VFX/INiagaraModule.h"
#include "VFX/CParticleBuffer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "Resource/Texture.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    ID3D11DeviceContext* GetCtx(IRHIDevice* p)
    {
        return static_cast<ID3D11DeviceContext*>(
            p->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }
}

std::unique_ptr<CNiagaraEmitter> CNiagaraEmitter::Create(
    IRHIDevice* pDevice, const EmitterDesc& desc)
{
    if (!pDevice || desc.iMaxParticles == 0) return nullptr;

    auto pEmitter = std::unique_ptr<CNiagaraEmitter>(new CNiagaraEmitter());
    pEmitter->m_Desc    = desc;
    pEmitter->m_pDevice = pDevice;

    pEmitter->m_pBuffer = CParticleBuffer::Create(pDevice, desc.iMaxParticles);
    if (!pEmitter->m_pBuffer) return nullptr;

    pEmitter->m_vecSpawnStaging.reserve(
        static_cast<size_t>(desc.fSpawnRate * 0.05f) + 16);

    return pEmitter;
}

void CNiagaraEmitter::AddModule(std::unique_ptr<INiagaraModule> pModule)
{
    if (!pModule) return;
    switch (pModule->GetPhase())
    {
    case eNiagaraModulePhase::Spawn:
        m_vecSpawnModules.push_back(std::move(pModule));
        break;
    case eNiagaraModulePhase::Update:
    case eNiagaraModulePhase::Render:
        m_vecUpdateModules.push_back(std::move(pModule));
        break;
    }
}

void CNiagaraEmitter::Tick(ID3D11DeviceContext* pContext, f32_t fDeltaTime,
                            const Vec3& vSystemPos)
{
    if (m_bFinished) return;

    m_fElapsedTime += fDeltaTime;

    // Check duration
    if (!m_Desc.bLoop && m_Desc.fDuration > 0.f && m_fElapsedTime >= m_Desc.fDuration)
    {
        m_bFinished = true;
        return;
    }

    // 1. Spawn
    SpawnParticles(fDeltaTime, vSystemPos);

    // 2. Upload spawned to GPU
    if (!m_vecSpawnStaging.empty())
    {
        m_pBuffer->UploadSpawned(pContext,
            m_vecSpawnStaging.data(),
            static_cast<u32_t>(m_vecSpawnStaging.size()));
        m_vecSpawnStaging.clear();
    }

    // 3. CS update dispatch
    DispatchUpdate(pContext, fDeltaTime);

    // 4. Update indirect args
    m_pBuffer->UpdateIndirectArgs(pContext);
    m_iAliveCount = m_pBuffer->ReadbackAliveCount(pContext);
}

void CNiagaraEmitter::SpawnParticles(f32_t fDeltaTime, const Vec3& vSystemPos)
{
    m_fSpawnAccum += m_Desc.fSpawnRate * fDeltaTime;
    u32_t iToSpawn = static_cast<u32_t>(m_fSpawnAccum);
    m_fSpawnAccum -= static_cast<f32_t>(iToSpawn);

    // Clamp to remaining capacity
    const u32_t iRemaining = m_Desc.iMaxParticles - m_iNextSpawnSlot;
    if (iToSpawn > iRemaining)
    {
        if (m_Desc.bLoop)
            m_iNextSpawnSlot = 0;  // wrap
        else
            iToSpawn = iRemaining;
    }

    for (u32_t i = 0; i < iToSpawn; ++i)
    {
        ParticlePayload p{};
        p.vPosition = vSystemPos;
        p.fLifetime = m_Desc.fDefaultLifetime;
        p.fAge      = 0.f;
        p.iAlive    = 1;

        // Apply spawn modules (CPU side)
        for (auto& mod : m_vecSpawnModules)
            mod->Execute(p, 0.f);

        m_vecSpawnStaging.push_back(p);
        ++m_iNextSpawnSlot;
    }
}

void CNiagaraEmitter::DispatchUpdate(ID3D11DeviceContext* pContext, f32_t fDeltaTime)
{
    if (!pContext || !m_pUpdateCS) return;

    // Bind particle buffer as UAV
    m_pBuffer->BindForCompute(pContext, 0);

    // Set compute shader
    pContext->CSSetShader(m_pUpdateCS.Get(), nullptr, 0);

    // Dispatch: 1 thread per particle, 64 threads per group
    const u32_t iGroups = (m_Desc.iMaxParticles + 63) / 64;
    pContext->Dispatch(iGroups, 1, 1);

    // Unbind
    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    u32_t initCounts[] = { 0, 0 };
    pContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, initCounts);
    pContext->CSSetShader(nullptr, nullptr, 0);
}

void CNiagaraEmitter::Render(ID3D11DeviceContext* pContext, const Mat4& matVP,
                              const Vec3& vCamRight, const Vec3& vCamUp)
{
    if (!pContext || m_iAliveCount == 0) return;

    // Bind particle data as SRV for VS
    m_pBuffer->BindForRender(pContext, 10); // t10

    // Bind texture
    if (m_pTexture)
        m_pTexture->Bind(pContext, 0);

    // Draw via indirect args
    switch (m_Desc.eRenderer)
    {
    case eParticleRenderer::Billboard:
    case eParticleRenderer::Mesh:
        pContext->DrawInstancedIndirect(
            m_pBuffer->GetIndirectArgsBuffer(), 0);
        break;
    case eParticleRenderer::Ribbon:
        // Ribbon uses GS expansion -- DrawIndirect with GS bound
        pContext->DrawInstancedIndirect(
            m_pBuffer->GetIndirectArgsBuffer(), 0);
        break;
    case eParticleRenderer::Light:
        // Deferred light injection -- future
        break;
    }

    // Unbind SRV
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
    pContext->VSSetShaderResources(10, 1, nullSRVs);
    pContext->PSSetShaderResources(10, 1, nullSRVs);
}
```

### 3.6 CNiagaraSystem.h

```cpp
// Engine/Public/VFX/CNiagaraSystem.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "VFX/CNiagaraEmitter.h"
#include <memory>
#include <vector>
#include <string>

class IRHIDevice;
struct ID3D11DeviceContext;

// ---------------------------------------------------------------
//  CNiagaraSystem -- top-level effect asset
//
//  A system contains 1..N emitters, each with their own modules
//  and renderer. Systems are instantiated at a world position and
//  ticked/rendered each frame.
//
//  Usage:
//    auto pFx = CNiagaraSystem::Create(pDevice, "IreliaQ_Trail");
//    pFx->AddEmitter(...);
//    // each frame:
//    pFx->SetPosition(worldPos);
//    pFx->Tick(dt);
//    pFx->Render(matVP, camRight, camUp);
// ---------------------------------------------------------------
class WINTERS_ENGINE CNiagaraSystem
{
public:
    ~CNiagaraSystem() = default;
    CNiagaraSystem(const CNiagaraSystem&) = delete;
    CNiagaraSystem& operator=(const CNiagaraSystem&) = delete;

    static std::unique_ptr<CNiagaraSystem> Create(IRHIDevice* pDevice,
                                                   const std::string& strName);

    // --- Build ---
    void AddEmitter(std::unique_ptr<CNiagaraEmitter> pEmitter);

    // --- Runtime ---
    void SetPosition(const Vec3& vPos)    { m_vPosition = vPos; }
    void SetRotation(const Vec3& vRot)    { m_vRotation = vRot; }
    void SetScale(f32_t fScale)           { m_fScale = fScale; }

    void Tick(f32_t fDeltaTime);
    void Render(const Mat4& matVP,
                const Vec3& vCamRight, const Vec3& vCamUp);

    // Deactivate: stop spawning, let existing particles die
    void Deactivate();

    // Hard kill: destroy immediately
    void Kill();

    // --- Queries ---
    bool IsFinished() const;
    const std::string& GetName() const { return m_strName; }
    u32_t GetTotalAliveParticles() const;
    u32_t GetEmitterCount() const { return static_cast<u32_t>(m_vecEmitters.size()); }

    // --- ImGui debug ---
    void OnImGui();

private:
    CNiagaraSystem() = default;

    std::string m_strName;
    IRHIDevice* m_pDevice = nullptr;

    Vec3  m_vPosition = { 0.f, 0.f, 0.f };
    Vec3  m_vRotation = { 0.f, 0.f, 0.f };
    f32_t m_fScale    = 1.f;
    bool  m_bActive   = true;

    std::vector<std::unique_ptr<CNiagaraEmitter>> m_vecEmitters;
};
```

### 3.7 CNiagaraSystem.cpp

```cpp
// Engine/Private/VFX/CNiagaraSystem.cpp
#include "VFX/CNiagaraSystem.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"

#ifdef _DEBUG
#undef new
#include <imgui.h>
#define new DBG_NEW
#endif

namespace
{
    ID3D11DeviceContext* GetCtx(IRHIDevice* p)
    {
        return static_cast<ID3D11DeviceContext*>(
            p->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }
}

std::unique_ptr<CNiagaraSystem> CNiagaraSystem::Create(
    IRHIDevice* pDevice, const std::string& strName)
{
    if (!pDevice) return nullptr;
    auto p = std::unique_ptr<CNiagaraSystem>(new CNiagaraSystem());
    p->m_strName = strName;
    p->m_pDevice = pDevice;
    return p;
}

void CNiagaraSystem::AddEmitter(std::unique_ptr<CNiagaraEmitter> pEmitter)
{
    if (pEmitter)
        m_vecEmitters.push_back(std::move(pEmitter));
}

void CNiagaraSystem::Tick(f32_t fDeltaTime)
{
    if (!m_bActive) return;

    auto* pCtx = GetCtx(m_pDevice);
    if (!pCtx) return;

    for (auto& pEmitter : m_vecEmitters)
    {
        if (!pEmitter->IsFinished())
            pEmitter->Tick(pCtx, fDeltaTime, m_vPosition);
    }
}

void CNiagaraSystem::Render(const Mat4& matVP,
                             const Vec3& vCamRight, const Vec3& vCamUp)
{
    auto* pCtx = GetCtx(m_pDevice);
    if (!pCtx) return;

    for (auto& pEmitter : m_vecEmitters)
    {
        if (!pEmitter->IsFinished())
            pEmitter->Render(pCtx, matVP, vCamRight, vCamUp);
    }
}

void CNiagaraSystem::Deactivate()
{
    m_bActive = false;
    // Emitters finish naturally when their particles die
}

void CNiagaraSystem::Kill()
{
    m_bActive = false;
    m_vecEmitters.clear();
}

bool CNiagaraSystem::IsFinished() const
{
    if (m_bActive) return false;
    for (auto& pEmitter : m_vecEmitters)
    {
        if (!pEmitter->IsFinished()) return false;
    }
    return true;
}

u32_t CNiagaraSystem::GetTotalAliveParticles() const
{
    u32_t total = 0;
    for (auto& pEmitter : m_vecEmitters)
        total += pEmitter->GetAliveCount();
    return total;
}

void CNiagaraSystem::OnImGui()
{
#ifdef _DEBUG
    if (!ImGui::TreeNode(m_strName.c_str()))
        return;

    ImGui::Text("Active: %s", m_bActive ? "Yes" : "No");
    ImGui::Text("Emitters: %u", GetEmitterCount());
    ImGui::Text("Total Particles: %u", GetTotalAliveParticles());
    ImGui::DragFloat3("Position", &m_vPosition.x, 0.1f);
    ImGui::DragFloat("Scale", &m_fScale, 0.01f, 0.01f, 10.f);

    if (ImGui::Button("Deactivate")) Deactivate();
    ImGui::SameLine();
    if (ImGui::Button("Kill")) Kill();

    for (u32_t i = 0; i < static_cast<u32_t>(m_vecEmitters.size()); ++i)
    {
        auto& em = m_vecEmitters[i];
        if (ImGui::TreeNode(em->GetName().c_str()))
        {
            ImGui::Text("Alive: %u", em->GetAliveCount());
            ImGui::Text("Renderer: %u", static_cast<u32_t>(em->GetRendererType()));
            ImGui::Text("Finished: %s", em->IsFinished() ? "Yes" : "No");
            ImGui::TreePop();
        }
    }

    ImGui::TreePop();
#endif
}
```

### 3.8 NiagaraModules.h (Built-in Modules)

```cpp
// Engine/Public/VFX/NiagaraModules.h
#pragma once

#include "VFX/INiagaraModule.h"
#include "VFX/CParticleBuffer.h"
#include <cmath>
#include <cstdlib>

// ---------------------------------------------------------------
//  Built-in Niagara Modules
// ---------------------------------------------------------------

// --- Spawn Phase Modules ---

class WINTERS_ENGINE NModInitialVelocity final : public INiagaraModule
{
public:
    NModInitialVelocity(const Vec3& vMin, const Vec3& vMax)
        : m_vMin(vMin), m_vMax(vMax) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Spawn; }
    std::string GetName() const override { return "InitialVelocity"; }

    void Execute(ParticlePayload& p, f32_t /*dt*/) override
    {
        auto randF = [](f32_t lo, f32_t hi) -> f32_t {
            f32_t t = static_cast<f32_t>(std::rand()) / static_cast<f32_t>(RAND_MAX);
            return lo + t * (hi - lo);
        };
        p.vVelocity.x = randF(m_vMin.x, m_vMax.x);
        p.vVelocity.y = randF(m_vMin.y, m_vMax.y);
        p.vVelocity.z = randF(m_vMin.z, m_vMax.z);
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModInitialVelocity>(m_vMin, m_vMax);
    }

private:
    Vec3 m_vMin, m_vMax;
};

class WINTERS_ENGINE NModInitialSize final : public INiagaraModule
{
public:
    NModInitialSize(f32_t fMin, f32_t fMax)
        : m_fMin(fMin), m_fMax(fMax) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Spawn; }
    std::string GetName() const override { return "InitialSize"; }

    void Execute(ParticlePayload& p, f32_t /*dt*/) override
    {
        f32_t t = static_cast<f32_t>(std::rand()) / static_cast<f32_t>(RAND_MAX);
        p.fSize = m_fMin + t * (m_fMax - m_fMin);
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModInitialSize>(m_fMin, m_fMax);
    }

private:
    f32_t m_fMin, m_fMax;
};

class WINTERS_ENGINE NModInitialColor final : public INiagaraModule
{
public:
    NModInitialColor(const Vec4& vColor) : m_vColor(vColor) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Spawn; }
    std::string GetName() const override { return "InitialColor"; }

    void Execute(ParticlePayload& p, f32_t /*dt*/) override
    {
        p.vColor = m_vColor;
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModInitialColor>(m_vColor);
    }

private:
    Vec4 m_vColor;
};

// --- Update Phase Modules ---

class WINTERS_ENGINE NModGravity final : public INiagaraModule
{
public:
    NModGravity(f32_t fGravity = -9.81f) : m_fGravity(fGravity) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Update; }
    std::string GetName() const override { return "Gravity"; }

    void Execute(ParticlePayload& p, f32_t dt) override
    {
        p.vVelocity.y += m_fGravity * dt;
    }

    std::string GetHLSLSource() const override
    {
        return "particle.velocity.y += g_fGravity * g_fDeltaTime;\n";
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModGravity>(m_fGravity);
    }

private:
    f32_t m_fGravity;
};

class WINTERS_ENGINE NModColorOverLife final : public INiagaraModule
{
public:
    NModColorOverLife(const Vec4& vStart, const Vec4& vEnd)
        : m_vStart(vStart), m_vEnd(vEnd) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Update; }
    std::string GetName() const override { return "ColorOverLife"; }

    void Execute(ParticlePayload& p, f32_t /*dt*/) override
    {
        f32_t t = (p.fLifetime > 0.f) ? (p.fAge / p.fLifetime) : 1.f;
        t = (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t);
        p.vColor.x = m_vStart.x + (m_vEnd.x - m_vStart.x) * t;
        p.vColor.y = m_vStart.y + (m_vEnd.y - m_vStart.y) * t;
        p.vColor.z = m_vStart.z + (m_vEnd.z - m_vStart.z) * t;
        p.vColor.w = m_vStart.w + (m_vEnd.w - m_vStart.w) * t;
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModColorOverLife>(m_vStart, m_vEnd);
    }

private:
    Vec4 m_vStart, m_vEnd;
};

class WINTERS_ENGINE NModSizeOverLife final : public INiagaraModule
{
public:
    NModSizeOverLife(f32_t fStart, f32_t fEnd)
        : m_fStart(fStart), m_fEnd(fEnd) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Update; }
    std::string GetName() const override { return "SizeOverLife"; }

    void Execute(ParticlePayload& p, f32_t /*dt*/) override
    {
        f32_t t = (p.fLifetime > 0.f) ? (p.fAge / p.fLifetime) : 1.f;
        t = (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t);
        p.fSize = m_fStart + (m_fEnd - m_fStart) * t;
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModSizeOverLife>(m_fStart, m_fEnd);
    }

private:
    f32_t m_fStart, m_fEnd;
};

class WINTERS_ENGINE NModDrag final : public INiagaraModule
{
public:
    NModDrag(f32_t fCoeff = 0.1f) : m_fCoeff(fCoeff) {}

    eNiagaraModulePhase GetPhase() const override { return eNiagaraModulePhase::Update; }
    std::string GetName() const override { return "Drag"; }

    void Execute(ParticlePayload& p, f32_t dt) override
    {
        f32_t factor = 1.f - m_fCoeff * dt;
        if (factor < 0.f) factor = 0.f;
        p.vVelocity.x *= factor;
        p.vVelocity.y *= factor;
        p.vVelocity.z *= factor;
    }

    std::unique_ptr<INiagaraModule> Clone() const override
    {
        return std::make_unique<NModDrag>(m_fCoeff);
    }

private:
    f32_t m_fCoeff;
};
```

---

## 4. Usage Examples

### 4.1 Irelia Q Dash Trail

```cpp
// Client/Private/GameObject/Champion/Irelia/IreliaQ_Fx.cpp (example usage)
void SpawnIreliaQ_Trail(IRHIDevice* pDevice, const Vec3& vStart, const Vec3& vEnd)
{
    auto pSystem = CNiagaraSystem::Create(pDevice, "IreliaQ_DashTrail");

    EmitterDesc desc;
    desc.strName        = "BladeTrail";
    desc.iMaxParticles  = 256;
    desc.fSpawnRate     = 200.f;
    desc.fDefaultLifetime = 0.4f;
    desc.eRenderer      = eParticleRenderer::Ribbon;
    desc.bLoop          = false;
    desc.fDuration      = 0.5f;

    auto pEmitter = CNiagaraEmitter::Create(pDevice, desc);

    // Spawn modules
    Vec3 dir = { vEnd.x - vStart.x, vEnd.y - vStart.y, vEnd.z - vStart.z };
    pEmitter->AddModule(std::make_unique<NModInitialVelocity>(
        Vec3{ dir.x * 8.f, 0.f, dir.z * 8.f },
        Vec3{ dir.x * 12.f, 1.f, dir.z * 12.f }));
    pEmitter->AddModule(std::make_unique<NModInitialColor>(
        Vec4{ 0.3f, 0.8f, 1.0f, 1.0f }));  // Irelia blade blue
    pEmitter->AddModule(std::make_unique<NModInitialSize>(0.05f, 0.15f));

    // Update modules
    pEmitter->AddModule(std::make_unique<NModColorOverLife>(
        Vec4{ 0.3f, 0.8f, 1.0f, 1.0f },
        Vec4{ 0.1f, 0.3f, 0.8f, 0.0f }));
    pEmitter->AddModule(std::make_unique<NModSizeOverLife>(0.15f, 0.0f));
    pEmitter->AddModule(std::make_unique<NModDrag>(2.0f));

    pSystem->AddEmitter(std::move(pEmitter));
    pSystem->SetPosition(vStart);

    // Register with scene VFX manager for ticking/rendering
    // g_pVfxManager->Register(std::move(pSystem));
}
```

### 4.2 Yasuo Tornado

```cpp
void SpawnYasuoTornado(IRHIDevice* pDevice, const Vec3& vPos, const Vec3& vDir)
{
    auto pSystem = CNiagaraSystem::Create(pDevice, "Yasuo_Tornado");

    EmitterDesc desc;
    desc.strName        = "TornadoCore";
    desc.iMaxParticles  = 512;
    desc.fSpawnRate     = 300.f;
    desc.fDefaultLifetime = 1.5f;
    desc.eRenderer      = eParticleRenderer::Billboard;
    desc.bLoop          = true;
    desc.fDuration      = 3.0f;

    auto pEmitter = CNiagaraEmitter::Create(pDevice, desc);
    pEmitter->AddModule(std::make_unique<NModInitialVelocity>(
        Vec3{ -2.f, 3.f, -2.f }, Vec3{ 2.f, 8.f, 2.f }));
    pEmitter->AddModule(std::make_unique<NModInitialColor>(
        Vec4{ 0.9f, 0.95f, 1.0f, 0.8f }));
    pEmitter->AddModule(std::make_unique<NModInitialSize>(0.3f, 0.8f));
    pEmitter->AddModule(std::make_unique<NModGravity>(-2.f)); // upward pull
    pEmitter->AddModule(std::make_unique<NModColorOverLife>(
        Vec4{ 0.9f, 0.95f, 1.0f, 0.8f },
        Vec4{ 1.0f, 1.0f, 1.0f, 0.0f }));
    pEmitter->AddModule(std::make_unique<NModSizeOverLife>(0.8f, 2.0f));

    pSystem->AddEmitter(std::move(pEmitter));
    pSystem->SetPosition(vPos);
}
```

### 4.3 Ezreal Q Projectile

```cpp
void SpawnEzrealQ(IRHIDevice* pDevice, const Vec3& vPos, const Vec3& vDir)
{
    auto pSystem = CNiagaraSystem::Create(pDevice, "Ezreal_Q");

    // Core glow
    EmitterDesc core;
    core.strName        = "CoreGlow";
    core.iMaxParticles  = 64;
    core.fSpawnRate     = 60.f;
    core.fDefaultLifetime = 0.3f;
    core.eRenderer      = eParticleRenderer::Billboard;
    core.bLoop          = true;

    auto pCore = CNiagaraEmitter::Create(pDevice, core);
    pCore->AddModule(std::make_unique<NModInitialVelocity>(
        Vec3{ -0.5f, -0.5f, -0.5f }, Vec3{ 0.5f, 0.5f, 0.5f }));
    pCore->AddModule(std::make_unique<NModInitialColor>(
        Vec4{ 1.0f, 0.85f, 0.2f, 1.0f })); // golden
    pCore->AddModule(std::make_unique<NModInitialSize>(0.1f, 0.2f));
    pCore->AddModule(std::make_unique<NModColorOverLife>(
        Vec4{ 1.0f, 0.85f, 0.2f, 1.0f },
        Vec4{ 1.0f, 0.5f, 0.0f, 0.0f }));

    // Trail sparkles
    EmitterDesc trail;
    trail.strName        = "TrailSparkles";
    trail.iMaxParticles  = 128;
    trail.fSpawnRate     = 100.f;
    trail.fDefaultLifetime = 0.5f;
    trail.eRenderer      = eParticleRenderer::Billboard;
    trail.bLoop          = true;

    auto pTrail = CNiagaraEmitter::Create(pDevice, trail);
    pTrail->AddModule(std::make_unique<NModInitialVelocity>(
        Vec3{ -1.f, -1.f, -1.f }, Vec3{ 1.f, 1.f, 1.f }));
    pTrail->AddModule(std::make_unique<NModInitialColor>(
        Vec4{ 1.0f, 0.9f, 0.4f, 0.6f }));
    pTrail->AddModule(std::make_unique<NModInitialSize>(0.02f, 0.06f));
    pTrail->AddModule(std::make_unique<NModDrag>(3.f));
    pTrail->AddModule(std::make_unique<NModSizeOverLife>(0.06f, 0.0f));

    pSystem->AddEmitter(std::move(pCore));
    pSystem->AddEmitter(std::move(pTrail));
    pSystem->SetPosition(vPos);
}
```

---

## 5. Migration from Current FxSystem

### Before (manual FxBillboardComponent)

```cpp
// Irelia Q: hardcoded per-component spawn
FxBillboardComponent fx;
fx.texturePath = L"Client/Bin/Resource/Texture/FX/trail_blue.png";
fx.vWorldPos   = vStart;
fx.fWidth      = 0.5f;
fx.fHeight     = 2.0f;
fx.fLifetime   = 0.4f;
fx.vColor      = { 0.3f, 0.8f, 1.0f, 1.0f };
fx.fFadeOut    = 0.2f;
fx.blendMode   = eBlendPreset::Additive;
CFxSystem::Spawn(world, fx);
```

### After (data-driven Niagara)

```cpp
// Load pre-authored system asset (or construct in code as above)
auto pFx = CNiagaraSystem::Create(pDevice, "IreliaQ_DashTrail");
// ... add emitters + modules (see 4.1) ...
g_pVfxManager->Register(std::move(pFx));
```

### Coexistence Strategy

- `CFxSystem` continues to work for existing champion FX
- New FX are authored with `CNiagaraSystem`
- Gradual per-champion migration: Irelia first, then Yasuo, etc.
- `CFxSystem::Spawn()` can internally create a `CNiagaraSystem` wrapper (adapter)

---

## 6. Verification Checklist

| # | Check | Pass Criteria |
|---|---|---|
| 1 | `CParticleBuffer::Create` succeeds | Non-null return, UAV+SRV valid |
| 2 | `CNiagaraEmitter::Tick` spawns particles | `GetAliveCount() > 0` after 1 frame |
| 3 | CS dispatch completes without device error | `ID3D11InfoQueue` 0 errors |
| 4 | `DrawInstancedIndirect` renders quads | Visual: particles appear on screen |
| 5 | Modules modify particle state | Gravity: particles fall; ColorOverLife: fade visible |
| 6 | System deactivate/kill works | `IsFinished()` returns true after kill |
| 7 | ImGui overlay shows particle counts | `CNiagaraSystem::OnImGui()` renders tree |
| 8 | Frame time <= 12ms with 2048 particles | GPU profiler stamp |
| 9 | No D3D resource leak on shutdown | `ID3D11Debug::ReportLiveDeviceObjects` clean |

---

## 7. Compute Shader: ParticleUpdate.hlsl

```hlsl
// Shaders/VFX/ParticleUpdate.hlsl
// Compute shader: updates all particles each frame.
// Dispatched with (maxParticles / 64, 1, 1) groups.

struct Particle
{
    float3 position;
    float3 velocity;
    float4 color;
    float  size;
    float  age;
    float  lifetime;
    float  rotation;
    uint   alive;
    uint   _pad0;
    uint   _pad1;
    uint   _pad2;
};

cbuffer CBParticleUpdate : register(b0)
{
    float g_fDeltaTime;
    float g_fGravity;
    float g_fDrag;
    float _padding;
};

RWStructuredBuffer<Particle> g_Particles : register(u0);
RWStructuredBuffer<uint>     g_AliveCount : register(u1);

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    Particle p = g_Particles[idx];

    if (p.alive == 0)
        return;

    // Age
    p.age += g_fDeltaTime;
    if (p.age >= p.lifetime)
    {
        p.alive = 0;
        g_Particles[idx] = p;
        return;
    }

    // Gravity
    p.velocity.y += g_fGravity * g_fDeltaTime;

    // Drag
    float dragFactor = max(0.0, 1.0 - g_fDrag * g_fDeltaTime);
    p.velocity *= dragFactor;

    // Integration
    p.position += p.velocity * g_fDeltaTime;

    // Rotation
    p.rotation += 1.0 * g_fDeltaTime; // simple spin

    // Write back
    g_Particles[idx] = p;

    // Increment alive counter
    uint dummy;
    InterlockedAdd(g_AliveCount[0], 1, dummy);
}
```
