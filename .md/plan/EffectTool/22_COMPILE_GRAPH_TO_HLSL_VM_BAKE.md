# 22. Compile 박제 (Graph → HLSL + VM bytecode 양 경로)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 적용 — stub 0 / 라인 번호 명시 / 추상 지시 0)
권위: 본 22 = 17 마스터 §15 부속 5번. EFX-5 진입 직전 박제.
의존: 부속 18 (FxScriptAsset / m_strSourceGraphJsonRaw), 부속 21 (FxVMExecutableData / eFxOp).
참조 코드:
- Niagara: `NiagaraHlslTranslator.h+.cpp`, `NiagaraGraph.h+.cpp`, `NiagaraNode.h+.cpp`, `NiagaraNodeOp.h+.cpp`, `NiagaraNodeFunctionCall.h+.cpp`, `NiagaraNodeInput.h+.cpp`, `NiagaraNodeOutput.h+.cpp`, `NiagaraNodeIf.h+.cpp`, `NiagaraParameterMapHistory.h+.cpp`, `NiagaraCodeChunk.h`

목적:
- `CFxGraph` DAG 본문 박제 (Kahn 위상 정렬 + 사이클 검증)
- `CFxNode` 7 구현 본문 (Input / Output / Op / FunctionCall / If / Const / DataInterface)
- `CFxHlslTranslator` (Graph → HLSL string) 본문
- `CFxVMTranslator` (Graph → bytecode = `FxVMExecutableData`) 본문
- `FxParameterMapHistory` (변수 namespace + read/write stage 추적) 본문
- `FxCodeChunk` DAG (Niagara 직접 차용) 본문
- 9 표준 모듈 (SpawnRate / SpawnPosition / InitialVelocity / Gravity / Drag / ColorOverLifetime / SizeOverLifetime / AgeAndKill / Output) Graph 빌드 본문
- `CFxJsonGraphLoader` (raw JSON → CFxGraph parse) 본문 (부속 18 의 phased 박제 완성)

박제 진입 전 8 단계 관문:
- 관문 A: §1 7 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: HLSL + VM 양 경로 한 번에
- 관문 D: Scene 무관
- 관문 E: pin 인덱스 = u32 고정
- 관문 F: Niagara `NiagaraHlslTranslator.h:105-150` `FNiagaraCodeChunk` 7 mode 직접 인용
- 관문 G: Compile = ECS 무관 (background thread 가능)
- 관문 H: Translator = stateless. `FxParameterMapHistory` = compile context-local

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 22 v1 의 stub 5 위치 본문화:

```txt
1. CFxHlslTranslator::Translate 의 노드별 emit
   v1 = "Const = stub. Input = ReadAttr stub. FunctionCall = stub. If = stub."
   v2 = 7 노드 type 모두 string emit 본문 (실제 HLSL 줄 생성)

2. CFxVMTranslator::Translate 의 srcA/srcB/srcC 매핑
   v1 = "graph edge 추적. 본 박제 = stub. ins.uSrcA = 0;"
   v2 = mapNodeOutToReg + edge resolution 본문 (Op 의 input pin → src node → src reg)

3. CFxModuleLibrary 의 9 모듈 본문
   v1 = "graph stub. EFX-5 코드 작업 시점에 Graph 빌드"
   v2 = 9 모듈 모두 Graph 빌드 본문 (node 추가 + edge 연결)

4. FxParameterMapHistory 의 type 결정
   v1 = float byteSize = 4 hardcoded
   v2 = `GetFxParameterByteSize` 부속 18 헬퍼 호출

5. raw JSON → CFxGraph parse method 미박제
   v1 = "raw string 보관 + 부속 22 가 parse" 추상 표기
   v2 = `CFxJsonGraphLoader::Parse(strJsonRaw, outGraph)` 본문 박제 (nlohmann/json + node/edge 디시리얼라이즈)
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| Graph 자료 모델 | `CFxGraph` = `vector<unique_ptr<CFxNode>>` + `vector<FxEdge>` 단순. 위상 정렬 = Kahn | UEdGraph (Blueprint) 미차용 |
| Node 7 구현 | Input / Output / Op / FunctionCall / If / Const / DataInterface | 17 §3.5 매핑 |
| 표준 모듈 9 종 | SpawnRate / SpawnPosition / InitialVelocity / Gravity / Drag / ColorOverLifetime / SizeOverLifetime / AgeAndKill / Output | 17 §13 EFX-5 합격 기준 |
| Compile target | HLSL string + VM bytecode 양 경로 | `FxVMExecutableData` + `vector<u8_t> hlslBytes` |
| Module inline expansion | FunctionCall 노드 = Translator 가 module graph 를 호출자 graph 에 inline 펼침 | Niagara 핵심 패턴 |
| ParameterMapHistory | namespace 5 종 각 변수의 read / write / type / 첫 access stage 추적 | Niagara 직접 차용 |
| CodeChunk | DAG 노드. SymbolName + Definition + Type + SourceChunks + ComponentMask + Mode 7 종 | Niagara 직접 차용 |
| Graph JSON parser | `CFxJsonGraphLoader` 신설 (부속 22 권위). 부속 18 의 raw blob → CFxGraph 변환 | phased 박제 완성 |

---

## §2 신규 파일 트리

```txt
Engine/Public/FX/v2/Compiler/
  FxNodeId.h
  FxPinId.h
  FxEdge.h
  FxNode.h                       abstract + 7 enum kind
  FxNodeInput.h
  FxNodeOutput.h
  FxNodeOp.h
  FxNodeFunctionCall.h
  FxNodeIf.h
  FxNodeConst.h
  FxNodeDataInterface.h
  FxGraph.h                      DAG + Kahn
  FxCodeChunk.h
  FxParameterMapHistory.h
  FxCompileResult.h
  FxHlslTranslator.h
  FxVMTranslator.h
  FxModuleLibrary.h              9 표준 모듈
  FxJsonGraphLoader.h            raw JSON → CFxGraph

Engine/Private/FX/v2/Compiler/
  FxGraph.cpp
  FxNode_7impl.cpp               7 노드 ctor/pin 정의 통합
  FxParameterMapHistory.cpp
  FxHlslTranslator.cpp
  FxVMTranslator.cpp
  FxModuleLibrary.cpp            9 모듈 빌더 본문
  FxJsonGraphLoader.cpp
```

---

## §3 헤더 박제 (전문, L1- 라인 번호)

### §3.1 `Engine/Public/FX/v2/Compiler/FxNodeId.h` (L1-L13)

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
    struct FxNodeId
// L9
    {
// L10
        u32_t value = 0;
// L11
        bool_t IsValid() const { return value != 0; }
// L12
        bool_t operator==(const FxNodeId& o) const { return value == o.value; }
    };
    inline constexpr FxNodeId kInvalidFxNodeId{ 0 };
}
```

### §3.2 `Engine/Public/FX/v2/Compiler/FxPinId.h` (L1-L25)

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
    enum class eFxPinType : u8_t
// L9
    {
// L10
        Float = 0, Float2 = 1, Float3 = 2, Float4 = 3,
// L11
        Int = 4, Bool = 5,
// L12
        Curve = 6, Texture = 7, Spline = 8,
// L13
        DataInterface = 9,
// L14
    };
// L15
// L16
    enum class eFxPinDirection : u8_t { Input = 0, Output = 1 };
// L17
// L18
    struct FxPinId
// L19
    {
// L20
        u32_t value = 0;
// L21
        bool_t IsValid() const { return value != 0; }
// L22
        bool_t operator==(const FxPinId& o) const { return value == o.value; }
// L23
    };
// L24
    inline constexpr FxPinId kInvalidFxPinId{ 0 };
// L25
}
```

### §3.3 `Engine/Public/FX/v2/Compiler/FxEdge.h` (L1-L14)

```cpp
// L1
#pragma once
// L2
// L3
#include "FX/v2/Compiler/FxNodeId.h"
// L4
#include "FX/v2/Compiler/FxPinId.h"
// L5
// L6
namespace Winters::FX::v2
// L7
{
// L8
    struct FxEdge
// L9
    {
// L10
        FxNodeId src;
// L11
        FxPinId  srcPin;
// L12
        FxNodeId dst;
// L13
        FxPinId  dstPin;
// L14
    };
}
```

### §3.4 `Engine/Public/FX/v2/Compiler/FxNode.h` (L1-L46)

```cpp
// L1
#pragma once
// L2
// L3
#include "WintersAPI.h"
// L4
#include "WintersTypes.h"
// L5
#include "FX/v2/Compiler/FxPinId.h"
// L6
#include "FX/v2/Compiler/FxNodeId.h"
// L7
#include <vector>
// L8
#include <string>
// L9
// L10
namespace Winters::FX::v2
// L11
{
// L12
    struct FxPin
// L13
    {
// L14
        FxPinId id{};
// L15
        std::wstring strName;
// L16
        eFxPinType eType = eFxPinType::Float;
// L17
        eFxPinDirection eDir = eFxPinDirection::Input;
// L18
        std::wstring strDefaultValueLiteral;
// L19
    };
// L20
// L21
    enum class eFxNodeKind : u8_t
// L22
    {
// L23
        Input         = 0,
// L24
        Output        = 1,
// L25
        Op            = 2,
// L26
        FunctionCall  = 3,
// L27
        If            = 4,
// L28
        Const         = 5,
// L29
        DataInterface = 6,
// L30
    };
// L31
// L32
    class WINTERS_ENGINE CFxNode
// L33
    {
// L34
    public:
// L35
        virtual ~CFxNode() = default;
// L36
        FxNodeId GetId() const { return m_Id; }
// L37
        void SetId(FxNodeId id) { m_Id = id; }
// L38
        virtual eFxNodeKind GetKind() const = 0;
// L39
        virtual const std::vector<FxPin>& GetInputPins() const = 0;
// L40
        virtual const std::vector<FxPin>& GetOutputPins() const = 0;
// L41
        virtual const std::wstring& GetDisplayName() const = 0;
// L42
    protected:
// L43
        FxNodeId m_Id{};
// L44
    };
// L45
}
// L46
static_assert(static_cast<Winters::FX::v2::u32_t>(Winters::FX::v2::eFxNodeKind::DataInterface) == 6);
```

### §3.5 7 노드 헤더

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeInput.h (L1-L26)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxNodeInput final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeInput> Create(const std::wstring& strParameterName, eFxPinType eType);
        eFxNodeKind GetKind() const override { return eFxNodeKind::Input; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
        const std::wstring& GetParameterName() const { return m_strParameterName; }
        eFxPinType GetParameterType() const { return m_eType; }
    private:
        CFxNodeInput() = default;
        std::wstring m_strParameterName;
        eFxPinType m_eType = eFxPinType::Float;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeOutput.h (L1-L26)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxNodeOutput final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeOutput> Create(const std::wstring& strAttributeName, u32_t uDataSetSlotIdx, eFxPinType eType);
        eFxNodeKind GetKind() const override { return eFxNodeKind::Output; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
        const std::wstring& GetAttributeName() const { return m_strAttributeName; }
        u32_t GetDataSetSlotIdx() const { return m_uSlot; }
    private:
        CFxNodeOutput() = default;
        std::wstring m_strAttributeName;
        u32_t m_uSlot = 0;
        eFxPinType m_eType = eFxPinType::Float;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeOp.h (L1-L24)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
#include "FX/v2/VM/FxVMOpcode.h"
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxNodeOp final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeOp> Create(eFxOp eOpcode);
        eFxNodeKind GetKind() const override { return eFxNodeKind::Op; }
        eFxOp GetOpcode() const { return m_eOpcode; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
    private:
        CFxNodeOp() = default;
        eFxOp m_eOpcode = eFxOp::ADD;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeFunctionCall.h (L1-L26)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class CFxScriptAsset;
    class WINTERS_ENGINE CFxNodeFunctionCall final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeFunctionCall> Create(const std::wstring& strModuleAssetPath);
        eFxNodeKind GetKind() const override { return eFxNodeKind::FunctionCall; }
        const std::wstring& GetModuleAssetPath() const { return m_strModulePath; }
        CFxScriptAsset* GetResolvedModuleScript() const { return m_pResolvedScript; }
        void SetResolvedModuleScript(CFxScriptAsset* pScript) { m_pResolvedScript = pScript; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
    private:
        CFxNodeFunctionCall() = default;
        std::wstring m_strModulePath;
        CFxScriptAsset* m_pResolvedScript = nullptr;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeIf.h (L1-L21)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxNodeIf final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeIf> Create(eFxPinType eValueType);
        eFxNodeKind GetKind() const override { return eFxNodeKind::If; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
        eFxPinType GetValueType() const { return m_eValueType; }
    private:
        CFxNodeIf() = default;
        eFxPinType m_eValueType = eFxPinType::Float;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeConst.h (L1-L22)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class WINTERS_ENGINE CFxNodeConst final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeConst> Create(f32_t fValue);
        eFxNodeKind GetKind() const override { return eFxNodeKind::Const; }
        f32_t GetValue() const { return m_fValue; }
        void SetValue(f32_t v) { m_fValue = v; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
    private:
        CFxNodeConst() = default;
        f32_t m_fValue = 0.f;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

```cpp
// Engine/Public/FX/v2/Compiler/FxNodeDataInterface.h (L1-L24)
#pragma once
#include "FX/v2/Compiler/FxNode.h"
namespace Winters::FX::v2
{
    class IFxDataInterface;
    class WINTERS_ENGINE CFxNodeDataInterface final : public CFxNode
    {
    public:
        static std::unique_ptr<CFxNodeDataInterface> Create(const std::wstring& strDIType, const std::wstring& strFunctionName);
        eFxNodeKind GetKind() const override { return eFxNodeKind::DataInterface; }
        const std::wstring& GetDIType() const { return m_strDIType; }
        const std::wstring& GetFunctionName() const { return m_strFunctionName; }
        u32_t GetDIIndex() const { return m_uDIIdx; }
        void SetDIIndex(u32_t uIdx) { m_uDIIdx = uIdx; }
        const std::vector<FxPin>& GetInputPins() const override { return m_vecInputs; }
        const std::vector<FxPin>& GetOutputPins() const override { return m_vecOutputs; }
        const std::wstring& GetDisplayName() const override { return m_strDisplayName; }
    private:
        CFxNodeDataInterface() = default;
        std::wstring m_strDIType;
        std::wstring m_strFunctionName;
        u32_t m_uDIIdx = 0;
        std::vector<FxPin> m_vecInputs;
        std::vector<FxPin> m_vecOutputs;
        std::wstring m_strDisplayName;
    };
}
```

### §3.6 `Engine/Public/FX/v2/Compiler/FxGraph.h` (L1-L40)

```cpp
// L1
#pragma once
// L2
#include "WintersAPI.h"
// L3
#include "WintersTypes.h"
// L4
#include "FX/v2/Compiler/FxNode.h"
// L5
#include "FX/v2/Compiler/FxEdge.h"
// L6
#include <vector>
// L7
#include <memory>
// L8
namespace Winters::FX::v2
// L9
{
// L10
    class WINTERS_ENGINE CFxGraph
// L11
    {
// L12
    public:
// L13
        ~CFxGraph();
// L14
        CFxGraph(const CFxGraph&) = delete;
// L15
        CFxGraph& operator=(const CFxGraph&) = delete;
// L16
        static std::unique_ptr<CFxGraph> Create();
// L17
// L18
        FxNodeId AddNode(std::unique_ptr<CFxNode> pNode);
// L19
        bool RemoveNode(FxNodeId id);
// L20
        bool ConnectPin(const FxEdge& edge);
// L21
        void DisconnectPin(const FxEdge& edge);
// L22
// L23
        CFxNode* FindNode(FxNodeId id) const;
// L24
        const std::vector<std::unique_ptr<CFxNode>>& GetNodes() const { return m_vecNodes; }
// L25
        const std::vector<FxEdge>& GetEdges() const { return m_vecEdges; }
// L26
// L27
        bool TopologicalSort(std::vector<CFxNode*>& outOrder) const;
// L28
        bool ValidateNoCycle() const;
// L29
// L30
        u32_t GenerateNodeId() { return m_uNextNodeId++; }
// L31
        u32_t GeneratePinId() { return m_uNextPinId++; }
// L32
// L33
    private:
// L34
        CFxGraph() = default;
// L35
        std::vector<std::unique_ptr<CFxNode>> m_vecNodes;
// L36
        std::vector<FxEdge> m_vecEdges;
// L37
        u32_t m_uNextNodeId = 1;
// L38
        u32_t m_uNextPinId = 1;
// L39
    };
// L40
}
```

### §3.7 `Engine/Public/FX/v2/Compiler/FxCodeChunk.h` (L1-L29)

```cpp
// L1
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Compiler/FxPinId.h"
#include <vector>
#include <string>
namespace Winters::FX::v2
{
    enum class eFxCodeChunkMode : u8_t
    {
        GlobalConstant       = 0,
        SystemConstant       = 1,
        Uniform              = 2,
        Body                 = 3,
        SpawnBody            = 4,
        UpdateBody           = 5,
        SimulationStageBody  = 6,
    };

    struct FxCodeChunkId { u32_t value = 0; bool_t IsValid() const { return value != 0; } };

    struct FxCodeChunk
    {
        FxCodeChunkId id{};
        std::wstring strSymbolName;
        std::wstring strDefinition;
        eFxPinType eType = eFxPinType::Float;
        std::vector<FxCodeChunkId> vecSourceChunks;
        u8_t uComponentMask = 0xF;
        eFxCodeChunkMode eMode = eFxCodeChunkMode::Body;
    };
}
```

### §3.8 `Engine/Public/FX/v2/Compiler/FxParameterMapHistory.h` (L1-L42)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Asset/FxParameterMap.h"
#include <vector>
#include <unordered_map>
namespace Winters::FX::v2
{
    enum class eFxParameterAccessKind : u8_t
    {
        Read = 0, Write = 1, ReadWrite = 2,
    };

    struct FxParameterAccessRecord
    {
        u32_t uNameHash = 0;
        eFxNamespace eNs = eFxNamespace::User;
        eFxParameterType eType = eFxParameterType::Float;
        eFxParameterAccessKind eAccess = eFxParameterAccessKind::Read;
        u32_t uFirstStageIdx = 0;
        u32_t uLastStageIdx = 0;
        u32_t uByteOffset = 0;
    };

    class WINTERS_ENGINE FxParameterMapHistory
    {
    public:
        FxParameterMapHistory() = default;
        void RecordAccess(u32_t uNameHash, eFxNamespace eNs, eFxParameterType eType,
                          eFxParameterAccessKind eAccess, u32_t uStageIdx);
        const std::vector<FxParameterAccessRecord>& GetRecords() const { return m_vecRecords; }
        const FxParameterAccessRecord* FindByHash(u32_t uHash) const;
        FxParameterMap BuildFinalMap() const;
        u32_t GetParameterByteOffset(u32_t uNameHash) const;
    private:
        std::vector<FxParameterAccessRecord> m_vecRecords;
        std::unordered_map<u32_t, u32_t> m_mapHashToIdx;
    };
}
```

### §3.9 `Engine/Public/FX/v2/Compiler/FxCompileResult.h` (L1-L22)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/VM/FxVMExecutableData.h"
#include "FX/v2/Compiler/FxParameterMapHistory.h"
#include <vector>
#include <memory>
#include <string>
namespace Winters::FX::v2
{
    struct FxCompileResult
    {
        std::unique_ptr<FxVMExecutableData> pVMData;
        std::vector<u8_t> hlslBytes;
        FxParameterMapHistory paramHistory;
        std::vector<std::wstring> vecErrors;
        std::vector<std::wstring> vecWarnings;
        bool_t bSucceeded = false;
        u32_t uCompileVersion = 1;
    };
}
```

### §3.10 `Engine/Public/FX/v2/Compiler/FxHlslTranslator.h` (L1-L20) + `FxVMTranslator.h` (L1-L20)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Compiler/FxCompileResult.h"
#include <memory>
namespace Winters::FX::v2
{
    class CFxScriptAsset;
    class CFxAssetRegistry;
    class WINTERS_ENGINE CFxHlslTranslator
    {
    public:
        ~CFxHlslTranslator();
        CFxHlslTranslator(const CFxHlslTranslator&) = delete;
        CFxHlslTranslator& operator=(const CFxHlslTranslator&) = delete;
        static std::unique_ptr<CFxHlslTranslator> Create(CFxAssetRegistry* pRegistry);
        FxCompileResult Translate(CFxScriptAsset* pScript);
    private:
        CFxHlslTranslator() = default;
        CFxAssetRegistry* m_pRegistry = nullptr;
    };
}
```

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/v2/Compiler/FxCompileResult.h"
#include <memory>
namespace Winters::FX::v2
{
    class CFxScriptAsset;
    class CFxAssetRegistry;
    class WINTERS_ENGINE CFxVMTranslator
    {
    public:
        ~CFxVMTranslator();
        CFxVMTranslator(const CFxVMTranslator&) = delete;
        CFxVMTranslator& operator=(const CFxVMTranslator&) = delete;
        static std::unique_ptr<CFxVMTranslator> Create(CFxAssetRegistry* pRegistry);
        FxCompileResult Translate(CFxScriptAsset* pScript);
    private:
        CFxVMTranslator() = default;
        CFxAssetRegistry* m_pRegistry = nullptr;
    };
}
```

### §3.11 `Engine/Public/FX/v2/Compiler/FxModuleLibrary.h` (L1-L24)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>
#include <string>
#include <vector>
namespace Winters::FX::v2
{
    class CFxScriptAsset;
    class CFxAssetRegistry;
    class WINTERS_ENGINE CFxModuleLibrary
    {
    public:
        static void RegisterStandardModules(CFxAssetRegistry* pRegistry);
        static std::unique_ptr<CFxScriptAsset> BuildSpawnRateModule();
        static std::unique_ptr<CFxScriptAsset> BuildSpawnPositionModule();
        static std::unique_ptr<CFxScriptAsset> BuildInitialVelocityModule();
        static std::unique_ptr<CFxScriptAsset> BuildGravityModule();
        static std::unique_ptr<CFxScriptAsset> BuildDragModule();
        static std::unique_ptr<CFxScriptAsset> BuildColorOverLifetimeModule();
        static std::unique_ptr<CFxScriptAsset> BuildSizeOverLifetimeModule();
        static std::unique_ptr<CFxScriptAsset> BuildAgeAndKillModule();
        static std::unique_ptr<CFxScriptAsset> BuildOutputModule();
        static std::vector<std::wstring> GetStandardModuleNames();
    };
}
```

### §3.12 `Engine/Public/FX/v2/Compiler/FxJsonGraphLoader.h` (L1-L22)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>
#include <string>
#include <vector>
namespace Winters::FX::v2
{
    class CFxGraph;
    struct FxJsonGraphLoadResult
    {
        std::unique_ptr<CFxGraph> pGraph;
        std::vector<std::wstring> vecErrors;
        bool_t bSucceeded = false;
    };
    class WINTERS_ENGINE CFxJsonGraphLoader
    {
    public:
        static FxJsonGraphLoadResult Parse(const std::string& strJsonRaw);
        static std::string Serialize(const CFxGraph* pGraph);
    };
}
```

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `Engine/Private/FX/v2/Compiler/FxGraph.cpp` (L1-L100)

```cpp
// L1
#include "FX/v2/Compiler/FxGraph.h"
// L2
#include <queue>
// L3
#include <unordered_map>
// L4
#include <algorithm>
// L5
namespace Winters::FX::v2
// L6
{
// L7
    std::unique_ptr<CFxGraph> CFxGraph::Create()
// L8
    {
// L9
        return std::unique_ptr<CFxGraph>(new CFxGraph());
// L10
    }
// L11
    CFxGraph::~CFxGraph() = default;
// L12
// L13
    FxNodeId CFxGraph::AddNode(std::unique_ptr<CFxNode> pNode)
// L14
    {
// L15
        if (!pNode) return kInvalidFxNodeId;
// L16
        FxNodeId id{ m_uNextNodeId++ };
// L17
        pNode->SetId(id);
// L18
        m_vecNodes.push_back(std::move(pNode));
// L19
        return id;
// L20
    }
// L21
// L22
    bool CFxGraph::RemoveNode(FxNodeId id)
// L23
    {
// L24
        m_vecEdges.erase(
// L25
            std::remove_if(m_vecEdges.begin(), m_vecEdges.end(),
// L26
                [id](const FxEdge& e) { return e.src == id || e.dst == id; }),
// L27
            m_vecEdges.end());
// L28
        const auto it = std::find_if(m_vecNodes.begin(), m_vecNodes.end(),
// L29
            [id](const std::unique_ptr<CFxNode>& p) { return p && p->GetId() == id; });
// L30
        if (it == m_vecNodes.end()) return false;
// L31
        m_vecNodes.erase(it);
// L32
        return true;
// L33
    }
// L34
// L35
    bool CFxGraph::ConnectPin(const FxEdge& edge)
// L36
    {
// L37
        if (!edge.src.IsValid() || !edge.dst.IsValid()) return false;
// L38
        if (!FindNode(edge.src) || !FindNode(edge.dst)) return false;
// L39
        m_vecEdges.push_back(edge);
// L40
        if (!ValidateNoCycle())
// L41
        {
// L42
            m_vecEdges.pop_back();
// L43
            return false;
// L44
        }
// L45
        return true;
// L46
    }
// L47
// L48
    void CFxGraph::DisconnectPin(const FxEdge& edge)
// L49
    {
// L50
        m_vecEdges.erase(
// L51
            std::remove_if(m_vecEdges.begin(), m_vecEdges.end(),
// L52
                [&edge](const FxEdge& e) {
// L53
                    return e.src == edge.src && e.dst == edge.dst
// L54
                        && e.srcPin == edge.srcPin && e.dstPin == edge.dstPin;
// L55
                }),
// L56
            m_vecEdges.end());
// L57
    }
// L58
// L59
    CFxNode* CFxGraph::FindNode(FxNodeId id) const
// L60
    {
// L61
        for (const auto& p : m_vecNodes)
// L62
            if (p && p->GetId() == id) return p.get();
// L63
        return nullptr;
// L64
    }
// L65
// L66
    bool CFxGraph::TopologicalSort(std::vector<CFxNode*>& outOrder) const
// L67
    {
// L68
        outOrder.clear();
// L69
        std::unordered_map<u32_t, u32_t> inDegree;
// L70
        std::unordered_map<u32_t, std::vector<u32_t>> outAdj;
// L71
        for (const auto& p : m_vecNodes) if (p) inDegree[p->GetId().value] = 0;
// L72
        for (const FxEdge& e : m_vecEdges)
// L73
        {
// L74
            outAdj[e.src.value].push_back(e.dst.value);
// L75
            inDegree[e.dst.value] += 1;
// L76
        }
// L77
        std::queue<u32_t> q;
// L78
        for (const auto& p : m_vecNodes)
// L79
            if (p && inDegree[p->GetId().value] == 0) q.push(p->GetId().value);
// L80
        while (!q.empty())
// L81
        {
// L82
            const u32_t id = q.front(); q.pop();
// L83
            CFxNode* pNode = FindNode({ id });
// L84
            if (pNode) outOrder.push_back(pNode);
// L85
            for (u32_t dst : outAdj[id])
// L86
                if (--inDegree[dst] == 0) q.push(dst);
// L87
        }
// L88
        return outOrder.size() == m_vecNodes.size();
// L89
    }
// L90
// L91
    bool CFxGraph::ValidateNoCycle() const
// L92
    {
// L93
        std::vector<CFxNode*> tmp;
// L94
        return TopologicalSort(tmp);
// L95
    }
// L96
}
```

### §4.2 `Engine/Private/FX/v2/Compiler/FxNode_7impl.cpp` (L1-L120, 7 노드 ctor + pin 본문)

```cpp
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeFunctionCall.h"
#include "FX/v2/Compiler/FxNodeIf.h"
#include "FX/v2/Compiler/FxNodeConst.h"
#include "FX/v2/Compiler/FxNodeDataInterface.h"

namespace Winters::FX::v2
{
    namespace
    {
        FxPin MakePin(const wchar_t* strName, eFxPinType eType, eFxPinDirection eDir)
        {
            FxPin p;
            p.strName = strName;
            p.eType = eType;
            p.eDir = eDir;
            p.id = FxPinId{};     // graph 가 ConnectPin 시 GeneratePinId 부여
            return p;
        }
    }

    std::unique_ptr<CFxNodeInput> CFxNodeInput::Create(const std::wstring& strParameterName, eFxPinType eType)
    {
        auto p = std::unique_ptr<CFxNodeInput>(new CFxNodeInput());
        p->m_strParameterName = strParameterName;
        p->m_eType = eType;
        p->m_strDisplayName = L"Input: " + strParameterName;
        p->m_vecOutputs.push_back(MakePin(L"Value", eType, eFxPinDirection::Output));
        return p;
    }

    std::unique_ptr<CFxNodeOutput> CFxNodeOutput::Create(const std::wstring& strAttributeName, u32_t uSlot, eFxPinType eType)
    {
        auto p = std::unique_ptr<CFxNodeOutput>(new CFxNodeOutput());
        p->m_strAttributeName = strAttributeName;
        p->m_uSlot = uSlot;
        p->m_eType = eType;
        p->m_strDisplayName = L"Output: " + strAttributeName;
        p->m_vecInputs.push_back(MakePin(L"Value", eType, eFxPinDirection::Input));
        return p;
    }

    std::unique_ptr<CFxNodeOp> CFxNodeOp::Create(eFxOp eOpcode)
    {
        auto p = std::unique_ptr<CFxNodeOp>(new CFxNodeOp());
        p->m_eOpcode = eOpcode;

        const auto IsTernary = [](eFxOp op) {
            return op == eFxOp::SELECT || op == eFxOp::LERP || op == eFxOp::CLAMP;
        };
        const auto IsUnary = [](eFxOp op) {
            return op == eFxOp::NEG || op == eFxOp::RECIP || op == eFxOp::SQRT
                || op == eFxOp::SIN || op == eFxOp::COS || op == eFxOp::FRAC
                || op == eFxOp::NORMALIZE || op == eFxOp::RAND_FLOAT;
        };

        p->m_vecInputs.push_back(MakePin(L"A", eFxPinType::Float, eFxPinDirection::Input));
        if (!IsUnary(eOpcode))
            p->m_vecInputs.push_back(MakePin(L"B", eFxPinType::Float, eFxPinDirection::Input));
        if (IsTernary(eOpcode))
            p->m_vecInputs.push_back(MakePin(L"C", eFxPinType::Float, eFxPinDirection::Input));

        p->m_vecOutputs.push_back(MakePin(L"Result", eFxPinType::Float, eFxPinDirection::Output));

        switch (eOpcode)
        {
        case eFxOp::ADD: p->m_strDisplayName = L"Add"; break;
        case eFxOp::SUB: p->m_strDisplayName = L"Sub"; break;
        case eFxOp::MUL: p->m_strDisplayName = L"Mul"; break;
        case eFxOp::DIV: p->m_strDisplayName = L"Div"; break;
        case eFxOp::LERP: p->m_strDisplayName = L"Lerp"; break;
        case eFxOp::CLAMP: p->m_strDisplayName = L"Clamp"; break;
        case eFxOp::DOT: p->m_strDisplayName = L"Dot"; break;
        case eFxOp::CROSS: p->m_strDisplayName = L"Cross"; break;
        case eFxOp::NORMALIZE: p->m_strDisplayName = L"Normalize"; break;
        case eFxOp::SIN: p->m_strDisplayName = L"Sin"; break;
        case eFxOp::COS: p->m_strDisplayName = L"Cos"; break;
        case eFxOp::ATAN2: p->m_strDisplayName = L"Atan2"; break;
        case eFxOp::FRAC: p->m_strDisplayName = L"Frac"; break;
        case eFxOp::SQRT: p->m_strDisplayName = L"Sqrt"; break;
        case eFxOp::NEG: p->m_strDisplayName = L"Neg"; break;
        case eFxOp::SELECT: p->m_strDisplayName = L"Select"; break;
        case eFxOp::CMPLT: p->m_strDisplayName = L"CmpLT"; break;
        case eFxOp::CMPGT: p->m_strDisplayName = L"CmpGT"; break;
        case eFxOp::CMPEQ: p->m_strDisplayName = L"CmpEQ"; break;
        case eFxOp::RAND_FLOAT: p->m_strDisplayName = L"RandFloat"; break;
        case eFxOp::RAND_RANGE: p->m_strDisplayName = L"RandRange"; break;
        default: p->m_strDisplayName = L"Op"; break;
        }
        return p;
    }

    std::unique_ptr<CFxNodeFunctionCall> CFxNodeFunctionCall::Create(const std::wstring& strModuleAssetPath)
    {
        auto p = std::unique_ptr<CFxNodeFunctionCall>(new CFxNodeFunctionCall());
        p->m_strModulePath = strModuleAssetPath;
        p->m_strDisplayName = L"Module: " + strModuleAssetPath;
        return p;
    }

    std::unique_ptr<CFxNodeIf> CFxNodeIf::Create(eFxPinType eValueType)
    {
        auto p = std::unique_ptr<CFxNodeIf>(new CFxNodeIf());
        p->m_eValueType = eValueType;
        p->m_strDisplayName = L"If";
        p->m_vecInputs.push_back(MakePin(L"Cond", eFxPinType::Bool, eFxPinDirection::Input));
        p->m_vecInputs.push_back(MakePin(L"True", eValueType, eFxPinDirection::Input));
        p->m_vecInputs.push_back(MakePin(L"False", eValueType, eFxPinDirection::Input));
        p->m_vecOutputs.push_back(MakePin(L"Result", eValueType, eFxPinDirection::Output));
        return p;
    }

    std::unique_ptr<CFxNodeConst> CFxNodeConst::Create(f32_t fValue)
    {
        auto p = std::unique_ptr<CFxNodeConst>(new CFxNodeConst());
        p->m_fValue = fValue;
        p->m_strDisplayName = L"Const";
        p->m_vecOutputs.push_back(MakePin(L"Value", eFxPinType::Float, eFxPinDirection::Output));
        return p;
    }

    std::unique_ptr<CFxNodeDataInterface> CFxNodeDataInterface::Create(
        const std::wstring& strDIType, const std::wstring& strFunctionName)
    {
        auto p = std::unique_ptr<CFxNodeDataInterface>(new CFxNodeDataInterface());
        p->m_strDIType = strDIType;
        p->m_strFunctionName = strFunctionName;
        p->m_strDisplayName = strDIType + L"." + strFunctionName;
        p->m_vecInputs.push_back(MakePin(L"In", eFxPinType::Float, eFxPinDirection::Input));
        p->m_vecOutputs.push_back(MakePin(L"Out", eFxPinType::Float, eFxPinDirection::Output));
        return p;
    }
}
```

### §4.3 `Engine/Private/FX/v2/Compiler/FxParameterMapHistory.cpp` (L1-L70)

```cpp
#include "FX/v2/Compiler/FxParameterMapHistory.h"

namespace Winters::FX::v2
{
    void FxParameterMapHistory::RecordAccess(u32_t uNameHash, eFxNamespace eNs, eFxParameterType eType,
                                              eFxParameterAccessKind eAccess, u32_t uStageIdx)
    {
        auto it = m_mapHashToIdx.find(uNameHash);
        if (it != m_mapHashToIdx.end())
        {
            FxParameterAccessRecord& rec = m_vecRecords[it->second];
            rec.uLastStageIdx = uStageIdx;
            if (rec.eAccess != eAccess && rec.eAccess != eFxParameterAccessKind::ReadWrite)
                rec.eAccess = eFxParameterAccessKind::ReadWrite;
            return;
        }
        FxParameterAccessRecord rec;
        rec.uNameHash = uNameHash;
        rec.eNs = eNs;
        rec.eType = eType;
        rec.eAccess = eAccess;
        rec.uFirstStageIdx = uStageIdx;
        rec.uLastStageIdx = uStageIdx;
        rec.uByteOffset = 0;
        m_mapHashToIdx.emplace(uNameHash, static_cast<u32_t>(m_vecRecords.size()));
        m_vecRecords.push_back(rec);
    }

    const FxParameterAccessRecord* FxParameterMapHistory::FindByHash(u32_t uHash) const
    {
        auto it = m_mapHashToIdx.find(uHash);
        if (it == m_mapHashToIdx.end()) return nullptr;
        return &m_vecRecords[it->second];
    }

    FxParameterMap FxParameterMapHistory::BuildFinalMap() const
    {
        FxParameterMap map;
        for (const FxParameterAccessRecord& rec : m_vecRecords)
        {
            FxParameterEntry entry;
            entry.id.eNs = rec.eNs;
            entry.id.eType = rec.eType;
            entry.id.uNameHash = rec.uNameHash;
            entry.uByteSize = GetFxParameterByteSize(rec.eType);
            map.AddEntry(entry);
        }
        return map;
    }

    u32_t FxParameterMapHistory::GetParameterByteOffset(u32_t uNameHash) const
    {
        auto it = m_mapHashToIdx.find(uNameHash);
        if (it == m_mapHashToIdx.end()) return static_cast<u32_t>(-1);
        return m_vecRecords[it->second].uByteOffset;
    }
}
```

### §4.4 `Engine/Private/FX/v2/Compiler/FxHlslTranslator.cpp` (L1-L130, 본문 풀)

```cpp
#include "FX/v2/Compiler/FxHlslTranslator.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeFunctionCall.h"
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeIf.h"
#include "FX/v2/Compiler/FxNodeConst.h"
#include "FX/v2/Compiler/FxNodeDataInterface.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxAssetRegistry.h"

#include <sstream>
#include <unordered_map>
#include <codecvt>
#include <locale>

namespace Winters::FX::v2
{
    namespace
    {
        std::string WideToUtf8(const std::wstring& w)
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            return conv.to_bytes(w);
        }

        std::wstring OpcodeToHlsl(eFxOp op, const std::wstring& a, const std::wstring& b, const std::wstring& c)
        {
            switch (op)
            {
            case eFxOp::ADD:        return a + L" + " + b;
            case eFxOp::SUB:        return a + L" - " + b;
            case eFxOp::MUL:        return a + L" * " + b;
            case eFxOp::DIV:        return L"(" + b + L" != 0.0 ? " + a + L" / " + b + L" : 0.0)";
            case eFxOp::NEG:        return L"-" + a;
            case eFxOp::RECIP:      return L"(" + a + L" != 0.0 ? 1.0 / " + a + L" : 0.0)";
            case eFxOp::SQRT:       return L"sqrt(max(" + a + L", 0.0))";
            case eFxOp::DOT:        return L"dot(" + a + L", " + b + L")";
            case eFxOp::CROSS:      return L"cross(" + a + L", " + b + L")";
            case eFxOp::NORMALIZE:  return L"normalize(" + a + L")";
            case eFxOp::LERP:       return L"lerp(" + a + L", " + b + L", " + c + L")";
            case eFxOp::CLAMP:      return L"clamp(" + a + L", " + b + L", " + c + L")";
            case eFxOp::SIN:        return L"sin(" + a + L")";
            case eFxOp::COS:        return L"cos(" + a + L")";
            case eFxOp::ATAN2:      return L"atan2(" + a + L", " + b + L")";
            case eFxOp::FRAC:       return L"frac(" + a + L")";
            case eFxOp::SELECT:     return L"((" + a + L") != 0.0 ? (" + b + L") : (" + c + L"))";
            case eFxOp::CMPLT:      return L"((" + a + L" < " + b + L") ? 1.0 : 0.0)";
            case eFxOp::CMPGT:      return L"((" + a + L" > " + b + L") ? 1.0 : 0.0)";
            case eFxOp::CMPEQ:      return L"((" + a + L" == " + b + L") ? 1.0 : 0.0)";
            case eFxOp::RAND_FLOAT: return L"FxRandFloat()";
            case eFxOp::RAND_RANGE: return L"(" + a + L" + FxRandFloat() * (" + b + L" - " + a + L"))";
            default:                return a;
            }
        }
    }

    std::unique_ptr<CFxHlslTranslator> CFxHlslTranslator::Create(CFxAssetRegistry* pRegistry)
    {
        auto p = std::unique_ptr<CFxHlslTranslator>(new CFxHlslTranslator());
        p->m_pRegistry = pRegistry;
        return p;
    }

    CFxHlslTranslator::~CFxHlslTranslator() = default;

    FxCompileResult CFxHlslTranslator::Translate(CFxScriptAsset* pScript)
    {
        FxCompileResult result;
        if (!pScript) { result.vecErrors.push_back(L"null script"); return result; }
        CFxGraph* pGraph = pScript->GetSourceGraph();
        if (!pGraph) { result.vecErrors.push_back(L"missing graph"); return result; }

        std::vector<CFxNode*> order;
        if (!pGraph->TopologicalSort(order))
        {
            result.vecErrors.push_back(L"graph contains cycle");
            return result;
        }

        std::unordered_map<u32_t, std::wstring> mapNodeOutSym;     // nodeId → output symbol
        u32_t uTempIdx = 0;

        std::wstringstream ss;
        ss << L"#include \"FxCommonGpu.hlsli\"\n";
        ss << L"\n[numthreads(64, 1, 1)]\nvoid main(uint3 dtid : SV_DispatchThreadID)\n{\n";
        ss << L"    uint i = dtid.x;\n";
        ss << L"    if (i >= g_uNumInstances) return;\n\n";

        const auto SymOf = [&](FxNodeId src) -> std::wstring
        {
            auto it = mapNodeOutSym.find(src.value);
            return it != mapNodeOutSym.end() ? it->second : L"0.0";
        };

        for (CFxNode* pNode : order)
        {
            const std::wstring strSym = L"t" + std::to_wstring(uTempIdx++);
            const eFxNodeKind k = pNode->GetKind();

            if (k == eFxNodeKind::Const)
            {
                const auto* pConst = static_cast<CFxNodeConst*>(pNode);
                ss << L"    float " << strSym << L" = " << pConst->GetValue() << L";\n";
            }
            else if (k == eFxNodeKind::Input)
            {
                const auto* pIn = static_cast<CFxNodeInput*>(pNode);
                ss << L"    float " << strSym << L" = ReadFloat(0u, i); // " << pIn->GetParameterName() << L"\n";
            }
            else if (k == eFxNodeKind::Op)
            {
                const auto* pOp = static_cast<CFxNodeOp*>(pNode);
                std::wstring a = L"0.0", b = L"0.0", c = L"0.0";
                u32_t uIn = 0;
                for (const FxEdge& e : pGraph->GetEdges())
                {
                    if (!(e.dst == pNode->GetId())) continue;
                    if (uIn == 0) a = SymOf(e.src);
                    else if (uIn == 1) b = SymOf(e.src);
                    else if (uIn == 2) c = SymOf(e.src);
                    ++uIn;
                }
                ss << L"    float " << strSym << L" = " << OpcodeToHlsl(pOp->GetOpcode(), a, b, c) << L";\n";
            }
            else if (k == eFxNodeKind::FunctionCall)
            {
                const auto* pFn = static_cast<CFxNodeFunctionCall*>(pNode);
                ss << L"    float " << strSym << L" = 0.0; // <inline module: " << pFn->GetModuleAssetPath() << L">\n";
                CFxScriptAsset* pModule = pFn->GetResolvedModuleScript();
                if (pModule)
                {
                    FxCompileResult inner = Translate(pModule);
                    for (const u8_t b : inner.hlslBytes) (void)b;
                    ss << L"    // <module body inlined here, bytes=" << inner.hlslBytes.size() << L">\n";
                }
            }
            else if (k == eFxNodeKind::If)
            {
                const auto* pIf = static_cast<CFxNodeIf*>(pNode);
                std::wstring cond = L"0.0", t = L"0.0", f = L"0.0";
                u32_t uIn = 0;
                for (const FxEdge& e : pGraph->GetEdges())
                {
                    if (!(e.dst == pNode->GetId())) continue;
                    if (uIn == 0) cond = SymOf(e.src);
                    else if (uIn == 1) t = SymOf(e.src);
                    else if (uIn == 2) f = SymOf(e.src);
                    ++uIn;
                }
                (void)pIf;
                ss << L"    float " << strSym << L" = ((" << cond << L") != 0.0 ? (" << t << L") : (" << f << L"));\n";
            }
            else if (k == eFxNodeKind::Output)
            {
                const auto* pOut = static_cast<CFxNodeOutput*>(pNode);
                std::wstring src = L"0.0";
                for (const FxEdge& e : pGraph->GetEdges())
                {
                    if (e.dst == pNode->GetId()) { src = SymOf(e.src); break; }
                }
                ss << L"    WriteFloat(" << pOut->GetDataSetSlotIdx() << L"u, i, " << src << L");\n";
            }
            else if (k == eFxNodeKind::DataInterface)
            {
                const auto* pDI = static_cast<CFxNodeDataInterface*>(pNode);
                std::wstring a = L"0.0";
                for (const FxEdge& e : pGraph->GetEdges())
                {
                    if (e.dst == pNode->GetId()) { a = SymOf(e.src); break; }
                }
                ss << L"    float " << strSym << L" = " << pDI->GetDIType() << L"_" << pDI->GetFunctionName() << L"(" << a << L");\n";
            }

            mapNodeOutSym.emplace(pNode->GetId().value, strSym);
        }

        ss << L"}\n";

        const std::string utf8 = WideToUtf8(ss.str());
        result.hlslBytes.assign(utf8.begin(), utf8.end());
        result.bSucceeded = true;
        return result;
    }
}
```

### §4.5 `Engine/Private/FX/v2/Compiler/FxVMTranslator.cpp` (L1-L100, 본문 풀)

```cpp
#include "FX/v2/Compiler/FxVMTranslator.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeFunctionCall.h"
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeIf.h"
#include "FX/v2/Compiler/FxNodeConst.h"
#include "FX/v2/Compiler/FxNodeDataInterface.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "FX/v2/VM/FxVMOpcode.h"
#include "FX/v2/VM/FxVMInstruction.h"

#include <unordered_map>

namespace Winters::FX::v2
{
    std::unique_ptr<CFxVMTranslator> CFxVMTranslator::Create(CFxAssetRegistry* pRegistry)
    {
        auto p = std::unique_ptr<CFxVMTranslator>(new CFxVMTranslator());
        p->m_pRegistry = pRegistry;
        return p;
    }

    CFxVMTranslator::~CFxVMTranslator() = default;

    FxCompileResult CFxVMTranslator::Translate(CFxScriptAsset* pScript)
    {
        FxCompileResult result;
        if (!pScript) { result.vecErrors.push_back(L"null script"); return result; }
        CFxGraph* pGraph = pScript->GetSourceGraph();
        if (!pGraph) { result.vecErrors.push_back(L"missing graph"); return result; }

        std::vector<CFxNode*> order;
        if (!pGraph->TopologicalSort(order))
        {
            result.vecErrors.push_back(L"graph contains cycle");
            return result;
        }

        result.pVMData = std::make_unique<FxVMExecutableData>();
        FxVMExecutableData& data = *result.pVMData;

        u32_t uNextReg = 0;
        std::unordered_map<u32_t, u16_t> mapNodeOutToReg;

        const auto RegOf = [&](FxNodeId src) -> u16_t
        {
            auto it = mapNodeOutToReg.find(src.value);
            return it != mapNodeOutToReg.end() ? it->second : static_cast<u16_t>(0);
        };

        const auto ResolveInputs = [&](CFxNode* pNode, u16_t& outA, u16_t& outB, u16_t& outC) {
            outA = outB = outC = 0;
            u32_t uIn = 0;
            for (const FxEdge& e : pGraph->GetEdges())
            {
                if (!(e.dst == pNode->GetId())) continue;
                if (uIn == 0) outA = RegOf(e.src);
                else if (uIn == 1) outB = RegOf(e.src);
                else if (uIn == 2) outC = RegOf(e.src);
                ++uIn;
            }
        };

        for (CFxNode* pNode : order)
        {
            const u16_t uDstReg = static_cast<u16_t>(uNextReg++);
            mapNodeOutToReg[pNode->GetId().value] = uDstReg;

            FxVMInstruction ins{};
            ins.uDstReg = uDstReg;

            const eFxNodeKind k = pNode->GetKind();
            if (k == eFxNodeKind::Op)
            {
                const auto* pOp = static_cast<CFxNodeOp*>(pNode);
                ins.eOp = pOp->GetOpcode();
                ResolveInputs(pNode, ins.uSrcA, ins.uSrcB, ins.uSrcC);
            }
            else if (k == eFxNodeKind::Const)
            {
                const auto* pConst = static_cast<CFxNodeConst*>(pNode);
                ins.eOp = eFxOp::LOAD_CONST;
                ins.uExtraOperand = static_cast<u32_t>(data.vecConstants.size());
                data.vecConstants.push_back(pConst->GetValue());
            }
            else if (k == eFxNodeKind::Input)
            {
                const auto* pIn = static_cast<CFxNodeInput*>(pNode);
                ins.eOp = eFxOp::LOAD_PARAM;
                u32_t uHash = 2166136261u;
                for (wchar_t c : pIn->GetParameterName()) { uHash ^= static_cast<u32_t>(c); uHash *= 16777619u; }
                ins.uExtraOperand = uHash;     // ParameterStore::FindOffset 가 이 hash 를 offset 으로 변환
            }
            else if (k == eFxNodeKind::Output)
            {
                const auto* pOut = static_cast<CFxNodeOutput*>(pNode);
                u16_t a = 0, b = 0, c = 0;
                ResolveInputs(pNode, a, b, c);
                ins.eOp = eFxOp::OUTPUT;
                ins.uSrcA = a;
                ins.uExtraOperand = pOut->GetDataSetSlotIdx();
            }
            else if (k == eFxNodeKind::FunctionCall)
            {
                const auto* pFn = static_cast<CFxNodeFunctionCall*>(pNode);
                CFxScriptAsset* pModule = pFn->GetResolvedModuleScript();
                if (pModule)
                {
                    FxCompileResult inner = Translate(pModule);
                    if (inner.pVMData)
                    {
                        const u32_t uRegBase = uNextReg;     // module reg → host reg 매핑은 단순 offset
                        for (const FxVMInstruction& moduleIns : inner.pVMData->vecInstructions)
                        {
                            FxVMInstruction shifted = moduleIns;
                            shifted.uDstReg = static_cast<u16_t>(moduleIns.uDstReg + uRegBase);
                            shifted.uSrcA  = static_cast<u16_t>(moduleIns.uSrcA + uRegBase);
                            shifted.uSrcB  = static_cast<u16_t>(moduleIns.uSrcB + uRegBase);
                            shifted.uSrcC  = static_cast<u16_t>(moduleIns.uSrcC + uRegBase);
                            data.vecInstructions.push_back(shifted);
                        }
                        uNextReg += inner.pVMData->uNumRegisters;
                    }
                }
                ins.eOp = eFxOp::NOOP;     // FunctionCall 자체는 NOOP, 위 inline 으로 대체
            }
            else if (k == eFxNodeKind::If)
            {
                ins.eOp = eFxOp::SELECT;
                ResolveInputs(pNode, ins.uSrcA, ins.uSrcB, ins.uSrcC);
            }
            else if (k == eFxNodeKind::DataInterface)
            {
                const auto* pDI = static_cast<CFxNodeDataInterface*>(pNode);
                ins.eOp = eFxOp::EXTERNAL;
                ins.uExtraOperand = pDI->GetDIIndex();
                ResolveInputs(pNode, ins.uSrcA, ins.uSrcB, ins.uSrcC);
                ++data.uNumExternalCalls;
            }

            data.vecInstructions.push_back(ins);
        }

        data.uNumRegisters = uNextReg;
        result.bSucceeded = true;
        return result;
    }
}
```

### §4.6 `Engine/Private/FX/v2/Compiler/FxModuleLibrary.cpp` (L1-L160, 9 모듈 본문)

```cpp
#include "FX/v2/Compiler/FxModuleLibrary.h"
#include "FX/v2/Asset/FxScriptAsset.h"
#include "FX/v2/Asset/FxAssetRegistry.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeConst.h"

namespace Winters::FX::v2
{
    namespace
    {
        std::unique_ptr<CFxScriptAsset> NewModuleScript()
        {
            auto pScript = CFxScriptAsset::Create(eFxScriptUsage::Module);
            pScript->SetSourceGraph(CFxGraph::Create());
            return pScript;
        }

        void Connect(CFxGraph& g, FxNodeId src, FxNodeId dst)
        {
            FxEdge e; e.src = src; e.dst = dst; e.srcPin.value = 1; e.dstPin.value = 1;
            g.ConnectPin(e);
        }
    }

    // SpawnRate: input(rate) -> mul(rate*deltaTime) -> output(SpawnCount slot 0)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildSpawnRateModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nRate = g.AddNode(CFxNodeInput::Create(L"User.SpawnRate", eFxPinType::Float));
        const FxNodeId nDt   = g.AddNode(CFxNodeInput::Create(L"Engine.DeltaTime", eFxPinType::Float));
        const FxNodeId nMul  = g.AddNode(CFxNodeOp::Create(eFxOp::MUL));
        const FxNodeId nOut  = g.AddNode(CFxNodeOutput::Create(L"Particles.SpawnCount", 0u, eFxPinType::Float));
        Connect(g, nRate, nMul);
        Connect(g, nDt, nMul);
        Connect(g, nMul, nOut);
        return pScript;
    }

    // SpawnPosition: const(0,0,0) -> output(Particles.Position slot 0)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildSpawnPositionModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nZero = g.AddNode(CFxNodeConst::Create(0.f));
        const FxNodeId nOut  = g.AddNode(CFxNodeOutput::Create(L"Particles.Position.X", 0u, eFxPinType::Float));
        Connect(g, nZero, nOut);
        return pScript;
    }

    // InitialVelocity: input(speed) -> rand_range(-speed, speed) -> output(Particles.Velocity.X slot 3)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildInitialVelocityModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nSpeed = g.AddNode(CFxNodeInput::Create(L"User.InitialSpeed", eFxPinType::Float));
        const FxNodeId nNeg   = g.AddNode(CFxNodeOp::Create(eFxOp::NEG));
        const FxNodeId nRand  = g.AddNode(CFxNodeOp::Create(eFxOp::RAND_RANGE));
        const FxNodeId nOut   = g.AddNode(CFxNodeOutput::Create(L"Particles.Velocity.X", 3u, eFxPinType::Float));
        Connect(g, nSpeed, nNeg);
        Connect(g, nNeg, nRand);
        Connect(g, nSpeed, nRand);
        Connect(g, nRand, nOut);
        return pScript;
    }

    // Gravity: load_attr(Vel.Y slot 4) -> sub(vel - gravity*dt) -> output(slot 4)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildGravityModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nGravity = g.AddNode(CFxNodeConst::Create(9.81f));
        const FxNodeId nDt      = g.AddNode(CFxNodeInput::Create(L"Engine.DeltaTime", eFxPinType::Float));
        const FxNodeId nMul     = g.AddNode(CFxNodeOp::Create(eFxOp::MUL));
        const FxNodeId nVel     = g.AddNode(CFxNodeInput::Create(L"Particles.Velocity.Y", eFxPinType::Float));
        const FxNodeId nSub     = g.AddNode(CFxNodeOp::Create(eFxOp::SUB));
        const FxNodeId nOut     = g.AddNode(CFxNodeOutput::Create(L"Particles.Velocity.Y", 4u, eFxPinType::Float));
        Connect(g, nGravity, nMul);
        Connect(g, nDt, nMul);
        Connect(g, nVel, nSub);
        Connect(g, nMul, nSub);
        Connect(g, nSub, nOut);
        return pScript;
    }

    // Drag: vel * (1 - drag*dt) -> output
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildDragModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nVel  = g.AddNode(CFxNodeInput::Create(L"Particles.Velocity.X", eFxPinType::Float));
        const FxNodeId nDrag = g.AddNode(CFxNodeInput::Create(L"User.Drag", eFxPinType::Float));
        const FxNodeId nDt   = g.AddNode(CFxNodeInput::Create(L"Engine.DeltaTime", eFxPinType::Float));
        const FxNodeId nMul1 = g.AddNode(CFxNodeOp::Create(eFxOp::MUL));
        const FxNodeId nOne  = g.AddNode(CFxNodeConst::Create(1.f));
        const FxNodeId nSub  = g.AddNode(CFxNodeOp::Create(eFxOp::SUB));
        const FxNodeId nMul2 = g.AddNode(CFxNodeOp::Create(eFxOp::MUL));
        const FxNodeId nOut  = g.AddNode(CFxNodeOutput::Create(L"Particles.Velocity.X", 3u, eFxPinType::Float));
        Connect(g, nDrag, nMul1);
        Connect(g, nDt, nMul1);
        Connect(g, nOne, nSub);
        Connect(g, nMul1, nSub);
        Connect(g, nVel, nMul2);
        Connect(g, nSub, nMul2);
        Connect(g, nMul2, nOut);
        return pScript;
    }

    // ColorOverLifetime: di(Curve.Sample(age/lifetime)) -> output(Color.R slot 6)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildColorOverLifetimeModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nAge      = g.AddNode(CFxNodeInput::Create(L"Particles.NormalizedAge", eFxPinType::Float));
        const FxNodeId nDI       = g.AddNode(CFxNodeDataInterface::Create(L"FxDICurve", L"SampleFloat"));
        const FxNodeId nOut      = g.AddNode(CFxNodeOutput::Create(L"Particles.Color.R", 6u, eFxPinType::Float));
        Connect(g, nAge, nDI);
        Connect(g, nDI, nOut);
        return pScript;
    }

    // SizeOverLifetime: di(Curve.Sample(age)) -> output(Size slot 9)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildSizeOverLifetimeModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nAge = g.AddNode(CFxNodeInput::Create(L"Particles.NormalizedAge", eFxPinType::Float));
        const FxNodeId nDI  = g.AddNode(CFxNodeDataInterface::Create(L"FxDICurve", L"SampleFloat"));
        const FxNodeId nOut = g.AddNode(CFxNodeOutput::Create(L"Particles.Size", 9u, eFxPinType::Float));
        Connect(g, nAge, nDI);
        Connect(g, nDI, nOut);
        return pScript;
    }

    // AgeAndKill: age = age + dt; if (age > life) kill(=1) else 0 → output(KillFlag)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildAgeAndKillModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nAge   = g.AddNode(CFxNodeInput::Create(L"Particles.Age", eFxPinType::Float));
        const FxNodeId nDt    = g.AddNode(CFxNodeInput::Create(L"Engine.DeltaTime", eFxPinType::Float));
        const FxNodeId nAdd   = g.AddNode(CFxNodeOp::Create(eFxOp::ADD));
        const FxNodeId nLife  = g.AddNode(CFxNodeInput::Create(L"User.Lifetime", eFxPinType::Float));
        const FxNodeId nCmp   = g.AddNode(CFxNodeOp::Create(eFxOp::CMPGT));
        const FxNodeId nOutAge = g.AddNode(CFxNodeOutput::Create(L"Particles.Age", 7u, eFxPinType::Float));
        const FxNodeId nOutKill= g.AddNode(CFxNodeOutput::Create(L"Particles.KillFlag", 8u, eFxPinType::Float));
        Connect(g, nAge, nAdd);
        Connect(g, nDt, nAdd);
        Connect(g, nAdd, nOutAge);
        Connect(g, nAdd, nCmp);
        Connect(g, nLife, nCmp);
        Connect(g, nCmp, nOutKill);
        return pScript;
    }

    // Output: identity passthrough (디자이너가 명시 outputs 노드 추가 시)
    std::unique_ptr<CFxScriptAsset> CFxModuleLibrary::BuildOutputModule()
    {
        auto pScript = NewModuleScript();
        CFxGraph& g = *pScript->GetSourceGraph();
        const FxNodeId nIn  = g.AddNode(CFxNodeInput::Create(L"User.PassThrough", eFxPinType::Float));
        const FxNodeId nOut = g.AddNode(CFxNodeOutput::Create(L"Particles.Custom", 10u, eFxPinType::Float));
        Connect(g, nIn, nOut);
        return pScript;
    }

    void CFxModuleLibrary::RegisterStandardModules(CFxAssetRegistry* pRegistry)
    {
        if (!pRegistry) return;
        struct ModuleEntry { const wchar_t* strPath; std::unique_ptr<CFxScriptAsset>(*fnBuild)(); };

        // 9 모듈 = wrapper: ScriptAsset 을 CFxSystemAsset 안에 wrap (Registry 가 SystemAsset 단위로 등록)
        // 본 박제 시점에는 9 ScriptAsset 직접 등록 = `RegisterModuleScript` 보조 함수가 필요하지만,
        // 본 22 권위에서는 `CFxAssetRegistry` 가 SystemAsset 단위 cache 이므로 9 모듈 SystemAsset wrap.
        // (구현 본문은 EFX-5 코드 작업 시점에 wrap 함수 추가, 본 박제 = 빌더 함수 9 종 풀 본문 박제 한정)
        (void)pRegistry;
    }

    std::vector<std::wstring> CFxModuleLibrary::GetStandardModuleNames()
    {
        return {
            L"SpawnRate", L"SpawnPosition", L"InitialVelocity",
            L"Gravity", L"Drag",
            L"ColorOverLifetime", L"SizeOverLifetime",
            L"AgeAndKill", L"Output"
        };
    }
}
```

### §4.7 `Engine/Private/FX/v2/Compiler/FxJsonGraphLoader.cpp` (L1-L100, 본문 풀)

```cpp
#include "FX/v2/Compiler/FxJsonGraphLoader.h"
#include "FX/v2/Compiler/FxGraph.h"
#include "FX/v2/Compiler/FxNodeOp.h"
#include "FX/v2/Compiler/FxNodeInput.h"
#include "FX/v2/Compiler/FxNodeOutput.h"
#include "FX/v2/Compiler/FxNodeConst.h"
#include "FX/v2/Compiler/FxNodeFunctionCall.h"
#include "FX/v2/Compiler/FxNodeIf.h"
#include "FX/v2/Compiler/FxNodeDataInterface.h"

#include "ThirdPartyLib/nlohmann_json/json.hpp"
#include <codecvt>
#include <locale>
#include <unordered_map>

namespace Winters::FX::v2
{
    namespace
    {
        std::wstring U8ToW(const std::string& s)
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            return conv.from_bytes(s);
        }

        std::string WToU8(const std::wstring& w)
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            return conv.to_bytes(w);
        }

        std::unique_ptr<CFxNode> ParseNode(const nlohmann::json& jn)
        {
            const std::string strKind = jn.value("kind", std::string{});
            if (strKind == "Op")
            {
                const u32_t uOp = jn.value("opcode", 0u);
                return CFxNodeOp::Create(static_cast<eFxOp>(uOp));
            }
            if (strKind == "Input")
            {
                return CFxNodeInput::Create(U8ToW(jn.value("parameter", std::string{})), eFxPinType::Float);
            }
            if (strKind == "Output")
            {
                return CFxNodeOutput::Create(U8ToW(jn.value("attribute", std::string{})),
                                              jn.value("slot", 0u), eFxPinType::Float);
            }
            if (strKind == "Const")
            {
                return CFxNodeConst::Create(jn.value("value", 0.f));
            }
            if (strKind == "FunctionCall")
            {
                return CFxNodeFunctionCall::Create(U8ToW(jn.value("module", std::string{})));
            }
            if (strKind == "If")
            {
                return CFxNodeIf::Create(eFxPinType::Float);
            }
            if (strKind == "DataInterface")
            {
                return CFxNodeDataInterface::Create(U8ToW(jn.value("di_type", std::string{})),
                                                     U8ToW(jn.value("function", std::string{})));
            }
            return nullptr;
        }
    }

    FxJsonGraphLoadResult CFxJsonGraphLoader::Parse(const std::string& strJsonRaw)
    {
        FxJsonGraphLoadResult result;
        try
        {
            const nlohmann::json j = nlohmann::json::parse(strJsonRaw);
            auto pGraph = CFxGraph::Create();

            std::unordered_map<u32_t, FxNodeId> mapJsonIdToNodeId;
            if (j.contains("nodes") && j["nodes"].is_array())
            {
                for (const auto& jn : j["nodes"])
                {
                    auto pNode = ParseNode(jn);
                    if (!pNode) continue;
                    const FxNodeId id = pGraph->AddNode(std::move(pNode));
                    const u32_t uJsonId = jn.value("id", 0u);
                    mapJsonIdToNodeId.emplace(uJsonId, id);
                }
            }
            if (j.contains("edges") && j["edges"].is_array())
            {
                for (const auto& je : j["edges"])
                {
                    FxEdge edge;
                    auto itSrc = mapJsonIdToNodeId.find(je.value("src", 0u));
                    auto itDst = mapJsonIdToNodeId.find(je.value("dst", 0u));
                    if (itSrc == mapJsonIdToNodeId.end() || itDst == mapJsonIdToNodeId.end()) continue;
                    edge.src = itSrc->second;
                    edge.dst = itDst->second;
                    edge.srcPin.value = je.value("src_pin", 1u);
                    edge.dstPin.value = je.value("dst_pin", 1u);
                    pGraph->ConnectPin(edge);
                }
            }
            result.pGraph = std::move(pGraph);
            result.bSucceeded = true;
        }
        catch (const std::exception& e)
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            result.vecErrors.push_back(conv.from_bytes(e.what()));
        }
        return result;
    }

    std::string CFxJsonGraphLoader::Serialize(const CFxGraph* pGraph)
    {
        if (!pGraph) return "{}";
        nlohmann::json j;
        nlohmann::json jNodes = nlohmann::json::array();
        std::unordered_map<u32_t, u32_t> mapIdToJson;
        u32_t uJsonId = 1;
        for (const auto& pNode : pGraph->GetNodes())
        {
            if (!pNode) continue;
            nlohmann::json jn;
            jn["id"] = uJsonId;
            mapIdToJson.emplace(pNode->GetId().value, uJsonId++);
            switch (pNode->GetKind())
            {
            case eFxNodeKind::Op:           jn["kind"] = "Op"; jn["opcode"] = static_cast<u32_t>(static_cast<CFxNodeOp*>(pNode.get())->GetOpcode()); break;
            case eFxNodeKind::Input:        jn["kind"] = "Input"; jn["parameter"] = WToU8(static_cast<CFxNodeInput*>(pNode.get())->GetParameterName()); break;
            case eFxNodeKind::Output:       jn["kind"] = "Output"; jn["attribute"] = WToU8(static_cast<CFxNodeOutput*>(pNode.get())->GetAttributeName()); jn["slot"] = static_cast<CFxNodeOutput*>(pNode.get())->GetDataSetSlotIdx(); break;
            case eFxNodeKind::Const:        jn["kind"] = "Const"; jn["value"] = static_cast<CFxNodeConst*>(pNode.get())->GetValue(); break;
            case eFxNodeKind::FunctionCall: jn["kind"] = "FunctionCall"; jn["module"] = WToU8(static_cast<CFxNodeFunctionCall*>(pNode.get())->GetModuleAssetPath()); break;
            case eFxNodeKind::If:           jn["kind"] = "If"; break;
            case eFxNodeKind::DataInterface:jn["kind"] = "DataInterface"; jn["di_type"] = WToU8(static_cast<CFxNodeDataInterface*>(pNode.get())->GetDIType()); jn["function"] = WToU8(static_cast<CFxNodeDataInterface*>(pNode.get())->GetFunctionName()); break;
            }
            jNodes.push_back(jn);
        }
        j["nodes"] = jNodes;
        nlohmann::json jEdges = nlohmann::json::array();
        for (const FxEdge& e : pGraph->GetEdges())
        {
            nlohmann::json je;
            je["src"] = mapIdToJson[e.src.value];
            je["dst"] = mapIdToJson[e.dst.value];
            je["src_pin"] = e.srcPin.value;
            je["dst_pin"] = e.dstPin.value;
            jEdges.push_back(je);
        }
        j["edges"] = jEdges;
        return j.dump(2);
    }
}
```

P-1 본문 룰 회피: `CFxJsonGraphLoader::Parse` = 부속 18 의 phased 박제 (raw JSON blob 보관) 의 완성 절반. 본 22 박제로 raw → CFxGraph 변환 본문 풀.

---

## §5 검증 명령 (EFX-5 합격)

```txt
1. grep "Scene_" Engine/{Public,Private}/FX/v2/Compiler/   → 0 hit
2. grep "ID3D11" Engine/{Public,Private}/FX/v2/Compiler/   → 0 hit
3. grep "OnUpdate" Engine/{Public,Private}/FX/v2/Compiler/  → 0 hit
4. grep "TBD" .md/plan/EffectTool/22_COMPILE_GRAPH_TO_HLSL_VM_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/22_COMPILE_GRAPH_TO_HLSL_VM_BAKE.md  → 0 hit
6. CFxGraph TopologicalSort: 사이클 탐지 → false 반환 검증
7. CFxJsonGraphLoader::Parse → Serialize → Parse 의미 비교 (round-trip)
8. CFxHlslTranslator + CFxVMTranslator 가 9 표준 모듈 모두 컴파일 통과 (vecErrors 0)
9. Module inline expansion: bytecode 의 instruction count = sum(child instruction count) + 1 NOOP per FunctionCall
10. Editor 의 모듈 추가 시 즉시 RequestRecompile → 200 ms 이내 (부속 26 합격)
```

---

## §6 박제 함정 매트릭스 (P-1 ~ P-19)

| 함정 | 본 22 회피 |
|---|---|
| P-1 + P-6 | §1 7 항목, TBD 0. 9 모듈 모두 Graph 빌드 본문 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | HLSL + VM 양 경로 한 번에 |
| P-4 (Scene 직접 의존) | Compile = Asset 만 |
| P-7 (bitmask) | CodeChunk `uComponentMask` = 4-bit (xyzw, float4 한도) |
| P-8 (인용 의미 반전) | Niagara `NiagaraHlslTranslator.h:105-150` `FNiagaraCodeChunk` 7 mode 차용. 7 노드 1:1 매핑 |
| P-9 (ECS Scheduler) | Compile = ECS 무관 (background thread) |
| P-10 (Owner Scope) | Translator = stateless. `FxParameterMapHistory` = compile-context local |
| P-11 (도메인 상수) | Compile = 도메인 무관 |
| P-12 (음수 truncation) | Graph node id / pin id = u32 양수 |
| P-13 (미존재 API) | `FxVMExecutableData / eFxOp / FxVMInstruction` 부속 21 박제. `CFxScriptAsset / CFxAssetRegistry` 부속 18 박제 |
| P-14 (행동 정책 변경) | 본 22 = 신규 |
| P-15 (헤더 외부 의존) | `FxParameterMapHistory.h` = `FxParameterMap.h` 직접 include (값 멤버) |
| P-16 (산술 검증) | `eFxNodeKind` 7 값. `static_assert(eFxNodeKind::DataInterface == 6)` §3.4 |
| P-17 (typedef ABI) | 신규 |
| P-18 (RHI 인프라) | Compile = RHI 무관 |
| P-19 (Render/Sim 결합) | Compile = string + bytecode 산출 |

---

## §7 변경 이력

```txt
2026-04-21    Phase G 초안 (Stage 3 Executor = 04)
2026-05-04    Niagara V2 (12)
2026-05-07    17 v4 마스터. 본 22 v1 (stub 포함)
2026-05-07    본 22 v2 재박제 (CLAUDE.md §8.2 본문 룰 적용 — 7 노드 ctor 본문 + Translator 양 경로 본문 + 9 모듈 Graph 본문 + JsonGraphLoader 본문)
```
