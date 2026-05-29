#include "WintersPCH.h"
#include "AI/BTNodes_Champion.h"
#include "ECS/SpatialIndex.h"
#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <vector>

NS_BEGIN(Engine)

namespace
{
    bool_t TryGetSelfTeam(CWorld& world, EntityID self, u8_t& outTeam)
    {
        if (world.HasComponent<SpatialAgentComponent>(self))
        {
            outTeam = world.GetComponent<SpatialAgentComponent>(self).team;
            return true;
        }
        if (world.HasComponent<ChampionComponent>(self))
        {
            outTeam = static_cast<u8_t>(world.GetComponent<ChampionComponent>(self).team);
            return true;
        }
        return false;
    }

    bool_t TryStoreTarget(BTContext& ctx, EntityID target)
    {
        if (!ctx.pWorld || !ctx.pBB || target == NULL_ENTITY)
            return false;
        if (!ctx.pWorld->IsAlive(target) ||
            !ctx.pWorld->HasComponent<TransformComponent>(target))
        {
            return false;
        }

        ctx.pBB->Set("targetEntity", static_cast<uint64_t>(target));
        ctx.pBB->Set("targetPos", ctx.pWorld->GetComponent<TransformComponent>(target).GetPosition());
        return true;
    }

    EntityID FindClosestByMask(CWorld& world, const Vec3& origin,
        f32_t range, u8_t selfTeam, u32_t kindMask)
    {
        if (CSpatialIndex* pSpatial = world.Get_SpatialIndex())
        {
            std::vector<EntityID> candidates;
            pSpatial->QueryRadius(origin, range, kindMask, (1u << selfTeam), candidates);

            EntityID best = NULL_ENTITY;
            f32_t bestDistSq = range * range;
            for (EntityID candidate : candidates)
            {
                if (!world.IsAlive(candidate) ||
                    !world.HasComponent<TransformComponent>(candidate) ||
                    !world.HasComponent<HealthComponent>(candidate))
                {
                    continue;
                }

                const HealthComponent& hp = world.GetComponent<HealthComponent>(candidate);
                if (hp.bIsDead || hp.fCurrent <= 0.f)
                    continue;

                const Vec3 pos = world.GetComponent<TransformComponent>(candidate).GetPosition();
                const f32_t dx = pos.x - origin.x;
                const f32_t dz = pos.z - origin.z;
                const f32_t distSq = dx * dx + dz * dz;
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    best = candidate;
                }
            }
            return best;
        }

        return NULL_ENTITY;
    }

    void PushCommand(CWorld& world, EntityID self, const Command& command)
    {
        if (!world.HasComponent<CommandQueueComponent>(self))
            world.AddComponent<CommandQueueComponent>(self);

        CommandQueueComponent& queue = world.GetComponent<CommandQueueComponent>(self);
        queue.queue.push_back(command);
        queue.bActive = true;
    }
}

namespace BTNodes
{
    std::unique_ptr<CBTNode> Cond_HpBelow(f32_t pct)
    {
        return std::make_unique<CBTCondition>("HpBelow",
            [pct](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pWorld->HasComponent<HealthComponent>(ctx.self))
                    return false;
                const HealthComponent& hp = ctx.pWorld->GetComponent<HealthComponent>(ctx.self);
                const f32_t denom = (hp.fMaximum > 0.f) ? hp.fMaximum : 1.f;
                return (hp.fCurrent / denom) < pct;
            });
    }

    std::unique_ptr<CBTNode> Cond_ManaBelow(f32_t pct)
    {
        return std::make_unique<CBTCondition>("ManaBelow",
            [pct](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pWorld->HasComponent<ChampionComponent>(ctx.self))
                    return false;
                const ChampionComponent& champion = ctx.pWorld->GetComponent<ChampionComponent>(ctx.self);
                const f32_t denom = (champion.maxMana > 0.f) ? champion.maxMana : 1.f;
                return (champion.mana / denom) < pct;
            });
    }

    std::unique_ptr<CBTNode> Cond_EnemyChampInSight(f32_t range)
    {
        return std::make_unique<CBTCondition>("EnemyChampionInSight",
            [range](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pBB || !ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                    return false;

                u8_t selfTeam = 0;
                if (!TryGetSelfTeam(*ctx.pWorld, ctx.self, selfTeam))
                    return false;

                const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const EntityID target = FindClosestByMask(*ctx.pWorld, origin, range,
                    selfTeam, SpatialMask(eSpatialKind::Champion));
                return TryStoreTarget(ctx, target);
            });
    }

    std::unique_ptr<CBTNode> Cond_EnemyMinionInRange(f32_t range)
    {
        return std::make_unique<CBTCondition>("EnemyMinionInRange",
            [range](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pBB || !ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                    return false;

                u8_t selfTeam = 0;
                if (!TryGetSelfTeam(*ctx.pWorld, ctx.self, selfTeam))
                    return false;

                const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const EntityID target = FindClosestByMask(*ctx.pWorld, origin, range,
                    selfTeam, SpatialMask(eSpatialKind::Minion));
                return TryStoreTarget(ctx, target);
            });
    }

    std::unique_ptr<CBTNode> Cond_AllyChampInSight(f32_t range)
    {
        return std::make_unique<CBTCondition>("AllyChampionInSight",
            [range](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                    return false;

                u8_t selfTeam = 0;
                if (!TryGetSelfTeam(*ctx.pWorld, ctx.self, selfTeam))
                    return false;

                const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                EntityID best = NULL_ENTITY;
                f32_t bestDistSq = range * range;
                ctx.pWorld->ForEach<ChampionComponent, TransformComponent>(
                    function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                        [&](EntityID ID, ChampionComponent& champion, TransformComponent& xf)
                        {
                            if (ID == ctx.self || static_cast<u8_t>(champion.team) != selfTeam)
                                return;
                            const f32_t distSq = (xf.GetPosition().x - origin.x) * (xf.GetPosition().x - origin.x)
                                + (xf.GetPosition().z - origin.z) * (xf.GetPosition().z - origin.z);
                            if (distSq < bestDistSq)
                            {
                                bestDistSq = distSq;
                                best = ID;
                            }
                        }));
                return best != NULL_ENTITY;
            });
    }

    std::unique_ptr<CBTNode> Cond_InTowerRange(f32_t range)
    {
        return std::make_unique<CBTCondition>("InTowerRange",
            [range](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                    return false;

                u8_t selfTeam = 0;
                if (!TryGetSelfTeam(*ctx.pWorld, ctx.self, selfTeam))
                    return false;

                const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                const EntityID tower = FindClosestByMask(*ctx.pWorld, origin, range,
                    selfTeam, SpatialMask(eSpatialKind::Turret));
                return tower != NULL_ENTITY;
            });
    }

    std::unique_ptr<CBTNode> Cond_BBKeySet(const std::string& key)
    {
        return std::make_unique<CBTCondition>("BBKeySet",
            [key](BTContext& ctx) -> bool_t
            {
                return ctx.pBB && ctx.pBB->Has(key);
            });
    }

    std::unique_ptr<CBTNode> Cond_SkillReady(u8_t slot)
    {
        return std::make_unique<CBTCondition>("SkillReady",
            [slot](BTContext& ctx) -> bool_t
            {
                if (!ctx.pWorld || !ctx.pWorld->HasComponent<ChampionComponent>(ctx.self))
                    return false;
                if (slot == 0 || slot > 4)
                    return true;
                const ChampionComponent& champion = ctx.pWorld->GetComponent<ChampionComponent>(ctx.self);
                return champion.cooldowns[slot - 1] <= 0.f;
            });
    }

    std::unique_ptr<CBTNode> Act_MoveTo(const std::string& bbKeyTargetPos)
    {
        return std::make_unique<CBTAction>("MoveTo",
            [bbKeyTargetPos](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld || !ctx.pBB)
                    return eBTStatus::Failure;

                Command command{};
                command.type = eCommandType::Move;
                command.vTargetPos = ctx.pBB->GetVec3(bbKeyTargetPos, {});
                PushCommand(*ctx.pWorld, ctx.self, command);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_AttackTarget(const std::string& bbKeyTargetEntity)
    {
        return std::make_unique<CBTAction>("AttackTarget",
            [bbKeyTargetEntity](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld || !ctx.pBB)
                    return eBTStatus::Failure;

                const EntityID target = ctx.pBB->GetEntity(bbKeyTargetEntity, NULL_ENTITY);
                if (target == NULL_ENTITY || !ctx.pWorld->IsAlive(target))
                    return eBTStatus::Failure;

                Command command{};
                command.type = eCommandType::Attack;
                command.targetEntity = target;
                PushCommand(*ctx.pWorld, ctx.self, command);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_CastSkill(u8_t slot, const std::string& bbKeyTargetPos)
    {
        return std::make_unique<CBTAction>("CastSkill",
            [slot, bbKeyTargetPos](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld || !ctx.pBB)
                    return eBTStatus::Failure;

                Command command{};
                command.type = eCommandType::Cast;
                command.iSkillSlot = slot;
                command.vTargetPos = ctx.pBB->GetVec3(bbKeyTargetPos, {});
                command.targetEntity = ctx.pBB->GetEntity("targetEntity", NULL_ENTITY);
                PushCommand(*ctx.pWorld, ctx.self, command);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_Recall()
    {
        return std::make_unique<CBTAction>("Recall",
            [](BTContext&) -> eBTStatus
            {
                return eBTStatus::Success;
            });
    }

    std::unique_ptr<CBTNode> Act_Retreat()
    {
        return std::make_unique<CBTAction>("Retreat",
            [](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld || !ctx.pBB || !ctx.pWorld->HasComponent<TransformComponent>(ctx.self))
                    return eBTStatus::Failure;

                u8_t selfTeam = 0;
                if (!TryGetSelfTeam(*ctx.pWorld, ctx.self, selfTeam))
                    return eBTStatus::Failure;

                const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.self).GetPosition();
                EntityID bestTower = NULL_ENTITY;
                f32_t bestDistSq = 1000000.f;

                ctx.pWorld->ForEach<TurretComponent, TransformComponent>(
                    function<void(EntityID, TurretComponent&, TransformComponent&)>(
                        [&](EntityID ID, TurretComponent& turret, TransformComponent& xf)
                        {
                            if (static_cast<u8_t>(turret.team) != selfTeam)
                                return;
                            const f32_t distSq = (xf.GetPosition().x - origin.x) * (xf.GetPosition().x - origin.x)
                                + (xf.GetPosition().z - origin.z) * (xf.GetPosition().z - origin.z);
                            if (distSq < bestDistSq)
                            {
                                bestDistSq = distSq;
                                bestTower = ID;
                            }
                        }));

                if (bestTower != NULL_ENTITY)
                    ctx.pBB->Set("retreatPos", ctx.pWorld->GetComponent<TransformComponent>(bestTower).GetPosition());
                else
                    ctx.pBB->Set("retreatPos", ctx.pBB->GetVec3("lanePushPos", { 0.f, 0.f, 0.f }));

                Command command{};
                command.type = eCommandType::Move;
                command.vTargetPos = ctx.pBB->GetVec3("retreatPos", {});
                PushCommand(*ctx.pWorld, ctx.self, command);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_FarmMinion()
    {
        return std::make_unique<CBTAction>("FarmMinion",
            [](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pWorld || !ctx.pBB)
                    return eBTStatus::Failure;

                const EntityID target = ctx.pBB->GetEntity("targetEntity", NULL_ENTITY);
                if (target == NULL_ENTITY)
                    return eBTStatus::Failure;

                Command command{};
                command.type = eCommandType::Attack;
                command.targetEntity = target;
                PushCommand(*ctx.pWorld, ctx.self, command);
                return eBTStatus::Running;
            });
    }

    std::unique_ptr<CBTNode> Act_SetBBValue(const std::string& key, f32_t value)
    {
        return std::make_unique<CBTAction>("SetBBValue",
            [key, value](BTContext& ctx) -> eBTStatus
            {
                if (!ctx.pBB)
                    return eBTStatus::Failure;
                ctx.pBB->Set(key, value);
                return eBTStatus::Success;
            });
    }

    std::unique_ptr<CBTNode> Act_LogDebug(const std::string& message)
    {
        return std::make_unique<CBTAction>("LogDebug",
            [message](BTContext&) -> eBTStatus
            {
#if defined(_DEBUG)
                OutputDebugStringA(message.c_str());
                OutputDebugStringA("\n");
#endif
                return eBTStatus::Success;
            });
    }
}

std::unique_ptr<CBehaviorTree> BuildStandardChampionBT()
{
    auto bt = std::make_unique<CBehaviorTree>();
    auto root = std::make_unique<CBTSelector>();

    {
        auto retreat = std::make_unique<CBTSequence>();
        retreat->AddChild(BTNodes::Cond_HpBelow(0.30f));
        retreat->AddChild(BTNodes::Act_Retreat());
        root->AddChild(std::move(retreat));
    }

    {
        auto fight = std::make_unique<CBTSequence>();
        fight->AddChild(BTNodes::Cond_EnemyChampInSight(2.5f));
        fight->AddChild(BTNodes::Act_AttackTarget("targetEntity"));
        root->AddChild(std::move(fight));
    }

    {
        auto farm = std::make_unique<CBTSequence>();
        farm->AddChild(BTNodes::Cond_EnemyMinionInRange(8.f));
        farm->AddChild(BTNodes::Act_FarmMinion());
        root->AddChild(std::move(farm));
    }

    root->AddChild(BTNodes::Act_MoveTo("lanePushPos"));

    bt->SetRoot(std::move(root));
    return bt;
}

std::unique_ptr<CBehaviorTree> BuildMasterTierChampionBT()
{
    return BuildStandardChampionBT();
}

NS_END
