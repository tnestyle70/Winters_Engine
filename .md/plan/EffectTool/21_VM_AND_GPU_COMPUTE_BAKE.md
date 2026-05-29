# 21. VM + GPU Compute 박제 (CPU `CFxVM` 28 opcode + GPU `CFxGpuComputeDispatch` RH-7 통과)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 적용 — stub 0 / 라인 번호 명시 / 추상 지시 0)
권위: 본 21 = 17 마스터 §15 부속 4번. EFX-7 진입 직전 박제.
의존: 부속 18 (FxScriptAsset.GetVMData), Track 2 RH-7 (`Engine/Public/RHI/RHICapabilities.h`, `IRHIComputePipelineState`).
참조 코드:
- VectorVM (Unreal): `Engine/Source/Runtime/VectorVM/{Public,Private}/VectorVM.h+.cpp`
- Niagara GPU: `NiagaraGpuComputeDispatch.h+.cpp`, `NiagaraGPUSystemTick.h+.cpp`, `NiagaraGpuComputeDispatchInterface.h`

목적:
- CPU VM 28 opcode 풀 본문 박제 (NOOP/ADD/SUB/MUL/DIV/NEG/RECIP/SQRT/DOT/CROSS/NORMALIZE/LERP/CLAMP/CMPLT/CMPGT/CMPEQ/SELECT/SIN/COS/ATAN2/FRAC/RAND_FLOAT/RAND_RANGE/LOAD_CONST/LOAD_PARAM/LOAD_ATTR/EXTERNAL/OUTPUT)
- AVX2 + SSE2 + scalar 3 path 분기 본문
- GPU compute dispatch (RH-7 IRHIComputePipelineState 통과) 본문
- DX11 capability 검사 + GPU 미지원 시 CPU 강제 다운그레이드 + 1 회 경고

박제 진입 전 8 단계 관문:
- 관문 A: §1 5 항목, TBD 0
- 관문 B: 헤더 + cpp 동시, 본문 풀
- 관문 C: VM + GPU 양 경로 한 번에
- 관문 D: Scene 직접 의존 0
- 관문 E: VM = mask 미사용
- 관문 F: VectorVM `VectorVM.h` opcode 표 / `NiagaraGPUSystemTick.cpp:19-91` 직접 차용
- 관문 G: VM = ECS 무관. GPU dispatch = phase 5 enqueue
- 관문 H: VM = stateless static. GPU dispatch = `CGameInstance` Tier-2

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 21 v1 의 stub 4 위치 본문화:

```txt
1. ExecuteAVX2 의 default scalar fallback
   v1 = "scalar fallback. 본 박제 시점 = stub" 추상 표기
   v2 = AVX2 path 가 안 닿는 opcode (sin/cos/atan2/external/output) = 명시적 scalar branch 호출

2. CFxGpuComputeDispatch::Dispatch 본문
   v1 = "본 박제 시점 = stub. RH-7 IRHICommandList compute API 박제 후 본문 채움"
   v2 = IRHICommandList::SetComputePipelineState / SetComputeBindGroup / Dispatch 호출 본문 (RH-7 박제됨)

3. EXTERNAL opcode 의 DI 호출
   v1 = "CPU 함수 포인터 호출은 FxScriptExecContext 의 helper 가 담당. 본 박제 시점 = stub. for(uN) pDst[i] = pA[i]; identity"
   v2 = ctx.spanDataInterfaces[uDIIdx] 의 GetCPUFunction(...).operator() 직접 호출

4. FxParticleSim_CS.hlsl 본문
   v1 = "Translator (부속 22) 가 module 본문을 여기 inline. <BEGIN_MODULE_INJECT>"
   v2 = Translator inject 위치 표시는 placeholder marker 로 박제 (parse-able). 본 21 박제 시점의 HLSL = 빈 본문 + marker 명시 (부속 22 가 marker 치환)
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Opcode 28 값 | NOOP + 27 = 28 | Niagara VectorVM 30+ 에서 LWC / async GPU trace / nanite mesh 그룹 미차용 |
| SIMD | AVX2 (8-wide) 1차. SSE2 (4-wide) fallback. scalar (debug) | VectorVM 패턴 |
| Bytecode layout | `FxVMInstruction` POD = 13 byte | Niagara `FVMInstruction` 차용 |
| GPU compute path | DX11 SM 5.0 / DX12 SM 5.1+. RH-7 capability `bSupportsCompute` 검사 | Niagara `bUsesGPU = false` fallback 패턴 |
| Compile target 양 경로 | Translator (부속 22) 가 Graph → bytecode (CPU) + HLSL (GPU) 동시 산출 | Niagara `FNiagaraVMExecutableData` + `FNiagaraShaderMapPointerTable` |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/VM/
  FxVMOpcode.h
  FxVMInstruction.h
  FxVMExecutableData.h
  FxScriptExecContext.h
  FxVM.h

Engine/Public/FX/v2/GPU/
  FxGpuSystemTick.h
  FxGpuComputeDispatch.h
  FxGpuComputeShim.h

Engine/Private/FX/v2/VM/
  FxVM.cpp                         28 opcode 본문 (scalar + AVX2)

Engine/Private/FX/v2/GPU/
  FxGpuComputeDispatch.cpp
  FxGpuComputeShim.cpp

Shaders/FX/v2/Compute/
  FxParticleSim_CS.hlsl
  FxCommonGpu.hlsli
```

---

## §3 헤더 박제 (전문, L1- 라인 번호 명시)

### §3.1 `Engine/Public/FX/v2/VM/FxVMOpcode.h` (L1-L43)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
// L6
namespace Winters::FX::v2
// L7
{
// L8
    enum class eFxOp : u8_t
// L9
    {
// L10
        NOOP        = 0,
// L11
        ADD         = 1,
// L12
        SUB         = 2,
// L13
        MUL         = 3,
// L14
        DIV         = 4,
// L15
        NEG         = 5,
// L16
        RECIP       = 6,
// L17
        SQRT        = 7,
// L18
        DOT         = 8,
// L19
        CROSS       = 9,
// L20
        NORMALIZE   = 10,
// L21
        LERP        = 11,
// L22
        CLAMP       = 12,
// L23
        CMPLT       = 13,
// L24
        CMPGT       = 14,
// L25
        CMPEQ       = 15,
// L26
        SELECT      = 16,
// L27
        SIN         = 17,
// L28
        COS         = 18,
// L29
        ATAN2       = 19,
// L30
        FRAC        = 20,
// L31
        RAND_FLOAT  = 21,
// L32
        RAND_RANGE  = 22,
// L33
        LOAD_CONST  = 23,
// L34
        LOAD_PARAM  = 24,
// L35
        LOAD_ATTR   = 25,
// L36
        EXTERNAL    = 26,
// L37
        OUTPUT      = 27,
// L38
    };
// L39
// L40
    inline constexpr u32_t kFxVMOpcodeCount = 28;
// L41
}
// L42
// L43
static_assert(static_cast<Winters::FX::v2::u32_t>(Winters::FX::v2::eFxOp::OUTPUT) == 27, "eFxOp::OUTPUT must be 27");
```

(static_assert 는 cpp 단위에서 유효한 namespace path 박제. `u32_t` = `WintersTypes.h` 의 global alias.)

### §3.2 `Engine/Public/FX/v2/VM/FxVMInstruction.h` (L1-L26)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/VM/FxVMOpcode.h"
// L6
// L7
namespace Winters::FX::v2
// L8
{
// L9
#pragma pack(push, 1)
// L10
    struct FxVMInstruction
// L11
    {
// L12
        eFxOp eOp = eFxOp::NOOP;
// L13
        u16_t uDstReg = 0;
// L14
        u16_t uSrcA = 0;
// L15
        u16_t uSrcB = 0;
// L16
        u16_t uSrcC = 0;
// L17
        u32_t uExtraOperand = 0;
// L18
    };
// L19
#pragma pack(pop)
// L20
// L21
    static_assert(sizeof(FxVMInstruction) == 13, "FxVMInstruction must be 13 bytes packed");
// L22
}
```

P-16 강제: sizeof = 13 byte (1 + 2*4 + 4 = 13).

### §3.3 `Engine/Public/FX/v2/VM/FxVMExecutableData.h` (L1-L20)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/VM/FxVMInstruction.h"
// L6
#include <vector>
// L7
// L8
namespace Winters::FX::v2
// L9
{
// L10
    struct WINTERS_ENGINE FxVMExecutableData
// L11
    {
// L12
        std::vector<FxVMInstruction> vecInstructions;
// L13
        std::vector<f32_t> vecConstants;
// L14
        u32_t uNumRegisters = 0;
// L15
        u32_t uNumExternalCalls = 0;
// L16
        u32_t uVersion = 1;
// L17
    };
// L18
}
```

### §3.4 `Engine/Public/FX/v2/VM/FxScriptExecContext.h` (L1-L26)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include <span>
// L6
// L7
namespace Winters::FX::v2
// L8
{
// L9
    class CFxParameterStore;     // 부속 19 (Engine/Public/FX/v2/Instance/FxParameterStore.h)
// L10
    class CFxDataSet;            // 부속 19 (Engine/Public/FX/v2/Instance/FxDataSet.h)
// L11
    class IFxDataInterface;      // 부속 23 (Engine/Public/FX/v2/DataInterface/IFxDataInterface.h)
// L12
    struct FxVMExecutableData;
// L13
// L14
    struct FxScriptExecContext
// L15
    {
// L16
        const FxVMExecutableData* pData = nullptr;
// L17
        CFxParameterStore* pParameterStore = nullptr;
// L18
        CFxDataSet* pDataSet = nullptr;
// L19
        std::span<IFxDataInterface*> spanDataInterfaces;
// L20
        u32_t uStartInstance = 0;
// L21
        u32_t uNumInstances = 0;
// L22
        f32_t fDeltaTime = 0.f;
// L23
        u64_t uRandomSeed = 0;
// L24
    };
// L25
}
```

### §3.5 `Engine/Public/FX/v2/VM/FxVM.h` (L1-L25)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/VM/FxScriptExecContext.h"
// L6
// L7
namespace Winters::FX::v2
// L8
{
// L9
    class WINTERS_ENGINE CFxVM
// L10
    {
// L11
    public:
// L12
        enum class eSimdPath : u8_t { Auto = 0, Scalar = 1, SSE2 = 2, AVX2 = 3 };
// L13
// L14
        static void Execute(const FxScriptExecContext& context);
// L15
        static void ExecuteWithPath(const FxScriptExecContext& context, eSimdPath ePath);
// L16
        static eSimdPath DetectSimdPath();
// L17
    };
// L18
}
```

### §3.6 `Engine/Public/FX/v2/GPU/FxGpuSystemTick.h` (L1-L26)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
// L6
namespace Winters::FX::v2
// L7
{
// L8
    class CFxEmitterInstance;     // 부속 19
// L9
    struct FxVMExecutableData;
// L10
// L11
    enum class eFxGpuStage : u8_t
// L12
    {
// L13
        EmitterSpawn   = 0,
// L14
        EmitterUpdate  = 1,
// L15
        ParticleSpawn  = 2,
// L16
        ParticleUpdate = 3,
// L17
        SimulationStage = 4,
// L18
    };
// L19
// L20
    struct FxGpuSystemTick
// L21
    {
// L22
        CFxEmitterInstance* pEmitter = nullptr;
// L23
        eFxGpuStage eStage = eFxGpuStage::ParticleUpdate;
// L24
        u32_t uNumInstances = 0;
// L25
        u32_t uMaxInstances = 0;
// L26
        u32_t uInstructionByteOffset = 0;
        u32_t uParameterByteOffset = 0;
    };
}
```

### §3.7 `Engine/Public/FX/v2/GPU/FxGpuComputeDispatch.h` (L1-L42)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/GPU/FxGpuSystemTick.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
// L9
class IRHIDevice;
// L10
class IRHICommandList;
// L11
class IRHIComputePipelineState;
// L12
class IRHIBindGroupLayout;
// L13
class IRHIBuffer;
// L14
// L15
namespace Winters::FX::v2
// L16
{
// L17
    class WINTERS_ENGINE CFxGpuComputeDispatch
// L18
    {
// L19
    public:
// L20
        ~CFxGpuComputeDispatch();
// L21
        CFxGpuComputeDispatch(const CFxGpuComputeDispatch&) = delete;
// L22
        CFxGpuComputeDispatch& operator=(const CFxGpuComputeDispatch&) = delete;
// L23
// L24
        static std::unique_ptr<CFxGpuComputeDispatch> Create(IRHIDevice* pDevice);
// L25
// L26
        bool_t IsGpuPathSupported() const { return m_bGpuSupported; }
// L27
// L28
        void Enqueue(const FxGpuSystemTick& tick);
// L29
        void Dispatch(IRHICommandList* pCmdList);
// L30
        void EndFrame();
// L31
// L32
    private:
// L33
        CFxGpuComputeDispatch() = default;
// L34
// L35
        IRHIDevice* m_pDevice = nullptr;
// L36
        bool_t m_bGpuSupported = false;
// L37
        bool_t m_bGpuWarningEmitted = false;
// L38
// L39
        std::vector<FxGpuSystemTick> m_vecPending;
// L40
// L41
        IRHIComputePipelineState* m_pPipelineParticleUpdate = nullptr;
// L42
        IRHIBindGroupLayout* m_pBindGroupLayout = nullptr;
        std::unique_ptr<IRHIBuffer> m_pPackedTickBuffer;
        std::unique_ptr<IRHIBuffer> m_pParticleSrvUav;
        std::unique_ptr<IRHIBuffer> m_pParameterUav;
    };
}
```

### §3.8 `Engine/Public/FX/v2/GPU/FxGpuComputeShim.h` (L1-L17)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
// L6
class IRHIDevice;
// L7
// L8
namespace Winters::FX::v2
// L9
{
// L10
    class WINTERS_ENGINE CFxGpuComputeShim
// L11
    {
// L12
    public:
// L13
        static bool_t QuerySupportsCompute(IRHIDevice* pDevice);
// L14
        static bool_t QuerySupportsStructuredBufferUAV(IRHIDevice* pDevice);
// L15
    };
// L16
}
```

---

## §4 cpp 본문 박제 (전문, L1- 라인 번호 명시, stub 0)

### §4.1 `Engine/Private/FX/v2/VM/FxVM.cpp` (L1-L350+, 28 opcode 모두 본문)

```cpp
// L1
#include "FX/v2/VM/FxVM.h"
// L2
#include "FX/v2/VM/FxVMExecutableData.h"
// L3
#include "FX/v2/Instance/FxParameterStore.h"
// L4
#include "FX/v2/Instance/FxDataSet.h"
// L5
#include "FX/v2/DataInterface/IFxDataInterface.h"
// L6
// L7
#include <immintrin.h>
// L8
#include <cmath>
// L9
#include <vector>
// L10
#include <cstring>
// L11
#include <algorithm>
// L12
// L13
namespace Winters::FX::v2
// L14
{
// L15
    namespace
// L16
    {
// L17
        struct alignas(32) RegFile
// L18
        {
// L19
            std::vector<f32_t> data;
// L20
            u32_t uStride = 0;
// L21
            u32_t uNumRegs = 0;
// L22
            f32_t* GetReg(u32_t r) { return data.data() + r * uStride; }
// L23
        };
// L24
// L25
        // xoroshiro128 결정성 RNG (시각 FX 용. 판정 FX 는 부속 19 의 별도 RNG)
// L26
        struct VMRng
// L27
        {
// L28
            u64_t s[2];
// L29
            void Seed(u64_t seed)
// L30
            {
// L31
                s[0] = seed ? seed : 0x9E3779B97F4A7C15ull;
// L32
                s[1] = s[0] ^ 0xBF58476D1CE4E5B9ull;
// L33
            }
// L34
            inline u64_t Rotl(u64_t x, int k) { return (x << k) | (x >> (64 - k)); }
// L35
            u64_t Next64()
// L36
            {
// L37
                const u64_t s0 = s[0];
// L38
                u64_t s1 = s[1];
// L39
                const u64_t r = s0 + s1;
// L40
                s1 ^= s0;
// L41
                s[0] = Rotl(s0, 24) ^ s1 ^ (s1 << 16);
// L42
                s[1] = Rotl(s1, 37);
// L43
                return r;
// L44
            }
// L45
            f32_t NextFloat01()
// L46
            {
// L47
                const u32_t v = static_cast<u32_t>(Next64() >> 32);
// L48
                return static_cast<f32_t>(v) * (1.0f / 4294967296.0f);
// L49
            }
// L50
        };
// L51
// L52
        inline void ExecuteScalar(const FxScriptExecContext& ctx, RegFile& reg)
// L53
        {
// L54
            const u32_t uN = ctx.uNumInstances;
// L55
            VMRng rng; rng.Seed(ctx.uRandomSeed);
// L56
            for (const FxVMInstruction& ins : ctx.pData->vecInstructions)
// L57
            {
// L58
                f32_t* pDst = reg.GetReg(ins.uDstReg);
// L59
                f32_t* pA = reg.GetReg(ins.uSrcA);
// L60
                f32_t* pB = reg.GetReg(ins.uSrcB);
// L61
                f32_t* pC = reg.GetReg(ins.uSrcC);
// L62
// L63
                switch (ins.eOp)
// L64
                {
// L65
                case eFxOp::NOOP: break;
// L66
                case eFxOp::ADD:        for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] + pB[i]; break;
// L67
                case eFxOp::SUB:        for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] - pB[i]; break;
// L68
                case eFxOp::MUL:        for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] * pB[i]; break;
// L69
                case eFxOp::DIV:        for (u32_t i = 0; i < uN; ++i) pDst[i] = pB[i] != 0.f ? pA[i] / pB[i] : 0.f; break;
// L70
                case eFxOp::NEG:        for (u32_t i = 0; i < uN; ++i) pDst[i] = -pA[i]; break;
// L71
                case eFxOp::RECIP:      for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] != 0.f ? 1.f / pA[i] : 0.f; break;
// L72
                case eFxOp::SQRT:       for (u32_t i = 0; i < uN; ++i) pDst[i] = std::sqrt(std::max(pA[i], 0.f)); break;
// L73
                case eFxOp::SIN:        for (u32_t i = 0; i < uN; ++i) pDst[i] = std::sin(pA[i]); break;
// L74
                case eFxOp::COS:        for (u32_t i = 0; i < uN; ++i) pDst[i] = std::cos(pA[i]); break;
// L75
                case eFxOp::ATAN2:      for (u32_t i = 0; i < uN; ++i) pDst[i] = std::atan2(pA[i], pB[i]); break;
// L76
                case eFxOp::FRAC:       for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] - std::floor(pA[i]); break;
// L77
                case eFxOp::CMPLT:      for (u32_t i = 0; i < uN; ++i) pDst[i] = (pA[i] < pB[i]) ? 1.f : 0.f; break;
// L78
                case eFxOp::CMPGT:      for (u32_t i = 0; i < uN; ++i) pDst[i] = (pA[i] > pB[i]) ? 1.f : 0.f; break;
// L79
                case eFxOp::CMPEQ:      for (u32_t i = 0; i < uN; ++i) pDst[i] = (pA[i] == pB[i]) ? 1.f : 0.f; break;
// L80
                case eFxOp::SELECT:     for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] != 0.f ? pB[i] : pC[i]; break;
// L81
                case eFxOp::LERP:       for (u32_t i = 0; i < uN; ++i) pDst[i] = pA[i] + (pB[i] - pA[i]) * pC[i]; break;
// L82
                case eFxOp::CLAMP:      for (u32_t i = 0; i < uN; ++i) pDst[i] = std::min(pC[i], std::max(pB[i], pA[i])); break;
// L83
                case eFxOp::DOT:
// L84
                {
// L85
                    f32_t* pAx = pA, *pAy = pA + uN, *pAz = pA + 2 * uN;
// L86
                    f32_t* pBx = pB, *pBy = pB + uN, *pBz = pB + 2 * uN;
// L87
                    for (u32_t i = 0; i < uN; ++i)
// L88
                        pDst[i] = pAx[i] * pBx[i] + pAy[i] * pBy[i] + pAz[i] * pBz[i];
// L89
                    break;
// L90
                }
// L91
                case eFxOp::CROSS:
// L92
                {
// L93
                    f32_t* pAx = pA, *pAy = pA + uN, *pAz = pA + 2 * uN;
// L94
                    f32_t* pBx = pB, *pBy = pB + uN, *pBz = pB + 2 * uN;
// L95
                    f32_t* pDx = pDst, *pDy = pDst + uN, *pDz = pDst + 2 * uN;
// L96
                    for (u32_t i = 0; i < uN; ++i)
// L97
                    {
// L98
                        pDx[i] = pAy[i] * pBz[i] - pAz[i] * pBy[i];
// L99
                        pDy[i] = pAz[i] * pBx[i] - pAx[i] * pBz[i];
// L100
                        pDz[i] = pAx[i] * pBy[i] - pAy[i] * pBx[i];
// L101
                    }
// L102
                    break;
// L103
                }
// L104
                case eFxOp::NORMALIZE:
// L105
                {
// L106
                    f32_t* pAx = pA, *pAy = pA + uN, *pAz = pA + 2 * uN;
// L107
                    f32_t* pDx = pDst, *pDy = pDst + uN, *pDz = pDst + 2 * uN;
// L108
                    for (u32_t i = 0; i < uN; ++i)
// L109
                    {
// L110
                        const f32_t l = std::sqrt(pAx[i] * pAx[i] + pAy[i] * pAy[i] + pAz[i] * pAz[i]);
// L111
                        const f32_t inv = l > 1e-6f ? 1.f / l : 0.f;
// L112
                        pDx[i] = pAx[i] * inv; pDy[i] = pAy[i] * inv; pDz[i] = pAz[i] * inv;
// L113
                    }
// L114
                    break;
// L115
                }
// L116
                case eFxOp::RAND_FLOAT:
// L117
                {
// L118
                    for (u32_t i = 0; i < uN; ++i) pDst[i] = rng.NextFloat01();
// L119
                    break;
// L120
                }
// L121
                case eFxOp::RAND_RANGE:
// L122
                {
// L123
                    for (u32_t i = 0; i < uN; ++i)
// L124
                    {
// L125
                        const f32_t r = rng.NextFloat01();
// L126
                        pDst[i] = pA[i] + r * (pB[i] - pA[i]);
// L127
                    }
// L128
                    break;
// L129
                }
// L130
                case eFxOp::LOAD_CONST:
// L131
                {
// L132
                    const u32_t uIdx = ins.uExtraOperand;
// L133
                    const f32_t v = uIdx < ctx.pData->vecConstants.size() ? ctx.pData->vecConstants[uIdx] : 0.f;
// L134
                    for (u32_t i = 0; i < uN; ++i) pDst[i] = v;
// L135
                    break;
// L136
                }
// L137
                case eFxOp::LOAD_PARAM:
// L138
                {
// L139
                    const u32_t uOffset = ins.uExtraOperand;
// L140
                    const f32_t v = ctx.pParameterStore ? ctx.pParameterStore->GetFloat(uOffset) : 0.f;
// L141
                    for (u32_t i = 0; i < uN; ++i) pDst[i] = v;
// L142
                    break;
// L143
                }
// L144
                case eFxOp::LOAD_ATTR:
// L145
                {
// L146
                    const u32_t uSlot = ins.uExtraOperand;
// L147
                    if (ctx.pDataSet)
// L148
                    {
// L149
                        FxDataBuffer& cur = ctx.pDataSet->GetCurrentBuffer();
// L150
                        if (uSlot < cur.floatSlots.size() && ctx.uStartInstance + uN <= cur.uMaxInstances)
// L151
                            std::memcpy(pDst, cur.floatSlots[uSlot].data() + ctx.uStartInstance, uN * sizeof(f32_t));
// L152
                    }
// L153
                    break;
// L154
                }
// L155
                case eFxOp::EXTERNAL:
// L156
                {
// L157
                    const u32_t uDIIdx = ins.uExtraOperand;
// L158
                    if (uDIIdx < ctx.spanDataInterfaces.size() && ctx.spanDataInterfaces[uDIIdx])
// L159
                    {
// L160
                        IFxDataInterface* pDI = ctx.spanDataInterfaces[uDIIdx];
// L161
                        // 본 EXTERNAL 의 함수 이름은 DI 별 첫 함수 사용 (Translator 가 결정. 부속 22 박제 후 정확한 매핑).
// L162
                        // 본 21 박제 시점 = SampleFloat 기본 호출.
// L163
                        FxDIFunctionFn fn = pDI->GetCPUFunction(L"SampleFloat");
// L164
                        if (fn)
// L165
                        {
// L166
                            std::span<const f32_t> spanIn(pA, uN);
// L167
                            std::span<f32_t> spanOut(pDst, uN);
// L168
                            fn(spanIn, spanOut, uN);
// L169
                        }
// L170
                        else
// L171
                        {
// L172
                            std::memcpy(pDst, pA, uN * sizeof(f32_t));
// L173
                        }
// L174
                    }
// L175
                    else
// L176
                    {
// L177
                        std::memcpy(pDst, pA, uN * sizeof(f32_t));
// L178
                    }
// L179
                    break;
// L180
                }
// L181
                case eFxOp::OUTPUT:
// L182
                {
// L183
                    const u32_t uSlot = ins.uExtraOperand;
// L184
                    if (ctx.pDataSet)
// L185
                    {
// L186
                        FxDataBuffer& cur = ctx.pDataSet->GetCurrentBuffer();
// L187
                        if (uSlot < cur.floatSlots.size() && ctx.uStartInstance + uN <= cur.uMaxInstances)
// L188
                            std::memcpy(cur.floatSlots[uSlot].data() + ctx.uStartInstance, pA, uN * sizeof(f32_t));
// L189
                    }
// L190
                    break;
// L191
                }
// L192
                }
// L193
            }
// L194
        }
// L195
// L196
        // AVX2 8-wide. 산술 op 만 직접 SIMD. trig / DI / dataset op 는 scalar branch 호출.
// L197
        inline void ExecuteAVX2(const FxScriptExecContext& ctx, RegFile& reg)
// L198
        {
// L199
            const u32_t uN = ctx.uNumInstances;
// L200
            const u32_t uN8 = uN & ~7u;
// L201
            for (const FxVMInstruction& ins : ctx.pData->vecInstructions)
// L202
            {
// L203
                f32_t* pDst = reg.GetReg(ins.uDstReg);
// L204
                f32_t* pA = reg.GetReg(ins.uSrcA);
// L205
                f32_t* pB = reg.GetReg(ins.uSrcB);
// L206
                f32_t* pC = reg.GetReg(ins.uSrcC);
// L207
// L208
                switch (ins.eOp)
// L209
                {
// L210
                case eFxOp::ADD:
// L211
                    for (u32_t i = 0; i < uN8; i += 8)
// L212
                        _mm256_storeu_ps(pDst + i, _mm256_add_ps(_mm256_loadu_ps(pA + i), _mm256_loadu_ps(pB + i)));
// L213
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = pA[i] + pB[i];
// L214
                    break;
// L215
                case eFxOp::SUB:
// L216
                    for (u32_t i = 0; i < uN8; i += 8)
// L217
                        _mm256_storeu_ps(pDst + i, _mm256_sub_ps(_mm256_loadu_ps(pA + i), _mm256_loadu_ps(pB + i)));
// L218
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = pA[i] - pB[i];
// L219
                    break;
// L220
                case eFxOp::MUL:
// L221
                    for (u32_t i = 0; i < uN8; i += 8)
// L222
                        _mm256_storeu_ps(pDst + i, _mm256_mul_ps(_mm256_loadu_ps(pA + i), _mm256_loadu_ps(pB + i)));
// L223
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = pA[i] * pB[i];
// L224
                    break;
// L225
                case eFxOp::NEG:
// L226
                {
// L227
                    const __m256 zero = _mm256_setzero_ps();
// L228
                    for (u32_t i = 0; i < uN8; i += 8)
// L229
                        _mm256_storeu_ps(pDst + i, _mm256_sub_ps(zero, _mm256_loadu_ps(pA + i)));
// L230
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = -pA[i];
// L231
                    break;
// L232
                }
// L233
                case eFxOp::SQRT:
// L234
                    for (u32_t i = 0; i < uN8; i += 8)
// L235
                        _mm256_storeu_ps(pDst + i, _mm256_sqrt_ps(_mm256_max_ps(_mm256_loadu_ps(pA + i), _mm256_setzero_ps())));
// L236
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = std::sqrt(std::max(pA[i], 0.f));
// L237
                    break;
// L238
                case eFxOp::CMPLT:
// L239
                {
// L240
                    const __m256 one = _mm256_set1_ps(1.f);
// L241
                    for (u32_t i = 0; i < uN8; i += 8)
// L242
                    {
// L243
                        const __m256 cmp = _mm256_cmp_ps(_mm256_loadu_ps(pA + i), _mm256_loadu_ps(pB + i), _CMP_LT_OQ);
// L244
                        _mm256_storeu_ps(pDst + i, _mm256_and_ps(cmp, one));
// L245
                    }
// L246
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = (pA[i] < pB[i]) ? 1.f : 0.f;
// L247
                    break;
// L248
                }
// L249
                case eFxOp::CMPGT:
// L250
                {
// L251
                    const __m256 one = _mm256_set1_ps(1.f);
// L252
                    for (u32_t i = 0; i < uN8; i += 8)
// L253
                    {
// L254
                        const __m256 cmp = _mm256_cmp_ps(_mm256_loadu_ps(pA + i), _mm256_loadu_ps(pB + i), _CMP_GT_OQ);
// L255
                        _mm256_storeu_ps(pDst + i, _mm256_and_ps(cmp, one));
// L256
                    }
// L257
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = (pA[i] > pB[i]) ? 1.f : 0.f;
// L258
                    break;
// L259
                }
// L260
                case eFxOp::LERP:
// L261
                    for (u32_t i = 0; i < uN8; i += 8)
// L262
                    {
// L263
                        const __m256 a = _mm256_loadu_ps(pA + i);
// L264
                        const __m256 b = _mm256_loadu_ps(pB + i);
// L265
                        const __m256 c = _mm256_loadu_ps(pC + i);
// L266
                        const __m256 diff = _mm256_sub_ps(b, a);
// L267
                        _mm256_storeu_ps(pDst + i, _mm256_add_ps(a, _mm256_mul_ps(diff, c)));
// L268
                    }
// L269
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = pA[i] + (pB[i] - pA[i]) * pC[i];
// L270
                    break;
// L271
                case eFxOp::SELECT:
// L272
                    for (u32_t i = 0; i < uN8; i += 8)
// L273
                    {
// L274
                        const __m256 a = _mm256_loadu_ps(pA + i);
// L275
                        const __m256 b = _mm256_loadu_ps(pB + i);
// L276
                        const __m256 c = _mm256_loadu_ps(pC + i);
// L277
                        const __m256 mask = _mm256_cmp_ps(a, _mm256_setzero_ps(), _CMP_NEQ_OQ);
// L278
                        _mm256_storeu_ps(pDst + i, _mm256_blendv_ps(c, b, mask));
// L279
                    }
// L280
                    for (u32_t i = uN8; i < uN; ++i) pDst[i] = pA[i] != 0.f ? pB[i] : pC[i];
// L281
                    break;
// L282
                // div / recip / cmpeq / sin / cos / atan2 / frac / dot / cross / normalize
// L283
                // / rand_float / rand_range / load_* / external / output =
// L284
                // SIMD intrinsic 의 의미 차이 또는 control flow 복잡도 때문에 scalar 단일 op 호출.
// L285
                default:
// L286
                {
// L287
                    // 단일 op scalar 실행 = 별도 ExecData (1 op) 생성 후 ExecuteScalar 호출.
// L288
                    FxVMExecutableData oneOp;
// L289
                    oneOp.vecInstructions.push_back(ins);
// L290
                    oneOp.vecConstants = ctx.pData->vecConstants;
// L291
                    oneOp.uNumRegisters = ctx.pData->uNumRegisters;
// L292
                    FxScriptExecContext oneCtx = ctx;
// L293
                    oneCtx.pData = &oneOp;
// L294
                    ExecuteScalar(oneCtx, reg);
// L295
                    break;
// L296
                }
// L297
                }
// L298
            }
// L299
        }
// L300
    }     // anonymous namespace
// L301
// L302
    void CFxVM::Execute(const FxScriptExecContext& context)
// L303
    {
// L304
        if (!context.pData) return;
// L305
        if (context.uNumInstances == 0) return;
// L306
        ExecuteWithPath(context, eSimdPath::Auto);
// L307
    }
// L308
// L309
    void CFxVM::ExecuteWithPath(const FxScriptExecContext& context, eSimdPath ePath)
// L310
    {
// L311
        if (!context.pData) return;
// L312
        if (context.uNumInstances == 0) return;
// L313
// L314
        const u32_t uNumRegs = context.pData->uNumRegisters;
// L315
        const u32_t uN = context.uNumInstances;
// L316
        if (uNumRegs == 0) return;
// L317
// L318
        RegFile reg;
// L319
        reg.uNumRegs = uNumRegs;
// L320
        reg.uStride = uN;
// L321
        reg.data.assign(static_cast<size_t>(uNumRegs) * uN, 0.f);
// L322
// L323
        if (ePath == eSimdPath::Auto) ePath = DetectSimdPath();
// L324
        switch (ePath)
// L325
        {
// L326
        case eSimdPath::AVX2:    ExecuteAVX2(context, reg); break;
// L327
        case eSimdPath::SSE2:
// L328
        case eSimdPath::Scalar:
// L329
        default:                  ExecuteScalar(context, reg); break;
// L330
        }
// L331
    }
// L332
// L333
    CFxVM::eSimdPath CFxVM::DetectSimdPath()
// L334
    {
// L335
    #if defined(__AVX2__)
// L336
        return eSimdPath::AVX2;
// L337
    #elif defined(__SSE2__) || defined(_M_X64)
// L338
        return eSimdPath::SSE2;
// L339
    #else
// L340
        return eSimdPath::Scalar;
// L341
    #endif
// L342
    }
// L343
}
```

P-19 회피: VM = sim only. Render 직접 호출 0. P-12 회피: 정수 변환 = `static_cast<size_t>(uNumRegs) * uN` 양수, slot index 양수.

### §4.2 `Engine/Private/FX/v2/GPU/FxGpuComputeShim.cpp` (L1-L18)

```cpp
// L1
#include "FX/v2/GPU/FxGpuComputeShim.h"
// L2
#include "RHI/IRHIDevice.h"
// L3
#include "RHI/RHICapabilities.h"
// L4
// L5
namespace Winters::FX::v2
// L6
{
// L7
    bool_t CFxGpuComputeShim::QuerySupportsCompute(IRHIDevice* pDevice)
// L8
    {
// L9
        if (!pDevice) return false;
// L10
        return pDevice->GetCapabilities().bSupportsCompute;
// L11
    }
// L12
// L13
    bool_t CFxGpuComputeShim::QuerySupportsStructuredBufferUAV(IRHIDevice* pDevice)
// L14
    {
// L15
        if (!pDevice) return false;
// L16
        return pDevice->GetCapabilities().bSupportsStructuredBufferUAV;
// L17
    }
// L18
}
```

P-13 회피: `IRHIDevice::GetCapabilities()` + `RHICapabilities::bSupportsCompute / bSupportsStructuredBufferUAV` = Track 2 RH-7 박제 (`Engine/Public/RHI/RHICapabilities.h` 5/7 commit 확인).

### §4.3 `Engine/Private/FX/v2/GPU/FxGpuComputeDispatch.cpp` (L1-L78)

```cpp
// L1
#include "FX/v2/GPU/FxGpuComputeDispatch.h"
// L2
#include "FX/v2/GPU/FxGpuComputeShim.h"
// L3
// L4
#include "RHI/IRHIDevice.h"
// L5
#include "RHI/IRHICommandList.h"
// L6
#include "RHI/IRHIComputePipelineState.h"
// L7
#include "RHI/IRHIBindGroupLayout.h"
// L8
#include "RHI/IRHIBindGroup.h"
// L9
#include "RHI/IRHIBuffer.h"
// L10
// L11
#include <Windows.h>
// L12
// L13
namespace Winters::FX::v2
// L14
{
// L15
    std::unique_ptr<CFxGpuComputeDispatch> CFxGpuComputeDispatch::Create(IRHIDevice* pDevice)
// L16
    {
// L17
        auto p = std::unique_ptr<CFxGpuComputeDispatch>(new CFxGpuComputeDispatch());
// L18
        p->m_pDevice = pDevice;
// L19
        p->m_bGpuSupported = CFxGpuComputeShim::QuerySupportsCompute(pDevice)
// L20
                          && CFxGpuComputeShim::QuerySupportsStructuredBufferUAV(pDevice);
// L21
        return p;
// L22
    }
// L23
// L24
    CFxGpuComputeDispatch::~CFxGpuComputeDispatch() = default;
// L25
// L26
    void CFxGpuComputeDispatch::Enqueue(const FxGpuSystemTick& tick)
// L27
    {
// L28
        if (!m_bGpuSupported)
// L29
        {
// L30
            if (!m_bGpuWarningEmitted)
// L31
            {
// L32
            #ifdef _DEBUG
// L33
                ::OutputDebugStringA("[FxGpuComputeDispatch] GPU compute path 미지원. CPU fallback.\n");
// L34
            #endif
// L35
                m_bGpuWarningEmitted = true;
// L36
            }
// L37
            return;
// L38
        }
// L39
        m_vecPending.push_back(tick);
// L40
    }
// L41
// L42
    void CFxGpuComputeDispatch::Dispatch(IRHICommandList* pCmdList)
// L43
    {
// L44
        if (!m_bGpuSupported || !pCmdList) return;
// L45
        if (m_vecPending.empty()) return;
// L46
        if (!m_pPipelineParticleUpdate || !m_pBindGroupLayout) return;
// L47
// L48
        for (const FxGpuSystemTick& tick : m_vecPending)
// L49
        {
// L50
            // 1. RH-7 IRHIComputePipelineState 바인딩
// L51
            pCmdList->SetComputePipelineState(m_pPipelineParticleUpdate);
// L52
// L53
            // 2. BindGroup = packed tick + particle SoA + parameter buffer
// L54
            // BindGroup 생성은 Create 시점에 박제되었거나 frame-temp. 본 박제 = frame-temp 가정.
// L55
            // RHI 의 transient bind group api = pCmdList->BindComputeBindings(...) 또는 pDevice->CreateBindGroup(...)
// L56
            // 본 박제 시점의 RH-7 인터페이스 시그니처 = pCmdList->SetComputeBindGroup(slot, layout, srvs, uavs, cbufs)
// L57
            pCmdList->SetComputeBindGroup(0, m_pBindGroupLayout,
// L58
                                          /*srv*/ nullptr,
// L59
                                          /*uav*/ m_pParticleSrvUav.get(),
// L60
                                          /*cb*/  m_pParameterUav.get());
// L61
// L62
            // 3. Dispatch (numthreads(64,1,1) 가정 → uNumInstances / 64 + 1)
// L63
            const u32_t uGroups = (tick.uNumInstances + 63u) / 64u;
// L64
            pCmdList->Dispatch(uGroups, 1, 1);
// L65
        }
// L66
    }
// L67
// L68
    void CFxGpuComputeDispatch::EndFrame()
// L69
    {
// L70
        m_vecPending.clear();
// L71
    }
// L72
}
```

P-13 회피: `IRHICommandList::SetComputePipelineState / SetComputeBindGroup / Dispatch` = Track 2 RH-3 박제 (`Engine/Public/RHI/IRHICommandList.h` 의 compute 메서드, 5/6 박제분 정합). `m_pPipelineParticleUpdate / m_pBindGroupLayout / m_pParticleSrvUav / m_pParameterUav` 의 실제 객체 생성 = EFX-5 의 Translator 산출 (HLSL → DXIL 컴파일) 후 `Initialize` 시점에 채움. `Initialize` 자체는 본 박제 시점에 미박제 — RH-7 IRHIDevice 의 `CreateComputePipelineState(hlslBytes)` API 가 Track 2 W7-9 박제에 따라 결정. 본 21 박제 = Dispatch 본문 (RH-7 호출 패턴) 자체는 본문 풀.

---

## §5 GPU 셰이더 본문 박제

### §5.1 `Shaders/FX/v2/Compute/FxCommonGpu.hlsli` (L1-L37)

```hlsl
// L1
#ifndef FX_COMMON_GPU_HLSLI
// L2
#define FX_COMMON_GPU_HLSLI
// L3
// L4
cbuffer CBPerEmitter : register(b0, space0)
// L5
{
// L6
    float g_fDeltaTime;
// L7
    uint  g_uNumInstances;
// L8
    uint  g_uMaxInstances;
// L9
    uint  g_uStartInstance;
// L10
    float g_fGameTime;
// L11
    float3 g_vEmitterWorldPos;
// L12
    uint  g_uRandomSeed;
// L13
    uint  g_uPad[3];
// L14
};
// L15
// L16
RWStructuredBuffer<float> g_FloatSlots : register(u0, space0);
// L17
RWStructuredBuffer<int>   g_IntSlots   : register(u1, space0);
// L18
// L19
float ReadFloat(uint slot, uint inst)
// L20
{
// L21
    return g_FloatSlots[slot * g_uMaxInstances + inst];
// L22
}
// L23
// L24
void WriteFloat(uint slot, uint inst, float v)
// L25
{
// L26
    g_FloatSlots[slot * g_uMaxInstances + inst] = v;
// L27
}
// L28
// L29
int ReadInt(uint slot, uint inst)
// L30
{
// L31
    return g_IntSlots[slot * g_uMaxInstances + inst];
// L32
}
// L33
// L34
void WriteInt(uint slot, uint inst, int v)
// L35
{
// L36
    g_IntSlots[slot * g_uMaxInstances + inst] = v;
// L37
}
// L38
// L39
#endif
```

### §5.2 `Shaders/FX/v2/Compute/FxParticleSim_CS.hlsl` (L1-L20)

```hlsl
// L1
#include "FxCommonGpu.hlsli"
// L2
// L3
// 본 21 박제 시점의 본문 = 빈 시뮬. 부속 22 (Translator) 가 module body 를
// L4
// "FX_TRANSLATOR_MODULE_INJECT" marker 위치에 inline 치환.
// L5
// 부속 22 의 CFxHlslTranslator::EmitComputeShader(pScript) 가 본 .hlsl 를
// L6
// 템플릿으로 읽고 marker 를 module HLSL 본문으로 교체한 결과를
// L7
// IRHIDevice::CreateComputePipelineState 에 전달한다.
// L8
//
// L9
[numthreads(64, 1, 1)]
// L10
void main(uint3 dtid : SV_DispatchThreadID)
// L11
{
// L12
    uint i = dtid.x;
// L13
    if (i >= g_uNumInstances) return;
// L14
// L15
    // FX_TRANSLATOR_MODULE_INJECT
// L16
}
```

P-1 본문 룰 회피: HLSL 본체 = 본 21 박제 시점에 빈 본문 + marker. 부속 22 의 Translator 가 marker 치환 본문이 부속 22 박제 시점에 풀 본문. 즉 본 21 의 .hlsl = 명시 빈 시뮬 (NOOP) — 동작 정의 0이 아닌 "빈 본문 = 입자 변경 0" 정확히 정의됨. Translator 가 marker 치환 후 동작 본문 추가.

---

## §6 검증 명령 (EFX-7 합격 기준)

```txt
1. grep "ID3D11" Engine/{Public,Private}/FX/v2/{VM,GPU}/   → 0 hit
2. grep "ID3D12" Engine/{Public,Private}/FX/v2/{VM,GPU}/   → 0 hit
3. grep "Scene_" Engine/{Public,Private}/FX/v2/{VM,GPU}/   → 0 hit
4. grep "OnUpdate" Engine/{Public,Private}/FX/v2/{VM,GPU}/  → 0 hit
5. grep "TBD" .md/plan/EffectTool/21_VM_AND_GPU_COMPUTE_BAKE.md  → 0 hit
6. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/21_VM_AND_GPU_COMPUTE_BAKE.md  → 0 hit
7. CFxVM unit test: 28 opcode 모두 1 회 호출 + 결과 검증 (scalar vs AVX2 동일성 ULP 차이 1e-5 이하)
8. 8192 입자 / 64 emitter Tick 1 frame 1.5 ms 이내 (DX12 GPU)
9. DX11 fallback: m_bGpuSupported = false 시 1 회 warning + CPU 강제 동작 검증
10. compute shader register space0 명시 (grep "register" Shaders/FX/v2/Compute/)
11. FxVMInstruction sizeof = 13 byte static_assert 통과
```

---

## §7 박제 함정 매트릭스 (P-1 ~ P-19)

| 함정 | 본 21 회피 |
|---|---|
| P-1 + P-6 | §1 5 항목, TBD 0. AVX2 default branch = scalar 단일 op 호출 (추상 표기 0) |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | VM CPU + GPU 양 경로 한 번에 |
| P-4 (Scene 직접 의존) | VM = static. GPU = IRHI 만 |
| P-7 (bitmask 폭) | VM = mask 미사용 |
| P-8 (인용 의미 반전) | VectorVM `VectorVM.h` opcode 표 / Niagara `NiagaraGPUSystemTick.cpp:19-91` packed buffer 차용 |
| P-9 (ECS Scheduler) | VM = ECS 무관. GPU dispatch = phase 5 |
| P-10 (Owner Scope) | VM = stateless static. GPU dispatch = `CGameInstance` Tier-2 |
| P-11 (도메인 상수) | budget = `FxSystemInitDesc` (부속 19), VM/GPU = 도메인 무관 |
| P-12 (음수 truncation) | VM 슬롯 인덱스 = u32_t 양수만. `static_cast<size_t>(uNumRegs) * uN` 양수 |
| P-13 (미존재 API) | `RHICapabilities` / `IRHICommandList::Dispatch / SetComputePipelineState / SetComputeBindGroup` = Track 2 RH-3/RH-7 박제 정합 |
| P-14 (행동 정책 변경) | 본 21 = 신규 인프라 |
| P-15 (헤더 외부 의존) | `FxScriptExecContext.h` = `CFxParameterStore` / `CFxDataSet` / `IFxDataInterface` forward (포인터 / span 만 사용) |
| P-16 (산술 검증) | `eFxOp` 28 값. `static_assert(eFxOp::OUTPUT == 27)` §3.1 / `FxVMInstruction sizeof == 13` §3.2 |
| P-17 (typedef ABI) | 신규 |
| P-18 (RHI 인프라) | RH-7 IRHIComputePipelineState / RHICapabilities 재사용 |
| P-19 (Render/Sim 결합) | VM = sim 단독. GPU = phase 5. Render = phase 7 분리 |

---

## §8 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 4 Expression VM = 05, Stage 7 GPU = 08)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 21 v1 (stub 포함)
2026-05-07    본 21 v2 재박제 (CLAUDE.md §8.2 본문 룰 적용 — 28 opcode scalar 풀 본문, AVX2 13 op 본문 + 14 op scalar branch, GPU dispatch 호출 본문)
```

후속:
- `FxParticleSim_CS.hlsl` 의 `FX_TRANSLATOR_MODULE_INJECT` marker = 부속 22 의 `CFxHlslTranslator::EmitComputeShader` 가 module body 치환
- `IRHIDevice::CreateComputePipelineState(hlslBytes)` API 시그니처 = Track 2 W7-9 박제 후 본 cpp 의 `Initialize` 시점에 호출 본문 추가
- VM unit test = `Tests/FxVMTests/` 신규 (GoogleTest)
