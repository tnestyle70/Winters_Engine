#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <memory>
#include <random>
#include <vector>

class CWorld;

NS_BEGIN(Engine)

enum class eMCTSAction : u8_t
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
    END
};

struct WorldSnapshot
{
    struct UnitState
    {
        EntityID id = NULL_ENTITY;
        u8_t team = 0;
        Vec3 pos{};
        f32_t hp = 0.f;
        f32_t maxHp = 1.f;
        f32_t mana = 0.f;
        f32_t cooldowns[4]{};
    };

    std::vector<UnitState> units;
    f32_t time = 0.f;

    void CaptureFromWorld(CWorld& world, EntityID self, f32_t radius);
    UnitState* FindUnit(EntityID id);
    const UnitState* FindUnit(EntityID id) const;
};

struct CMCTSNode
{
    eMCTSAction action = eMCTSAction::None;
    CMCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<CMCTSNode>> children;
    u32_t visits = 0;
    f32_t totalReward = 0.f;
    bool_t bExpanded = false;
};

class WINTERS_ENGINE CMCTSPlanner
{
public:
    CMCTSPlanner();
    ~CMCTSPlanner() = default;

    CMCTSPlanner(const CMCTSPlanner&) = delete;
    CMCTSPlanner& operator=(const CMCTSPlanner&) = delete;

    eMCTSAction Plan(CWorld& world, EntityID self, u32_t iterations = 100);

private:
    CMCTSNode* Select(CMCTSNode* node);
    CMCTSNode* Expand(CMCTSNode* node, WorldSnapshot& snap, EntityID self);
    f32_t Rollout(WorldSnapshot& snap, EntityID self, u32_t depth);
    void Backpropagate(CMCTSNode* node, f32_t reward);
    void ApplyAction(WorldSnapshot& snap, EntityID self, eMCTSAction action);
    f32_t EvaluateReward(const WorldSnapshot& snap, EntityID self) const;
    std::vector<eMCTSAction> ListLegalActions(const WorldSnapshot& snap, EntityID self) const;

    std::mt19937 m_rng;
    f32_t m_fExploration = 1.4142f;
    u32_t m_uMaxDepth = 5;
};

NS_END
