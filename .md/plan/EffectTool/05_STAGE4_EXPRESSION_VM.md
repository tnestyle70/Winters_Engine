# Stage 4 — Expression VM (바이트코드 스택 머신)

## 목표

아티스트가 **임의 수식** 으로 파티클 속성을 제어할 수 있도록 미니 바이트코드 VM 을 구현.
Niagara 의 VectorVM 의 축소판. CPU 실행에 먼저 쓰고, **동일 IR** 을 Stage 7 에서 HLSL 로 번역.

## 왜 필요한가 — 하드코딩 곡선의 한계

Stage 3 의 `UpdateColorOverLife` 는 keyframe 만 지원.
아티스트가 원하는 복잡 표현:

- `color = lerp(startColor, endColor, sin(age * 10) * 0.5 + 0.5)` (맥박)
- `size = baseSize * (1 - pow(age/lifetime, 2))` (제곱 감쇠)
- `velocity.y += sin(time + seed * PI) * 3.0` (난류)

이걸 전부 전용 노드로 만들면 N 종 → 수백 종 폭발. **수식 하나를 데이터로 표현** 하는 게 훨씬 유연.

## 설계 원칙

| 원칙 | 이유 |
|---|---|
| 스택 머신 (레지스터 아님) | 단순, 컴파일 쉬움, 파티클당 32 word 면 충분 |
| 파티클당 한 번 실행 | N 파티클 = N 번 루프. 벡터화는 나중 |
| 고정 부동소수점 (float32) | 혼합 타입 없음 → Vec3/Vec4 는 3~4 스텝으로 풀어씀 |
| IR 은 CPU / GPU 공유 | Stage 7 에서 동일 명령어 → HLSL 생성 |
| 파라미터는 immediate 또는 attr slot | 파일 크기 작음 |

## IR (Intermediate Representation)

```cpp
// Engine/Public/FX/Expression/FxBytecode.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::FX {

enum class eOp : std::uint8_t
{
    // 스택 조작
    PushConst   = 0x01,     // imm (float) 푸시
    LoadAttr    = 0x02,     // slot 의 attr 값 푸시
    StoreAttr   = 0x03,     // 스택 top 을 slot attr 에 저장 (pop)
    PushParam   = 0x04,     // cbuffer 느낌 — global 값 (time, seed)
    Dup         = 0x05,
    Drop        = 0x06,
    Swap        = 0x07,

    // 산술 (2-ary)
    Add         = 0x10,
    Sub         = 0x11,
    Mul         = 0x12,
    Div         = 0x13,
    Min         = 0x14,
    Max         = 0x15,
    Mod         = 0x16,
    Pow         = 0x17,

    // 1-ary
    Neg         = 0x20,
    Abs         = 0x21,
    Sqrt        = 0x22,
    Sin         = 0x23,
    Cos         = 0x24,
    Exp         = 0x25,
    Log         = 0x26,
    Floor       = 0x27,
    Saturate    = 0x28,     // clamp [0, 1]

    // 3-ary
    Lerp        = 0x30,     // stack: a, b, t → a + (b-a)*t
    Clamp       = 0x31,     // stack: x, lo, hi → min(max(x, lo), hi)
    Smoothstep  = 0x32,     // stack: edge0, edge1, x

    // 비교 → 0/1
    LessThan    = 0x40,
    Greater     = 0x41,
    Equal       = 0x42,

    // 종료
    End         = 0xFF
};

struct Instr
{
    eOp            op;
    f32_t          imm   = 0.f;   // PushConst, Smoothstep 같은 하나의 float immediate
    std::uint16_t  slot  = 0;     // LoadAttr/StoreAttr 가 참조할 attr index
};

} // namespace Engine::FX
```

## Slot Table

VM 은 attribute 이름을 모른다. 컴파일 시 `slot_id → "Position.y"` 식 매핑 생성:

```cpp
// Engine/Public/FX/Expression/FxSlotTable.h
#pragma once
#include "FxBytecode.h"
#include "ParticlePool.h"

namespace Engine::FX {

struct AttrSlot
{
    std::string attrName;    // "Color"
    std::uint8_t component = 0;   // 0=x/r, 1=y/g, 2=z/b, 3=w/a (Float 면 0)
};

class CFxSlotTable
{
public:
    // "Color.r" → slot 1 등록 (중복 시 기존 slot 반환)
    std::uint16_t Resolve(const std::string& attrName, std::uint8_t component);

    const AttrSlot& Get(std::uint16_t slot) const { return m_slots.at(slot); }
    std::uint16_t   Count() const { return static_cast<std::uint16_t>(m_slots.size()); }

private:
    std::vector<AttrSlot> m_slots;
};

} // namespace Engine::FX
```

## VM

```cpp
// Engine/Public/FX/Expression/FxExprVM.h
#pragma once
#include "FxBytecode.h"
#include "FxSlotTable.h"
#include "ParticlePool.h"

namespace Engine::FX {

struct ExprGlobals
{
    f32_t time      = 0.f;
    f32_t deltaTime = 0.f;
    f32_t emitterAge = 0.f;
};

// 한 파티클에 대해 프로그램 실행.
// code: 컴파일된 Instr 배열
// pool: 파티클 데이터
// i: 파티클 인덱스
// slots: 슬롯 → (attr, component) 매핑
// globals: 전역 값
void RunExpr(const std::vector<Instr>& code,
             std::uint32_t i,
             CParticlePool& pool,
             const CFxSlotTable& slots,
             const ExprGlobals& globals);

// N 개 파티클 전부 실행 (핫 루프)
void RunExprBatch(const std::vector<Instr>& code,
                  CParticlePool& pool,
                  const CFxSlotTable& slots,
                  const ExprGlobals& globals);

} // namespace Engine::FX
```

### 구현

```cpp
// Engine/Private/FX/Expression/FxExprVM.cpp
#include "FxExprVM.h"
#include "FxAttributeRegistry.h"
#include <cmath>
#include <algorithm>

namespace Engine::FX {

static f32_t ReadAttr(CParticlePool& p, std::uint32_t i, const AttrSlot& s)
{
    // attr 타입에 따라 다르게 읽어야 하나, Expression 은 scalar 전용 → 전부 Float 로 취급
    // Float3/Float4 는 (attrName, component) 로 풀었음
    const AttrType t = [&]{
        for (const auto& d : p.GetAttributes())
            if (d.name == s.attrName) return d.type;
        return AttrType::Float;
    }();

    switch (t) {
        case AttrType::Float: {
            auto* a = p.Data<f32_t>(s.attrName);
            return a[i];
        }
        case AttrType::Float2: {
            auto* a = p.Data<Vec2>(s.attrName);
            return (s.component == 0) ? a[i].x : a[i].y;
        }
        case AttrType::Float3: {
            auto* a = p.Data<Vec3>(s.attrName);
            if (s.component == 0) return a[i].x;
            if (s.component == 1) return a[i].y;
            return a[i].z;
        }
        case AttrType::Float4: {
            auto* a = p.Data<Vec4>(s.attrName);
            if (s.component == 0) return a[i].x;
            if (s.component == 1) return a[i].y;
            if (s.component == 2) return a[i].z;
            return a[i].w;
        }
        default: return 0.f;
    }
}

static void WriteAttr(CParticlePool& p, std::uint32_t i, const AttrSlot& s, f32_t val)
{
    for (const auto& d : p.GetAttributes()) {
        if (d.name != s.attrName) continue;
        switch (d.type) {
            case AttrType::Float:  p.Data<f32_t>(s.attrName)[i] = val; return;
            case AttrType::Float2: {
                auto& v = p.Data<Vec2>(s.attrName)[i];
                if (s.component == 0) v.x = val; else v.y = val;
                return;
            }
            case AttrType::Float3: {
                auto& v = p.Data<Vec3>(s.attrName)[i];
                if (s.component == 0) v.x = val;
                else if (s.component == 1) v.y = val;
                else v.z = val;
                return;
            }
            case AttrType::Float4: {
                auto& v = p.Data<Vec4>(s.attrName)[i];
                if (s.component == 0) v.x = val;
                else if (s.component == 1) v.y = val;
                else if (s.component == 2) v.z = val;
                else v.w = val;
                return;
            }
            default: return;
        }
    }
}

void RunExpr(const std::vector<Instr>& code,
             std::uint32_t i,
             CParticlePool& pool,
             const CFxSlotTable& slots,
             const ExprGlobals& g)
{
    f32_t stack[32];
    int sp = 0;

    auto push = [&](f32_t v) { stack[sp++] = v; };
    auto pop  = [&]() -> f32_t { return stack[--sp]; };

    for (const auto& ins : code) {
        switch (ins.op) {
        case eOp::PushConst:   push(ins.imm); break;
        case eOp::LoadAttr:    push(ReadAttr(pool, i, slots.Get(ins.slot))); break;
        case eOp::StoreAttr:   WriteAttr(pool, i, slots.Get(ins.slot), pop()); break;
        case eOp::PushParam: {
            // ins.slot 을 global 식별자로 재활용
            switch (ins.slot) {
                case 0: push(g.time); break;
                case 1: push(g.deltaTime); break;
                case 2: push(g.emitterAge); break;
                default: push(0.f); break;
            }
        } break;
        case eOp::Dup:  { f32_t t = stack[sp-1]; push(t); } break;
        case eOp::Drop: { --sp; } break;
        case eOp::Swap: { std::swap(stack[sp-1], stack[sp-2]); } break;

        case eOp::Add: { f32_t b = pop(), a = pop(); push(a + b); } break;
        case eOp::Sub: { f32_t b = pop(), a = pop(); push(a - b); } break;
        case eOp::Mul: { f32_t b = pop(), a = pop(); push(a * b); } break;
        case eOp::Div: { f32_t b = pop(), a = pop(); push(a / b); } break;
        case eOp::Min: { f32_t b = pop(), a = pop(); push(std::min(a, b)); } break;
        case eOp::Max: { f32_t b = pop(), a = pop(); push(std::max(a, b)); } break;
        case eOp::Mod: { f32_t b = pop(), a = pop(); push(std::fmod(a, b)); } break;
        case eOp::Pow: { f32_t b = pop(), a = pop(); push(std::pow(a, b)); } break;

        case eOp::Neg:      stack[sp-1] = -stack[sp-1]; break;
        case eOp::Abs:      stack[sp-1] = std::abs(stack[sp-1]); break;
        case eOp::Sqrt:     stack[sp-1] = std::sqrt(std::max(0.f, stack[sp-1])); break;
        case eOp::Sin:      stack[sp-1] = std::sin(stack[sp-1]); break;
        case eOp::Cos:      stack[sp-1] = std::cos(stack[sp-1]); break;
        case eOp::Exp:      stack[sp-1] = std::exp(stack[sp-1]); break;
        case eOp::Log:      stack[sp-1] = std::log(std::max(1e-8f, stack[sp-1])); break;
        case eOp::Floor:    stack[sp-1] = std::floor(stack[sp-1]); break;
        case eOp::Saturate: stack[sp-1] = std::clamp(stack[sp-1], 0.f, 1.f); break;

        case eOp::Lerp: {
            f32_t t = pop(), b = pop(), a = pop();
            push(a + (b - a) * t);
        } break;
        case eOp::Clamp: {
            f32_t hi = pop(), lo = pop(), x = pop();
            push(std::clamp(x, lo, hi));
        } break;
        case eOp::Smoothstep: {
            f32_t x = pop(), e1 = pop(), e0 = pop();
            f32_t t = std::clamp((x - e0) / std::max(1e-8f, e1 - e0), 0.f, 1.f);
            push(t * t * (3.f - 2.f * t));
        } break;

        case eOp::LessThan: { f32_t b = pop(), a = pop(); push(a < b ? 1.f : 0.f); } break;
        case eOp::Greater:  { f32_t b = pop(), a = pop(); push(a > b ? 1.f : 0.f); } break;
        case eOp::Equal:    { f32_t b = pop(), a = pop(); push(std::abs(a - b) < 1e-6f ? 1.f : 0.f); } break;

        case eOp::End: return;
        }
    }
}

void RunExprBatch(const std::vector<Instr>& code,
                  CParticlePool& pool,
                  const CFxSlotTable& slots,
                  const ExprGlobals& g)
{
    const std::uint32_t N = pool.AliveCount();
    for (std::uint32_t i = 0; i < N; ++i)
        RunExpr(code, i, pool, slots, g);
}

} // namespace Engine::FX
```

## 컴파일러 — Expression 서브그래프 → 바이트코드

Expression 노드 (`ExprBinOp`, `ExprConst`, `ExprAttrRead`, `ExprAttrWrite`, `ExprLerp`, `ExprUnaryOp`)
가 연결된 서브그래프를 후위 순회하면 스택 머신 명령어가 자연스럽게 나온다.

```cpp
// Engine/Public/FX/Expression/FxExprCompiler.h
#pragma once
#include "FxGraph.h"
#include "FxBytecode.h"
#include "FxSlotTable.h"

namespace Engine::FX {

class CFxExprCompiler
{
public:
    // StoreAttr 로 끝나는 루트 노드 (표현식의 최종 대상 — Attr write) 를 받아
    // 재귀적으로 후위 순회하며 명령어 생성.
    bool Compile(const CFxGraph& graph,
                 NodeId rootExprWrite,
                 std::vector<Instr>& outCode,
                 CFxSlotTable& inOutSlots);

private:
    // 후위 순회
    bool Emit(const CFxGraph& graph,
              NodeId node,
              std::vector<Instr>& outCode,
              CFxSlotTable& slots);

    // 노드의 fromNode 입력을 따라 상류로
    NodeId UpstreamOf(const CFxGraph& graph, NodeId node, PinId inputPin) const;
};

} // namespace Engine::FX
```

### 컴파일러 구현 핵심

```cpp
// Engine/Private/FX/Expression/FxExprCompiler.cpp
#include "FxExprCompiler.h"

namespace Engine::FX {

NodeId CFxExprCompiler::UpstreamOf(const CFxGraph& g, NodeId n, PinId inPin) const
{
    for (const auto& e : g.AllEdges())
        if (e.toNode == n && e.toPin == inPin) return e.fromNode;
    return NULL_NODE;
}

bool CFxExprCompiler::Emit(const CFxGraph& g,
                            NodeId nid,
                            std::vector<Instr>& code,
                            CFxSlotTable& slots)
{
    if (!g.Exists(nid)) return false;
    const Node& n = g.Get(nid);

    switch (n.kind) {
    case eNodeKind::ExprConst: {
        const f32_t v = std::get<f32_t>(n.params.at("value"));
        code.push_back({ eOp::PushConst, v, 0 });
        return true;
    }
    case eNodeKind::ExprAttrRead: {
        const auto& aname = std::get<std::string>(n.params.at("attr"));
        const std::int32_t c = std::get<std::int32_t>(n.params.at("component"));
        const std::uint16_t sid = slots.Resolve(aname, static_cast<std::uint8_t>(c));
        code.push_back({ eOp::LoadAttr, 0.f, sid });
        return true;
    }
    case eNodeKind::ExprBinOp: {
        // 왼쪽 피연산자 → 오른쪽 피연산자 → Op
        NodeId L = UpstreamOf(g, nid, 1);
        NodeId R = UpstreamOf(g, nid, 2);
        if (!Emit(g, L, code, slots)) return false;
        if (!Emit(g, R, code, slots)) return false;
        const auto& opStr = std::get<std::string>(n.params.at("op"));
        eOp op = eOp::Add;
        if (opStr == "add") op = eOp::Add;
        else if (opStr == "sub") op = eOp::Sub;
        else if (opStr == "mul") op = eOp::Mul;
        else if (opStr == "div") op = eOp::Div;
        else if (opStr == "min") op = eOp::Min;
        else if (opStr == "max") op = eOp::Max;
        else if (opStr == "pow") op = eOp::Pow;
        code.push_back({ op, 0.f, 0 });
        return true;
    }
    case eNodeKind::ExprUnaryOp: {
        NodeId In = UpstreamOf(g, nid, 1);
        if (!Emit(g, In, code, slots)) return false;
        const auto& opStr = std::get<std::string>(n.params.at("op"));
        eOp op = eOp::Abs;
        if (opStr == "abs")      op = eOp::Abs;
        else if (opStr == "neg") op = eOp::Neg;
        else if (opStr == "sin") op = eOp::Sin;
        else if (opStr == "cos") op = eOp::Cos;
        else if (opStr == "saturate") op = eOp::Saturate;
        code.push_back({ op, 0.f, 0 });
        return true;
    }
    case eNodeKind::ExprLerp: {
        NodeId A = UpstreamOf(g, nid, 1);
        NodeId B = UpstreamOf(g, nid, 2);
        NodeId T = UpstreamOf(g, nid, 3);
        if (!Emit(g, A, code, slots)) return false;
        if (!Emit(g, B, code, slots)) return false;
        if (!Emit(g, T, code, slots)) return false;
        code.push_back({ eOp::Lerp, 0.f, 0 });
        return true;
    }
    default:
        return false;   // 모르는 Expr 노드
    }
}

bool CFxExprCompiler::Compile(const CFxGraph& g,
                               NodeId rootWrite,
                               std::vector<Instr>& code,
                               CFxSlotTable& slots)
{
    if (!g.Exists(rootWrite)) return false;
    const Node& n = g.Get(rootWrite);
    if (n.kind != eNodeKind::ExprAttrWrite) return false;

    // 대상 attr + component 찾기
    const auto& aname = std::get<std::string>(n.params.at("attr"));
    const std::int32_t c = std::get<std::int32_t>(n.params.at("component"));
    const std::uint16_t sid = slots.Resolve(aname, static_cast<std::uint8_t>(c));

    // 값 표현식 (입력 pin 1)
    NodeId valNode = UpstreamOf(g, rootWrite, 1);
    code.clear();
    if (!Emit(g, valNode, code, slots)) return false;

    code.push_back({ eOp::StoreAttr, 0.f, sid });
    code.push_back({ eOp::End,       0.f, 0 });
    return true;
}

} // namespace Engine::FX
```

## 예시 — "맥박 색" 표현식

```
ExprConst(v=10.0) ─┐
                   ├──▶ ExprBinOp(op=mul) ─┐
ExprAttrRead(Age) ─┘                       ├──▶ ExprUnaryOp(op=sin) ─┐
                                                                      ├──▶ ExprBinOp(op=mul) ─┐
ExprConst(v=0.5)  ──────────────────────────────────────────────────  ┘                       ├──▶ ExprBinOp(op=add) ──▶ ExprAttrWrite(Color.a)
                                                                                                ExprConst(v=0.5) ─────────┘
```

컴파일 결과:
```
PushConst 10.0
LoadAttr  0    ; slot 0 = Age
Mul
Sin
PushConst 0.5
Mul
PushConst 0.5
Add
StoreAttr 1    ; slot 1 = Color.a
End
```

즉 `Color.a = sin(Age * 10) * 0.5 + 0.5`.

## Update 스테이지에 Expression 노드 실행

`ExprAttrWrite` 는 내부적으로 "값 표현식 전체" 를 **바이트코드로 미리 컴파일** 해둔 노드.
Executor 가 이 노드를 만나면 바이트코드를 파티클 전체에 돌림:

```cpp
// NodeRegistry.cpp 에 추가
static void ExecUpdateExprWrite(const Node& n, FxExecContext& c)
{
    // 컴파일된 바이트코드는 params["compiled_code"] 에 std::any 로 저장
    const auto* code  = std::any_cast<std::vector<Instr>>(&n.params.at("compiled_code_any"));
    const auto* slots = std::any_cast<CFxSlotTable>(&n.params.at("compiled_slots_any"));
    if (!code || !slots) return;

    ExprGlobals g;
    g.deltaTime  = c.deltaTime;
    g.emitterAge = c.emitterAge;
    g.time       = /* 외부에서 주입 — Tier 2 global 접근 */ 0.f;

    RunExprBatch(*code, *c.pool, *slots, g);
}
```

단 `std::variant` 에 `std::any` 저장 불가 → `Node::params` 의 타입 확장 필요:

```cpp
// FxTypes.h 의 FxValue 에 추가 (Phase 1 MVP 가 끝나면 확장)
using FxValue = std::variant<
    float, Vec2, Vec3, Vec4,
    std::int32_t, bool, std::string,
    std::vector<Instr>,       // ★ Expression 컴파일 결과
    CFxSlotTable              // ★ slot table (or 별도 멤버로)
>;
```

또는 간단한 대안: `Node` 에 `std::shared_ptr<CompiledExpr>` 별도 멤버. **이게 더 깔끔**:

```cpp
struct CompiledExpr {
    std::vector<Instr> code;
    CFxSlotTable       slots;
};

struct Node {
    // ... 기존 ...
    std::shared_ptr<CompiledExpr> compiledExpr;   // 선택. ExprAttrWrite 만 사용
};
```

## HLSL 백엔드 (Stage 7 예고)

동일 IR → HLSL 문자열 생성. 스택 기반이므로 local `float s0, s1, ...` 를 직접 푼다:

```cpp
// 의사 코드
std::string ToHlsl(const std::vector<Instr>& code, const CFxSlotTable& slots)
{
    std::string src;
    int sp = 0;
    std::vector<std::string> stack;   // HLSL 식 문자열 스택

    for (const auto& ins : code) {
        switch (ins.op) {
        case eOp::PushConst: stack.push_back(std::to_string(ins.imm)); break;
        case eOp::LoadAttr: {
            const auto& s = slots.Get(ins.slot);
            stack.push_back("g_" + s.attrName + "[gid]" + swizzle(s.component));
        } break;
        case eOp::Mul: {
            std::string b = stack.back(); stack.pop_back();
            std::string a = stack.back(); stack.pop_back();
            stack.push_back("(" + a + " * " + b + ")");
        } break;
        case eOp::Sin: {
            std::string x = stack.back(); stack.pop_back();
            stack.push_back("sin(" + x + ")");
        } break;
        case eOp::StoreAttr: {
            const auto& s = slots.Get(ins.slot);
            std::string x = stack.back(); stack.pop_back();
            src += "    g_" + s.attrName + "[gid]" + swizzle(s.component) + " = " + x + ";\n";
        } break;
        // ...
        }
    }
    return src;
}
```

이게 Stage 7 의 "FxGraph → HLSL Compute Shader" 핵심.

## 성능

- 간단 표현식 (10~20 Instr) × 10,000 파티클 = 200K op = **~0.3 ms (CPU)**
- 복잡 표현식 (50 Instr) = ~1 ms — 여전히 예산 내
- 만 단위 파티클에서 느껴지면 Stage 7 GPU 로 이관

## 단위 테스트

```cpp
TEST(ExprVM, ConstAddStore)
{
    using namespace Engine::FX;
    std::vector<Instr> code = {
        { eOp::PushConst, 1.5f, 0 },
        { eOp::PushConst, 2.0f, 0 },
        { eOp::Add,       0.f,  0 },
        { eOp::StoreAttr, 0.f,  0 },
        { eOp::End,       0.f,  0 },
    };

    auto pool = CParticlePool::Create();
    pool->RegisterAttribute("Size", AttrType::Float);
    pool->Reserve(4);
    pool->Allocate(4);

    CFxSlotTable slots;
    slots.Resolve("Size", 0);

    RunExprBatch(code, *pool, slots, ExprGlobals{});

    auto* s = pool->Data<f32_t>("Size");
    for (int i = 0; i < 4; ++i) EXPECT_FLOAT_EQ(s[i], 3.5f);
}

TEST(ExprVM, SinOfAge)
{
    using namespace Engine::FX;
    std::vector<Instr> code = {
        { eOp::LoadAttr, 0.f, 0 },   // slot 0 = Age
        { eOp::Sin,      0.f, 0 },
        { eOp::StoreAttr, 0.f, 1 },   // slot 1 = Size
        { eOp::End,       0.f, 0 },
    };

    auto pool = CParticlePool::Create();
    pool->RegisterAttribute("Age",  AttrType::Float);
    pool->RegisterAttribute("Size", AttrType::Float);
    pool->Reserve(1);
    pool->Allocate(1);
    pool->Data<f32_t>("Age")[0] = 1.5707963f;   // π/2

    CFxSlotTable slots;
    slots.Resolve("Age", 0);
    slots.Resolve("Size", 0);

    RunExprBatch(code, *pool, slots, ExprGlobals{});
    EXPECT_NEAR(pool->Data<f32_t>("Size")[0], 1.0f, 1e-5f);
}

TEST(ExprCompiler, SimpleMulAddGraph)
{
    // (Age * 2) + 1 → Color.a
    auto g = CFxGraph::Create();
    NodeId cA = g->AddNode(eNodeKind::ExprAttrRead,  eStage::Update);
    g->GetMutable(cA).params["attr"]      = std::string("Age");
    g->GetMutable(cA).params["component"] = std::int32_t(0);

    NodeId cB = g->AddNode(eNodeKind::ExprConst,     eStage::Update);
    g->GetMutable(cB).params["value"]     = 2.f;

    NodeId cMul = g->AddNode(eNodeKind::ExprBinOp,   eStage::Update);
    g->GetMutable(cMul).params["op"]      = std::string("mul");

    NodeId cC = g->AddNode(eNodeKind::ExprConst,     eStage::Update);
    g->GetMutable(cC).params["value"]     = 1.f;

    NodeId cAdd = g->AddNode(eNodeKind::ExprBinOp,   eStage::Update);
    g->GetMutable(cAdd).params["op"]      = std::string("add");

    NodeId cW = g->AddNode(eNodeKind::ExprAttrWrite, eStage::Update);
    g->GetMutable(cW).params["attr"]      = std::string("Color");
    g->GetMutable(cW).params["component"] = std::int32_t(3);  // alpha

    // 엣지: cA → cMul[1], cB → cMul[2], cMul → cAdd[1], cC → cAdd[2], cAdd → cW[1]
    g->Connect(cA, 1, cMul, 1);
    g->Connect(cB, 1, cMul, 2);
    g->Connect(cMul, 1, cAdd, 1);
    g->Connect(cC,  1, cAdd, 2);
    g->Connect(cAdd, 1, cW, 1);

    CFxExprCompiler comp;
    std::vector<Instr> code;
    CFxSlotTable slots;
    ASSERT_TRUE(comp.Compile(*g, cW, code, slots));

    // Age = 3, 기대 결과 = 3*2+1 = 7
    auto pool = CParticlePool::Create();
    pool->RegisterAttribute("Age",   AttrType::Float);
    pool->RegisterAttribute("Color", AttrType::Float4);
    pool->Reserve(1); pool->Allocate(1);
    pool->Data<f32_t>("Age")[0] = 3.f;

    RunExprBatch(code, *pool, slots, ExprGlobals{});
    EXPECT_FLOAT_EQ(pool->Data<Vec4>("Color")[0].w, 7.f);
}
```

## Gotchas

- **스택 오버플로우**: 고정 크기 32 word. 복잡 표현식이 초과할 수 있음 → 컴파일 시 정적 분석으로 필요 깊이 계산해 크기 결정 또는 상한 초과 시 에러
- **`std::fmod` vs HLSL `%`**: HLSL 은 부호 처리 다름. 양쪽 동일 결과 원하면 `fmod` 를 명시적으로 재구현
- **`std::any_cast` / `std::get<variant>` 예외**: 잘못된 타입이면 throw. Node 빌드 단계에서 검증
- **컴파일 결과 캐싱**: ExprAttrWrite 노드마다 컴파일 → `Node::compiledExpr` 에 저장. Executor 가 매 프레임 재컴파일 금지
- **부동소수 결정성**: 결정적 FX 에서 같은 표현식이 플랫폼별 다른 값을 뱉을 수 있음. `-ffast-math` 끄기, FPU 모드 `_controlfp` 고정 (Phase 4 네트워크 준비 시)
- **SlotTable 불일치**: 같은 attr 이름이라도 컴파일 시 component 다르면 별도 slot. Slot 공유하려면 컴파일 캐시 공유

## 구현 순서

1. `FxBytecode.h` (enum + Instr)
2. `FxSlotTable.h` + `.cpp`
3. `FxExprVM.h` + `.cpp` (RunExpr + RunExprBatch)
4. 단위 테스트 (Const, Add, Sin 등)
5. `FxExprCompiler.h` + `.cpp`
6. 복잡 표현식 roundtrip 테스트 ((a*2)+1)
7. NodeRegistry 에 `ExecUpdateExprWrite` 추가
8. Stage 3 의 `UpdateColorOverLife` 노드를 Expression 서브그래프로 재구성 가능함을 문서화

## 다음 Stage

Stage 5 — DX11 인스턴싱 렌더러. 파티클이 시뮬되고 있지만 아직 화면에 **안 보임**. 빌보드 쿼드를 인스턴스로 뿌려야 첫 불꽃이 스크린에 뜬다.
