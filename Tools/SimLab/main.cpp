// SimLab — headless GameSim determinism runner (Phase 0 golden test).
// Same seed + same scripted command stream => identical per-tick state hashes.
// Usage: SimLab.exe [tickCount] [seed]   (defaults: 1800 ticks = 60s sim, seed 42)
// Exit code 0 = deterministic, 1 = divergence detected.

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/GameplayComponents.h"

#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"

#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/AreaAura/AreaAuraSystem.h"
#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Combat/CombatActionSystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/Death/DeathSystem.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/JungleAI/JungleAISystem.h"
#include "Shared/GameSim/Systems/Move/MoveSystem.h"
#include "Shared/GameSim/Systems/Recall/RecallSystem.h"
#include "Shared/GameSim/Systems/Rune/RuneSystem.h"
#include "Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.h"

#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"
#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
    // All-walkable flat plane: SimLab has no navgrid; determinism is what matters here.
    struct FlatWalkable final : IWalkableQuery
    {
        bool_t IsWalkableXZ(const Vec3&) const override { return true; }
        bool_t SegmentWalkableXZ(const Vec3&, const Vec3&, f32_t) const override { return true; }
        bool_t TryClampMoveSegmentXZ(const Vec3&, const Vec3& vDesired, f32_t, Vec3& vOutPosition) const override
        {
            vOutPosition = vDesired;
            return true;
        }
        bool_t TryResolveMoveTarget(const Vec3&, const Vec3& rawTarget, Vec3& outTarget) const override
        {
            outTarget = rawTarget;
            return true;
        }
        bool_t TryBuildMovePath(const Vec3&, const Vec3& rawTarget,
            Vec3* pOutWaypoints, u16_t maxWaypoints, u16_t& outWaypointCount,
            Vec3& outTarget) const override
        {
            outTarget = rawTarget;
            outWaypointCount = 0;
            if (maxWaypoints >= 1 && pOutWaypoints)
            {
                pOutWaypoints[0] = rawTarget;
                outWaypointCount = 1;
            }
            return true;
        }
        bool_t TrySampleHeight(f32_t, f32_t, f32_t& outY) const override
        {
            outY = 0.f;
            return true;
        }
    };

    void HashU64(u64_t& h, u64_t v)
    {
        for (int i = 0; i < 8; ++i)
        {
            h ^= (v >> (i * 8)) & 0xFFull;
            h *= 1099511628211ull;
        }
    }

    void HashF32(u64_t& h, f32_t v)
    {
        u32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        HashU64(h, bits);
    }

    void RegisterAllChampionHooks()
    {
        RegisterDefaultChampionSkillScalingTables();
        AnnieGameSim::RegisterHooks();
        AsheGameSim::RegisterHooks();
        FioraGameSim::RegisterHooks();
        IreliaGameSim::RegisterHooks();
        JaxGameSim::RegisterHooks();
        KalistaGameSim::RegisterHooks();
        LeeSinGameSim::RegisterHooks();
        KindredGameSim::RegisterHooks();
        MasterYiGameSim::RegisterHooks();
        RivenGameSim::RegisterHooks();
        SylasGameSim::RegisterHooks();
        ViegoGameSim::RegisterHooks();
        YoneGameSim::RegisterHooks();
        YasuoGameSim::RegisterHooks();
        ZedGameSim::RegisterHooks();
    }

    EntityID SpawnChampion(CWorld& world, EntityIdMap& entityMap,
        eChampion champ, u8_t team, u8_t slotId)
    {
        const EntityID entity = world.CreateEntity();

        const Vec3 spawnPos = GetGameSimRosterSpawnPosition(slotId, team);

        TransformComponent transform{};
        transform.SetPosition(spawnPos);
        world.AddComponent<TransformComponent>(entity, transform);

        const ChampionStatsDef statsDef = CChampionStatsRegistry::Instance().Resolve(champ);
        StatComponent stat = CStatSystem::BuildBaseStats(statsDef, 6);
        world.AddComponent<StatComponent>(entity, stat);

        HealthComponent health{};
        health.fCurrent = stat.hpMax;
        health.fMaximum = stat.hpMax;
        health.bIsDead = false;
        world.AddComponent<HealthComponent>(entity, health);

        RespawnComponent respawn{};
        respawn.spawnPos = spawnPos;
        respawn.respawnDelay = 5.f;
        world.AddComponent<RespawnComponent>(entity, respawn);

        world.AddComponent<SkillStateComponent>(entity, SkillStateComponent{});

        CExperienceSystem::InitializeChampionExperience(world, entity, stat.level);

        SkillRankComponent skillRank{};
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
        world.AddComponent<SkillRankComponent>(entity, skillRank);

        GoldComponent gold{};
        gold.amount = 10000;
        world.AddComponent<GoldComponent>(entity, gold);

        world.AddComponent<InventoryComponent>(entity, InventoryComponent{});

        RuneLoadoutComponent runeLoadout{};
        runeLoadout.eRunes[0] = eRuneId::LethalTempo;
        runeLoadout.iCount = 1u;
        world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
        world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

        world.AddComponent<ChampionScoreComponent>(entity, ChampionScoreComponent{});

        ChampionComponent champion{};
        champion.id = champ;
        champion.team = static_cast<eTeam>(team);
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
        champion.mana = stat.manaMax;
        champion.maxMana = stat.manaMax;
        champion.moveSpeed = stat.moveSpeed;
        champion.level = stat.level;
        world.AddComponent<ChampionComponent>(entity, champion);

        if (champ == eChampion::YASUO)
            world.AddComponent<YasuoStateComponent>(entity, YasuoStateComponent{});
        if (champ == eChampion::ASHE)
            world.AddComponent<AsheSimComponent>(entity, AsheSimComponent{});
        if (champ == eChampion::ANNIE)
            world.AddComponent<AnnieSimComponent>(entity, AnnieSimComponent{});
        if (champ == eChampion::FIORA)
            world.AddComponent<FioraSimComponent>(entity, FioraSimComponent{});
        if (champ == eChampion::JAX)
            world.AddComponent<JaxSimComponent>(entity, JaxSimComponent{});
        if (champ == eChampion::VIEGO)
            world.AddComponent<ViegoSimComponent>(entity, ViegoSimComponent{});
        if (champ == eChampion::YONE)
            world.AddComponent<YoneSimComponent>(entity, YoneSimComponent{});
        if (champ == eChampion::LEESIN)
            world.AddComponent<LeeSinSimComponent>(entity, LeeSinSimComponent{});
        if (champ == eChampion::KINDRED)
            world.AddComponent<KindredSimComponent>(entity, KindredSimComponent{});
        if (champ == eChampion::MASTERYI)
            world.AddComponent<MasterYiSimComponent>(entity, MasterYiSimComponent{});

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Champion;
        spatial.team = team;
        spatial.radius = statsDef.spatialRadius;
        world.AddComponent<SpatialAgentComponent>(entity, spatial);

        world.AddComponent<TargetableTag>(entity);

        SetPoseState(world, entity, ePoseStateId::Idle, 0, true);

        NetEntityIdComponent netEntity{};
        netEntity.netId = entityMap.IssueNew(entity);
        world.AddComponent<NetEntityIdComponent>(entity, netEntity);

        return entity;
    }

    struct MatchResult
    {
        std::vector<u64_t> tickHashes;
        u64_t finalHash = 0;
    };

    MatchResult RunMatch(u64_t seed, u64_t tickCount)
    {
        CWorld world;
        DeterministicRng rng(seed);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        static constexpr eChampion kRoster[10] =
        {
            eChampion::YASUO, eChampion::ZED, eChampion::ASHE, eChampion::ANNIE, eChampion::LEESIN,
            eChampion::RIVEN, eChampion::SYLAS, eChampion::VIEGO, eChampion::YONE, eChampion::JAX,
        };

        std::vector<EntityID> champs;
        champs.reserve(10);
        for (u8_t slot = 0; slot < 10; ++slot)
        {
            const u8_t team = slot < 5 ? 0 : 1;
            champs.push_back(SpawnChampion(world, entityMap, kRoster[slot], team, slot));
        }

        // Scripted battleground center: midpoint of the two team spawn clusters.
        const Vec3 blueAnchor = GetGameSimRosterSpawnPosition(0, 0);
        const Vec3 redAnchor = GetGameSimRosterSpawnPosition(5, 1);
        const f32_t centerX = (blueAnchor.x + redAnchor.x) * 0.5f;
        const f32_t centerZ = (blueAnchor.z + redAnchor.z) * 0.5f;

        static constexpr u8_t kSkillSlots[4] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        MatchResult result;
        result.tickHashes.reserve(static_cast<size_t>(tickCount));

        std::vector<GameCommand> pendingCommands;
        u32_t seq = 0;

        for (u64_t tick = 1; tick <= tickCount; ++tick)
        {
            TickContext tc{};
            tc.tickIndex = tick;
            tc.fDt = DeterministicTime::kFixedDt;
            tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
            tc.pRng = &rng;
            tc.pEntityMap = &entityMap;
            tc.localPlayer = NULL_ENTITY;
            tc.pWalkable = &walkable;
            tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

            // Deterministic command script: converge on center, trade attacks and skills.
            for (size_t i = 0; i < champs.size(); ++i)
            {
                const EntityID e = champs[i];
                if (world.GetComponent<HealthComponent>(e).bIsDead)
                    continue;

                const EntityID enemy = champs[(i < 5 ? 5 : 0) + (rng.NextU32() % 5)];

                if ((tick + i * 3) % 15 == 0)
                {
                    GameCommand cmd{};
                    cmd.kind = eCommandKind::Move;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.groundPos = Vec3{
                        centerX + (rng.NextF01() * 2.f - 1.f) * 12.f,
                        0.f,
                        centerZ + (rng.NextF01() * 2.f - 1.f) * 12.f };
                    executor->ExecuteCommand(world, tc, cmd);
                }
                if ((tick + i * 7) % 45 == 0)
                {
                    GameCommand cmd{};
                    cmd.kind = eCommandKind::BasicAttack;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.targetEntity = enemy;
                    executor->ExecuteCommand(world, tc, cmd);
                }
                if ((tick + i * 11) % 60 == 0)
                {
                    const Vec3 myPos = world.GetComponent<TransformComponent>(e).GetPosition();
                    const Vec3 enemyPos = world.GetComponent<TransformComponent>(enemy).GetPosition();
                    Vec3 dir{ enemyPos.x - myPos.x, 0.f, enemyPos.z - myPos.z };
                    const f32_t len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
                    if (len > 0.0001f)
                    {
                        dir.x /= len;
                        dir.z /= len;
                    }
                    else
                    {
                        dir = Vec3{ 1.f, 0.f, 0.f };
                    }

                    GameCommand cmd{};
                    cmd.kind = eCommandKind::CastSkill;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.slot = kSkillSlots[rng.NextU32() % 4];
                    cmd.targetEntity = enemy;
                    cmd.groundPos = enemyPos;
                    cmd.direction = dir;
                    executor->ExecuteCommand(world, tc, cmd);
                }
                if ((tick + i * 13) % 90 == 0)
                {
                    GameCommand cmd{};
                    cmd.kind = eCommandKind::LevelSkill;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.slot = kSkillSlots[rng.NextU32() % 4];
                    executor->ExecuteCommand(world, tc, cmd);
                }
            }

            // Mirror of CGameRoom::Phase_SimulationSystems (GameSim-only portion).
            GameplayStatus::TickStatusEffects(world, tc);
            CSpellbookFormOverrideSystem::Execute(world, tc);
            CAreaAuraSystem::Execute(world, tc);
            CRuneSystem::Execute(world, tc);
            CStatSystem::Execute(world);
            CBuffSystem::Execute(world, tc);
            CSkillCooldownSystem::Execute(world, tc);
            CRecallSystem::Execute(world, tc);
            CWaypointPatrolSystem::Execute(world, tc);
            CCombatActionSystem::Execute(world, tc);
            CMoveSystem::Execute(world, tc);
            CJungleAISystem::Execute(world, tc, pendingCommands);
            CAttackChaseSystem::Execute(world, tc, pendingCommands);
            for (const auto& cmd : pendingCommands)
                executor->ExecuteCommand(world, tc, cmd);
            pendingCommands.clear();
            AnnieGameSim::Tick(world, tc);
            AsheGameSim::Tick(world, tc);
            FioraGameSim::Tick(world, tc);
            IreliaGameSim::Tick(world, tc);
            JaxGameSim::Tick(world, tc);
            KalistaGameSim::Tick(world, tc);
            LeeSinGameSim::Tick(world, tc);
            KindredGameSim::Tick(world, tc);
            MasterYiGameSim::Tick(world, tc);
            RivenGameSim::Tick(world, tc);
            SylasGameSim::Tick(world, tc);
            ViegoGameSim::Tick(world, tc);
            YoneGameSim::Tick(world, tc);
            YasuoGameSim::Tick(world, tc);
            ZedGameSim::Tick(world, tc);
            CDamageQueueSystem::Execute(world, tc);
            CStatSystem::Execute(world);
            CDeathSystem::Execute(world, tc);

            u64_t hash = 1469598103934665603ull;
            HashU64(hash, tick);
            for (const EntityID e : champs)
            {
                const Vec3 pos = world.GetComponent<TransformComponent>(e).GetPosition();
                const HealthComponent& hp = world.GetComponent<HealthComponent>(e);
                const ChampionComponent& ch = world.GetComponent<ChampionComponent>(e);
                const GoldComponent& gd = world.GetComponent<GoldComponent>(e);
                HashF32(hash, pos.x);
                HashF32(hash, pos.y);
                HashF32(hash, pos.z);
                HashF32(hash, hp.fCurrent);
                HashU64(hash, hp.bIsDead ? 1u : 0u);
                HashF32(hash, ch.mana);
                HashU64(hash, ch.level);
                HashU64(hash, static_cast<u64_t>(gd.amount));
            }
            HashU64(hash, rng.GetState());
            result.tickHashes.push_back(hash);
        }

        u64_t finalHash = 1469598103934665603ull;
        for (const u64_t h : result.tickHashes)
            HashU64(finalHash, h);
        result.finalHash = finalHash;
        return result;
    }

    TickContext MakeProbeTickContext(
        u64_t tick,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        FlatWalkable& walkable)
    {
        TickContext tc{};
        tc.tickIndex = tick;
        tc.fDt = DeterministicTime::kFixedDt;
        tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
        tc.pRng = &rng;
        tc.pEntityMap = &entityMap;
        tc.localPlayer = NULL_ENTITY;
        tc.pWalkable = &walkable;
        tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();
        return tc;
    }

    void TickYoneProbe(CWorld& world, const TickContext& tc)
    {
        GameplayStatus::TickStatusEffects(world, tc);
        CSpellbookFormOverrideSystem::Execute(world, tc);
        CSkillCooldownSystem::Execute(world, tc);
        CCombatActionSystem::Execute(world, tc);
        CMoveSystem::Execute(world, tc);
        YoneGameSim::Tick(world, tc);
        CDamageQueueSystem::Execute(world, tc);
        CStatSystem::Execute(world);
        CDeathSystem::Execute(world, tc);
    }

    void TickYoneProbeRange(
        CWorld& world,
        u64_t firstTick,
        u64_t lastTick,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        FlatWalkable& walkable)
    {
        for (u64_t tick = firstTick; tick <= lastTick; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            TickYoneProbe(world, tc);
        }
    }

    f32_t DistanceSqXZLocal(const Vec3& a, const Vec3& b)
    {
        const f32_t dx = a.x - b.x;
        const f32_t dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    bool_t RunYoneEReturnProbe()
    {
        CWorld world;
        DeterministicRng rng(20260624ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID yone = SpawnChampion(world, entityMap, eChampion::YONE, 0, 0);
        const EntityID target = SpawnChampion(world, entityMap, eChampion::JAX, 1, 5);

        const Vec3 anchor{ 0.f, 0.f, 0.f };
        const Vec3 targetPos{ 6.f, 0.f, 0.f };
        world.GetComponent<TransformComponent>(yone).SetPosition(anchor);
        world.GetComponent<TransformComponent>(target).SetPosition(targetPos);

        auto& ranks = world.GetComponent<SkillRankComponent>(yone);
        if (!CSkillRankSystem::TryLevelSkill(ranks, static_cast<u8_t>(eSkillSlot::E)))
        {
            std::printf("[SimLab][YoneE] FAIL: could not learn E\n");
            return false;
        }

        TickContext tick1 = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand eOut{};
        eOut.kind = eCommandKind::CastSkill;
        eOut.issuerEntity = yone;
        eOut.issuedAtTick = tick1.tickIndex;
        eOut.sequenceNum = 1u;
        eOut.slot = static_cast<u8_t>(eSkillSlot::E);
        eOut.targetEntity = target;
        eOut.groundPos = targetPos;
        eOut.direction = Vec3{ 1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick1, eOut);

        const auto& stateAfterOut = world.GetComponent<YoneSimComponent>(yone);
        if (!stateAfterOut.bSoulUnboundActive || stateAfterOut.bReturning)
        {
            std::printf("[SimLab][YoneE] FAIL: E out did not enter soul state\n");
            return false;
        }
        if (DistanceSqXZLocal(stateAfterOut.anchorPosition, anchor) > 0.0001f)
        {
            std::printf("[SimLab][YoneE] FAIL: E out anchor mismatch\n");
            return false;
        }

        TickYoneProbeRange(world, 2ull, 28ull, rng, entityMap, walkable);

        const auto& skillSlot =
            world.GetComponent<SkillStateComponent>(yone)
                .slots[static_cast<u8_t>(eSkillSlot::E)];
        if (skillSlot.currentStage != 1u || skillSlot.stageWindow <= 0.f)
        {
            std::printf("[SimLab][YoneE] FAIL: E stage window closed before recast\n");
            return false;
        }

        TickContext recastTick = MakeProbeTickContext(29ull, rng, entityMap, walkable);
        GameCommand eReturn{};
        eReturn.kind = eCommandKind::CastSkill;
        eReturn.issuerEntity = yone;
        eReturn.issuedAtTick = recastTick.tickIndex;
        eReturn.sequenceNum = 2u;
        eReturn.slot = static_cast<u8_t>(eSkillSlot::E);
        eReturn.itemId = 2u;
        eReturn.groundPos = anchor;
        eReturn.direction = Vec3{ -1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, recastTick, eReturn);

        const auto& stateAfterRecast = world.GetComponent<YoneSimComponent>(yone);
        if (!stateAfterRecast.bSoulUnboundActive || !stateAfterRecast.bReturning)
        {
            std::printf("[SimLab][YoneE] FAIL: E stage-2 did not start return\n");
            return false;
        }

        TickYoneProbeRange(world, 30ull, 70ull, rng, entityMap, walkable);

        const auto& stateAfterReturn = world.GetComponent<YoneSimComponent>(yone);
        const Vec3 finalPos = world.GetComponent<TransformComponent>(yone).GetPosition();
        if (stateAfterReturn.bSoulUnboundActive ||
            stateAfterReturn.bReturning ||
            stateAfterReturn.soulTimerSec > 0.f)
        {
            std::printf("[SimLab][YoneE] FAIL: return did not clear soul state\n");
            return false;
        }
        if (DistanceSqXZLocal(finalPos, anchor) > 0.0001f)
        {
            std::printf("[SimLab][YoneE] FAIL: return did not reach anchor\n");
            return false;
        }

        std::printf("[SimLab][YoneE] PASS: stage-2 return reached anchor=(%.2f,%.2f,%.2f)\n",
            anchor.x,
            anchor.y,
            anchor.z);
        return true;
    }
}

int main(int argc, char** argv)
{
    const u64_t tickCount = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1800ull;
    const u64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42ull;

    std::printf("[SimLab] ticks=%llu seed=%llu (5v5 scripted match, 30Hz)\n",
        static_cast<unsigned long long>(tickCount),
        static_cast<unsigned long long>(seed));

    RegisterAllChampionHooks();

    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    bool bPass = bYoneEReturnProbePass;

    if (runA.finalHash != runB.finalHash)
    {
        bPass = false;
        u64_t firstDivergence = 0;
        for (size_t i = 0; i < runA.tickHashes.size() && i < runB.tickHashes.size(); ++i)
        {
            if (runA.tickHashes[i] != runB.tickHashes[i])
            {
                firstDivergence = i + 1;
                break;
            }
        }
        std::printf("[SimLab] FAIL: same-seed runs diverged (first divergent tick=%llu)\n",
            static_cast<unsigned long long>(firstDivergence));
        std::printf("[SimLab]   runA=%016llX runB=%016llX\n",
            static_cast<unsigned long long>(runA.finalHash),
            static_cast<unsigned long long>(runB.finalHash));
    }
    else
    {
        std::printf("[SimLab] same-seed replay OK: hash=%016llX\n",
            static_cast<unsigned long long>(runA.finalHash));
    }

    if (runA.finalHash == runC.finalHash)
    {
        bPass = false;
        std::printf("[SimLab] FAIL: different seed produced identical hash — hash is not capturing sim state\n");
    }
    else
    {
        std::printf("[SimLab] seed sensitivity OK: seed+1 hash=%016llX\n",
            static_cast<unsigned long long>(runC.finalHash));
    }

    std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
    return bPass ? 0 : 1;
}
