# GPU Driven Pipeline - Winters Engine 종합 구현 계획서

## 1. 핵심 이론과 필요성

### 1.1 GPU Driven Rendering이란

전통적 렌더링에서는 CPU가 모든 드로우콜을 발행한다. 100개의 오브젝트가 있으면 CPU에서 100번의 `DrawIndexed()` 호출이 일어나고, 각 호출마다 상태 변경(셰이더 교체, 상수 버퍼 업데이트, 버텍스/인덱스 버퍼 바인딩)이 발생한다. 이것이 CPU 병목의 핵심이다.

GPU Driven Rendering은 이 의사결정을 GPU 컴퓨트 셰이더로 옮긴다:
- **가시성 판정(Culling)**: 프러스텀 컬링, 오클루전 컬링을 GPU 컴퓨트 셰이더에서 수행
- **LOD 선택**: 화면 크기 기반 LOD를 GPU에서 계산
- **Indirect Arguments 생성**: 가시 인스턴스만 담은 드로우 인자를 GPU에서 생성
- **Indirect Draw**: `DrawIndexedInstancedIndirect()`로 GPU가 생성한 인자로 드로우

### 1.2 PvP 게임에서의 필요성

Winters Engine의 타겟 게임 시나리오별 부하:
- **MOBA (LoL)**: 10 챔피언 + 수백 미니언 + 타워 + 스킬 이펙트 = ~500+ 오브젝트
- **서바이벌 (이터널 리턴)**: 18 플레이어 + 몬스터 + 아이템 + 맵 오브젝트 = ~1000+ 오브젝트
- **배틀로얄 (PUBG)**: 100 플레이어 + 차량 + 건물 + 파괴물 + 초목 = ~10,000+ 오브젝트

CPU에서 매 프레임 10,000개의 오브젝트를 순회하며 컬링/LOD/드로우콜을 발행하면 5~15ms 소모. GPU Driven으로 전환하면 CPU 비용은 버퍼 업데이트 + 소수의 Dispatch/DrawIndirect 호출로 줄어들어 ~1ms 이하로 떨어진다.

### 1.3 핵심 참고 자료
- "GPU-Driven Rendering Pipelines" (Sebastian Aaltonen, SIGGRAPH 2015) - 프러스텀/오클루전 컬링의 GPU 이관
- Assassin's Creed Unity (Wihlidal, GDC 2015) - 메가버퍼 + Indirect Draw 패턴
- "Rendering of Call of Duty: Infinite Warfare" (GDC 2017) - DX11에서의 GPU Driven 기법

### 1.4 DX11에서의 실현 방법

DX11은 DX12의 `ExecuteIndirect`(멀티 드로우)를 지원하지 않지만, 다음으로 충분히 구현 가능하다:
- `ID3D11DeviceContext::DrawIndexedInstancedIndirect()` - 단일 메시 그룹당 1회 호출
- `ID3D11DeviceContext::Dispatch()` - 컴퓨트 셰이더 실행
- `StructuredBuffer` / `RWStructuredBuffer` - GPU 측 데이터 배열
- `AppendStructuredBuffer` - 가변 길이 출력
- `D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS` - Indirect 인자 버퍼

핵심 제약: DX11에서는 메시 그룹(같은 VB/IB + 같은 셰이더)별로 DrawIndexedInstancedIndirect를 1회씩 호출해야 한다. 따라서 **메가버퍼(모든 메시를 하나의 VB/IB에 통합)** + **머티리얼 인덱싱** 전략이 필수다.

---

## 2. 아키텍처 개요

### 2.1 데이터 플로우 (프레임당)

```
┌─────────── CPU SIDE ───────────┐     ┌─────────── GPU SIDE ────────────┐
│                                │     │                                  │
│ ECS TransformComponent         │     │ [1] Compute: Frustum Cull       │
│   ↓ dirty flag check           │     │     Input: InstanceBuffer,      │
│ Update GPU InstanceBuffer      │──→  │            FrustumPlanes        │
│ (Map/WRITE_DISCARD)            │     │     Output: VisibilityBits      │
│                                │     │                                  │
│ Upload FrustumPlanes(cbuffer)  │──→  │ [2] Compute: Hi-Z Generate      │
│                                │     │     Input: Prev-frame Depth      │
│                                │     │     Output: Hi-Z Mipchain       │
│                                │     │                                  │
│                                │     │ [3] Compute: Occlusion Cull     │
│                                │     │     Input: VisibilityBits,      │
│                                │     │            Hi-Z Texture          │
│                                │     │     Output: FinalVisibility     │
│                                │     │                                  │
│                                │     │ [4] Compute: LOD Select         │
│                                │     │     Input: FinalVisibility,     │
│                                │     │            ScreenSize           │
│                                │     │     Output: LODLevels           │
│                                │     │                                  │
│                                │     │ [5] Compute: Compact + Args     │
│                                │     │     Input: FinalVisibility,     │
│                                │     │            LODLevels,           │
│                                │     │            MeshDescriptors      │
│ DrawIndexedInstancedIndirect() │←──  │     Output: IndirectArgs,       │
│   (per mesh group)             │     │             CompactInstances    │
│                                │     │                                  │
│                                │     │ [6] VS/PS: GPU Driven Draw      │
│                                │     │     VS reads from               │
│                                │     │       CompactInstances (SRV)    │
│                                │     │     PS reads material by index  │
└────────────────────────────────┘     └──────────────────────────────────┘
```

### 2.2 기존 시스템과의 통합

```
CDX11Device (00. Manager)
  ├── 기존: GetDevice(), GetContext(), BeginFrame(), EndFrame()
  └── 변경 없음 — 새 RHI 클래스들이 Device/Context를 받아서 사용

DX11Shader (00. Manager)
  ├── 기존: VS + PS 컴파일/바인딩
  └── 확장: DX11ComputeShader 별도 클래스 추가 (cs_5_0)

DX11Buffer (00. Manager)
  ├── 기존: Vertex/Index IMMUTABLE 버퍼
  └── 확장: DX11StructuredBuffer, DX11IndirectArgsBuffer 추가

ECS (05. ECS)
  ├── 기존: CWorld, CComponentStore<TransformComponent>
  └── 연동: RenderComponent 추가 (meshId, materialId, AABB)
            CGPURenderSystem이 dirty 인스턴스를 GPU에 업로드

Render Graph (03. Renderer) [가정: 이미 구현됨]
  └── 연동: GPU Driven 패스들을 Render Graph에 등록
            FrustumCullPass → HiZPass → OcclusionCullPass →
            LODSelectPass → CompactPass → IndirectDrawPass
```

---

## 3. 파일 구조 전체 목록

```
Engine/
  Header/
    RHI/DX11/
      DX11ComputeShader.h          ← cs_5_0 컴파일/디스패치
      DX11StructuredBuffer.h       ← StructuredBuffer + SRV/UAV
      DX11IndirectArgsBuffer.h     ← DrawIndexedInstancedIndirect 인자 버퍼
      DX11AppendBuffer.h           ← AppendStructuredBuffer 래퍼
    Renderer/
      GPUDriven/
        CGPUScene.h                ← GPU 측 씬 데이터 관리
        CGPUCuller.h               ← 프러스텀 컬링 디스패치
        CHiZBuffer.h               ← Hi-Z 밉체인 생성
        CGPUOcclusionCuller.h      ← Hi-Z 오클루전 컬링
        CGPULODSelector.h          ← LOD 선택 컴퓨트
        CIndirectRenderer.h        ← Indirect Draw 발행
        GPUDrivenTypes.h           ← 공용 구조체 (GPU/CPU 공유)
    ECS/Components/
      RenderComponent.h            ← meshId, materialId, AABB
  Code/
    RHI/DX11/
      DX11ComputeShader.cpp
      DX11StructuredBuffer.cpp
      DX11IndirectArgsBuffer.cpp
      DX11AppendBuffer.cpp
    Renderer/
      GPUDriven/
        CGPUScene.cpp
        CGPUCuller.cpp
        CHiZBuffer.cpp
        CGPUOcclusionCuller.cpp
        CGPULODSelector.cpp
        CIndirectRenderer.cpp

Shaders/
  GPUDriven/
    Common.hlsli                   ← 공유 구조체/유틸리티
    FrustumCull.hlsl               ← 프러스텀 컬링 CS
    HiZGenerate.hlsl               ← Hi-Z 밉맵 생성 CS
    OcclusionCull.hlsl             ← 오클루전 컬링 CS
    LODSelect.hlsl                 ← LOD 선택 CS
    CompactAndArgs.hlsl            ← 가시 인스턴스 압축 + Indirect Args 생성 CS
    GPUDrivenVS.hlsl               ← StructuredBuffer에서 인스턴스 읽는 VS
    GPUDrivenPS.hlsl               ← 머티리얼 인덱싱 PS
```

---

## 4. 상세 파일 명세

### 4.1 RHI 확장 (00. Manager 필터)

#### 4.1.1 `Engine/Header/RHI/DX11/DX11ComputeShader.h`

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  DX11ComputeShader  |  cs_5_0 컴퓨트 셰이더 컴파일 및 디스패치
//
//  GPU Driven Pipeline의 핵심.
//  프러스텀 컬링, Hi-Z 생성, 오클루전 컬링, LOD 선택, Args 생성에 사용.
//
//  사용 흐름:
//    DX11ComputeShader cs;
//    cs.Load(device, L"Shaders/GPUDriven/FrustumCull.hlsl", "CSMain");
//    cs.Bind(context);
//    cs.Dispatch(context, groupX, groupY, groupZ);
//    cs.Unbind(context);
// ─────────────────────────────────────────────────────────────────

class DX11ComputeShader
{
public:
    DX11ComputeShader()  = default;
    ~DX11ComputeShader() { Release(); }

    // 복사 금지, 이동 허용
    DX11ComputeShader(const DX11ComputeShader&) = delete;
    DX11ComputeShader& operator=(const DX11ComputeShader&) = delete;
    DX11ComputeShader(DX11ComputeShader&& other) noexcept;
    DX11ComputeShader& operator=(DX11ComputeShader&& other) noexcept;

    // .hlsl에서 cs_5_0 컴파일 및 생성
    // csEntry: 컴퓨트 셰이더 진입 함수명 (기본 "CSMain")
    [[nodiscard]] bool Load(ID3D11Device* device,
                            const wchar_t* hlslPath,
                            const char* csEntry = "CSMain");

    // 컴퓨트 셰이더 바인딩
    void Bind(ID3D11DeviceContext* context) const;

    // 디스패치 — 스레드 그룹 수 지정
    void Dispatch(ID3D11DeviceContext* context,
                  uint32 groupX, uint32 groupY, uint32 groupZ) const;

    // 바인딩 해제 (UAV/SRV 충돌 방지용)
    void Unbind(ID3D11DeviceContext* context) const;

    void Release();

    ID3D11ComputeShader* GetCS() const { return m_pCS; }

private:
    static ID3DBlob* CompileCS(const wchar_t* path,
                               const char* entry);

    ID3D11ComputeShader* m_pCS = nullptr;
};
```

**구현 참고** (`Engine/Code/RHI/DX11/DX11ComputeShader.cpp`):
- `CompileCS()`: 기존 `DX11Shader::CompileShader()` 패턴 재사용, target = `"cs_5_0"`
- `Load()`: `CompileCS()` 후 `device->CreateComputeShader()`
- `Bind()`: `context->CSSetShader(m_pCS, nullptr, 0)`
- `Dispatch()`: `context->Dispatch(groupX, groupY, groupZ)`
- `Unbind()`: `context->CSSetShader(nullptr, nullptr, 0)` + 필요시 UAV/SRV null 바인딩

#### 4.1.2 `Engine/Header/RHI/DX11/DX11StructuredBuffer.h`

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <cassert>
#include <type_traits>

// ─────────────────────────────────────────────────────────────────
//  DX11StructuredBuffer<T>  |  GPU StructuredBuffer + SRV/UAV
//
//  GPU Driven Pipeline의 데이터 저장소.
//  인스턴스 데이터, 메시 디스크립터, 가시성 비트 등 저장.
//
//  모드:
//    eGPUReadOnly  : CPU→GPU 업로드 전용 (DYNAMIC, SRV only)
//    eGPUReadWrite : GPU 읽기/쓰기 (DEFAULT, SRV + UAV)
//    eStaging      : GPU→CPU 리드백 (STAGING, CPU_READ)
//
//  사용 흐름:
//    DX11StructuredBuffer<InstanceData> buf;
//    buf.Create(device, 10000, eGPUReadOnly);
//    buf.Upload(context, dataPtr, count);
//    buf.BindSRV_VS(context, 0);       // t0 in VS
//    buf.BindSRV_CS(context, 0);       // t0 in CS
//    buf.BindUAV_CS(context, 0);       // u0 in CS
// ─────────────────────────────────────────────────────────────────

enum class EStructuredBufferMode : uint8_t
{
    eGPUReadOnly,    // CPU 업로드 → GPU 읽기 (SRV)
    eGPUReadWrite,   // GPU 읽기/쓰기 (SRV + UAV)
    eStaging,        // GPU → CPU 리드백
};

template<typename T>
class DX11StructuredBuffer
{
public:
    DX11StructuredBuffer()  = default;
    ~DX11StructuredBuffer() { Release(); }

    // 복사 금지, 이동 허용
    DX11StructuredBuffer(const DX11StructuredBuffer&) = delete;
    DX11StructuredBuffer& operator=(const DX11StructuredBuffer&) = delete;
    DX11StructuredBuffer(DX11StructuredBuffer&& other) noexcept;
    DX11StructuredBuffer& operator=(DX11StructuredBuffer&& other) noexcept;

    // 버퍼 생성
    // maxElements: 최대 원소 수 (사전 할당, 리사이즈 불가)
    [[nodiscard]] bool Create(ID3D11Device* device,
                              uint32_t maxElements,
                              EStructuredBufferMode mode);

    // CPU → GPU 업로드 (eGPUReadOnly 모드)
    // Map/WRITE_DISCARD로 전체 교체
    void Upload(ID3D11DeviceContext* context,
                const T* data, uint32_t count);

    // GPU → CPU 리드백 (eStaging 모드)
    // 1. CopyResource로 staging 버퍼에 복사
    // 2. Map/READ로 CPU에서 읽기
    bool Readback(ID3D11DeviceContext* context,
                  T* outData, uint32_t count);

    // ── SRV 바인딩 ─────────────────────────────────────────────
    void BindSRV_VS(ID3D11DeviceContext* context, uint32_t slot) const;
    void BindSRV_PS(ID3D11DeviceContext* context, uint32_t slot) const;
    void BindSRV_CS(ID3D11DeviceContext* context, uint32_t slot) const;

    // ── UAV 바인딩 (eGPUReadWrite 모드) ────────────────────────
    void BindUAV_CS(ID3D11DeviceContext* context, uint32_t slot) const;

    // ── 바인딩 해제 ────────────────────────────────────────────
    void UnbindSRV_VS(ID3D11DeviceContext* context, uint32_t slot) const;
    void UnbindSRV_CS(ID3D11DeviceContext* context, uint32_t slot) const;
    void UnbindUAV_CS(ID3D11DeviceContext* context, uint32_t slot) const;

    void Release();

    // ── 접근자 ──────────────────────────────────────────────────
    ID3D11Buffer*             GetBuffer() const { return m_pBuffer; }
    ID3D11ShaderResourceView* GetSRV()    const { return m_pSRV; }
    ID3D11UnorderedAccessView* GetUAV()   const { return m_pUAV; }
    uint32_t GetMaxElements()    const { return m_MaxElements; }
    uint32_t GetCurrentCount()   const { return m_CurrentCount; }

private:
    ID3D11Buffer*              m_pBuffer    = nullptr;
    ID3D11ShaderResourceView*  m_pSRV       = nullptr;
    ID3D11UnorderedAccessView* m_pUAV       = nullptr;
    uint32_t                   m_MaxElements  = 0;
    uint32_t                   m_CurrentCount = 0;
    EStructuredBufferMode      m_Mode = EStructuredBufferMode::eGPUReadOnly;
};
```

**구현 참고** (`Engine/Code/RHI/DX11/DX11StructuredBuffer.cpp`):

`Create()` 핵심:
```cpp
D3D11_BUFFER_DESC desc = {};
desc.ByteWidth           = sizeof(T) * maxElements;
desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
desc.StructureByteStride = sizeof(T);

switch (mode)
{
case eGPUReadOnly:
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    break;
case eGPUReadWrite:
    desc.Usage          = D3D11_USAGE_DEFAULT;
    desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.CPUAccessFlags = 0;
    break;
case eStaging:
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    break;
}
```

SRV 생성:
```cpp
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.ViewDimension           = D3D11_SRV_DIMENSION_BUFFER;
srvDesc.Buffer.FirstElement     = 0;
srvDesc.Buffer.NumElements      = maxElements;
```

UAV 생성 (eGPUReadWrite만):
```cpp
D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
uavDesc.ViewDimension            = D3D11_UAV_DIMENSION_BUFFER;
uavDesc.Buffer.FirstElement      = 0;
uavDesc.Buffer.NumElements       = maxElements;
uavDesc.Buffer.Flags             = 0;  // APPEND 시 D3D11_BUFFER_UAV_FLAG_APPEND
```

#### 4.1.3 `Engine/Header/RHI/DX11/DX11IndirectArgsBuffer.h`

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  DX11IndirectArgsBuffer  |  DrawIndexedInstancedIndirect 인자 버퍼
//
//  GPU 컴퓨트 셰이더가 작성하고, DrawIndexedInstancedIndirect()가 소비.
//  D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS 플래그 필수.
//
//  인자 구조 (D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS):
//    uint IndexCountPerInstance;
//    uint InstanceCount;
//    uint StartIndexLocation;
//    int  BaseVertexLocation;
//    uint StartInstanceLocation;
//
//  여러 메시 그룹의 인자를 연속 배열로 저장.
//  각 그룹은 20바이트(5 * uint32) 점유.
//
//  사용 흐름:
//    DX11IndirectArgsBuffer args;
//    args.Create(device, maxMeshGroups);
//    // CS가 UAV로 인자를 기록
//    args.BindUAV_CS(context, 0);
//    // DrawIndirect 시 오프셋 지정
//    args.DrawIndexedInstancedIndirect(context, groupIndex);
// ─────────────────────────────────────────────────────────────────

struct DrawIndexedIndirectArgs
{
    uint32 IndexCountPerInstance;
    uint32 InstanceCount;
    uint32 StartIndexLocation;
    int32  BaseVertexLocation;
    uint32 StartInstanceLocation;
};
static_assert(sizeof(DrawIndexedIndirectArgs) == 20);

class DX11IndirectArgsBuffer
{
public:
    DX11IndirectArgsBuffer()  = default;
    ~DX11IndirectArgsBuffer() { Release(); }

    DX11IndirectArgsBuffer(const DX11IndirectArgsBuffer&) = delete;
    DX11IndirectArgsBuffer& operator=(const DX11IndirectArgsBuffer&) = delete;

    // maxDrawCalls: 최대 메시 그룹 수
    [[nodiscard]] bool Create(ID3D11Device* device, uint32 maxDrawCalls);

    // CS에서 UAV로 바인딩하여 인자 기록
    void BindUAV_CS(ID3D11DeviceContext* context, uint32 slot) const;
    void UnbindUAV_CS(ID3D11DeviceContext* context, uint32 slot) const;

    // 초기값 리셋 (매 프레임 시작 시)
    // CPU에서 0으로 초기화된 데이터를 UpdateSubresource
    void Reset(ID3D11DeviceContext* context, uint32 drawCallCount);

    // DrawIndexedInstancedIndirect 발행
    // drawIndex: 몇 번째 메시 그룹인지 (바이트 오프셋 = drawIndex * 20)
    void DrawIndirect(ID3D11DeviceContext* context, uint32 drawIndex) const;

    void Release();

    ID3D11Buffer*              GetBuffer() const { return m_pBuffer; }
    ID3D11UnorderedAccessView* GetUAV()    const { return m_pUAV; }
    uint32 GetMaxDrawCalls() const { return m_MaxDrawCalls; }

private:
    ID3D11Buffer*              m_pBuffer       = nullptr;
    ID3D11UnorderedAccessView* m_pUAV          = nullptr;
    uint32                     m_MaxDrawCalls  = 0;
};
```

**구현 참고** (`Engine/Code/RHI/DX11/DX11IndirectArgsBuffer.cpp`):

`Create()` 핵심:
```cpp
D3D11_BUFFER_DESC desc = {};
desc.ByteWidth      = sizeof(DrawIndexedIndirectArgs) * maxDrawCalls;
desc.Usage           = D3D11_USAGE_DEFAULT;
desc.BindFlags       = D3D11_BIND_UNORDERED_ACCESS;
desc.MiscFlags       = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
                     | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
desc.CPUAccessFlags  = 0;
```

UAV 생성 (RW바이트 주소 버퍼 or typed):
```cpp
D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
uavDesc.Buffer.FirstElement = 0;
uavDesc.Buffer.NumElements  = maxDrawCalls * 5;  // 5 uint per draw
uavDesc.Buffer.Flags        = D3D11_BUFFER_UAV_FLAG_RAW;
uavDesc.Format              = DXGI_FORMAT_R32_TYPELESS;
```

`DrawIndirect()`:
```cpp
uint32 byteOffset = drawIndex * sizeof(DrawIndexedIndirectArgs);
context->DrawIndexedInstancedIndirect(m_pBuffer, byteOffset);
```

#### 4.1.4 `Engine/Header/RHI/DX11/DX11AppendBuffer.h`

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <cassert>

// ─────────────────────────────────────────────────────────────────
//  DX11AppendBuffer<T>  |  AppendStructuredBuffer 래퍼
//
//  Compute Shader에서 가변 길이 출력을 위한 Append 버퍼.
//  컬링 통과한 인스턴스 ID를 모으는 데 사용.
//
//  내부 카운터(hidden counter)를 가지며, UAV 바인딩 시
//  initialCount를 지정하여 리셋 가능.
//
//  사용 흐름:
//    DX11AppendBuffer<uint32> visible;
//    visible.Create(device, maxInstances);
//    // CS에서 AppendStructuredBuffer로 바인딩
//    visible.BindUAV_CS(context, slot, 0);  // initialCount = 0 (리셋)
//    // CS Dispatch 후, 카운터를 읽어서 가시 인스턴스 수 확인
//    uint32 count = visible.CopyCount(context);
// ─────────────────────────────────────────────────────────────────

template<typename T>
class DX11AppendBuffer
{
public:
    DX11AppendBuffer()  = default;
    ~DX11AppendBuffer() { Release(); }

    DX11AppendBuffer(const DX11AppendBuffer&) = delete;
    DX11AppendBuffer& operator=(const DX11AppendBuffer&) = delete;

    [[nodiscard]] bool Create(ID3D11Device* device, uint32_t maxElements);

    // UAV 바인딩 (AppendStructuredBuffer)
    // initialCount: 내부 카운터 초기값 (0이면 리셋)
    void BindUAV_CS(ID3D11DeviceContext* context, uint32_t slot,
                    uint32_t initialCount = 0) const;
    void UnbindUAV_CS(ID3D11DeviceContext* context, uint32_t slot) const;

    // SRV로 읽기 (다음 패스에서 결과 소비)
    void BindSRV_VS(ID3D11DeviceContext* context, uint32_t slot) const;
    void BindSRV_CS(ID3D11DeviceContext* context, uint32_t slot) const;
    void UnbindSRV_CS(ID3D11DeviceContext* context, uint32_t slot) const;

    // 내부 카운터를 staging 버퍼로 복사 후 CPU에서 읽기
    // CopyStructureCount() → Map/READ
    uint32_t CopyCount(ID3D11DeviceContext* context);

    void Release();

    ID3D11Buffer*              GetBuffer() const { return m_pBuffer; }
    ID3D11ShaderResourceView*  GetSRV()    const { return m_pSRV; }
    ID3D11UnorderedAccessView* GetUAV()    const { return m_pUAV; }

private:
    ID3D11Buffer*              m_pBuffer        = nullptr;
    ID3D11ShaderResourceView*  m_pSRV           = nullptr;
    ID3D11UnorderedAccessView* m_pUAV           = nullptr;
    ID3D11Buffer*              m_pCounterBuffer  = nullptr;  // 카운터 리드백용 staging
    uint32_t                   m_MaxElements     = 0;
};
```

**구현 참고**:

`Create()` - UAV 생성 시 `D3D11_BUFFER_UAV_FLAG_APPEND` 플래그:
```cpp
D3D11_BUFFER_DESC desc = {};
desc.ByteWidth           = sizeof(T) * maxElements;
desc.Usage               = D3D11_USAGE_DEFAULT;
desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
desc.StructureByteStride = sizeof(T);

// UAV with APPEND flag
D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
uavDesc.Buffer.FirstElement = 0;
uavDesc.Buffer.NumElements  = maxElements;
uavDesc.Buffer.Flags        = D3D11_BUFFER_UAV_FLAG_APPEND;

// 카운터 리드백용 staging 버퍼 (4 bytes)
D3D11_BUFFER_DESC counterDesc = {};
counterDesc.ByteWidth      = sizeof(uint32_t);
counterDesc.Usage           = D3D11_USAGE_STAGING;
counterDesc.CPUAccessFlags  = D3D11_CPU_ACCESS_READ;
```

`CopyCount()`:
```cpp
context->CopyStructureCount(m_pCounterBuffer, 0, m_pUAV);
D3D11_MAPPED_SUBRESOURCE mapped;
context->Map(m_pCounterBuffer, 0, D3D11_MAP_READ, 0, &mapped);
uint32_t count = *static_cast<uint32_t*>(mapped.pData);
context->Unmap(m_pCounterBuffer, 0);
return count;
```

---

### 4.2 GPU Driven Types (공유 구조체)

#### 4.2.1 `Engine/Header/Renderer/GPUDriven/GPUDrivenTypes.h`

```cpp
#pragma once
#include <DirectXMath.h>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────
//  GPUDrivenTypes.h  |  GPU/CPU 공유 데이터 구조체
//
//  이 구조체들은 CPU(C++)와 GPU(HLSL) 양쪽에서 동일한 메모리 레이아웃.
//  HLSL 쪽은 Common.hlsli에서 동일 구조를 정의.
//  16바이트 정렬 주의 (HLSL float4x4 = 64B, float4 = 16B).
// ─────────────────────────────────────────────────────────────────

// ── 인스턴스 데이터 (GPU에 업로드) ─────────────────────────────
// 각 렌더 가능 엔티티당 1개
struct GPUInstanceData
{
    DirectX::XMFLOAT4X4 worldMatrix;    // 64B — 월드 변환 행렬
    DirectX::XMFLOAT3   aabbMin;        // 12B — AABB 최소점 (로컬)
    uint32_t            meshId;         // 4B  — 메가버퍼 내 메시 인덱스
    DirectX::XMFLOAT3   aabbMax;        // 12B — AABB 최대점 (로컬)
    uint32_t            materialId;     // 4B  — 머티리얼 인덱스
    // Total: 96B (16B 정렬 OK)
};
static_assert(sizeof(GPUInstanceData) == 96);

// ── 메시 디스크립터 (메가버퍼 내 메시 위치 정보) ──────────────
// 메시 종류당 1개 (예: Cube, Sphere, Tree ...)
// LOD 배열 포함 — LOD 레벨별 다른 인덱스 범위
struct GPUMeshDescriptor
{
    uint32_t indexCountPerLOD[4];       // 16B — LOD 0~3 인덱스 수
    uint32_t startIndexPerLOD[4];      // 16B — LOD 0~3 시작 인덱스
    int32_t  baseVertexPerLOD[4];      // 16B — LOD 0~3 베이스 버텍스
    DirectX::XMFLOAT3 localAABBMin;    // 12B — 로컬 AABB 최소
    float    lodDistances[4];          // 16B — LOD 전환 거리 (제곱)
    DirectX::XMFLOAT3 localAABBMax;    // 12B — 로컬 AABB 최대
    float    _pad0;                    // 4B  — 패딩
    // Total: 96B (16B 정렬 OK)
};
static_assert(sizeof(GPUMeshDescriptor) == 96);

// ── 가시 인스턴스 (컬링 후 컴팩트 결과) ──────────────────────
struct GPUCompactInstance
{
    uint32_t            instanceId;     // 4B  — 원본 인스턴스 인덱스
    uint32_t            meshId;         // 4B  — 메시 인덱스
    uint32_t            lodLevel;       // 4B  — 선택된 LOD
    uint32_t            drawCallId;     // 4B  — 소속 드로우콜 인덱스
    // Total: 16B
};
static_assert(sizeof(GPUCompactInstance) == 16);

// ── 프러스텀 평면 (cbuffer로 전달) ────────────────────────────
// 16B 정렬 cbuffer 구조
struct CBFrustumCull
{
    DirectX::XMFLOAT4  frustumPlanes[6]; // 96B — 6개 프러스텀 평면 (ABCD)
    DirectX::XMFLOAT4  cameraPosition;   // 16B — 카메라 위치 (LOD 거리 계산)
    uint32_t           instanceCount;     // 4B  — 총 인스턴스 수
    uint32_t           meshGroupCount;    // 4B  — 메시 그룹 수
    float              _pad[2];           // 8B  — 패딩
    // Total: 128B (16B 정렬 OK)
};
static_assert(sizeof(CBFrustumCull) % 16 == 0);

// ── Hi-Z 생성 cbuffer ─────────────────────────────────────────
struct CBHiZGenerate
{
    uint32_t srcMipWidth;
    uint32_t srcMipHeight;
    uint32_t _pad[2];
    // Total: 16B
};
static_assert(sizeof(CBHiZGenerate) % 16 == 0);

// ── 오클루전 컬링 cbuffer ─────────────────────────────────────
struct CBOcclusionCull
{
    DirectX::XMFLOAT4X4 viewProjection;  // 64B
    DirectX::XMFLOAT2   hiZSize;         // 8B  — Hi-Z 텍스처 크기
    uint32_t            hiZMipLevels;     // 4B  — 밉 레벨 수
    uint32_t            instanceCount;    // 4B
    // Total: 80B (16B 정렬 OK)
};
static_assert(sizeof(CBOcclusionCull) % 16 == 0);
```

---

### 4.3 ECS 확장

#### 4.3.1 `Engine/Header/ECS/Components/RenderComponent.h`

```cpp
#pragma once
#include <cstdint>
#include "WintersMath.h"

// ─────────────────────────────────────────────────────────────────
//  RenderComponent  |  렌더 가능 엔티티에 부착
//
//  meshId     : CGPUScene에 등록된 메시 인덱스
//  materialId : 머티리얼 테이블 인덱스
//  gpuIndex   : GPU 인스턴스 버퍼 내 슬롯 (-1 = 미등록)
//  visible    : 최종 가시성 (디버그/CPU 측 참고)
// ─────────────────────────────────────────────────────────────────

struct RenderComponent
{
    uint32_t meshId      = 0;
    uint32_t materialId  = 0;
    int32_t  gpuIndex    = -1;     // GPU 인스턴스 버퍼 내 슬롯
    bool     bCastShadow = true;
    bool     bVisible    = true;   // CPU 측 활성/비활성
};
```

---

### 4.4 GPU Scene 관리

#### 4.4.1 `Engine/Header/Renderer/GPUDriven/CGPUScene.h`

```cpp
#pragma once
#include "RHI/DX11/DX11StructuredBuffer.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  CGPUScene  |  GPU 측 씬 데이터 관리
//
//  모든 렌더 가능 인스턴스의 GPU 버퍼를 관리.
//  ECS의 TransformComponent + RenderComponent → GPUInstanceData.
//
//  메가버퍼: 모든 메시의 VB/IB를 하나의 큰 버퍼에 통합.
//  메시 등록 시 메가버퍼에 추가되고, MeshDescriptor가 생성됨.
//
//  사용 흐름:
//    CGPUScene scene;
//    scene.Initialize(device, 10000, 64);  // max 10K instances, 64 mesh types
//    uint32 meshId = scene.RegisterMesh(vertices, indices, aabb, lodData);
//    int32 slot = scene.AddInstance(worldMat, meshId, matId);
//    scene.UpdateInstance(slot, newWorldMat);
//    scene.UploadDirtyInstances(context);
// ─────────────────────────────────────────────────────────────────

class CDX11Device;  // forward

class CGPUScene
{
public:
    CGPUScene() = default;
    ~CGPUScene() { Shutdown(); }

    // ── 초기화 ──────────────────────────────────────────────────
    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t maxInstances,
                                  uint32_t maxMeshTypes);
    void Shutdown();

    // ── 메시 등록 (메가버퍼에 추가) ─────────────────────────────
    // 반환: meshId (GPUMeshDescriptor 인덱스)
    // vertices: 바이트 배열, vertexCount, stride
    // indices: uint32 배열, indexCount
    // LOD 데이터: 각 LOD별 인덱스 범위
    struct MeshLODInfo
    {
        const void*  pVertices;
        uint32_t     vertexCount;
        uint32_t     vertexStride;
        const uint32_t* pIndices;
        uint32_t     indexCount;
    };
    // lodMeshes[0] = LOD0 (최고 품질), lodMeshes[n] = LOD_n
    uint32_t RegisterMesh(const MeshLODInfo* lodMeshes, uint32_t lodCount,
                          const Vec3& aabbMin, const Vec3& aabbMax,
                          const float* lodDistancesSq);

    // ── 인스턴스 관리 ───────────────────────────────────────────
    // 반환: 슬롯 인덱스 (gpuIndex)
    int32_t  AddInstance(const DirectX::XMFLOAT4X4& worldMat,
                         uint32_t meshId, uint32_t materialId);
    void     RemoveInstance(int32_t gpuIndex);
    void     UpdateInstanceTransform(int32_t gpuIndex,
                                     const DirectX::XMFLOAT4X4& worldMat);

    // ── GPU 업로드 (매 프레임) ──────────────────────────────────
    // dirty 인스턴스만 업로드 (전체 WRITE_DISCARD)
    void     UploadDirtyInstances(ID3D11DeviceContext* context);

    // ── 접근자 (렌더러에서 사용) ────────────────────────────────
    DX11StructuredBuffer<GPUInstanceData>&    GetInstanceBuffer()    { return m_InstanceBuffer; }
    DX11StructuredBuffer<GPUMeshDescriptor>&  GetMeshDescBuffer()    { return m_MeshDescBuffer; }
    ID3D11Buffer*   GetMegaVertexBuffer() const { return m_pMegaVB; }
    ID3D11Buffer*   GetMegaIndexBuffer()  const { return m_pMegaIB; }
    uint32_t        GetInstanceCount()    const { return m_ActiveInstances; }
    uint32_t        GetMeshGroupCount()   const { return m_MeshGroupCount; }
    uint32_t        GetMegaVBStride()     const { return m_MegaVBStride; }

private:
    // ── GPU 버퍼 ────────────────────────────────────────────────
    DX11StructuredBuffer<GPUInstanceData>   m_InstanceBuffer;
    DX11StructuredBuffer<GPUMeshDescriptor> m_MeshDescBuffer;

    // ── 메가버퍼 (모든 메시 통합 VB/IB) ────────────────────────
    ID3D11Buffer*  m_pMegaVB      = nullptr;
    ID3D11Buffer*  m_pMegaIB      = nullptr;
    uint32_t       m_MegaVBStride = 0;

    // ── CPU 측 미러 (업로드용) ──────────────────────────────────
    std::vector<GPUInstanceData>   m_vecInstances;
    std::vector<GPUMeshDescriptor> m_vecMeshDescs;
    std::vector<bool>              m_vecDirty;       // dirty flag per instance
    bool                           m_bAnyDirty = false;

    // ── 슬롯 관리 (프리 리스트) ─────────────────────────────────
    std::vector<int32_t>  m_vecFreeSlots;
    uint32_t              m_ActiveInstances = 0;
    uint32_t              m_MaxInstances    = 0;

    // ── 메가버퍼 빌더 ──────────────────────────────────────────
    std::vector<uint8_t>  m_vecMegaVBData;   // CPU 측 정점 누적
    std::vector<uint32_t> m_vecMegaIBData;   // CPU 측 인덱스 누적
    uint32_t              m_MegaVBOffset = 0; // 현재 정점 오프셋
    uint32_t              m_MegaIBOffset = 0; // 현재 인덱스 오프셋
    uint32_t              m_MeshGroupCount = 0;

    // 메가버퍼 재빌드 (메시 등록 후 GPU 업로드)
    void RebuildMegaBuffers(ID3D11Device* device);
};
```

**구현 참고** (`Engine/Code/Renderer/GPUDriven/CGPUScene.cpp`):

- `RegisterMesh()`: LOD별 정점/인덱스를 `m_vecMegaVBData`/`m_vecMegaIBData`에 누적, `GPUMeshDescriptor` 채움
- `AddInstance()`: 프리 리스트에서 슬롯 할당, `m_vecInstances[slot]` 채움, dirty 마크
- `UploadDirtyInstances()`: `m_bAnyDirty`가 true면 `m_InstanceBuffer.Upload(context, m_vecInstances.data(), m_ActiveInstances)` (WRITE_DISCARD이므로 전체 업로드, dirty 체크는 CPU 측 최적화용)
- `RebuildMegaBuffers()`: `CreateVertex`/`CreateIndex` 호출하여 IMMUTABLE 메가버퍼 재생성 (메시 등록은 초기화 시에만 일어나므로 빈번하지 않음)

---

### 4.5 GPU Frustum Culling

#### 4.5.1 `Engine/Header/Renderer/GPUDriven/CGPUCuller.h`

```cpp
#pragma once
#include "RHI/DX11/DX11ComputeShader.h"
#include "RHI/DX11/DX11StructuredBuffer.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CGPUCuller  |  GPU 프러스텀 컬링
//
//  각 인스턴스의 월드 AABB를 프러스텀 6개 평면과 비교.
//  결과: uint 배열 (0 = 비가시, 1 = 가시) → VisibilityBuffer
//
//  스레드 그룹: 256 스레드 (인스턴스당 1스레드)
//  디스패치 그룹 수: ceil(instanceCount / 256)
// ─────────────────────────────────────────────────────────────────

class CGPUScene;

class CGPUCuller
{
public:
    CGPUCuller() = default;
    ~CGPUCuller() { Shutdown(); }

    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t maxInstances);
    void Shutdown();

    // 프러스텀 평면 추출 (ViewProjection 행렬에서)
    // 결과를 cbuffer에 업로드
    void UpdateFrustum(ID3D11DeviceContext* context,
                       const DirectX::XMFLOAT4X4& viewProjection,
                       const DirectX::XMFLOAT3& cameraPos,
                       uint32_t instanceCount,
                       uint32_t meshGroupCount);

    // 컬링 디스패치
    // instanceBuffer: SRV (읽기), visibilityBuffer: UAV (쓰기)
    void Dispatch(ID3D11DeviceContext* context,
                  CGPUScene& scene);

    // 결과 접근
    DX11StructuredBuffer<uint32_t>& GetVisibilityBuffer() { return m_VisibilityBuffer; }

private:
    // 프러스텀 평면 추출 유틸리티
    static void ExtractFrustumPlanes(const DirectX::XMFLOAT4X4& viewProj,
                                     DirectX::XMFLOAT4 outPlanes[6]);

    DX11ComputeShader                    m_CullShader;
    DX11ConstantBuffer<CBFrustumCull>    m_CullCB;
    DX11StructuredBuffer<uint32_t>       m_VisibilityBuffer;  // 0/1 per instance

    static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};
```

**구현 참고** (`Engine/Code/Renderer/GPUDriven/CGPUCuller.cpp`):

`Initialize()`:
- `m_CullShader.Load(device, L"Shaders/GPUDriven/FrustumCull.hlsl")`
- `m_VisibilityBuffer.Create(device, maxInstances, eGPUReadWrite)` (UAV + SRV)
- `m_CullCB.Create(device)`

`UpdateFrustum()`:
- `ExtractFrustumPlanes()`: Gribb-Hartmann 방법으로 ViewProj 행렬에서 6개 평면 추출
- `m_CullCB.Update(context, data)`

`Dispatch()`:
```cpp
// SRV t0: 인스턴스 데이터
scene.GetInstanceBuffer().BindSRV_CS(context, 0);
// SRV t1: 메시 디스크립터
scene.GetMeshDescBuffer().BindSRV_CS(context, 1);
// UAV u0: 가시성 버퍼 (출력)
m_VisibilityBuffer.BindUAV_CS(context, 0);
// CB b0: 프러스텀 데이터
m_CullCB.BindCS(context, 0);  // DX11ConstantBuffer에 BindCS 추가 필요

m_CullShader.Bind(context);
uint32 groups = (instanceCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
m_CullShader.Dispatch(context, groups, 1, 1);
m_CullShader.Unbind(context);

// 언바인드
scene.GetInstanceBuffer().UnbindSRV_CS(context, 0);
m_VisibilityBuffer.UnbindUAV_CS(context, 0);
```

**참고**: `DX11ConstantBuffer<T>`에 `BindCS()` 메서드 추가 필요:
```cpp
// Engine/Header/RHI/DX11/DX11ConstantBuffer.h L92 이후 추가
void BindCS(ID3D11DeviceContext* context, UINT slot) const
{
    assert(context && m_pBuffer);
    context->CSSetConstantBuffers(slot, 1, &m_pBuffer);
}
```

---

### 4.6 Hi-Z Buffer

#### 4.6.1 `Engine/Header/Renderer/GPUDriven/CHiZBuffer.h`

```cpp
#pragma once
#include "RHI/DX11/DX11ComputeShader.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"
#include <d3d11.h>

// ─────────────────────────────────────────────────────────────────
//  CHiZBuffer  |  Hierarchical Z-Buffer (밉체인 생성)
//
//  이전 프레임의 뎁스 버퍼에서 Hi-Z 밉맵 피라미드를 생성.
//  각 밉 레벨은 2x2 블록의 최대(가장 먼) 깊이값을 저장.
//  오클루전 컬링에서 오브젝트의 최소 깊이를 Hi-Z와 비교.
//
//  생성 과정:
//    1. 뎁스 텍스처 → Mip 0 복사 (SRV→UAV)
//    2. Mip 0 → Mip 1 다운샘플 (CS)
//    3. Mip 1 → Mip 2 다운샘플 (CS)
//    4. ... 반복
//
//  DX11 제약: UAV는 텍스처의 특정 mip에 직접 바인딩 불가.
//  → 밉 레벨마다 별도 UAV를 미리 생성해둠.
// ─────────────────────────────────────────────────────────────────

class CHiZBuffer
{
public:
    CHiZBuffer() = default;
    ~CHiZBuffer() { Shutdown(); }

    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t width, uint32_t height);
    void Shutdown();

    // 이전 프레임의 뎁스 텍스처 SRV를 입력으로 받아 Hi-Z 밉체인 생성
    void Generate(ID3D11DeviceContext* context,
                  ID3D11ShaderResourceView* depthSRV);

    // Hi-Z 텍스처 SRV (오클루전 컬링에서 사용)
    ID3D11ShaderResourceView* GetHiZSRV() const { return m_pHiZSRV; }
    uint32_t GetMipLevels() const { return m_MipLevels; }
    uint32_t GetWidth()     const { return m_Width; }
    uint32_t GetHeight()    const { return m_Height; }

private:
    DX11ComputeShader                    m_HiZShader;
    DX11ConstantBuffer<CBHiZGenerate>    m_HiZCB;

    // Hi-Z 텍스처 (R32_FLOAT, 전체 밉체인)
    ID3D11Texture2D*           m_pHiZTexture  = nullptr;
    ID3D11ShaderResourceView*  m_pHiZSRV      = nullptr;

    // 밉 레벨별 UAV + SRV (다운샘플 시 src=SRV, dst=UAV)
    static constexpr uint32_t MAX_MIP_LEVELS = 12;  // 4096x4096까지
    ID3D11UnorderedAccessView* m_pMipUAVs[MAX_MIP_LEVELS] = {};
    ID3D11ShaderResourceView*  m_pMipSRVs[MAX_MIP_LEVELS] = {};

    uint32_t m_Width     = 0;
    uint32_t m_Height    = 0;
    uint32_t m_MipLevels = 0;

    static constexpr uint32_t HIZ_THREAD_GROUP = 16;  // 16x16 스레드
};
```

**구현 참고** (`Engine/Code/Renderer/GPUDriven/CHiZBuffer.cpp`):

`Initialize()`:
```cpp
// Hi-Z 텍스처 (R32_FLOAT, 전체 밉체인, SRV + UAV)
D3D11_TEXTURE2D_DESC texDesc = {};
texDesc.Width     = width;
texDesc.Height    = height;
texDesc.MipLevels = m_MipLevels;  // log2(max(w,h)) + 1
texDesc.ArraySize = 1;
texDesc.Format    = DXGI_FORMAT_R32_FLOAT;
texDesc.SampleDesc = {1, 0};
texDesc.Usage     = D3D11_USAGE_DEFAULT;
texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
```

`Generate()` - 밉 레벨별 다운샘플 루프:
```cpp
// Mip 0: 뎁스 텍스처에서 복사 (SRV → UAV[0])
// context->CSSetShaderResources(0, 1, &depthSRV);
// context->CSSetUnorderedAccessViews(0, 1, &m_pMipUAVs[0], nullptr);
// Dispatch

for (uint32 mip = 1; mip < m_MipLevels; ++mip)
{
    uint32 srcW = max(1u, m_Width >> (mip - 1));
    uint32 srcH = max(1u, m_Height >> (mip - 1));
    // 이전 밉을 SRV로, 현재 밉을 UAV로
    // Dispatch(ceil(dstW/16), ceil(dstH/16), 1)
}
```

---

### 4.7 GPU Occlusion Culling

#### 4.7.1 `Engine/Header/Renderer/GPUDriven/CGPUOcclusionCuller.h`

```cpp
#pragma once
#include "RHI/DX11/DX11ComputeShader.h"
#include "RHI/DX11/DX11StructuredBuffer.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CGPUOcclusionCuller  |  Hi-Z 기반 오클루전 컬링
//
//  프러스텀 컬링을 통과한 인스턴스에 대해 Hi-Z 오클루전 테스트.
//  오브젝트의 스크린 공간 AABB를 Hi-Z 텍스처와 비교.
//
//  알고리즘:
//    1. 인스턴스 AABB의 8개 꼭짓점을 스크린 공간으로 투영
//    2. 스크린 AABB 크기에 맞는 Hi-Z 밉 레벨 선택
//    3. 해당 밉에서 depth 샘플 → 오브젝트 최소 Z와 비교
//    4. Hi-Z depth < 오브젝트 min depth → 가려짐 (비가시)
// ─────────────────────────────────────────────────────────────────

class CGPUScene;
class CHiZBuffer;

class CGPUOcclusionCuller
{
public:
    CGPUOcclusionCuller() = default;
    ~CGPUOcclusionCuller() { Shutdown(); }

    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t maxInstances);
    void Shutdown();

    // VP 행렬, Hi-Z 정보 업데이트
    void UpdateParams(ID3D11DeviceContext* context,
                      const DirectX::XMFLOAT4X4& viewProjection,
                      const CHiZBuffer& hiZ,
                      uint32_t instanceCount);

    // 오클루전 컬링 디스패치
    // 입력: visibilityBuffer (프러스텀 결과), instanceBuffer, hiZ SRV
    // 출력: visibilityBuffer 업데이트 (가려진 인스턴스 0으로 마킹)
    void Dispatch(ID3D11DeviceContext* context,
                  CGPUScene& scene,
                  DX11StructuredBuffer<uint32_t>& visibilityBuffer,
                  const CHiZBuffer& hiZ);

private:
    DX11ComputeShader                     m_OcclusionShader;
    DX11ConstantBuffer<CBOcclusionCull>   m_OcclusionCB;

    static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};
```

---

### 4.8 GPU LOD Selector

#### 4.8.1 `Engine/Header/Renderer/GPUDriven/CGPULODSelector.h`

```cpp
#pragma once
#include "RHI/DX11/DX11ComputeShader.h"
#include "RHI/DX11/DX11StructuredBuffer.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CGPULODSelector  |  GPU LOD 선택 컴퓨트 셰이더
//
//  가시 인스턴스의 카메라 거리를 기반으로 LOD 레벨 결정.
//  MeshDescriptor의 lodDistances[] 배열과 비교.
//
//  출력: uint 배열 (인스턴스당 LOD 레벨 0~3)
// ─────────────────────────────────────────────────────────────────

class CGPUScene;

class CGPULODSelector
{
public:
    CGPULODSelector() = default;
    ~CGPULODSelector() { Shutdown(); }

    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t maxInstances);
    void Shutdown();

    // LOD 선택 디스패치
    // 입력: instanceBuffer(SRV), meshDescBuffer(SRV), visibility(SRV), cameraPos
    // 출력: lodLevelBuffer(UAV) — 인스턴스당 LOD 레벨
    void Dispatch(ID3D11DeviceContext* context,
                  CGPUScene& scene,
                  DX11StructuredBuffer<uint32_t>& visibilityBuffer,
                  const DirectX::XMFLOAT3& cameraPos);

    DX11StructuredBuffer<uint32_t>& GetLODBuffer() { return m_LODBuffer; }

private:
    DX11ComputeShader                  m_LODShader;
    DX11StructuredBuffer<uint32_t>     m_LODBuffer;     // LOD level per instance

    static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};
```

---

### 4.9 Indirect Renderer (메인 GPU Driven 드로우 시스템)

#### 4.9.1 `Engine/Header/Renderer/GPUDriven/CIndirectRenderer.h`

```cpp
#pragma once
#include "RHI/DX11/DX11ComputeShader.h"
#include "RHI/DX11/DX11StructuredBuffer.h"
#include "RHI/DX11/DX11IndirectArgsBuffer.h"
#include "RHI/DX11/DX11AppendBuffer.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "Renderer/GPUDriven/GPUDrivenTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CIndirectRenderer  |  GPU Driven Indirect Draw 시스템
//
//  전체 GPU Driven Pipeline의 최종 단계.
//  1. CompactAndArgs CS: 가시 인스턴스를 압축하여 연속 배열 생성
//     + DrawIndexedInstancedIndirect 인자 버퍼 기록
//  2. DrawIndexedInstancedIndirect: 메시 그룹별 1회 호출
//
//  VS는 InstanceID → CompactInstances[InstanceID] → 원본 인스턴스 데이터 참조.
//  PS는 materialId로 머티리얼 테이블 인덱싱.
//
//  사용 흐름 (프레임당):
//    renderer.CompactAndGenerateArgs(context, scene, visibility, lodLevels);
//    renderer.DrawAllGroups(context, scene, vpMatrix);
// ─────────────────────────────────────────────────────────────────

class CGPUScene;

class CIndirectRenderer
{
public:
    CIndirectRenderer() = default;
    ~CIndirectRenderer() { Shutdown(); }

    [[nodiscard]] bool Initialize(ID3D11Device* device,
                                  uint32_t maxInstances,
                                  uint32_t maxMeshGroups);
    void Shutdown();

    // ── Phase 1: 컴팩트 + 인자 생성 (Compute) ──────────────────
    void CompactAndGenerateArgs(
        ID3D11DeviceContext* context,
        CGPUScene& scene,
        DX11StructuredBuffer<uint32_t>& visibilityBuffer,
        DX11StructuredBuffer<uint32_t>& lodBuffer);

    // ── Phase 2: Indirect Draw (Graphics) ───────────────────────
    void DrawAllGroups(
        ID3D11DeviceContext* context,
        CGPUScene& scene,
        const DirectX::XMFLOAT4X4& viewProjection);

private:
    // ── Compact + Args 컴퓨트 ──────────────────────────────────
    DX11ComputeShader                      m_CompactShader;
    DX11StructuredBuffer<GPUCompactInstance> m_CompactBuffer;    // 압축된 가시 인스턴스
    DX11IndirectArgsBuffer                 m_IndirectArgs;       // 메시 그룹별 드로우 인자

    // ── 드로우 셰이더 ──────────────────────────────────────────
    DX11Shader     m_DrawShader;     // GPUDrivenVS + GPUDrivenPS
    DX11Pipeline   m_DrawPipeline;   // InputLayout (위치만, 인스턴스는 SRV)

    // ── Per-Frame cbuffer ──────────────────────────────────────
    DX11ConstantBuffer<CBPerFrame> m_cbPerFrame;

    // ── 카운터 리드백 ──────────────────────────────────────────
    // CompactShader의 Append 카운터를 읽어서 실제 드로우 수 확인
    // (DX11에서는 DrawIndexedInstancedIndirect가 Args에서 읽으므로
    //  CPU에서 카운터를 읽을 필요는 없지만, 디버그/통계용)

    uint32_t m_MaxInstances   = 0;
    uint32_t m_MaxMeshGroups  = 0;

    static constexpr uint32_t THREAD_GROUP_SIZE = 256;
};
```

**구현 참고** (`Engine/Code/Renderer/GPUDriven/CIndirectRenderer.cpp`):

`CompactAndGenerateArgs()`:
```cpp
// 인자 버퍼 리셋 (InstanceCount = 0)
m_IndirectArgs.Reset(context, scene.GetMeshGroupCount());

// CS 바인딩
// SRV t0: 인스턴스 데이터
// SRV t1: 가시성 버퍼
// SRV t2: LOD 버퍼
// SRV t3: 메시 디스크립터
// UAV u0: CompactBuffer (Append)
// UAV u1: IndirectArgs (RW)

m_CompactShader.Bind(context);
uint32 groups = (instanceCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
m_CompactShader.Dispatch(context, groups, 1, 1);
m_CompactShader.Unbind(context);
// 언바인드 all
```

`DrawAllGroups()`:
```cpp
// VP cbuffer 업데이트
CBPerFrame data;
data.viewProjection = viewProjection;
m_cbPerFrame.Update(context, data);
m_cbPerFrame.BindVS(context, 0);

// 파이프라인 바인딩
m_DrawPipeline.Bind(context);
m_DrawShader.Bind(context);

// 메가버퍼 바인딩
UINT stride = scene.GetMegaVBStride();
UINT offset = 0;
auto* megaVB = scene.GetMegaVertexBuffer();
context->IASetVertexBuffers(0, 1, &megaVB, &stride, &offset);
context->IASetIndexBuffer(scene.GetMegaIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

// SRV t0 (VS): CompactInstance 데이터
m_CompactBuffer.BindSRV_VS(context, 0);
// SRV t1 (VS): 원본 인스턴스 데이터 (월드 행렬)
scene.GetInstanceBuffer().BindSRV_VS(context, 1);

// 메시 그룹별 DrawIndexedInstancedIndirect
for (uint32 g = 0; g < scene.GetMeshGroupCount(); ++g)
{
    m_IndirectArgs.DrawIndirect(context, g);
}

m_DrawShader.Unbind(context);
```

---

### 4.10 HLSL 셰이더

#### 4.10.1 `Shaders/GPUDriven/Common.hlsli`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  Common.hlsli  |  GPU Driven 공유 구조체 / 유틸리티
//
//  C++ GPUDrivenTypes.h와 동일한 메모리 레이아웃 유지 필수.
// ─────────────────────────────────────────────────────────────────

// ── 인스턴스 데이터 (96B) ──────────────────────────────────────
struct InstanceData
{
    float4x4 worldMatrix;      // 64B
    float3   aabbMin;          // 12B
    uint     meshId;           // 4B
    float3   aabbMax;          // 12B
    uint     materialId;       // 4B
};

// ── 메시 디스크립터 (96B) ──────────────────────────────────────
struct MeshDescriptor
{
    uint   indexCountPerLOD[4];    // 16B
    uint   startIndexPerLOD[4];   // 16B
    int    baseVertexPerLOD[4];   // 16B
    float3 localAABBMin;          // 12B
    float  lodDistances[4];       // 16B
    float3 localAABBMax;          // 12B
    float  _pad0;                 // 4B
};

// ── 컴팩트 인스턴스 (16B) ──────────────────────────────────────
struct CompactInstance
{
    uint instanceId;
    uint meshId;
    uint lodLevel;
    uint drawCallId;
};

// ── AABB vs Frustum 판정 ────────────────────────────────────────
// 6개 프러스텀 평면과 월드 AABB 비교. 하나라도 완전 바깥이면 false.
bool IsAABBInsideFrustum(float4 planes[6], float3 aabbMin, float3 aabbMax)
{
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float3 p;
        p.x = (planes[i].x >= 0) ? aabbMax.x : aabbMin.x;
        p.y = (planes[i].y >= 0) ? aabbMax.y : aabbMin.y;
        p.z = (planes[i].z >= 0) ? aabbMax.z : aabbMin.z;

        if (dot(float4(p, 1.0f), planes[i]) < 0.0f)
            return false;
    }
    return true;
}

// ── 로컬 AABB → 월드 AABB 변환 ─────────────────────────────────
// 월드 행렬의 각 축을 사용하여 AABB 확장
void TransformAABB(float4x4 world, float3 localMin, float3 localMax,
                   out float3 worldMin, out float3 worldMax)
{
    float3 center   = (localMin + localMax) * 0.5f;
    float3 halfSize = (localMax - localMin) * 0.5f;

    float3 worldCenter = mul(float4(center, 1.0f), world).xyz;

    // 각 축의 절대값으로 AABB 확장
    float3 worldHalf = float3(0, 0, 0);
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        worldHalf.x += abs(world[i][0]) * halfSize[i];
        worldHalf.y += abs(world[i][1]) * halfSize[i];
        worldHalf.z += abs(world[i][2]) * halfSize[i];
    }

    worldMin = worldCenter - worldHalf;
    worldMax = worldCenter + worldHalf;
}
```

#### 4.10.2 `Shaders/GPUDriven/FrustumCull.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  FrustumCull.hlsl  |  GPU 프러스텀 컬링 Compute Shader
//
//  인스턴스당 1 스레드. 월드 AABB vs 프러스텀 6평면 비교.
//  결과: VisibilityBuffer[instanceId] = 0 or 1
// ─────────────────────────────────────────────────────────────────

#include "Common.hlsli"

cbuffer CBFrustumCull : register(b0)
{
    float4 g_FrustumPlanes[6];
    float4 g_CameraPosition;     // xyz = pos, w = unused
    uint   g_InstanceCount;
    uint   g_MeshGroupCount;
    float2 _pad;
};

StructuredBuffer<InstanceData>    g_Instances   : register(t0);
StructuredBuffer<MeshDescriptor>  g_MeshDescs   : register(t1);
RWStructuredBuffer<uint>          g_Visibility  : register(u0);

[numthreads(256, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= g_InstanceCount)
        return;

    InstanceData inst = g_Instances[idx];

    // 로컬 AABB → 월드 AABB
    float3 worldMin, worldMax;
    TransformAABB(inst.worldMatrix, inst.aabbMin, inst.aabbMax,
                  worldMin, worldMax);

    // 프러스텀 판정
    g_Visibility[idx] = IsAABBInsideFrustum(g_FrustumPlanes, worldMin, worldMax) ? 1 : 0;
}
```

#### 4.10.3 `Shaders/GPUDriven/HiZGenerate.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  HiZGenerate.hlsl  |  Hi-Z 밉맵 생성 Compute Shader
//
//  2x2 블록의 최대 depth를 다음 밉 레벨에 기록.
//  (최대값 = 가장 먼 depth → conservative occlusion)
// ─────────────────────────────────────────────────────────────────

cbuffer CBHiZGenerate : register(b0)
{
    uint g_SrcMipWidth;
    uint g_SrcMipHeight;
    uint2 _pad;
};

Texture2D<float>       g_SrcMip : register(t0);
RWTexture2D<float>     g_DstMip : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint2 dstCoord = dtid.xy;
    uint2 srcCoord = dstCoord * 2;

    // 2x2 블록 샘플 (경계 클램프)
    float d00 = g_SrcMip[min(srcCoord + uint2(0, 0), uint2(g_SrcMipWidth-1, g_SrcMipHeight-1))];
    float d10 = g_SrcMip[min(srcCoord + uint2(1, 0), uint2(g_SrcMipWidth-1, g_SrcMipHeight-1))];
    float d01 = g_SrcMip[min(srcCoord + uint2(0, 1), uint2(g_SrcMipWidth-1, g_SrcMipHeight-1))];
    float d11 = g_SrcMip[min(srcCoord + uint2(1, 1), uint2(g_SrcMipWidth-1, g_SrcMipHeight-1))];

    // 최대값 (가장 먼 depth) → conservative
    g_DstMip[dstCoord] = max(max(d00, d10), max(d01, d11));
}
```

#### 4.10.4 `Shaders/GPUDriven/OcclusionCull.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  OcclusionCull.hlsl  |  Hi-Z 오클루전 컬링 Compute Shader
//
//  프러스텀 통과 인스턴스에 대해 Hi-Z 테스트.
//  AABB를 스크린 공간 투영 → 적절한 밉 레벨 선택 → depth 비교.
// ─────────────────────────────────────────────────────────────────

#include "Common.hlsli"

cbuffer CBOcclusionCull : register(b0)
{
    float4x4 g_ViewProjection;
    float2   g_HiZSize;        // Hi-Z 텍스처 크기 (mip 0)
    uint     g_HiZMipLevels;
    uint     g_InstanceCount;
};

StructuredBuffer<InstanceData>    g_Instances   : register(t0);
Texture2D<float>                  g_HiZTexture  : register(t1);
SamplerState                      g_SamplerPoint : register(s0);
RWStructuredBuffer<uint>          g_Visibility  : register(u0);

[numthreads(256, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= g_InstanceCount)
        return;

    // 이미 프러스텀에서 걸러진 인스턴스는 스킵
    if (g_Visibility[idx] == 0)
        return;

    InstanceData inst = g_Instances[idx];

    // 월드 AABB 계산
    float3 worldMin, worldMax;
    TransformAABB(inst.worldMatrix, inst.aabbMin, inst.aabbMax,
                  worldMin, worldMax);

    // AABB 8개 꼭짓점을 클립 공간으로 투영
    float minZ = 1.0f;
    float2 screenMin = float2(1.0f, 1.0f);
    float2 screenMax = float2(0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float3 corner = float3(
            (i & 1) ? worldMax.x : worldMin.x,
            (i & 2) ? worldMax.y : worldMin.y,
            (i & 4) ? worldMax.z : worldMin.z
        );

        float4 clipPos = mul(float4(corner, 1.0f), g_ViewProjection);
        // 카메라 뒤에 있는 경우 보수적으로 가시 처리
        if (clipPos.w <= 0.0f)
            return;  // 가시로 유지

        float3 ndc = clipPos.xyz / clipPos.w;
        float2 uv  = ndc.xy * float2(0.5f, -0.5f) + 0.5f;

        screenMin = min(screenMin, uv);
        screenMax = max(screenMax, uv);
        minZ      = min(minZ, ndc.z);
    }

    // 스크린 AABB 크기로 밉 레벨 선택
    float2 screenSize = (screenMax - screenMin) * g_HiZSize;
    float  maxDim     = max(screenSize.x, screenSize.y);
    uint   mipLevel   = clamp((uint)ceil(log2(maxDim)), 0, g_HiZMipLevels - 1);

    // Hi-Z 밉에서 최대 depth 샘플 (보수적)
    float2 mipSize = g_HiZSize / (float)(1 << mipLevel);
    uint2 minCoord = (uint2)(screenMin * mipSize);
    uint2 maxCoord = (uint2)(screenMax * mipSize);
    minCoord = clamp(minCoord, uint2(0,0), (uint2)(mipSize - 1));
    maxCoord = clamp(maxCoord, uint2(0,0), (uint2)(mipSize - 1));

    float maxHiZ = 0.0f;
    for (uint y = minCoord.y; y <= maxCoord.y; ++y)
        for (uint x = minCoord.x; x <= maxCoord.x; ++x)
            maxHiZ = max(maxHiZ, g_HiZTexture.mips[mipLevel][uint2(x, y)]);

    // 오클루전 테스트: 오브젝트의 최소 Z > Hi-Z 최대 depth → 가려짐
    if (minZ > maxHiZ)
        g_Visibility[idx] = 0;
}
```

#### 4.10.5 `Shaders/GPUDriven/LODSelect.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  LODSelect.hlsl  |  GPU LOD 선택 Compute Shader
//
//  카메라 거리 기반 LOD 레벨 결정.
//  MeshDescriptor의 lodDistances[] (거리^2)와 비교.
// ─────────────────────────────────────────────────────────────────

#include "Common.hlsli"

cbuffer CBLODSelect : register(b0)
{
    float3 g_CameraPos;
    uint   g_InstanceCount;
};

StructuredBuffer<InstanceData>    g_Instances   : register(t0);
StructuredBuffer<MeshDescriptor>  g_MeshDescs   : register(t1);
StructuredBuffer<uint>            g_Visibility  : register(t2);
RWStructuredBuffer<uint>          g_LODLevels   : register(u0);

[numthreads(256, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= g_InstanceCount)
        return;

    if (g_Visibility[idx] == 0)
    {
        g_LODLevels[idx] = 0;
        return;
    }

    InstanceData inst = g_Instances[idx];
    MeshDescriptor mesh = g_MeshDescs[inst.meshId];

    // 인스턴스 중심과 카메라 사이 거리^2
    float3 center = mul(float4(0, 0, 0, 1), inst.worldMatrix).xyz;
    float distSq = dot(center - g_CameraPos, center - g_CameraPos);

    // LOD 선택 (거리 비교)
    uint lod = 0;
    [unroll]
    for (uint i = 1; i < 4; ++i)
    {
        if (distSq > mesh.lodDistances[i] && mesh.lodDistances[i] > 0)
            lod = i;
    }

    g_LODLevels[idx] = lod;
}
```

#### 4.10.6 `Shaders/GPUDriven/CompactAndArgs.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  CompactAndArgs.hlsl  |  가시 인스턴스 압축 + Indirect Args 생성
//
//  가시 인스턴스를 연속 배열로 압축하고,
//  메시 그룹별 DrawIndexedInstancedIndirect 인자를 기록.
//
//  DX11 제약: AppendStructuredBuffer + RWByteAddressBuffer 동시 사용.
//  InterlockedAdd로 메시 그룹별 InstanceCount를 원자적 증가.
// ─────────────────────────────────────────────────────────────────

#include "Common.hlsli"

cbuffer CBCompact : register(b0)
{
    uint g_InstanceCount;
    uint g_MeshGroupCount;
    uint2 _pad;
};

StructuredBuffer<InstanceData>     g_Instances    : register(t0);
StructuredBuffer<uint>             g_Visibility   : register(t1);
StructuredBuffer<uint>             g_LODLevels    : register(t2);
StructuredBuffer<MeshDescriptor>   g_MeshDescs    : register(t3);

RWStructuredBuffer<CompactInstance> g_CompactOut  : register(u0);
RWByteAddressBuffer                g_IndirectArgs : register(u1);

// IndirectArgs 레이아웃 (per mesh group, 20 bytes each):
// [0] IndexCountPerInstance
// [1] InstanceCount          ← 여기에 InterlockedAdd
// [2] StartIndexLocation
// [3] BaseVertexLocation
// [4] StartInstanceLocation  ← 누적 오프셋

[numthreads(256, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x;
    if (idx >= g_InstanceCount)
        return;

    if (g_Visibility[idx] == 0)
        return;

    InstanceData inst = g_Instances[idx];
    uint meshId  = inst.meshId;
    uint lod     = g_LODLevels[idx];
    MeshDescriptor mesh = g_MeshDescs[meshId];

    // 이 메시 그룹의 IndirectArgs 내 InstanceCount를 원자적 증가
    uint argsOffset = meshId * 20;  // 5 * 4 bytes per group

    // IndexCountPerInstance 설정 (첫 스레드가 기록 — race condition 문제 없음,
    // 같은 meshId의 모든 스레드가 같은 값을 쓰므로)
    g_IndirectArgs.Store(argsOffset + 0, mesh.indexCountPerLOD[lod]);
    g_IndirectArgs.Store(argsOffset + 8, mesh.startIndexPerLOD[lod]);
    g_IndirectArgs.Store(argsOffset + 12, mesh.baseVertexPerLOD[lod]);

    // InstanceCount 원자적 증가 → 이 인스턴스의 컴팩트 슬롯 결정
    uint prevCount;
    g_IndirectArgs.InterlockedAdd(argsOffset + 4, 1, prevCount);

    // StartInstanceLocation 기반으로 글로벌 컴팩트 인덱스 계산
    // (초기화 시 CPU에서 각 그룹의 StartInstanceLocation을 미리 설정)
    uint startInstLoc;
    g_IndirectArgs.Load(argsOffset + 16, startInstLoc);
    // 주의: 위 Load는 race condition 없음 (CPU에서 미리 설정, 변경 안 됨)

    uint compactIdx = startInstLoc + prevCount;

    CompactInstance ci;
    ci.instanceId = idx;
    ci.meshId     = meshId;
    ci.lodLevel   = lod;
    ci.drawCallId = meshId;

    g_CompactOut[compactIdx] = ci;
}
```

**중요 참고**: `StartInstanceLocation`은 CPU에서 사전 계산해야 한다. 각 메시 그룹에 최대 몇 개 인스턴스가 올 수 있는지의 누적합을 `Reset()` 시 IndirectArgs에 기록한다.
대안: 2-pass 방식 (1st pass: 카운트만, 2nd pass: prefix sum + compact). 위 코드는 단순화된 1-pass 버전이다.

#### 4.10.7 `Shaders/GPUDriven/GPUDrivenVS.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  GPUDrivenVS.hlsl  |  GPU Driven 버텍스 셰이더
//
//  SV_InstanceID로 CompactInstances 인덱싱 → 원본 인스턴스 월드 행렬 참조.
//  InputLayout의 정점 데이터 + StructuredBuffer의 인스턴스 데이터 조합.
// ─────────────────────────────────────────────────────────────────

#include "Common.hlsli"

cbuffer CBPerFrame : register(b0)
{
    float4x4 g_ViewProjection;
};

StructuredBuffer<CompactInstance> g_CompactInstances : register(t0);
StructuredBuffer<InstanceData>    g_Instances        : register(t1);

struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 col    : COLOR;
    uint   instId : SV_InstanceID;
};

struct PSInput
{
    float4 pos        : SV_POSITION;
    float3 normal     : NORMAL;
    float4 col        : COLOR;
    uint   materialId : MATERIAL_ID;
};

PSInput VS(VSInput v)
{
    PSInput o;

    // SV_InstanceID → CompactInstance → 원본 인스턴스
    CompactInstance ci = g_CompactInstances[v.instId];
    InstanceData inst  = g_Instances[ci.instanceId];

    float4x4 world = inst.worldMatrix;

    // Local → World → Clip
    float4 worldPos = mul(float4(v.pos, 1.0f), world);
    o.pos = mul(worldPos, g_ViewProjection);

    // Normal (균등 스케일 가정)
    o.normal = normalize(mul(v.normal, (float3x3)world));

    o.col = v.col;
    o.materialId = inst.materialId;

    return o;
}
```

#### 4.10.8 `Shaders/GPUDriven/GPUDrivenPS.hlsl`

```hlsl
// ─────────────────────────────────────────────────────────────────
//  GPUDrivenPS.hlsl  |  GPU Driven 픽셀 셰이더
//
//  materialId로 머티리얼 테이블 인덱싱 (향후 텍스처 배열/바인들리스).
//  현재는 간단한 디퓨즈 라이팅 + 버텍스 컬러.
// ─────────────────────────────────────────────────────────────────

struct PSInput
{
    float4 pos        : SV_POSITION;
    float3 normal     : NORMAL;
    float4 col        : COLOR;
    uint   materialId : MATERIAL_ID;
};

float4 PS(PSInput p) : SV_TARGET
{
    // 간단한 디퓨즈 라이팅 (Default3D.hlsl과 동일 패턴)
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.3f));
    float  ndotl    = max(dot(p.normal, lightDir), 0.0f);

    float3 ambient = p.col.rgb * 0.3f;
    float3 diffuse = p.col.rgb * ndotl * 0.7f;

    return float4(ambient + diffuse, p.col.a);
}
```

---

## 5. 기존 파일 수정 사항

### 5.1 `Engine/Header/RHI/DX11/DX11ConstantBuffer.h` (L92 이후)

`BindCS()` 메서드 추가 필요:
```cpp
// 기존 L94-L98 (Bind 함수) 이후에 추가:
void BindCS(ID3D11DeviceContext* context, UINT slot) const
{
    assert(context && m_pBuffer);
    context->CSSetConstantBuffers(slot, 1, &m_pBuffer);
}
```

### 5.2 `Engine/Header/RHI/CDX11Device.h` (L56 이후)

깊이 버퍼를 SRV로 접근하기 위해 별도의 깊이 텍스처 포맷 변경이 필요:
```cpp
// 기존 L56 이후에 추가:
ID3D11Texture2D*          GetDepthTexture() const { return m_pDepthStencilBuffer; }
uint32 GetWidth()  const { return m_Width; }
uint32 GetHeight() const { return m_Height; }
```

### 5.3 `Engine/Code/RHI/DX11/CDX11Device.cpp` (L164-L182)

깊이 버퍼 포맷을 SRV 호환으로 변경 (Hi-Z에서 읽기 위해):
```cpp
// 수정 전 (L171):
dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

// 수정 후:
dsDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // SRV 호환
dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

// DSV 생성 시 포맷 명시 필요:
D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
hr = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &dsvDesc, &m_pDepthStencilView);

// 추가: 깊이 SRV 생성 (Hi-Z용)
D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MipLevels = 1;
hr = m_pDevice->CreateShaderResourceView(m_pDepthStencilBuffer, &srvDesc, &m_pDepthSRV);
```

이를 위해 `CDX11Device`에 새 멤버 추가:
```cpp
ID3D11ShaderResourceView* m_pDepthSRV = nullptr;
// + 접근자:
ID3D11ShaderResourceView* GetDepthSRV() const { return m_pDepthSRV; }
```

---

## 6. ECS 연동 (CGPURenderSystem)

ECS 시스템으로 구현하여 `CSystemSchedular`에 등록:

```cpp
// Engine/Header/ECS/Systems/RenderUploadSystem.h
class CGPURenderUploadSystem : public ISystem
{
public:
    CGPURenderUploadSystem(CGPUScene* pScene) : m_pScene(pScene) {}

    uint32_t GetPhase() const override { return 3; } // Phase 3: Renderer
    void Execute(CWorld& world, float fTimeDelta) override;
    const char* GetName() const override { return "GPURenderUpload"; }

private:
    CGPUScene* m_pScene = nullptr;
};
```

`Execute()` 구현:
```cpp
void CGPURenderUploadSystem::Execute(CWorld& world, float fTimeDelta)
{
    // TransformComponent + RenderComponent를 가진 엔티티 순회
    world.ForEach<TransformComponent, RenderComponent>(
        [this](EntityID e, TransformComponent& transform, RenderComponent& render)
        {
            if (render.gpuIndex < 0)
            {
                // 새 인스턴스 등록
                render.gpuIndex = m_pScene->AddInstance(
                    transform.worldMatrix.m, render.meshId, render.materialId);
            }
            else if (transform.dirty)
            {
                // 트랜스폼 변경 반영
                m_pScene->UpdateInstanceTransform(render.gpuIndex, transform.worldMatrix.m);
                transform.dirty = false;
            }
        }
    );
}
```

---

## 7. Render Graph 연동

Render Graph가 이미 존재한다고 가정하고, 다음과 같이 패스를 등록:

```
RenderGraph::Build()
{
    // GPU Driven Passes
    AddPass("FrustumCull",   [](RGBuilder& b) { /* CGPUCuller::Dispatch */ });
    AddPass("HiZGenerate",   [](RGBuilder& b) { /* CHiZBuffer::Generate */ });
    AddPass("OcclusionCull", [](RGBuilder& b) { /* CGPUOcclusionCuller::Dispatch */ });
    AddPass("LODSelect",     [](RGBuilder& b) { /* CGPULODSelector::Dispatch */ });
    AddPass("CompactArgs",   [](RGBuilder& b) { /* CIndirectRenderer::CompactAndGenerateArgs */ });
    AddPass("IndirectDraw",  [](RGBuilder& b) { /* CIndirectRenderer::DrawAllGroups */ });

    // 의존성 자동 추출 (UAV→SRV 전환으로)
    // FrustumCull → OcclusionCull (VisibilityBuffer)
    // HiZGenerate → OcclusionCull (HiZ Texture)
    // OcclusionCull → LODSelect (VisibilityBuffer)
    // LODSelect → CompactArgs (LODBuffer)
    // CompactArgs → IndirectDraw (CompactBuffer, IndirectArgs)
}
```

---

## 8. 성능 고려사항

### 8.1 메모리 예산

| 버퍼 | 인스턴스당 크기 | 10,000 인스턴스 | 비고 |
|------|----------------|----------------|------|
| InstanceBuffer | 96B | 960 KB | DYNAMIC, 매 프레임 업로드 |
| VisibilityBuffer | 4B | 40 KB | DEFAULT, GPU only |
| LODBuffer | 4B | 40 KB | DEFAULT, GPU only |
| CompactBuffer | 16B | 160 KB | DEFAULT, GPU only |
| IndirectArgs | 20B/group | ~1.2 KB (64 groups) | DEFAULT + DRAWINDIRECT |
| MeshDescBuffer | 96B/type | ~6 KB (64 types) | DYNAMIC |
| Hi-Z Texture | - | ~2 MB (1280x720 full mip) | DEFAULT |
| **총합** | | **~3.2 MB** | |

배틀로얄 급(10K 인스턴스, 64 메시 타입)에서도 ~3.2MB로 가벼운 편.

### 8.2 버퍼 업데이트 전략

- `InstanceBuffer`: `D3D11_USAGE_DYNAMIC` + `MAP_WRITE_DISCARD`. 매 프레임 전체 업로드. DX11에서 Persistent Mapping은 불가능하므로 DISCARD가 최선.
- 10,000 인스턴스 x 96B = ~960KB — DISCARD 업로드 ~0.1ms 이하.
- `MeshDescBuffer`: 메시 추가 시에만 업로드 (거의 불변).
- 메가버퍼: `IMMUTABLE`. 메시 등록 시 한 번만 생성.

### 8.3 스레드 그룹 크기

- 모든 컴퓨트 셰이더: `[numthreads(256, 1, 1)]` — DX11 GPU에서 1D 디스패치에 최적.
  - 10,000 인스턴스 → 40 그룹 디스패치. GPU에서 ~0.01ms.
- Hi-Z 생성: `[numthreads(16, 16, 1)]` — 2D 텍스처 연산에 적합.
  - 1280x720 → Mip 0: 80x45 그룹. 총 밉 체인 ~0.1ms.

### 8.4 DX11 제약 및 우회

1. **ExecuteIndirect 부재**: DX12의 `ExecuteIndirect`는 한 번의 API 호출로 여러 DrawIndirect를 수행하지만, DX11에서는 메시 그룹마다 `DrawIndexedInstancedIndirect()` 호출 필요. 그룹 수를 최소화하는 것이 핵심(메가버퍼 + 머티리얼 인덱싱).
2. **UAV 바인딩 제한**: DX11.0에서 UAV는 PS에서만 8개까지. DX11.1에서 CS는 제한 없음. Feature Level 11.1을 우선 타겟.
3. **UAV↔SRV 전환**: 같은 버퍼를 UAV로 쓴 후 SRV로 읽을 때, 반드시 이전 UAV를 언바인드한 후 SRV를 바인딩해야 함.

---

## 9. 구현 순서 (서브 페이즈)

### Phase 3A: RHI 기반 확장 (1~2일)
1. `DX11ComputeShader` 구현 + 테스트 (간단한 CS 컴파일/디스패치)
2. `DX11StructuredBuffer` 구현 + 테스트 (업로드/SRV 읽기)
3. `DX11ConstantBuffer`에 `BindCS()` 추가
4. `CDX11Device` 깊이 버퍼 SRV 호환으로 수정

### Phase 3B: GPU Scene + 메가버퍼 (2~3일)
1. `GPUDrivenTypes.h` 공유 구조체 정의
2. `RenderComponent` ECS 컴포넌트 추가
3. `CGPUScene` 구현 (인스턴스 관리, 메가버퍼)
4. 기존 CubeGeometry를 CGPUScene에 등록하여 테스트

### Phase 3C: GPU 프러스텀 컬링 (2일)
1. `Shaders/GPUDriven/Common.hlsli` + `FrustumCull.hlsl`
2. `CGPUCuller` 구현
3. 카메라 회전 시 오브젝트가 컬링되는지 시각적 확인
4. 성능 비교 (CPU 컬링 vs GPU 컬링)

### Phase 3D: Indirect Draw (2일)
1. `DX11IndirectArgsBuffer` 구현
2. `DX11AppendBuffer` 구현
3. `CompactAndArgs.hlsl` + `GPUDrivenVS.hlsl` + `GPUDrivenPS.hlsl`
4. `CIndirectRenderer` 구현
5. 100개 큐브를 GPU Driven으로 렌더링 테스트

### Phase 3E: Hi-Z + 오클루전 컬링 (3일)
1. `CHiZBuffer` 구현 + `HiZGenerate.hlsl`
2. Hi-Z 밉체인을 화면에 시각화하여 검증
3. `CGPUOcclusionCuller` 구현 + `OcclusionCull.hlsl`
4. 큰 벽 뒤의 오브젝트가 컬링되는지 확인

### Phase 3F: LOD 시스템 (1일)
1. `CGPULODSelector` 구현 + `LODSelect.hlsl`
2. 다른 LOD 메시 준비 (간단한 Sphere LOD: 고/중/저폴리)
3. 카메라 거리에 따라 LOD 전환 확인

### Phase 3G: ECS + Render Graph 통합 (2일)
1. `CGPURenderUploadSystem` 구현
2. Render Graph에 GPU Driven 패스 등록
3. 기존 CubeRenderer → GPU Driven Pipeline 전환
4. 1000개 오브젝트 스트레스 테스트

### Phase 3H: 최적화 + 프로파일링 (2일)
1. GPU 타이밍 쿼리 추가 (`ID3D11Query` TIMESTAMP)
2. 병목 지점 식별 및 최적화
3. 디버그 시각화 (컬링 결과 오버레이, LOD 컬러 표시)

**예상 총 소요 기간**: 약 15~17일

---

## 10. vcxproj.filters 배치

```xml
<!-- 00. Manager (RHI 확장) -->
<Filter Include="00. Manager\DX11">
  <ClInclude Include="Header\RHI\DX11\DX11ComputeShader.h" />
  <ClInclude Include="Header\RHI\DX11\DX11StructuredBuffer.h" />
  <ClInclude Include="Header\RHI\DX11\DX11IndirectArgsBuffer.h" />
  <ClInclude Include="Header\RHI\DX11\DX11AppendBuffer.h" />
  <ClCompile Include="Code\RHI\DX11\DX11ComputeShader.cpp" />
  <ClCompile Include="Code\RHI\DX11\DX11StructuredBuffer.cpp" />
  <ClCompile Include="Code\RHI\DX11\DX11IndirectArgsBuffer.cpp" />
  <ClCompile Include="Code\RHI\DX11\DX11AppendBuffer.cpp" />
</Filter>

<!-- 03. Renderer\GPUDriven -->
<Filter Include="03. Renderer\GPUDriven">
  <ClInclude Include="Header\Renderer\GPUDriven\GPUDrivenTypes.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CGPUScene.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CGPUCuller.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CHiZBuffer.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CGPUOcclusionCuller.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CGPULODSelector.h" />
  <ClInclude Include="Header\Renderer\GPUDriven\CIndirectRenderer.h" />
  <ClCompile Include="Code\Renderer\GPUDriven\CGPUScene.cpp" />
  <ClCompile Include="Code\Renderer\GPUDriven\CGPUCuller.cpp" />
  <ClCompile Include="Code\Renderer\GPUDriven\CHiZBuffer.cpp" />
  <ClCompile Include="Code\Renderer\GPUDriven\CGPUOcclusionCuller.cpp" />
  <ClCompile Include="Code\Renderer\GPUDriven\CGPULODSelector.cpp" />
  <ClCompile Include="Code\Renderer\GPUDriven\CIndirectRenderer.cpp" />
</Filter>

<!-- 05. ECS -->
<Filter Include="05. ECS\Components">
  <ClInclude Include="Header\ECS\Components\RenderComponent.h" />
</Filter>

<!-- Shaders -->
<Filter Include="Shaders\GPUDriven">
  <None Include="..\..\Shaders\GPUDriven\Common.hlsli" />
  <None Include="..\..\Shaders\GPUDriven\FrustumCull.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\HiZGenerate.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\OcclusionCull.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\LODSelect.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\CompactAndArgs.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\GPUDrivenVS.hlsl" />
  <None Include="..\..\Shaders\GPUDriven\GPUDrivenPS.hlsl" />
</Filter>
```

---

### Critical Files for Implementation
- `C:/Users/tnest/Desktop/Winters/Engine/Header/RHI/DX11/DX11ConstantBuffer.h` - BindCS() 메서드 추가가 필요하며, DX11StructuredBuffer 템플릿 패턴의 참고 대상
- `C:/Users/tnest/Desktop/Winters/Engine/Code/RHI/DX11/CDX11Device.cpp` - 깊이 버퍼 포맷을 R32_TYPELESS + SRV 호환으로 변경 필수 (Hi-Z 파이프라인 전제조건)
- `C:/Users/tnest/Desktop/Winters/Engine/Code/RHI/DX11/DX11Shader.cpp` - DX11ComputeShader 구현 시 CompileShader 패턴 재사용 대상
- `C:/Users/tnest/Desktop/Winters/Engine/Header/ECS/Components/CoreComponents.h` - RenderComponent 추가 위치, 기존 컴포넌트 패턴 참고
- `C:/Users/tnest/Desktop/Winters/Engine/Code/Renderer/CubeRenderer.cpp` - pImpl 패턴, 셰이더/버퍼/파이프라인 초기화 패턴의 참고 대상이며 향후 CIndirectRenderer로 대체될 렌더링 경로