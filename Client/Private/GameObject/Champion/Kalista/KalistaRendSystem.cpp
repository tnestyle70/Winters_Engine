#include "GameObject/Champion/Kalista/KalistaRendSystem.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"
#include <Windows.h>
#include <cstdio>
#include <functional>
#include <vector>

std::unique_ptr<CKalistaRendSystem> CKalistaRendSystem::Create()
{
    return std::unique_ptr<CKalistaRendSystem>(new CKalistaRendSystem());
}

void CKalistaRendSystem::AddStack(CWorld& world, EntityID target, EntityID caster, 
    eTeam casterTeam, Engine::CFxStaticMeshRenderer* pRenderer, f32_t fSpearScale)
{
    //적중시 적에게 창 꽂아서 누적 시키기
    //객체들을 Entity로 관리를 한다.NULL_ENTITY -1 맞지?
    if (target == NULL_ENTITY || caster == NULL_ENTITY)
        return;

    bool_t bFound = false;
    
    world.ForEach<KalistaRendComponent>(
        std::function<void(EntityID, KalistaRendComponent&)>(
            [&](EntityID, KalistaRendComponent& rend)
            {
                if (rend.hostTarget == target && rend.caster == caster)
                {
                    rend.iStackCount += 1;
                    bFound = true;
                }
            }));
    //박힌 창 시각 Entity Spawn
    char dbg[200]{};
    sprintf_s(dbg, "[KalistaRend] new stack target=%u pRenderer=%p stuckScale=%.4f\n",
        static_cast<u32_t>(target), pRenderer, fSpearScale);
    ::OutputDebugStringA(dbg);

    EntityID spearVisual = NULL_ENTITY;
    if (pRenderer)
        spearVisual = KalistaFx::SpawnESpearStuck(world, pRenderer, target, fSpearScale);

    sprintf_s(dbg, "[KalistaRend] SpawnESpearStuck spearVisual=%u\n",
        static_cast<u32_t>(spearVisual));
    ::OutputDebugStringA(dbg);

    EntityID entity = world.CreateEntity();
    //트리플 AAA 게임 엔진에서도 이런 식으로 함수 내에서 지역변수로 구조체 선언하고
    //값 채워 넣는 형식으로 감?
    KalistaRendComponent rend{};
    rend.hostTarget = target;
    rend.caster = caster;
    rend.casterTeam = casterTeam;
    rend.iStackCount = 1;
    rend.spearVisualEntity = spearVisual; //TriggerExplode/Execute가 dispose?
    world.AddComponent<KalistaRendComponent>(entity, rend);
}

void CKalistaRendSystem::TriggerExplode(CWorld& world, EntityID caster,
    f32_t fBaseDamage, f32_t fStackDmg)
{
    std::vector<EntityID> vecDelete;
    std::vector<EntityID> vecWispTargets;

    world.ForEach<KalistaRendComponent>(
        std::function<void(EntityID, KalistaRendComponent&)>(
            [&](EntityID entity, KalistaRendComponent& rend)
            {
                if (rend.caster != caster)
                    return;

                if (rend.hostTarget == NULL_ENTITY
                    || !world.HasComponent<ChampionComponent>(rend.hostTarget))
                {
                    vecDelete.push_back(entity);
                    return;
                }

                auto& champion = world.GetComponent<ChampionComponent>(rend.hostTarget);
                const f32_t totalDamage = fBaseDamage
                    + fStackDmg * static_cast<f32_t>(rend.iStackCount);
                champion.hp -= totalDamage;
                if (champion.hp < 0.f)
                    champion.hp = 0.f;

                bool_t bWispAlreadySpawned = false;
                for (EntityID wispTarget : vecWispTargets)
                {
                    if (wispTarget == rend.hostTarget)
                    {
                        bWispAlreadySpawned = true;
                        break;
                    }
                }

                if (!bWispAlreadySpawned)
                {
                    const Kalista::KalistaTuning& tuning = Kalista::GetTuning();
                    KalistaFx::SpawnEExplode(world, rend.hostTarget, tuning.eRendWispLifetime);
                    vecWispTargets.push_back(rend.hostTarget);
                }

                char dbg[160]{};
                sprintf_s(dbg,
                    "[KalistaRend] explode target=%u stacks=%d dmg=%.1f hp=%.1f\n",
                    static_cast<u32_t>(rend.hostTarget),
                    rend.iStackCount,
                    totalDamage,
                    champion.hp);
                ::OutputDebugStringA(dbg);

                if (rend.spearVisualEntity != NULL_ENTITY
                    && world.HasComponent<FxMeshComponent>(rend.spearVisualEntity))
                {
                    world.GetComponent<FxMeshComponent>(rend.spearVisualEntity).bPendingDelete = true;
                }

                vecDelete.push_back(entity);
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}

void CKalistaRendSystem::Execute(CWorld& world, f32_t)
{
    WINTERS_PROFILE_SCOPE("KalistaRend::Execute");

    std::vector<EntityID> vecDelete;

    world.ForEach<KalistaRendComponent>(
        std::function<void(EntityID, KalistaRendComponent&)>(
            [&](EntityID entity, KalistaRendComponent& rend)
            {
                if (rend.hostTarget == NULL_ENTITY
                    || !world.HasComponent<ChampionComponent>(rend.hostTarget))
                {
                    vecDelete.push_back(entity);
                    return;
                }

                const auto& champion = world.GetComponent<ChampionComponent>(rend.hostTarget);
                if (champion.hp <= 0.f)
                {
                    if (rend.spearVisualEntity != NULL_ENTITY
                        && world.HasComponent<FxMeshComponent>(rend.spearVisualEntity))
                    {
                        world.GetComponent<FxMeshComponent>(rend.spearVisualEntity).bPendingDelete = true;
                    }
                    vecDelete.push_back(entity);
                }
            }));

    for (EntityID entity : vecDelete)
        world.DestroyEntity(entity);
}
