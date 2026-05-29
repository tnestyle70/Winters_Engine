# Winters Engine — 리그오브레전드 30일 마스터 플랜

> **프로젝트**: Winters Engine 기반 LoL 스타일 MOBA 풀스택 구현
> **기간**: 30일 (D-Day 0 ~ D-Day 29)
> **목표**: 회원가입 → 로그인 → 상점 → 매치메이킹 → 5v5 게임 → 결과 DB 저장
> **기술 스택**: DX11 C++20 / Go Backend / PostgreSQL+Redis / Kafka / 커널 레벨 안티치트
> **기반**: 현재 Phase 1 완료 상태 (DX11 디바이스, Win32 윈도우, 기본 렌더링)

---

## Context — 왜 이 계획이 필요한가

마인크래프트 던전스 프로젝트(DX9)에서 CScene/CGameObject/CComponent/CVIBuffer 프레임워크,
IOCP 기반 서버, 에디터, CCD IK 드래곤 등을 구현한 경험이 있다.
이를 DX11 + 현대적 아키텍처로 업그레이드하여 LoL 수준의 멀티플레이어 MOBA를 구현한다.

### 던전스 프로젝트에서 가져올 것
- Scene/Layer/GameObject/Component 계층 패턴 (DX11용으로 재작성)
- Block 시스템의 QuadTree 프러스텀 컬링 → 미니언/챔피언 AOI에 활용
- 에디터 ImGui 패턴 → 맵 에디터 + 챔피언 에디터
- IOCP 서버 구조 → 게임 서버 기반
- 해상도 문제 해결 경험 → 설정 기반 동적 해상도 시스템

### 던전스에서 바꿀 것
- DX9 → DX11 (Deferred Rendering, Compute Shader)
- 수동 `PacketDef.h` → FlatBuffers 직렬화
- TCP 소켓 → UDP/KCP 신뢰성 레이어
- 단순 레퍼런스 카운팅 → RAII + smart_ptr
- 싱글 스레드 → Fiber JobSystem 병렬화

---

## Phase 0 — 모델링 추출 & 에셋 파이프라인 (D0~D2, 3일)

### 0-1. LoL 에셋 추출 도구 구축

**목표**: LoL 챔피언/맵 3D 모델을 엔진에서 사용 가능한 포맷으로 변환

```
도구 체인:
  1. Obsidian (LoL 에셋 추출기) → .skn/.skl/.anm 파일 추출
  2. lol2fbx / LoL2Blender 플러그인 → Blender로 임포트
  3. Blender Python 스크립트 → 일괄 변환 파이프라인
  4. 커스텀 WintersAssetConverter → 엔진 전용 포맷 (.wmesh, .wanim, .wmat)
```

**엔진 에셋 포맷 설계**:

```
Engine/Public/Resource/AssetFormat.h
```

```cpp
#pragma once
#include "WintersPCH.h"

// ── .wmesh (Winters Mesh) 바이너리 포맷 ──────────────────
#pragma pack(push, 1)
struct WMeshHeader
{
    char     magic[4]      = {'W','M','S','H'};  // 매직 넘버
    uint32_t version       = 1;
    uint32_t vertexCount   = 0;
    uint32_t indexCount    = 0;
    uint32_t submeshCount  = 0;
    uint32_t boneCount     = 0;
    uint32_t flags         = 0;  // bit0: hasSkeleton, bit1: hasBlendShapes
};

struct WMeshVertex
{
    float position[3];
    float normal[3];
    float tangent[4];    // xyz + handedness w
    float texcoord[2];
    uint8_t boneIndices[4];
    float   boneWeights[4];
};

struct WMeshSubmesh
{
    uint32_t indexStart;
    uint32_t indexCount;
    uint32_t materialIndex;
};

// ── .wanim (Winters Animation) 바이너리 포맷 ─────────────
struct WAnimHeader
{
    char     magic[4]      = {'W','A','N','M'};
    uint32_t version       = 1;
    uint32_t boneCount     = 0;
    uint32_t frameCount    = 0;
    float    duration      = 0.f;
    float    fps           = 30.f;
};

struct WAnimBoneFrame
{
    float translation[3];
    float rotation[4];     // quaternion xyzw
    float scale[3];
};

// ── .wmat (Winters Material) JSON 메타데이터 ──────────────
// {
//   "shader": "Shaders/Champion.hlsl",
//   "albedo": "Textures/Champions/Ahri/Base_Albedo.dds",
//   "normal": "Textures/Champions/Ahri/Base_Normal.dds",
//   "emission": "Textures/Champions/Ahri/Base_Emission.dds",
//   "params": { "metallic": 0.0, "roughness": 0.8 }
// }
#pragma pack(pop)
```

**Blender 일괄 변환 스크립트 (`Tools/blender_export.py`)**:

```python
# Tools/blender_export.py
# Blender 명령줄: blender --background --python blender_export.py -- input.fbx output.wmesh
import bpy, struct, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
input_path, output_path = argv[0], argv[1]

bpy.ops.import_scene.fbx(filepath=input_path)
obj = bpy.context.selected_objects[0]
mesh = obj.data
mesh.calc_normals_split()
mesh.calc_tangents()

vertices = []
indices = []
for poly in mesh.polygons:
    for loop_idx in poly.loop_indices:
        loop = mesh.loops[loop_idx]
        vert = mesh.vertices[loop.vertex_index]
        uv = mesh.uv_layers.active.data[loop_idx].uv if mesh.uv_layers.active else (0, 0)
        vertices.append({
            'pos': vert.co[:],
            'normal': loop.normal[:],
            'tangent': (*loop.tangent[:], loop.bitangent_sign),
            'uv': (uv[0], 1.0 - uv[1]),
            'bones': [0,0,0,0],
            'weights': [1,0,0,0]
        })
        indices.append(len(vertices) - 1)

with open(output_path, 'wb') as f:
    header = struct.pack('<4sIIIIII', b'WMSH', 1, len(vertices), len(indices), 1, 0, 0)
    f.write(header)
    for v in vertices:
        f.write(struct.pack('<3f3f4f2f4B4f',
            *v['pos'], *v['normal'], *v['tangent'], *v['uv'],
            *v['bones'], *v['weights']))
    for i in indices:
        f.write(struct.pack('<I', i))
    f.write(struct.pack('<III', 0, len(indices), 0))  # submesh

print(f"Exported {len(vertices)} verts, {len(indices)} indices to {output_path}")
```

### 0-2. 텍스처 파이프라인

```
원본 PNG/TGA → DirectXTex texconv.exe → BC7 DDS (Mipmap 포함)

배치 스크립트 (Tools/convert_textures.bat):
  for %%f in (Raw\Textures\*.png) do (
    texconv.exe -f BC7_UNORM -m 0 -y -o Textures\ %%f
  )

텍스처 타입별 압축:
  Albedo   → BC7_UNORM (고품질 4bpp)
  Normal   → BC5_UNORM (RG 채널, xy만 저장, z = sqrt(1-x²-y²))
  Roughness/AO → BC4_UNORM (단일 채널)
  Emission → BC7_UNORM_SRGB
```

### 0-3. 추가 파일

```
Tools/
├── blender_export.py          # Blender 일괄 변환
├── convert_textures.bat       # 텍스처 압축 배치
├── asset_manifest.json        # 에셋 목록 + 경로 매핑
└── WintersAssetConverter/     # C++ 커맨드라인 변환 도구
    ├── main.cpp
    └── CMakeLists.txt

Engine/Public/Resource/
├── AssetFormat.h              # 바이너리 포맷 정의
├── CMeshLoader.h              # .wmesh 로더
├── CAnimLoader.h              # .wanim 로더
├── CTextureLoader.h           # DDS/PNG 텍스처 로더 (DirectXTex)
└── CMaterialLoader.h          # .wmat JSON 머티리얼 로더

Engine/Private/Resource/
├── CMeshLoader.cpp
├── CAnimLoader.cpp
├── CTextureLoader.cpp
└── CMaterialLoader.cpp
```

---

## Phase 1 — Fiber JobSystem & 코어 시스템 강화 (D3~D5, 3일)

### 1-1. Fiber 기반 JobSystem 완성

**현재 상태**: Worker Thread Pool + 기본 Queue만 구현됨
**목표**: Naughty Dog GDC 2015 스타일 Fiber + Counter 의존성 그래프

```
Engine/Public/Core/JobSystem.h (재작성)
```

```cpp
#pragma once
#include "WintersPCH.h"

// ── Job 선언 ──────────────────────────────────────────────
using JobFunction = void(*)(void* pParam);

struct JobDecl
{
    JobFunction pfnEntry  = nullptr;
    void*       pParam    = nullptr;
};

// ── Atomic Counter (의존성 관리) ──────────────────────────
class CJobCounter
{
public:
    ~CJobCounter() = default;
    static std::unique_ptr<CJobCounter> Create(uint32_t initialValue = 0);

    void  Increment();
    void  Decrement();
    [[nodiscard]] uint32_t GetValue() const;

private:
    CJobCounter() = default;
    std::atomic<uint32_t> m_Value{0};
};

// ── Fiber JobSystem ───────────────────────────────────────
class CJobSystem
{
public:
    ~CJobSystem();

    static std::unique_ptr<CJobSystem> Create(uint32_t workerCount = 0);
    // workerCount = 0이면 (코어 수 - 2) 자동 설정

    // Job 제출: pCounter가 non-null이면 완료 시 Decrement
    void KickJobs(std::span<const JobDecl> jobs, CJobCounter* pCounter = nullptr);

    // 현재 Fiber를 대기 상태로 전환, counter가 targetValue에 도달하면 재개
    void WaitForCounter(CJobCounter* pCounter, uint32_t targetValue = 0);

    // 전체 대기 (프레임 끝)
    void WaitAll();

    [[nodiscard]] uint32_t GetWorkerCount() const;

private:
    CJobSystem() = default;

    struct Fiber;
    struct WorkerThread;

    static constexpr uint32_t FIBER_POOL_SIZE = 128;
    static constexpr uint32_t FIBER_STACK_SIZE = 64 * 1024;  // 64KB per fiber

    std::vector<std::unique_ptr<WorkerThread>> m_Workers;
    // Lock-Free Job Queue (Chase-Lev Work-Stealing Deque)
    // Fiber Pool (pre-allocated)
    // Waiting Fiber List (counter → fiber mapping)
};
```

**병렬화 프레임 구조**:

```
매 프레임 Job 그래프:

  [Input Poll]
       │
       ▼
  ┌────────────────┬──────────────────┬─────────────────┐
  │ Physics Update │ Animation Update │ AI BehaviorTree │
  │ (Jolt Physics) │ (Bone Transform) │ (FSM/BT Tick)   │
  └───────┬────────┴────────┬─────────┴────────┬────────┘
          │                 │                  │
          └─────────────────┼──────────────────┘
                            ▼
                    [ECS System Update]
                    (Transform, Velocity, Ability, Network)
                            │
                            ▼
                    [Render Submit]
                    (Visible 오브젝트 → RenderGraph)
                            │
                            ▼
                    [GPU Kick + Present]
                            │
                    [Network Send/Recv]
```

### 1-2. Memory Allocator (Linear / Pool)

```
Engine/Public/Core/CLinearAllocator.h
Engine/Public/Core/CPoolAllocator.h
```

```cpp
// ── Linear Allocator (프레임 임시 할당, 매 프레임 리셋) ────
class CLinearAllocator
{
public:
    ~CLinearAllocator() = default;
    static std::unique_ptr<CLinearAllocator> Create(size_t capacityBytes);

    [[nodiscard]] void* Allocate(size_t size, size_t alignment = 16);
    void Reset();  // 매 프레임 끝에 호출

    [[nodiscard]] size_t GetUsed() const;
    [[nodiscard]] size_t GetCapacity() const;

private:
    CLinearAllocator() = default;
    std::unique_ptr<uint8_t[]> m_pMemory;
    size_t m_Capacity = 0;
    size_t m_Offset   = 0;
};

// ── Pool Allocator (고정 크기 오브젝트 빈번한 생성/삭제) ────
template<typename T, size_t BlockSize = 256>
class CPoolAllocator
{
public:
    ~CPoolAllocator() = default;
    static std::unique_ptr<CPoolAllocator> Create();

    template<typename... Args>
    [[nodiscard]] T* Allocate(Args&&... args);
    void Deallocate(T* ptr);

    [[nodiscard]] size_t GetActiveCount() const;

private:
    CPoolAllocator() = default;
    struct Block { alignas(T) uint8_t data[sizeof(T)]; };
    std::vector<std::unique_ptr<Block[]>> m_Blocks;
    std::vector<T*> m_FreeList;
};
```

### 1-3. Event Bus (타입세이프 이벤트 시스템)

```
Engine/Public/Core/CEventBus.h
```

```cpp
#pragma once
#include "WintersPCH.h"

// ── 이벤트 정의 예시 ─────────────────────────────────────
struct DamageEvent    { uint32_t attackerId; uint32_t targetId; float damage; };
struct ChampKillEvent { uint32_t killerId; uint32_t victimId; };
struct TurretDestroyEvent { uint32_t teamId; uint32_t turretId; };

// ── 타입세이프 Event Bus ──────────────────────────────────
class CEventBus
{
public:
    ~CEventBus() = default;
    static std::unique_ptr<CEventBus> Create();

    template<typename TEvent>
    using Handler = std::function<void(const TEvent&)>;

    template<typename TEvent>
    uint32_t Subscribe(Handler<TEvent> handler);

    template<typename TEvent>
    void Unsubscribe(uint32_t handlerId);

    template<typename TEvent>
    void Publish(const TEvent& evt);

    // 지연 발행 (프레임 끝에 일괄 처리)
    template<typename TEvent>
    void PublishDeferred(const TEvent& evt);

    void FlushDeferred();

private:
    CEventBus() = default;
    // type_index → vector<function<void(const void*)>> 매핑
};
```

---

## Phase 2 — Render Graph & Deferred Pipeline (D6~D10, 5일)

### 2-1. RenderGraph 시스템

**목표**: 렌더 패스를 그래프로 선언하고, 자동으로 리소스 생명주기 + 배리어 관리

```
Engine/Public/RHI/CRenderGraph.h
```

```cpp
#pragma once
#include "WintersPCH.h"

// ── 리소스 핸들 (그래프 내부 가상 리소스) ──────────────────
struct RGTextureHandle { uint32_t id; };
struct RGBufferHandle  { uint32_t id; };

// ── 렌더 패스 빌더 ───────────────────────────────────────
class CRenderPassBuilder
{
public:
    // 입력 (읽기 전용)
    RGTextureHandle Read(RGTextureHandle tex);
    RGBufferHandle  Read(RGBufferHandle buf);

    // 출력 (쓰기)
    RGTextureHandle Write(RGTextureHandle tex);
    RGTextureHandle CreateTexture(const D3D11_TEXTURE2D_DESC& desc);
    RGBufferHandle  CreateBuffer(const D3D11_BUFFER_DESC& desc);

    // 깊이 스텐실
    RGTextureHandle UseDepth(RGTextureHandle depth);

private:
    friend class CRenderGraph;
    struct PassData;
};

// ── 렌더 그래프 ──────────────────────────────────────────
class CRenderGraph
{
public:
    ~CRenderGraph() = default;
    static std::unique_ptr<CRenderGraph> Create(class CDX11Device* pDevice);

    // 패스 등록 (Lambda로 실행 로직 전달)
    template<typename TData>
    TData& AddPass(
        const char* name,
        std::function<void(CRenderPassBuilder&, TData&)> setup,
        std::function<void(const TData&, ID3D11DeviceContext*)> execute
    );

    // 그래프 컴파일 + 실행
    void Compile();
    void Execute(ID3D11DeviceContext* pContext);

    // 백버퍼 핸들
    [[nodiscard]] RGTextureHandle GetBackBuffer() const;

private:
    CRenderGraph() = default;
    CDX11Device* m_pDevice = nullptr;
    // Pass DAG, Resource Pool, Barrier 관리
};
```

### 2-2. G-Buffer 패스 (Deferred Rendering)

```
Shaders/GBuffer.hlsl
```

```hlsl
// ── G-Buffer 생성 셰이더 ──────────────────────────────────

cbuffer CBPerFrame : register(b0)
{
    float4x4 g_ViewProj;
    float4x4 g_View;
    float3   g_CameraPos;
    float    g_Time;
};

cbuffer CBPerObject : register(b1)
{
    float4x4 g_World;
    float4x4 g_WorldInvTranspose;
};

// 텍스처
Texture2D    g_AlbedoTex   : register(t0);
Texture2D    g_NormalTex   : register(t1);
Texture2D    g_RoughnessTex: register(t2);
SamplerState g_Sampler     : register(s0);

struct VS_IN
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float4 tangent : TANGENT;
    float2 uv      : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH    : SV_POSITION;
    float3 posW    : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float3 tangentW: TEXCOORD2;
    float3 bitanW  : TEXCOORD3;
    float2 uv      : TEXCOORD4;
};

VS_OUT VS_GBuffer(VS_IN input)
{
    VS_OUT output;
    float4 worldPos = mul(float4(input.pos, 1.0), g_World);
    output.posH     = mul(worldPos, g_ViewProj);
    output.posW     = worldPos.xyz;
    output.normalW  = normalize(mul(input.normal, (float3x3)g_WorldInvTranspose));
    output.tangentW = normalize(mul(input.tangent.xyz, (float3x3)g_World));
    output.bitanW   = cross(output.normalW, output.tangentW) * input.tangent.w;
    output.uv       = input.uv;
    return output;
}

struct PS_OUT
{
    float4 albedo  : SV_TARGET0;   // RT0: RGB=Albedo, A=Metallic
    float4 normal  : SV_TARGET1;   // RT1: RGB=WorldNormal (octahedral), A=unused
    float4 roughAO : SV_TARGET2;   // RT2: R=Roughness, G=AO, B=Emissive, A=unused
};

// 옥타헤드럴 노말 인코딩 ([-1,1]³ → [0,1]²)
float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    return n.xy * 0.5 + 0.5;
}

PS_OUT PS_GBuffer(VS_OUT input)
{
    PS_OUT output;

    // 탄젠트 스페이스 → 월드 노말
    float3x3 TBN = float3x3(input.tangentW, input.bitanW, input.normalW);
    float3 normalTS = g_NormalTex.Sample(g_Sampler, input.uv).xyz * 2.0 - 1.0;
    float3 normalW  = normalize(mul(normalTS, TBN));

    float4 albedo = g_AlbedoTex.Sample(g_Sampler, input.uv);
    float roughness = g_RoughnessTex.Sample(g_Sampler, input.uv).r;

    output.albedo  = float4(albedo.rgb, 0.0);  // metallic = 0 (비금속)
    output.normal  = float4(OctEncode(normalW), 0.0, 1.0);
    output.roughAO = float4(roughness, 1.0, 0.0, 1.0);

    return output;
}
```

### 2-3. Clustered Deferred Lighting (Compute Shader)

```
Shaders/ClusteredLighting.hlsl
```

```hlsl
// ── Clustered Deferred Lighting Compute Shader ────────────

#define CLUSTER_X 16
#define CLUSTER_Y 9
#define CLUSTER_Z 24
#define MAX_LIGHTS_PER_CLUSTER 64

struct PointLight
{
    float3 position;
    float  radius;
    float3 color;
    float  intensity;
};

// G-Buffer 입력
Texture2D<float4> g_GBufAlbedo  : register(t0);
Texture2D<float4> g_GBufNormal  : register(t1);
Texture2D<float4> g_GBufRoughAO : register(t2);
Texture2D<float>  g_DepthBuffer : register(t3);

// 라이트 데이터
StructuredBuffer<PointLight> g_Lights       : register(t4);
StructuredBuffer<uint>       g_LightIndices : register(t5);
StructuredBuffer<uint2>      g_ClusterGrid  : register(t6);
// ClusterGrid[clusterIdx] = (offset, count)

// 출력
RWTexture2D<float4> g_OutputHDR : register(u0);

cbuffer CBLighting : register(b0)
{
    float4x4 g_InvViewProj;
    float3   g_CameraPos;
    uint     g_LightCount;
    float2   g_ScreenSize;
    float    g_NearZ;
    float    g_FarZ;
};

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y *= -1.0;
    float4 worldPos = mul(clipPos, g_InvViewProj);
    return worldPos.xyz / worldPos.w;
}

float3 CookTorranceBRDF(float3 N, float3 V, float3 L, float3 albedo, float roughness, float metallic)
{
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a = roughness * roughness;
    float a2 = a * a;

    // GGX NDF
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * denom * denom);

    // Schlick Fresnel
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Smith GGX
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));

    float3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.001);
    float3 diffuse  = (1.0 - F) * (1.0 - metallic) * albedo / 3.14159;

    return (diffuse + specular) * NdotL;
}

[numthreads(8, 8, 1)]
void CS_ClusteredLighting(uint3 dispatchID : SV_DispatchThreadID)
{
    float2 uv = (float2(dispatchID.xy) + 0.5) / g_ScreenSize;
    float depth = g_DepthBuffer[dispatchID.xy];
    if (depth >= 1.0) { g_OutputHDR[dispatchID.xy] = float4(0.1, 0.1, 0.15, 1.0); return; }

    float3 worldPos = ReconstructWorldPos(uv, depth);
    float4 albedoM  = g_GBufAlbedo[dispatchID.xy];
    float4 normalEnc = g_GBufNormal[dispatchID.xy];
    float4 roughAO  = g_GBufRoughAO[dispatchID.xy];

    float3 albedo   = albedoM.rgb;
    float  metallic = albedoM.a;
    float3 N        = OctDecode(normalEnc.rg);
    float  roughness = roughAO.r;
    float  ao       = roughAO.g;
    float3 V        = normalize(g_CameraPos - worldPos);

    // 클러스터 인덱스 계산
    uint clusterX = dispatchID.x / (g_ScreenSize.x / CLUSTER_X);
    uint clusterY = dispatchID.y / (g_ScreenSize.y / CLUSTER_Y);
    float linearDepth = g_NearZ * g_FarZ / (g_FarZ - depth * (g_FarZ - g_NearZ));
    uint clusterZ = uint(log(linearDepth / g_NearZ) / log(g_FarZ / g_NearZ) * CLUSTER_Z);
    uint clusterIdx = clusterX + clusterY * CLUSTER_X + clusterZ * CLUSTER_X * CLUSTER_Y;

    uint2 clusterData = g_ClusterGrid[clusterIdx];
    uint offset = clusterData.x;
    uint count  = clusterData.y;

    float3 totalLight = float3(0, 0, 0);

    // Ambient
    totalLight += albedo * 0.03 * ao;

    // 클러스터 내 라이트만 순회
    for (uint i = 0; i < count && i < MAX_LIGHTS_PER_CLUSTER; ++i)
    {
        uint lightIdx = g_LightIndices[offset + i];
        PointLight light = g_Lights[lightIdx];

        float3 L = light.position - worldPos;
        float dist = length(L);
        if (dist > light.radius) continue;
        L /= dist;

        float attenuation = 1.0 - saturate(dist / light.radius);
        attenuation *= attenuation;

        totalLight += CookTorranceBRDF(N, V, L, albedo, roughness, metallic)
                    * light.color * light.intensity * attenuation;
    }

    g_OutputHDR[dispatchID.xy] = float4(totalLight, 1.0);
}
```

### 2-4. PostFX 파이프라인

```
Shaders/PostFX/
├── Bloom.hlsl         # Threshold → Downsample → Gaussian Blur → Upsample → Composite
├── ToneMapping.hlsl   # ACES Filmic Tone Mapping
├── FXAA.hlsl          # Fast Approximate Anti-Aliasing
├── SSAO.hlsl          # Screen-Space Ambient Occlusion (Horizon-Based)
├── Fog.hlsl           # Distance + Height Fog (소환사의 협곡 안개)
└── ColorGrading.hlsl  # LUT 기반 색보정
```

```hlsl
// Shaders/PostFX/ToneMapping.hlsl
// ACES Filmic Tone Mapping

Texture2D<float4> g_HDRInput : register(t0);
RWTexture2D<float4> g_LDROutput : register(u0);

float3 ACESFilm(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

[numthreads(8, 8, 1)]
void CS_ToneMap(uint3 id : SV_DispatchThreadID)
{
    float3 hdr = g_HDRInput[id.xy].rgb;
    // Exposure
    hdr *= 1.2;
    // Tone map
    float3 ldr = ACESFilm(hdr);
    // Gamma correction
    ldr = pow(ldr, 1.0/2.2);
    g_LDROutput[id.xy] = float4(ldr, 1.0);
}
```

### 2-5. Shadow System (Cascaded Shadow Maps)

```
4단계 캐스케이드:
  Cascade 0: 0~10m   (2048x2048) — 챔피언 + 미니언 그림자
  Cascade 1: 10~25m  (2048x2048) — 중거리 오브젝트
  Cascade 2: 25~60m  (1024x1024) — 터렛, 건물
  Cascade 3: 60~150m (1024x1024) — 원거리 지형

PCF (Percentage Closer Filtering) 4x4 소프트 섀도우
```

### 2-6. 추가 파일

```
Engine/Public/RHI/
├── CRenderGraph.h
├── CRenderGraph.cpp
├── CGBuffer.h              # G-Buffer MRT 관리
├── CShadowMap.h            # Cascaded Shadow Map
├── CPostFXPipeline.h       # PostFX 체인 관리
└── CClusteredLightMgr.h    # 클러스터 빌드 + 라이트 컬링

Engine/Private/RHI/
├── CRenderGraph.cpp
├── CGBuffer.cpp
├── CShadowMap.cpp
├── CPostFXPipeline.cpp
└── CClusteredLightMgr.cpp
```

---

## Phase 3 — GPU-Driven Rendering & Profiling (D11~D13, 3일)

### 3-1. GPU-Driven Rendering

**목표**: CPU 부하를 최소화하고 GPU에서 컬링/드로우콜 생성

```
파이프라인:
  1. [CPU] 전체 오브젝트 AABB를 StructuredBuffer에 업로드
  2. [GPU Compute] 프러스텀 컬링 + 오클루전 컬링
  3. [GPU Compute] Indirect Draw Arguments 생성
  4. [GPU] ExecuteIndirect로 일괄 드로우
```

```
Engine/Public/RHI/DX11/
├── DX11IndirectDraw.h      # ID3D11Buffer (DrawIndexedInstancedIndirect)
└── DX11GPUCuller.h         # Compute Shader 기반 컬링
```

```hlsl
// Shaders/GPUCull.hlsl — 프러스텀 컬링 Compute Shader

struct ObjectData
{
    float4x4 world;
    float3   aabbMin;
    float    pad0;
    float3   aabbMax;
    float    pad1;
    uint     meshId;
    uint     materialId;
    uint2    pad2;
};

struct DrawArgs
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
};

StructuredBuffer<ObjectData>  g_Objects    : register(t0);
RWStructuredBuffer<DrawArgs>  g_DrawArgs   : register(u0);
RWStructuredBuffer<uint>      g_VisibleList: register(u1);
RWByteAddressBuffer           g_Counter    : register(u2);

cbuffer CBCull : register(b0)
{
    float4 g_FrustumPlanes[6];
    uint   g_ObjectCount;
};

bool FrustumTest(float3 aabbMin, float3 aabbMax)
{
    for (int i = 0; i < 6; ++i)
    {
        float3 p;
        p.x = (g_FrustumPlanes[i].x > 0) ? aabbMax.x : aabbMin.x;
        p.y = (g_FrustumPlanes[i].y > 0) ? aabbMax.y : aabbMin.y;
        p.z = (g_FrustumPlanes[i].z > 0) ? aabbMax.z : aabbMin.z;
        if (dot(float4(p, 1.0), g_FrustumPlanes[i]) < 0.0)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void CS_FrustumCull(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_ObjectCount) return;

    ObjectData obj = g_Objects[id.x];

    // AABB를 월드 공간으로 변환
    float3 worldMin = mul(float4(obj.aabbMin, 1.0), obj.world).xyz;
    float3 worldMax = mul(float4(obj.aabbMax, 1.0), obj.world).xyz;

    if (FrustumTest(min(worldMin, worldMax), max(worldMin, worldMax)))
    {
        uint idx;
        g_Counter.InterlockedAdd(0, 1, idx);
        g_VisibleList[idx] = id.x;
    }
}
```

### 3-2. Profiling Tool (엔진 내장 프로파일러)

```
Engine/Public/Core/CProfiler.h
```

```cpp
#pragma once
#include "WintersPCH.h"

// ── 프로파일 스코프 매크로 ────────────────────────────────
#define PROFILE_SCOPE(name) \
    auto _profScope##__LINE__ = CProfiler::Get().BeginScope(name)

#define PROFILE_GPU_SCOPE(ctx, name) \
    auto _gpuScope##__LINE__ = CProfiler::Get().BeginGPUScope(ctx, name)

// ── CPU/GPU 타이밍 프로파일러 ─────────────────────────────
class CProfiler
{
public:
    ~CProfiler() = default;
    static CProfiler& Get();  // 싱글톤

    void Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

    // CPU 타이밍
    struct ScopeGuard
    {
        ~ScopeGuard();
        const char* name;
        std::chrono::high_resolution_clock::time_point start;
    };
    [[nodiscard]] ScopeGuard BeginScope(const char* name);

    // GPU 타이밍 (ID3D11Query::Timestamp)
    struct GPUScopeGuard
    {
        ~GPUScopeGuard();
        const char* name;
        ID3D11DeviceContext* pContext;
        ID3D11Query* pBeginQuery;
        ID3D11Query* pEndQuery;
    };
    [[nodiscard]] GPUScopeGuard BeginGPUScope(ID3D11DeviceContext* pCtx, const char* name);

    // 결과 수집 (프레임 끝)
    struct ProfileEntry
    {
        const char* name;
        float cpuMs;
        float gpuMs;
        uint32_t drawCalls;
    };
    [[nodiscard]] std::span<const ProfileEntry> GetLastFrameResults() const;

    // ImGui 오버레이 렌더
    void RenderOverlay();

    // 프레임 통계
    [[nodiscard]] float GetFPS() const;
    [[nodiscard]] float GetFrameTimeMs() const;
    [[nodiscard]] uint32_t GetTotalDrawCalls() const;
    [[nodiscard]] uint32_t GetTotalTriangles() const;

private:
    CProfiler() = default;
    // GPU Query Pool (더블 버퍼링: 이전 프레임 결과 읽기)
    // CPU 타이밍 스택
    // ImGui 히스토그램 데이터
};
```

**프로파일러 ImGui 오버레이**:
```
┌─ Winters Profiler ──────────────────────────┐
│ FPS: 144.2 | Frame: 6.93ms                 │
│ Draw Calls: 342 | Triangles: 1.2M          │
│                                             │
│ ▓▓▓▓▓▓▓▓░░░░ GBuffer Pass    2.1ms (GPU)  │
│ ▓▓▓▓░░░░░░░░ Shadow Pass     1.3ms (GPU)  │
│ ▓▓▓░░░░░░░░░ Lighting CS     0.9ms (GPU)  │
│ ▓▓░░░░░░░░░░ PostFX          0.6ms (GPU)  │
│ ▓░░░░░░░░░░░ UI/ImGui        0.2ms (GPU)  │
│                                             │
│ ▓▓▓▓░░░░░░░░ Physics         1.1ms (CPU)  │
│ ▓▓▓░░░░░░░░░ Animation       0.8ms (CPU)  │
│ ▓▓░░░░░░░░░░ ECS Systems     0.5ms (CPU)  │
│ ▓░░░░░░░░░░░ Network         0.3ms (CPU)  │
└─────────────────────────────────────────────┘
```

### 3-3. 해상도 설정 시스템

**던전스 프로젝트에서의 문제**: 4개 클라이언트를 로컬에서 띄울 때 해상도가 맞지 않아 UV 값 깨짐

```
Engine/Public/Platform/CDisplaySettings.h
```

```cpp
#pragma once
#include "WintersPCH.h"

struct DisplayMode
{
    uint32_t width;
    uint32_t height;
    uint32_t refreshRate;
    const wchar_t* displayName;  // "1920x1080 (60Hz)"
};

enum class WindowMode : uint8_t
{
    Fullscreen,          // 전체화면 (배타적)
    BorderlessWindowed,  // 전체화면 창모드
    Windowed             // 창모드
};

enum class RenderScale : uint8_t
{
    Scale_50  = 50,
    Scale_75  = 75,
    Scale_100 = 100,
    Scale_125 = 125,
    Scale_150 = 150
};

class CDisplaySettings
{
public:
    ~CDisplaySettings() = default;
    static std::unique_ptr<CDisplaySettings> Create(IDXGIFactory1* pFactory);

    // 사용 가능한 해상도 열거 (모니터 지원 목록)
    [[nodiscard]] std::span<const DisplayMode> GetAvailableModes() const;

    // 현재 설정
    [[nodiscard]] uint32_t   GetWidth() const;
    [[nodiscard]] uint32_t   GetHeight() const;
    [[nodiscard]] WindowMode GetWindowMode() const;
    [[nodiscard]] RenderScale GetRenderScale() const;

    // 설정 변경 (SwapChain 리사이즈 자동 처리)
    bool ApplyResolution(uint32_t width, uint32_t height);
    bool ApplyWindowMode(WindowMode mode);
    bool ApplyRenderScale(RenderScale scale);

    // 내부 렌더 해상도 (RenderScale 적용 후)
    [[nodiscard]] uint32_t GetRenderWidth() const;
    [[nodiscard]] uint32_t GetRenderHeight() const;

    // 설정 파일 저장/로드
    bool SaveToFile(const wchar_t* path);
    bool LoadFromFile(const wchar_t* path);

private:
    CDisplaySettings() = default;
    std::vector<DisplayMode> m_AvailableModes;
    uint32_t   m_Width       = 1920;
    uint32_t   m_Height      = 1080;
    WindowMode m_WindowMode  = WindowMode::Windowed;
    RenderScale m_RenderScale = RenderScale::Scale_100;
    // SwapChain resize callback
};
```

**설정 파일 포맷 (`Config/display.json`)**:
```json
{
    "width": 1920,
    "height": 1080,
    "windowMode": "windowed",
    "renderScale": 100,
    "vsync": true,
    "fpsLimit": 144
}
```

---

## Phase 4 — 네트워크 & 게임 서버 (D14~D19, 6일)

### 4-1. UDP/KCP 전송 계층

```
Engine/Public/Network/
├── CUDPSocket.h           # Raw UDP 소켓 래퍼
├── CKCPTransport.h        # KCP 신뢰성 레이어 (ARQ)
├── CPacketSerializer.h    # FlatBuffers 직렬화/역직렬화
├── CClientNet.h           # 클라이언트 네트워크 매니저
└── CNetworkPrediction.h   # 클라이언트 예측 + 서버 보정

Engine/Private/Network/
├── CUDPSocket.cpp
├── CKCPTransport.cpp
├── CPacketSerializer.cpp
├── CClientNet.cpp
└── CNetworkPrediction.cpp
```

```cpp
// Engine/Public/Network/CClientNet.h
#pragma once
#include "WintersPCH.h"

class CClientNet
{
public:
    ~CClientNet() = default;
    static std::unique_ptr<CClientNet> Create();

    // 연결
    bool Connect(const char* serverIP, uint16_t port, const char* authToken);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const;

    // 매 프레임 호출
    void Update(float dt);  // Recv + 패킷 디스패치

    // 입력 전송 (클라이언트 예측 시스템과 연동)
    void SendInput(const struct PlayerInput& input);

    // 스냅샷 수신 콜백 등록
    using SnapshotCallback = std::function<void(const struct WorldSnapshot&)>;
    void OnSnapshot(SnapshotCallback cb);

    // RTT / 패킷 손실률
    [[nodiscard]] float GetRTT() const;
    [[nodiscard]] float GetPacketLoss() const;

    // 게임 이벤트 전송
    void SendAbilityCast(uint32_t abilityId, const Vec3& targetPos);
    void SendChat(const char* message);
    void SendPing(const Vec3& worldPos, uint8_t pingType);

private:
    CClientNet() = default;
    std::unique_ptr<class CKCPTransport> m_pTransport;
    // 입력 히스토리 (서버 보정용)
    // 스냅샷 버퍼 (보간용)
};
```

### 4-2. FlatBuffers 패킷 스키마

```
Shared/Schemas/
├── common.fbs           # 공용 타입
├── auth.fbs             # 인증 패킷
├── game_input.fbs       # 게임 입력
├── game_state.fbs       # 월드 스냅샷
├── ability.fbs          # 스킬 시스템
├── chat.fbs             # 채팅
└── matchmaking.fbs      # 매치메이킹
```

```fbs
// Shared/Schemas/game_state.fbs
namespace Winters.Net;

struct Vec3f { x:float; y:float; z:float; }
struct Vec2f { x:float; y:float; }

enum ChampionState : byte {
    Idle, Moving, Casting, Dead, Recalling
}

table ChampionSnapshot {
    entity_id:uint32;
    owner_id:uint32;
    position:Vec3f;
    rotation:float;       // Y축 회전 (라디안)
    velocity:Vec2f;
    hp:float;
    max_hp:float;
    mana:float;
    level:uint8;
    state:ChampionState;
    ability_cooldowns:[float];  // 4개 스킬 쿨다운
}

table MinionSnapshot {
    entity_id:uint32;
    position:Vec3f;
    hp:float;
    team:uint8;
}

table TurretSnapshot {
    entity_id:uint32;
    hp:float;
    team:uint8;
    target_id:uint32;     // 현재 공격 대상
}

table ProjectileSnapshot {
    entity_id:uint32;
    position:Vec3f;
    direction:Vec3f;
    speed:float;
    owner_id:uint32;
}

table WorldSnapshot {
    server_tick:uint32;
    timestamp:double;
    champions:[ChampionSnapshot];
    minions:[MinionSnapshot];
    turrets:[TurretSnapshot];
    projectiles:[ProjectileSnapshot];
}

root_type WorldSnapshot;
```

### 4-3. 게임 서버 (C++ IOCP)

```
Server/Public/
├── CIOCPServer.h          # IOCP 네트워크 코어
├── CSession.h             # 클라이언트 세션
├── CSessionMgr.h          # 세션 풀 관리
├── CGameRoom.h            # 게임 룸 (5v5 매치)
├── CGameLogic.h           # 서버 권위 게임 로직
├── CServerWorld.h         # 서버 사이드 ECS World
├── CAOIManager.h          # Area of Interest (50m 그리드)
├── CLagCompensation.h     # 지연 보상 (히스토리 롤백)
├── CPacketHandler.h       # 패킷 라우팅
├── CAntiCheatServer.h     # 서버사이드 치트 감지
└── CServerConfig.h        # 서버 설정

Server/Private/
├── main.cpp               # 서버 엔트리
├── CIOCPServer.cpp
├── CSession.cpp
├── CSessionMgr.cpp
├── CGameRoom.cpp
├── CGameLogic.cpp
├── CServerWorld.cpp
├── CAOIManager.cpp
├── CLagCompensation.cpp
├── CPacketHandler.cpp
└── CAntiCheatServer.cpp
```

```cpp
// Server/Public/CGameRoom.h
#pragma once

class CGameRoom
{
public:
    ~CGameRoom() = default;
    static std::unique_ptr<CGameRoom> Create(uint32_t roomId, uint8_t maxPlayers = 10);

    // 게임 루프 (20 TPS)
    void Tick(float dt);

    // 플레이어 입/퇴장
    bool AddPlayer(class CSession* pSession, uint8_t team, uint32_t championId);
    void RemovePlayer(uint32_t playerId);

    // 게임 상태
    enum class RoomState : uint8_t
    {
        WaitingPlayers,   // 로딩 대기
        ChampSelect,      // 챔피언 선택
        Loading,          // 인게임 로딩
        InGame,           // 게임 진행
        PostGame          // 결과 화면
    };
    [[nodiscard]] RoomState GetState() const;

    // 게임 이벤트
    void OnPlayerInput(uint32_t playerId, const struct PlayerInput& input);
    void OnAbilityCast(uint32_t playerId, uint32_t abilityId, const Vec3& target);

    // AOI 기반 스냅샷 전송
    void BroadcastSnapshots();

private:
    CGameRoom() = default;
    uint32_t m_RoomId = 0;
    RoomState m_State = RoomState::WaitingPlayers;
    std::unique_ptr<class CServerWorld> m_pWorld;
    std::unique_ptr<class CAOIManager>  m_pAOI;
    std::unique_ptr<class CLagCompensation> m_pLagComp;
    // 타이머: 게임 시간, 미니언 스폰, 드래곤 리스폰 등
};
```

### 4-4. 클라이언트 예측 + 서버 보정

```cpp
// Engine/Public/Network/CNetworkPrediction.h
#pragma once

class CNetworkPrediction
{
public:
    ~CNetworkPrediction() = default;
    static std::unique_ptr<CNetworkPrediction> Create();

    // 1. 로컬 입력 → 즉시 예측 적용
    void ApplyInputLocally(const struct PlayerInput& input, struct CharacterState& state);

    // 2. 입력을 히스토리에 저장 (서버 ACK 전까지 보관)
    void RecordInput(uint32_t inputSequence, const PlayerInput& input);

    // 3. 서버 스냅샷 수신 시 보정
    void Reconcile(uint32_t lastProcessedInput, const CharacterState& serverState);

    // 4. 보정 후 미처리 입력 재적용
    void ReplayPendingInputs(CharacterState& state);

    // 다른 플레이어 보간
    void InterpolateRemote(const CharacterState& from, const CharacterState& to, 
                           float alpha, CharacterState& result);

private:
    CNetworkPrediction() = default;
    struct InputRecord { uint32_t sequence; PlayerInput input; CharacterState predictedState; };
    std::deque<InputRecord> m_InputHistory;
    uint32_t m_LastAckedSequence = 0;
};
```

---

## Phase 5 — Go 백엔드 & DB (D20~D23, 4일)

### 5-1. 마이크로서비스 아키텍처

```
Services/
├── docker-compose.yml      # 전체 인프라 (PostgreSQL, Redis, Kafka, 서비스들)
├── go.mod
│
├── cmd/                    # 서비스 엔트리포인트
│   ├── auth/main.go        # 인증 서비스 (JWT)
│   ├── matchmaking/main.go # 매치메이킹 서비스 (MMR/Elo)
│   ├── shop/main.go        # 상점 서비스 (카탈로그, 구매)
│   ├── profile/main.go     # 프로필 서비스 (전적, 통계)
│   ├── gateway/main.go     # API 게이트웨이 (라우팅, Rate Limit)
│   └── game-coord/main.go  # 게임 서버 코디네이터 (방 할당)
│
├── internal/
│   ├── auth/
│   │   ├── handler.go      # HTTP 핸들러 (Register, Login, Refresh, Logout)
│   │   ├── service.go      # 비즈니스 로직
│   │   ├── repository.go   # PostgreSQL 쿼리
│   │   └── model.go        # 데이터 모델
│   │
│   ├── matchmaking/
│   │   ├── handler.go      # 큐 진입/이탈 API
│   │   ├── service.go      # MMR 매칭 알고리즘
│   │   ├── queue.go        # Redis 기반 대기열
│   │   └── model.go        # 매치 데이터
│   │
│   ├── shop/
│   │   ├── handler.go      # 상점 API (목록, 구매, 인벤토리)
│   │   ├── service.go      # 구매 로직 (잔액 체크, 트랜잭션)
│   │   ├── repository.go   # DB 쿼리
│   │   └── model.go        # 아이템, 스킨, 챔피언 데이터
│   │
│   ├── profile/
│   │   ├── handler.go      # 전적 API
│   │   ├── service.go      # 통계 계산
│   │   └── repository.go   # DB 쿼리
│   │
│   └── game/
│       ├── coordinator.go  # 게임 서버 할당
│       ├── result.go       # 게임 결과 처리
│       └── model.go        # 게임 결과 데이터
│
├── pkg/                    # 공용 패키지 (기존 + 확장)
│   ├── auth/jwt.go
│   ├── cache/redis.go
│   ├── config/config.go
│   ├── database/postgres.go
│   ├── messaging/kafka.go
│   └── middleware/
│       ├── auth.go
│       ├── logging.go
│       ├── ratelimit.go    # Rate Limiter
│       └── cors.go         # CORS
│
└── migrations/             # DB 마이그레이션 SQL
    ├── 001_users.up.sql
    ├── 002_champions.up.sql
    ├── 003_items.up.sql
    ├── 004_match_history.up.sql
    └── 005_inventory.up.sql
```

### 5-2. DB 스키마 (PostgreSQL)

```sql
-- migrations/001_users.up.sql
CREATE TABLE users (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username    VARCHAR(32) UNIQUE NOT NULL,
    email       VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(32) NOT NULL,
    level       INT DEFAULT 1,
    xp          INT DEFAULT 0,
    currency    INT DEFAULT 5000,         -- 기본 지급 화폐
    premium_currency INT DEFAULT 0,       -- 유료 화폐
    mmr         INT DEFAULT 1200,         -- 매치메이킹 레이팅
    rank_tier   VARCHAR(20) DEFAULT 'IRON',
    created_at  TIMESTAMPTZ DEFAULT NOW(),
    updated_at  TIMESTAMPTZ DEFAULT NOW(),
    last_login  TIMESTAMPTZ
);

CREATE INDEX idx_users_mmr ON users(mmr);
CREATE INDEX idx_users_rank ON users(rank_tier);

-- migrations/002_champions.up.sql
CREATE TABLE champions (
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(32) UNIQUE NOT NULL,
    title       VARCHAR(64),
    role        VARCHAR(20),  -- FIGHTER, MAGE, MARKSMAN, ASSASSIN, TANK, SUPPORT
    difficulty  INT,          -- 1~3
    base_hp     FLOAT,
    base_mana   FLOAT,
    base_ad     FLOAT,
    base_ap     FLOAT,
    base_armor  FLOAT,
    base_mr     FLOAT,
    base_ms     FLOAT,        -- 이동속도
    attack_range FLOAT,
    price       INT DEFAULT 3150,
    is_free_rotation BOOLEAN DEFAULT false
);

-- migrations/003_items.up.sql
CREATE TABLE shop_items (
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(64) NOT NULL,
    type        VARCHAR(20),   -- CHAMPION, SKIN, EMOTE, WARD, ICON
    champion_id INT REFERENCES champions(id),
    price       INT NOT NULL,
    premium_price INT,         -- 유료 화폐 가격 (null이면 구매 불가)
    rarity      VARCHAR(20),   -- COMMON, RARE, EPIC, LEGENDARY, ULTIMATE
    asset_path  VARCHAR(255),  -- 클라이언트 에셋 경로
    is_available BOOLEAN DEFAULT true,
    release_date TIMESTAMPTZ
);

-- migrations/004_match_history.up.sql
CREATE TABLE matches (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    mode        VARCHAR(20) NOT NULL,  -- RANKED, NORMAL, ARAM
    map_id      INT DEFAULT 1,
    duration_sec INT,
    winner_team INT,          -- 0=Blue, 1=Red
    created_at  TIMESTAMPTZ DEFAULT NOW(),
    ended_at    TIMESTAMPTZ
);

CREATE TABLE match_participants (
    id          SERIAL PRIMARY KEY,
    match_id    UUID REFERENCES matches(id),
    user_id     UUID REFERENCES users(id),
    champion_id INT REFERENCES champions(id),
    team        INT,          -- 0=Blue, 1=Red
    kills       INT DEFAULT 0,
    deaths      INT DEFAULT 0,
    assists     INT DEFAULT 0,
    cs          INT DEFAULT 0,  -- 크리프 스코어
    gold_earned INT DEFAULT 0,
    damage_dealt INT DEFAULT 0,
    damage_taken INT DEFAULT 0,
    wards_placed INT DEFAULT 0,
    mmr_change  INT DEFAULT 0,
    is_winner   BOOLEAN,
    items       JSONB         -- 최종 아이템 빌드
);

CREATE INDEX idx_match_participants_user ON match_participants(user_id);
CREATE INDEX idx_match_participants_match ON match_participants(match_id);

-- migrations/005_inventory.up.sql
CREATE TABLE user_inventory (
    id          SERIAL PRIMARY KEY,
    user_id     UUID REFERENCES users(id),
    item_id     INT REFERENCES shop_items(id),
    acquired_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(user_id, item_id)
);

CREATE TABLE user_champions (
    user_id     UUID REFERENCES users(id),
    champion_id INT REFERENCES champions(id),
    mastery_level INT DEFAULT 1,
    mastery_xp  INT DEFAULT 0,
    games_played INT DEFAULT 0,
    wins        INT DEFAULT 0,
    PRIMARY KEY (user_id, champion_id)
);
```

### 5-3. 매치메이킹 알고리즘

```go
// internal/matchmaking/service.go (핵심 로직)

// MMR 기반 매칭:
// 1. Redis Sorted Set에 대기 중인 플레이어를 MMR로 정렬
// 2. 매 1초마다 매칭 루프 실행
// 3. MMR 차이가 ±100 이내인 10명을 찾아서 팀 분배
// 4. 대기 시간이 길어지면 MMR 허용 범위 확대 (30초마다 ±50 증가)
// 5. 매칭 성공 시 Kafka로 game-coordinator에 게임 생성 이벤트 발행

type MatchmakingConfig struct {
    InitialMMRRange    int           // 100
    MMRRangeExpansion  int           // 50 (30초마다)
    MaxMMRRange        int           // 500
    ExpansionInterval  time.Duration // 30s
    PlayersPerMatch    int           // 10
    TickInterval       time.Duration // 1s
}
```

### 5-4. 게임 흐름 (클라이언트 → 백엔드 → 게임 서버)

```
┌─────────────────────────────────────────────────────────────┐
│                    전체 게임 흐름                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. [Client] 회원가입 화면                                    │
│     POST /api/auth/register {username, email, password}     │
│     → [Auth Service] 비밀번호 bcrypt 해싱 → PostgreSQL 저장   │
│     ← 201 Created                                           │
│                                                             │
│  2. [Client] 로그인 화면                                      │
│     POST /api/auth/login {username, password}               │
│     → [Auth Service] 비밀번호 검증 → JWT 토큰 발급            │
│     ← {accessToken, refreshToken, userProfile}              │
│                                                             │
│  3. [Client] 메인 로비                                       │
│     GET /api/profile/me (Authorization: Bearer <JWT>)       │
│     → [Profile Service] DB에서 유저 정보 조회                 │
│     ← {level, mmr, rank, currency, recentMatches}           │
│                                                             │
│  4. [Client] 상점                                            │
│     GET /api/shop/catalog                                   │
│     → [Shop Service] 카탈로그 조회 (Redis 캐시)              │
│     ← {champions[], skins[], bundles[]}                     │
│                                                             │
│     POST /api/shop/purchase {itemId}                        │
│     → [Shop Service] 잔액 확인 → 트랜잭션 → 인벤토리 추가    │
│     ← {success, newBalance, item}                           │
│                                                             │
│  5. [Client] 매치 찾기                                       │
│     POST /api/matchmaking/queue {mode: "RANKED"}            │
│     → [Matchmaking] Redis 대기열에 등록 (MMR 정렬)           │
│     ← {queueId, estimatedWait}                              │
│                                                             │
│     SSE /api/matchmaking/events (Server-Sent Events)        │
│     → [Matchmaking] 매칭 성공 시 이벤트 푸시                 │
│     ← {matchFound: true, matchId, serverIP, serverPort}     │
│                                                             │
│  6. [Client] 챔피언 선택                                     │
│     UDP 연결 → Game Server (serverIP:port)                   │
│     C2S_JOIN {matchId, authToken, selectedChampion}          │
│     → [Game Server] 토큰 검증 → 룸 입장                     │
│     ← S2C_CHAMP_SELECT {banPhase, pickOrder, timer}         │
│                                                             │
│  7. [Client] 인게임                                          │
│     C2S_INPUT {sequence, moveDir, buttons, lookDir} @ 60Hz  │
│     → [Game Server] 서버 권위 시뮬레이션 (20 TPS)            │
│     ← S2C_SNAPSHOT {tick, champions[], minions[], turrets[]} │
│     클라이언트: 예측 + 보간 + 렌더링                          │
│                                                             │
│  8. [Game Server] 게임 종료                                  │
│     넥서스 파괴 → 결과 집계                                   │
│     Kafka 이벤트 → [Profile Service]                         │
│     → PostgreSQL: matches + match_participants INSERT        │
│     → Redis: MMR 업데이트, 랭킹 갱신                         │
│     ← S2C_GAME_RESULT {winner, stats, mmrChange, xpGained}  │
│                                                             │
│  9. [Client] 결과 화면                                       │
│     KDA, 골드, CS, 데미지 통계 표시                           │
│     "다시 하기" → 5번으로 돌아감                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 6 — 안티치트 (Vanguard 스타일 커널 레벨) (D24~D26, 3일)

### 6-1. 아키텍처 개요

```
┌──────────────────────────────────────────────────────────┐
│                   Anti-Cheat 아키텍처                      │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  [Ring 0 — 커널 드라이버] WintersGuard.sys               │
│  ├─ 프로세스 보호 (ObRegisterCallbacks)                   │
│  │  → 게임 프로세스에 대한 메모리 접근 차단                │
│  │  → OpenProcess, ReadProcessMemory 후킹 감지            │
│  ├─ 커널 무결성 검사                                      │
│  │  → SSDT (System Service Descriptor Table) 변조 감지    │
│  │  → 커널 모듈 서명 검증                                 │
│  ├─ 드라이버 로드 모니터링 (PsSetLoadImageNotifyRoutine)  │
│  │  → 미서명 드라이버 로드 차단/보고                      │
│  └─ DMA 공격 방어                                        │
│     → IOMMU 상태 확인                                    │
│                                                          │
│  [Ring 3 — 유저모드 서비스] WintersGuardService.exe       │
│  ├─ 프로세스 무결성 검사                                  │
│  │  → 게임 실행 파일 해시 검증                            │
│  │  → 로드된 DLL 서명 검증                                │
│  │  → 코드 섹션 체크섬 (실행 중 패치 감지)                │
│  ├─ 메모리 스캔                                          │
│  │  → 알려진 치트 시그니처 패턴 매칭                      │
│  │  → 인젝션 감지 (VirtualAllocEx, WriteProcessMemory)    │
│  ├─ 화면 캡처 방지                                       │
│  │  → OBS/캡처카드 감지 (경고만, 차단은 선택)             │
│  ├─ 입력 디바이스 검증                                    │
│  │  → Raw Input 체크 (매크로/오토핫키 감지)               │
│  └─ 하트비트 (서버 통신)                                  │
│     → 주기적 무결성 보고서 서버 전송                      │
│     → 서버가 검증 실패 시 세션 킥                         │
│                                                          │
│  [서버사이드 검증] (CAntiCheatServer)                     │
│  ├─ 이동 속도 검증 (speedhack)                            │
│  │  → 서버 틱 간 이동 거리 > maxSpeed * dt * 1.1 → 킥   │
│  ├─ 쿨다운 검증 (cooldown hack)                           │
│  │  → 서버가 쿨다운 타이머 관리                           │
│  ├─ 시야 검증 (maphack)                                   │
│  │  → FOW 밖 엔티티 정보 미전송                           │
│  ├─ 데미지 검증                                           │
│  │  → 서버가 스킬 계산 수행, 클라이언트 값 무시           │
│  └─ 통계 이상 감지                                        │
│     → 비정상적 KDA, 정확도, 반응속도 ML 모델             │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 6-2. 커널 드라이버 (WintersGuard.sys)

```
AntiCheat/
├── Driver/
│   ├── WintersGuard.c         # 드라이버 엔트리
│   ├── ProcessProtect.c       # 프로세스 보호 콜백
│   ├── DriverMonitor.c        # 드라이버 로드 감시
│   ├── IntegrityCheck.c       # 커널 무결성
│   ├── Communication.c        # 유저모드↔커널 통신 (IOCTL)
│   └── WintersGuard.inf       # 드라이버 설치 INF
│
├── Service/
│   ├── WintersGuardService.cpp  # 유저모드 서비스
│   ├── CMemoryScanner.cpp       # 메모리 스캔
│   ├── CIntegrityChecker.cpp    # 파일/코드 무결성
│   ├── CInputValidator.cpp      # 입력 검증
│   └── CHeartbeat.cpp           # 서버 하트비트
│
└── Shared/
    ├── IOCTLDefs.h              # 커널↔유저 IOCTL 정의
    └── SignatureDefs.h          # 치트 시그니처 DB
```

```c
// AntiCheat/Driver/WintersGuard.c — 커널 드라이버 엔트리

#include <ntddk.h>
#include <wdm.h>

#define DEVICE_NAME     L"\\Device\\WintersGuard"
#define SYMLINK_NAME    L"\\DosDevices\\WintersGuard"

// 보호 대상 프로세스 PID
static HANDLE g_ProtectedPID = NULL;

// ObRegisterCallbacks: 프로세스 핸들 접근 필터링
OB_PREOP_CALLBACK_STATUS PreOperationCallback(
    PVOID RegistrationContext,
    POB_PRE_OPERATION_INFORMATION OperationInfo)
{
    // 보호 대상 프로세스에 대한 접근 요청 필터링
    if (OperationInfo->ObjectType == *PsProcessType)
    {
        PEPROCESS targetProcess = (PEPROCESS)OperationInfo->Object;
        HANDLE targetPid = PsGetProcessId(targetProcess);

        if (targetPid == g_ProtectedPID)
        {
            // 자기 자신의 접근은 허용
            PEPROCESS currentProcess = PsGetCurrentProcess();
            if (PsGetProcessId(currentProcess) == g_ProtectedPID)
                return OB_PREOP_SUCCESS;

            // 외부 프로세스의 메모리 읽기/쓰기 권한 제거
            if (OperationInfo->Operation == OB_OPERATION_HANDLE_CREATE)
            {
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &=
                    ~(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
            }
        }
    }
    return OB_PREOP_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    // 디바이스 + 심볼릭 링크 생성
    UNICODE_STRING devName = RTL_CONSTANT_STRING(DEVICE_NAME);
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYMLINK_NAME);

    PDEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName,
                                      FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&symLink, &devName);

    // ObRegisterCallbacks 등록
    OB_CALLBACK_REGISTRATION cbReg = {0};
    OB_OPERATION_REGISTRATION opReg = {0};
    opReg.ObjectType = PsProcessType;
    opReg.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    opReg.PreOperation = PreOperationCallback;

    cbReg.Version = OB_FLT_REGISTRATION_VERSION;
    cbReg.OperationRegistrationCount = 1;
    cbReg.OperationRegistration = &opReg;
    RtlInitUnicodeString(&cbReg.Altitude, L"321000");

    PVOID regHandle = NULL;
    status = ObRegisterCallbacks(&cbReg, &regHandle);

    // 드라이버 로드 알림 등록
    PsSetLoadImageNotifyRoutine(LoadImageNotifyRoutine);

    // IRP 핸들러 설정
    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[WintersGuard] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}
```

### 6-3. 서버사이드 안티치트

```cpp
// Server/Public/CAntiCheatServer.h
#pragma once

class CAntiCheatServer
{
public:
    ~CAntiCheatServer() = default;
    static std::unique_ptr<CAntiCheatServer> Create();

    // 매 서버 틱 호출
    void Validate(class CGameRoom* pRoom, float dt);

    // 검증 결과
    enum class Violation : uint8_t
    {
        None,
        SpeedHack,        // 비정상 이동 속도
        CooldownHack,     // 쿨다운 무시
        RangeHack,        // 사거리 초과 스킬
        DamageHack,       // 비정상 데미지
        TeleportHack,     // 순간이동 (프레임간 거리 > threshold)
        HeartbeatTimeout, // 안티치트 클라이언트 무응답
        IntegrityFail     // 클라이언트 무결성 실패
    };

    struct ViolationReport
    {
        uint32_t playerId;
        Violation type;
        float severity;       // 0.0~1.0
        std::string details;
    };

    [[nodiscard]] std::span<const ViolationReport> GetPendingReports() const;

private:
    CAntiCheatServer() = default;

    // 이동 검증: 틱 간 이동 거리 체크
    void ValidateMovement(uint32_t playerId, const Vec3& prevPos, const Vec3& currPos, 
                          float maxSpeed, float dt);

    // 쿨다운 검증: 서버 타이머 기반
    void ValidateCooldown(uint32_t playerId, uint32_t abilityId);

    // 통계 기반 이상 감지
    void UpdatePlayerStats(uint32_t playerId, const struct MatchStats& stats);

    std::vector<ViolationReport> m_PendingReports;
};
```

---

## Phase 7 — 에디터 & 게임 콘텐츠 (D27~D28, 2일)

### 7-1. ImGui 인게임 에디터

```
Engine/Public/Editor/
├── CImGuiRenderer.h       # ImGui DX11 백엔드 초기화
├── CEditorWindow.h        # 에디터 윈도우 베이스
├── CSceneHierarchy.h      # 씬 계층 패널
├── CInspector.h           # 프로퍼티 인스펙터
├── CMapEditor.h           # 소환사의 협곡 맵 에디터
├── CChampionEditor.h      # 챔피언 스탯/스킬 에디터
├── CShaderEditor.h        # HLSL 실시간 편집
└── CConsole.h             # 인게임 콘솔 (명령어)
```

### 7-2. 소환사의 협곡 맵 데이터

```
Client/Data/Maps/
├── SummonersRift.wmap      # 맵 바이너리 (지형, 브러시, 벽)
├── SummonersRift.wnav      # NavMesh (경로 탐색용)
├── SummonersRift.wlight    # 라이트맵
└── SummonersRift.json      # 메타데이터 (스폰 포인트, 오브젝트 배치)

맵 구조:
  - 지형 메시 (높이맵 기반)
  - 충돌 메시 (벽, 터렛 위치)
  - NavMesh (미니언 경로, 챔피언 이동 영역)
  - 브러시 영역 (시야 차단 볼륨)
  - 스폰 포인트: 챔피언(10), 미니언(6 웨이브), 정글 몬스터(14), 드래곤, 바론
```

### 7-3. 챔피언 시스템 (Lua 데이터 드리븐)

```lua
-- Client/Data/Champions/Ahri/champion.lua
DefineChampion("Ahri", {
    title   = "구미호",
    role    = "MAGE",
    range   = 550,

    baseStats = {
        hp = 526, hpPerLevel = 92,
        mana = 418, manaPerLevel = 25,
        ad = 53, adPerLevel = 3,
        ap = 0,
        armor = 21, armorPerLevel = 3.5,
        mr = 30, mrPerLevel = 0.5,
        ms = 330,
        attackSpeed = 0.668
    },

    abilities = {
        passive = {
            name = "자연의 폭풍",
            description = "스킬 적중 시 체력 회복",
            onHit = function(caster, target)
                local healAmount = 3 + caster.level * 2
                Heal(caster, healAmount)
            end
        },
        Q = {
            name = "기만의 구슬",
            cooldown = {7, 7, 7, 7, 7},
            manaCost = {65, 70, 75, 80, 85},
            range = 880,
            projectile = {
                speed = 1700,
                width = 100,
                onHit = function(caster, target, isReturn)
                    local damage = 40 + caster.level * 40 + caster.ap * 0.35
                    if isReturn then
                        ApplyTrueDamage(caster, target, damage)
                    else
                        ApplyMagicDamage(caster, target, damage)
                    end
                end
            }
        },
        W = {
            name = "여우불",
            cooldown = {9, 8, 7, 6, 5},
            manaCost = {40, 40, 40, 40, 40},
            onCast = function(caster)
                for i = 1, 3 do
                    SpawnFoxFire(caster, {
                        damage = 40 + caster.level * 30 + caster.ap * 0.3,
                        duration = 5.0,
                        seekRange = 725
                    })
                end
            end
        },
        E = {
            name = "매혹",
            cooldown = {14, 14, 14, 14, 14},
            manaCost = {70, 70, 70, 70, 70},
            range = 975,
            projectile = {
                speed = 1600,
                width = 60,
                onHit = function(caster, target)
                    local damage = 60 + caster.level * 20 + caster.ap * 0.6
                    ApplyMagicDamage(caster, target, damage)
                    ApplyCC(target, "CHARM", 1.2 + caster.level * 0.2)
                end
            }
        },
        R = {
            name = "혼령 질주",
            cooldown = {130, 105, 80},
            manaCost = {100, 100, 100},
            dashes = 3,
            dashRange = 450,
            dashSpeed = 1400,
            onDash = function(caster, dashIndex)
                local targets = FindEnemiesInRange(caster.position, 600, 3)
                for _, target in ipairs(targets) do
                    local damage = 60 + caster.level * 30 + caster.ap * 0.35
                    ApplyMagicDamage(caster, target, damage)
                end
            end
        }
    }
})
```

---

## Phase 8 — 통합 테스트 & 폴리싱 (D29, 1일)

### 8-1. 통합 테스트 체크리스트

```
[ ] 회원가입 → 로그인 → JWT 토큰 발급 확인
[ ] 상점 목록 로드 → 챔피언 구매 → 인벤토리 확인
[ ] 매치메이킹 큐 진입 → 매칭 성공 → 게임 서버 연결
[ ] 챔피언 선택 → 로딩 → 인게임 진입
[ ] 5v5 기본 게임플레이 (이동, 스킬, 미니언, 터렛)
[ ] 게임 종료 → 결과 DB 저장 → 결과 화면 표시
[ ] MMR 변동 확인
[ ] 해상도 변경 → UV/UI 정상 표시 확인
[ ] 다중 클라이언트 동시 접속 테스트
[ ] 안티치트 드라이버 로드 → 보호 동작 확인
[ ] 프로파일러 오버레이 정상 표시
[ ] 메모리 누수 검사 (Visual Studio Diagnostic Tools)
```

### 8-2. 성능 목표

```
렌더링:
  - 1080p 60FPS 이상 (GTX 1060 기준)
  - G-Buffer Pass: < 3ms
  - Lighting Pass: < 2ms
  - PostFX: < 1ms
  - 총 프레임: < 16.6ms

네트워크:
  - 클라이언트 → 서버 입력 지연: < 50ms (로컬)
  - 서버 틱레이트: 20 TPS 안정
  - 패킷 크기: 스냅샷 < 1KB (10명 기준)

서버:
  - 동시 접속 100명 (10 게임 룸) 안정
  - 틱 처리: < 50ms / tick
```

---

## 전체 파일 구조 요약

```
Winters/
├── CLAUDE.md
├── .md/
│   ├── WINTERS_ENGINE_ARCHITECTURE_FINAL.md
│   ├── ROADMAP.md
│   └── LOL_30DAY_MASTER_PLAN.md          ← 이 문서
│
├── Engine/
│   ├── Include/          # 공개 API (기존 + 확장)
│   ├── Public/           # 내부 헤더
│   │   ├── Core/         # Timer, JobSystem(Fiber), Allocator, EventBus, Profiler
│   │   ├── Platform/     # Win32Window, Input, DisplaySettings
│   │   ├── RHI/          # CDX11Device, RenderGraph, GBuffer, Shadow, PostFX
│   │   ├── Framework/    # CEngineApp
│   │   ├── ECS/          # Entity, Component, System, World
│   │   ├── Resource/     # MeshLoader, AnimLoader, TextureLoader, MaterialLoader
│   │   ├── Network/      # UDPSocket, KCPTransport, ClientNet, Prediction
│   │   └── Editor/       # ImGuiRenderer, EditorWindows
│   └── Private/          # 구현 파일 (.cpp)
│
├── Client/
│   ├── Include/          # vcxproj
│   ├── Public/           # CGameApp, Scenes, GameObjects
│   ├── Private/          # 구현
│   ├── Data/             # 게임 데이터
│   │   ├── Champions/    # Lua 챔피언 정의
│   │   ├── Maps/         # 맵 데이터
│   │   └── Items/        # 아이템 정의
│   └── Bin/              # 출력
│
├── Server/
│   ├── Include/          # vcxproj
│   ├── Public/           # IOCP, Session, GameRoom, GameLogic, AOI, AntiCheat
│   ├── Private/          # 구현
│   └── Bin/              # 출력
│
├── Shared/
│   ├── PacketDef.h       # (레거시, Phase 4에서 FlatBuffers로 교체)
│   └── Schemas/          # FlatBuffers .fbs 스키마
│
├── Services/             # Go 백엔드
│   ├── cmd/              # 서비스 엔트리
│   ├── internal/         # 비즈니스 로직
│   ├── pkg/              # 공용 패키지
│   └── migrations/       # DB 마이그레이션
│
├── AntiCheat/            # Vanguard 스타일 안티치트
│   ├── Driver/           # 커널 드라이버 (.sys)
│   ├── Service/          # 유저모드 서비스
│   └── Shared/           # 커널↔유저 공유 정의
│
├── Shaders/
│   ├── GBuffer.hlsl
│   ├── ClusteredLighting.hlsl
│   ├── GPUCull.hlsl
│   ├── Champion.hlsl
│   ├── Terrain.hlsl
│   ├── PostFX/           # Bloom, ToneMapping, FXAA, SSAO, Fog
│   └── Shadow.hlsl
│
└── Tools/
    ├── blender_export.py
    ├── convert_textures.bat
    └── WintersAssetConverter/
```

---

## 일정 요약 (30일)

| 일차 | Phase | 내용 | 핵심 산출물 |
|------|-------|------|------------|
| D0~D2 | 0 | 모델링 추출 & 에셋 파이프라인 | .wmesh/.wanim/.wmat 포맷, Blender 변환기, DirectXTex 텍스처 파이프라인 |
| D3~D5 | 1 | Fiber JobSystem & 코어 강화 | Fiber Pool + Counter 의존성, Linear/Pool Allocator, EventBus |
| D6~D10 | 2 | RenderGraph & Deferred Pipeline | G-Buffer, Clustered Lighting CS, CSM Shadow, PostFX (Bloom/ToneMap/FXAA/SSAO) |
| D11~D13 | 3 | GPU-Driven & Profiling | GPU 컬링 CS, IndirectDraw, CPU/GPU 프로파일러, 해상도 설정 시스템 |
| D14~D19 | 4 | 네트워크 & 게임 서버 | UDP/KCP, FlatBuffers, IOCP 서버, GameRoom, AOI, 클라이언트 예측, 지연 보상 |
| D20~D23 | 5 | Go 백엔드 & DB | Auth/Shop/Matchmaking/Profile 서비스, PostgreSQL 스키마, Redis 캐시, Kafka 이벤트 |
| D24~D26 | 6 | 안티치트 (커널 레벨) | WintersGuard.sys 커널 드라이버, 유저모드 서비스, 서버사이드 검증 |
| D27~D28 | 7 | 에디터 & 게임 콘텐츠 | ImGui 에디터, 소환사의 협곡 맵, Lua 챔피언 시스템 |
| D29 | 8 | 통합 테스트 & 폴리싱 | E2E 테스트, 성능 최적화, 메모리 누수 검사 |

---

## Verification (검증 방법)

1. **빌드 검증**: Engine.dll + Client.exe + Server.exe 전부 x64 Release 빌드 성공
2. **렌더링 검증**: 프로파일러 오버레이로 FPS/프레임 타임 확인, G-Buffer 시각화
3. **네트워크 검증**: 로컬 서버 + 2~4 클라이언트 동시 접속, 이동 동기화 확인
4. **백엔드 검증**: Postman/curl로 Auth → Shop → Matchmaking API 시퀀스 테스트
5. **DB 검증**: 게임 종료 후 PostgreSQL에서 match_participants 레코드 확인
6. **안티치트 검증**: CheatEngine으로 메모리 접근 시도 → 차단 확인
7. **해상도 검증**: 1280x720, 1920x1080, 2560x1440 전환 후 UI/UV 정상 확인
8. **성능 검증**: 1080p 60FPS, 서버 20TPS 안정 유지

---

## 구현 시작 시 첫 번째 작업

`LOL_30DAY_MASTER_PLAN.md`를 `Winters/.md/` 디렉토리에 저장 후,
**Phase 0-1 (에셋 포맷 정의)**부터 시작:
- `Engine/Public/Resource/AssetFormat.h` 작성
- `Tools/blender_export.py` 작성
- `Tools/convert_textures.bat` 작성

---

*작성일: 2026-04-12 | Winters Engine — LoL 30일 마스터 플랜 v1.0*
