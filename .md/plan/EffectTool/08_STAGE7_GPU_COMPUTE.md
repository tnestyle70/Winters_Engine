# Stage 7 — GPU Compute 백엔드 (HLSL 코드 생성 + Indirect Draw)

## 목표

동일 `FxGraph` 를 **HLSL Compute Shader** 로 번역해 GPU 에서 시뮬레이션.
100,000+ 파티클을 실시간에 돌리는 경지까지 확장. Indirect Draw 로 CPU 라운드트립 제거.

## 왜 자체 코드 생성인가

- **상용 엔진 의존 없이 "그래프 → GPU" 파이프라인 경험**: 포트폴리오 핵심 가치
- **Stage 4 Expression VM 과 IR 공유** — 이미 바이트코드가 있으므로 HLSL 번역 비용이 저렴
- **엘든링 모작의 대규모 마법진** / **메테오 샤워** / **보스 광탄 탄막** 이 CPU 로 불가능

## 설계 원칙

| 원칙 | 이유 |
|---|---|
| CPU ↔ GPU 동일 결과 | 결정적 FX 유지. 같은 seed → 같은 수열 |
| Per-particle thread | N 파티클 = N 스레드 dispatch. `[numthreads(64, 1, 1)]` |
| Append / Consume buffer 피함 | UAV counter 동기화 비용 큼 → `alive count` 를 별도 atomic 카운터로 |
| Indirect Draw | 렌더 draw 인수를 CPU 로 읽지 않고 GPU 버퍼에서 읽어 CPU 스톨 제거 |
| 한 이미터 = 한 Compute shader | 노드 구성별 permutation → 시그니처 캐싱 |

## 전체 데이터 흐름

```
┌─────────────────────────────────────────────┐
│        Per-Attribute RWStructuredBuffer     │
│   ─ g_Position  (RWStructuredBuffer<float3>)│
│   ─ g_Velocity  (RWStructuredBuffer<float3>)│
│   ─ g_Color     (RWStructuredBuffer<float4>)│
│   ─ g_Size      (RWStructuredBuffer<float>) │
│   ─ g_Age       (RWStructuredBuffer<float>) │
│   ─ g_Lifetime  (RWStructuredBuffer<float>) │
│                                             │
│        Alive / FreeList 관리                │
│   ─ g_AliveMask (RWStructuredBuffer<uint>)  │
│   ─ g_AliveCount (RWByteAddressBuffer)      │
└─────────────────────────────────────────────┘
           ▲          │
           │          ▼
┌─────Compute Dispatch─────┐
│ 1. SpawnCS               │  ← SpawnBurst / Rate
│ 2. InitCS                │  ← Init* 노드 묶음
│ 3. UpdateCS              │  ← Gravity, Drag, Curl, Age, Kill
│ 4. CompactCS (옵션)      │  ← swap-back 대신 prefix sum 기반 압축
└──────────────────────────┘
           │
           ▼
┌─ Render: DrawIndexedInstancedIndirect ─┐
│  인스턴스 수를 GPU 카운터에서 읽음       │
└─────────────────────────────────────────┘
```

## 공유 HLSL 인클루드

```hlsl
// Shaders/FX/FxCommon.hlsli
#ifndef FX_COMMON_HLSLI
#define FX_COMMON_HLSLI

// ── Attribute Buffers ─────────────────────────────
RWStructuredBuffer<float3> g_Position  : register(u0);
RWStructuredBuffer<float3> g_Velocity  : register(u1);
RWStructuredBuffer<float4> g_Color     : register(u2);
RWStructuredBuffer<float>  g_Size      : register(u3);
RWStructuredBuffer<float>  g_Age       : register(u4);
RWStructuredBuffer<float>  g_Lifetime  : register(u5);
RWStructuredBuffer<uint>   g_AliveMask : register(u6);    // 0 or 1
RWByteAddressBuffer        g_AliveCount : register(u7);    // atomic counter

// ── Constants ─────────────────────────────────────
cbuffer CBFxFrame : register(b0)
{
    float   g_DeltaTime;
    float   g_EmitterAge;
    float   g_Time;
    uint    g_RngSeed;
    uint    g_Capacity;
    uint    g_SpawnBegin;
    uint    g_SpawnEnd;
    uint    _pad;
};

// ── PCG Hash (결정적 난수) ─────────────────────────
uint PCGHash(uint state)
{
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float RandFloat01(inout uint state)
{
    state = PCGHash(state);
    return (state & 0x00FFFFFFu) / float(0x01000000);
}

float RandRange(inout uint state, float a, float b)
{
    return a + (b - a) * RandFloat01(state);
}

float3 RandUnitSphere(inout uint state)
{
    float z   = RandRange(state, -1.0, 1.0);
    float r   = sqrt(max(0.0, 1.0 - z * z));
    float phi = RandRange(state, 0.0, 6.283185307);
    return float3(r * cos(phi), r * sin(phi), z);
}

#endif
```

## 생성된 Compute Shader — 예시

불꽃 이펙트 Update 스테이지를 코드 생성하면:

```hlsl
// [generated] Shaders/FX/Generated/Fire_Burst_Small_Update.hlsl
#include "FxCommon.hlsli"

[numthreads(64, 1, 1)]
void CSUpdate(uint3 gid : SV_DispatchThreadID)
{
    uint i = gid.x;
    if (i >= g_Capacity) return;
    if (g_AliveMask[i] == 0) return;

    // ── UpdateGravity ──
    float3 gravity = float3(0.0, -2.0, 0.0);
    g_Velocity[i] += gravity * g_DeltaTime;

    // ── UpdateDrag ──
    float drag = 0.5;
    float factor = exp(-drag * g_DeltaTime);
    g_Velocity[i] *= factor;

    // ── UpdateIntegratePosition ──
    g_Position[i] += g_Velocity[i] * g_DeltaTime;

    // ── UpdateColorOverLife (keyframe 인라인) ──
    float t = (g_Lifetime[i] > 1e-6) ? (g_Age[i] / g_Lifetime[i]) : 1.0;
    float4 c0 = float4(1.0, 0.8, 0.3, 1.0);
    float4 c1 = float4(1.0, 0.3, 0.1, 0.8);
    float4 c2 = float4(0.2, 0.1, 0.1, 0.0);
    float4 col;
    if (t < 0.5) {
        float u = t / 0.5;
        col = lerp(c0, c1, u);
    } else {
        float u = (t - 0.5) / 0.5;
        col = lerp(c1, c2, u);
    }
    g_Color[i] = col;

    // ── UpdateAgeAndKill ──
    g_Age[i] += g_DeltaTime;
    if (g_Age[i] >= g_Lifetime[i]) {
        g_AliveMask[i] = 0;
        uint prev;
        g_AliveCount.InterlockedAdd(0, (uint)-1, prev);   // atomic decrement
    }
}
```

## HLSL 코드 생성기

```cpp
// Engine/Public/FX/GPU/FxComputeCodeGen.h
#pragma once
#include "FxGraph.h"

namespace Engine::FX {

class CFxComputeCodeGen
{
public:
    struct Output
    {
        std::string hlslSource;            // CSSpawn / CSInit / CSUpdate 병합
        std::uint32_t threadGroupSize;     // 기본 64
        std::vector<std::string> uavNames; // 바인딩 순서 기록
    };

    static bool Generate(const CFxGraph& graph, Output& out);

private:
    static std::string EmitSpawnStage (const CFxGraph& g);
    static std::string EmitInitStage  (const CFxGraph& g);
    static std::string EmitUpdateStage(const CFxGraph& g);

    static std::string EmitNode(const Node& n);
    static std::string EmitExpr(const CFxGraph& g, NodeId exprRoot);  // Stage 4 IR 재활용
};

} // namespace Engine::FX
```

### EmitNode 예

```cpp
// Engine/Private/FX/GPU/FxComputeCodeGen.cpp 발췌
std::string CFxComputeCodeGen::EmitNode(const Node& n)
{
    std::string s;
    switch (n.kind) {
    case eNodeKind::UpdateGravity: {
        Vec3 g = std::get<Vec3>(n.params.at("gravity"));
        char buf[256];
        std::snprintf(buf, 256,
            "    g_Velocity[i] += float3(%.6f, %.6f, %.6f) * g_DeltaTime;\n",
            g.x, g.y, g.z);
        s = buf;
    } break;
    case eNodeKind::UpdateDrag: {
        f32_t k = std::get<f32_t>(n.params.at("drag"));
        char buf[256];
        std::snprintf(buf, 256,
            "    g_Velocity[i] *= exp(-%.6f * g_DeltaTime);\n", k);
        s = buf;
    } break;
    case eNodeKind::UpdateIntegratePosition:
        s = "    g_Position[i] += g_Velocity[i] * g_DeltaTime;\n";
        break;
    case eNodeKind::UpdateAgeAndKill:
        s = "    g_Age[i] += g_DeltaTime;\n"
            "    if (g_Age[i] >= g_Lifetime[i]) {\n"
            "        g_AliveMask[i] = 0;\n"
            "        uint _prev;\n"
            "        g_AliveCount.InterlockedAdd(0, (uint)-1, _prev);\n"
            "    }\n";
        break;
    // ... 기타 노드 ...
    }
    return s;
}
```

### Expression 노드 HLSL 변환

Stage 4 의 바이트코드를 그대로 HLSL 로 번역 (스택 시뮬):

```cpp
std::string CFxComputeCodeGen::EmitExpr(const CFxGraph& g, NodeId rootWrite)
{
    // 1) Stage 4 컴파일러로 바이트코드 얻기
    std::vector<Instr> code;
    CFxSlotTable slots;
    CFxExprCompiler().Compile(g, rootWrite, code, slots);

    // 2) 스택을 HLSL 지역변수 체인으로 풀어씀
    std::vector<std::string> stk;
    std::string out;
    auto tmp = [&](int idx) { return "t" + std::to_string(idx); };
    int tmpId = 0;

    for (const auto& ins : code) {
        switch (ins.op) {
        case eOp::PushConst: {
            std::string n = tmp(tmpId++);
            out += "    float " + n + " = " + std::to_string(ins.imm) + ";\n";
            stk.push_back(n);
        } break;
        case eOp::LoadAttr: {
            const auto& s = slots.Get(ins.slot);
            std::string swiz;
            if (s.component == 0) swiz = ".x";
            else if (s.component == 1) swiz = ".y";
            else if (s.component == 2) swiz = ".z";
            else if (s.component == 3) swiz = ".w";
            std::string bufName = "g_" + s.attrName;
            std::string n = tmp(tmpId++);
            out += "    float " + n + " = " + bufName + "[i]" + swiz + ";\n";
            stk.push_back(n);
        } break;
        case eOp::Mul: {
            std::string b = stk.back(); stk.pop_back();
            std::string a = stk.back(); stk.pop_back();
            std::string n = tmp(tmpId++);
            out += "    float " + n + " = " + a + " * " + b + ";\n";
            stk.push_back(n);
        } break;
        case eOp::Sin: {
            std::string x = stk.back(); stk.pop_back();
            std::string n = tmp(tmpId++);
            out += "    float " + n + " = sin(" + x + ");\n";
            stk.push_back(n);
        } break;
        case eOp::StoreAttr: {
            const auto& s = slots.Get(ins.slot);
            std::string swiz = s.component == 0 ? ".x"
                             : s.component == 1 ? ".y"
                             : s.component == 2 ? ".z" : ".w";
            std::string x = stk.back(); stk.pop_back();
            out += "    g_" + s.attrName + "[i]" + swiz + " = " + x + ";\n";
        } break;
        // ... 나머지 op ...
        default: break;
        }
    }
    return out;
}
```

즉 CPU VM 과 GPU 셰이더가 **완전히 같은 바이트코드에서 파생**.

## FxComputeEmitter — 런타임

```cpp
// Engine/Public/FX/GPU/FxComputeEmitter.h
#pragma once
#include "FxGraph.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Engine::FX {

class CFxComputeEmitter
{
public:
    static std::unique_ptr<CFxComputeEmitter> Create(
        ID3D11Device* device,
        const CFxGraph& graph,
        std::uint32_t capacity,
        std::uint32_t rngSeed);

    void Tick(ID3D11DeviceContext* ctx, f32_t dt, f32_t emitterAge);

    // Indirect Draw 용. DrawInstancedIndirect arguments buffer.
    ID3D11Buffer* GetIndirectArgsBuffer() const { return m_indirectArgs.Get(); }

    // 렌더 시 바인딩할 SRV 들 (StructuredBuffer 의 SRV)
    const std::vector<ID3D11ShaderResourceView*>& GetAttributeSRVs() const { return m_attrSRVs; }

private:
    CFxComputeEmitter() = default;

    // UAV 리소스
    struct Attr {
        std::string name;
        AttrType    type;
        Microsoft::WRL::ComPtr<ID3D11Buffer>              buffer;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  srv;
    };
    std::vector<Attr> m_attrs;

    // AliveMask + AliveCount
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_aliveMask;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_aliveMaskUAV;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_aliveCount;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_aliveCountUAV;

    // Indirect args: {IndexCountPerInstance, InstanceCount, ...}
    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_indirectArgs;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_indirectArgsUAV;

    // Compute Shaders (Spawn / Init / Update / IndirectPrepare)
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_csSpawn;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_csInit;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_csUpdate;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_csIndirectPrepare;

    // CBFxFrame
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbFrame;

    // 메타
    std::uint32_t m_capacity = 0;
    std::uint32_t m_rngSeed  = 0;
    std::vector<ID3D11ShaderResourceView*> m_attrSRVs;   // 렌더 시 캐싱
};

} // namespace Engine::FX
```

### 구현 요점

```cpp
// Engine/Private/FX/GPU/FxComputeEmitter.cpp 발췌
std::unique_ptr<CFxComputeEmitter>
CFxComputeEmitter::Create(ID3D11Device* device, const CFxGraph& graph,
                          std::uint32_t capacity, std::uint32_t seed)
{
    auto e = std::unique_ptr<CFxComputeEmitter>(new CFxComputeEmitter());
    e->m_capacity = capacity;
    e->m_rngSeed  = seed;

    // 1) 기본 attribute 버퍼 생성 (UAV + SRV)
    auto CreateAttr = [&](const std::string& name, AttrType t, std::uint32_t bytesPerElem) {
        Attr a; a.name = name; a.type = t;
        D3D11_BUFFER_DESC bd{};
        bd.Usage          = D3D11_USAGE_DEFAULT;
        bd.BindFlags      = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        bd.ByteWidth      = capacity * bytesPerElem;
        bd.MiscFlags      = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = bytesPerElem;
        device->CreateBuffer(&bd, nullptr, &a.buffer);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.Format             = DXGI_FORMAT_UNKNOWN;
        uavd.ViewDimension      = D3D11_UAV_DIMENSION_BUFFER;
        uavd.Buffer.FirstElement = 0;
        uavd.Buffer.NumElements = capacity;
        device->CreateUnorderedAccessView(a.buffer.Get(), &uavd, &a.uav);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format             = DXGI_FORMAT_UNKNOWN;
        srvd.ViewDimension      = D3D11_SRV_DIMENSION_BUFFEREX;
        srvd.BufferEx.FirstElement = 0;
        srvd.BufferEx.NumElements  = capacity;
        device->CreateShaderResourceView(a.buffer.Get(), &srvd, &a.srv);

        e->m_attrs.push_back(std::move(a));
    };

    CreateAttr("Position", AttrType::Float3, 12);
    CreateAttr("Velocity", AttrType::Float3, 12);
    CreateAttr("Color",    AttrType::Float4, 16);
    CreateAttr("Size",     AttrType::Float,  4);
    CreateAttr("Age",      AttrType::Float,  4);
    CreateAttr("Lifetime", AttrType::Float,  4);

    // 2) AliveMask
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_UNORDERED_ACCESS;
        bd.ByteWidth  = capacity * sizeof(std::uint32_t);
        bd.MiscFlags  = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = sizeof(std::uint32_t);
        device->CreateBuffer(&bd, nullptr, &e->m_aliveMask);
        // UAV ...
    }

    // 3) AliveCount (ByteAddressBuffer, 4 bytes)
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_UNORDERED_ACCESS;
        bd.ByteWidth  = 16;   // atomic align
        bd.MiscFlags  = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        device->CreateBuffer(&bd, nullptr, &e->m_aliveCount);
        // UAV (DXGI_FORMAT_R32_TYPELESS, D3D11_BUFFER_UAV_FLAG_RAW) ...
    }

    // 4) Indirect args (20 bytes)
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_UNORDERED_ACCESS;
        bd.ByteWidth  = 20;   // {IndexCountPerInstance, InstanceCount, StartIndex, BaseVertex, StartInstance}
        bd.MiscFlags  = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
                      | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        device->CreateBuffer(&bd, nullptr, &e->m_indirectArgs);
        // UAV raw ...
    }

    // 5) HLSL 코드 생성 → 컴파일 → ComputeShader 3~4 개
    CFxComputeCodeGen::Output gen;
    if (!CFxComputeCodeGen::Generate(graph, gen)) return nullptr;

    // D3DCompile(gen.hlslSource, "CSSpawn", "cs_5_0", ..., &e->m_csSpawn);
    // D3DCompile(gen.hlslSource, "CSInit",  "cs_5_0", ..., &e->m_csInit);
    // D3DCompile(gen.hlslSource, "CSUpdate","cs_5_0", ..., &e->m_csUpdate);
    // (CS 함수들은 같은 파일 안에 엔트리포인트 다름)

    // 6) CBFxFrame
    // ...

    e->m_attrSRVs.reserve(e->m_attrs.size());
    for (auto& a : e->m_attrs) e->m_attrSRVs.push_back(a.srv.Get());
    return e;
}

void CFxComputeEmitter::Tick(ID3D11DeviceContext* ctx, f32_t dt, f32_t emitterAge)
{
    // 1) CBFxFrame 갱신
    D3D11_MAPPED_SUBRESOURCE m{};
    ctx->Map(m_cbFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    // struct CB {...} 에 dt, emitterAge, seed, capacity, spawnBegin, spawnEnd 주입
    ctx->Unmap(m_cbFrame.Get(), 0);

    // 2) UAV 바인딩
    std::vector<ID3D11UnorderedAccessView*> uavs;
    for (auto& a : m_attrs) uavs.push_back(a.uav.Get());
    uavs.push_back(m_aliveMaskUAV.Get());
    uavs.push_back(m_aliveCountUAV.Get());
    ctx->CSSetUnorderedAccessViews(0, (UINT)uavs.size(), uavs.data(), nullptr);

    ctx->CSSetConstantBuffers(0, 1, m_cbFrame.GetAddressOf());

    const UINT groups = (m_capacity + 63) / 64;

    // 3) Dispatch 순서
    ctx->CSSetShader(m_csSpawn.Get(), nullptr, 0);
    ctx->Dispatch(1, 1, 1);   // Spawn 은 대부분 저 스레드 수

    ctx->CSSetShader(m_csInit.Get(), nullptr, 0);
    ctx->Dispatch(groups, 1, 1);

    ctx->CSSetShader(m_csUpdate.Get(), nullptr, 0);
    ctx->Dispatch(groups, 1, 1);

    ctx->CSSetShader(m_csIndirectPrepare.Get(), nullptr, 0);
    ctx->Dispatch(1, 1, 1);   // indirect args 에 aliveCount 복사

    // 4) UAV 해제
    ID3D11UnorderedAccessView* nullUAVs[16]{};
    ctx->CSSetUnorderedAccessViews(0, (UINT)uavs.size(), nullUAVs, nullptr);
}
```

## IndirectPrepareCS — GPU 에서 DrawInstanced 인수 구성

```hlsl
// Shaders/FX/IndirectPrepareCS.hlsl
#include "FxCommon.hlsli"

RWByteAddressBuffer g_IndirectArgs : register(u8);

[numthreads(1, 1, 1)]
void CSIndirectPrepare(uint3 tid : SV_DispatchThreadID)
{
    // DrawIndexedInstancedIndirect args 포맷:
    // [0]  IndexCountPerInstance  = 6 (quad)
    // [4]  InstanceCount          = aliveCount
    // [8]  StartIndexLocation     = 0
    // [12] BaseVertexLocation     = 0
    // [16] StartInstanceLocation  = 0

    uint aliveCount = g_AliveCount.Load(0);
    g_IndirectArgs.Store(0,  6);
    g_IndirectArgs.Store(4,  aliveCount);
    g_IndirectArgs.Store(8,  0);
    g_IndirectArgs.Store(12, 0);
    g_IndirectArgs.Store(16, 0);
}
```

## 렌더러 확장 — DrawInstancedIndirect

```cpp
// FxBillboardRenderer 에 GPU 경로 추가
void CFxBillboardRenderer::RenderGpu(
    ID3D11DeviceContext* ctx,
    const CFxComputeEmitter& emitter,
    const Mat4& viewProj,
    const Vec3& camRight, const Vec3& camUp, const Vec3& camPos,
    ID3D11ShaderResourceView* atlasSRV,
    eFxBlendMode blendMode)
{
    // CBPerFrame 업데이트 (동일)

    // VS / PS 는 GPU 경로 전용 셰이더 사용 — 인스턴스 VB 대신 SRV 읽기
    // (Stage 5 의 CPU 경로 VS 를 확장: "SV_InstanceID" 로 g_Position[iid] 직접 샘플)

    // Indirect Draw
    ID3D11Buffer* args = emitter.GetIndirectArgsBuffer();
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // IA 는 쿼드 VB/IB 만 바인딩. 인스턴스 VB 없음 (SRV 로 대체)

    // VS 에 Attribute SRV 바인딩
    const auto& srvs = emitter.GetAttributeSRVs();
    ctx->VSSetShaderResources(0, (UINT)srvs.size(), srvs.data());

    // 블렌드 / DSV 동일

    ctx->DrawIndexedInstancedIndirect(args, 0);
}
```

### GPU 경로 VS

```hlsl
// Shaders/FX/FxBillboardGpu.hlsl (VS)
StructuredBuffer<float3> g_Position : register(t0);
StructuredBuffer<float4> g_Color    : register(t1);
StructuredBuffer<float>  g_Size     : register(t2);

// (CBPerFrame 은 Stage 5 와 동일)

struct VSIn
{
    float2 vQuadPos  : POSITION0;
    float2 vQuadUV   : TEXCOORD0;
    uint   instID    : SV_InstanceID;
};

VSOut VSMain(VSIn v)
{
    VSOut o;
    float3 wp = g_Position[v.instID];
    float  sz = g_Size    [v.instID];
    float4 c  = g_Color   [v.instID];

    float3 world = wp + g_camRight * v.vQuadPos.x * sz
                     + g_camUp    * v.vQuadPos.y * sz;
    o.pos = mul(float4(world, 1.0), g_viewProj);
    o.uv  = v.vQuadUV;
    o.color = c;
    return o;
}
```

## 난수 — CPU ↔ GPU 결정성

| 상황 | 권장 |
|---|---|
| 결정적 FX (판정) | 서버가 seed 내려줌 → GPU 도 같은 seed 로 시작 |
| PCG vs xorshift | **PCG** 추천 (GPU 에서 품질 + 성능 균형 우수) |
| 파티클 seed | `seed = hash(emitterSeed, spawnIndex)` — 파티클마다 독립 |

```hlsl
uint PerParticleSeed(uint emitterSeed, uint i)
{
    return PCGHash(emitterSeed + i * 0x9E3779B9u);
}
```

## 컬링 / LOD

- 거리 기반 컬링: 카메라 > X 미터면 dispatch 건너뜀
- LOD: 거리에 따라 capacity 를 1/2, 1/4 로 줄임
- 프러스텀 컬링: CPU 가 바운딩 볼륨 비교 후 Dispatch 호출 여부 결정 (개별 파티클 컬은 GPU)

## 메모리 — 100K 파티클 기준

```
Position  : 100K × 12 = 1.2 MB
Velocity  : 100K × 12 = 1.2 MB
Color     : 100K × 16 = 1.6 MB
Size      : 100K ×  4 = 0.4 MB
Age       : 100K ×  4 = 0.4 MB
Lifetime  : 100K ×  4 = 0.4 MB
AliveMask : 100K ×  4 = 0.4 MB
                           총 5.6 MB / 이미터
```

이미터 10 개 동시 = 56 MB VRAM. 용인 가능.

## 성능 목표 (RTX 3060 기준)

| 파티클 수 | Sim CS | Render | 합계 |
|---|---|---|---|
| 10K | 0.05 ms | 0.1 ms | 0.2 ms |
| 100K | 0.3 ms | 0.5 ms | 1.0 ms |
| 1M | 2 ms | 3 ms | 6 ms |

1M = 엘든링 보스 마법진 / 대격변 광경. MOBA 에선 100K 가 상한 (바론 죽을 때 폭발).

## Gotchas

- **D3DCompile 런타임 의존**: 코드 생성 후 `D3DCompile` 호출 → 셰이더 바이너리 생성. 릴리스에서는 미리 컴파일한 `.cso` 를 써야 하지만, 그래프마다 permutation 이 달라 사전 컴파일 어려움 → **최초 로드 시 컴파일 + 디스크 캐시** 패턴
- **Structured Buffer stride 불일치**: C++ 측 `sizeof(Vec3) = 12`, HLSL `float3` = 16 으로 **stride 다름** → StructuredBuffer 로는 `float3` 대신 `float` × 3 개별 배열 권장 OR `float4` 로 승격. **Float3 는 GPU 에서 Float4 로 align**
- **UAV slot 수 제한**: DX11 은 CS 에서 UAV 8 개까지. 기본 attribute 6 + AliveMask + AliveCount = 8 → 한도 초과 시 일부를 StructuredBuffer 에서 Texture2D 로 변경 or DX11.1 16 슬롯 사용
- **AliveCount atomic**: `InterlockedAdd` 는 DXBC 에서 `atomic_imax_u` 등으로 번역. UAV 가 `D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS` 여야 Raw UAV 가능
- **Indirect Args 버퍼**: `D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS` 플래그 필수. 누락 시 DrawInstancedIndirect 가 실패
- **VS SRV 바인딩 슬롯 오프셋**: PS 가 atlas 를 t0 사용 중. VS attribute SRV 를 t0~t2 에 두면 충돌 → PS SRV 는 t8 로 이동 or namespace 분리
- **이미터 파괴 시 리소스 해제**: ComPtr 이 자동 해제하나 UAV 는 디바이스 컨텍스트에 여전히 바인딩되어 있으면 GPU 가 사용 중. `CSSetUnorderedAccessViews(...null...)` 으로 언바인드 후 파괴
- **Per-permutation 셰이더 캐시**: 그래프 hash → `ComputeShader` 맵. 같은 그래프 중복 컴파일 방지
- **float 결정성**: `[fastmath]` 같은 HLSL 프래그마 켜지 말 것. `-Od /Gis` 컴파일 옵션 고려 (결정적 FX 한정)
- **Xbox/플레이스테이션 이식성**: 본 계획은 DX11 전용. Stage 2 확장에서 DX12 / Vulkan 으로 가면 UAV 가 DescriptorHeap 으로 바뀜

## 단위 테스트

- **CPU vs GPU 결과 일치**: 동일 seed + 고정 dt 로 100 frame 시뮬 후 Position/Color 버퍼를 CPU 로 readback → Vec3 차이가 1e-3 미만
- **aliveCount 일관성**: Spawn 100 + Kill 50 후 aliveCount = 50
- **IndirectArgs 갱신**: 매 프레임 Args buffer readback 으로 InstanceCount == GPU alive 일치 확인
- **1M 파티클 stress test**: 렌더 fps > 60 유지

## 구현 순서

1. `FxCommon.hlsli` 공통 유틸
2. `FxComputeCodeGen::Generate` 최소 — Update 스테이지만 (노드 3~4 종)
3. Structured buffer + UAV 생성 (Attribute 6 + AliveMask + AliveCount)
4. D3DCompile 런타임 셰이더 컴파일
5. `FxComputeEmitter::Tick` — 1 개 CS dispatch 만
6. CPU readback 으로 결과 확인 (위치가 실제로 갱신되는지)
7. Spawn / Init 스테이지 추가
8. Indirect args + `IndirectPrepareCS`
9. GPU 경로 VS (`FxBillboardGpu.hlsl`) + `RenderGpu`
10. CPU vs GPU 비교 테스트
11. Expression 노드 HLSL 번역 (Stage 4 IR 재사용)
12. 1M 파티클 스트레스 + 프로파일링
13. 셰이더 바이너리 디스크 캐시 (`$(OutDir)FxShaderCache/`)

## 다음 문서

[09_INTEGRATION.md](09_INTEGRATION.md) — 스킬 시스템 / 사운드 / 네트워크 통합.
여기까지 왔으면 엔진 내부 구현은 완료. 남은 건 게임 시스템과의 접점.
