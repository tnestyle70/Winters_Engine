#include "WintersPCH.h"
#include "AI/MCTSPlanner.h"
#include "ECS/World.h"
#include "ECS/Components/AIControlComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <limits>

NS_BEGIN(Engine)

void WorldSnapshot::CaptureFromWorld(CWorld& world, EntityID self, f32_t radius)
{
    units.clear();
    if (!world.HasComponent<TransformComponent>(self))
        return;

    const Vec3 selfPos = world.GetComponent<TransformComponent>(self).GetPosition();
    const f32_t radiusSq = radius * radius;

    world.ForEach<TransformComponent, HealthComponent, SpatialAgentComponent>(
        function<void(EntityID, TransformComponent&, HealthComponent&, SpatialAgentComponent&)>(
            [&](EntityID id, TransformComponent& xf, HealthComponent& hp, SpatialAgentComponent& agent)
            {
                const Vec3 pos = xf.GetPosition();
                const f32_t dx = pos.x - selfPos.x;
                const f32_t dz = pos.z - selfPos.z;
                if (id != self && dx * dx + dz * dz > radiusSq)
                    return;

                UnitState state{};
                state.id = id;
                state.team = agent.team;
                state.pos = pos;
                state.hp = hp.fCurrent;
                state.maxHp = (hp.fMaximum > 0.f) ? hp.fMaximum : 1.f;

                if (world.HasComponent<AIResourceStateComponent>(id))
                {
                    const AIResourceStateComponent& resource =
                        world.GetComponent<AIResourceStateComponent>(id);
                    state.mana = resource.fMana;
                    for (u32_t i = 0; i < 4; ++i)
                        state.cooldowns[i] = resource.fCooldowns[i];
                }

                units.push_back(state);
            }));
}

WorldSnapshot::UnitState* WorldSnapshot::FindUnit(EntityID id)
{
    for (UnitState& unit : units)
    {
        if (unit.id == id)
            return &unit;
    }
    return nullptr;
}

const WorldSnapshot::UnitState* WorldSnapshot::FindUnit(EntityID id) const
{
    for (const UnitState& unit : units)
    {
        if (unit.id == id)
            return &unit;
    }
    return nullptr;
}

CMCTSPlanner::CMCTSPlanner()
    : m_rng(std::random_device{}())
{
}

eMCTSAction CMCTSPlanner::Plan(CWorld& world, EntityID self, u32_t iterations)
{
    WINTERS_PROFILE_SCOPE("MCTS::Plan");

    WorldSnapshot rootSnap;
    rootSnap.CaptureFromWorld(world, self, 30.f);
    if (!rootSnap.FindUnit(self))
        return eMCTSAction::None;

    auto root = std::make_unique<CMCTSNode>();

    for (u32_t i = 0; i < iterations; ++i)
    {
        WorldSnapshot snap = rootSnap;
        CMCTSNode* node = Select(root.get());

        if (node->visits > 0 && !node->bExpanded)
            node = Expand(node, snap, self);

        const f32_t reward = Rollout(snap, self, m_uMaxDepth);
        Backpropagate(node, reward);
    }

    if (root->children.empty())
        Expand(root.get(), rootSnap, self);
    if (root->children.empty())
        return eMCTSAction::None;

    CMCTSNode* best = root->children.front().get();
    for (auto& child : root->children)
    {
        if (child->visits > best->visits)
            best = child.get();
    }

    return best->action;
}

CMCTSNode* CMCTSPlanner::Select(CMCTSNode* node)
{
    while (node->bExpanded && !node->children.empty())
    {
        CMCTSNode* best = node->children.front().get();
        f32_t bestUcb = -std::numeric_limits<f32_t>::infinity();
        for (auto& child : node->children)
        {
            if (child->visits == 0)
                return child.get();

            const f32_t exploit = child->totalReward / static_cast<f32_t>(child->visits);
            const f32_t parentVisits = static_cast<f32_t>((node->visits > 0) ? node->visits : 1);
            const f32_t explore = m_fExploration *
                std::sqrt(std::log(parentVisits) / static_cast<f32_t>(child->visits));
            const f32_t ucb = exploit + explore;
            if (ucb > bestUcb)
            {
                bestUcb = ucb;
                best = child.get();
            }
        }
        node = best;
    }

    return node;
}

CMCTSNode* CMCTSPlanner::Expand(CMCTSNode* node, WorldSnapshot& snap, EntityID self)
{
    const std::vector<eMCTSAction> actions = ListLegalActions(snap, self);
    for (eMCTSAction action : actions)
    {
        auto child = std::make_unique<CMCTSNode>();
        child->action = action;
        child->parent = node;
        node->children.push_back(std::move(child));
    }

    node->bExpanded = true;
    return node->children.empty() ? node : node->children.front().get();
}

f32_t CMCTSPlanner::Rollout(WorldSnapshot& snap, EntityID self, u32_t depth)
{
    for (u32_t i = 0; i < depth; ++i)
    {
        const std::vector<eMCTSAction> actions = ListLegalActions(snap, self);
        if (actions.empty())
            break;

        std::uniform_int_distribution<size_t> dist(0, actions.size() - 1);
        ApplyAction(snap, self, actions[dist(m_rng)]);
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
    WorldSnapshot::UnitState* me = snap.FindUnit(self);
    if (!me)
        return;

    switch (action)
    {
    case eMCTSAction::AttackNearest:
    case eMCTSAction::CastQ:
    case eMCTSAction::CastW:
    case eMCTSAction::CastE:
    case eMCTSAction::CastR:
        for (WorldSnapshot::UnitState& unit : snap.units)
        {
            if (unit.team != me->team)
            {
                const f32_t damage = (action == eMCTSAction::AttackNearest) ? 45.f : 70.f;
                unit.hp = (unit.hp > damage) ? (unit.hp - damage) : 0.f;
                break;
            }
        }
        break;
    case eMCTSAction::Retreat:
        me->hp += 30.f;
        if (me->hp > me->maxHp)
            me->hp = me->maxHp;
        break;
    default:
        break;
    }
}

f32_t CMCTSPlanner::EvaluateReward(const WorldSnapshot& snap, EntityID self) const
{
    const WorldSnapshot::UnitState* me = snap.FindUnit(self);
    if (!me)
        return 0.f;

    const f32_t myHpRatio = me->hp / ((me->maxHp > 0.f) ? me->maxHp : 1.f);

    f32_t enemyHpSum = 0.f;
    f32_t enemyMaxHpSum = 0.f;
    for (const WorldSnapshot::UnitState& unit : snap.units)
    {
        if (unit.team == me->team)
            continue;
        enemyHpSum += unit.hp;
        enemyMaxHpSum += unit.maxHp;
    }

    const f32_t enemyHpRatio = (enemyMaxHpSum > 0.f) ? (enemyHpSum / enemyMaxHpSum) : 1.f;
    return myHpRatio - enemyHpRatio;
}

std::vector<eMCTSAction> CMCTSPlanner::ListLegalActions(const WorldSnapshot& snap,
    EntityID self) const
{
    const WorldSnapshot::UnitState* me = snap.FindUnit(self);
    if (!me)
        return {};

    bool_t bHasEnemy = false;
    for (const WorldSnapshot::UnitState& unit : snap.units)
    {
        if (unit.team != me->team && unit.hp > 0.f)
        {
            bHasEnemy = true;
            break;
        }
    }

    std::vector<eMCTSAction> actions;
    actions.push_back(eMCTSAction::Hold);
    actions.push_back(eMCTSAction::MoveTowardEnemy);
    actions.push_back(eMCTSAction::MoveAway);
    actions.push_back(eMCTSAction::Retreat);
    if (bHasEnemy)
    {
        actions.push_back(eMCTSAction::AttackNearest);
        actions.push_back(eMCTSAction::CastQ);
        actions.push_back(eMCTSAction::CastW);
        actions.push_back(eMCTSAction::CastE);
        actions.push_back(eMCTSAction::CastR);
    }

    return actions;
}

NS_END
