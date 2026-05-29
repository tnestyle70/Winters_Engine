# Stage 1 — 그래프 데이터 모델 (Graph / Node / Pin / Edge + JSON + 위상 정렬)

## 목표

**모든 이펙트는 노드 그래프로 정의** 되며 그래프는 JSON 으로 디스크에 영속된다.
런타임은 JSON → `FxGraph` → 위상 정렬 → 실행 스텝 리스트 순으로 컴파일한다.

## 왜 먼저인가

- 이후 Stage 전부 (2~7) 가 이 데이터 모델 위에 올라감
- 에셋 파이프라인 (디스크 포맷) 초기 확정해야 마이그레이션 비용 없음
- 에디터 (Stage 6) 와 런타임 양쪽이 같은 `FxGraph` 공유

## 핵심 개념

```
┌──── FxGraph ─────────────────────────────────────┐
│                                                  │
│   ┌──Node #1──┐    ┌──Node #2──┐                 │
│   │SpawnBurst │───▶│InitPos    │───▶ Init stage  │
│   │count=20   │    │radius=2.0 │                 │
│   └───────────┘    └─────┬─────┘                 │
│                          ▼                       │
│                    ┌─Node #3──┐                  │
│                    │InitVel    │                 │
│                    │cone=30°   │                 │
│                    └───────────┘                 │
│                                                  │
│   ┌──Node #4──┐    ┌──Node #5──┐                 │
│   │Gravity    │───▶│AgeAndKill │───▶ Update      │
│   └───────────┘    └───────────┘                 │
└──────────────────────────────────────────────────┘
```

노드는 **Stage** (System / Emitter / Spawn / Update) 4 종 중 하나에 소속.
엣지는 같은 Stage 내에서만 유효 (cross-stage 데이터는 attribute 를 경유).

## 타입 체계

```cpp
// Engine/Public/FX/Core/FxTypes.h
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "WintersMath.h"

namespace Engine::FX {

// ── Attribute Type ──────────────────────────────────
enum class AttrType : std::uint8_t
{
    Float, Float2, Float3, Float4, Int, Bool
};

inline std::uint32_t SizeOfAttr(AttrType t)
{
    switch (t) {
        case AttrType::Float:  return 4;
        case AttrType::Float2: return 8;
        case AttrType::Float3: return 12;
        case AttrType::Float4: return 16;
        case AttrType::Int:    return 4;
        case AttrType::Bool:   return 1;
    }
    return 0;
}

// ── Pin Value (런타임 / 에디터 공용) ────────────────
using FxValue = std::variant<
    float,
    Vec2,
    Vec3,
    Vec4,
    std::int32_t,
    bool,
    std::string    // 텍스처 경로 등
>;

// ── Stage ──────────────────────────────────────────
enum class eStage : std::uint8_t
{
    System,      // 전역, 1 회 / 프레임
    Emitter,     // 이미터당 1 회 / 프레임
    Spawn,       // 신규 파티클 범위에만 적용
    Update       // 전체 alive 파티클
};

// ── NodeKind (최소 셋 — Phase 1 MVP) ────────────────
enum class eNodeKind : std::uint16_t
{
    // Spawn
    SpawnBurst       = 0x0001,   // N 개 일시 생성
    SpawnRate        = 0x0002,   // 초당 N 개

    // Initialize (신규 파티클)
    InitPositionPoint    = 0x0101,
    InitPositionSphere   = 0x0102,
    InitPositionBox      = 0x0103,
    InitVelocityCone     = 0x0111,
    InitVelocityInDir    = 0x0112,
    InitLifetime         = 0x0121,
    InitColor            = 0x0122,
    InitSize             = 0x0123,

    // Update
    UpdateGravity            = 0x0201,
    UpdateDrag               = 0x0202,
    UpdateCurlNoise          = 0x0203,
    UpdateColorOverLife      = 0x0204,
    UpdateSizeOverLife       = 0x0205,
    UpdateAgeAndKill         = 0x0206,
    UpdateIntegratePosition  = 0x0207,

    // Expression 서브그래프 ★ Stage 4
    ExprBinOp           = 0x0301,
    ExprConst           = 0x0302,
    ExprAttrRead        = 0x0303,
    ExprAttrWrite       = 0x0304,
    ExprUnaryOp         = 0x0305,
    ExprLerp            = 0x0306
};

// ── AttributeDesc (Pool 에서 사용) ──────────────────
struct AttributeDesc
{
    std::string name;          // "Position", "Velocity", ...
    AttrType    type;
    std::uint32_t stride;      // = SizeOfAttr(type)
};

} // namespace Engine::FX
```

## 표준 Attribute 레지스트리

파티클이 기본 지원하는 attribute 는 고정:

```cpp
// Engine/Public/FX/Core/FxAttributeRegistry.h
#pragma once
#include "FxTypes.h"

namespace Engine::FX {

// 표준 attribute 이름 상수 (문자열 오타 방지)
namespace Attr {
    inline constexpr const char* Position   = "Position";    // Float3
    inline constexpr const char* Velocity   = "Velocity";    // Float3
    inline constexpr const char* Color      = "Color";       // Float4 (RGBA)
    inline constexpr const char* Size       = "Size";        // Float
    inline constexpr const char* Age        = "Age";         // Float
    inline constexpr const char* Lifetime   = "Lifetime";    // Float
    inline constexpr const char* Rotation   = "Rotation";    // Float (2D) or Float3 (3D)
    inline constexpr const char* Seed       = "Seed";        // Int (per-particle 난수 시드)
}

// 표준 attribute 레이아웃
inline std::vector<AttributeDesc> DefaultAttributeSchema()
{
    return {
        { Attr::Position, AttrType::Float3, SizeOfAttr(AttrType::Float3) },
        { Attr::Velocity, AttrType::Float3, SizeOfAttr(AttrType::Float3) },
        { Attr::Color,    AttrType::Float4, SizeOfAttr(AttrType::Float4) },
        { Attr::Size,     AttrType::Float,  SizeOfAttr(AttrType::Float)  },
        { Attr::Age,      AttrType::Float,  SizeOfAttr(AttrType::Float)  },
        { Attr::Lifetime, AttrType::Float,  SizeOfAttr(AttrType::Float)  },
    };
}

} // namespace Engine::FX
```

## FxGraph

```cpp
// Engine/Public/FX/Graph/FxGraph.h
#pragma once
#include "FxTypes.h"
#include <unordered_map>

namespace Engine::FX {

using NodeId = std::uint32_t;
using PinId  = std::uint32_t;
constexpr NodeId NULL_NODE = 0;
constexpr PinId  NULL_PIN  = 0;

// ── Pin ────────────────────────────────────────────
struct Pin
{
    PinId        id;
    std::string  name;
    AttrType     type;
    FxValue      defaultValue;   // 연결 없을 때 쓰는 기본값
};

// ── Node ───────────────────────────────────────────
struct Node
{
    NodeId                id;
    eNodeKind             kind;
    eStage                stage;
    std::vector<Pin>      inputs;
    std::vector<Pin>      outputs;
    // 정적 파라미터 (핀으로 안 쓰는 설정): "shape" = "Cone", "angle" = 30.0
    std::unordered_map<std::string, FxValue> params;

    // 에디터용 위치 (런타임은 안 씀)
    Vec2 editorPos { 0.f, 0.f };
};

// ── Edge ───────────────────────────────────────────
struct Edge
{
    NodeId fromNode;
    PinId  fromPin;
    NodeId toNode;
    PinId  toPin;
};

// ── FxGraph ────────────────────────────────────────
class CFxGraph
{
public:
    static std::unique_ptr<CFxGraph> Create() { return std::unique_ptr<CFxGraph>(new CFxGraph()); }
    ~CFxGraph() = default;

    NodeId AddNode(eNodeKind kind, eStage stage);
    void   RemoveNode(NodeId id);
    bool   Connect(NodeId from, PinId fromPin, NodeId to, PinId toPin);
    void   Disconnect(NodeId to, PinId toPin);

    // 노드 접근
    const Node& Get(NodeId id) const { return m_nodes.at(id); }
    Node&       GetMutable(NodeId id) { return m_nodes.at(id); }
    bool        Exists(NodeId id) const { return m_nodes.count(id) > 0; }

    const std::unordered_map<NodeId, Node>& AllNodes() const { return m_nodes; }
    const std::vector<Edge>&                AllEdges() const { return m_edges; }

private:
    CFxGraph() = default;
    NodeId AllocId() { return m_nextId++; }

    std::unordered_map<NodeId, Node> m_nodes;
    std::vector<Edge>                m_edges;
    NodeId                           m_nextId = 1;
};

} // namespace Engine::FX
```

### 구현 핵심

```cpp
// Engine/Private/FX/Graph/FxGraph.cpp
#include "FxGraph.h"

namespace Engine::FX {

NodeId CFxGraph::AddNode(eNodeKind kind, eStage stage)
{
    Node n;
    n.id    = AllocId();
    n.kind  = kind;
    n.stage = stage;
    // 핀 기본 레이아웃은 NodeRegistry 쪽에서 NodeKind 별로 주입
    m_nodes.emplace(n.id, std::move(n));
    return n.id;
}

void CFxGraph::RemoveNode(NodeId id)
{
    // 관련 엣지 제거
    m_edges.erase(
        std::remove_if(m_edges.begin(), m_edges.end(),
            [id](const Edge& e){ return e.fromNode == id || e.toNode == id; }),
        m_edges.end()
    );
    m_nodes.erase(id);
}

bool CFxGraph::Connect(NodeId from, PinId fromPin, NodeId to, PinId toPin)
{
    if (!Exists(from) || !Exists(to)) return false;
    if (from == to) return false;   // self-loop 금지

    // 같은 입력 핀에 이미 엣지 있으면 기존 것 제거 (대체)
    Disconnect(to, toPin);

    // 타입 체크
    const auto& src = m_nodes[from];
    const auto& dst = m_nodes[to];
    AttrType outT = {}; AttrType inT = {};
    bool okOut = false, okIn = false;
    for (const auto& p : src.outputs) if (p.id == fromPin) { outT = p.type; okOut = true; break; }
    for (const auto& p : dst.inputs)  if (p.id == toPin)   { inT  = p.type; okIn  = true; break; }
    if (!okOut || !okIn || outT != inT) return false;

    m_edges.push_back({ from, fromPin, to, toPin });
    return true;
}

void CFxGraph::Disconnect(NodeId to, PinId toPin)
{
    m_edges.erase(
        std::remove_if(m_edges.begin(), m_edges.end(),
            [&](const Edge& e){ return e.toNode == to && e.toPin == toPin; }),
        m_edges.end()
    );
}

} // namespace Engine::FX
```

## 위상 정렬 (Kahn's Algorithm)

```cpp
// Engine/Public/FX/Graph/FxTopoSort.h
#pragma once
#include "FxGraph.h"
#include <queue>

namespace Engine::FX {

// 같은 stage 에 속한 노드만 정렬한다. 사이클이면 false.
inline bool TopologicalSort(const CFxGraph& g,
                            eStage stage,
                            std::vector<NodeId>& out)
{
    out.clear();

    std::unordered_map<NodeId, int> inDegree;
    std::unordered_map<NodeId, std::vector<NodeId>> adj;

    for (const auto& [id, n] : g.AllNodes())
        if (n.stage == stage) inDegree[id] = 0;

    for (const auto& e : g.AllEdges()) {
        auto itF = g.AllNodes().find(e.fromNode);
        auto itT = g.AllNodes().find(e.toNode);
        if (itF == g.AllNodes().end() || itT == g.AllNodes().end()) continue;
        if (itF->second.stage != stage || itT->second.stage != stage) continue;
        adj[e.fromNode].push_back(e.toNode);
        inDegree[e.toNode]++;
    }

    std::queue<NodeId> q;
    for (auto& [id, d] : inDegree)
        if (d == 0) q.push(id);

    while (!q.empty()) {
        NodeId n = q.front(); q.pop();
        out.push_back(n);
        for (NodeId m : adj[n])
            if (--inDegree[m] == 0) q.push(m);
    }

    return out.size() == inDegree.size();   // 사이클이면 false
}

} // namespace Engine::FX
```

## JSON 직렬화

### 외부 포맷 예시 (불꽃 이펙트)

```json
{
  "version": 1,
  "displayName": "Fire_Burst_Small",
  "deterministic": false,
  "attributes": [
    {"name": "Position", "type": "Float3"},
    {"name": "Velocity", "type": "Float3"},
    {"name": "Color",    "type": "Float4"},
    {"name": "Size",     "type": "Float"},
    {"name": "Age",      "type": "Float"},
    {"name": "Lifetime", "type": "Float"}
  ],
  "nodes": [
    {"id": 1, "kind": "SpawnBurst", "stage": "Spawn",
     "params": {"count": 30}, "editorPos": [0, 0]},
    {"id": 2, "kind": "InitPositionSphere", "stage": "Spawn",
     "params": {"radius": 0.2}, "editorPos": [200, 0]},
    {"id": 3, "kind": "InitVelocityCone", "stage": "Spawn",
     "params": {
       "direction": [0, 1, 0], "coneAngleRad": 0.5, "speed": 4.0
     }, "editorPos": [400, 0]},
    {"id": 4, "kind": "InitLifetime", "stage": "Spawn",
     "params": {"min": 0.8, "max": 1.5}, "editorPos": [600, 0]},
    {"id": 5, "kind": "InitColor", "stage": "Spawn",
     "params": {"color": [1.0, 0.6, 0.2, 1.0]}, "editorPos": [400, 200]},
    {"id": 6, "kind": "InitSize", "stage": "Spawn",
     "params": {"min": 0.1, "max": 0.25}, "editorPos": [600, 200]},

    {"id": 10, "kind": "UpdateGravity", "stage": "Update",
     "params": {"gravity": [0, -2.0, 0]}, "editorPos": [0, 400]},
    {"id": 11, "kind": "UpdateDrag", "stage": "Update",
     "params": {"drag": 0.5}, "editorPos": [200, 400]},
    {"id": 12, "kind": "UpdateIntegratePosition", "stage": "Update",
     "params": {}, "editorPos": [400, 400]},
    {"id": 13, "kind": "UpdateColorOverLife", "stage": "Update",
     "params": {
       "keyframes": [
         {"t": 0.0, "color": [1.0, 0.8, 0.3, 1.0]},
         {"t": 0.5, "color": [1.0, 0.3, 0.1, 0.8]},
         {"t": 1.0, "color": [0.2, 0.1, 0.1, 0.0]}
       ]
     }, "editorPos": [600, 400]},
    {"id": 14, "kind": "UpdateAgeAndKill", "stage": "Update",
     "params": {}, "editorPos": [800, 400]}
  ],
  "edges": [
    {"from": 1, "fromPin": 1, "to": 2, "toPin": 1},
    {"from": 2, "fromPin": 1, "to": 3, "toPin": 1},
    {"from": 3, "fromPin": 1, "to": 4, "toPin": 1},
    {"from": 4, "fromPin": 1, "to": 5, "toPin": 1},
    {"from": 5, "fromPin": 1, "to": 6, "toPin": 1},

    {"from": 10, "fromPin": 1, "to": 11, "toPin": 1},
    {"from": 11, "fromPin": 1, "to": 12, "toPin": 1},
    {"from": 12, "fromPin": 1, "to": 13, "toPin": 1},
    {"from": 13, "fromPin": 1, "to": 14, "toPin": 1}
  ],
  "audioLinks": [
    {"soundKey": "Common/Fire/burst_small.wav",
     "channel": "Effect0", "volume": 0.8, "delaySec": 0.0}
  ]
}
```

**핵심**: `stage` 필드로 노드를 분류. 각 stage 는 독립 DAG 로 정렬된다.
엣지는 실행 순서 힌트 — Spawn 스테이지의 "Spawn→Init→Init→Init" 순서 강제.

### 직렬화 인터페이스

```cpp
// Engine/Public/FX/Graph/FxGraphSerializer.h
#pragma once
#include "FxGraph.h"
#include <string>

namespace Engine::FX {

class CFxGraphSerializer
{
public:
    // 직렬화 (버전 1 고정)
    static std::string SerializeToJson(const CFxGraph& g,
                                       const std::string& displayName,
                                       bool bDeterministic);

    static bool DeserializeFromJson(const std::string& json,
                                    CFxGraph& outGraph,
                                    std::string& outDisplayName,
                                    bool& outDeterministic);

    // 파일 편의 함수 (UTF-8 쓰기)
    static HRESULT SaveToFile(const std::wstring& path,
                              const CFxGraph& g,
                              const std::string& displayName,
                              bool bDeterministic);

    static HRESULT LoadFromFile(const std::wstring& path,
                                std::unique_ptr<CFxGraph>& outGraph,
                                std::string& outDisplayName,
                                bool& outDeterministic);

private:
    static std::string NodeKindToString(eNodeKind k);
    static eNodeKind   NodeKindFromString(const std::string& s);
    static std::string StageToString(eStage s);
    static eStage      StageFromString(const std::string& s);
};

} // namespace Engine::FX
```

### nlohmann::json 의존

`Engine/ThirdPartyLib/json/` 에 header-only 편입. CLAUDE.md `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차:
1. `nlohmann/json.hpp` 단일 헤더 다운로드
2. `Engine/ThirdPartyLib/json/Inc/nlohmann/json.hpp` 배치
3. Engine.vcxproj 의 `AdditionalIncludeDirectories` 에 `$(SolutionDir)Engine\ThirdPartyLib\json\Inc` 추가
4. 런타임 / 링크 의존 없음 (header-only)

## 그래프 검증 (Validator)

```cpp
// Engine/Public/FX/Graph/FxGraphValidator.h
#pragma once
#include "FxGraph.h"

namespace Engine::FX {

struct ValidationIssue
{
    enum class Level : std::uint8_t { Info, Warning, Error } level;
    NodeId      node = NULL_NODE;
    PinId       pin  = NULL_PIN;
    std::string message;
};

class CFxGraphValidator
{
public:
    // 전체 검증. Warning / Error 리스트 반환.
    static std::vector<ValidationIssue> Validate(const CFxGraph& g);

    // 빠른 검사 (에디터 드래그 중 매 프레임 호출해도 OK)
    static bool HasCycle(const CFxGraph& g);
    static bool HasMissingRequiredAttributes(const CFxGraph& g);
    static bool HasTypeMismatch(const CFxGraph& g);
};

} // namespace Engine::FX
```

### 검증 규칙

| 레벨 | 조건 | 예 |
|---|---|---|
| Error | DAG 사이클 | Node #3 → Node #5 → Node #3 |
| Error | 타입 불일치 | Float3 출력 → Float 입력 |
| Error | Update 스테이지에 IntegratePosition 누락 | Position 갱신 안 됨 |
| Error | Spawn 스테이지에 InitLifetime 누락 | Age/Kill 이 0 으로 판단해 바로 죽음 |
| Warning | AgeAndKill 앞에 IntegratePosition 없음 | 위치 갱신 전에 kill → 최종 프레임 위치 틀림 |
| Warning | 연결되지 않은 노드 | dead code |
| Info | 성능: 10만 파티클 예상 | Stage 7 GPU 권장 |

## 단위 테스트

```cpp
TEST(FxGraph, TopologicalSort_Linear)
{
    auto g = CFxGraph::Create();
    NodeId a = g->AddNode(eNodeKind::SpawnBurst, eStage::Spawn);
    NodeId b = g->AddNode(eNodeKind::InitPositionSphere, eStage::Spawn);
    NodeId c = g->AddNode(eNodeKind::InitVelocityCone, eStage::Spawn);

    g->Connect(a, 1, b, 1);
    g->Connect(b, 1, c, 1);

    std::vector<NodeId> order;
    EXPECT_TRUE(TopologicalSort(*g, eStage::Spawn, order));
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
}

TEST(FxGraph, TopologicalSort_Cycle_ReturnsFalse)
{
    auto g = CFxGraph::Create();
    NodeId a = g->AddNode(eNodeKind::ExprBinOp, eStage::Update);
    NodeId b = g->AddNode(eNodeKind::ExprBinOp, eStage::Update);
    g->Connect(a, 1, b, 1);
    g->Connect(b, 1, a, 1);   // 사이클

    std::vector<NodeId> order;
    EXPECT_FALSE(TopologicalSort(*g, eStage::Update, order));
}

TEST(FxGraph, Connect_TypeMismatch_Rejected)
{
    auto g = CFxGraph::Create();
    // (Node 생성 후 핀 타입 강제 주입 — 테스트 헬퍼로)
    NodeId a = g->AddNode(eNodeKind::ExprConst, eStage::Update);
    NodeId b = g->AddNode(eNodeKind::ExprBinOp, eStage::Update);
    g->GetMutable(a).outputs.push_back({ 1, "out", AttrType::Float, 0.f });
    g->GetMutable(b).inputs .push_back({ 1, "in",  AttrType::Float3, Vec3{} });

    EXPECT_FALSE(g->Connect(a, 1, b, 1));
}

TEST(FxGraphSerializer, Roundtrip_MatchesOriginal)
{
    auto g1 = CFxGraph::Create();
    g1->AddNode(eNodeKind::SpawnBurst, eStage::Spawn);
    std::string json = CFxGraphSerializer::SerializeToJson(*g1, "Test", false);

    CFxGraph g2_holder; std::string name; bool det = true;
    ASSERT_TRUE(CFxGraphSerializer::DeserializeFromJson(json, g2_holder, name, det));
    EXPECT_EQ(name, "Test");
    EXPECT_FALSE(det);
    EXPECT_EQ(g2_holder.AllNodes().size(), 1u);
}
```

## Gotchas (예상)

- **`Winters::Map::eTeam` 같은 네임스페이스 충돌 피하기**: `eStage` / `eNodeKind` 은 `Engine::FX` 네임스페이스 안으로 격리 (CLAUDE.md Gotcha "enum class 이름 충돌 across namespace" 대응)
- **JSON 파일 UTF-8 / BOM 처리**: 한글 displayName 이 들어갈 수 있으므로 `WintersResolveContentPath` 의 파일 쓰기는 `std::ofstream` 바이너리 모드 + BOM 없이. `.bat` 자동화 스크립트는 ASCII 전용 (CLAUDE.md Gotcha)
- **POD 포맷 변경 시 VERSION bump**: 맵 에디터와 동일하게 JSON `"version"` 증가 + 마이그레이션 함수. 초기에는 `version=1` 고정
- **EngineSDK/inc flat 경로**: 공개 헤더 include 는 `"FxTypes.h"` 식 flat. Engine.vcxproj 의 AdditionalIncludeDirectories 에 `Engine\Public\FX\Core` 등 추가 필요
- **공개 헤더는 `std::` 명시**: `FxGraph.h` 가 Client TU 에서 파싱 실패 안 하도록 `std::unordered_map` / `std::vector` / `std::variant` 전부 qualify

## 구현 순서

1. `FxTypes.h` (enum + variant + AttrType + SizeOfAttr)
2. `FxAttributeRegistry.h` (표준 이름 상수)
3. `FxGraph.h` / `.cpp` (Add/Remove/Connect + type check)
4. `FxTopoSort.h` (Kahn's)
5. 단위 테스트 3~5 케이스 (위상 정렬 / 사이클 / 타입 체크)
6. nlohmann::json ThirdPartyLib 편입
7. `FxGraphSerializer.h` / `.cpp`
8. 직렬화 roundtrip 테스트
9. `FxGraphValidator.h` / `.cpp`
10. 수동 JSON 작성 → 로드 → 검증 → 위상 정렬 전체 파이프 검증

## 다음 Stage

Stage 2 — `ParticlePool` (SoA) 설계. 위상 정렬된 노드를 실행하려면 먼저 **파티클이 살 집** 이 필요하다.
