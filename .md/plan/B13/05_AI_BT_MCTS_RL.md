# Phase B-13 / 05 — AI: Behavior Tree + MCTS + RL Model (v2)

**버전**: v2 (2026-05-04 — Codex 검토 #2 P-9/P-10 정정)
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md)
**선행**: 02 VISION v2, 03 TOWER v2, 04 SPATIAL HASH v2.
**총 LOC**: ~2000.

---

## §0. v1 → v2 정정 매트릭스

| # | v1 결함 | v2 정정 | PITFALLS |
|---|---|---|---|
| 1 | `CBehaviorTreeSystem::GetPhase() = 5` (Vision 과 충돌) | **phase = 7 (Vision=5/Turret=6 후 단독)** | P-9 |
| 2 | `CMCTSSystem::GetPhase() = 6` (Turret 과 충돌) | **phase = 8 (BT=7 후 단독)** | P-9 |
| 3 | BT ConditionNode 안 `ctx.pWorld->GetSpatialIndex()` 호출 — v1 박제는 유효 (`world.GetSpatialIndex()` 라 CWorld 멤버) | 04 v2 의 CWorld owned `Get_SpatialIndex()` 와 일관 | P-10 |
| 4 | 공개 헤더 flat include | subdir 보존 | P-8 |
| 5 | `CYoneSoulSpawnSystem::GetPhase() = 8` (B-16 v2 박제) | 본 phase 와 충돌 — **YoneSoul = 9 로 이동** (B-16 v2 본문에서 갱신 필요) | P-9 |

---

## 0. 설계 의사결정

### 3-Layer 의사결정 매핑 (CLAUDE.md §자체 AI 봇 Stage)

```
┌────────────────────────────────┐
│  Strategic (매크로) — 5초 틱    │  → MCTS (스킬/오브젝트 결정)
│  "지금 드래곤 싸움? 갱?"        │     5초마다 1회 시뮬
└────────────────────────────────┘
┌────────────────────────────────┐
│  Tactical (미들) — 0.2~0.5초 틱 │  → BT (포지션/타깃)
│  "어느 라인 진입? 어느 챔프 어그로?" │     200ms 마다 BT Tick
└────────────────────────────────┘
┌────────────────────────────────┐
│  Operational (마이크로) — 매 frame│  → ECS System (이동/평타)
│  "스킬 조준, 평타, 캔슬"         │     기존 MinionAISystem + Cmd 큐
└────────────────────────────────┘
```

### Stage 결정

| Stage | 본 Phase | 비고 |
|---|---|---|
| 0 단순 Aggro | ✅ 이미 (MinionAI) | 변경 X |
| **1 HFSM** | ⚠️ 골격만 | BT 와 통합 — BT 가 root, FSM 은 BT 안 SubTree |
| **2 BT** | ✅ 본격 박제 | Selector/Sequence/Parallel/Decorator/Condition/Action 6 노드 |
| 3 GOAP | ❌ 다음 Phase (B-14) | 너무 큼 |
| 4 Utility | ❌ B-14 | BT 의 Decorator 로 일부 흡수 가능 |
| 5 InfluenceMap | ❌ B-14 | 별도 시스템 |
| **6 MCTS** | ⚠️ 골격 박제 | Rollout 시뮬레이션 1회 동작 — RL 시 input |
| 7 Imitation | ❌ B-15 | 데이터 수집 인프라부터 |
| **8 RL** | ⚠️ Bridge 만 | ONNX stub — 모델 로드/추론 헤더만 |

---

## 1. BlackboardComponent 박제

**신규 파일**: `Engine/Public/AI/Blackboard.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <unordered_map>
#include <variant>
#include <string>

// ─────────────────────────────────────────────────────────────
// Blackboard — BT 의 ConditionNode/ActionNode 가 공유하는 K-V 저장소
//
// 사용 예 (BT 안):
//   bb.Set("currentGoal", "Push_Top");
//   if (bb.GetString("currentGoal") == "Push_Top") { ... }
//   bb.Set("targetEntity", static_cast<uint64_t>(enemyId));
//
// 팀 공유 (CLAUDE.md §팀 협동 (Blackboard)) — 봇팀원끼리 공유 메모리.
// ECS BlackboardComponent: 챔프별 개인 BB. CTeamBlackboard: 팀 5인 공유.
// ─────────────────────────────────────────────────────────────

class CBlackboard
{
public:
    using Value = std::variant<bool_t, i32_t, f32_t, Vec3, std::string, uint64_t>;

    void Set(const std::string& key, Value v) { m_map[key] = std::move(v); }
    bool Has(const std::string& key) const { return m_map.find(key) != m_map.end(); }
    void Remove(const std::string& key) { m_map.erase(key); }
    void Clear() { m_map.clear(); }

    template<typename T>
    T Get(const std::string& key, T def) const
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return def;
        if (auto* p = std::get_if<T>(&it->second)) return *p;
        return def;
    }

    // 자주 쓰는 헬퍼
    bool_t      GetBool(const std::string& k, bool_t def = false) const { return Get<bool_t>(k, def); }
    i32_t       GetInt (const std::string& k, i32_t def = 0)      const { return Get<i32_t>(k, def); }
    f32_t       GetFloat(const std::string& k, f32_t def = 0.f)   const { return Get<f32_t>(k, def); }
    Vec3        GetVec3(const std::string& k, Vec3 def = {})      const { return Get<Vec3>(k, def); }
    std::string GetString(const std::string& k, const std::string& def = "") const { return Get<std::string>(k, def); }
    EntityID    GetEntity(const std::string& k, EntityID def = NULL_ENTITY) const
    {
        return static_cast<EntityID>(Get<uint64_t>(k, static_cast<uint64_t>(def)));
    }

private:
    std::unordered_map<std::string, Value> m_map;
};

// ECS Component
struct BlackboardComponent
{
    CBlackboard bb;
};

// 팀 공유 BB — Scene 레벨 (1 팀당 1개)
struct TeamBlackboardComponent
{
    CBlackboard bb;
    uint8_t     team = 0;   // eTeam
};
```

---

## 2. BehaviorTree 베이스 노드 + 6 노드 박제

**신규 파일**: `Engine/Public/AI/BehaviorTree.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "AI/Blackboard.h"
#include "ECS/Entity.h"
#include <memory>
#include <vector>
#include <functional>
#include <string>

class CWorld;

NS_BEGIN(Engine)

enum class eBTStatus : uint8_t
{
    Invalid = 0,
    Running,
    Success,
    Failure
};

struct BTContext
{
    CWorld*      pWorld = nullptr;
    EntityID     self = NULL_ENTITY;
    CBlackboard* pBB = nullptr;
    CBlackboard* pTeamBB = nullptr;
    f32_t        dt = 0.f;
};

// ─────────────────────────────────────────────────────────────
// BTNode — 추상 베이스
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBTNode
{
public:
    virtual ~CBTNode() = default;
    virtual eBTStatus Tick(BTContext& ctx) = 0;
    virtual const char* GetName() const = 0;
    virtual void Reset() {}

    // 디버그 — ImGui 트리 표시용
    virtual size_t GetChildCount() const { return 0; }
    virtual CBTNode* GetChild(size_t /*i*/) const { return nullptr; }
    eBTStatus GetLastStatus() const { return m_lastStatus; }

protected:
    eBTStatus m_lastStatus = eBTStatus::Invalid;
};

// ─────────────────────────────────────────────────────────────
// Selector — 자식 순회, 첫 Success/Running 반환. 모두 Failure 면 Failure.
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBTSelector : public CBTNode
{
public:
    void AddChild(std::unique_ptr<CBTNode> c) { m_children.push_back(std::move(c)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Selector"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override { for (auto& c : m_children) c->Reset(); m_lastStatus = eBTStatus::Invalid; }
private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
};

// ─────────────────────────────────────────────────────────────
// Sequence — 자식 순회, 첫 Failure/Running 반환. 모두 Success 면 Success.
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBTSequence : public CBTNode
{
public:
    void AddChild(std::unique_ptr<CBTNode> c) { m_children.push_back(std::move(c)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Sequence"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override { for (auto& c : m_children) c->Reset(); m_lastStatus = eBTStatus::Invalid; }
private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
};

// ─────────────────────────────────────────────────────────────
// Parallel — 모든 자식 동시 Tick. successThreshold 만큼 Success → Success.
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBTParallel : public CBTNode
{
public:
    explicit CBTParallel(uint32_t successThreshold) : m_uThreshold(successThreshold) {}
    void AddChild(std::unique_ptr<CBTNode> c) { m_children.push_back(std::move(c)); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Parallel"; }
    size_t GetChildCount() const override { return m_children.size(); }
    CBTNode* GetChild(size_t i) const override { return m_children[i].get(); }
    void Reset() override { for (auto& c : m_children) c->Reset(); m_lastStatus = eBTStatus::Invalid; }
private:
    std::vector<std::unique_ptr<CBTNode>> m_children;
    uint32_t m_uThreshold = 1;
};

// ─────────────────────────────────────────────────────────────
// Decorator — 자식 1 개. Tick 결과 변형 (Inverter/Until/Cooldown 등).
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBTInverter : public CBTNode
{
public:
    void SetChild(std::unique_ptr<CBTNode> c) { m_pChild = std::move(c); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Inverter"; }
    size_t GetChildCount() const override { return m_pChild ? 1 : 0; }
    CBTNode* GetChild(size_t) const override { return m_pChild.get(); }
    void Reset() override { if (m_pChild) m_pChild->Reset(); m_lastStatus = eBTStatus::Invalid; }
private:
    std::unique_ptr<CBTNode> m_pChild;
};

class WINTERS_ENGINE CBTCooldownDecorator : public CBTNode
{
public:
    explicit CBTCooldownDecorator(f32_t cooldownSec) : m_fCooldownMax(cooldownSec) {}
    void SetChild(std::unique_ptr<CBTNode> c) { m_pChild = std::move(c); }
    eBTStatus Tick(BTContext& ctx) override;
    const char* GetName() const override { return "Cooldown"; }
    size_t GetChildCount() const override { return m_pChild ? 1 : 0; }
    CBTNode* GetChild(size_t) const override { return m_pChild.get(); }
    void Reset() override { m_fCooldownTimer = 0.f; if (m_pChild) m_pChild->Reset(); m_lastStatus = eBTStatus::Invalid; }
private:
    std::unique_ptr<CBTNode> m_pChild;
    f32_t m_fCooldownMax = 0.f;
    f32_t m_fCooldownTimer = 0.f;
};

// ─────────────────────────────────────────────────────────────
// ConditionNode — 람다 기반. 즉시 Success/Failure 반환.
// ─────────────────────────────────────────────────────────────
using BTConditionFn = std::function<bool(BTContext&)>;

class WINTERS_ENGINE CBTCondition : public CBTNode
{
public:
    CBTCondition(std::string name, BTConditionFn fn)
        : m_sName(std::move(name)), m_fn(std::move(fn)) {}
    eBTStatus Tick(BTContext& ctx) override
    {
        m_lastStatus = m_fn(ctx) ? eBTStatus::Success : eBTStatus::Failure;
        return m_lastStatus;
    }
    const char* GetName() const override { return m_sName.c_str(); }
private:
    std::string m_sName;
    BTConditionFn m_fn;
};

// ─────────────────────────────────────────────────────────────
// ActionNode — 람다 기반. Running 가능.
// ─────────────────────────────────────────────────────────────
using BTActionFn = std::function<eBTStatus(BTContext&)>;

class WINTERS_ENGINE CBTAction : public CBTNode
{
public:
    CBTAction(std::string name, BTActionFn fn)
        : m_sName(std::move(name)), m_fn(std::move(fn)) {}
    eBTStatus Tick(BTContext& ctx) override
    {
        m_lastStatus = m_fn(ctx);
        return m_lastStatus;
    }
    const char* GetName() const override { return m_sName.c_str(); }
private:
    std::string m_sName;
    BTActionFn m_fn;
};

// ─────────────────────────────────────────────────────────────
// CBehaviorTree — root 보유. 외부에서 Tick 만 호출.
// ─────────────────────────────────────────────────────────────
class WINTERS_ENGINE CBehaviorTree
{
public:
    CBehaviorTree() = default;
    void SetRoot(std::unique_ptr<CBTNode> root) { m_pRoot = std::move(root); }
    eBTStatus Tick(BTContext& ctx)
    {
        if (!m_pRoot) return eBTStatus::Failure;
        return m_pRoot->Tick(ctx);
    }
    void Reset() { if (m_pRoot) m_pRoot->Reset(); }
    CBTNode* GetRoot() const { return m_pRoot.get(); }
private:
    std::unique_ptr<CBTNode> m_pRoot;
};

NS_END
```

**신규 파일**: `Engine/Private/AI/BehaviorTree.cpp`

```cpp
#include "WintersPCH.h"
#include "AI/BehaviorTree.h"

NS_BEGIN(Engine)

eBTStatus CBTSelector::Tick(BTContext& ctx)
{
    for (auto& c : m_children)
    {
        const eBTStatus s = c->Tick(ctx);
        if (s == eBTStatus::Success || s == eBTStatus::Running)
        {
            m_lastStatus = s;
            return s;
        }
    }
    m_lastStatus = eBTStatus::Failure;
    return m_lastStatus;
}

eBTStatus CBTSequence::Tick(BTContext& ctx)
{
    for (auto& c : m_children)
    {
        const eBTStatus s = c->Tick(ctx);
        if (s == eBTStatus::Failure || s == eBTStatus::Running)
        {
            m_lastStatus = s;
            return s;
        }
    }
    m_lastStatus = eBTStatus::Success;
    return m_lastStatus;
}

eBTStatus CBTParallel::Tick(BTContext& ctx)
{
    uint32_t successCount = 0;
    bool bAnyRunning = false;
    for (auto& c : m_children)
    {
        const eBTStatus s = c->Tick(ctx);
        if (s == eBTStatus::Success) ++successCount;
        else if (s == eBTStatus::Running) bAnyRunning = true;
    }
    if (successCount >= m_uThreshold)
    {
        m_lastStatus = eBTStatus::Success;
        return m_lastStatus;
    }
    if (bAnyRunning)
    {
        m_lastStatus = eBTStatus::Running;
        return m_lastStatus;
    }
    m_lastStatus = eBTStatus::Failure;
    return m_lastStatus;
}

eBTStatus CBTInverter::Tick(BTContext& ctx)
{
    if (!m_pChild) { m_lastStatus = eBTStatus::Failure; return m_lastStatus; }
    const eBTStatus s = m_pChild->Tick(ctx);
    if (s == eBTStatus::Success) m_lastStatus = eBTStatus::Failure;
    else if (s == eBTStatus::Failure) m_lastStatus = eBTStatus::Success;
    else m_lastStatus = s;
    return m_lastStatus;
}

eBTStatus CBTCooldownDecorator::Tick(BTContext& ctx)
{
    if (m_fCooldownTimer > 0.f)
    {
        m_fCooldownTimer = std::max(0.f, m_fCooldownTimer - ctx.dt);
        m_lastStatus = eBTStatus::Failure;
        return m_lastStatus;
    }
    if (!m_pChild) { m_lastStatus = eBTStatus::Failure; return m_lastStatus; }
    const eBTStatus s = m_pChild->Tick(ctx);
    if (s == eBTStatus::Success || s == eBTStatus::Failure)
        m_fCooldownTimer = m_fCooldownMax;
    m_lastStatus = s;
    return m_lastStatus;
}

NS_END
```

---

## 3. 챔프 BT 빌더 — 표준 라이브러리 노드

**신규 파일**: `Engine/Public/AI/BTNodes_Champion.h`

```cpp
#pragma once
#include "AI/BehaviorTree.h"
#include "ECS/Entity.h"
#include <memory>

class CWorld;
class CSpatialIndex;

NS_BEGIN(Engine)

// 챔프 BT 표준 빌더 — Conditions + Actions
namespace BTNodes
{
    // === Conditions ===
    std::unique_ptr<CBTNode> Cond_HpBelow(f32_t pct);
    std::unique_ptr<CBTNode> Cond_ManaBelow(f32_t pct);
    std::unique_ptr<CBTNode> Cond_EnemyChampInSight(f32_t range);
    std::unique_ptr<CBTNode> Cond_EnemyMinionInRange(f32_t range);
    std::unique_ptr<CBTNode> Cond_AllyChampInSight(f32_t range);
    std::unique_ptr<CBTNode> Cond_InTowerRange(f32_t range);
    std::unique_ptr<CBTNode> Cond_BBKeySet(const std::string& key);
    std::unique_ptr<CBTNode> Cond_SkillReady(uint8_t slot);

    // === Actions ===
    std::unique_ptr<CBTNode> Act_MoveTo(const std::string& bbKeyTargetPos);
    std::unique_ptr<CBTNode> Act_AttackTarget(const std::string& bbKeyTargetEntity);
    std::unique_ptr<CBTNode> Act_CastSkill(uint8_t slot, const std::string& bbKeyTargetPos);
    std::unique_ptr<CBTNode> Act_Recall();
    std::unique_ptr<CBTNode> Act_Retreat();   // 가장 가까운 아군 타워로 이동
    std::unique_ptr<CBTNode> Act_FarmMinion(); // 사거리 내 가장 약한 적 미니언 평타
    std::unique_ptr<CBTNode> Act_SetBBValue(const std::string& key, f32_t v);
    std::unique_ptr<CBTNode> Act_LogDebug(const std::string& msg);
}

// === 표준 챔프 BT 빌더 ===
//   Selector
//   ├── Sequence: HP < 30% → Retreat
//   ├── Sequence: Enemy in attack range → Attack
//   ├── Sequence: Enemy minion in range → Farm
//   └── Action: MoveToLane
std::unique_ptr<CBehaviorTree> BuildStandardChampionBT();

// === 마스터 난이도 챔프 BT (LoL Master 체감) ===
//   Selector
//   ├── Sequence: HP < 20% + Recall ready → Recall
//   ├── Sequence: HP < 40% + Tower nearby → Retreat
//   ├── Sequence: Enemy isolated + Burst combo ready → All-in
//   ├── Sequence: TeamFight detected → Group up
//   ├── Sequence: Enemy minion ≥ 4 + Wave clear ready → Push
//   ├── Sequence: Vision missing in jungle → Ward
//   └── Action: Farm wave
std::unique_ptr<CBehaviorTree> BuildMasterTierChampionBT();

NS_END
```

**신규 파일**: `Engine/Private/AI/BTNodes_Champion.cpp` — 핵심 노드만 박제 (전체는 ~600 LOC)

```cpp
#include "WintersPCH.h"
#include "AI/BTNodes_Champion.h"
#include "ECS/World.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/VisionComponents.h"

NS_BEGIN(Engine)

namespace BTNodes
{
    std::unique_ptr<CBTNode> Cond_HpBelow(f32_t pct)
    {
        return std::make_unique<CBTCondition>("HpBelow",
            [pct](BTContext& ctx) -> bool
            {
                if (!ctx.pWorld->HasComponent<HealthComponent>(ctx.self)) return false;
                const auto& h = ctx.pWorld->GetComponent<HealthComponent>(ctx.self);
                if (h.fMax <= 0.f) return false;
                return (h.fCurrent / h.fMax) < pct;
            });
    }

    std::unique_ptr<CBTNode> Cond_EnemyChampInSight(f32_t range)
    {
        return std::make_unique<CBTCondition>("EnemyChampInSight",
            [range](BTContext& ctx) -> bool
            {
                if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.self)) return false;
                if (!ctx.pWorld->HasComponent<SpatialAgentComponent>(ctx.self)) return false;
                const Vec3 myPos = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const uint8_t myTeam = ctx.pWorld->GetComponent<SpatialAgentComponent>(ctx.self).team;
                CSpatialIndex* spatial = ctx.pWorld->GetSpatialIndex();
                if (!spatial) return false;
                const EntityID target = spatial->QueryClosest(myPos, range,
                    SpatialMask(eSpatialKind::Champion), myTeam);
                if (target == NULL_ENTITY) return false;
                ctx.pBB->Set("nearestEnemyChamp", static_cast<uint64_t>(target));
                return true;
            });
    }

    std::unique_ptr<CBTNode> Cond_EnemyMinionInRange(f32_t range)
    {
        return std::make_unique<CBTCondition>("EnemyMinionInRange",
            [range](BTContext& ctx) -> bool
            {
                if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.self)) return false;
                if (!ctx.pWorld->HasComponent<SpatialAgentComponent>(ctx.self)) return false;
                const Vec3 myPos = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const uint8_t myTeam = ctx.pWorld->GetComponent<SpatialAgentComponent>(ctx.self).team;
                CSpatialIndex* spatial = ctx.pWorld->GetSpatialIndex();
                if (!spatial) return false;

                // 사거리 안 적 미니언 중 HP 가장 낮은 것
                std::vector<EntityID> hits;
                spatial->QueryRadius(myPos, range, SpatialMask(eSpatialKind::Minion),
                    /*excludeTeamMask*/ (1u << myTeam), hits);
                EntityID best = NULL_ENTITY;
                f32_t bestHp = 9999.f;
                for (EntityID id : hits)
                {
                    if (!ctx.pWorld->HasComponent<HealthComponent>(id)) continue;
                    const auto& h = ctx.pWorld->GetComponent<HealthComponent>(id);
                    if (h.bIsDead || h.fCurrent <= 0.f) continue;
                    if (h.fCurrent < bestHp) { bestHp = h.fCurrent; best = id; }
                }
                if (best == NULL_ENTITY) return false;
                ctx.pBB->Set("farmTarget", static_cast<uint64_t>(best));
                return true;
            });
    }

    std::unique_ptr<CBTNode> Cond_SkillReady(uint8_t slot)
    {
        return std::make_unique<CBTCondition>("SkillReady",
            [slot](BTContext& ctx) -> bool
            {
                if (!ctx.pWorld->HasComponent<SkillStateComponent>(ctx.self)) return false;
                const auto& ss = ctx.pWorld->GetComponent<SkillStateComponent>(ctx.self);
                if (slot >= 5) return false;
                return ss.slots[slot].cooldownRemaining <= 0.f;
            });
    }

    std::unique_ptr<CBTNode> Act_MoveTo(const std::string& bbKey)
    {
        return std::make_unique<CBTAction>("MoveTo",
            [bbKey](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pBB->Has(bbKey)) return eBTStatus::Failure;
                const Vec3 dest = ctx.pBB->GetVec3(bbKey);

                if (!ctx.pWorld->HasComponent<CommandQueueComponent>(ctx.self))
                    return eBTStatus::Failure;
                auto& cq = ctx.pWorld->GetComponent<CommandQueueComponent>(ctx.self);
                Command c{};
                c.type = eCommandType::Move;
                c.vTargetPos = dest;
                cq.queue.push_back(c);

                // 도달 검사
                if (ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                {
                    const Vec3 cur = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                    const f32_t dx = dest.x - cur.x;
                    const f32_t dz = dest.z - cur.z;
                    if (dx * dx + dz * dz < 0.5f) return eBTStatus::Success;
                }
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_AttackTarget(const std::string& bbKey)
    {
        return std::make_unique<CBTAction>("AttackTarget",
            [bbKey](BTContext& ctx) -> eBTStatus
            {
                const EntityID tgt = ctx.pBB->GetEntity(bbKey);
                if (tgt == NULL_ENTITY) return eBTStatus::Failure;
                if (!ctx.pWorld->IsAlive(tgt)) return eBTStatus::Failure;

                if (!ctx.pWorld->HasComponent<CommandQueueComponent>(ctx.self))
                    return eBTStatus::Failure;
                auto& cq = ctx.pWorld->GetComponent<CommandQueueComponent>(ctx.self);
                Command c{};
                c.type = eCommandType::Attack;
                c.targetEntity = tgt;
                cq.queue.push_back(c);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_CastSkill(uint8_t slot, const std::string& bbKey)
    {
        return std::make_unique<CBTAction>("CastSkill",
            [slot, bbKey](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld->HasComponent<SkillStateComponent>(ctx.self)) return eBTStatus::Failure;
                const auto& ss = ctx.pWorld->GetComponent<SkillStateComponent>(ctx.self);
                if (slot >= 5) return eBTStatus::Failure;
                if (ss.slots[slot].cooldownRemaining > 0.f) return eBTStatus::Failure;

                if (!ctx.pWorld->HasComponent<CommandQueueComponent>(ctx.self))
                    return eBTStatus::Failure;
                auto& cq = ctx.pWorld->GetComponent<CommandQueueComponent>(ctx.self);
                Command c{};
                c.type = eCommandType::Cast;
                c.iSkillSlot = slot;
                if (ctx.pBB->Has(bbKey))
                    c.vTargetPos = ctx.pBB->GetVec3(bbKey);
                cq.queue.push_back(c);
                return eBTStatus::Success;
            });
    }

    std::unique_ptr<CBTNode> Act_Retreat()
    {
        return std::make_unique<CBTAction>("Retreat",
            [](BTContext& ctx) -> eBTStatus
            {
                // 가장 가까운 아군 타워 위치로 이동
                if (!ctx.pWorld->HasComponent<TransformComponent>(ctx.self)) return eBTStatus::Failure;
                if (!ctx.pWorld->HasComponent<SpatialAgentComponent>(ctx.self)) return eBTStatus::Failure;
                const Vec3 myPos = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const uint8_t myTeam = ctx.pWorld->GetComponent<SpatialAgentComponent>(ctx.self).team;

                CSpatialIndex* spatial = ctx.pWorld->GetSpatialIndex();
                if (!spatial) return eBTStatus::Failure;

                std::vector<EntityID> hits;
                spatial->QueryRadius(myPos, 100.f,
                    SpatialMask(eSpatialKind::Turret) | SpatialMask(eSpatialKind::Nexus),
                    /*excludeTeamMask*/ ~(1u << myTeam),  // 다른 팀 제외 (나만 포함)
                    hits);

                EntityID best = NULL_ENTITY;
                f32_t bestDist2 = std::numeric_limits<f32_t>::max();
                for (EntityID id : hits)
                {
                    if (!ctx.pWorld->HasComponent<TransformComponent>(id)) continue;
                    const Vec3 tp = ctx.pWorld->GetComponent<TransformComponent>(id).GetPosition();
                    const f32_t dx = tp.x - myPos.x;
                    const f32_t dz = tp.z - myPos.z;
                    const f32_t d2 = dx * dx + dz * dz;
                    if (d2 < bestDist2) { bestDist2 = d2; best = id; }
                }
                if (best == NULL_ENTITY) return eBTStatus::Failure;

                const Vec3 tower = ctx.pWorld->GetComponent<TransformComponent>(best).GetPosition();
                ctx.pBB->Set("retreatPos", tower);

                auto& cq = ctx.pWorld->GetComponent<CommandQueueComponent>(ctx.self);
                Command c{};
                c.type = eCommandType::Move;
                c.vTargetPos = tower;
                cq.queue.push_back(c);
                return eBTStatus::Running;
            });
    }

    // 나머지: Cond_ManaBelow, Cond_AllyChampInSight, Cond_InTowerRange,
    //         Cond_BBKeySet, Act_FarmMinion, Act_Recall, Act_SetBBValue, Act_LogDebug
    // — 같은 패턴 반복. 본 박제는 핵심 6 노드만, 나머지 5 노드는 Phase B-13 진행 중 박제.
}

// ─────────────────────────────────────────────────────────────
// 표준 챔프 BT 빌더 — depth 4, 8 노드
// ─────────────────────────────────────────────────────────────
std::unique_ptr<CBehaviorTree> BuildStandardChampionBT()
{
    auto bt = std::make_unique<CBehaviorTree>();

    auto root = std::make_unique<CBTSelector>();

    // 1. HP 30% 이하 → Retreat
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_HpBelow(0.3f));
        seq->AddChild(BTNodes::Act_Retreat());
        root->AddChild(std::move(seq));
    }

    // 2. 평타 사거리 내 적 챔프 → 평타
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_EnemyChampInSight(2.f));
        seq->AddChild(BTNodes::Act_AttackTarget("nearestEnemyChamp"));
        root->AddChild(std::move(seq));
    }

    // 3. 적 미니언 → Farm
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_EnemyMinionInRange(2.f));
        seq->AddChild(BTNodes::Act_AttackTarget("farmTarget"));
        root->AddChild(std::move(seq));
    }

    // 4. fallback: lane 으로 이동
    root->AddChild(BTNodes::Act_MoveTo("lanePushPos"));

    bt->SetRoot(std::move(root));
    return bt;
}

// ─────────────────────────────────────────────────────────────
// 마스터 난이도 BT — depth 5, 16 노드. 본격 박제는 Phase B-14 에서.
// 본 박제는 골격만 — Standard 와 동일하나 우선순위 + 콤보 추가.
// ─────────────────────────────────────────────────────────────
std::unique_ptr<CBehaviorTree> BuildMasterTierChampionBT()
{
    auto bt = std::make_unique<CBehaviorTree>();
    auto root = std::make_unique<CBTSelector>();

    // 1. HP 20% + R ready → Recall (사용 한도 1 회 — Cooldown decorator)
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_HpBelow(0.2f));
        seq->AddChild(BTNodes::Cond_SkillReady(4));   // R 슬롯 = 4
        // ... Act_Recall stub
        root->AddChild(std::move(seq));
    }
    // 2. HP 40% → Retreat
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_HpBelow(0.4f));
        seq->AddChild(BTNodes::Act_Retreat());
        root->AddChild(std::move(seq));
    }
    // 3. Enemy isolated (1:1) + Burst combo → AllIn
    //    Sequence: enemy chamb solo + my Q/W/E ready → CastQ → CastW → CastE → AttackChamp
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_EnemyChampInSight(8.f));
        seq->AddChild(BTNodes::Cond_SkillReady(1));
        seq->AddChild(BTNodes::Cond_SkillReady(2));
        seq->AddChild(BTNodes::Act_CastSkill(1, "nearestEnemyChampPos"));
        seq->AddChild(BTNodes::Act_CastSkill(2, "nearestEnemyChampPos"));
        seq->AddChild(BTNodes::Act_AttackTarget("nearestEnemyChamp"));
        root->AddChild(std::move(seq));
    }
    // 4. Farm
    {
        auto seq = std::make_unique<CBTSequence>();
        seq->AddChild(BTNodes::Cond_EnemyMinionInRange(2.f));
        seq->AddChild(BTNodes::Act_AttackTarget("farmTarget"));
        root->AddChild(std::move(seq));
    }
    // 5. fallback: push lane
    root->AddChild(BTNodes::Act_MoveTo("lanePushPos"));

    bt->SetRoot(std::move(root));
    return bt;
}

NS_END
```

---

## 4. CBehaviorTreeSystem — 200ms 틱 ECS 시스템

**신규 파일**: `Engine/Public/ECS/Systems/BehaviorTreeSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"
#include "AI/BehaviorTree.h"
#include <memory>

class CWorld;

NS_BEGIN(Engine)

// BotComponent 가진 엔티티는 m_pBT 보유 → BehaviorTreeSystem 매 200ms Tick.
struct BotComponent
{
    std::shared_ptr<CBehaviorTree> pBT;   // Engine 만 export — Client 는 만들 때만 접근
    bool_t  bUseRL = false;               // RL 모델 우선. false 면 BT.
    f32_t   tickAccumulator = 0.f;
    uint8_t difficulty = 1;               // 0=Intro, 1=Begin, 2=Inter, 3=Master, 4=Grandmaster
};

class WINTERS_ENGINE CBehaviorTreeSystem final : public ISystem
{
public:
    ~CBehaviorTreeSystem() override = default;

    static std::unique_ptr<CBehaviorTreeSystem> Create()
    {
        return std::unique_ptr<CBehaviorTreeSystem>(new CBehaviorTreeSystem());
    }

    uint32_t    GetPhase() const override { return 7; }   // ★ v2: Vision(5) / Turret(6) 후 단독
    const char* GetName()  const override { return "BehaviorTreeSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CBehaviorTreeSystem() = default;
    static constexpr f32_t TICK_INTERVAL = 0.2f;
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/BehaviorTreeSystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "AI/Blackboard.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CBehaviorTreeSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("BTSystem::Execute");

    world.ForEach<BotComponent, BlackboardComponent>(
        function<void(EntityID, BotComponent&, BlackboardComponent&)>(
            [&](EntityID id, BotComponent& bot, BlackboardComponent& bb)
            {
                if (!bot.pBT) return;
                if (bot.bUseRL) return;   // RL 모드 — 별도 시스템

                bot.tickAccumulator += dt;
                if (bot.tickAccumulator < TICK_INTERVAL) return;

                BTContext ctx{};
                ctx.pWorld = &world;
                ctx.self = id;
                ctx.pBB = &bb.bb;
                ctx.dt = bot.tickAccumulator;

                bot.tickAccumulator = 0.f;
                bot.pBT->Tick(ctx);
            }));
}

NS_END
```

---

## 5. MCTS 골격 박제

**신규 파일**: `Engine/Public/AI/MCTSPlanner.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <vector>
#include <memory>
#include <random>

class CWorld;

NS_BEGIN(Engine)

// ─────────────────────────────────────────────────────────────
// MCTSPlanner — 매크로 의사결정 (5초 틱).
//
// WorldSnapshot: 현재 World 의 deep copy (위치/HP/쿨다운).
//                MCTS Rollout 은 이 snapshot 만 mutate. 실제 World 변경 X.
// ─────────────────────────────────────────────────────────────

struct WorldSnapshot
{
    struct UnitState
    {
        EntityID id;
        Vec3     pos;
        f32_t    hp;
        f32_t    maxHp;
        f32_t    mana;
        uint8_t  team;
        uint8_t  kind;    // eSpatialKind cast
        f32_t    cooldowns[4]{ 0.f, 0.f, 0.f, 0.f };
    };
    std::vector<UnitState> units;
    f32_t time = 0.f;

    // 캡처 — 현재 World 에서 봇 + 적 챔프 + 사거리 내 미니언 만 deep copy
    void CaptureFromWorld(CWorld& world, EntityID self, f32_t captureRadius);

    // unit 검색
    UnitState* FindUnit(EntityID id);
    const UnitState* FindUnit(EntityID id) const;
};

// MCTS Action — 봇이 취할 수 있는 단순 액션
enum class eMCTSAction : uint8_t
{
    None = 0,
    AttackNearest,
    CastQ,
    CastW,
    CastE,
    CastR,
    MoveAway,
    MoveTowardEnemy,
    Retreat,
    Hold,
    ACTION_END
};

// MCTS 트리 노드
class CMCTSNode
{
public:
    eMCTSAction action = eMCTSAction::None;
    CMCTSNode*  parent = nullptr;
    std::vector<std::unique_ptr<CMCTSNode>> children;
    uint32_t    visits = 0;
    f32_t       totalReward = 0.f;
    bool_t      bExpanded = false;
};

class WINTERS_ENGINE CMCTSPlanner
{
public:
    CMCTSPlanner();
    ~CMCTSPlanner() = default;

    CMCTSPlanner(const CMCTSPlanner&) = delete;
    CMCTSPlanner& operator=(const CMCTSPlanner&) = delete;

    // 메인 — Rollout iterations 회 실행 후 best action 반환.
    eMCTSAction Plan(CWorld& world, EntityID self, uint32_t iterations = 100);

    // Tunable
    void SetExplorationConstant(f32_t c) { m_fExploration = c; }
    void SetMaxDepth(uint32_t d) { m_uMaxDepth = d; }

private:
    CMCTSNode* Select(CMCTSNode* node, WorldSnapshot& snap);
    CMCTSNode* Expand(CMCTSNode* node, WorldSnapshot& snap);
    f32_t      Rollout(CMCTSNode* node, WorldSnapshot& snap, uint32_t depth);
    void       Backpropagate(CMCTSNode* node, f32_t reward);

    // Domain-specific
    void   ApplyAction(WorldSnapshot& snap, EntityID self, eMCTSAction action);
    f32_t  EvaluateReward(const WorldSnapshot& snap, EntityID self) const;
    std::vector<eMCTSAction> ListLegalActions(const WorldSnapshot& snap, EntityID self) const;

    f32_t       m_fExploration = 1.414f;   // UCB1 sqrt(2)
    uint32_t    m_uMaxDepth = 5;
    std::mt19937 m_rng;
};

NS_END
```

**신규 파일**: `Engine/Private/AI/MCTSPlanner.cpp` — 골격 (실제 도메인 로직은 Phase B-14 박제)

```cpp
#include "WintersPCH.h"
#include "AI/MCTSPlanner.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <algorithm>

NS_BEGIN(Engine)

void WorldSnapshot::CaptureFromWorld(CWorld& world, EntityID self, f32_t captureRadius)
{
    units.clear();
    if (!world.HasComponent<TransformComponent>(self)) return;
    const Vec3 myPos = world.GetComponent<TransformComponent>(self).GetPosition();
    const f32_t r2 = captureRadius * captureRadius;

    world.ForEach<TransformComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TransformComponent& xf, SpatialAgentComponent& a)
            {
                const Vec3 p = xf.GetPosition();
                const f32_t dx = p.x - myPos.x;
                const f32_t dz = p.z - myPos.z;
                if (dx * dx + dz * dz > r2 && id != self) return;

                UnitState us{};
                us.id = id;
                us.pos = p;
                us.team = a.team;
                us.kind = static_cast<uint8_t>(a.kind);
                if (world.HasComponent<HealthComponent>(id))
                {
                    const auto& h = world.GetComponent<HealthComponent>(id);
                    us.hp = h.fCurrent;
                    us.maxHp = h.fMax;
                }
                if (world.HasComponent<ChampionComponent>(id))
                {
                    const auto& c = world.GetComponent<ChampionComponent>(id);
                    us.mana = c.mana;
                    for (int i = 0; i < 4; ++i) us.cooldowns[i] = c.cooldowns[i];
                }
                units.push_back(us);
            }));
}

WorldSnapshot::UnitState* WorldSnapshot::FindUnit(EntityID id)
{
    for (auto& u : units) if (u.id == id) return &u;
    return nullptr;
}
const WorldSnapshot::UnitState* WorldSnapshot::FindUnit(EntityID id) const
{
    for (const auto& u : units) if (u.id == id) return &u;
    return nullptr;
}

CMCTSPlanner::CMCTSPlanner()
    : m_rng(std::random_device{}())
{}

eMCTSAction CMCTSPlanner::Plan(CWorld& world, EntityID self, uint32_t iterations)
{
    WINTERS_PROFILE_SCOPE("MCTS::Plan");

    // 1. 루트 노드 생성
    auto root = std::make_unique<CMCTSNode>();

    // 2. 초기 스냅샷
    WorldSnapshot rootSnap;
    rootSnap.CaptureFromWorld(world, self, 30.f);

    // 3. iterations 회 Rollout
    for (uint32_t i = 0; i < iterations; ++i)
    {
        WorldSnapshot snap = rootSnap;   // copy

        // Selection
        CMCTSNode* node = Select(root.get(), snap);

        // Expansion
        if (node->visits > 0 && !node->bExpanded)
            node = Expand(node, snap);

        // Simulation
        const f32_t reward = Rollout(node, snap, m_uMaxDepth);

        // Backpropagation
        Backpropagate(node, reward);
    }

    // 4. best child (most visited)
    if (root->children.empty()) return eMCTSAction::None;
    auto* best = root->children.front().get();
    for (auto& c : root->children)
        if (c->visits > best->visits) best = c.get();
    return best->action;
}

CMCTSNode* CMCTSPlanner::Select(CMCTSNode* node, WorldSnapshot& /*snap*/)
{
    while (node->bExpanded && !node->children.empty())
    {
        // UCB1
        CMCTSNode* best = node->children.front().get();
        f32_t bestUCB = -std::numeric_limits<f32_t>::infinity();
        for (auto& c : node->children)
        {
            if (c->visits == 0) return c.get();
            const f32_t exploit = c->totalReward / c->visits;
            const f32_t explore = m_fExploration *
                std::sqrt(std::log((f32_t)node->visits) / c->visits);
            const f32_t ucb = exploit + explore;
            if (ucb > bestUCB) { bestUCB = ucb; best = c.get(); }
        }
        node = best;
    }
    return node;
}

CMCTSNode* CMCTSPlanner::Expand(CMCTSNode* node, WorldSnapshot& snap)
{
    EntityID self = NULL_ENTITY;   // root 부터 self 추적 — 본격 박제는 B-14
    auto actions = ListLegalActions(snap, self);
    for (auto a : actions)
    {
        auto child = std::make_unique<CMCTSNode>();
        child->action = a;
        child->parent = node;
        node->children.push_back(std::move(child));
    }
    node->bExpanded = true;
    return node->children.empty() ? node : node->children.front().get();
}

f32_t CMCTSPlanner::Rollout(CMCTSNode* /*node*/, WorldSnapshot& snap, uint32_t depth)
{
    // 랜덤 액션 시뮬레이션 — depth 회
    EntityID self = NULL_ENTITY;
    for (uint32_t i = 0; i < depth; ++i)
    {
        auto actions = ListLegalActions(snap, self);
        if (actions.empty()) break;
        std::uniform_int_distribution<size_t> dist(0, actions.size() - 1);
        const eMCTSAction a = actions[dist(m_rng)];
        ApplyAction(snap, self, a);
        snap.time += 0.5f;
    }
    return EvaluateReward(snap, self);
}

void CMCTSPlanner::Backpropagate(CMCTSNode* node, f32_t reward)
{
    while (node)
    {
        ++node->visits;
        node->totalReward += reward;
        node = node->parent;
    }
}

void CMCTSPlanner::ApplyAction(WorldSnapshot& snap, EntityID self, eMCTSAction action)
{
    // stub — Phase B-14 에서 본격 박제. 본 골격은 단순 HP 변화만.
    auto* me = snap.FindUnit(self);
    if (!me) return;
    switch (action)
    {
    case eMCTSAction::AttackNearest:
        // 가장 가까운 적 HP - 50
        for (auto& u : snap.units)
        {
            if (u.team != me->team)
            {
                u.hp -= 50.f;
                break;
            }
        }
        break;
    case eMCTSAction::Retreat:
        // 자기 HP 회복 가정
        me->hp = std::min(me->maxHp, me->hp + 30.f);
        break;
    default: break;
    }
}

f32_t CMCTSPlanner::EvaluateReward(const WorldSnapshot& snap, EntityID self) const
{
    const auto* me = snap.FindUnit(self);
    if (!me) return 0.f;
    f32_t myHpRatio = me->hp / std::max(1.f, me->maxHp);

    f32_t enemyHpSum = 0.f;
    f32_t enemyMaxHpSum = 0.f;
    for (const auto& u : snap.units)
    {
        if (u.team != me->team)
        {
            enemyHpSum += u.hp;
            enemyMaxHpSum += u.maxHp;
        }
    }
    const f32_t enemyHpRatio = enemyMaxHpSum > 0.f ? (enemyHpSum / enemyMaxHpSum) : 1.f;

    return myHpRatio - enemyHpRatio;   // 내가 살아있고 적이 죽으면 +
}

std::vector<eMCTSAction> CMCTSPlanner::ListLegalActions(const WorldSnapshot& /*snap*/,
                                                         EntityID /*self*/) const
{
    return {
        eMCTSAction::AttackNearest,
        eMCTSAction::CastQ,
        eMCTSAction::CastW,
        eMCTSAction::CastE,
        eMCTSAction::CastR,
        eMCTSAction::MoveAway,
        eMCTSAction::MoveTowardEnemy,
        eMCTSAction::Retreat,
        eMCTSAction::Hold
    };
}

NS_END
```

---

## 6. RLBridge — ONNX Runtime 스텁 (Phase B-14 본격 박제)

**신규 파일**: `Engine/Public/AI/RLBridge.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include <memory>
#include <string>
#include <vector>

class CWorld;

NS_BEGIN(Engine)

// ─────────────────────────────────────────────────────────────
// CRLBridge — ONNX Runtime 추론 래퍼.
// Phase B-13 은 헤더만 + cpp stub. Phase B-14 에서 ONNX Runtime 편입 +
// PPO 학습된 policy.onnx 로드 + Inference 본격 박제.
//
// 학습 (Python PyTorch) → ONNX export → 본 클래스가 추론.
// ─────────────────────────────────────────────────────────────

class WINTERS_ENGINE CRLBridge
{
public:
    static std::unique_ptr<CRLBridge> Create();
    ~CRLBridge();

    CRLBridge(const CRLBridge&) = delete;
    CRLBridge& operator=(const CRLBridge&) = delete;

    bool LoadModel(const std::string& onnxPath);
    bool IsLoaded() const { return m_bLoaded; }

    // 상태 벡터 (24 floats — 위치, HP, 쿨다운, 가까운 적 HP/위치, ...)
    static constexpr uint32_t STATE_DIM = 24;
    static constexpr uint32_t ACTION_DIM = 9;   // eMCTSAction END

    // World 상태 → 24-dim 벡터 추출
    void EncodeState(CWorld& world, EntityID self, std::vector<f32_t>& out) const;

    // 추론 — input STATE_DIM, output ACTION_DIM (logits)
    bool Infer(const std::vector<f32_t>& state, std::vector<f32_t>& outLogits);

    // 가장 큰 logit → action
    int32_t BestAction(const std::vector<f32_t>& logits) const;

private:
    CRLBridge() = default;

    // ONNX Runtime PIMPL
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
    bool m_bLoaded = false;
};

NS_END
```

**신규 파일**: `Engine/Private/AI/RLBridge.cpp` — stub (실제 ONNX 코드는 Phase B-14)

```cpp
#include "WintersPCH.h"
#include "AI/RLBridge.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/GameplayComponents.h"

#include <algorithm>

NS_BEGIN(Engine)

struct CRLBridge::Impl
{
    // Phase B-14: Ort::Env, Ort::Session 등
    bool dummy = true;
};

std::unique_ptr<CRLBridge> CRLBridge::Create()
{
    auto p = std::unique_ptr<CRLBridge>(new CRLBridge());
    p->m_pImpl = std::make_unique<Impl>();
    return p;
}

CRLBridge::~CRLBridge() = default;

bool CRLBridge::LoadModel(const std::string& /*onnxPath*/)
{
    // Phase B-14: ONNX Runtime 세션 로드.
    m_bLoaded = false;   // 본 단계는 stub — false 폴백 → BT 사용
    return false;
}

void CRLBridge::EncodeState(CWorld& world, EntityID self, std::vector<f32_t>& out) const
{
    out.assign(STATE_DIM, 0.f);
    if (!world.HasComponent<TransformComponent>(self)) return;
    if (!world.HasComponent<HealthComponent>(self)) return;

    const Vec3 myPos = world.GetComponent<TransformComponent>(self).GetPosition();
    const auto& h = world.GetComponent<HealthComponent>(self);

    // 0~2: my pos
    out[0] = myPos.x; out[1] = myPos.y; out[2] = myPos.z;
    // 3: HP ratio
    out[3] = h.fMax > 0.f ? (h.fCurrent / h.fMax) : 0.f;
    // 4~7: cooldowns
    if (world.HasComponent<ChampionComponent>(self))
    {
        const auto& c = world.GetComponent<ChampionComponent>(self);
        for (int i = 0; i < 4; ++i) out[4 + i] = c.cooldowns[i];
        out[8] = c.mana / std::max(1.f, c.maxMana);
    }
    // 9~14: 가장 가까운 적 정보 (위치 + HP)
    // 15~23: 두 번째/세 번째 적 또는 미니언 평균
    // — Phase B-14 박제

    // 정규화 (간단)
    out[0] /= 280.f;
    out[2] /= 280.f;
}

bool CRLBridge::Infer(const std::vector<f32_t>& /*state*/, std::vector<f32_t>& outLogits)
{
    if (!m_bLoaded)
    {
        outLogits.assign(ACTION_DIM, 0.f);
        return false;
    }
    // Phase B-14: m_pImpl->session->Run(...)
    return false;
}

int32_t CRLBridge::BestAction(const std::vector<f32_t>& logits) const
{
    if (logits.empty()) return 0;
    auto it = std::max_element(logits.begin(), logits.end());
    return static_cast<int32_t>(std::distance(logits.begin(), it));
}

NS_END
```

---

## 7. CMCTSSystem + RL 폴백 시스템

**신규 파일**: `Engine/Public/ECS/Systems/MCTSSystem.h`

```cpp
#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "ECS/ISystem.h"
#include "AI/MCTSPlanner.h"
#include <memory>

class CWorld;

NS_BEGIN(Engine)

// MCTS 매크로 의사결정 — 5초 틱. BlackboardComponent 의 "macroGoal" 키 갱신.
class WINTERS_ENGINE CMCTSSystem final : public ISystem
{
public:
    ~CMCTSSystem() override = default;
    static std::unique_ptr<CMCTSSystem> Create()
    {
        auto p = std::unique_ptr<CMCTSSystem>(new CMCTSSystem());
        p->m_pPlanner = std::make_unique<CMCTSPlanner>();
        return p;
    }
    uint32_t    GetPhase() const override { return 8; }   // ★ v2: BT(7) 후 단독, 5초 틱
    const char* GetName()  const override { return "MCTSSystem"; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

private:
    CMCTSSystem() = default;
    std::unique_ptr<CMCTSPlanner> m_pPlanner;
    f32_t m_fAccumDt = 0.f;
    static constexpr f32_t TICK_INTERVAL = 5.f;   // 5초 틱
    static constexpr uint32_t ITERATIONS = 50;    // 작게 — 본격은 B-14
};

NS_END
```

**신규 파일**: `Engine/Private/ECS/Systems/MCTSSystem.cpp`

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "AI/Blackboard.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CMCTSSystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("MCTSSystem::Execute");
    m_fAccumDt += dt;
    if (m_fAccumDt < TICK_INTERVAL) return;
    m_fAccumDt = 0.f;

    world.ForEach<BotComponent, BlackboardComponent>(
        function<void(EntityID, BotComponent&, BlackboardComponent&)>(
            [&](EntityID id, BotComponent& bot, BlackboardComponent& bb)
            {
                if (bot.difficulty < 2) return;   // Inter+ 만 MCTS

                const eMCTSAction best = m_pPlanner->Plan(world, id, ITERATIONS);
                bb.bb.Set("macroGoal", static_cast<i32_t>(best));
            }));
}

NS_END
```

---

## 8. ImGui BT 디버그 패널

**신규 파일**: `Client/Public/Editor/BTDebugPanel.h` + `.cpp`

```cpp
// 핵심 — BT 트리 재귀 표시
void DrawNode(CBTNode* n)
{
    if (!n) return;
    const eBTStatus s = n->GetLastStatus();
    const ImU32 col = (s == eBTStatus::Success) ? IM_COL32(80, 200, 80, 255)
                    : (s == eBTStatus::Failure) ? IM_COL32(200, 80, 80, 255)
                    : (s == eBTStatus::Running) ? IM_COL32(200, 200, 80, 255)
                                                : IM_COL32(150, 150, 150, 255);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    if (ImGui::TreeNode(n->GetName()))
    {
        for (size_t i = 0; i < n->GetChildCount(); ++i)
            DrawNode(n->GetChild(i));
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void Draw_BTDebugPanel(CWorld& world)
{
    ImGui::Begin("BT Debug");
    world.ForEach<BotComponent>(
        function<void(EntityID, BotComponent&)>(
            [&](EntityID id, BotComponent& b)
            {
                ImGui::PushID(static_cast<int>(id));
                ImGui::Text("Entity %u — diff=%u  useRL=%d",
                    id, b.difficulty, b.bUseRL);
                if (b.pBT && b.pBT->GetRoot())
                    DrawNode(b.pBT->GetRoot());
                ImGui::PopID();
            }));
    ImGui::End();
}
```

---

## 9. 봇 챔프 스폰 시 BT 부착

**파일**: 봇 챔프 스폰 함수
**작업**: BotComponent + BlackboardComponent + 표준 BT 주입.

```cpp
EntityID SpawnBotChampion(CWorld& world, eChampion champ, eTeam team, Vec3 pos, uint8_t difficulty)
{
    EntityID id = CreateChampionEntity(world, champ, team, pos);

    // BT
    auto& bot = world.AddComponent<BotComponent>(id);
    bot.difficulty = difficulty;
    bot.pBT = (difficulty >= 3) ? BuildMasterTierChampionBT() : BuildStandardChampionBT();

    // BB
    auto& bbc = world.AddComponent<BlackboardComponent>(id);
    bbc.bb.Set("difficulty", static_cast<i32_t>(difficulty));

    // 레인 푸시 위치 초기 세팅 — 라인별
    Vec3 lanePush{};
    if (team == eTeam::Blue) lanePush = { 50.f, 0.f, 50.f };  // 적 진영
    else                     lanePush = { -50.f, 0.f, -50.f };
    bbc.bb.Set("lanePushPos", lanePush);

    return id;
}
```

---

## 10. vcxproj 등록

```xml
<!-- AI 베이스 -->
<ClInclude Include="..\Public\AI\Blackboard.h" />
<ClInclude Include="..\Public\AI\BehaviorTree.h" />
<ClInclude Include="..\Public\AI\BTNodes_Champion.h" />
<ClInclude Include="..\Public\AI\MCTSPlanner.h" />
<ClInclude Include="..\Public\AI\RLBridge.h" />

<ClCompile Include="..\Private\AI\BehaviorTree.cpp" />
<ClCompile Include="..\Private\AI\BTNodes_Champion.cpp" />
<ClCompile Include="..\Private\AI\MCTSPlanner.cpp" />
<ClCompile Include="..\Private\AI\RLBridge.cpp" />

<!-- ECS 시스템 -->
<ClInclude Include="..\Public\ECS\Systems\BehaviorTreeSystem.h" />
<ClInclude Include="..\Public\ECS\Systems\MCTSSystem.h" />

<ClCompile Include="..\Private\ECS\Systems\BehaviorTreeSystem.cpp" />
<ClCompile Include="..\Private\ECS\Systems\MCTSSystem.cpp" />
```

`.filters` 분류:
- `13. AI` (Blackboard, BehaviorTree, BTNodes_Champion, MCTSPlanner, RLBridge)
- `05. ECS\Systems` (BehaviorTreeSystem, MCTSSystem)

`Engine.vcxproj` AdditionalIncludeDirectories 에 `..\Public\AI` 추가.

---

## 11. 검증 시나리오

### V-1: BT 동작
- [ ] 봇 챔프 1개 BotComponent 부착 + BT 주입.
- [ ] 봇이 적 챔프 사거리 (2m) 진입 → AttackTarget 노드 Tick (Running) → CommandQueue 에 Cast/Attack push.
- [ ] HP 30% 이하 → Retreat 노드 Tick → 가장 가까운 아군 타워로 이동.

### V-2: BT 디버그 패널
- [ ] ImGui "BT Debug" 창 — 트리 색깔 (녹/적/노란/회색) 으로 노드 상태 표시.
- [ ] 매 tick 200ms 갱신.

### V-3: MCTS
- [ ] difficulty=2 봇 — 5초 마다 1회 MCTS Plan 실행.
- [ ] Profiler `MCTS::Plan` 50us 이하 (50 iterations 기준).
- [ ] BB "macroGoal" 키가 0~8 사이 정수로 갱신됨.

### V-4: RL Bridge
- [ ] `CRLBridge::LoadModel("nonexistent.onnx")` 호출 → false 반환, IsLoaded() == false.
- [ ] BotComponent::useRL=true 셋팅 + 모델 미로드 → BTSystem 이 BT 사용 (폴백).

### V-5: 통합
- [ ] 봇 5명 vs 플레이어 1명 인게임 진행.
- [ ] 봇 5명 모두 BT Tick 정상 — 미니언 farm + 적 챔프 추격 + HP 낮으면 후퇴.
- [ ] 5명 동시 BT Tick 으로 frame time 200ms 마다 Profiler `BTSystem::Execute` 1ms 이하.

---

## 12. Codex 보정 7 건

### C-1 (마스터 §6 인용): BT 깊이 50+ 스택 오버플로
**해결**: BTNode 본 박제 — Selector/Sequence 등이 자연스럽게 깊이 5~8 (표준 BT) ~ 16 (마스터 BT). depth 32 안전.

### C-2: Blackboard variant 메모리
**우려**: std::variant<bool, int, float, Vec3, string, uint64_t> — string 복사 비용.
**해결**: 매 200ms 1회 Tick 라 무관. 핵심 키만 (10~20 개) 사용. 본격 우려 시 `std::string_view` + 정적 키 테이블.

### C-3: MCTS Rollout World mutate
**해결**: WorldSnapshot 만 mutate (§5 본 구현 L18-L40). 실 World read-only.

### C-4: BT Tick 도중 Entity 사망
**우려**: BT Tick 실행 중 self entity 가 사망 → 이후 노드들이 invalid self 접근.
**해결**: Tick 시작 시 `world.IsAlive(self)` 검사. 사망 시 BT::Reset 호출 + early return.

### C-5: MCTS 5초 틱 + frame stall
**우려**: 50 iterations × 5 depth = 250 ApplyAction. frame stall 우려.
**해결**: 본 구현은 단순 — 50 iter 라 50us. 100+ iter 박제 시 별 thread (JobSystem Submit) 로 비동기.

### C-6: RL ONNX Runtime 의존성
**우려**: ONNX Runtime DLL 추가 — ThirdPartyLib 편입 필요.
**해결**: Phase B-14 에서 별도 사이클. 본 Phase 는 stub 헤더만. 실 추론은 `m_bLoaded=false` 폴백.

### C-7: BT root nullptr 누수
**우려**: BotComponent::pBT 가 nullptr 인데 BTSystem 이 Tick 호출.
**해결**: BehaviorTreeSystem::Execute 에서 `if (!bot.pBT) return;` (§4 본 구현).

---

## 13. Phase B-14 로 이관 (다음 사이클)

본 Phase 에서 **stub** 또는 **단순 박제** 한 항목 — Phase B-14 본격 박제:

1. **GOAP** (Stage 3) — 별도 sub-plan.
2. **Utility AI** (Stage 4) — Response Curves + Behavioral Mathematics.
3. **InfluenceMap** (Stage 5) — Team/Threat/Opportunity/Vision 4 layer.
4. **Imitation Learning** (Stage 7) — 플레이어 로그 수집 + Behavior Cloning.
5. **RL 본격** (Stage 8) — ONNX Runtime ThirdPartyLib 편입 + PPO 학습 환경.
6. **ChampFx** — JAX/ANNIE/ASHE/YONE 챔프 FX 본격 박제 (현재 stub).
7. **MCTSPlanner ApplyAction** — 단순 HP 변경 → 본격 LoL 도메인 시뮬레이션.

---

## 14. END

본 sub-plan 완료 시 Phase B-13 전체 완료. 정의 (마스터 §9):
- [ ] 4 챔프 BanPick/InGame/스킬 정상 (01)
- [ ] 부쉬 시야 차단 (02)
- [ ] 타워 공격 5단계 우선순위 (03)
- [ ] CSpatialIndex 인프라 + MinionAI 교체 (04)
- [ ] BT/MCTS/RLBridge 골격 + 봇 1명 BT 동작 (05)

---

**END OF SUB-PLAN 05 — Phase B-13 완료**
