// SimLab — headless GameSim determinism runner (Phase 0 golden test).
// Same seed + same scripted command stream => identical per-tick state hashes.
// Usage: SimLab.exe [tickCount] [seed]   (defaults: 1800 ticks = 60s sim, seed 42)
// Exit code 0 = deterministic, 1 = divergence detected.

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Manager/Navigation/NavGrid.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include "Shared/GameSim/Core/Checkpoint/WorldKeyframe.h"
#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/EzrealSimComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/ShieldComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"

#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/AreaAura/AreaAuraSystem.h"
#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Combat/CombatActionSystem.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"
#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/Death/DeathSystem.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/JungleAI/JungleAISystem.h"
#include "Shared/GameSim/Systems/Move/MoveSystem.h"
#include "Shared/GameSim/Systems/Recall/RecallSystem.h"
#include "Shared/GameSim/Systems/Gold/GoldIncomeSystem.h"
#include "Shared/GameSim/Systems/Rune/RuneSystem.h"
#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"
#include "Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
#include "Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.h"

#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/Garen/GarenGameSim.h"
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
#include "Tools/AIResearch/Native/AiDecisionTraceCaptureWriter.h"

#include <bcrypt.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
    // All-walkable flat plane: SimLab has no navgrid; determinism is what matters here.
    struct FlatWalkable final : IWalkableQuery
    {
        bool_t bPointWalkable = true;
        bool_t bSegmentWalkable = true;
        mutable u32_t pointQueryCount = 0u;
        mutable u32_t segmentQueryCount = 0u;

        bool_t IsWalkableXZ(const Vec3&) const override
        {
            ++pointQueryCount;
            return bPointWalkable;
        }
        bool_t SegmentWalkableXZ(const Vec3&, const Vec3&, f32_t) const override
        {
            ++segmentQueryCount;
            return bSegmentWalkable;
        }
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

        void ResetQueryCounts() const
        {
            pointQueryCount = 0u;
            segmentQueryCount = 0u;
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
        // 서버 GameRoom 초기화와 동일하게 경제 정의를 레지스트리에 반영 (값 동일 = 골든 불변).
        if (const EconomyGameplayDef* pEconomy =
            ServerData::GetLoLGameplayDefinitionPack().FindEconomy())
        {
            CRewardRegistry::Instance().LoadFromEconomyDef(*pEconomy);
        }
        // 아이템 정의도 동일하게 반영 (값 동일 = 골든 불변).
        std::size_t itemDefCount = 0u;
        if (const ItemDef* pItemDefs =
            ServerData::GetLoLGameplayDefinitionPack().FindItems(itemDefCount))
        {
            CItemRegistry::Instance().LoadFromItemDefs(pItemDefs, itemDefCount);
        }
        AnnieGameSim::RegisterHooks();
        AsheGameSim::RegisterHooks();
        FioraGameSim::RegisterHooks();
        EzrealGameSim::RegisterHooks();
        GarenGameSim::RegisterHooks();
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

        // 서버 SpawnChampionForLobbySlot과 동일하게 정의 팩을 정본으로 쓴다.
        // 레벨만 프로브 호환을 위해 6 고정 유지 (서버는 spawnLoadout.startLevel).
        const SpawnObjectDefinitionPack& objectDefs =
            ServerData::GetLoLSpawnObjectDefinitionPack();
        const SpawnLoadoutPolicyDef& spawnPolicy = objectDefs.spawnLoadout;
        const GameplayDefinitionPack& definitions =
            ServerData::GetLoLGameplayDefinitionPack();
        const ChampionGameplayDef* championDef = definitions.FindChampion(champ);
        constexpr u8_t kSimLabStartLevel = 6u;

        StatComponent stat{};
        f32_t spatialRadius = 0.75f;
        f32_t sightRange = 19.f;
        if (championDef)
        {
            ChampionDefinitionComponent identity{};
            identity.championDefId = championDef->id;
            world.AddComponent<ChampionDefinitionComponent>(entity, identity);

            SkillLoadoutComponent loadout{};
            for (u8_t skillSlot = 0u; skillSlot < kChampionSkillSlotCount; ++skillSlot)
                loadout.skills[skillSlot] = championDef->skillLoadout[skillSlot];
            world.AddComponent<SkillLoadoutComponent>(entity, loadout);

            stat = CStatSystem::BuildBaseStats(
                championDef->stats,
                championDef->legacyChampion,
                kSimLabStartLevel);
            spatialRadius = championDef->stats.spatialRadius;
            sightRange = championDef->stats.sightRange;
        }
        else
        {
            const ChampionStatsDef statsDef =
                CChampionStatsRegistry::Instance().Resolve(champ);
            stat = CStatSystem::BuildBaseStats(statsDef, kSimLabStartLevel);
            spatialRadius = statsDef.spatialRadius;
        }
        world.AddComponent<StatComponent>(entity, stat);

        HealthComponent health{};
        health.fCurrent = stat.hpMax;
        health.fMaximum = stat.hpMax;
        health.bIsDead = false;
        world.AddComponent<HealthComponent>(entity, health);

        RespawnComponent respawn{};
        respawn.spawnPos = spawnPos;
        respawn.respawnDelay = spawnPolicy.respawnDelaySec;
        world.AddComponent<RespawnComponent>(entity, respawn);

        world.AddComponent<SkillStateComponent>(entity, SkillStateComponent{});

        CExperienceSystem::InitializeChampionExperience(world, entity, stat.level);

        SkillRankComponent skillRank{};
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
        world.AddComponent<SkillRankComponent>(entity, skillRank);

        GoldComponent gold{};
        gold.amount = spawnPolicy.startGold;
        world.AddComponent<GoldComponent>(entity, gold);

        world.AddComponent<InventoryComponent>(entity, InventoryComponent{});

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
        spatial.kind = eSpatialKind::Character;
        spatial.team = team;
        spatial.radius = spatialRadius;
        world.AddComponent<SpatialAgentComponent>(entity, spatial);

        VisionSourceComponent vision{};
        vision.sightRange = sightRange;
        world.AddComponent<VisionSourceComponent>(entity, vision);

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

            if (CBuffSystem::PruneExpiredTickBuffs(world, tc))
                CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());

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
            GameplayStatus::TickForcedMotions(world, tc);
            CSpellbookFormOverrideSystem::Execute(world, tc);
            CAreaAuraSystem::Execute(world, tc);
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CBuffSystem::AdvanceDurationsAfterStat(world, tc);
            CSkillCooldownSystem::Execute(world, tc);
            CRecallSystem::Execute(world, tc);
            CGoldIncomeSystem::Execute(world, tc);
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
            EzrealGameSim::Tick(world, tc);
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
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
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
        IWalkableQuery& walkable)
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

    bool_t RunServerAuthoritativeShieldProbe()
    {
        CWorld world;
        DeterministicRng rng(20260714ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID riven = SpawnChampion(
            world, entityMap, eChampion::RIVEN,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID leeSin = SpawnChampion(
            world, entityMap, eChampion::LEESIN,
            static_cast<u8_t>(eTeam::Blue), 1u);
        const EntityID ally = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 2u);
        const EntityID yasuo = SpawnChampion(
            world, entityMap, eChampion::YASUO,
            static_cast<u8_t>(eTeam::Blue), 3u);
        const EntityID attacker = SpawnChampion(
            world, entityMap, eChampion::ANNIE,
            static_cast<u8_t>(eTeam::Red), 5u);

        world.GetComponent<TransformComponent>(leeSin).SetPosition(Vec3{ 0.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(ally).SetPosition(Vec3{ 1.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(riven)
            .ranks[static_cast<u8_t>(eSkillSlot::E)] = 1u;
        world.GetComponent<SkillRankComponent>(leeSin)
            .ranks[static_cast<u8_t>(eSkillSlot::W)] = 1u;

        TickContext invalidShieldTick = MakeProbeTickContext(
            9ull, rng, entityMap, walkable);
        if (CShieldSystem::Grant(
                world,
                invalidShieldTick,
                riven,
                (std::numeric_limits<f32_t>::quiet_NaN)(),
                3.f) ||
            CShieldSystem::Grant(
                world,
                invalidShieldTick,
                riven,
                70.f,
                (std::numeric_limits<f32_t>::infinity)()) ||
            world.HasComponent<ShieldComponent>(riven))
        {
            std::printf("[SimLab][Shield] FAIL: invalid numeric grant accepted\n");
            return false;
        }

        TickContext rivenTick = MakeProbeTickContext(
            10ull, rng, entityMap, walkable);
        GameCommand rivenE{};
        rivenE.kind = eCommandKind::CastSkill;
        rivenE.issuerEntity = riven;
        rivenE.slot = static_cast<u8_t>(eSkillSlot::E);
        rivenE.sequenceNum = 1u;
        rivenE.issuedAtTick = rivenTick.tickIndex;
        executor->ExecuteCommand(world, rivenTick, rivenE);
        if (!world.HasComponent<ShieldComponent>(riven) ||
            std::abs(world.GetComponent<ShieldComponent>(riven).fCurrent - 70.f) > 0.001f ||
            world.GetComponent<ShieldComponent>(riven).uExpireTick != 100ull ||
            std::abs(world.GetComponent<ChampionComponent>(riven).shield - 70.f) > 0.001f)
        {
            std::printf("[SimLab][Shield] FAIL: Riven E did not grant 70 for 90 ticks\n");
            return false;
        }

        const f32_t rivenHp = world.GetComponent<HealthComponent>(riven).fCurrent;
        DamageRequest damage{};
        damage.source = attacker;
        damage.target = riven;
        damage.sourceTeam = eTeam::Red;
        damage.type = eDamageType::True;
        damage.flatAmount = 25.f;
        TickContext damageTick = MakeProbeTickContext(
            11ull, rng, entityMap, walkable);
        const DamageResult partial = ApplyDamageRequest(world, damageTick, damage);
        if (!partial.bWasShielded || partial.finalAmount != 0.f ||
            std::abs(world.GetComponent<HealthComponent>(riven).fCurrent - rivenHp) > 0.001f ||
            std::abs(world.GetComponent<ChampionComponent>(riven).shield - 45.f) > 0.001f)
        {
            std::printf("[SimLab][Shield] FAIL: partial absorption changed health\n");
            return false;
        }

        damage.flatAmount = 60.f;
        TickContext spillTick = MakeProbeTickContext(
            12ull, rng, entityMap, walkable);
        const DamageResult spill = ApplyDamageRequest(world, spillTick, damage);
        if (!spill.bWasShielded ||
            std::abs(spill.finalAmount - 15.f) > 0.001f ||
            world.HasComponent<ShieldComponent>(riven) ||
            world.GetComponent<ChampionComponent>(riven).shield != 0.f)
        {
            std::printf("[SimLab][Shield] FAIL: spillover/depletion mismatch\n");
            return false;
        }

        TickContext grantTick = MakeProbeTickContext(
            20ull, rng, entityMap, walkable);
        if (!CShieldSystem::Grant(world, grantTick, riven, 70.f, 3.f))
            return false;
        TickContext beforeExpiry = MakeProbeTickContext(
            109ull, rng, entityMap, walkable);
        CShieldSystem::Execute(world, beforeExpiry);
        if (!world.HasComponent<ShieldComponent>(riven))
        {
            std::printf("[SimLab][Shield] FAIL: shield expired before grant+90\n");
            return false;
        }
        TickContext atExpiry = MakeProbeTickContext(
            110ull, rng, entityMap, walkable);
        CShieldSystem::Execute(world, atExpiry);
        if (world.HasComponent<ShieldComponent>(riven) ||
            world.GetComponent<ChampionComponent>(riven).shield != 0.f)
        {
            std::printf("[SimLab][Shield] FAIL: shield survived grant+90\n");
            return false;
        }

        TickContext firstGrant = MakeProbeTickContext(
            120ull, rng, entityMap, walkable);
        TickContext refreshGrant = MakeProbeTickContext(
            150ull, rng, entityMap, walkable);
        CShieldSystem::Grant(world, firstGrant, riven, 40.f, 3.f);
        CShieldSystem::Grant(world, refreshGrant, riven, 70.f, 3.f);
        const ShieldComponent& refreshed = world.GetComponent<ShieldComponent>(riven);
        if (std::abs(refreshed.fCurrent - 70.f) > 0.001f ||
            refreshed.uExpireTick != 240ull)
        {
            std::printf("[SimLab][Shield] FAIL: recast did not replace and refresh\n");
            return false;
        }

        std::vector<u8_t> keyframe;
        if (!SimCheckpoint::SaveWorldKeyframe(
                world, rng, entityMap, 150ull, keyframe))
        {
            std::printf("[SimLab][Shield] FAIL: active shield keyframe save failed\n");
            return false;
        }
        CWorld restoredWorld;
        DeterministicRng restoredRng(1ull);
        EntityIdMap restoredEntityMap;
        u64_t restoredTick = 0u;
        if (!SimCheckpoint::RestoreWorldKeyframe(
                restoredWorld,
                restoredRng,
                restoredEntityMap,
                restoredTick,
                keyframe) ||
            restoredTick != 150ull ||
            !restoredWorld.HasComponent<ShieldComponent>(riven) ||
            restoredWorld.GetComponent<ShieldComponent>(riven).uExpireTick != 240ull ||
            std::abs(restoredWorld.GetComponent<ChampionComponent>(riven).shield - 70.f) > 0.001f)
        {
            std::printf("[SimLab][Shield] FAIL: active shield keyframe restore mismatch\n");
            return false;
        }

        TickContext invalidLeeTick = MakeProbeTickContext(
            199ull, rng, entityMap, walkable);
        GameCommand invalidLeeW{};
        invalidLeeW.kind = eCommandKind::CastSkill;
        invalidLeeW.issuerEntity = leeSin;
        invalidLeeW.targetEntity = attacker;
        invalidLeeW.slot = static_cast<u8_t>(eSkillSlot::W);
        invalidLeeW.sequenceNum = 2u;
        invalidLeeW.issuedAtTick = invalidLeeTick.tickIndex;
        const f32_t leeManaBeforeInvalid =
            world.GetComponent<ChampionComponent>(leeSin).mana;
        const f32_t leeCooldownBeforeInvalid =
            world.GetComponent<SkillStateComponent>(leeSin)
                .slots[static_cast<u8_t>(eSkillSlot::W)].cooldownRemaining;
        const CommandExecutionResult invalidLeeResult =
            executor->ExecuteCommand(world, invalidLeeTick, invalidLeeW);
        if (invalidLeeResult.state != eCommandExecutionState::Rejected ||
            invalidLeeResult.reason != eCommandExecutionReason::ChampionRuleBlocked ||
            world.GetComponent<ChampionComponent>(leeSin).mana != leeManaBeforeInvalid ||
            world.GetComponent<SkillStateComponent>(leeSin)
                .slots[static_cast<u8_t>(eSkillSlot::W)].cooldownRemaining !=
                    leeCooldownBeforeInvalid)
        {
            std::printf("[SimLab][Shield] FAIL: invalid Lee Sin W consumed cast state\n");
            return false;
        }

        TickContext leeTick = MakeProbeTickContext(
            200ull, rng, entityMap, walkable);
        GameCommand leeW{};
        leeW.kind = eCommandKind::CastSkill;
        leeW.issuerEntity = leeSin;
        leeW.targetEntity = ally;
        leeW.slot = static_cast<u8_t>(eSkillSlot::W);
        leeW.sequenceNum = 3u;
        leeW.issuedAtTick = leeTick.tickIndex;
        executor->ExecuteCommand(world, leeTick, leeW);
        if (!world.HasComponent<ShieldComponent>(leeSin) ||
            std::abs(world.GetComponent<ShieldComponent>(leeSin).fCurrent - 80.f) > 0.001f ||
            world.GetComponent<ShieldComponent>(leeSin).uExpireTick != 290ull)
        {
            std::printf("[SimLab][Shield] FAIL: Lee Sin W1 did not grant 80 for 90 ticks\n");
            return false;
        }

        DamageRequest yasuoHit{};
        yasuoHit.source = attacker;
        yasuoHit.target = yasuo;
        yasuoHit.sourceTeam = eTeam::Red;
        yasuoHit.type = eDamageType::True;
        yasuoHit.flatAmount = 0.f;
        EnqueueDamageRequest(world, yasuoHit);
        TickContext zeroDamageTick = MakeProbeTickContext(
            299ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, zeroDamageTick);
        const YasuoStateComponent& yasuoBeforeHit =
            world.GetComponent<YasuoStateComponent>(yasuo);
        if (world.HasComponent<ShieldComponent>(yasuo) ||
            yasuoBeforeHit.fPassiveFlow < yasuoBeforeHit.fPassiveFlowMax)
        {
            std::printf("[SimLab][Shield] FAIL: zero damage consumed Yasuo passive\n");
            return false;
        }

        yasuoHit.flatAmount = 30.f;
        EnqueueDamageRequest(world, yasuoHit);
        TickContext yasuoTick = MakeProbeTickContext(
            300ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, yasuoTick);
        if (!world.HasComponent<ShieldComponent>(yasuo) ||
            std::abs(world.GetComponent<ShieldComponent>(yasuo).fCurrent - 70.f) > 0.001f ||
            world.GetComponent<YasuoStateComponent>(yasuo).fPassiveFlow != 0.f)
        {
            std::printf("[SimLab][Shield] FAIL: Yasuo passive first-hit absorption mismatch\n");
            return false;
        }

        yasuoHit.flatAmount = 10.f;
        EnqueueDamageRequest(world, yasuoHit);
        TickContext secondYasuoTick = MakeProbeTickContext(
            301ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, secondYasuoTick);

        u32_t passiveEventCount = 0u;
        bool_t bPassiveDurationMatched = false;
        const u32_t passiveEffectId = MakeGameplayHookId(
            eChampion::YASUO,
            GameplayHookVariant::Passive_Trigger);
        world.ForEach<ReplicatedEventComponent>(
            [&](EntityID, ReplicatedEventComponent& event)
            {
                if (event.kind == eReplicatedEventKind::EffectTrigger &&
                    event.effectId == passiveEffectId)
                {
                    ++passiveEventCount;
                    bPassiveDurationMatched = event.durationMs == 3000u;
                }
            });
        if (passiveEventCount != 1u || !bPassiveDurationMatched ||
            std::abs(world.GetComponent<ChampionComponent>(yasuo).shield - 60.f) > 0.001f)
        {
            std::printf("[SimLab][Shield] FAIL: Yasuo passive cue count/duration mismatch\n");
            return false;
        }

        std::printf(
            "[SimLab][Shield] PASS: Riven=70 LeeSin=80 Yasuo=100 duration=90ticks\n");
        return true;
    }

    void TickYoneProbe(CWorld& world, const TickContext& tc)
    {
        GameplayStatus::TickStatusEffects(world, tc);
        GameplayStatus::TickForcedMotions(world, tc);
        CSpellbookFormOverrideSystem::Execute(world, tc);
        CSkillCooldownSystem::Execute(world, tc);
        CCombatActionSystem::Execute(world, tc);
        CMoveSystem::Execute(world, tc);
        YoneGameSim::Tick(world, tc);
        CDamageQueueSystem::Execute(world, tc);
        CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
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

    EntityID SpawnStatusProbeTarget(
        CWorld& world,
        GameplayStateQuery::eGameplayTargetKind kind,
        eTeam team,
        const Vec3& position)
    {
        const EntityID entity = world.CreateEntity();

        TransformComponent transform{};
        transform.SetPosition(position);
        world.AddComponent<TransformComponent>(entity, transform);

        HealthComponent health{};
        health.fCurrent = 1000.f;
        health.fMaximum = 1000.f;
        world.AddComponent<HealthComponent>(entity, health);
        world.AddComponent<TargetableTag>(entity);

        SpatialAgentComponent spatial{};
        spatial.team = static_cast<u8_t>(team);
        spatial.radius = 0.5f;

        switch (kind)
        {
        case GameplayStateQuery::eGameplayTargetKind::MinionOrSummon:
        {
            MinionComponent minion{};
            minion.team = team;
            world.AddComponent<MinionComponent>(entity, minion);

            MinionStateComponent state{};
            state.team = team;
            world.AddComponent<MinionStateComponent>(entity, state);
            spatial.kind = eSpatialKind::Unit;
            break;
        }
        case GameplayStateQuery::eGameplayTargetKind::JungleMonster:
        {
            world.AddComponent<JungleComponent>(entity, JungleComponent{});
            world.AddComponent<JungleMonsterTag>(entity);
            spatial.kind = eSpatialKind::NeutralUnit;
            break;
        }
        case GameplayStateQuery::eGameplayTargetKind::Structure:
        {
            StructureComponent structure{};
            structure.team = team;
            world.AddComponent<StructureComponent>(entity, structure);
            spatial.kind = eSpatialKind::Structure;
            break;
        }
        default:
            return NULL_ENTITY;
        }

        world.AddComponent<SpatialAgentComponent>(entity, spatial);
        return entity;
    }

    u8_t CountStatusEffects(
        CWorld& world,
        EntityID target,
        eStatusEffectId effectId,
        EntityID source)
    {
        if (!world.HasComponent<StatusEffectComponent>(target))
            return 0u;

        const StatusEffectComponent& effects =
            world.GetComponent<StatusEffectComponent>(target);
        u8_t count = 0u;
        for (u8_t i = 0u; i < effects.count; ++i)
        {
            const StatusEffectInstance& effect = effects.active[i];
            if (effect.effectId == effectId && effect.sourceEntity == source)
                ++count;
        }
        return count;
    }

    bool_t RunServerAuthoritativeStatusProbe()
    {
        CWorld world;
        DeterministicRng rng(20260712ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;

        const EntityID sourceA =
            SpawnChampion(world, entityMap, eChampion::SYLAS, 0u, 0u);
        const EntityID sourceB =
            SpawnChampion(world, entityMap, eChampion::SYLAS, 0u, 1u);
        const EntityID champion =
            SpawnChampion(world, entityMap, eChampion::JAX, 1u, 5u);
        const EntityID gatherChampion =
            SpawnChampion(world, entityMap, eChampion::RIVEN, 1u, 6u);
        const EntityID minion = SpawnStatusProbeTarget(
            world,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            eTeam::Red,
            Vec3{ 12.f, 0.f, 2.f });
        const EntityID jungle = SpawnStatusProbeTarget(
            world,
            GameplayStateQuery::eGameplayTargetKind::JungleMonster,
            eTeam::Neutral,
            Vec3{ 14.f, 0.f, 4.f });
        const EntityID structure = SpawnStatusProbeTarget(
            world,
            GameplayStateQuery::eGameplayTargetKind::Structure,
            eTeam::Red,
            Vec3{ 16.f, 0.f, 6.f });

        const Vec3 championStart{ 10.f, 0.f, 0.f };
        const Vec3 gatherStart{ 20.f, 0.f, 3.f };
        world.GetComponent<TransformComponent>(champion).SetPosition(championStart);
        world.GetComponent<TransformComponent>(gatherChampion).SetPosition(gatherStart);

        TickContext applyTick =
            MakeProbeTickContext(1ull, rng, entityMap, walkable);
        constexpr f32_t kStunDurationSec = 1.f;
        constexpr f32_t kSlowDurationSec = 1.5f;
        constexpr f32_t kSlowMultiplier = 0.65f;
        constexpr f32_t kArcHeight = 1.5f;
        const f32_t arcDurationSec = DeterministicTime::kFixedDt * 4.f;

        const StatusEffectApplyDesc stun = GameplayStatus::MakeStunDesc(
            sourceA,
            eChampion::SYLAS,
            eSkillSlot::E,
            kStunDurationSec);
        const StatusEffectApplyDesc slow = GameplayStatus::MakeSlowDesc(
            sourceA,
            eChampion::SYLAS,
            eSkillSlot::E,
            kSlowDurationSec,
            kSlowMultiplier);

        const EntityID mobileTargets[] = { champion, minion, jungle };
        for (EntityID target : mobileTargets)
        {
            const bool_t stunApplied = GameplayStatus::TryApplyStatusEffect(
                world, target, stun, applyTick);
            const bool_t slowApplied = GameplayStatus::TryApplyStatusEffect(
                world, target, slow, applyTick);
            const bool_t airborneApplied = GameplayStatus::ApplyAirborne(
                world,
                applyTick,
                target,
                sourceA,
                eChampion::SYLAS,
                eSkillSlot::E,
                arcDurationSec,
                kArcHeight);
            if (!stunApplied || !slowApplied || !airborneApplied)
            {
                std::printf(
                    "[SimLab][StatusCC] FAIL: mobile target %u rejected stun/slow/airborne\n",
                    static_cast<unsigned>(target));
                return false;
            }

            if (CountStatusEffects(world, target, eStatusEffectId::GenericStun, sourceA) != 1u ||
                CountStatusEffects(world, target, eStatusEffectId::GenericSlow, sourceA) != 1u ||
                CountStatusEffects(world, target, eStatusEffectId::GenericAirborne, sourceA) != 1u ||
                !world.HasComponent<ForcedMotionComponent>(target))
            {
                std::printf(
                    "[SimLab][StatusCC] FAIL: mobile target %u did not retain independent effects\n",
                    static_cast<unsigned>(target));
                return false;
            }

            if (GameplayStateQuery::CanMove(world, target) ||
                GameplayStateQuery::CanAttack(world, target) ||
                GameplayStateQuery::CanCast(world, target) ||
                std::fabs(
                    GameplayStateQuery::GetMoveSpeedMultiplier(world, target) -
                    kSlowMultiplier) > 0.0001f)
            {
                std::printf(
                    "[SimLab][StatusCC] FAIL: mobile target %u aggregate state mismatch\n",
                    static_cast<unsigned>(target));
                return false;
            }
        }

        if (GameplayStateQuery::ResolveTargetKind(world, champion) !=
                GameplayStateQuery::eGameplayTargetKind::Champion ||
            GameplayStateQuery::ResolveTargetKind(world, minion) !=
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon ||
            GameplayStateQuery::ResolveTargetKind(world, jungle) !=
                GameplayStateQuery::eGameplayTargetKind::JungleMonster ||
            GameplayStateQuery::ResolveTargetKind(world, structure) !=
                GameplayStateQuery::eGameplayTargetKind::Structure)
        {
            std::printf("[SimLab][StatusCC] FAIL: target-kind classification mismatch\n");
            return false;
        }

        const bool_t structureStunApplied = GameplayStatus::TryApplyStatusEffect(
            world, structure, stun, applyTick);
        const bool_t structureSlowApplied = GameplayStatus::TryApplyStatusEffect(
            world, structure, slow, applyTick);
        const bool_t structureAirborneApplied = GameplayStatus::ApplyAirborne(
            world,
            applyTick,
            structure,
            sourceA,
            eChampion::SYLAS,
            eSkillSlot::E,
            arcDurationSec,
            kArcHeight);
        if (structureStunApplied || structureSlowApplied || structureAirborneApplied ||
            world.HasComponent<StatusEffectComponent>(structure) ||
            world.HasComponent<ForcedMotionComponent>(structure))
        {
            std::printf("[SimLab][StatusCC] FAIL: structure accepted crowd control\n");
            return false;
        }

        const StatusEffectApplyDesc secondCasterSlow = GameplayStatus::MakeSlowDesc(
            sourceB,
            eChampion::SYLAS,
            eSkillSlot::E,
            2.f,
            0.8f);
        if (!GameplayStatus::TryApplyStatusEffect(
                world, champion, secondCasterSlow, applyTick) ||
            CountStatusEffects(
                world, champion, eStatusEffectId::GenericSlow, sourceA) != 1u ||
            CountStatusEffects(
                world, champion, eStatusEffectId::GenericSlow, sourceB) != 1u ||
            CountStatusEffects(
                world, champion, eStatusEffectId::GenericAirborne, sourceA) != 1u)
        {
            std::printf(
                "[SimLab][StatusCC] FAIL: same slot cross-effect/cross-caster independence mismatch\n");
            return false;
        }

        for (u64_t tick = 2ull; tick <= 3ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickForcedMotions(world, tc);
        }
        const Vec3 championMid =
            world.GetComponent<TransformComponent>(champion).GetPosition();
        if (championMid.y <= championStart.y ||
            DistanceSqXZLocal(championMid, championStart) > 0.0001f)
        {
            std::printf("[SimLab][StatusCC] FAIL: airborne arc midpoint mismatch\n");
            return false;
        }

        for (u64_t tick = 4ull; tick <= 5ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickForcedMotions(world, tc);
        }
        const Vec3 championLanded =
            world.GetComponent<TransformComponent>(champion).GetPosition();
        if (world.HasComponent<ForcedMotionComponent>(champion) ||
            std::fabs(championLanded.y - championStart.y) > 0.0001f ||
            DistanceSqXZLocal(championLanded, championStart) > 0.0001f)
        {
            std::printf("[SimLab][StatusCC] FAIL: airborne arc did not land cleanly\n");
            return false;
        }

        const Vec3 gatherLanding{ 26.f, 0.f, 9.f };
        if (!GameplayStatus::ApplyAirborne(
                world,
                applyTick,
                gatherChampion,
                sourceA,
                eChampion::YONE,
                eSkillSlot::R,
                arcDurationSec,
                kArcHeight,
                &gatherLanding))
        {
            std::printf("[SimLab][StatusCC] FAIL: gather airborne was rejected\n");
            return false;
        }
        for (u64_t tick = 6ull; tick <= 7ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickForcedMotions(world, tc);
        }
        const Vec3 gatherMid =
            world.GetComponent<TransformComponent>(gatherChampion).GetPosition();
        const Vec3 expectedGatherMid{
            (gatherStart.x + gatherLanding.x) * 0.5f,
            0.f,
            (gatherStart.z + gatherLanding.z) * 0.5f
        };
        if (gatherMid.y <= gatherStart.y ||
            DistanceSqXZLocal(gatherMid, expectedGatherMid) > 0.0001f)
        {
            std::printf("[SimLab][StatusCC] FAIL: gather airborne midpoint mismatch\n");
            return false;
        }
        for (u64_t tick = 8ull; tick <= 9ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickForcedMotions(world, tc);
        }
        const Vec3 gatherEnd =
            world.GetComponent<TransformComponent>(gatherChampion).GetPosition();
        if (world.HasComponent<ForcedMotionComponent>(gatherChampion) ||
            std::fabs(gatherEnd.y - gatherLanding.y) > 0.0001f ||
            DistanceSqXZLocal(gatherEnd, gatherLanding) > 0.0001f)
        {
            std::printf("[SimLab][StatusCC] FAIL: gather airborne endpoint mismatch\n");
            return false;
        }

        const Vec3 clearLanding{ 18.f, 0.f, 8.f };
        if (!GameplayStatus::ApplyAirborne(
                world,
                applyTick,
                jungle,
                sourceA,
                eChampion::YONE,
                eSkillSlot::R,
                arcDurationSec,
                kArcHeight,
                &clearLanding))
        {
            std::printf("[SimLab][StatusCC] FAIL: clear fixture motion was rejected\n");
            return false;
        }
        GameplayStatus::ClearStatusEffects(world, jungle);
        const Vec3 clearedPosition =
            world.GetComponent<TransformComponent>(jungle).GetPosition();
        const GameplayStateComponent& clearedState =
            world.GetComponent<GameplayStateComponent>(jungle);
        if (world.HasComponent<StatusEffectComponent>(jungle) ||
            world.HasComponent<ForcedMotionComponent>(jungle) ||
            clearedState.stateFlags != 0u ||
            std::fabs(clearedState.fMoveSpeedMul - 1.f) > 0.0001f ||
            !GameplayStateQuery::CanMove(world, jungle) ||
            !GameplayStateQuery::CanAttack(world, jungle) ||
            !GameplayStateQuery::CanCast(world, jungle) ||
            std::fabs(clearedPosition.y - clearLanding.y) > 0.0001f ||
            DistanceSqXZLocal(clearedPosition, clearLanding) > 0.0001f)
        {
            std::printf("[SimLab][StatusCC] FAIL: ClearStatusEffects did not restore baseline\n");
            return false;
        }

        std::printf(
            "[SimLab][StatusCC] PASS: mobile CC, structure immunity, stack identity, arc/gather, clear\n");
        return true;
    }

    bool_t RunSylasUltimateCoverageProbe()
    {
        static constexpr eChampion kStealableRoster[] =
        {
            eChampion::IRELIA,
            eChampion::YASUO,
            eChampion::KALISTA,
            eChampion::GAREN,
            eChampion::ZED,
            eChampion::RIVEN,
            eChampion::EZREAL,
            eChampion::FIORA,
            eChampion::JAX,
            eChampion::LEESIN,
            eChampion::KINDRED,
            eChampion::MASTERYI,
            eChampion::ANNIE,
            eChampion::ASHE,
            eChampion::VIEGO,
            eChampion::YONE,
        };
        for (eChampion champion : kStealableRoster)
        {
            if (!CSpellbookFormOverrideSystem::CanDispatchCapturedUltimate(champion))
            {
                std::printf(
                    "[SimLab][SylasR] FAIL: missing ultimate hook champion=%u\n",
                    static_cast<unsigned>(champion));
                return false;
            }
        }
        if (CSpellbookFormOverrideSystem::CanDispatchCapturedUltimate(eChampion::SYLAS))
        {
            std::printf("[SimLab][SylasR] FAIL: Sylas R should not capture itself\n");
            return false;
        }

		{
			CWorld guardWorld;
			DeterministicRng guardRng(20260713ull);
			EntityIdMap guardEntityMap;
			FlatWalkable guardWalkable;
			auto guardExecutor = CDefaultCommandExecutor::Create();

			const EntityID guardSylas = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::SYLAS,
				static_cast<u8_t>(eTeam::Blue), 0u);
			const EntityID guardAlly = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::JAX,
				static_cast<u8_t>(eTeam::Blue), 1u);
			const EntityID guardNearEnemy = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::JAX,
				static_cast<u8_t>(eTeam::Red), 5u);
			const EntityID guardFarEnemy = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::JAX,
				static_cast<u8_t>(eTeam::Red), 6u);
			const EntityID baseFiora = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::FIORA,
				static_cast<u8_t>(eTeam::Blue), 2u);
			const EntityID baseZed = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::ZED,
				static_cast<u8_t>(eTeam::Blue), 3u);
			const EntityID baseKindred = SpawnChampion(
				guardWorld, guardEntityMap, eChampion::KINDRED,
				static_cast<u8_t>(eTeam::Blue), 4u);

			guardWorld.GetComponent<TransformComponent>(guardSylas).SetPosition(Vec3{});
			guardWorld.GetComponent<TransformComponent>(guardAlly).SetPosition(Vec3{ 1.f, 0.f, 0.f });
			guardWorld.GetComponent<TransformComponent>(guardNearEnemy).SetPosition(Vec3{ 2.f, 0.f, 0.f });
			guardWorld.GetComponent<TransformComponent>(guardFarEnemy).SetPosition(Vec3{ 100.f, 0.f, 0.f });
			guardWorld.GetComponent<TransformComponent>(baseFiora).SetPosition(Vec3{});
			guardWorld.GetComponent<TransformComponent>(baseZed).SetPosition(Vec3{});
			guardWorld.GetComponent<TransformComponent>(baseKindred).SetPosition(Vec3{});

			for (EntityID caster : { guardSylas, baseFiora, baseZed, baseKindred })
			{
				guardWorld.GetComponent<SkillRankComponent>(caster).ranks[
					static_cast<u8_t>(eSkillSlot::R)] = 1u;
			}

			TickContext guardTick = MakeProbeTickContext(
				10ull, guardRng, guardEntityMap, guardWalkable);
			if (FioraGameSim::CanCastGrandChallenge(
					guardWorld, guardTick, guardSylas, NULL_ENTITY) ||
				FioraGameSim::CanCastGrandChallenge(
					guardWorld, guardTick, guardSylas, guardAlly) ||
				FioraGameSim::CanCastGrandChallenge(
					guardWorld, guardTick, guardSylas, guardFarEnemy) ||
				ZedGameSim::CanCastDeathMark(
					guardWorld, guardTick, guardSylas, NULL_ENTITY) ||
				ZedGameSim::CanCastDeathMark(
					guardWorld, guardTick, guardSylas, guardAlly) ||
				ZedGameSim::CanCastDeathMark(
					guardWorld, guardTick, guardSylas, guardFarEnemy) ||
				KindredGameSim::CanCastLambsRespite(
					guardWorld, guardTick, guardSylas, Vec3{ 100.f, 0.f, 0.f }))
			{
				std::printf("[SimLab][SylasR] FAIL: target/range validator accepted invalid R\n");
				return false;
			}

			guardWalkable.bSegmentWalkable = false;
			if (FioraGameSim::CanCastGrandChallenge(
					guardWorld, guardTick, guardSylas, guardNearEnemy) ||
				ZedGameSim::CanCastDeathMark(
					guardWorld, guardTick, guardSylas, guardNearEnemy) ||
				KindredGameSim::CanCastLambsRespite(
					guardWorld, guardTick, guardSylas, Vec3{ 2.f, 0.f, 0.f }))
			{
				std::printf("[SimLab][SylasR] FAIL: wall validator accepted blocked R\n");
				return false;
			}
			guardWalkable.bSegmentWalkable = true;
			guardWalkable.bPointWalkable = false;
			if (KindredGameSim::CanCastLambsRespite(
					guardWorld, guardTick, guardSylas, Vec3{ 2.f, 0.f, 0.f }))
			{
				std::printf("[SimLab][SylasR] FAIL: Kindred R accepted unwalkable center\n");
				return false;
			}
			guardWalkable.bPointWalkable = true;

			auto ResetRSlot = [&](EntityID caster)
			{
				auto& slot = guardWorld.GetComponent<SkillStateComponent>(caster).slots[
					static_cast<u8_t>(eSkillSlot::R)];
				slot.cooldownRemaining = 0.f;
				slot.cooldownDuration = 0.f;
				slot.currentStage = 0u;
				slot.stageWindow = 0.f;
			};
			auto ExpectCapturedReject = [&] (
				eChampion sourceChampion,
				EntityID target,
				const Vec3& groundPos,
				const Vec3& direction,
				u32_t sequenceNum) -> bool_t
			{
				ResetRSlot(guardSylas);
				SpellbookOverrideComponent overrideState{};
				overrideState.sourceChampion = sourceChampion;
				overrideState.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
				overrideState.localSlot = static_cast<u8_t>(eSkillSlot::R);
				overrideState.sourceRank = 1u;
				overrideState.fRemainingSec = 45.f;
				overrideState.bActive = true;
				if (guardWorld.HasComponent<SpellbookOverrideComponent>(guardSylas))
				{
					guardWorld.GetComponent<SpellbookOverrideComponent>(guardSylas) = overrideState;
				}
				else
				{
					guardWorld.AddComponent<SpellbookOverrideComponent>(guardSylas, overrideState);
				}

				GameCommand command{};
				command.kind = eCommandKind::CastSkill;
				command.issuerEntity = guardSylas;
				command.slot = static_cast<u8_t>(eSkillSlot::R);
				command.targetEntity = target;
				command.groundPos = groundPos;
				command.direction = direction;
				command.sequenceNum = sequenceNum;
				command.issuedAtTick = guardTick.tickIndex;
				guardExecutor->ExecuteCommand(guardWorld, guardTick, command);

				const auto& slot = guardWorld.GetComponent<SkillStateComponent>(guardSylas).slots[
					static_cast<u8_t>(eSkillSlot::R)];
				return guardWorld.HasComponent<SpellbookOverrideComponent>(guardSylas) &&
					guardWorld.GetComponent<SpellbookOverrideComponent>(guardSylas).sourceChampion ==
						sourceChampion &&
					slot.cooldownRemaining <= 0.f &&
					slot.currentStage == 0u;
			};

			if (!ExpectCapturedReject(
					eChampion::FIORA, guardAlly, Vec3{}, Vec3{}, 10u) ||
				!ExpectCapturedReject(
					eChampion::ZED, guardFarEnemy, Vec3{}, Vec3{}, 11u) ||
				!ExpectCapturedReject(
					eChampion::KINDRED, NULL_ENTITY, Vec3{ 100.f, 0.f, 0.f }, Vec3{}, 12u) ||
				!ExpectCapturedReject(
					eChampion::GAREN,
					guardNearEnemy,
					Vec3{},
					Vec3{ (std::numeric_limits<f32_t>::quiet_NaN)(), 0.f, 0.f },
					13u))
			{
				std::printf("[SimLab][SylasR] FAIL: rejected stolen R consumed state/cooldown\n");
				return false;
			}

			auto ExpectBaseReject = [&] (
				EntityID caster,
				EntityID target,
				const Vec3& groundPos,
				u32_t sequenceNum) -> bool_t
			{
				ResetRSlot(caster);
				GameCommand command{};
				command.kind = eCommandKind::CastSkill;
				command.issuerEntity = caster;
				command.slot = static_cast<u8_t>(eSkillSlot::R);
				command.targetEntity = target;
				command.groundPos = groundPos;
				command.sequenceNum = sequenceNum;
				command.issuedAtTick = guardTick.tickIndex;
				guardExecutor->ExecuteCommand(guardWorld, guardTick, command);
				const auto& slot = guardWorld.GetComponent<SkillStateComponent>(caster).slots[
					static_cast<u8_t>(eSkillSlot::R)];
				return slot.cooldownRemaining <= 0.f && slot.currentStage == 0u;
			};

			if (!ExpectBaseReject(baseFiora, guardAlly, Vec3{}, 20u) ||
				!ExpectBaseReject(baseZed, guardFarEnemy, Vec3{}, 21u) ||
				!ExpectBaseReject(
					baseKindred, NULL_ENTITY, Vec3{ 100.f, 0.f, 0.f }, 22u))
			{
				std::printf("[SimLab][SylasR] FAIL: rejected base R committed cooldown\n");
				return false;
			}
		}

        CWorld world;
        DeterministicRng rng(20260712ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID sylas = SpawnChampion(
            world,
            entityMap,
            eChampion::SYLAS,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID kalista = SpawnChampion(
            world,
            entityMap,
            eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Red),
            5u);
        world.GetComponent<TransformComponent>(sylas).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(kalista).SetPosition(Vec3{ 2.f, 0.f, 0.f });
        auto& ranks = world.GetComponent<SkillRankComponent>(sylas);
        ranks.ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;

        TickContext captureTick = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand capture{};
        capture.kind = eCommandKind::CastSkill;
        capture.issuerEntity = sylas;
        capture.slot = static_cast<u8_t>(eSkillSlot::R);
        capture.targetEntity = kalista;
        capture.sequenceNum = 1u;
        capture.issuedAtTick = captureTick.tickIndex;
        executor->ExecuteCommand(world, captureTick, capture);
        if (!world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            world.GetComponent<SpellbookOverrideComponent>(sylas).sourceChampion !=
                eChampion::KALISTA)
        {
            std::printf("[SimLab][SylasR] FAIL: Kalista R capture was rejected\n");
            return false;
        }

        TickContext rejectedTick = MakeProbeTickContext(2ull, rng, entityMap, walkable);
        GameCommand fateCall{};
        fateCall.kind = eCommandKind::CastSkill;
        fateCall.issuerEntity = sylas;
        fateCall.slot = static_cast<u8_t>(eSkillSlot::R);
        fateCall.direction = Vec3{ 1.f, 0.f, 0.f };
        fateCall.sequenceNum = 2u;
        fateCall.issuedAtTick = rejectedTick.tickIndex;
        executor->ExecuteCommand(world, rejectedTick, fateCall);
        const auto& rejectedSlot = world.GetComponent<SkillStateComponent>(sylas).slots[
            static_cast<u8_t>(eSkillSlot::R)];
        if (!world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            world.HasComponent<KalistaFateCallComponent>(sylas) ||
            rejectedSlot.cooldownRemaining > 0.f ||
            rejectedSlot.currentStage != 0u)
        {
            std::printf(
                "[SimLab][SylasR] FAIL: no-ally Fate's Call committed state/cooldown\n");
            return false;
        }

        const EntityID ally = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue),
            1u);
        world.GetComponent<TransformComponent>(ally).SetPosition(Vec3{ 1.f, 0.f, 0.f });

        TickContext stage1Tick = MakeProbeTickContext(3ull, rng, entityMap, walkable);
        fateCall.sequenceNum = 3u;
        fateCall.issuedAtTick = stage1Tick.tickIndex;
        executor->ExecuteCommand(world, stage1Tick, fateCall);
        if (!world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            !world.HasComponent<KalistaFateCallComponent>(sylas) ||
            world.GetComponent<KalistaFateCallComponent>(sylas).eStage !=
                eKalistaFateCallStage::Pulling ||
            world.GetComponent<SkillStateComponent>(sylas).slots[
                static_cast<u8_t>(eSkillSlot::R)].currentStage != 1u)
        {
            std::printf(
                "[SimLab][SylasR] FAIL: stage1 did not retain override/carry state\n");
            return false;
        }

        for (u64_t tick = 4ull; tick <= 12ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickStatusEffects(world, tc);
            KalistaGameSim::Tick(world, tc);
        }
        if (!world.HasComponent<KalistaFateCallComponent>(sylas) ||
            world.GetComponent<KalistaFateCallComponent>(sylas).eStage !=
                eKalistaFateCallStage::Carrying)
        {
            std::printf(
                "[SimLab][SylasR] FAIL: Fate's Call pull did not reach carrying stage\n");
            return false;
        }

        TickContext stage2Tick = MakeProbeTickContext(13ull, rng, entityMap, walkable);
        fateCall.sequenceNum = 4u;
        fateCall.issuedAtTick = stage2Tick.tickIndex;
        fateCall.itemId = 2u;
        executor->ExecuteCommand(world, stage2Tick, fateCall);
        if (world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            !world.HasComponent<KalistaFateCallComponent>(sylas) ||
            world.GetComponent<KalistaFateCallComponent>(sylas).eStage !=
                eKalistaFateCallStage::Launching ||
            !world.HasComponent<ActionStateComponent>(sylas) ||
            world.GetComponent<ActionStateComponent>(sylas).stage != 2u ||
            world.GetComponent<ActionStateComponent>(sylas).commandSequence != 4u ||
            world.GetComponent<ActionStateComponent>(sylas).movePolicy !=
                eSkillActionMovePolicy::QueueUntilUnlock ||
            world.GetComponent<ActionStateComponent>(sylas).lockEndTick <=
                stage2Tick.tickIndex)
        {
            std::printf(
                "[SimLab][SylasR] FAIL: terminal stage did not launch/consume override\n");
            return false;
        }

        SpellbookOverrideComponent expiringOverride{};
        expiringOverride.sourceChampion = eChampion::RIVEN;
        expiringOverride.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        expiringOverride.localSlot = static_cast<u8_t>(eSkillSlot::R);
        expiringOverride.sourceRank = 1u;
        expiringOverride.fRemainingSec = 45.f;
        expiringOverride.bActive = true;
        world.AddComponent<SpellbookOverrideComponent>(sylas, expiringOverride);
        auto& expiringSlot = world.GetComponent<SkillStateComponent>(sylas).slots[
            static_cast<u8_t>(eSkillSlot::R)];
        expiringSlot.currentStage = 1u;
        expiringSlot.stageWindow = DeterministicTime::kFixedDt * 0.5f;
        TickContext expiryTick = MakeProbeTickContext(14ull, rng, entityMap, walkable);
        CSkillCooldownSystem::Execute(world, expiryTick);
        if (world.HasComponent<SpellbookOverrideComponent>(sylas) ||
            expiringSlot.currentStage != 0u ||
            expiringSlot.stageWindow > 0.f)
        {
            std::printf(
                "[SimLab][SylasR] FAIL: expired stage window retained override\n");
            return false;
        }

        std::printf(
            "[SimLab][SylasR] PASS: 16-opponent coverage, authority guards, terminal/expiry consume\n");
        return true;
    }

    bool_t RunKalistaOathswornFateCallProbe()
    {
        CWorld world;
        DeterministicRng rng(20260712ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID kalista = SpawnChampion(
            world, entityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID ally = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        const EntityID enemy = SpawnChampion(
            world, entityMap, eChampion::RIVEN,
            static_cast<u8_t>(eTeam::Red), 5u);
        world.GetComponent<TransformComponent>(kalista).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(ally).SetPosition(Vec3{ 1.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(enemy).SetPosition(Vec3{ 2.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(kalista).ranks[
            static_cast<u8_t>(eSkillSlot::R)] = 1u;

        const f32_t allyHpBefore =
            world.GetComponent<HealthComponent>(ally).fCurrent;
        const u32_t kalistaGoldBefore =
            world.GetComponent<GoldComponent>(kalista).amount;
        const u32_t allyGoldBefore =
            world.GetComponent<GoldComponent>(ally).amount;
        const ChampionScoreComponent kalistaScoreBefore =
            world.GetComponent<ChampionScoreComponent>(kalista);
        const ChampionScoreComponent allyScoreBefore =
            world.GetComponent<ChampionScoreComponent>(ally);

        GameCommand bind{};
        bind.kind = eCommandKind::UseItem;
        bind.issuerEntity = kalista;
        bind.targetEntity = ally;
        bind.itemId = kKalistaOathswornItemId;
        bind.sequenceNum = 1u;
        TickContext rejectedTick =
            MakeProbeTickContext(1ull, rng, entityMap, walkable);
        bind.issuedAtTick = rejectedTick.tickIndex;
        executor->ExecuteCommand(world, rejectedTick, bind);
        if (world.HasComponent<KalistaOathswornComponent>(kalista) ||
            world.HasComponent<KalistaOathswornByComponent>(ally))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: contract succeeded without slot-6 item\n");
            return false;
        }

        auto& inventory = world.GetComponent<InventoryComponent>(kalista);
        inventory.itemIds[kKalistaOathswornInventorySlot] =
            kKalistaOathswornItemId;
        inventory.count = InventoryComponent::kMaxSlots;

        world.GetComponent<TransformComponent>(ally).SetPosition(
            Vec3{ kKalistaOathswornContractRange + 2.f, 0.f, 0.f });
        TickContext farBindTick =
            MakeProbeTickContext(2ull, rng, entityMap, walkable);
        bind.sequenceNum = 2u;
        bind.issuedAtTick = farBindTick.tickIndex;
        executor->ExecuteCommand(world, farBindTick, bind);
        if (world.HasComponent<KalistaOathswornComponent>(kalista) ||
            inventory.itemIds[kKalistaOathswornInventorySlot] !=
                kKalistaOathswornItemId)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: out-of-range contract was accepted/consumed\n");
            return false;
        }

        world.GetComponent<TransformComponent>(ally).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        TickContext blockedBindTick =
            MakeProbeTickContext(3ull, rng, entityMap, walkable);
        const StatusEffectApplyDesc contractStun = GameplayStatus::MakeStunDesc(
            enemy,
            eChampion::RIVEN,
            eSkillSlot::W,
            1.f);
        if (!GameplayStatus::TryApplyStatusEffect(
            world,
            kalista,
            contractStun,
            blockedBindTick))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: contract caster stun fixture rejected\n");
            return false;
        }
        bind.sequenceNum = 3u;
        bind.issuedAtTick = blockedBindTick.tickIndex;
        executor->ExecuteCommand(world, blockedBindTick, bind);
        if (world.HasComponent<KalistaOathswornComponent>(kalista) ||
            inventory.itemIds[kKalistaOathswornInventorySlot] !=
                kKalistaOathswornItemId)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: stunned Kalista contract was accepted/consumed\n");
            return false;
        }
        GameplayStatus::RemoveStatusEffect(
            world,
            kalista,
            eStatusEffectId::GenericStun,
            enemy);

        TickContext bindTick =
            MakeProbeTickContext(4ull, rng, entityMap, walkable);
        bind.sequenceNum = 4u;
        bind.issuedAtTick = bindTick.tickIndex;
        executor->ExecuteCommand(world, bindTick, bind);
        if (!world.HasComponent<KalistaOathswornComponent>(kalista) ||
            !world.HasComponent<KalistaOathswornByComponent>(ally) ||
            world.GetComponent<KalistaOathswornComponent>(kalista).eStage !=
                eKalistaOathswornStage::Binding ||
            world.GetComponent<KalistaOathswornByComponent>(ally).entityKalista !=
                kalista ||
            CountStatusEffects(
                world,
                ally,
                eStatusEffectId::KalistaOathswornRitual,
                kalista) != 1u ||
            inventory.itemIds[kKalistaOathswornInventorySlot] != 0u ||
            inventory.count != 0u ||
            GameplayStateQuery::CanMove(world, ally) ||
            GameplayStateQuery::CanAttack(world, ally) ||
            GameplayStateQuery::CanCast(world, ally) ||
            GameplayStateQuery::CanReceiveDamage(world, enemy, ally))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: binding relation or ritual lock mismatch\n");
            return false;
        }

        if (!world.HasComponent<ActionStateComponent>(ally) ||
            !world.HasComponent<PoseStateComponent>(ally))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: ritual presentation components missing\n");
            return false;
        }
        if (world.GetComponent<ActionStateComponent>(ally).actionId !=
                static_cast<u16_t>(eActionStateId::DeathStart) ||
            world.GetComponent<HealthComponent>(ally).bIsDead ||
            std::fabs(world.GetComponent<HealthComponent>(ally).fCurrent -
                allyHpBefore) > 0.0001f)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: ritual presentation action=%u pose=%u dead=%u hp=%.3f/%.3f\n",
                static_cast<unsigned>(
                    world.GetComponent<ActionStateComponent>(ally).actionId),
                static_cast<unsigned>(
                    world.GetComponent<PoseStateComponent>(ally).poseId),
                world.GetComponent<HealthComponent>(ally).bIsDead ? 1u : 0u,
                world.GetComponent<HealthComponent>(ally).fCurrent,
                allyHpBefore);
            return false;
        }

        for (u64_t tick = 5ull; tick <= 52ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickStatusEffects(world, tc);
            KalistaGameSim::Tick(world, tc);
        }
        if (world.GetComponent<KalistaOathswornComponent>(kalista).eStage !=
                eKalistaOathswornStage::Bound ||
            CountStatusEffects(
                world,
                ally,
                eStatusEffectId::KalistaOathswornRitual,
                kalista) != 0u ||
            !GameplayStateQuery::CanMove(world, ally) ||
            !GameplayStateQuery::CanAttack(world, ally) ||
            !GameplayStateQuery::CanCast(world, ally) ||
            world.GetComponent<PoseStateComponent>(ally).poseId !=
                static_cast<u16_t>(ePoseStateId::Idle) ||
            world.GetComponent<HealthComponent>(ally).bIsDead)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: ritual did not transition to live Bound state\n");
            return false;
        }

        GameCommand fateCall{};
        fateCall.kind = eCommandKind::CastSkill;
        fateCall.issuerEntity = kalista;
        fateCall.slot = static_cast<u8_t>(eSkillSlot::R);
        fateCall.direction = Vec3{ 1.f, 0.f, 0.f };
        fateCall.groundPos = Vec3{ 8.f, 0.f, 0.f };
        fateCall.sequenceNum = 5u;
        TickContext rTick =
            MakeProbeTickContext(53ull, rng, entityMap, walkable);
        fateCall.issuedAtTick = rTick.tickIndex;
        executor->ExecuteCommand(world, rTick, fateCall);
        if (!world.HasComponent<KalistaFateCallComponent>(kalista) ||
            world.GetComponent<KalistaFateCallComponent>(kalista).eStage !=
                eKalistaFateCallStage::Pulling ||
            !world.HasComponent<KalistaFateCallCarriedComponent>(ally) ||
            world.GetComponent<KalistaFateCallCarriedComponent>(ally).bHidden)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: R did not begin visible Pulling stage\n");
            return false;
        }

        for (u64_t tick = 54ull; tick <= 62ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickStatusEffects(world, tc);
            KalistaGameSim::Tick(world, tc);
        }
        const Vec3 carriedPosition =
            world.GetComponent<TransformComponent>(ally).GetPosition();
        const Vec3 ownerPosition =
            world.GetComponent<TransformComponent>(kalista).GetPosition();
        if (world.GetComponent<KalistaFateCallComponent>(kalista).eStage !=
                eKalistaFateCallStage::Carrying ||
            !world.GetComponent<KalistaFateCallCarriedComponent>(ally).bHidden ||
            DistanceSqXZLocal(carriedPosition, ownerPosition) > 0.0001f)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: Pulling did not converge to hidden Carrying\n");
            return false;
        }

        const f32_t humanCarryRemainingBefore =
            world.GetComponent<KalistaFateCallComponent>(kalista).fRemainingSec;
        for (u64_t tick = 63ull; tick <= 184ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            GameplayStatus::TickStatusEffects(world, tc);
            KalistaGameSim::Tick(world, tc);
        }
        if (!world.HasComponent<KalistaFateCallComponent>(kalista) ||
            world.GetComponent<KalistaFateCallComponent>(kalista).eStage !=
                eKalistaFateCallStage::Carrying ||
            !world.GetComponent<KalistaFateCallCarriedComponent>(ally).bHidden ||
            std::fabs(
                world.GetComponent<KalistaFateCallComponent>(kalista).fRemainingSec -
                humanCarryRemainingBefore) > 0.0001f)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: human oathsworn auto-launched without marker-gated click\n");
            return false;
        }

        GameCommand launch{};
        launch.kind = eCommandKind::Move;
        launch.issuerEntity = ally;
        launch.groundPos = Vec3{ 8.f, 0.f, 0.f };
        launch.direction = Vec3{ 1.f, 0.f, 0.f };
        launch.sequenceNum = 6u;
        TickContext staleMoveTick =
            MakeProbeTickContext(185ull, rng, entityMap, walkable);
        launch.issuedAtTick = staleMoveTick.tickIndex;
        executor->ExecuteCommand(world, staleMoveTick, launch);
        if (world.GetComponent<KalistaFateCallComponent>(kalista).eStage !=
                eKalistaFateCallStage::Carrying ||
            !world.GetComponent<KalistaFateCallCarriedComponent>(ally).bHidden)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: delayed ordinary Move launched carried ally\n");
            return false;
        }

        launch.itemId = kKalistaFateCallLaunchCommandMarker;
        launch.sequenceNum = 7u;
        TickContext launchTick =
            MakeProbeTickContext(186ull, rng, entityMap, walkable);
        launch.issuedAtTick = launchTick.tickIndex;
        executor->ExecuteCommand(world, launchTick, launch);
        if (world.GetComponent<KalistaFateCallComponent>(kalista).eStage !=
                eKalistaFateCallStage::Launching ||
            world.GetComponent<KalistaFateCallCarriedComponent>(ally).bHidden ||
            !world.HasComponent<ActionStateComponent>(kalista) ||
            world.GetComponent<ActionStateComponent>(kalista).actionId !=
                static_cast<u16_t>(eActionStateId::SkillR) ||
            world.GetComponent<ActionStateComponent>(kalista).stage != 2u ||
            world.GetComponent<ActionStateComponent>(kalista).movePolicy !=
                eSkillActionMovePolicy::QueueUntilUnlock ||
            world.GetComponent<ActionStateComponent>(kalista).lockEndTick <=
                launchTick.tickIndex ||
            world.GetComponent<SkillStateComponent>(kalista).slots[
                static_cast<u8_t>(eSkillSlot::R)].currentStage != 0u)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: carried player Move did not begin visible launch\n");
            return false;
        }

        TickContext collisionTick =
            MakeProbeTickContext(187ull, rng, entityMap, walkable);
        KalistaGameSim::Tick(world, collisionTick);
        if (!world.HasComponent<ForcedMotionComponent>(enemy) ||
            CountStatusEffects(
                world,
                enemy,
                eStatusEffectId::GenericAirborne,
                kalista) != 1u ||
            GameplayStateQuery::CanMove(world, enemy) ||
            world.HasComponent<KalistaFateCallComponent>(kalista) ||
            world.HasComponent<KalistaFateCallCarriedComponent>(ally))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: launch collision did not apply/clean up airborne\n");
            return false;
        }

        const ChampionScoreComponent& kalistaScoreAfter =
            world.GetComponent<ChampionScoreComponent>(kalista);
        const ChampionScoreComponent& allyScoreAfter =
            world.GetComponent<ChampionScoreComponent>(ally);
        if (std::fabs(world.GetComponent<HealthComponent>(ally).fCurrent -
                allyHpBefore) > 0.0001f ||
            world.GetComponent<HealthComponent>(ally).bIsDead ||
            world.GetComponent<GoldComponent>(kalista).amount != kalistaGoldBefore ||
            world.GetComponent<GoldComponent>(ally).amount != allyGoldBefore ||
            kalistaScoreAfter.iKills != kalistaScoreBefore.iKills ||
            kalistaScoreAfter.iDeaths != kalistaScoreBefore.iDeaths ||
            kalistaScoreAfter.iAssists != kalistaScoreBefore.iAssists ||
            allyScoreAfter.iKills != allyScoreBefore.iKills ||
            allyScoreAfter.iDeaths != allyScoreBefore.iDeaths ||
            allyScoreAfter.iAssists != allyScoreBefore.iAssists)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: ritual/carry mutated health, gold, or score rewards\n");
            return false;
        }

        CWorld botWorld;
        DeterministicRng botRng(20260713ull);
        EntityIdMap botEntityMap;
        FlatWalkable botWalkable;
        auto botExecutor = CDefaultCommandExecutor::Create();
        const EntityID botKalista = SpawnChampion(
            botWorld, botEntityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID botAlly = SpawnChampion(
            botWorld, botEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        botWorld.GetComponent<TransformComponent>(botKalista).SetPosition(Vec3{});
        botWorld.GetComponent<TransformComponent>(botAlly).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        ChampionAIComponent botKalistaAI{};
        botKalistaAI.champion = eChampion::KALISTA;
        botKalistaAI.team = eTeam::Blue;
        botWorld.AddComponent<ChampionAIComponent>(botKalista, botKalistaAI);
        auto& botInventory = botWorld.GetComponent<InventoryComponent>(botKalista);
        botInventory.itemIds[kKalistaOathswornInventorySlot] =
            kKalistaOathswornItemId;
        botInventory.count = InventoryComponent::kMaxSlots;
        botWorld.GetComponent<SkillRankComponent>(botKalista).ranks[
            static_cast<u8_t>(eSkillSlot::R)] = 1u;

        TickContext botBindTick = MakeProbeTickContext(
            1ull, botRng, botEntityMap, botWalkable);
        std::vector<GameCommand> botContractCommands;
        CChampionAISystem::Execute(
            botWorld,
            botBindTick,
            botContractCommands);
        if (botContractCommands.size() != 1u ||
            botContractCommands[0].kind != eCommandKind::UseItem ||
            botContractCommands[0].itemId != kKalistaOathswornItemId ||
            botContractCommands[0].targetEntity != botAlly)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: bot Kalista did not choose oathsworn contract\n");
            return false;
        }
        botExecutor->ExecuteCommand(
            botWorld,
            botBindTick,
            botContractCommands[0]);
        ChampionAIComponent botAllyAI{};
        botAllyAI.champion = eChampion::JAX;
        botAllyAI.team = eTeam::Blue;
        botWorld.AddComponent<ChampionAIComponent>(botAlly, botAllyAI);
        for (u64_t tick = 2ull; tick <= 50ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick, botRng, botEntityMap, botWalkable);
            GameplayStatus::TickStatusEffects(botWorld, tc);
            KalistaGameSim::Tick(botWorld, tc);
        }

        GameCommand botR{};
        botR.kind = eCommandKind::CastSkill;
        botR.issuerEntity = botKalista;
        botR.slot = static_cast<u8_t>(eSkillSlot::R);
        botR.direction = Vec3{ 1.f, 0.f, 0.f };
        botR.sequenceNum = 2u;
        TickContext botRTick = MakeProbeTickContext(
            51ull, botRng, botEntityMap, botWalkable);
        botR.issuedAtTick = botRTick.tickIndex;
        botExecutor->ExecuteCommand(botWorld, botRTick, botR);

        bool_t bSawHiddenCarrying = false;
        bool_t bBotAutoLaunched = false;
        for (u64_t tick = 52ull; tick <= 170ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick, botRng, botEntityMap, botWalkable);
            GameplayStatus::TickStatusEffects(botWorld, tc);
            KalistaGameSim::Tick(botWorld, tc);
            if (!botWorld.HasComponent<KalistaFateCallComponent>(botKalista))
                break;

            const auto& state =
                botWorld.GetComponent<KalistaFateCallComponent>(botKalista);
            if (state.eStage == eKalistaFateCallStage::Carrying &&
                botWorld.HasComponent<KalistaFateCallCarriedComponent>(botAlly) &&
                botWorld.GetComponent<KalistaFateCallCarriedComponent>(
                    botAlly).bHidden)
            {
                bSawHiddenCarrying = true;
            }
            if (state.eStage == eKalistaFateCallStage::Launching)
            {
                bBotAutoLaunched = true;
                break;
            }
        }
        if (!bSawHiddenCarrying || !bBotAutoLaunched ||
            !botWorld.HasComponent<KalistaFateCallCarriedComponent>(botAlly) ||
            botWorld.GetComponent<KalistaFateCallCarriedComponent>(
                botAlly).bHidden)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: AI oathsworn did not auto-launch after carry window\n");
            return false;
        }

        CWorld orphanWorld;
        DeterministicRng orphanRng(20260716ull);
        EntityIdMap orphanEntityMap;
        FlatWalkable orphanWalkable;
        auto orphanExecutor = CDefaultCommandExecutor::Create();
        const EntityID disconnectedKalista = SpawnChampion(
            orphanWorld, orphanEntityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID orphanAlly = SpawnChampion(
            orphanWorld, orphanEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        auto& orphanInventory =
            orphanWorld.GetComponent<InventoryComponent>(disconnectedKalista);
        orphanInventory.itemIds[kKalistaOathswornInventorySlot] =
            kKalistaOathswornItemId;
        orphanInventory.count = InventoryComponent::kMaxSlots;

        GameCommand orphanBind{};
        orphanBind.kind = eCommandKind::UseItem;
        orphanBind.issuerEntity = disconnectedKalista;
        orphanBind.targetEntity = orphanAlly;
        orphanBind.itemId = kKalistaOathswornItemId;
        orphanBind.sequenceNum = 1u;
        TickContext orphanBindTick = MakeProbeTickContext(
            1ull, orphanRng, orphanEntityMap, orphanWalkable);
        orphanBind.issuedAtTick = orphanBindTick.tickIndex;
        orphanExecutor->ExecuteCommand(orphanWorld, orphanBindTick, orphanBind);
        orphanWorld.DestroyEntity(disconnectedKalista);
        TickContext orphanCleanupTick = MakeProbeTickContext(
            2ull, orphanRng, orphanEntityMap, orphanWalkable);
        KalistaGameSim::Tick(orphanWorld, orphanCleanupTick);
        if (orphanWorld.HasComponent<KalistaOathswornByComponent>(orphanAlly) ||
            CountStatusEffects(
                orphanWorld,
                orphanAlly,
                eStatusEffectId::KalistaOathswornRitual,
                disconnectedKalista) != 0u ||
            !GameplayStateQuery::CanMove(orphanWorld, orphanAlly) ||
            !GameplayStateQuery::CanAttack(orphanWorld, orphanAlly) ||
            !GameplayStateQuery::CanCast(orphanWorld, orphanAlly))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: disconnected owner left an orphan reverse bond\n");
            return false;
        }

        CWorld carryOrphanWorld;
        DeterministicRng carryOrphanRng(20260717ull);
        EntityIdMap carryOrphanEntityMap;
        FlatWalkable carryOrphanWalkable;
        auto carryOrphanExecutor = CDefaultCommandExecutor::Create();
        const EntityID carryOwner = SpawnChampion(
            carryOrphanWorld, carryOrphanEntityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID carriedOrphan = SpawnChampion(
            carryOrphanWorld, carryOrphanEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        carryOrphanWorld.GetComponent<TransformComponent>(carryOwner).SetPosition(
            Vec3{});
        carryOrphanWorld.GetComponent<TransformComponent>(carriedOrphan).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        auto& carryOrphanInventory =
            carryOrphanWorld.GetComponent<InventoryComponent>(carryOwner);
        carryOrphanInventory.itemIds[kKalistaOathswornInventorySlot] =
            kKalistaOathswornItemId;
        carryOrphanInventory.count = InventoryComponent::kMaxSlots;
        carryOrphanWorld.GetComponent<SkillRankComponent>(carryOwner).ranks[
            static_cast<u8_t>(eSkillSlot::R)] = 1u;

        GameCommand carryOrphanBind{};
        carryOrphanBind.kind = eCommandKind::UseItem;
        carryOrphanBind.issuerEntity = carryOwner;
        carryOrphanBind.targetEntity = carriedOrphan;
        carryOrphanBind.itemId = kKalistaOathswornItemId;
        carryOrphanBind.sequenceNum = 1u;
        TickContext carryOrphanBindTick = MakeProbeTickContext(
            1ull, carryOrphanRng, carryOrphanEntityMap, carryOrphanWalkable);
        carryOrphanBind.issuedAtTick = carryOrphanBindTick.tickIndex;
        carryOrphanExecutor->ExecuteCommand(
            carryOrphanWorld,
            carryOrphanBindTick,
            carryOrphanBind);
        for (u64_t tick = 2ull; tick <= 50ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick,
                carryOrphanRng,
                carryOrphanEntityMap,
                carryOrphanWalkable);
            GameplayStatus::TickStatusEffects(carryOrphanWorld, tc);
            KalistaGameSim::Tick(carryOrphanWorld, tc);
        }

        GameCommand carryOrphanR{};
        carryOrphanR.kind = eCommandKind::CastSkill;
        carryOrphanR.issuerEntity = carryOwner;
        carryOrphanR.slot = static_cast<u8_t>(eSkillSlot::R);
        carryOrphanR.sequenceNum = 2u;
        TickContext carryOrphanRTick = MakeProbeTickContext(
            51ull, carryOrphanRng, carryOrphanEntityMap, carryOrphanWalkable);
        carryOrphanR.issuedAtTick = carryOrphanRTick.tickIndex;
        carryOrphanExecutor->ExecuteCommand(
            carryOrphanWorld,
            carryOrphanRTick,
            carryOrphanR);
        for (u64_t tick = 52ull; tick <= 60ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(
                tick,
                carryOrphanRng,
                carryOrphanEntityMap,
                carryOrphanWalkable);
            GameplayStatus::TickStatusEffects(carryOrphanWorld, tc);
            KalistaGameSim::Tick(carryOrphanWorld, tc);
        }
        if (!carryOrphanWorld.HasComponent<KalistaFateCallCarriedComponent>(
                carriedOrphan) ||
            !carryOrphanWorld.GetComponent<KalistaFateCallCarriedComponent>(
                carriedOrphan).bHidden)
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: active-carry orphan fixture never hid ally\n");
            return false;
        }

        carryOrphanWorld.DestroyEntity(carryOwner);
        TickContext carryOrphanCleanupTick = MakeProbeTickContext(
            61ull, carryOrphanRng, carryOrphanEntityMap, carryOrphanWalkable);
        KalistaGameSim::Tick(carryOrphanWorld, carryOrphanCleanupTick);
        if (carryOrphanWorld.HasComponent<KalistaFateCallCarriedComponent>(
                carriedOrphan) ||
            CountStatusEffects(
                carryOrphanWorld,
                carriedOrphan,
                eStatusEffectId::KalistaFateCallUntargetable,
                carryOwner) != 0u ||
            !GameplayStateQuery::CanMove(carryOrphanWorld, carriedOrphan) ||
            !GameplayStateQuery::CanAttack(carryOrphanWorld, carriedOrphan) ||
            !GameplayStateQuery::CanCast(carryOrphanWorld, carriedOrphan))
        {
            std::printf(
                "[SimLab][KalistaR] FAIL: destroyed R owner left hidden carried orphan\n");
            return false;
        }

        std::printf(
            "[SimLab][KalistaR] PASS: authority gates, ritual, Bound, explicit player/AI launch, airborne, rewards, orphan cleanup\n");
        return true;
    }

    bool_t RunCombatActionGenerationProbe()
    {
        CWorld world;
        DeterministicRng rng(20260721ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID attacker = SpawnChampion(
            world,
            entityMap,
            eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID target = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Red),
            5u);
        world.GetComponent<TransformComponent>(attacker).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(target).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(attacker).ranks[
            static_cast<u8_t>(eSkillSlot::E)] = 1u;
        const f32_t targetHpBefore =
            world.GetComponent<HealthComponent>(target).fCurrent;

        TickContext attackTick =
            MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand attack{};
        attack.kind = eCommandKind::BasicAttack;
        attack.issuerEntity = attacker;
        attack.targetEntity = target;
        attack.direction = Vec3{ 1.f, 0.f, 0.f };
        attack.sequenceNum = 1u;
        attack.issuedAtTick = attackTick.tickIndex;
        executor->ExecuteCommand(world, attackTick, attack);

        if (!world.HasComponent<CombatActionComponent>(attacker) ||
            !world.HasComponent<ActionStateComponent>(attacker))
        {
            std::printf(
                "[SimLab][ActionGeneration] FAIL: BasicAttack did not create action state\n");
            return false;
        }

        const CombatActionComponent armedAction =
            world.GetComponent<CombatActionComponent>(attacker);
        if (armedAction.uOwnerActionSequence == 0u ||
            armedAction.uOwnerActionSequence !=
                world.GetComponent<ActionStateComponent>(attacker).sequence)
        {
            std::printf(
                "[SimLab][ActionGeneration] FAIL: BasicAttack owner generation was not captured\n");
            return false;
        }

        TickContext castTick = MakeProbeTickContext(
            attackTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        GameCommand cast{};
        cast.kind = eCommandKind::CastSkill;
        cast.issuerEntity = attacker;
        cast.slot = static_cast<u8_t>(eSkillSlot::E);
        cast.sequenceNum = 2u;
        cast.issuedAtTick = castTick.tickIndex;
        executor->ExecuteCommand(world, castTick, cast);
        const auto& replacementAction =
            world.GetComponent<ActionStateComponent>(attacker);
        if (replacementAction.sequence == armedAction.uOwnerActionSequence ||
            replacementAction.sourceSlot !=
                static_cast<u8_t>(eSkillSlot::E))
        {
            std::printf(
                "[SimLab][ActionGeneration] FAIL: real CastSkill did not replace BasicAttack generation\n");
            return false;
        }

        TickContext moveTick = MakeProbeTickContext(
            attackTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        GameCommand move{};
        move.kind = eCommandKind::Move;
        move.issuerEntity = attacker;
        move.groundPos = Vec3{ 5.f, 0.f, 0.f };
        move.direction = Vec3{ 1.f, 0.f, 0.f };
        move.sequenceNum = 3u;
        move.issuedAtTick = moveTick.tickIndex;
        executor->ExecuteCommand(world, moveTick, move);
        if (!world.HasComponent<MoveTargetComponent>(attacker) ||
            world.HasComponent<CombatActionComponent>(attacker))
        {
            std::printf(
                "[SimLab][ActionGeneration] FAIL: Move was consumed by stale BasicAttack generation\n");
            return false;
        }

        TickContext impactTick = MakeProbeTickContext(
            armedAction.uImpactTick,
            rng,
            entityMap,
            walkable);
        CCombatActionSystem::Execute(world, impactTick);

        u32_t pendingDamageCount = 0u;
        world.ForEach<DamageRequestComponent>(
            [&](EntityID, DamageRequestComponent&)
            {
                ++pendingDamageCount;
            });
        if (world.HasComponent<CombatActionComponent>(attacker) ||
            pendingDamageCount != 0u ||
            std::fabs(
                world.GetComponent<HealthComponent>(target).fCurrent -
                targetHpBefore) > 0.0001f)
        {
            std::printf(
                "[SimLab][ActionGeneration] FAIL: stale BA survived replacement pendingDamage=%u\n",
                pendingDamageCount);
            return false;
        }

        std::printf(
            "[SimLab][ActionGeneration] PASS: stale BasicAttack impact discarded after action replacement\n");
        return true;
    }

    bool_t RunKalistaProjectileAuthorityProbe()
    {
        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };
        const auto FindProjectile = [](
            CWorld& world,
            eProjectileKind kind,
            EntityID source)
        {
            EntityID result = NULL_ENTITY;
            world.ForEach<SkillProjectileComponent>(
                [&](EntityID entity, SkillProjectileComponent& projectile)
                {
                    if (result == NULL_ENTITY &&
                        projectile.kind == kind &&
                        projectile.sourceEntity == source)
                    {
                        result = entity;
                    }
                });
            return result;
        };
        const auto CountPendingDamage = [](CWorld& world)
        {
            u32_t count = 0u;
            world.ForEach<DamageRequestComponent>(
                [&](EntityID, DamageRequestComponent&)
                {
                    ++count;
                });
            return count;
        };

        CWorld world;
        DeterministicRng rng(2026072301ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID kalista = SpawnChampion(
            world, entityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID jax = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::YASUO,
            static_cast<u8_t>(eTeam::Red), 5u);
        world.GetComponent<TransformComponent>(kalista).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(jax).SetPosition(
            Vec3{ 0.f, 0.f, 1.f });
        world.GetComponent<TransformComponent>(target).SetPosition(
            Vec3{ 5.f, 0.f, 0.f });

        TickContext tc =
            MakeProbeTickContext(10ull, rng, entityMap, walkable);
        DamageRequest attack{};
        attack.source = kalista;
        attack.target = target;
        attack.sourceTeam = eTeam::Blue;
        attack.type = eDamageType::Physical;
        attack.flatAmount = 75.f;
        attack.rank = 1u;
        attack.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        attack.eSourceKind = eDamageSourceKind::BasicAttack;
        attack.flags =
            DamageFlag_OnHit |
            DamageFlag_CanCrit |
            DamageFlag_CanLifesteal;

        if (KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, jax, target, attack))
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: non-Kalista BA entered Kalista projectile path\n");
            return false;
        }
        if (!KalistaGameSim::TryLaunchBasicAttackProjectile(
            world, tc, kalista, target, attack))
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: Kalista BA did not launch projectile\n");
            return false;
        }

        const EntityID baEntity = FindProjectile(
            world, eProjectileKind::KalistaBasicAttack, kalista);
        if (baEntity == NULL_ENTITY)
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: Kalista BA projectile component missing\n");
            return false;
        }
        const auto& ba = world.GetComponent<SkillProjectileComponent>(baEntity);
        if (ba.targetEntity != target ||
            world.ResolveEntity(ba.targetHandle) != target ||
            !NearlyEqual(ba.speed, 30.f) ||
            ba.maxDistance < (std::numeric_limits<f32_t>::max)() * 0.5f ||
            ba.unitHitPolicy != eProjectileUnitHitPolicy::Destroy ||
            ba.bCollidesWithTerrain ||
            !ba.bPersistAfterSourceDeath ||
            !ba.bApplyDamageOnHit ||
            ba.damageSourceKind != eDamageSourceKind::BasicAttack ||
            ba.sourceSlot != static_cast<u8_t>(eSkillSlot::BasicAttack) ||
            CountPendingDamage(world) != 0u)
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: BA homing/deferred-damage contract mismatch\n");
            return false;
        }

        GameCommand qCommand{};
        qCommand.kind = eCommandKind::CastSkill;
        qCommand.issuerEntity = kalista;
        qCommand.slot = static_cast<u8_t>(eSkillSlot::Q);
        qCommand.direction = Vec3{ 0.f, 0.f, 1.f };
        qCommand.groundPos = Vec3{ 0.f, 0.f, 16.5f };
        qCommand.sequenceNum = 1u;
        qCommand.issuedAtTick = tc.tickIndex;

        GameplayHookContext qContext{};
        qContext.pWorld = &world;
        qContext.casterEntity = kalista;
        qContext.casterTeam = eTeam::Blue;
        qContext.casterChampion = eChampion::KALISTA;
        qContext.skillRank = 1u;
        qContext.pCommand = &qCommand;
        qContext.pTickCtx = &tc;
        if (!CGameplayHookRegistry::Instance().Dispatch(
            MakeGameplayHookId(
                eChampion::KALISTA,
                GameplayHookVariant::Q_CastFrame),
            qContext))
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: Kalista Q hook was not registered\n");
            return false;
        }

        const f32_t canonicalQRange = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            kalista,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::Q));
        const EntityID qEntity = FindProjectile(
            world, eProjectileKind::KalistaPierce, kalista);
        if (qEntity == NULL_ENTITY)
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: Kalista Q projectile component missing\n");
            return false;
        }
        const auto& q = world.GetComponent<SkillProjectileComponent>(qEntity);
        if (!NearlyEqual(canonicalQRange, 16.5f) ||
            q.targetEntity != NULL_ENTITY ||
            q.targetHandle.IsValid() ||
            !NearlyEqual(q.speed, 27.f) ||
            !NearlyEqual(q.maxDistance, 16.5f) ||
            !NearlyEqual(q.direction.x, 0.f) ||
            !NearlyEqual(q.direction.z, 1.f) ||
            q.unitHitPolicy != eProjectileUnitHitPolicy::Destroy ||
            q.maxUniqueHits != 1u ||
            q.bCollidesWithTerrain ||
            q.damageSourceKind != eDamageSourceKind::Skill ||
            q.sourceSlot != static_cast<u8_t>(eSkillSlot::Q) ||
            CountPendingDamage(world) != 0u)
        {
            std::printf(
                "[SimLab][KalistaProjectile] FAIL: Q speed/range/straight-first-hit contract mismatch\n");
            return false;
        }

        std::printf(
            "[SimLab][KalistaProjectile] PASS: BA 30-speed homing/deferred impact; Q 27-speed 16.5-range straight first-hit\n");
        return true;
    }

    bool_t RunKalistaSentinelAuthorityProbe()
    {
        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };

        CWorld world;
        DeterministicRng rng(2026072401ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID kalista = SpawnChampion(
            world, entityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID enemy = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Red), 5u);
        world.GetComponent<TransformComponent>(kalista).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(enemy).SetPosition(
            Vec3{ 6.f, 0.f, 0.f });
        world.GetComponent<SkillRankComponent>(kalista).ranks[
            static_cast<u8_t>(eSkillSlot::W)] = 1u;

        TickContext castTick =
            MakeProbeTickContext(100ull, rng, entityMap, walkable);
        GameCommand cast{};
        cast.kind = eCommandKind::CastSkill;
        cast.issuerEntity = kalista;
        cast.slot = static_cast<u8_t>(eSkillSlot::W);
        cast.groundPos = Vec3{ 12.f, 0.f, 0.f };
        cast.direction = Vec3{ 1.f, 0.f, 0.f };
        cast.sequenceNum = 1u;
        cast.issuedAtTick = castTick.tickIndex;

        const CommandExecutionResult castResult =
            executor->ExecuteCommand(world, castTick, cast);
        EntityID sentinel = NULL_ENTITY;
        u32_t sentinelCount = 0u;
        world.ForEach<KalistaSentinelComponent>(
            [&](EntityID entity, KalistaSentinelComponent&)
            {
                sentinel = entity;
                ++sentinelCount;
            });
        if (castResult.state != eCommandExecutionState::Accepted ||
            sentinelCount != 1u || sentinel == NULL_ENTITY)
        {
            std::printf(
                "[SimLab][KalistaSentinel] FAIL: accepted W did not create exactly one sentinel (result=%u count=%u)\n",
                static_cast<unsigned>(castResult.state),
                sentinelCount);
            return false;
        }

        const auto& state = world.GetComponent<KalistaSentinelComponent>(sentinel);
        const auto& transform = world.GetComponent<TransformComponent>(sentinel);
        const auto& spatial = world.GetComponent<SpatialAgentComponent>(sentinel);
        const auto& vision = world.GetComponent<VisionSourceComponent>(sentinel);
        const auto& cone = world.GetComponent<VisionConeComponent>(sentinel);
        const NetEntityId sentinelNet = entityMap.ToNet(sentinel);
        if (state.owner != kalista ||
            state.team != eTeam::Blue ||
            !NearlyEqual(state.start.x, 0.f) ||
            !NearlyEqual(state.end.x, 12.f) ||
            !NearlyEqual(state.lifetimeSec, 12.f) ||
            !NearlyEqual(state.patrolSpeed, 3.5f) ||
            !NearlyEqual(state.sightRange, 10.f) ||
            !NearlyEqual(state.halfAngleCos, 0.8660254f) ||
            !NearlyEqual(transform.GetPosition().x, 0.f) ||
            spatial.kind != eSpatialKind::Sensor ||
            spatial.team != static_cast<u8_t>(eTeam::Blue) ||
            !NearlyEqual(vision.sightRange, state.sightRange) ||
            !NearlyEqual(cone.forward.x, 1.f) ||
            !NearlyEqual(cone.forward.z, 0.f) ||
            !NearlyEqual(cone.halfAngleCos, state.halfAngleCos) ||
            sentinelNet == NULL_NET_ENTITY ||
            !world.HasComponent<NetEntityIdComponent>(sentinel))
        {
            std::printf(
                "[SimLab][KalistaSentinel] FAIL: spawned sentinel authority/replication state mismatch\n");
            return false;
        }

        ChampionAIComponent ai{};
        ai.champion = eChampion::KALISTA;
        ai.team = eTeam::Blue;
        ai.lane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        ai.activeLane = ai.lane;
        ai.state = eChampionAIState::LaneCombat;
        ai.intent = eChampionAIIntent::AttackChampion;
        ai.laneGoal = Vec3{ 8.f, 0.f, 0.f };
        ai.safeAnchor = Vec3{ -8.f, 0.f, 0.f };
        ai.retreatGoal = ai.safeAnchor;
        world.AddComponent<ChampionAIComponent>(kalista, ai);
        world.GetComponent<VisionSourceComponent>(kalista).sightRange = 0.f;
        const NetEntityId enemyNet = entityMap.ToNet(enemy);

        const auto IsObservedAt = [&](const Vec3& targetPos, u64_t tick)
        {
            world.GetComponent<TransformComponent>(enemy).SetPosition(targetPos);
            auto& aiState = world.GetComponent<ChampionAIComponent>(kalista);
            aiState.lockedChampion = NULL_ENTITY;
            aiState.comboTarget = NULL_ENTITY;
            aiState.diveTarget = NULL_ENTITY;
            aiState.divePhase = eChampionAIDivePhase::None;
            aiState.decisionTimer = 0.f;
            aiState.intentHoldTimer = 0.f;
            std::vector<GameCommand> commands;
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            CChampionAISystem::Execute(world, tc, commands);
            if (!world.HasComponent<ChampionAIResearchDebugComponent>(kalista))
                return false;
            const auto& observation =
                world.GetComponent<ChampionAIResearchDebugComponent>(kalista)
                    .decisionDraft.observation;
            return observation.enemyChampionNetEntityId == enemyNet;
        };

        if (!IsObservedAt(Vec3{ 6.f, 0.f, 0.f }, 101ull) ||
            IsObservedAt(Vec3{ 0.f, 0.f, 6.f }, 102ull) ||
            IsObservedAt(Vec3{ -6.f, 0.f, 0.f }, 103ull) ||
            IsObservedAt(Vec3{ 10.25f, 0.f, 0.f }, 104ull))
        {
            std::printf(
                "[SimLab][KalistaSentinel] FAIL: server perception did not enforce sentinel angle/range cone\n");
            return false;
        }

        bool_t bMoved = false;
        bool_t bReversed = false;
        for (u64_t tick = castTick.tickIndex;
            tick < castTick.tickIndex + 359ull;
            ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            KalistaGameSim::Tick(world, tc);
            if (!world.IsAlive(sentinel))
            {
                std::printf(
                    "[SimLab][KalistaSentinel] FAIL: sentinel expired before 12s/360-tick lifetime\n");
                return false;
            }

            const auto& liveState =
                world.GetComponent<KalistaSentinelComponent>(sentinel);
            const Vec3 pos =
                world.GetComponent<TransformComponent>(sentinel).GetPosition();
            bMoved = bMoved || pos.x > 0.01f;
            bReversed = bReversed || liveState.forward.x < -0.99f;
            if (pos.x < -0.001f || pos.x > 12.001f)
            {
                std::printf(
                    "[SimLab][KalistaSentinel] FAIL: patrol escaped authored segment x=%.3f\n",
                    pos.x);
                return false;
            }
        }

        if (!bMoved || !bReversed || entityMap.FromNet(sentinelNet) != sentinel)
        {
            std::printf(
                "[SimLab][KalistaSentinel] FAIL: patrol movement/reversal or live net binding missing\n");
            return false;
        }

        TickContext expiryTick = MakeProbeTickContext(
            castTick.tickIndex + 359ull,
            rng,
            entityMap,
            walkable);
        KalistaGameSim::Tick(world, expiryTick);
        if (world.IsAlive(sentinel) ||
            entityMap.FromNet(sentinelNet) != NULL_ENTITY)
        {
            std::printf(
                "[SimLab][KalistaSentinel] FAIL: sentinel or net binding survived 12s/360 ticks\n");
            return false;
        }

        std::printf(
            "[SimLab][KalistaSentinel] PASS: W command spawn, 360-tick patrol lifetime, net cleanup, server cone perception\n");
        return true;
    }

    bool_t RunEzrealProjectileAuthorityProbe()
    {
        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };
        const auto FindProjectile = [](
            CWorld& world,
            eProjectileKind kind,
            EntityID source)
        {
            EntityID result = NULL_ENTITY;
            world.ForEach<SkillProjectileComponent>(
                [&](EntityID entity, SkillProjectileComponent& projectile)
                {
                    if (result == NULL_ENTITY &&
                        projectile.kind == kind &&
                        projectile.sourceEntity == source)
                    {
                        result = entity;
                    }
                });
            return result;
        };
        const auto CountMarks = [](
            CWorld& world,
            EntityID source,
            EntityID target)
        {
            u32_t count = 0u;
            world.ForEach<EzrealEssenceFluxMarkComponent>(
                [&](EntityID, EzrealEssenceFluxMarkComponent& mark)
                {
                    if (world.ResolveEntity(mark.hSource) == source &&
                        world.ResolveEntity(mark.hTarget) == target)
                    {
                        ++count;
                    }
                });
            return count;
        };
        const auto CountPendingDamage = [](CWorld& world)
        {
            u32_t count = 0u;
            world.ForEach<DamageRequestComponent>(
                [&](EntityID, DamageRequestComponent&)
                {
                    ++count;
                });
            return count;
        };

        // Basic attacks enter this projectile path only for Ezreal.
        {
            CWorld world;
            DeterministicRng rng(2026072201ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID jax = SpawnChampion(
                world, entityMap, eChampion::JAX,
                static_cast<u8_t>(eTeam::Blue), 1u);
            const EntityID target = SpawnChampion(
                world, entityMap, eChampion::YASUO,
                static_cast<u8_t>(eTeam::Red), 5u);
            world.GetComponent<TransformComponent>(ezreal).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(jax).SetPosition(
                Vec3{ 0.f, 0.f, 1.f });
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 5.f, 0.f, 0.f });

            DamageRequest attack{};
            attack.sourceTeam = eTeam::Blue;
            attack.type = eDamageType::Physical;
            attack.flatAmount = 75.f;
            attack.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
            attack.eSourceKind = eDamageSourceKind::BasicAttack;
            attack.flags = DamageFlag_OnHit;
            TickContext tc =
                MakeProbeTickContext(10ull, rng, entityMap, walkable);

            attack.source = jax;
            attack.target = target;
            if (EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, jax, target, attack))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: non-Ezreal BA entered Ezreal projectile path\n");
                return false;
            }

            attack.source = ezreal;
            if (!EzrealGameSim::TryLaunchBasicAttackProjectile(
                world, tc, ezreal, target, attack))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Ezreal BA did not launch projectile\n");
                return false;
            }

            const EntityID projectileEntity = FindProjectile(
                world, eProjectileKind::EzrealBasicAttack, ezreal);
            if (projectileEntity == NULL_ENTITY)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Ezreal BA projectile component missing\n");
                return false;
            }
            const auto& projectile =
                world.GetComponent<SkillProjectileComponent>(projectileEntity);
            if (projectile.targetEntity != target ||
                projectile.unitHitPolicy != eProjectileUnitHitPolicy::Destroy)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Ezreal BA was not targeted/destroy-on-hit\n");
                return false;
            }
        }

        // W is source-owned and damage-free on attach. Q/BA consume it once.
        {
            CWorld world;
            DeterministicRng rng(2026072202ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID otherEzreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 1u);
            const EntityID target = SpawnChampion(
                world, entityMap, eChampion::JAX,
                static_cast<u8_t>(eTeam::Red), 5u);
            TickContext tc =
                MakeProbeTickContext(20ull, rng, entityMap, walkable);

            SkillProjectileComponent wProjectile{};
            wProjectile.sourceEntity = ezreal;
            wProjectile.sourceTeam = eTeam::Blue;
            wProjectile.kind = eProjectileKind::EssenceFlux;
            wProjectile.rank = 1u;
            wProjectile.sourceSlot = static_cast<u8_t>(eSkillSlot::W);
            wProjectile.bApplyDamageOnHit = false;

            DamageRequest outDamage{};
            bool_t bEnqueue = true;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, wProjectile, target, outDamage, bEnqueue) ||
                bEnqueue ||
                CountPendingDamage(world) != 0u ||
                CountMarks(world, ezreal, target) != 1u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W1 damage/source-owned mark contract\n");
                return false;
            }

            auto& skills = world.GetComponent<SkillStateComponent>(ezreal);
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::R);
                ++slot)
            {
                skills.slots[slot].cooldownRemaining = 10.f;
            }
            auto& champion = world.GetComponent<ChampionComponent>(ezreal);
            champion.mana = 0.f;

            SkillProjectileComponent qProjectile{};
            qProjectile.sourceEntity = ezreal;
            qProjectile.sourceTeam = eTeam::Blue;
            qProjectile.kind = eProjectileKind::MysticShot;
            qProjectile.rank = 1u;
            qProjectile.sourceSlot = static_cast<u8_t>(eSkillSlot::Q);
            qProjectile.damage = 20.f;
            qProjectile.totalAdRatio = 1.3f;
            qProjectile.apRatio = 0.4f;
            qProjectile.damageType = eDamageType::Physical;
            qProjectile.damageSourceKind = eDamageSourceKind::Skill;
            qProjectile.damageFlags = DamageFlag_OnHit;
            qProjectile.bApplyDamageOnHit = true;
            qProjectile.paidManaCost = 40.f;

            SkillProjectileComponent otherQ = qProjectile;
            otherQ.sourceEntity = otherEzreal;
            outDamage = {};
            bEnqueue = false;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, otherQ, target, outDamage, bEnqueue) ||
                !bEnqueue ||
                CountMarks(world, ezreal, target) != 1u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: another Ezreal consumed source-owned W mark\n");
                return false;
            }

            outDamage = {};
            bEnqueue = false;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, qProjectile, target, outDamage, bEnqueue) ||
                !bEnqueue ||
                outDamage.iSourceSlot != static_cast<u8_t>(eSkillSlot::W) ||
                outDamage.type != eDamageType::Magic ||
                CountMarks(world, ezreal, target) != 0u ||
                CountPendingDamage(world) != 1u ||
                !NearlyEqual(champion.mana, 100.f))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Q did not consume W once/refund 60+actual-paid-40 (mana=%.3f)\n",
                    champion.mana);
                return false;
            }
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::R);
                ++slot)
            {
                if (!NearlyEqual(skills.slots[slot].cooldownRemaining, 8.5f))
                {
                    std::printf(
                        "[SimLab][EzrealProjectile] FAIL: Q cooldown refund slot=%u value=%.3f\n",
                        static_cast<unsigned>(slot),
                        skills.slots[slot].cooldownRemaining);
                    return false;
                }
            }

            outDamage = {};
            bEnqueue = true;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, wProjectile, target, outDamage, bEnqueue) ||
                bEnqueue ||
                CountMarks(world, ezreal, target) != 1u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W mark could not be reattached for BA case\n");
                return false;
            }
            champion.mana = 0.f;

            SkillProjectileComponent baProjectile{};
            baProjectile.sourceEntity = ezreal;
            baProjectile.sourceTeam = eTeam::Blue;
            baProjectile.targetEntity = target;
            baProjectile.kind = eProjectileKind::EzrealBasicAttack;
            baProjectile.rank = 1u;
            baProjectile.sourceSlot =
                static_cast<u8_t>(eSkillSlot::BasicAttack);
            baProjectile.damage = 75.f;
            baProjectile.damageType = eDamageType::Physical;
            baProjectile.damageSourceKind = eDamageSourceKind::BasicAttack;
            baProjectile.damageFlags = DamageFlag_OnHit;
            baProjectile.bApplyDamageOnHit = true;

            outDamage = {};
            bEnqueue = false;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, baProjectile, target, outDamage, bEnqueue) ||
                !bEnqueue ||
                outDamage.iSourceSlot != static_cast<u8_t>(eSkillSlot::W) ||
                CountMarks(world, ezreal, target) != 0u ||
                !NearlyEqual(champion.mana, 0.f))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: BA W2 incorrectly restored mana (mana=%.3f)\n",
                    champion.mana);
                return false;
            }

            const f32_t manaAfterFirstConsume = champion.mana;
            outDamage = {};
            bEnqueue = false;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, baProjectile, target, outDamage, bEnqueue) ||
                !bEnqueue ||
                outDamage.iSourceSlot !=
                    static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                !NearlyEqual(champion.mana, manaAfterFirstConsume))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: consumed W mark detonated twice\n");
                return false;
            }

            outDamage = {};
            bEnqueue = true;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, wProjectile, target, outDamage, bEnqueue) ||
                CountMarks(world, ezreal, target) != 1u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W mark could not be armed for expiry\n");
                return false;
            }
            TickContext expireTick = MakeProbeTickContext(
                tc.tickIndex + 120u,
                rng,
                entityMap,
                walkable);
            EzrealGameSim::Tick(world, expireTick);
            if (CountMarks(world, ezreal, target) != 0u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W mark did not expire at 4s boundary\n");
                return false;
            }
        }

        // R's non-epic branch changes only flat damage.
        {
            CWorld world;
            DeterministicRng rng(2026072203ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID minion = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 5.f, 0.f, 0.f });
            TickContext tc =
                MakeProbeTickContext(30ull, rng, entityMap, walkable);

            SkillProjectileComponent rProjectile{};
            rProjectile.sourceEntity = ezreal;
            rProjectile.sourceTeam = eTeam::Blue;
            rProjectile.kind = eProjectileKind::GlobalBeam;
            rProjectile.rank = 1u;
            rProjectile.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
            rProjectile.damage = 350.f;
            rProjectile.bonusAdRatio = 1.f;
            rProjectile.apRatio = 1.1f;
            rProjectile.damageType = eDamageType::Magic;
            rProjectile.damageSourceKind = eDamageSourceKind::Skill;
            rProjectile.bApplyDamageOnHit = true;

            DamageRequest outDamage{};
            bool_t bEnqueue = false;
            if (!EzrealGameSim::HandleProjectileHit(
                    world, tc, rProjectile, minion, outDamage, bEnqueue) ||
                !bEnqueue ||
                !NearlyEqual(outDamage.flatAmount, 150.f) ||
                !NearlyEqual(outDamage.bonusAdRatioOverride, 1.f) ||
                !NearlyEqual(outDamage.apRatioOverride, 1.1f))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: R non-epic scaling flat=%.3f bAD=%.3f AP=%.3f\n",
                    outDamage.flatAmount,
                    outDamage.bonusAdRatioOverride,
                    outDamage.apRatioOverride);
                return false;
            }
        }

        // Command acceptance must arm a pending Q and launch the authored policy.
        {
            CWorld world;
            DeterministicRng rng(2026072204ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            world.GetComponent<TransformComponent>(ezreal).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::Q)] = 5u;
            world.GetComponent<ChampionComponent>(ezreal).mana = 100.f;

            TickContext castTick =
                MakeProbeTickContext(100ull, rng, entityMap, walkable);
            GameCommand qCast{};
            qCast.kind = eCommandKind::CastSkill;
            qCast.issuerEntity = ezreal;
            qCast.slot = static_cast<u8_t>(eSkillSlot::Q);
            qCast.direction = Vec3{ 1.f, 0.f, 0.f };
            qCast.groundPos = Vec3{ 12.f, 0.f, 0.f };
            qCast.sequenceNum = 1u;
            qCast.issuedAtTick = castTick.tickIndex;
            executor->ExecuteCommand(world, castTick, qCast);

            if (!world.HasComponent<EzrealPendingCastComponent>(ezreal))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: command did not arm pending Q\n");
                return false;
            }
            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(ezreal);
            if (pending.uRank != 5u ||
                !NearlyEqual(pending.fPaidManaCost, 40.f) ||
                !NearlyEqual(
                    world.GetComponent<ChampionComponent>(ezreal).mana,
                    60.f))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Q accept-time mana snapshot rank=%u paid=%.3f remaining=%.3f\n",
                    static_cast<unsigned>(pending.uRank),
                    pending.fPaidManaCost,
                    world.GetComponent<ChampionComponent>(ezreal).mana);
                return false;
            }
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::Q)] = 1u;
            TickContext launchTick = MakeProbeTickContext(
                pending.uLaunchTick, rng, entityMap, walkable);
            EzrealGameSim::Tick(world, launchTick);

            const EntityID projectileEntity = FindProjectile(
                world, eProjectileKind::MysticShot, ezreal);
            if (projectileEntity == NULL_ENTITY ||
                world.HasComponent<EzrealPendingCastComponent>(ezreal))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: pending Q did not launch/clear\n");
                return false;
            }
            const auto& projectile =
                world.GetComponent<SkillProjectileComponent>(projectileEntity);
            if (projectile.unitHitPolicy != eProjectileUnitHitPolicy::Destroy ||
                projectile.bCollidesWithTerrain ||
                !projectile.bBlockedByProjectileBarriers ||
                !NearlyEqual(projectile.paidManaCost, 40.f))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: Q policy destroy=%u terrain=%u barrier=%u paid=%.3f\n",
                    projectile.unitHitPolicy == eProjectileUnitHitPolicy::Destroy ? 1u : 0u,
                    projectile.bCollidesWithTerrain ? 1u : 0u,
                    projectile.bBlockedByProjectileBarriers ? 1u : 0u,
                    projectile.paidManaCost);
                return false;
            }
        }

        // W acceptance must not stall the authoritative timeline. Movement
        // entered during its short QueueUntilUnlock window is retained and
        // released on the exact unlock/launch tick.
        {
            CWorld world;
            DeterministicRng rng(2026072210ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            world.GetComponent<TransformComponent>(ezreal).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::W)] = 1u;
            world.GetComponent<ChampionComponent>(ezreal).mana = 100.f;

            TickContext castTick =
                MakeProbeTickContext(500ull, rng, entityMap, walkable);
            GameCommand wCast{};
            wCast.kind = eCommandKind::CastSkill;
            wCast.issuerEntity = ezreal;
            wCast.slot = static_cast<u8_t>(eSkillSlot::W);
            wCast.direction = Vec3{ 1.f, 0.f, 0.f };
            wCast.groundPos = Vec3{ 12.f, 0.f, 0.f };
            wCast.sequenceNum = 1u;
            wCast.issuedAtTick = castTick.tickIndex;
            const CommandExecutionResult castResult =
                executor->ExecuteCommand(world, castTick, wCast);
            if (castResult.state != eCommandExecutionState::Accepted ||
                !world.HasComponent<ActionStateComponent>(ezreal) ||
                !world.HasComponent<EzrealPendingCastComponent>(ezreal))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W accept did not arm action/pending state\n");
                return false;
            }

            const ActionStateComponent acceptedAction =
                world.GetComponent<ActionStateComponent>(ezreal);
            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(ezreal);
            if (acceptedAction.movePolicy !=
                    eSkillActionMovePolicy::QueueUntilUnlock ||
                acceptedAction.lockEndTick != pending.uLaunchTick ||
                acceptedAction.lockEndTick != castTick.tickIndex + 8u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W lock/launch mismatch policy=%u lock=%llu launch=%llu\n",
                    static_cast<unsigned>(acceptedAction.movePolicy),
                    static_cast<unsigned long long>(acceptedAction.lockEndTick),
                    static_cast<unsigned long long>(pending.uLaunchTick));
                return false;
            }

            TickContext moveTick = MakeProbeTickContext(
                castTick.tickIndex + 1u, rng, entityMap, walkable);
            GameCommand move{};
            move.kind = eCommandKind::Move;
            move.issuerEntity = ezreal;
            move.groundPos = Vec3{ 5.f, 0.f, 0.f };
            move.direction = Vec3{ 1.f, 0.f, 0.f };
            move.sequenceNum = 2u;
            move.issuedAtTick = moveTick.tickIndex;
            const CommandExecutionResult moveResult =
                executor->ExecuteCommand(world, moveTick, move);
            if (moveResult.state != eCommandExecutionState::Accepted ||
                !world.GetComponent<ActionStateComponent>(ezreal).bHasQueuedMove)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W-window Move was not retained\n");
                return false;
            }

            for (u64_t tick = moveTick.tickIndex;
                tick <= acceptedAction.lockEndTick;
                ++tick)
            {
                TickContext tc =
                    MakeProbeTickContext(tick, rng, entityMap, walkable);
                EzrealGameSim::Tick(world, tc);
                CMoveSystem::Execute(world, tc);
            }

            const Vec3 movedPosition =
                world.GetComponent<TransformComponent>(ezreal).GetPosition();
            const ActionStateComponent& releasedAction =
                world.GetComponent<ActionStateComponent>(ezreal);
            if (world.HasComponent<EzrealPendingCastComponent>(ezreal) ||
                FindProjectile(world, eProjectileKind::EssenceFlux, ezreal) ==
                    NULL_ENTITY ||
                releasedAction.bHasQueuedMove ||
                movedPosition.x <= 0.f)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: W unlock stalled pending=%u queued=%u movedX=%.3f\n",
                    world.HasComponent<EzrealPendingCastComponent>(ezreal) ? 1u : 0u,
                    releasedAction.bHasQueuedMove ? 1u : 0u,
                    movedPosition.x);
                return false;
            }
        }

        // E must prefer this Ezreal's W-mark, including structures, over a closer minion.
        const auto RunArcaneShiftPriorityCase = [&FindProjectile](bool_t bStructure)
        {
            CWorld world;
            DeterministicRng rng(bStructure ? 2026072206ull : 2026072205ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID markedTarget = bStructure
                ? SpawnStatusProbeTarget(
                    world,
                    GameplayStateQuery::eGameplayTargetKind::Structure,
                    eTeam::Red,
                    Vec3{ 10.f, 0.f, 0.f })
                : SpawnChampion(
                    world, entityMap, eChampion::JAX,
                    static_cast<u8_t>(eTeam::Red), 5u);
            const EntityID closerMinion = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 5.25f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(ezreal).SetPosition(Vec3{});
            if (!bStructure)
            {
                world.GetComponent<TransformComponent>(markedTarget).SetPosition(
                    Vec3{ 10.f, 0.f, 0.f });
            }
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::E)] = 1u;

            TickContext markTick =
                MakeProbeTickContext(200ull, rng, entityMap, walkable);
            SkillProjectileComponent wProjectile{};
            wProjectile.sourceEntity = ezreal;
            wProjectile.sourceTeam = eTeam::Blue;
            wProjectile.kind = eProjectileKind::EssenceFlux;
            wProjectile.rank = 1u;
            wProjectile.sourceSlot = static_cast<u8_t>(eSkillSlot::W);
            wProjectile.bApplyDamageOnHit = false;
            DamageRequest outDamage{};
            bool_t bEnqueue = true;
            if (!EzrealGameSim::HandleProjectileHit(
                    world,
                    markTick,
                    wProjectile,
                    markedTarget,
                    outDamage,
                    bEnqueue))
            {
                return false;
            }

            GameCommand eCast{};
            eCast.kind = eCommandKind::CastSkill;
            eCast.issuerEntity = ezreal;
            eCast.slot = static_cast<u8_t>(eSkillSlot::E);
            eCast.direction = Vec3{ 1.f, 0.f, 0.f };
            eCast.groundPos = Vec3{ 4.75f, 0.f, 0.f };
            eCast.sequenceNum = 1u;
            eCast.issuedAtTick = markTick.tickIndex;
            executor->ExecuteCommand(world, markTick, eCast);
            if (!world.HasComponent<EzrealPendingCastComponent>(ezreal))
                return false;

            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(ezreal);
            TickContext launchTick = MakeProbeTickContext(
                pending.uLaunchTick, rng, entityMap, walkable);
            EzrealGameSim::Tick(world, launchTick);
            const EntityID boltEntity = FindProjectile(
                world, eProjectileKind::ArcaneShiftBolt, ezreal);
            if (boltEntity == NULL_ENTITY)
                return false;

            const auto& bolt =
                world.GetComponent<SkillProjectileComponent>(boltEntity);
            return closerMinion != markedTarget &&
                bolt.targetEntity == markedTarget;
        };
        if (!RunArcaneShiftPriorityCase(false) ||
            !RunArcaneShiftPriorityCase(true))
        {
            std::printf(
                "[SimLab][EzrealProjectile] FAIL: E did not prioritize own W champion/structure over closer minion\n");
            return false;
        }

        // E clamps long requests to 4.75, preserves short requests, and walks
        // backward to the nearest valid landing without treating the blink
        // path itself as continuous movement.
        struct ArcaneShiftWalkable final : IWalkableQuery
        {
            f32_t fMaxWalkableX = 100.f;

            bool_t IsWalkableXZ(const Vec3& pos) const override
            {
                return pos.x <= fMaxWalkableX + 0.0001f;
            }
            bool_t SegmentWalkableXZ(
                const Vec3&,
                const Vec3&,
                f32_t) const override
            {
                return true;
            }
            bool_t TryClampMoveSegmentXZ(
                const Vec3&,
                const Vec3& desired,
                f32_t,
                Vec3& outPosition) const override
            {
                outPosition = desired;
                return true;
            }
            bool_t TryResolveMoveTarget(
                const Vec3&,
                const Vec3& rawTarget,
                Vec3& outTarget) const override
            {
                outTarget = rawTarget;
                return true;
            }
            bool_t TryBuildMovePath(
                const Vec3&,
                const Vec3& rawTarget,
                Vec3* pOutWaypoints,
                u16_t maxWaypoints,
                u16_t& outWaypointCount,
                Vec3& outTarget) const override
            {
                outTarget = rawTarget;
                outWaypointCount = maxWaypoints > 0u ? 1u : 0u;
                if (outWaypointCount > 0u && pOutWaypoints)
                    pOutWaypoints[0] = rawTarget;
                return true;
            }
            bool_t TrySampleHeight(
                f32_t,
                f32_t,
                f32_t& outY) const override
            {
                outY = 0.f;
                return true;
            }
        };
        const auto RunArcaneShiftLandingCase = [&NearlyEqual](
            f32_t requestedX,
            f32_t maxWalkableX,
            f32_t expectedMinX,
            f32_t expectedMaxX)
        {
            CWorld world;
            DeterministicRng rng(2026072300ull +
                static_cast<u64_t>(requestedX * 100.f));
            EntityIdMap entityMap;
            ArcaneShiftWalkable walkable{};
            walkable.fMaxWalkableX = maxWalkableX;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            world.GetComponent<TransformComponent>(ezreal).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::E)] = 1u;

            TickContext castTick = MakeProbeTickContext(
                250ull, rng, entityMap, walkable);
            GameCommand cast{};
            cast.kind = eCommandKind::CastSkill;
            cast.issuerEntity = ezreal;
            cast.slot = static_cast<u8_t>(eSkillSlot::E);
            cast.groundPos = Vec3{ requestedX, 0.f, 0.f };
            cast.direction = Vec3{ 1.f, 0.f, 0.f };
            cast.sequenceNum = 1u;
            cast.issuedAtTick = castTick.tickIndex;
            executor->ExecuteCommand(world, castTick, cast);
            if (!world.HasComponent<EzrealPendingCastComponent>(ezreal))
                return false;

            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(ezreal);
            TickContext launchTick = MakeProbeTickContext(
                pending.uLaunchTick, rng, entityMap, walkable);
            EzrealGameSim::Tick(world, launchTick);
            const Vec3 destination =
                world.GetComponent<TransformComponent>(ezreal).GetPosition();
            if (destination.x < expectedMinX - 0.0001f ||
                destination.x > expectedMaxX + 0.0001f ||
                !NearlyEqual(destination.z, 0.f) ||
                !world.HasComponent<PositionDiscontinuityComponent>(ezreal) ||
                world.GetComponent<PositionDiscontinuityComponent>(ezreal).uTick !=
                    launchTick.tickIndex)
            {
                return false;
            }
            return true;
        };
        if (!RunArcaneShiftLandingCase(10.f, 100.f, 4.75f, 4.75f) ||
            !RunArcaneShiftLandingCase(2.f, 100.f, 2.f, 2.f) ||
            !RunArcaneShiftLandingCase(10.f, 3.f, 2.8f, 3.f) ||
            !RunArcaneShiftLandingCase(10.f, 0.f, 0.f, 0.f))
        {
            std::printf(
                "[SimLab][EzrealProjectile] FAIL: E clamp/partial/landing correction/discontinuity contract\n");
            return false;
        }

        // R launches from the cast-start muzzle even if Ezreal moves during wind-up.
        {
            CWorld world;
            DeterministicRng rng(2026072207ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const Vec3 castStart{ 2.f, 0.f, 3.f };
            world.GetComponent<TransformComponent>(ezreal).SetPosition(castStart);
            world.GetComponent<SkillRankComponent>(ezreal).ranks[
                static_cast<u8_t>(eSkillSlot::R)] = 1u;

            TickContext castTick =
                MakeProbeTickContext(300ull, rng, entityMap, walkable);
            GameCommand rCast{};
            rCast.kind = eCommandKind::CastSkill;
            rCast.issuerEntity = ezreal;
            rCast.slot = static_cast<u8_t>(eSkillSlot::R);
            rCast.direction = Vec3{ 1.f, 0.f, 0.f };
            rCast.groundPos = Vec3{ 250.f, 0.f, 3.f };
            rCast.sequenceNum = 1u;
            rCast.issuedAtTick = castTick.tickIndex;
            executor->ExecuteCommand(world, castTick, rCast);
            if (!world.HasComponent<EzrealPendingCastComponent>(ezreal))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: command did not arm pending R\n");
                return false;
            }

            const EzrealPendingCastComponent pending =
                world.GetComponent<EzrealPendingCastComponent>(ezreal);
            const Vec3 expectedOrigin{ castStart.x, castStart.y + 1.f, castStart.z };
            if (!NearlyEqual(pending.vOrigin.x, expectedOrigin.x) ||
                !NearlyEqual(pending.vOrigin.y, expectedOrigin.y) ||
                !NearlyEqual(pending.vOrigin.z, expectedOrigin.z))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: R pending origin was not cast-start muzzle\n");
                return false;
            }

            world.GetComponent<TransformComponent>(ezreal).SetPosition(
                Vec3{ 20.f, 0.f, 30.f });
            TickContext launchTick = MakeProbeTickContext(
                pending.uLaunchTick, rng, entityMap, walkable);
            EzrealGameSim::Tick(world, launchTick);
            const EntityID projectileEntity = FindProjectile(
                world, eProjectileKind::GlobalBeam, ezreal);
            if (projectileEntity == NULL_ENTITY)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: pending R did not launch\n");
                return false;
            }

            const auto& projectile =
                world.GetComponent<SkillProjectileComponent>(projectileEntity);
            if (projectile.unitHitPolicy != eProjectileUnitHitPolicy::Pierce ||
                !NearlyEqual(projectile.currentPos.x, expectedOrigin.x) ||
                !NearlyEqual(projectile.currentPos.y, expectedOrigin.y) ||
                !NearlyEqual(projectile.currentPos.z, expectedOrigin.z))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: R origin/pierce origin=(%.3f,%.3f,%.3f)\n",
                    projectile.currentPos.x,
                    projectile.currentPos.y,
                    projectile.currentPos.z);
                return false;
            }
        }

        // Rising Spell Force is ability-hit owned, capped at five, and expires
        // on the exact [T, T + 180) tick boundary. Basic attacks neither add
        // nor refresh it.
        {
            CWorld world;
            DeterministicRng rng(2026072208ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID ezreal = SpawnChampion(
                world, entityMap, eChampion::EZREAL,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID targetA = SpawnChampion(
                world, entityMap, eChampion::JAX,
                static_cast<u8_t>(eTeam::Red), 5u);
            const EntityID targetB = SpawnChampion(
                world, entityMap, eChampion::YASUO,
                static_cast<u8_t>(eTeam::Red), 6u);
            if (world.HasComponent<RuneLoadoutComponent>(ezreal))
                world.RemoveComponent<RuneLoadoutComponent>(ezreal);
            if (world.HasComponent<RuneRuntimeComponent>(ezreal))
                world.RemoveComponent<RuneRuntimeComponent>(ezreal);

            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            const f32_t baseAttackSpeed =
                world.GetComponent<StatComponent>(ezreal).attackSpeed;
            TickContext hitTick = MakeProbeTickContext(
                400ull, rng, entityMap, walkable);

            const auto ApplyContact = [&world, &hitTick, ezreal](
                eProjectileKind kind,
                EntityID target)
            {
                SkillProjectileComponent projectile{};
                projectile.sourceEntity = ezreal;
                projectile.sourceTeam = eTeam::Blue;
                projectile.targetEntity = target;
                projectile.kind = kind;
                projectile.rank = 1u;
                projectile.sourceSlot = kind == eProjectileKind::EssenceFlux
                    ? static_cast<u8_t>(eSkillSlot::W)
                    : kind == eProjectileKind::ArcaneShiftBolt
                        ? static_cast<u8_t>(eSkillSlot::E)
                        : kind == eProjectileKind::GlobalBeam
                            ? static_cast<u8_t>(eSkillSlot::R)
                            : kind == eProjectileKind::EzrealBasicAttack
                                ? static_cast<u8_t>(eSkillSlot::BasicAttack)
                                : static_cast<u8_t>(eSkillSlot::Q);
                projectile.damageSourceKind =
                    kind == eProjectileKind::EzrealBasicAttack
                        ? eDamageSourceKind::BasicAttack
                        : eDamageSourceKind::Skill;
                projectile.bApplyDamageOnHit =
                    kind != eProjectileKind::EssenceFlux;
                DamageRequest damage{};
                bool_t bEnqueue = false;
                return EzrealGameSim::HandleProjectileHit(
                    world,
                    hitTick,
                    projectile,
                    target,
                    damage,
                    bEnqueue);
            };
            const auto FindPassive = [&world, ezreal]() -> const BuffInstance*
            {
                if (!world.HasComponent<BuffComponent>(ezreal))
                    return nullptr;
                const BuffComponent& buffs =
                    world.GetComponent<BuffComponent>(ezreal);
                for (u8_t i = 0u;
                    i < buffs.count && i < BuffComponent::kMaxBuffs;
                    ++i)
                {
                    if (buffs.buffs[i].buffDefId ==
                        kEzrealRisingSpellForceBuffDefId)
                    {
                        return &buffs.buffs[i];
                    }
                }
                return nullptr;
            };

            if (!ApplyContact(eProjectileKind::EzrealBasicAttack, targetA) ||
                FindPassive() != nullptr)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: BA added Rising Spell Force\n");
                return false;
            }

            if (!ApplyContact(eProjectileKind::GlobalBeam, targetA) ||
                !ApplyContact(eProjectileKind::GlobalBeam, targetB) ||
                !ApplyContact(eProjectileKind::MysticShot, targetA) ||
                !ApplyContact(eProjectileKind::EssenceFlux, targetB) ||
                !ApplyContact(eProjectileKind::ArcaneShiftBolt, targetA) ||
                !ApplyContact(eProjectileKind::MysticShot, targetB))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: passive ability contact was rejected\n");
                return false;
            }

            const BuffInstance* passive = FindPassive();
            if (!passive ||
                passive->stackCount != 5u ||
                !NearlyEqual(passive->bonusAttackSpeedPerStack, 0.10f) ||
                passive->uExpireTick != hitTick.tickIndex + 180u)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: passive stack/cap/expiry contract stacks=%u expire=%llu\n",
                    passive ? static_cast<unsigned>(passive->stackCount) : 0u,
                    passive
                        ? static_cast<unsigned long long>(passive->uExpireTick)
                        : 0ull);
                return false;
            }

            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            if (world.GetComponent<StatComponent>(ezreal).attackSpeed <=
                baseAttackSpeed)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: passive stacks did not affect generic StatSystem\n");
                return false;
            }

            const u64_t expireTick = passive->uExpireTick;
            if (!ApplyContact(eProjectileKind::EzrealBasicAttack, targetA) ||
                !FindPassive() ||
                FindPassive()->uExpireTick != expireTick)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: BA refreshed Rising Spell Force\n");
                return false;
            }

            TickContext beforeExpiry = MakeProbeTickContext(
                expireTick - 1u, rng, entityMap, walkable);
            if (CBuffSystem::PruneExpiredTickBuffs(world, beforeExpiry) ||
                FindPassive() == nullptr)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: passive expired before T+180\n");
                return false;
            }

            TickContext atExpiry = MakeProbeTickContext(
                expireTick, rng, entityMap, walkable);
            if (!CBuffSystem::PruneExpiredTickBuffs(world, atExpiry) ||
                FindPassive() != nullptr)
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: passive did not expire at T+180\n");
                return false;
            }
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            if (!NearlyEqual(
                    world.GetComponent<StatComponent>(ezreal).attackSpeed,
                    baseAttackSpeed))
            {
                std::printf(
                    "[SimLab][EzrealProjectile] FAIL: expired passive remained in attack speed\n");
                return false;
            }
        }

        std::printf(
            "[SimLab][EzrealProjectile] PASS: BA routing, paid mana, W/Q/BA, passive, R scaling, pending policies, E priority, R origin\n");
        return true;
    }

    bool_t RunChampionAIStateGateCommitmentProbe()
    {
        auto ConfigureCombatAI = [](
            CWorld& world,
            EntityID self,
            EntityID target,
            eChampion champion)
        {
            ChampionAIComponent ai{};
            ai.champion = champion;
            ai.team = eTeam::Blue;
            ai.lane = 0u;
            ai.state = eChampionAIState::LaneCombat;
            ai.intent = eChampionAIIntent::AttackChampion;
            ai.lockedChampion = target;
            ai.comboTarget = target;
            ai.comboStep = 1u;
            ai.diveTarget = target;
            ai.divePhase = eChampionAIDivePhase::ArmW;
            ai.decisionTimer = 0.f;
            ai.intentHoldTimer = 0.f;
            world.AddComponent<ChampionAIComponent>(self, ai);

            auto& ranks = world.GetComponent<SkillRankComponent>(self);
            ranks.ranks[static_cast<u8_t>(eSkillSlot::Q)] = 1u;
            ranks.ranks[static_cast<u8_t>(eSkillSlot::W)] = 1u;
            ranks.ranks[static_cast<u8_t>(eSkillSlot::E)] = 1u;
            ranks.ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;
        };

        CWorld blockedWorld;
        DeterministicRng blockedRng(20260714ull);
        EntityIdMap blockedEntityMap;
        FlatWalkable blockedWalkable;
        const EntityID stunnedAI = SpawnChampion(
            blockedWorld, blockedEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID airborneAI = SpawnChampion(
            blockedWorld, blockedEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 1u);
        const EntityID blockedEnemy = SpawnChampion(
            blockedWorld, blockedEntityMap, eChampion::RIVEN,
            static_cast<u8_t>(eTeam::Red), 5u);
        blockedWorld.GetComponent<TransformComponent>(stunnedAI).SetPosition(Vec3{});
        blockedWorld.GetComponent<TransformComponent>(airborneAI).SetPosition(
            Vec3{ 0.f, 0.f, 0.5f });
        blockedWorld.GetComponent<TransformComponent>(blockedEnemy).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        ConfigureCombatAI(
            blockedWorld, stunnedAI, blockedEnemy, eChampion::JAX);
        ConfigureCombatAI(
            blockedWorld, airborneAI, blockedEnemy, eChampion::JAX);

        TickContext blockedTick = MakeProbeTickContext(
            1ull, blockedRng, blockedEntityMap, blockedWalkable);
        const StatusEffectApplyDesc stun = GameplayStatus::MakeStunDesc(
            blockedEnemy,
            eChampion::RIVEN,
            eSkillSlot::W,
            1.f);
        if (!GameplayStatus::TryApplyStatusEffect(
                blockedWorld, stunnedAI, stun, blockedTick) ||
            !GameplayStatus::ApplyAirborne(
                blockedWorld,
                blockedTick,
                airborneAI,
                blockedEnemy,
                eChampion::RIVEN,
                eSkillSlot::Q,
                1.f,
                1.f))
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: CC fixtures were rejected\n");
            return false;
        }

        std::vector<GameCommand> blockedCommands;
        CChampionAISystem::Execute(blockedWorld, blockedTick, blockedCommands);
        const ChampionAIComponent& stunnedState =
            blockedWorld.GetComponent<ChampionAIComponent>(stunnedAI);
        const ChampionAIComponent& airborneState =
            blockedWorld.GetComponent<ChampionAIComponent>(airborneAI);
        if (!blockedCommands.empty() ||
            stunnedState.comboStep != 1u ||
            airborneState.comboStep != 1u ||
            stunnedState.divePhase != eChampionAIDivePhase::ArmW ||
            airborneState.divePhase != eChampionAIDivePhase::ArmW ||
            stunnedState.debugLastBlockReason !=
                eChampionAIDecisionBlockReason::StateBlocked ||
            airborneState.debugLastBlockReason !=
                eChampionAIDecisionBlockReason::StateBlocked)
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: CC emitted=%zu stun(step=%u,dive=%u,block=%u) airborne(step=%u,dive=%u,block=%u)\n",
                blockedCommands.size(),
                static_cast<unsigned>(stunnedState.comboStep),
                static_cast<unsigned>(stunnedState.divePhase),
                static_cast<unsigned>(stunnedState.debugLastBlockReason),
                static_cast<unsigned>(airborneState.comboStep),
                static_cast<unsigned>(airborneState.divePhase),
                static_cast<unsigned>(airborneState.debugLastBlockReason));
            return false;
        }

        CWorld commitmentWorld;
        DeterministicRng commitmentRng(20260715ull);
        EntityIdMap commitmentEntityMap;
        FlatWalkable commitmentWalkable;
        const EntityID ashe = SpawnChampion(
            commitmentWorld, commitmentEntityMap, eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID commitmentEnemy = SpawnChampion(
            commitmentWorld, commitmentEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Red), 5u);
        const EntityID farmMinion = SpawnStatusProbeTarget(
            commitmentWorld,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            eTeam::Red,
            Vec3{ 0.5f, 0.f, 0.5f });
        commitmentWorld.GetComponent<TransformComponent>(ashe).SetPosition(Vec3{});
        commitmentWorld.GetComponent<TransformComponent>(commitmentEnemy).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });
        commitmentWorld.GetComponent<HealthComponent>(ashe).fCurrent =
            commitmentWorld.GetComponent<HealthComponent>(ashe).fMaximum * 0.50f;
        ConfigureCombatAI(
            commitmentWorld, ashe, commitmentEnemy, eChampion::ASHE);
        auto& commitmentAI =
            commitmentWorld.GetComponent<ChampionAIComponent>(ashe);
        commitmentAI.diveTarget = NULL_ENTITY;
        commitmentAI.divePhase = eChampionAIDivePhase::None;
        commitmentAI.comboStep = 2u;
        commitmentAI.fSkillCastCooldownTimer = 5.f;

        TickContext commitmentTick = MakeProbeTickContext(
            1ull, commitmentRng, commitmentEntityMap, commitmentWalkable);
        std::vector<GameCommand> commitmentCommands;
        CChampionAISystem::Execute(
            commitmentWorld, commitmentTick, commitmentCommands);
        if (farmMinion == NULL_ENTITY ||
            commitmentAI.fFarmDecisionScore <=
                commitmentAI.fChampionDecisionScore ||
            commitmentAI.intent != eChampionAIIntent::AttackChampion ||
            commitmentAI.comboTarget != commitmentEnemy ||
            commitmentAI.comboStep != 3u ||
            commitmentAI.fSkillCastCooldownTimer <= 0.f ||
            commitmentCommands.size() != 1u ||
            commitmentCommands[0].kind != eCommandKind::BasicAttack ||
            commitmentCommands[0].targetEntity != commitmentEnemy)
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: combo commitment farm=%.3f fight=%.3f intent=%u step=%u emitted=%zu\n",
                commitmentAI.fFarmDecisionScore,
                commitmentAI.fChampionDecisionScore,
                static_cast<unsigned>(commitmentAI.intent),
                static_cast<unsigned>(commitmentAI.comboStep),
                commitmentCommands.size());
            return false;
        }

        CWorld cadenceWorld;
        DeterministicRng cadenceRng(20260718ull);
        EntityIdMap cadenceEntityMap;
        FlatWalkable cadenceWalkable;
        const EntityID cadenceKalista = SpawnChampion(
            cadenceWorld, cadenceEntityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID cadenceEnemy = SpawnChampion(
            cadenceWorld, cadenceEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Red), 5u);
        cadenceWorld.GetComponent<TransformComponent>(cadenceKalista).SetPosition(
            Vec3{});
        cadenceWorld.GetComponent<TransformComponent>(cadenceEnemy).SetPosition(
            Vec3{ 3.f, 0.f, 0.f });
        ChampionAIComponent cadenceAI{};
        cadenceAI.champion = eChampion::KALISTA;
        cadenceAI.team = eTeam::Blue;
        cadenceAI.state = eChampionAIState::LaneCombat;
        cadenceAI.decisionTimer = 1.f;
        cadenceAI.intentHoldTimer = 1.f;
        cadenceWorld.AddComponent<ChampionAIComponent>(cadenceKalista, cadenceAI);
        TickContext cadenceTick = MakeProbeTickContext(
            1ull, cadenceRng, cadenceEntityMap, cadenceWalkable);
        std::vector<GameCommand> cadenceCommands;
        CChampionAISystem::Execute(cadenceWorld, cadenceTick, cadenceCommands);
        const auto& cadenceState =
            cadenceWorld.GetComponent<ChampionAIComponent>(cadenceKalista);
        const f32_t expectedHold = 1.f - cadenceTick.fDt;
        if (!cadenceCommands.empty() ||
            std::fabs(cadenceState.retreatHpRatio - 0.45f) > 0.0001f ||
            std::fabs(cadenceState.reengageHpRatio - 0.65f) > 0.0001f ||
            std::fabs(cadenceState.intentHoldTimer - expectedHold) > 0.0001f)
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: profile/cadence retreat=%.3f reengage=%.3f hold=%.3f expected=%.3f\n",
                cadenceState.retreatHpRatio,
                cadenceState.reengageHpRatio,
                cadenceState.intentHoldTimer,
                expectedHold);
            return false;
        }

        CWorld diveWorld;
        DeterministicRng diveRng(20260719ull);
        EntityIdMap diveEntityMap;
        FlatWalkable diveWalkable;
        const EntityID diveJax = SpawnChampion(
            diveWorld, diveEntityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID diveEnemy = SpawnChampion(
            diveWorld, diveEntityMap, eChampion::RIVEN,
            static_cast<u8_t>(eTeam::Red), 5u);
        diveWorld.GetComponent<TransformComponent>(diveJax).SetPosition(Vec3{});
        diveWorld.GetComponent<TransformComponent>(diveEnemy).SetPosition(
            Vec3{ 6.f, 0.f, 0.f });
        ConfigureCombatAI(diveWorld, diveJax, diveEnemy, eChampion::JAX);
        auto& stalledDive = diveWorld.GetComponent<ChampionAIComponent>(diveJax);
        stalledDive.divePhase = eChampionAIDivePhase::BasicAttack;
        stalledDive.fDiveExtraBATimer = 0.f;
        stalledDive.decisionTimer = 0.f;
        TickContext diveTick = MakeProbeTickContext(
            1ull, diveRng, diveEntityMap, diveWalkable);
        std::vector<GameCommand> diveCommands;
        CChampionAISystem::Execute(diveWorld, diveTick, diveCommands);
        if (stalledDive.divePhase == eChampionAIDivePhase::BasicAttack ||
            stalledDive.diveTarget != NULL_ENTITY)
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: expired Jax dive remained stuck phase=%u target=%u\n",
                static_cast<unsigned>(stalledDive.divePhase),
                static_cast<unsigned>(stalledDive.diveTarget));
            return false;
        }

        CWorld noOathWorld;
        DeterministicRng noOathRng(20260720ull);
        EntityIdMap noOathEntityMap;
        FlatWalkable noOathWalkable;
        const EntityID noOathKalista = SpawnChampion(
            noOathWorld, noOathEntityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID noOathEnemy = SpawnChampion(
            noOathWorld, noOathEntityMap, eChampion::RIVEN,
            static_cast<u8_t>(eTeam::Red), 5u);
        noOathWorld.GetComponent<TransformComponent>(noOathKalista).SetPosition(
            Vec3{});
        noOathWorld.GetComponent<TransformComponent>(noOathEnemy).SetPosition(
            Vec3{ 2.f, 0.f, 0.f });
        ConfigureCombatAI(
            noOathWorld,
            noOathKalista,
            noOathEnemy,
            eChampion::KALISTA);
        auto& noOathAI =
            noOathWorld.GetComponent<ChampionAIComponent>(noOathKalista);
        noOathAI.diveTarget = NULL_ENTITY;
        noOathAI.divePhase = eChampionAIDivePhase::None;
        noOathAI.comboStep = 4u;
        noOathWorld.GetComponent<HealthComponent>(noOathEnemy).fCurrent =
            noOathWorld.GetComponent<HealthComponent>(noOathEnemy).fMaximum * 0.5f;
        TickContext noOathTick = MakeProbeTickContext(
            1ull, noOathRng, noOathEntityMap, noOathWalkable);
        std::vector<GameCommand> noOathCommands;
        CChampionAISystem::Execute(noOathWorld, noOathTick, noOathCommands);
        const bool_t bEmittedRejectedR = std::any_of(
            noOathCommands.begin(),
            noOathCommands.end(),
            [](const GameCommand& command)
            {
                return command.kind == eCommandKind::CastSkill &&
                    command.slot == static_cast<u8_t>(eSkillSlot::R);
            });
        if (bEmittedRejectedR ||
            noOathWorld.HasComponent<KalistaFateCallComponent>(noOathKalista))
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: no-oath Kalista committed rejected R\n");
            return false;
        }

        CWorld recallWorld;
        DeterministicRng recallRng(20260721ull);
        EntityIdMap recallEntityMap;
        FlatWalkable recallWalkable;
        const EntityID recallBot = SpawnChampion(
            recallWorld,
            recallEntityMap,
            eChampion::GAREN,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const Vec3 recallAnchor =
            recallWorld.GetComponent<TransformComponent>(recallBot).GetPosition();
        ChampionAIComponent recallAI{};
        recallAI.champion = eChampion::GAREN;
        recallAI.team = eTeam::Blue;
        recallAI.lane = 0u;
        recallAI.state = eChampionAIState::Retreat;
        recallAI.intent = eChampionAIIntent::Retreat;
        recallAI.retreatGoal = recallAnchor;
        recallAI.safeAnchor = recallAnchor;
        recallAI.decisionTimer = 0.f;
        recallWorld.AddComponent<ChampionAIComponent>(recallBot, recallAI);
        auto& recallHealth = recallWorld.GetComponent<HealthComponent>(recallBot);
        recallHealth.fCurrent = recallHealth.fMaximum * 0.05f;
        TickContext recallTick = MakeProbeTickContext(
            1ull, recallRng, recallEntityMap, recallWalkable);
        std::vector<GameCommand> recallCommands;
        CChampionAISystem::Execute(recallWorld, recallTick, recallCommands);
        auto recallExecutor = CDefaultCommandExecutor::Create();
        const CommandExecutionResult recallResult = recallCommands.size() == 1u
            ? recallExecutor->ExecuteCommand(recallWorld, recallTick, recallCommands[0])
            : CommandExecutionResult::Unknown(0u);
        const bool_t bRecallActive =
            recallWorld.HasComponent<RecallComponent>(recallBot) &&
            recallWorld.GetComponent<RecallComponent>(recallBot).bActive;
        if (recallCommands.size() != 1u ||
            recallCommands[0].kind != eCommandKind::Recall ||
            recallResult.state != eCommandExecutionState::Accepted ||
            !bRecallActive)
        {
            std::printf(
                "[SimLab][ChampionAI] FAIL: retreat arrival did not transition to accepted Recall (commands=%zu kind=%u result=%u active=%u)\n",
                recallCommands.size(),
                recallCommands.empty()
                    ? 0u
                    : static_cast<unsigned>(recallCommands[0].kind),
                static_cast<unsigned>(recallResult.state),
                bRecallActive ? 1u : 0u);
            return false;
        }

        std::printf(
            "[SimLab][ChampionAI] PASS: CC gates, commitment, cadence, dive timeout, Kalista precheck, retreat->Recall\n");
        return true;
    }

    bool_t RunChampionAIObservationFowProbe()
    {
        CWorld world;
        DeterministicRng rng(20260716ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;

        const EntityID bot = SpawnChampion(
            world,
            entityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID enemy = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Red),
            5u);
        const Vec3 visibleEnemyPos{ 2.f, 0.f, 0.f };
        const Vec3 hiddenEnemyPos{ 40.f, 0.f, 5.f };
        world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(enemy).SetPosition(visibleEnemyPos);

        ChampionAIComponent ai{};
        ai.champion = eChampion::ASHE;
        ai.team = eTeam::Blue;
        ai.lane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        ai.activeLane = ai.lane;
        ai.state = eChampionAIState::LaneCombat;
        ai.intent = eChampionAIIntent::AttackChampion;
        ai.decisionTimer = 0.f;
        ai.laneGoal = Vec3{ 8.f, 0.f, 0.f };
        ai.safeAnchor = Vec3{ -8.f, 0.f, 0.f };
        ai.retreatGoal = ai.safeAnchor;
        world.AddComponent<ChampionAIComponent>(bot, ai);

        const NetEntityId enemyNetId = entityMap.ToNet(enemy);
        TickContext visibleTick = MakeProbeTickContext(
            1ull,
            rng,
            entityMap,
            walkable);
        std::vector<GameCommand> commands;
        CChampionAISystem::Execute(world, visibleTick, commands);

        auto& state = world.GetComponent<ChampionAIComponent>(bot);
        auto& researchState =
            world.GetComponent<ChampionAIResearchDebugComponent>(bot);
        const AiObservationV1& visibleObservation =
            researchState.decisionDraft.observation;
        if (state.lastSeenEnemyChampion != enemy ||
            state.lastSeenEnemyChampionTick != visibleTick.tickIndex ||
            std::fabs(state.lastSeenEnemyChampionPos.x - visibleEnemyPos.x) > 0.001f ||
            std::fabs(state.lastSeenEnemyChampionPos.z - visibleEnemyPos.z) > 0.001f ||
            visibleObservation.enemyChampionNetEntityId != enemyNetId ||
            visibleObservation.provenanceFlags !=
                kAiObservationTeamFilteredFlagV1)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: visible target was not recorded as team-filtered observation\n");
            return false;
        }

        world.GetComponent<TransformComponent>(enemy).SetPosition(hiddenEnemyPos);
        state.lockedChampion = enemy;
        state.comboTarget = NULL_ENTITY;
        state.comboStep = 0u;
        state.diveTarget = NULL_ENTITY;
        state.divePhase = eChampionAIDivePhase::None;
        state.decisionTimer = 0.f;
        state.intentHoldTimer = 0.f;
        commands.clear();
        TickContext hiddenTick = MakeProbeTickContext(
            2ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, hiddenTick, commands);

        const AiObservationV1& hiddenObservation =
            researchState.decisionDraft.observation;
        const bool_t bLeakedHiddenCommand = std::any_of(
            commands.begin(),
            commands.end(),
            [&](const GameCommand& command)
            {
                return command.targetEntity == enemy ||
                    WintersMath::DistanceSqXZ(
                        command.groundPos,
                        hiddenEnemyPos) <= 0.0001f;
            });
        if (state.lockedChampion != NULL_ENTITY ||
            hiddenObservation.enemyChampionNetEntityId != NULL_NET_ENTITY ||
            hiddenObservation.enemyLevel != 0u ||
            hiddenObservation.enemyHpRatio != 0.f ||
            hiddenObservation.enemyGold != 0.f ||
            hiddenObservation.enemyDistance != 0.f ||
            (researchState.decisionDraft.actionMask.legalCandidateMask &
                kAiCandidateFightBitV1) != 0u ||
            state.lastSeenEnemyChampion != enemy ||
            state.lastSeenEnemyChampionTick != visibleTick.tickIndex ||
            std::fabs(state.lastSeenEnemyChampionPos.x - visibleEnemyPos.x) > 0.001f ||
            std::fabs(state.lastSeenEnemyChampionPos.z - visibleEnemyPos.z) > 0.001f ||
            bLeakedHiddenCommand)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: hidden truth leaked through target, memory, or command\n");
            return false;
        }

        state.decisionTimer = 0.f;
        commands.clear();
        TickContext expiredTick = MakeProbeTickContext(
            2ull + 5ull * DeterministicTime::kTicksPerSecond,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, expiredTick, commands);
        if (state.lastSeenEnemyChampion != NULL_ENTITY ||
            state.lastSeenEnemyChampionTick != 0u)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: last-seen memory did not expire\n");
            return false;
        }

        world.GetComponent<TransformComponent>(enemy).SetPosition(visibleEnemyPos);
        VisibilityComponent botVisibility{};
        VisibilityComponent enemyVisibility{};
        enemyVisibility.bInConcealment = true;
        enemyVisibility.concealmentId = 77u;
        world.AddComponent<VisibilityComponent>(bot, botVisibility);
        world.AddComponent<VisibilityComponent>(enemy, enemyVisibility);
        state.lockedChampion = enemy;
        state.comboTarget = NULL_ENTITY;
        state.diveTarget = NULL_ENTITY;
        state.divePhase = eChampionAIDivePhase::None;
        state.decisionTimer = 0.f;
        commands.clear();
        TickContext concealedTick = MakeProbeTickContext(
            expiredTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, concealedTick, commands);
        if (researchState.decisionDraft.observation.enemyChampionNetEntityId !=
            NULL_NET_ENTITY)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: outside source observed concealed target\n");
            return false;
        }

        auto& sourceVisibility = world.GetComponent<VisibilityComponent>(bot);
        sourceVisibility.bInConcealment = true;
        sourceVisibility.concealmentId = enemyVisibility.concealmentId;
        state.decisionTimer = 0.f;
        commands.clear();
        TickContext sameConcealmentTick = MakeProbeTickContext(
            concealedTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, sameConcealmentTick, commands);
        if (researchState.decisionDraft.observation.enemyChampionNetEntityId !=
            enemyNetId)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: same-concealment source missed target\n");
            return false;
        }

        sourceVisibility.bInConcealment = false;
        sourceVisibility.concealmentId = NULL_ENTITY;
        world.GetComponent<VisionSourceComponent>(bot).bTrueSight = true;
        state.lockedChampion = NULL_ENTITY;
        state.comboTarget = NULL_ENTITY;
        state.diveTarget = NULL_ENTITY;
        state.divePhase = eChampionAIDivePhase::None;
        state.decisionTimer = 0.f;
        commands.clear();
        TickContext trueSightTick = MakeProbeTickContext(
            sameConcealmentTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, trueSightTick, commands);
        if (researchState.decisionDraft.observation.enemyChampionNetEntityId !=
            enemyNetId)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: true-sight source missed concealed target\n");
            return false;
        }

        auto& liveEnemyVisibility =
            world.GetComponent<VisibilityComponent>(enemy);
        liveEnemyVisibility.bInConcealment = false;
        liveEnemyVisibility.concealmentId = NULL_ENTITY;
        VisionConeComponent cone{};
        cone.forward = Vec3{ 1.f, 0.f, 0.f };
        cone.halfAngleCos = 0.5f;
        world.AddComponent<VisionConeComponent>(bot, cone);
        world.GetComponent<TransformComponent>(enemy).SetPosition(
            Vec3{ -2.f, 0.f, 0.f });
        state.lockedChampion = NULL_ENTITY;
        state.comboTarget = NULL_ENTITY;
        state.diveTarget = NULL_ENTITY;
        state.divePhase = eChampionAIDivePhase::None;
        state.decisionTimer = 0.f;
        commands.clear();
        TickContext behindConeTick = MakeProbeTickContext(
            trueSightTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, behindConeTick, commands);
        if (researchState.decisionDraft.observation.enemyChampionNetEntityId !=
            NULL_NET_ENTITY)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: source observed target behind vision cone\n");
            return false;
        }

        world.GetComponent<TransformComponent>(enemy).SetPosition(visibleEnemyPos);
        state.decisionTimer = 0.f;
        commands.clear();
        TickContext insideConeTick = MakeProbeTickContext(
            behindConeTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, insideConeTick, commands);
        if (researchState.decisionDraft.observation.enemyChampionNetEntityId !=
            enemyNetId)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: source missed target inside vision cone\n");
            return false;
        }

        CombatActionComponent lockedAction{};
        lockedAction.eKind = eCombatActionKind::Skill;
        lockedAction.uStartTick = insideConeTick.tickIndex;
        lockedAction.uEndTick = insideConeTick.tickIndex + 30u;
        world.AddComponent<CombatActionComponent>(bot, lockedAction);
        state.decisionTimer = 0.f;
        walkable.ResetQueryCounts();
        commands.clear();
        TickContext firstLockedTick = MakeProbeTickContext(
            insideConeTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, firstLockedTick, commands);
        const u32_t firstPointQueries = walkable.pointQueryCount;
        const u32_t firstSegmentQueries = walkable.segmentQueryCount;
        if (firstPointQueries == 0u || firstSegmentQueries == 0u)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: due influence map did not query navigation\n");
            return false;
        }

        commands.clear();
        TickContext secondLockedTick = MakeProbeTickContext(
            firstLockedTick.tickIndex + 1ull,
            rng,
            entityMap,
            walkable);
        CChampionAISystem::Execute(world, secondLockedTick, commands);
        if (walkable.pointQueryCount != firstPointQueries ||
            walkable.segmentQueryCount != firstSegmentQueries)
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: action lock rebuilt influence navigation at 30 Hz\n");
            return false;
        }

        std::vector<u8_t> keyframeBytes;
        if (!SimCheckpoint::SaveWorldKeyframe(
                world,
                rng,
                entityMap,
                secondLockedTick.tickIndex,
                keyframeBytes))
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: transient AI research store blocked keyframe save\n");
            return false;
        }

        CWorld restoredWorld;
        DeterministicRng restoredRng(1ull);
        EntityIdMap restoredEntityMap;
        u64_t restoredTick = 0u;
        if (!SimCheckpoint::RestoreWorldKeyframe(
                restoredWorld,
                restoredRng,
                restoredEntityMap,
                restoredTick,
                keyframeBytes) ||
            restoredWorld.HasComponent<ChampionAIResearchDebugComponent>(bot))
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: transient AI research evidence survived keyframe restore\n");
            return false;
        }

        std::vector<GameCommand> restoredCommands;
        TickContext restoredDecisionTick = MakeProbeTickContext(
            restoredTick + 1ull,
            restoredRng,
            restoredEntityMap,
            walkable);
        CChampionAISystem::Execute(
            restoredWorld,
            restoredDecisionTick,
            restoredCommands);
        if (!restoredWorld.HasComponent<ChampionAIResearchDebugComponent>(bot))
        {
            std::printf(
                "[SimLab][ChampionAIFow] FAIL: transient AI research evidence was not recreated\n");
            return false;
        }

        std::printf(
            "[SimLab][ChampionAIFow] PASS: team vision, hidden command gate, last-seen expiry, concealment, cone, influence cadence, transient checkpoint\n");
        return true;
    }

    bool_t RunChampionAIMidDefenseDeterminismProbe()
    {
        struct ScenarioResult
        {
            u64_t hash = 1469598103934665603ull;
            u8_t homeLane = 0u;
            u8_t activeLane = 0u;
            eChampionAIBrainType brainType = eChampionAIBrainType::RuleBased;
            bool_t bActivated = false;
            bool_t bSawMoveCommand = false;
            bool_t bMoved = false;
            bool_t bMovedTowardAnchor = false;
            bool_t bNexusFallback = false;
            eChampionAIState finalState =
                eChampionAIState::MoveToOuterTurret;
            eChampionAIIntent finalIntent =
                eChampionAIIntent::FarmMinion;
            Vec3 firstMoveTarget{};
            Vec3 firstAnchor{};
            Vec3 finalAnchor{};
        };

        auto SpawnStructure = [](
            CWorld& world,
            eTeam team,
            Winters::Map::eObjectKind kind,
            Winters::Map::eTurretTier tier,
            Winters::Map::eLane lane,
            const Vec3& position,
            bool_t bDead) -> EntityID
        {
            const EntityID entity = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::Structure,
                team,
                position);
            if (entity == NULL_ENTITY)
                return NULL_ENTITY;

            auto& structure = world.GetComponent<StructureComponent>(entity);
            structure.team = team;
            structure.kind = static_cast<u32_t>(kind);
            structure.tier = static_cast<u32_t>(tier);
            structure.lane = static_cast<u32_t>(lane);
            structure.maxHp = 3000.f;
            structure.hp = bDead ? 0.f : structure.maxHp;

            auto& health = world.GetComponent<HealthComponent>(entity);
            health.fMaximum = structure.maxHp;
            health.fCurrent = structure.hp;
            health.bIsDead = bDead;
            return entity;
        };

        auto RunScenario = [&](u64_t seed) -> ScenarioResult
        {
            CWorld world;
            DeterministicRng rng(seed);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();

            const EntityID bot = SpawnChampion(
                world,
                entityMap,
                eChampion::ASHE,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});

            ChampionAIComponent ai{};
            ai.champion = eChampion::ASHE;
            ai.team = eTeam::Blue;
            ai.difficulty = 2u;
            ai.brainType = eChampionAIBrainType::PlayerLike;
            ai.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
            ai.activeLane = ai.lane;
            ai.state = eChampionAIState::LaneCombat;
            ai.decisionTimer = 0.f;
            ai.laneGoal = Vec3{ -20.f, 1.f, -20.f };
            ai.safeAnchor = Vec3{ -10.f, 1.f, -10.f };
            ai.retreatGoal = ai.safeAnchor;
            ai.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
            world.AddComponent<ChampionAIComponent>(bot, ai);

            const EntityID deadOuter = SpawnStructure(
                world,
                eTeam::Blue,
                Winters::Map::eObjectKind::Structure_Turret,
                Winters::Map::eTurretTier::Outer,
                Winters::Map::eLane::Top,
                Vec3{ -10.f, 0.f, -10.f },
                true);
            const EntityID liveMidInner = SpawnStructure(
                world,
                eTeam::Blue,
                Winters::Map::eObjectKind::Structure_Turret,
                Winters::Map::eTurretTier::Inner,
                Winters::Map::eLane::Mid,
                Vec3{ 20.f, 0.f, 10.f },
                false);
            const EntityID liveNexus = SpawnStructure(
                world,
                eTeam::Blue,
                Winters::Map::eObjectKind::Structure_Nexus,
                Winters::Map::eTurretTier::Nexus,
                Winters::Map::eLane::Base,
                Vec3{ 30.f, 0.f, 10.f },
                false);
            const EntityID alliedWave = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Blue,
                Vec3{ 2.f, 0.f, 0.f });
            if (alliedWave != NULL_ENTITY)
            {
                world.GetComponent<MinionComponent>(alliedWave).laneType =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
                world.GetComponent<MinionStateComponent>(alliedWave).lane =
                    static_cast<u8_t>(Winters::Map::eLane::Mid);
            }

            ScenarioResult result{};
            const Vec3 initialPos =
                world.GetComponent<TransformComponent>(bot).GetPosition();
            for (u64_t tick = 1ull; tick <= 180ull; ++tick)
            {
                if (tick == 91ull)
                {
                    auto& structure =
                        world.GetComponent<StructureComponent>(liveMidInner);
                    structure.hp = 0.f;
                    auto& health =
                        world.GetComponent<HealthComponent>(liveMidInner);
                    health.fCurrent = 0.f;
                    health.bIsDead = true;
                }

                TickContext tc =
                    MakeProbeTickContext(tick, rng, entityMap, walkable);
                std::vector<GameCommand> commands;
                CChampionAISystem::Execute(world, tc, commands);

                auto& currentAI = world.GetComponent<ChampionAIComponent>(bot);
                HashU64(result.hash, tick);
                HashU64(result.hash, static_cast<u64_t>(currentAI.state));
                HashU64(result.hash, static_cast<u64_t>(currentAI.intent));
                HashU64(result.hash, static_cast<u64_t>(currentAI.lastAction));
                HashU64(result.hash, static_cast<u64_t>(currentAI.brainType));
                HashU64(result.hash, currentAI.bMidDefenseActive ? 1u : 0u);
                HashU64(result.hash, currentAI.lane);
                HashU64(result.hash, currentAI.activeLane);

                for (const GameCommand& command : commands)
                {
                    HashU64(result.hash, static_cast<u64_t>(command.kind));
                    HashU64(result.hash, command.slot);
                    HashU64(result.hash, command.targetEntity);
                    HashU64(result.hash, command.sequenceNum);
                    HashF32(result.hash, command.groundPos.x);
                    HashF32(result.hash, command.groundPos.y);
                    HashF32(result.hash, command.groundPos.z);
                    if (command.kind == eCommandKind::Move &&
                        !result.bSawMoveCommand)
                    {
                        result.firstMoveTarget = command.groundPos;
                        result.firstAnchor = currentAI.midDefenseAnchor;
                    }
                    result.bSawMoveCommand =
                        result.bSawMoveCommand ||
                        command.kind == eCommandKind::Move;
                    executor->ExecuteCommand(world, tc, command);
                }

                CMoveSystem::Execute(world, tc);
                const Vec3 pos =
                    world.GetComponent<TransformComponent>(bot).GetPosition();
                HashF32(result.hash, pos.x);
                HashF32(result.hash, pos.y);
                HashF32(result.hash, pos.z);
                result.bActivated =
                    result.bActivated || currentAI.bMidDefenseActive;
            }

            const auto& finalAI = world.GetComponent<ChampionAIComponent>(bot);
            const Vec3 finalPos =
                world.GetComponent<TransformComponent>(bot).GetPosition();
            result.homeLane = finalAI.lane;
            result.activeLane = finalAI.activeLane;
            result.brainType = finalAI.brainType;
            result.finalState = finalAI.state;
            result.finalIntent = finalAI.intent;
            result.finalAnchor = finalAI.midDefenseAnchor;
            result.bMoved =
                WintersMath::DistanceSqXZ(initialPos, finalPos) > 0.25f;
            result.bMovedTowardAnchor =
                WintersMath::DistanceSqXZ(finalPos, finalAI.midDefenseAnchor) <
                WintersMath::DistanceSqXZ(initialPos, finalAI.midDefenseAnchor);
            // 포메이션 간격 1.75 x 슬롯 오프셋(최대 2칸)까지 허용 — 앵커는
            // 넥서스 기준 측면으로 최대 3.5 벌어질 수 있다.
            result.bNexusFallback =
                WintersMath::DistanceSqXZ(
                    finalAI.midDefenseAnchor,
                    Vec3{ 30.f, 0.f, 10.f }) <= 4.5f * 4.5f;

            if (deadOuter == NULL_ENTITY ||
                liveMidInner == NULL_ENTITY ||
                liveNexus == NULL_ENTITY ||
                alliedWave == NULL_ENTITY)
            {
                result.bActivated = false;
            }
            return result;
        };

        const ScenarioResult runA = RunScenario(20260722ull);
        const ScenarioResult runB = RunScenario(20260722ull);
        if (runA.hash != runB.hash ||
            !runA.bActivated ||
            !runA.bSawMoveCommand ||
            !runA.bMoved ||
            !runA.bMovedTowardAnchor ||
            !runA.bNexusFallback ||
            WintersMath::DistanceSqXZ(
                runA.firstMoveTarget,
                runA.firstAnchor) > 0.0001f ||
            runA.homeLane != static_cast<u8_t>(Winters::Map::eLane::Top) ||
            runA.activeLane != static_cast<u8_t>(Winters::Map::eLane::Mid) ||
            runA.brainType != eChampionAIBrainType::PlayerLike ||
            // 집결 반경 밖 = GroupMidDefense(앵커 복귀), 도착 후 = LaneCombat
            // 위임(파밍/교전 재개)이 새 계약이다. 제자리 대기 고정은 폐기.
            (runA.finalState != eChampionAIState::GroupMidDefense &&
                runA.finalState != eChampionAIState::LaneCombat))
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: hashA=%016llX hashB=%016llX active=%u moveCmd=%u moved=%u toward=%u nexus=%u home=%u activeLane=%u brain=%u state=%u intent=%u\n",
                static_cast<unsigned long long>(runA.hash),
                static_cast<unsigned long long>(runB.hash),
                runA.bActivated ? 1u : 0u,
                runA.bSawMoveCommand ? 1u : 0u,
                runA.bMoved ? 1u : 0u,
                runA.bMovedTowardAnchor ? 1u : 0u,
                runA.bNexusFallback ? 1u : 0u,
                static_cast<unsigned>(runA.homeLane),
                static_cast<unsigned>(runA.activeLane),
                static_cast<unsigned>(runA.brainType),
                static_cast<unsigned>(runA.finalState),
                static_cast<unsigned>(runA.finalIntent));
            return false;
        }

        CWorld commitmentWorld;
        DeterministicRng commitmentRng(20260723ull);
        EntityIdMap commitmentEntityMap;
        FlatWalkable commitmentWalkable;
        const EntityID commitmentBot = SpawnChampion(
            commitmentWorld,
            commitmentEntityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        const EntityID distantTarget = SpawnChampion(
            commitmentWorld,
            commitmentEntityMap,
            eChampion::JAX,
            static_cast<u8_t>(eTeam::Red),
            5u);
        commitmentWorld.GetComponent<TransformComponent>(commitmentBot).SetPosition(
            Vec3{});
        commitmentWorld.GetComponent<TransformComponent>(distantTarget).SetPosition(
            Vec3{ 40.f, 0.f, 0.f });

        ChampionAIComponent commitmentAI{};
        commitmentAI.champion = eChampion::ASHE;
        commitmentAI.team = eTeam::Blue;
        commitmentAI.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
        commitmentAI.activeLane = commitmentAI.lane;
        commitmentAI.state = eChampionAIState::LaneCombat;
        commitmentAI.comboTarget = distantTarget;
        commitmentAI.comboStep = 1u;
        commitmentAI.decisionTimer = 0.f;
        commitmentAI.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
        commitmentWorld.AddComponent<ChampionAIComponent>(
            commitmentBot,
            commitmentAI);
        SpawnStructure(
            commitmentWorld,
            eTeam::Blue,
            Winters::Map::eObjectKind::Structure_Turret,
            Winters::Map::eTurretTier::Outer,
            Winters::Map::eLane::Top,
            Vec3{ -10.f, 0.f, -10.f },
            true);
        SpawnStructure(
            commitmentWorld,
            eTeam::Blue,
            Winters::Map::eObjectKind::Structure_Turret,
            Winters::Map::eTurretTier::Inner,
            Winters::Map::eLane::Mid,
            Vec3{ 20.f, 0.f, 10.f },
            false);

        TickContext commitmentTick = MakeProbeTickContext(
            1ull,
            commitmentRng,
            commitmentEntityMap,
            commitmentWalkable);
        std::vector<GameCommand> commitmentCommands;
        CChampionAISystem::Execute(
            commitmentWorld,
            commitmentTick,
            commitmentCommands);
        if (commitmentWorld.GetComponent<ChampionAIComponent>(commitmentBot)
                .bMidDefenseActive)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: active combo was interrupted by macro transition\n");
            return false;
        }

        auto& releasedCommitment =
            commitmentWorld.GetComponent<ChampionAIComponent>(commitmentBot);
        releasedCommitment.comboTarget = NULL_ENTITY;
        releasedCommitment.comboStep = 0u;
        releasedCommitment.diveTarget = NULL_ENTITY;
        releasedCommitment.divePhase = eChampionAIDivePhase::None;
        releasedCommitment.decisionTimer = 0.f;
        TickContext releasedTick = MakeProbeTickContext(
            2ull,
            commitmentRng,
            commitmentEntityMap,
            commitmentWalkable);
        std::vector<GameCommand> releasedCommands;
        CChampionAISystem::Execute(
            commitmentWorld,
            releasedTick,
            releasedCommands);
        if (!releasedCommitment.bMidDefenseActive ||
            releasedCommitment.state != eChampionAIState::GroupMidDefense ||
            releasedCommitment.intent != eChampionAIIntent::DefendMid)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: macro did not start after commitment cleared\n");
            return false;
        }

        CWorld dangerWorld;
        DeterministicRng dangerRng(20260724ull);
        EntityIdMap dangerEntityMap;
        FlatWalkable dangerWalkable;
        const EntityID dangerBot = SpawnChampion(
            dangerWorld,
            dangerEntityMap,
            eChampion::ASHE,
            static_cast<u8_t>(eTeam::Blue),
            0u);
        dangerWorld.GetComponent<TransformComponent>(dangerBot).SetPosition(
            Vec3{});
        ChampionAIComponent dangerAI{};
        dangerAI.champion = eChampion::ASHE;
        dangerAI.team = eTeam::Blue;
        dangerAI.lane = static_cast<u8_t>(Winters::Map::eLane::Top);
        dangerAI.activeLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        dangerAI.state = eChampionAIState::GroupMidDefense;
        dangerAI.intent = eChampionAIIntent::DefendMid;
        dangerAI.bMidDefenseActive = true;
        dangerAI.decisionTimer = 0.f;
        dangerAI.safeAnchor = Vec3{ -10.f, 1.f, 0.f };
        dangerAI.retreatGoal = dangerAI.safeAnchor;
        dangerAI.midDefenseAnchor = Vec3{ 20.f, 1.f, 10.f };
        dangerWorld.AddComponent<ChampionAIComponent>(dangerBot, dangerAI);
        SpawnStructure(
            dangerWorld,
            eTeam::Red,
            Winters::Map::eObjectKind::Structure_Turret,
            Winters::Map::eTurretTier::Outer,
            Winters::Map::eLane::Top,
            Vec3{ 1.f, 0.f, 0.f },
            false);
        TickContext dangerTick = MakeProbeTickContext(
            1ull,
            dangerRng,
            dangerEntityMap,
            dangerWalkable);
        std::vector<GameCommand> dangerCommands;
        CChampionAISystem::Execute(
            dangerWorld,
            dangerTick,
            dangerCommands);
        const auto& dangerState =
            dangerWorld.GetComponent<ChampionAIComponent>(dangerBot);
        if (dangerState.state != eChampionAIState::Retreat ||
            dangerState.fDecisionTurretDanger <=
                dangerState.fTurretDangerThreshold)
        {
            std::printf(
                "[SimLab][ChampionAI][MidDefense] FAIL: cross-lane turret danger was ignored during rotation state=%u block=%u danger=%.3f inside=%u\n",
                static_cast<unsigned>(dangerState.state),
                static_cast<unsigned>(dangerState.debugLastBlockReason),
                dangerState.fDecisionTurretDanger,
                dangerState.bInsideEnemyTurretDanger ? 1u : 0u);
            return false;
        }

        std::printf(
            "[SimLab][ChampionAI][MidDefense] PASS: deterministic hash=%016llX home lane preserved, PlayerLike bot grouped mid after commitment gate\n",
            static_cast<unsigned long long>(runA.hash));
        return true;
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
        if (!CSkillRankSystem::TryLevelSkill(
                ranks,
                1u,
                static_cast<u8_t>(eSkillSlot::E)))
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

    EntityID FindViegoSoul(CWorld& world, EntityID eligibleViego = NULL_ENTITY)
    {
        EntityID result = NULL_ENTITY;
        world.ForEach<ViegoSoulComponent>(
            std::function<void(EntityID, ViegoSoulComponent&)>(
                [&](EntityID entity, ViegoSoulComponent& soul)
                {
                    if (result == NULL_ENTITY &&
                        (eligibleViego == NULL_ENTITY || soul.eligibleViego == eligibleViego))
                    {
                        result = entity;
                    }
                }));
        return result;
    }

    bool_t RunViegoPossessionProbe()
    {
        CWorld world;
        DeterministicRng rng(20260711ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID viego = SpawnChampion(world, entityMap, eChampion::VIEGO, 0, 0);
        const EntityID otherViego = SpawnChampion(world, entityMap, eChampion::VIEGO, 0, 1);
        const EntityID ally = SpawnChampion(world, entityMap, eChampion::ANNIE, 0, 2);
        const EntityID victim = SpawnChampion(world, entityMap, eChampion::JAX, 1, 5);

        world.GetComponent<TransformComponent>(viego).SetPosition(Vec3{ 0.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(otherViego).SetPosition(Vec3{ 5.5f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(victim).SetPosition(Vec3{ 6.f, 0.f, 0.f });

        auto& viegoRanks = world.GetComponent<SkillRankComponent>(viego);
        viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::Q)] = 1u;
        viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::W)] = 1u;
        viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::E)] = 1u;
        viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::R)] = 1u;
        viegoRanks.pointsAvailable = 2u;
        auto& viegoSkills = world.GetComponent<SkillStateComponent>(viego);
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::Q)].cooldownRemaining = 2.f;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::Q)].cooldownDuration = 2.f;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::W)].currentStage = 1u;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::W)].stageWindow = 1.f;

        auto& victimRanks = world.GetComponent<SkillRankComponent>(victim);
        victimRanks.ranks[static_cast<u8_t>(eSkillSlot::Q)] = 3u;
        victimRanks.ranks[static_cast<u8_t>(eSkillSlot::W)] = 2u;
        victimRanks.ranks[static_cast<u8_t>(eSkillSlot::E)] = 4u;

        TickContext tick1 = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        ViegoGameSim::TrySpawnSoulForKill(world, tick1, ally, victim);
        if (FindViegoSoul(world) != NULL_ENTITY)
        {
            std::printf("[SimLab][Viego] FAIL: non-Viego kill spawned a soul\n");
            return false;
        }

        ViegoGameSim::TrySpawnSoulForKill(world, tick1, viego, victim);
        const EntityID soul = FindViegoSoul(world, viego);
        if (soul == NULL_ENTITY)
        {
            std::printf("[SimLab][Viego] FAIL: Viego kill did not spawn a soul\n");
            return false;
        }

        const auto& soulState = world.GetComponent<ViegoSoulComponent>(soul);
        if (soulState.champion != eChampion::JAX ||
            soulState.eligibleViego != viego ||
            soulState.skillRanks[static_cast<u8_t>(eSkillSlot::Q)] != 3u)
        {
            std::printf("[SimLab][Viego] FAIL: soul owner/champion/ranks mismatch\n");
            return false;
        }

        const NetEntityId soulNetId = entityMap.ToNet(soul);
        DamageRequest damage{};
        damage.source = ally;
        damage.target = soul;
        damage.sourceTeam = eTeam::Blue;
        damage.flatAmount = 500.f;
        const DamageResult damageResult = ApplyDamageRequest(world, tick1, damage);
        if (damageResult.finalAmount != 0.f ||
            world.GetComponent<HealthComponent>(soul).fCurrent != 1.f)
        {
            std::printf("[SimLab][Viego] FAIL: soul accepted gameplay damage\n");
            return false;
        }

        GameCommand stealAttempt{};
        stealAttempt.kind = eCommandKind::BasicAttack;
        stealAttempt.issuerEntity = otherViego;
        stealAttempt.targetEntity = soul;
        stealAttempt.sequenceNum = 1u;
        executor->ExecuteCommand(world, tick1, stealAttempt);
        if (!world.IsAlive(soul) ||
            world.GetComponent<ViegoSimComponent>(otherViego).bPossessionPending)
        {
            std::printf("[SimLab][Viego] FAIL: non-owner consumed the soul\n");
            return false;
        }

        GameCommand consume{};
        consume.kind = eCommandKind::BasicAttack;
        consume.issuerEntity = viego;
        consume.targetEntity = soul;
        consume.sequenceNum = 2u;
        executor->ExecuteCommand(world, tick1, consume);
        if (!world.HasComponent<AttackChaseComponent>(viego))
        {
            std::printf("[SimLab][Viego] FAIL: distant soul did not start owner chase\n");
            return false;
        }

        std::vector<GameCommand> chasedCommands;
        TickContext tick2 = MakeProbeTickContext(2ull, rng, entityMap, walkable);
        CAttackChaseSystem::Execute(world, tick2, chasedCommands);
        if (!chasedCommands.empty() || !world.HasComponent<AttackChaseComponent>(viego))
        {
            std::printf("[SimLab][Viego] FAIL: soul chase was cancelled before arrival\n");
            return false;
        }

        world.GetComponent<TransformComponent>(viego).SetPosition(Vec3{ 5.5f, 0.f, 0.f });
        TickContext tick3 = MakeProbeTickContext(3ull, rng, entityMap, walkable);
        CAttackChaseSystem::Execute(world, tick3, chasedCommands);
        if (chasedCommands.size() != 1u)
        {
            std::printf("[SimLab][Viego] FAIL: soul chase did not issue consume command\n");
            return false;
        }
        executor->ExecuteCommand(world, tick3, chasedCommands.front());

        if (world.IsAlive(soul) || entityMap.FromNet(soulNetId) != NULL_ENTITY)
        {
            std::printf("[SimLab][Viego] FAIL: consumed soul or net mapping survived\n");
            return false;
        }
        const auto& pending = world.GetComponent<ViegoSimComponent>(viego);
        const auto& consumeAction = world.GetComponent<ActionStateComponent>(viego);
        if (!pending.bPossessionPending ||
            consumeAction.movePolicy != eSkillActionMovePolicy::StationaryChannel)
        {
            std::printf("[SimLab][Viego] FAIL: consume pending/action policy mismatch\n");
            return false;
        }

        const u32_t consumeActionSequence = consumeAction.sequence;
        GameCommand blockedR{};
        blockedR.kind = eCommandKind::CastSkill;
        blockedR.issuerEntity = viego;
        blockedR.slot = static_cast<u8_t>(eSkillSlot::R);
        blockedR.sequenceNum = 3u;
        blockedR.direction = Vec3{ 1.f, 0.f, 0.f };
        TickContext tick4 = MakeProbeTickContext(4ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick4, blockedR);
        if (!world.GetComponent<ViegoSimComponent>(viego).bPossessionPending ||
            world.GetComponent<ActionStateComponent>(viego).sequence != consumeActionSequence)
        {
            std::printf("[SimLab][Viego] FAIL: command interrupted soul consumption\n");
            return false;
        }

        GameCommand queuedMove{};
        queuedMove.kind = eCommandKind::Move;
        queuedMove.issuerEntity = viego;
        queuedMove.sequenceNum = 4u;
        queuedMove.groundPos = Vec3{ 12.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick4, queuedMove);
        if (!world.GetComponent<ActionStateComponent>(viego).bHasQueuedMove)
        {
            std::printf("[SimLab][Viego] FAIL: consume did not retain move intent\n");
            return false;
        }

        for (u64_t tick = 4ull; tick <= 25ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            ViegoGameSim::Tick(world, tc);
        }

        if (!world.HasComponent<FormOverrideComponent>(viego))
        {
            std::printf("[SimLab][Viego] FAIL: possession form was not applied\n");
            return false;
        }
        const auto& form = world.GetComponent<FormOverrideComponent>(viego);
        const auto attackIdentity = CSpellbookFormOverrideSystem::ResolveBasicAttack(
            world, viego, eChampion::VIEGO);
        if (!form.bActive || form.visualChampion != eChampion::JAX ||
            form.skillChampion != eChampion::JAX || form.skillSlotMask != 0x0fu ||
            form.fRemainingSec >= 0.f || attackIdentity.hookChampion != eChampion::JAX)
        {
            std::printf("[SimLab][Viego] FAIL: form/basic-attack override mismatch\n");
            return false;
        }
        if (viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::Q)] != 3u ||
            viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::W)] != 2u ||
            viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::E)] != 4u)
        {
            std::printf("[SimLab][Viego] FAIL: borrowed QWE ranks mismatch\n");
            return false;
        }

        for (u64_t tick = 26ull; tick <= 55ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            ViegoGameSim::Tick(world, tc);
        }
        const auto& activeState = world.GetComponent<ViegoSimComponent>(viego);
        const f32_t storedQCooldown = activeState.originalSkillState
            .slots[static_cast<u8_t>(eSkillSlot::Q)].cooldownRemaining;
        if (storedQCooldown >= 1.1f || storedQCooldown <= 0.8f)
        {
            std::printf("[SimLab][Viego] FAIL: original cooldown did not tick in background\n");
            return false;
        }

        GameCommand levelQ{};
        levelQ.kind = eCommandKind::LevelSkill;
        levelQ.issuerEntity = viego;
        levelQ.slot = static_cast<u8_t>(eSkillSlot::Q);
        levelQ.sequenceNum = 5u;
        TickContext tick56 = MakeProbeTickContext(56ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick56, levelQ);
        if (viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::Q)] != 3u ||
            world.GetComponent<ViegoSimComponent>(viego).originalSkillRanks
                .ranks[static_cast<u8_t>(eSkillSlot::Q)] != 2u)
        {
            std::printf("[SimLab][Viego] FAIL: level-up mutated borrowed rank bank\n");
            return false;
        }

        JaxSimComponent borrowedJax{};
        borrowedJax.bCounterStrikeActive = true;
        borrowedJax.counterTimerSec = 2.f;
        world.AddComponent<JaxSimComponent>(viego, borrowedJax);

        GameCommand returnR{};
        returnR.kind = eCommandKind::CastSkill;
        returnR.issuerEntity = viego;
        returnR.slot = static_cast<u8_t>(eSkillSlot::R);
        returnR.sequenceNum = 6u;
        returnR.direction = Vec3{ 1.f, 0.f, 0.f };
        returnR.groundPos = Vec3{ 10.f, 0.f, 0.f };
        TickContext tick57 = MakeProbeTickContext(57ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick57, returnR);
        if (world.HasComponent<FormOverrideComponent>(viego) ||
            world.GetComponent<ViegoSimComponent>(viego).bPossessionActive ||
            world.HasComponent<JaxSimComponent>(viego) ||
            viegoRanks.ranks[static_cast<u8_t>(eSkillSlot::Q)] != 2u ||
            viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)].cooldownRemaining <= 0.f)
        {
            std::printf("[SimLab][Viego] FAIL: R did not restore Viego and cancel borrowed state\n");
            return false;
        }

        FormOverrideComponent yoneForm{};
        yoneForm.baseChampion = eChampion::VIEGO;
        yoneForm.visualChampion = eChampion::YONE;
        yoneForm.skillChampion = eChampion::YONE;
        yoneForm.skillSlotMask = 0x0fu;
        yoneForm.fRemainingSec = -1.f;
        yoneForm.bActive = true;
        world.AddComponent<FormOverrideComponent>(viego, yoneForm);
        auto& syntheticYonePossession = world.GetComponent<ViegoSimComponent>(viego);
        syntheticYonePossession.bPossessionActive = true;
        syntheticYonePossession.possessionChampion = eChampion::YONE;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::E)] = SkillSlotRuntime{};

        GameCommand yoneE{};
        yoneE.kind = eCommandKind::CastSkill;
        yoneE.issuerEntity = viego;
        yoneE.slot = static_cast<u8_t>(eSkillSlot::E);
        yoneE.sequenceNum = 7u;
        yoneE.direction = Vec3{ 1.f, 0.f, 0.f };
        TickContext tick100 = MakeProbeTickContext(100ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick100, yoneE);
        if (!world.HasComponent<YoneSimComponent>(viego))
        {
            std::printf("[SimLab][Viego] FAIL: borrowed Yone E runtime did not start\n");
            return false;
        }

        ViegoGameSim::ClearPossession(world, viego);
        const Vec3 positionAfterYoneCancel =
            world.GetComponent<TransformComponent>(viego).GetPosition();
        for (u64_t tick = 101ull; tick <= 140ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            YoneGameSim::Tick(world, tc);
        }
        const Vec3 positionAfterYoneTicks =
            world.GetComponent<TransformComponent>(viego).GetPosition();
        if (world.HasComponent<YoneSimComponent>(viego) ||
            DistanceSqXZLocal(positionAfterYoneCancel, positionAfterYoneTicks) > 0.0001f)
        {
            std::printf("[SimLab][Viego] FAIL: borrowed Yone runtime survived form exit\n");
            return false;
        }

        ViegoGameSim::TrySpawnSoulForKill(world, tick57, viego, victim);
        const EntityID expiringSoul = FindViegoSoul(world, viego);
        const NetEntityId expiringNetId = entityMap.ToNet(expiringSoul);
        for (u64_t tick = 58ull; tick < 207ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            ViegoGameSim::Tick(world, tc);
        }
        if (expiringSoul == NULL_ENTITY || !world.IsAlive(expiringSoul))
        {
            std::printf("[SimLab][Viego] FAIL: soul expired before five seconds\n");
            return false;
        }
        for (u64_t tick = 207ull; tick <= 208ull && world.IsAlive(expiringSoul); ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            ViegoGameSim::Tick(world, tc);
        }
        if (world.IsAlive(expiringSoul) || entityMap.FromNet(expiringNetId) != NULL_ENTITY)
        {
            std::printf("[SimLab][Viego] FAIL: five-second soul did not expire cleanly\n");
            return false;
        }

        std::printf("[SimLab][Viego] PASS: owner soul, chase, possession, QWE/BA, R restore\n");
        return true;
    }

    bool_t RunActionMovePolicyProbe()
    {
        CWorld world;
        DeterministicRng rng(20260712ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();
        const EntityID viego = SpawnChampion(world, entityMap, eChampion::VIEGO, 0, 0);

        auto& ranks = world.GetComponent<SkillRankComponent>(viego);
        ranks.ranks[static_cast<u8_t>(eSkillSlot::Q)] = 1u;
        ranks.ranks[static_cast<u8_t>(eSkillSlot::W)] = 1u;
        ranks.ranks[static_cast<u8_t>(eSkillSlot::E)] = 1u;

        MoveTargetComponent move{};
        move.target = Vec3{ 10.f, 0.f, 0.f };
        move.bHasTarget = true;
        world.AddComponent<MoveTargetComponent>(viego, move);

        TickContext tick1 = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand castE{};
        castE.kind = eCommandKind::CastSkill;
        castE.issuerEntity = viego;
        castE.slot = static_cast<u8_t>(eSkillSlot::E);
        castE.sequenceNum = 1u;
        castE.direction = Vec3{ 1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick1, castE);
        if (!world.GetComponent<MoveTargetComponent>(viego).bHasTarget ||
            world.GetComponent<ActionStateComponent>(viego).movePolicy !=
                eSkillActionMovePolicy::Allow)
        {
            std::printf("[SimLab][ActionLock] FAIL: Allow cast cleared movement\n");
            return false;
        }

        TickContext tick2 = MakeProbeTickContext(2ull, rng, entityMap, walkable);
        GameCommand castQ{};
        castQ.kind = eCommandKind::CastSkill;
        castQ.issuerEntity = viego;
        castQ.slot = static_cast<u8_t>(eSkillSlot::Q);
        castQ.sequenceNum = 2u;
        castQ.direction = Vec3{ 1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick2, castQ);
        auto& action = world.GetComponent<ActionStateComponent>(viego);
        if (action.movePolicy != eSkillActionMovePolicy::QueueUntilUnlock ||
            action.lockEndTick <= tick2.tickIndex ||
            action.lockEndTick - tick2.tickIndex > 8u)
        {
            std::printf("[SimLab][ActionLock] FAIL: Queue cast lock policy/ticks mismatch\n");
            return false;
        }

        GameCommand moveA{};
        moveA.kind = eCommandKind::Move;
        moveA.issuerEntity = viego;
        moveA.sequenceNum = 3u;
        moveA.groundPos = Vec3{ 4.f, 0.f, 0.f };
        TickContext tick3 = MakeProbeTickContext(3ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick3, moveA);
        GameCommand moveB = moveA;
        moveB.sequenceNum = 4u;
        moveB.groundPos = Vec3{ 7.f, 0.f, 0.f };
        TickContext tick4 = MakeProbeTickContext(4ull, rng, entityMap, walkable);
        executor->ExecuteCommand(world, tick4, moveB);
        if (!action.bHasQueuedMove || action.queuedMoveSequence != 4u ||
            std::fabs(action.queuedMoveTarget.x - 7.f) > 0.001f)
        {
            std::printf("[SimLab][ActionLock] FAIL: last move intent was not retained\n");
            return false;
        }

        const u64_t unlockTick = action.lockEndTick;
        for (u64_t tick = 4ull; tick <= unlockTick; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            CMoveSystem::Execute(world, tc);
        }
        if (world.GetComponent<ActionStateComponent>(viego).bHasQueuedMove ||
            !world.GetComponent<MoveTargetComponent>(viego).bHasTarget)
        {
            std::printf("[SimLab][ActionLock] FAIL: queued move was not released\n");
            return false;
        }

        auto& skills = world.GetComponent<SkillStateComponent>(viego);
        skills.slots[static_cast<u8_t>(eSkillSlot::Q)].cooldownRemaining = 5.f;
        const u32_t actionSequenceBeforeReject =
            world.GetComponent<ActionStateComponent>(viego).sequence;
        TickContext rejectTick =
            MakeProbeTickContext(unlockTick + 1ull, rng, entityMap, walkable);
        castQ.sequenceNum = 5u;
        executor->ExecuteCommand(world, rejectTick, castQ);
        if (world.GetComponent<ActionStateComponent>(viego).sequence !=
            actionSequenceBeforeReject)
        {
            std::printf("[SimLab][ActionLock] FAIL: rejected cast created an action lock\n");
            return false;
        }

        skills.slots[static_cast<u8_t>(eSkillSlot::W)] = SkillSlotRuntime{};
        TickContext w1Tick =
            MakeProbeTickContext(unlockTick + 2ull, rng, entityMap, walkable);
        GameCommand castW{};
        castW.kind = eCommandKind::CastSkill;
        castW.issuerEntity = viego;
        castW.slot = static_cast<u8_t>(eSkillSlot::W);
        castW.sequenceNum = 6u;
        castW.direction = Vec3{ 1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, w1Tick, castW);
        auto& w1Action = world.GetComponent<ActionStateComponent>(viego);
        w1Action.lockEndTick = 999ull;
        w1Action.movePolicy = eSkillActionMovePolicy::QueueUntilUnlock;

        TickContext w2Tick =
            MakeProbeTickContext(unlockTick + 3ull, rng, entityMap, walkable);
        castW.sequenceNum = 7u;
        castW.itemId = 2u;
        executor->ExecuteCommand(world, w2Tick, castW);
        const auto& w2Action = world.GetComponent<ActionStateComponent>(viego);
        if (w2Action.stage != 2u ||
            w2Action.movePolicy != eSkillActionMovePolicy::ForcedMotion ||
            w2Action.lockEndTick >= 999ull)
        {
            std::printf("[SimLab][ActionLock] FAIL: W2 did not replace prior lock\n");
            return false;
        }

        std::printf("[SimLab][ActionLock] PASS: Allow/Queue/Forced, last-input, reject, W2 replace\n");
        return true;
    }

    bool_t RunPracticeDefinitionOverlayProbe()
    {
        CWorld world;
        const EntityID championEntity = world.CreateEntity();

        ChampionComponent champion{};
        champion.id = eChampion::ANNIE;
        world.AddComponent<ChampionComponent>(championEntity, champion);

        TickContext tc{};
        tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
        const f32_t canonicalValue = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            championEntity,
            tc,
            eChampion::ANNIE,
            qSlot,
            eSkillEffectParamId::BaseDamage,
            -1.f);

        PracticeSkillEffectOverrideComponent overrides{};
        overrides.count = 1u;
        overrides.revision = 1u;
        overrides.entries[0].slot = qSlot;
        overrides.entries[0].paramId =
            static_cast<u8_t>(eSkillEffectParamId::BaseDamage);
        overrides.entries[0].value = 777.f;
        world.AddComponent<PracticeSkillEffectOverrideComponent>(championEntity, overrides);

        const f32_t overriddenValue = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            championEntity,
            tc,
            eChampion::ANNIE,
            qSlot,
            eSkillEffectParamId::BaseDamage,
            -1.f);
        if (std::fabs(overriddenValue - 777.f) > 0.001f)
        {
            std::printf(
                "[SimLab][Practice] FAIL: effect override was not resolved (%.3f)\n",
                overriddenValue);
            return false;
        }

        world.RemoveComponent<PracticeSkillEffectOverrideComponent>(championEntity);
        const f32_t restoredValue = GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            championEntity,
            tc,
            eChampion::ANNIE,
            qSlot,
            eSkillEffectParamId::BaseDamage,
            -1.f);
        if (std::fabs(restoredValue - canonicalValue) > 0.001f)
        {
            std::printf(
                "[SimLab][Practice] FAIL: clear did not restore canonical value (%.3f vs %.3f)\n",
                restoredValue,
                canonicalValue);
            return false;
        }

        EntityIdMap entityMap;
        GameCommandWire wire{};
        wire.kind = eCommandKind::PracticeControl;
        wire.sequenceNum = 42u;
        wire.practiceOperation = ePracticeOperation::SetOptions;
        wire.practiceValue = 12.5f;
        wire.practiceFlags = kPracticeNoCooldownFlag | kPracticeInfiniteManaFlag;
        const GameCommand command =
            BuildServerCommand(wire, 77u, championEntity, entityMap);
        if (command.sourceSessionId != 77u ||
            command.practiceOperation != ePracticeOperation::SetOptions ||
            std::fabs(command.practiceValue - 12.5f) > 0.001f ||
            command.practiceFlags != wire.practiceFlags)
        {
            std::printf("[SimLab][Practice] FAIL: typed command payload was not preserved\n");
            return false;
        }

        std::printf(
            "[SimLab][Practice] PASS: command payload + temporary effect overlay + clear\n");
        return true;
    }

    bool_t RunItemMoveSpeedScaleProbe()
    {
        struct MoveSpeedCase
        {
            u16_t itemId = 0u;
            f32_t expectedMoveSpeed = 0.f;
            const char* pLabel = nullptr;
        };

        static constexpr MoveSpeedCase kCases[] =
        {
            { 0u, 5.00f, "base" },
            { 1001u, 5.25f, "boots" },
            { 3006u, 5.45f, "berserkers-greaves" },
            { 3020u, 5.45f, "sorcerers-shoes" },
            { 3047u, 5.45f, "plated-steelcaps" },
            { 3742u, 5.05f, "dead-mans-plate" },
        };
        constexpr u32_t kCaseCount =
            static_cast<u32_t>(sizeof(kCases) / sizeof(kCases[0]));

        for (u32_t caseIndex = 0u; caseIndex < kCaseCount; ++caseIndex)
        {
            const MoveSpeedCase& testCase = kCases[caseIndex];
            CWorld world;
            DeterministicRng rng(20260714ull + caseIndex);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID championEntity = SpawnChampion(
                world,
                entityMap,
                eChampion::ASHE,
                static_cast<u8_t>(eTeam::Blue),
                0u);

            if (testCase.itemId != 0u)
            {
                world.GetComponent<GoldComponent>(championEntity).amount = 10000u;
                GameCommand buyItem{};
                buyItem.kind = eCommandKind::BuyItem;
                buyItem.issuerEntity = championEntity;
                buyItem.itemId = testCase.itemId;
                buyItem.sequenceNum = caseIndex + 1u;
                TickContext tc = MakeProbeTickContext(
                    caseIndex + 1u,
                    rng,
                    entityMap,
                    walkable);
                executor->ExecuteCommand(world, tc, buyItem);

                const InventoryComponent& inventory =
                    world.GetComponent<InventoryComponent>(championEntity);
                const StatComponent& dirtyStat =
                    world.GetComponent<StatComponent>(championEntity);
                if (inventory.count != 1u ||
                    inventory.itemIds[0] != testCase.itemId ||
                    !dirtyStat.bDirty)
                {
                    std::printf(
                        "[SimLab][ItemMoveSpeed] FAIL: purchase path label=%s item=%u count=%u slot0=%u dirty=%u\n",
                        testCase.pLabel,
                        static_cast<u32_t>(testCase.itemId),
                        static_cast<u32_t>(inventory.count),
                        static_cast<u32_t>(inventory.itemIds[0]),
                        dirtyStat.bDirty ? 1u : 0u);
                    return false;
                }
            }

            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent& stat =
                world.GetComponent<StatComponent>(championEntity);
            const ChampionComponent& champion =
                world.GetComponent<ChampionComponent>(championEntity);
            if (std::fabs(stat.moveSpeed - testCase.expectedMoveSpeed) > 0.001f ||
                std::fabs(champion.moveSpeed - testCase.expectedMoveSpeed) > 0.001f)
            {
                std::printf(
                    "[SimLab][ItemMoveSpeed] FAIL: label=%s item=%u stat=%.3f champion=%.3f expected=%.3f\n",
                    testCase.pLabel,
                    static_cast<u32_t>(testCase.itemId),
                    stat.moveSpeed,
                    champion.moveSpeed,
                    testCase.expectedMoveSpeed);
                return false;
            }
        }

        std::printf(
            "[SimLab][ItemMoveSpeed] PASS: BuyItem -> dirty stat -> 0.01 world scale\n");
        return true;
    }

    bool_t RunGameplayFormulaDataDrivenProbe()
    {
        const GameplayDefinitionPack& pack =
            ServerData::GetLoLGameplayDefinitionPack();
        if (pack.championCount != 17u || pack.skillCount != 85u)
        {
            std::printf(
                "[SimLab][FormulaData] FAIL: pack coverage champions=%zu skills=%zu\n",
                pack.championCount,
                pack.skillCount);
            return false;
        }

        for (std::size_t championIndex = 0u;
            championIndex < pack.championCount;
            ++championIndex)
        {
            const ChampionGameplayDef& champion = pack.champions[championIndex];
            for (u8_t slot = 0u; slot < kChampionSkillSlotCount; ++slot)
            {
                const SkillGameplayDef* pSkill = pack.FindSkill(champion.skillLoadout[slot]);
                const u8_t expectedRankCount = slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
                    ? 1u
                    : (slot == static_cast<u8_t>(eSkillSlot::R) ? 3u : 5u);
                if (!pSkill ||
                    !pSkill->effect.damage.bValid ||
                    pSkill->effect.damage.rankCount != expectedRankCount ||
                    pSkill->cost.rankCount != expectedRankCount ||
                    pSkill->cooldown.rankCount != expectedRankCount)
                {
                    std::printf(
                        "[SimLab][FormulaData] FAIL: champion=%u slot=%u missing/invalid ranked definition\n",
                        static_cast<unsigned>(champion.legacyChampion),
                        static_cast<unsigned>(slot));
                    return false;
                }

                if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                {
                    const DamageFormulaDef& damage = pSkill->effect.damage;
                    const u32_t requiredFlags = DamageFlag_CanCrit |
                        DamageFlag_CanLifesteal |
                        DamageFlag_OnHit;
                    if (damage.type != eDamageType::Physical ||
                        damage.totalAdRatioByRank[0] != 1.f ||
                        (damage.flags & requiredFlags) != requiredFlags)
                    {
                        std::printf(
                            "[SimLab][FormulaData] FAIL: champion=%u basic attack formula contract\n",
                            static_cast<unsigned>(champion.legacyChampion));
                        return false;
                    }
                }
            }
        }

        CWorld world;
        DeterministicRng rng(2026071601ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::EZREAL,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::GAREN,
            static_cast<u8_t>(eTeam::Red), 5u);
        TickContext tc = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        DamageRequest request{};
        if (!GameplayDefinitionQuery::BuildSkillDamageRequest(
                world,
                source,
                target,
                tc,
                eChampion::EZREAL,
                static_cast<u8_t>(eSkillSlot::Q),
                3u,
                eTeam::Blue,
                eDamageSourceKind::Skill,
                request) ||
            std::fabs(request.flatAmount - 70.f) > 0.001f ||
            std::fabs(request.adRatioOverride - 1.3f) > 0.001f ||
            std::fabs(request.apRatioOverride - 0.4f) > 0.001f ||
            request.type != eDamageType::Physical ||
            (request.flags & DamageFlag_OnHit) == 0u)
        {
            std::printf(
                "[SimLab][FormulaData] FAIL: Ezreal Q rank-3 request was not built from the pack\n");
            return false;
        }

        std::printf(
            "[SimLab][FormulaData] PASS: 17 champions, 85 skills, ranked costs/cooldowns/damage\n");
        return true;
    }

    bool_t RunBladeOfTheRuinedKingProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071602ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::GAREN,
            static_cast<u8_t>(eTeam::Red), 5u);

        auto& inventory = world.GetComponent<InventoryComponent>(source);
        inventory.itemIds[0] = 3153u;
        inventory.count = 1u;

        auto& sourceStat = world.GetComponent<StatComponent>(source);
        sourceStat.baseAd = 100.f;
        sourceStat.bonusAd = 0.f;
        sourceStat.ad = 100.f;
        sourceStat.lifesteal = 0.20f;
        sourceStat.critChance = 0.f;
        sourceStat.bDirty = false;
        auto& sourceHealth = world.GetComponent<HealthComponent>(source);
        sourceHealth.fMaximum = 1000.f;
        sourceHealth.fCurrent = 500.f;

        auto& targetStat = world.GetComponent<StatComponent>(target);
        targetStat.baseArmor = 0.f;
        targetStat.bonusArmor = 0.f;
        targetStat.armor = 0.f;
        targetStat.bDirty = false;
        auto& targetHealth = world.GetComponent<HealthComponent>(target);
        targetHealth.fMaximum = 1000.f;
        targetHealth.fCurrent = 1000.f;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = eTeam::Blue;
        request.type = eDamageType::Physical;
        request.flatAmount = 100.f;
        request.flags = DamageFlag_CanLifesteal | DamageFlag_OnHit;
        request.eSourceKind = eDamageSourceKind::BasicAttack;
        request.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        request.skillId = static_cast<u16_t>(
            static_cast<u16_t>(eChampion::JAX) << 8u);
        EnqueueDamageRequest(world, request);

        TickContext tc = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        CDamageQueueSystem::Execute(world, tc);
        if (std::fabs(targetHealth.fCurrent - 800.f) > 0.001f ||
            std::fabs(sourceHealth.fCurrent - 540.f) > 0.001f)
        {
            std::printf(
                "[SimLab][BORK] FAIL: targetHp=%.3f sourceHp=%.3f expected 800/540\n",
                targetHealth.fCurrent,
                sourceHealth.fCurrent);
            return false;
        }

        std::printf(
            "[SimLab][BORK] PASS: one 10%% max-HP on-hit and post-mitigation lifesteal\n");
        return true;
    }

    bool_t RunSkillRankGateProbe()
    {
        SkillRankComponent ranks{};
        ranks.pointsAvailable = 1u;
        if (!CSkillRankSystem::TryLevelSkill(
                ranks, 1u, static_cast<u8_t>(eSkillSlot::Q)))
        {
            std::printf("[SimLab][SkillRank] FAIL: Q rank 1 rejected at level 1\n");
            return false;
        }

        ranks.pointsAvailable = 1u;
        if (CSkillRankSystem::TryLevelSkill(
                ranks, 2u, static_cast<u8_t>(eSkillSlot::Q)) ||
            ranks.pointsAvailable != 1u ||
            !CSkillRankSystem::TryLevelSkill(
                ranks, 3u, static_cast<u8_t>(eSkillSlot::Q)))
        {
            std::printf("[SimLab][SkillRank] FAIL: Q 1/3 level gate or point preservation\n");
            return false;
        }

        ranks.pointsAvailable = 1u;
        if (CSkillRankSystem::TryLevelSkill(
                ranks, 5u, static_cast<u8_t>(eSkillSlot::R)) ||
            !CSkillRankSystem::TryLevelSkill(
                ranks, 6u, static_cast<u8_t>(eSkillSlot::R)))
        {
            std::printf("[SimLab][SkillRank] FAIL: R rank 1 level-6 gate\n");
            return false;
        }
        ranks.pointsAvailable = 1u;
        if (!CSkillRankSystem::TryLevelSkill(
                ranks, 11u, static_cast<u8_t>(eSkillSlot::R)))
        {
            std::printf("[SimLab][SkillRank] FAIL: R rank 2 level-11 gate\n");
            return false;
        }
        ranks.pointsAvailable = 1u;
        if (!CSkillRankSystem::TryLevelSkill(
                ranks, 16u, static_cast<u8_t>(eSkillSlot::R)))
        {
            std::printf("[SimLab][SkillRank] FAIL: R rank 3 level-16 gate\n");
            return false;
        }

        std::printf(
            "[SimLab][SkillRank] PASS: Q/W/E 1/3/... and R 6/11/16 gates\n");
        return true;
    }

    bool_t RunAttackSpeedLabMatrixProbe()
    {
        static constexpr f32_t kTargetAttackSpeeds[] =
        {
            0.8f, 1.0f, 1.5f, 2.0f, 2.5f,
        };

        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };
        const auto ResolveExpectedTicks = [](f32_t durationSec, f32_t speedScale)
        {
            const f32_t scaledSec = std::clamp(
                durationSec / speedScale,
                DeterministicTime::kFixedDt,
                5.f);
            const u64_t ticks = static_cast<u64_t>(std::ceil(
                static_cast<f64_t>(scaledSec) *
                static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
            return ticks > 0u ? ticks : 1u;
        };

        const ChampionBasicAttackTimingDefaults attackTiming =
            GetDefaultChampionBasicAttackTiming(eChampion::IRELIA);
        u32_t sequence = 1u;

        for (const f32_t targetAttackSpeed : kTargetAttackSpeeds)
        {
            CWorld world;
            DeterministicRng rng(20260714ull + sequence);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();

            const EntityID attacker = SpawnChampion(
                world,
                entityMap,
                eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            const EntityID target = SpawnChampion(
                world,
                entityMap,
                eChampion::JAX,
                static_cast<u8_t>(eTeam::Red),
                5u);
            world.GetComponent<TransformComponent>(attacker).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 1.f, 0.f, 0.f });

            BuffComponent attackSpeedBuff{};
            attackSpeedBuff.count = 1u;
            attackSpeedBuff.buffs[0].stackCount = 1u;
            attackSpeedBuff.buffs[0].bonusAttackSpeedPerStack = 0.5f;
            world.AddComponent<BuffComponent>(attacker, attackSpeedBuff);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent canonicalStat =
                world.GetComponent<StatComponent>(attacker);

            PracticeChampionStatOverrideComponent overrides{};
            overrides.count = 1u;
            overrides.revision = sequence;
            overrides.entries[0].statId = static_cast<u8_t>(
                eChampionStatOverrideId::EffectiveAttackSpeed);
            overrides.entries[0].value = targetAttackSpeed;
            world.AddComponent<PracticeChampionStatOverrideComponent>(
                attacker,
                overrides);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());

            const StatComponent& appliedStat =
                world.GetComponent<StatComponent>(attacker);
            if (!NearlyEqual(appliedStat.attackSpeed, targetAttackSpeed) ||
                !NearlyEqual(appliedStat.baseAttackSpeed, canonicalStat.baseAttackSpeed) ||
                !NearlyEqual(appliedStat.attackSpeedRatio, canonicalStat.attackSpeedRatio))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: stat target=%.3f effective=%.3f base=%.3f canonicalBase=%.3f ratio=%.3f canonicalRatio=%.3f\n",
                    targetAttackSpeed,
                    appliedStat.attackSpeed,
                    appliedStat.baseAttackSpeed,
                    canonicalStat.baseAttackSpeed,
                    appliedStat.attackSpeedRatio,
                    canonicalStat.attackSpeedRatio);
                return false;
            }

            const f32_t speedBase = appliedStat.baseAttackSpeed > 0.001f
                ? appliedStat.baseAttackSpeed
                : appliedStat.attackSpeedRatio;
            const f32_t expectedSpeedScale = std::clamp(
                targetAttackSpeed / speedBase,
                0.2f,
                4.f);
            const u64_t expectedActionTicks = ResolveExpectedTicks(
                attackTiming.fActionDurationSec,
                expectedSpeedScale);
            const u64_t expectedWindupTicks = (std::min)(
                expectedActionTicks,
                ResolveExpectedTicks(
                    attackTiming.fWindupSec,
                    expectedSpeedScale));
            const f32_t expectedCooldown = std::clamp(
                1.f / targetAttackSpeed,
                0.333f,
                5.f);

            TickContext attackTick = MakeProbeTickContext(
                100ull + sequence,
                rng,
                entityMap,
                walkable);
            GameCommand attack{};
            attack.kind = eCommandKind::BasicAttack;
            attack.issuerEntity = attacker;
            attack.targetEntity = target;
            attack.direction = Vec3{ 1.f, 0.f, 0.f };
            attack.sequenceNum = sequence;
            attack.issuedAtTick = attackTick.tickIndex;
            const CommandExecutionResult result =
                executor->ExecuteCommand(world, attackTick, attack);
            if (result.state != eCommandExecutionState::Accepted ||
                !world.HasComponent<CombatActionComponent>(attacker))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: attack rejected target=%.3f state=%u reason=%u\n",
                    targetAttackSpeed,
                    static_cast<u32_t>(result.state),
                    static_cast<u32_t>(result.reason));
                return false;
            }

            const SkillSlotRuntime& basicAttackSlot =
                world.GetComponent<SkillStateComponent>(attacker).slots[0];
            const CombatActionComponent& action =
                world.GetComponent<CombatActionComponent>(attacker);
            const u64_t actualActionTicks = action.uEndTick - action.uStartTick;
            const u64_t actualWindupTicks = action.uImpactTick - action.uStartTick;
            if (!NearlyEqual(basicAttackSlot.cooldownRemaining, expectedCooldown) ||
                !NearlyEqual(basicAttackSlot.cooldownDuration, expectedCooldown) ||
                action.eKind != eCombatActionKind::BasicAttack ||
                action.uStartTick != attackTick.tickIndex ||
                actualActionTicks != expectedActionTicks ||
                actualWindupTicks != expectedWindupTicks)
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: timing target=%.3f cooldown=%.3f/%.3f action=%llu/%llu windup=%llu/%llu\n",
                    targetAttackSpeed,
                    basicAttackSlot.cooldownDuration,
                    expectedCooldown,
                    static_cast<unsigned long long>(actualActionTicks),
                    static_cast<unsigned long long>(expectedActionTicks),
                    static_cast<unsigned long long>(actualWindupTicks),
                    static_cast<unsigned long long>(expectedWindupTicks));
                return false;
            }

            world.RemoveComponent<PracticeChampionStatOverrideComponent>(attacker);
            world.GetComponent<StatComponent>(attacker).bDirty = true;
            CStatSystem::Execute(
                world,
                ServerData::GetLoLGameplayDefinitionPack());
            const StatComponent& restoredStat =
                world.GetComponent<StatComponent>(attacker);
            if (!NearlyEqual(restoredStat.attackSpeed, canonicalStat.attackSpeed) ||
                !NearlyEqual(restoredStat.baseAttackSpeed, canonicalStat.baseAttackSpeed) ||
                !NearlyEqual(restoredStat.attackSpeedRatio, canonicalStat.attackSpeedRatio))
            {
                std::printf(
                    "[SimLab][AttackSpeedLab] FAIL: clear target=%.3f restored=%.3f canonical=%.3f\n",
                    targetAttackSpeed,
                    restoredStat.attackSpeed,
                    canonicalStat.attackSpeed);
                return false;
            }

            std::printf(
                "[SimLab][AttackSpeedLab] target=%.3f cooldown=%.3f actionTicks=%llu windupTicks=%llu\n",
                targetAttackSpeed,
                expectedCooldown,
                static_cast<unsigned long long>(actualActionTicks),
                static_cast<unsigned long long>(actualWindupTicks));
            ++sequence;
        }

        std::printf(
            "[SimLab][AttackSpeedLab] PASS: effective 0.8..2.5, authoritative cadence/action/windup, clear restore\n");
        return true;
    }

    bool_t RunAuthoredNavGridProbe()
    {
        static constexpr const wchar_t* kCandidates[] =
        {
            L"Data\\Stage1.navgrid",
            L"..\\..\\Data\\Stage1.navgrid",
            L"..\\..\\..\\Data\\Stage1.navgrid",
        };

        std::unique_ptr<Engine::CNavGrid> navGrid;
        for (const wchar_t* pPath : kCandidates)
        {
            navGrid = Engine::CNavGrid::LoadFromFile(pPath);
            if (navGrid)
                break;
        }

        if (!navGrid)
        {
            std::printf("[SimLab][NavGrid] FAIL: Data/Stage1.navgrid was not loadable\n");
            return false;
        }

        const Vec3 a{ 94.75f, 0.f, -67.25f };
        const Vec3 b{ 95.75f, 0.f, -67.25f };
        const auto blockedCell = navGrid->WorldToCell(Vec3{ 95.75f, 0.f, -67.25f });
        if (blockedCell.x != 238 || blockedCell.y != 121 ||
            navGrid->IsWalkable(237, 121) ||
            navGrid->SegmentWalkable(a, b, 0.f))
        {
            std::printf(
                "[SimLab][NavGrid] FAIL: wall fixture cell=(%d,%d) walkable=%u segment=%u\n",
                blockedCell.x,
                blockedCell.y,
                navGrid->IsWalkable(237, 121) ? 1u : 0u,
                navGrid->SegmentWalkable(a, b, 0.f) ? 1u : 0u);
            return false;
        }

        std::printf(
            "[SimLab][NavGrid] PASS: authored cells=%u blocked=%u hash=%08X blocked=(237,121) endpoint=(238,121)\n",
            navGrid->CountWalkableCells(),
            Engine::CNavGrid::kTotalCells - navGrid->CountWalkableCells(),
            navGrid->ComputeContentHash());
        return true;
    }

    // Chrono Break P1 골든 테스트: save(K) -> restore -> replay(K+1..N) 해시가
    // 무중단 실행과 원소 단위로 동일해야 한다. 봇/시스템 명령은 저널 없이
    // 복원된 상태+RNG에서 재생성된다는 설계 원리를 그대로 검증한다.
    struct KeyframeStoreRecordOffsets
    {
        size_t hashOffset = 0u;
        size_t payloadOffset = 0u;
        size_t payloadSize = 0u;
    };

    struct KeyframeHeaderOffsets
    {
        size_t freeHeadOffset = 0u;
        size_t aliveCountOffset = 0u;
        size_t nextNetIdOffset = 0u;
        size_t bindingDataOffset = 0u;
        size_t bindingCount = 0u;
    };

    bool_t LocateKeyframeHeaderOffsets(
        const std::vector<u8_t>& bytes,
        KeyframeHeaderOffsets& out)
    {
        const u8_t* p = bytes.data();
        const u8_t* pEnd = bytes.data() + bytes.size();
        u64_t ignored = 0u;
        for (u32_t i = 0u; i < 4u; ++i)
        {
            if (!SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
                return false;
        }

        std::vector<CEntityManager::EntitySlot> slots;
        if (!SimCheckpoint::Detail::ReadVector(p, pEnd, slots))
            return false;
        out.freeHeadOffset = static_cast<size_t>(p - bytes.data());
        if (!SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
            return false;
        out.aliveCountOffset = static_cast<size_t>(p - bytes.data());
        if (!SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
            return false;
        out.nextNetIdOffset = static_cast<size_t>(p - bytes.data());
        if (!SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
            return false;

        u64_t bindingCount = 0u;
        if (!SimCheckpoint::Detail::ReadU64(p, pEnd, bindingCount) ||
            bindingCount > static_cast<u64_t>((std::numeric_limits<size_t>::max)()) ||
            static_cast<size_t>(bindingCount) >
                static_cast<size_t>(pEnd - p) /
                    sizeof(std::pair<u32_t, u32_t>))
        {
            return false;
        }
        out.bindingDataOffset = static_cast<size_t>(p - bytes.data());
        out.bindingCount = static_cast<size_t>(bindingCount);
        return true;
    }

    bool_t LocateKeyframeStoreRecord(const std::vector<u8_t>& bytes,
        u64_t targetHash, KeyframeStoreRecordOffsets& out)
    {
        if (bytes.size() < sizeof(u64_t) * 4u)
            return false;

        const u8_t* p = bytes.data();
        const u8_t* pEnd = bytes.data() + bytes.size();
        u64_t ignored = 0;
        for (u32_t i = 0; i < 4u; ++i)
        {
            if (!SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
                return false;
        }

        std::vector<CEntityManager::EntitySlot> slots;
        if (!SimCheckpoint::Detail::ReadVector(p, pEnd, slots) ||
            !SimCheckpoint::Detail::ReadU64(p, pEnd, ignored) ||
            !SimCheckpoint::Detail::ReadU64(p, pEnd, ignored) ||
            !SimCheckpoint::Detail::ReadU64(p, pEnd, ignored))
        {
            return false;
        }

        std::vector<std::pair<u32_t, u32_t>> bindings;
        if (!SimCheckpoint::Detail::ReadVector(p, pEnd, bindings))
            return false;

        u64_t storeCount = 0;
        if (!SimCheckpoint::Detail::ReadU64(p, pEnd, storeCount))
            return false;

        for (u64_t i = 0; i < storeCount; ++i)
        {
            const size_t hashOffset = static_cast<size_t>(p - bytes.data());
            u64_t nameHash = 0, payloadSize = 0;
            if (!SimCheckpoint::Detail::ReadU64(p, pEnd, nameHash) ||
                !SimCheckpoint::Detail::ReadU64(p, pEnd, payloadSize) ||
                payloadSize > static_cast<u64_t>((std::numeric_limits<size_t>::max)()) ||
                static_cast<size_t>(pEnd - p) < static_cast<size_t>(payloadSize))
            {
                return false;
            }

            if (nameHash == targetHash)
            {
                out.hashOffset = hashOffset;
                out.payloadOffset = static_cast<size_t>(p - bytes.data());
                out.payloadSize = static_cast<size_t>(payloadSize);
                return true;
            }
            p += static_cast<size_t>(payloadSize);
        }
        return false;
    }

    bool_t RunKeyframeTransactionalFailureProbe()
    {
        CWorld liveWorld;
        liveWorld.Initialize_Spatial(DefaultSpatialGridDesc());
        CSpatialIndex* const pLiveSpatialIndex = liveWorld.Get_SpatialIndex();
        DeterministicRng liveRng(0x1111222233334444ull);
        EntityIdMap liveEntityMap;
        const EntityID liveEntity = liveWorld.CreateEntity();
        TransformComponent liveTransform{};
        liveTransform.SetPosition(Vec3{ -7.f, 0.f, 3.f });
        liveWorld.AddComponent<TransformComponent>(liveEntity, liveTransform);
        liveWorld.AddComponent<HealthComponent>(
            liveEntity, HealthComponent{ 73.f, 125.f, false });
        liveWorld.AddComponent<GoldComponent>(liveEntity, GoldComponent{ 913u });
        const NetEntityId liveNetId = liveEntityMap.IssueNew(liveEntity);
        liveRng.NextU64();
        liveRng.NextU64();

        constexpr u64_t kLiveTick = 0x1234ull;
        u64_t liveTick = kLiveTick;
        const u64_t liveRngState = liveRng.GetState();
        const NetEntityId liveNextNetId = liveEntityMap.GetNextNetId();
        std::vector<u8_t> liveBytesBefore;
        if (!SimCheckpoint::SaveWorldKeyframe(
            liveWorld, liveRng, liveEntityMap, liveTick, liveBytesBefore))
        {
            std::printf("[SimLab][KeyframeAtomic] FAIL: live baseline save failed\n");
            return false;
        }

        CWorld sourceWorld;
        DeterministicRng sourceRng(0xAAAABBBBCCCCDDDDull);
        EntityIdMap sourceEntityMap;
        const EntityID sourceA = sourceWorld.CreateEntity();
        const EntityID sourceB = sourceWorld.CreateEntity();
        TransformComponent sourceTransformA{};
        sourceTransformA.SetPosition(Vec3{ 41.f, 0.f, -19.f });
        TransformComponent sourceTransformB{};
        sourceTransformB.SetPosition(Vec3{ 44.f, 0.f, -23.f });
        sourceWorld.AddComponent<TransformComponent>(sourceA, sourceTransformA);
        sourceWorld.AddComponent<TransformComponent>(sourceB, sourceTransformB);
        sourceWorld.AddComponent<HealthComponent>(
            sourceA, HealthComponent{ 11.f, 900.f, false });
        sourceWorld.AddComponent<HealthComponent>(
            sourceB, HealthComponent{ 0.f, 500.f, true });
        sourceWorld.AddComponent<GoldComponent>(sourceA, GoldComponent{ 9999u });
        sourceEntityMap.IssueNew(sourceA);
        sourceEntityMap.IssueNew(sourceB);
        sourceRng.NextU64();

        std::vector<u8_t> validSourceBytes;
        if (!SimCheckpoint::SaveWorldKeyframe(
            sourceWorld, sourceRng, sourceEntityMap, 0xBEEFull, validSourceBytes))
        {
            std::printf("[SimLab][KeyframeAtomic] FAIL: source save failed\n");
            return false;
        }

        KeyframeStoreRecordOffsets healthRecord{};
        KeyframeHeaderOffsets headerOffsets{};
        if (!LocateKeyframeStoreRecord(
            validSourceBytes,
            SimCheckpoint::Fnv1a64("HealthComponent"),
            healthRecord) ||
            healthRecord.payloadSize < sizeof(u64_t) ||
            !LocateKeyframeHeaderOffsets(validSourceBytes, headerOffsets) ||
            headerOffsets.bindingCount < 2u)
        {
            std::printf("[SimLab][KeyframeAtomic] FAIL: HealthComponent record was not located\n");
            return false;
        }

        const auto expectAtomicFailure = [&](const char* pLabel,
            const std::vector<u8_t>& corruptBytes) -> bool_t
        {
            if (SimCheckpoint::RestoreWorldKeyframe(
                liveWorld, liveRng, liveEntityMap, liveTick, corruptBytes))
            {
                std::printf(
                    "[SimLab][KeyframeAtomic] FAIL: %s unexpectedly restored\n",
                    pLabel);
                return false;
            }

            std::vector<u8_t> liveBytesAfter;
            if (!SimCheckpoint::SaveWorldKeyframe(
                liveWorld, liveRng, liveEntityMap, liveTick, liveBytesAfter) ||
                liveBytesAfter != liveBytesBefore ||
                liveRng.GetState() != liveRngState ||
                liveTick != kLiveTick ||
                liveEntityMap.ToNet(liveEntity) != liveNetId ||
                liveEntityMap.GetNextNetId() != liveNextNetId ||
                liveWorld.Get_SpatialIndex() != pLiveSpatialIndex)
            {
                std::printf(
                    "[SimLab][KeyframeAtomic] FAIL: %s changed live world/RNG/map/tick\n",
                    pLabel);
                return false;
            }
            return true;
        };

        std::vector<u8_t> truncatedBytes = validSourceBytes;
        truncatedBytes.pop_back();
        if (!expectAtomicFailure("truncated", truncatedBytes))
            return false;

        std::vector<u8_t> unknownHashBytes = validSourceBytes;
        u64_t unknownHash = 0u;
        const auto& registry = SimCheckpoint::KeyframeComponentRegistry::Get();
        while (registry.FindByHash(unknownHash))
            ++unknownHash;
        std::memcpy(
            unknownHashBytes.data() + healthRecord.hashOffset,
            &unknownHash,
            sizeof(unknownHash));
        if (!expectAtomicFailure("unknown-store-hash", unknownHashBytes))
            return false;

        std::vector<u8_t> invalidPayloadBytes = validSourceBytes;
        const u64_t impossibleVectorCount = (std::numeric_limits<u64_t>::max)();
        std::memcpy(
            invalidPayloadBytes.data() + healthRecord.payloadOffset,
            &impossibleVectorCount,
            sizeof(impossibleVectorCount));
        if (!expectAtomicFailure("invalid-store-payload", invalidPayloadBytes))
            return false;

        std::vector<u8_t> invalidFreeListBytes = validSourceBytes;
        const u64_t invalidFreeHead = (std::numeric_limits<u32_t>::max)();
        std::memcpy(
            invalidFreeListBytes.data() + headerOffsets.freeHeadOffset,
            &invalidFreeHead,
            sizeof(invalidFreeHead));
        if (!expectAtomicFailure("invalid-entity-free-list", invalidFreeListBytes))
            return false;

        std::vector<u8_t> duplicateBindingBytes = validSourceBytes;
        const std::pair<u32_t, u32_t> duplicateEntityBinding{
            2u,
            static_cast<u32_t>(sourceA)
        };
        std::memcpy(
            duplicateBindingBytes.data() +
                headerOffsets.bindingDataOffset +
                sizeof(std::pair<u32_t, u32_t>),
            &duplicateEntityBinding,
            sizeof(duplicateEntityBinding));
        if (!expectAtomicFailure("duplicate-entity-binding", duplicateBindingBytes))
            return false;

        std::vector<u8_t> invalidNextNetIdBytes = validSourceBytes;
        const u64_t invalidNextNetId = 1u;
        std::memcpy(
            invalidNextNetIdBytes.data() + headerOffsets.nextNetIdOffset,
            &invalidNextNetId,
            sizeof(invalidNextNetId));
        if (!expectAtomicFailure("invalid-next-net-id", invalidNextNetIdBytes))
            return false;

        std::vector<u8_t> invalidStoreTopologyBytes = validSourceBytes;
        const u8_t* topologyCursor =
            invalidStoreTopologyBytes.data() + healthRecord.payloadOffset;
        const u8_t* topologyEnd = topologyCursor + healthRecord.payloadSize;
        std::vector<u32_t> topologySparse;
        u64_t topologyDenseCount = 0u;
        if (!SimCheckpoint::Detail::ReadVector(
                topologyCursor,
                topologyEnd,
                topologySparse) ||
            !SimCheckpoint::Detail::ReadU64(
                topologyCursor,
                topologyEnd,
                topologyDenseCount) ||
            topologyDenseCount < 2u ||
            static_cast<size_t>(topologyEnd - topologyCursor) <
                2u * sizeof(EntityID))
        {
            std::printf(
                "[SimLab][KeyframeAtomic] FAIL: HealthComponent dense topology was not located\n");
            return false;
        }
        std::memcpy(
            invalidStoreTopologyBytes.data() +
                static_cast<size_t>(topologyCursor -
                    invalidStoreTopologyBytes.data()) +
                sizeof(EntityID),
            &sourceA,
            sizeof(sourceA));
        if (!expectAtomicFailure(
                "duplicate-component-dense-entity",
                invalidStoreTopologyBytes))
        {
            return false;
        }

        if (!SimCheckpoint::RestoreWorldKeyframe(
            liveWorld, liveRng, liveEntityMap, liveTick, validSourceBytes) ||
            liveTick != 0xBEEFull ||
            liveWorld.GetEntityCount() != 2u ||
            liveWorld.Get_SpatialIndex() != pLiveSpatialIndex)
        {
            std::printf(
                "[SimLab][KeyframeAtomic] FAIL: successful commit did not preserve the spatial index\n");
            return false;
        }

        std::printf(
            "[SimLab][KeyframeAtomic] PASS: semantic topology/bindings, failure atomicity, spatial-index preservation\n");
        return true;
    }

    bool_t RunEntityIdMapBijectionProbe()
    {
        CWorld world;
        const EntityID entityA = world.CreateEntity();
        const EntityID entityB = world.CreateEntity();
        EntityIdMap entityMap;
        const NetEntityId netA = entityMap.IssueNew(entityA);
        const NetEntityId netB = entityMap.IssueNew(entityB);

        entityMap.Bind(netA, entityB);
        const bool_t bPass =
            entityMap.FromNet(netA) == entityB &&
            entityMap.ToNet(entityB) == netA &&
            entityMap.FromNet(netB) == NULL_ENTITY &&
            entityMap.ToNet(entityA) == NULL_NET_ENTITY;
        std::printf(
            "[SimLab][EntityIdMap] %s: rebind preserves net/entity bijection\n",
            bPass ? "PASS" : "FAIL");
        return bPass;
    }

    bool_t RunCommandExecutionOutcomeTraceProbe()
    {
        CWorld world;
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();
        const EntityID bot = SpawnChampion(
            world,
            entityMap,
            eChampion::EZREAL,
            0u,
            0u);
        const EntityID castTarget = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            1u,
            5u);
        world.AddComponent<ChampionAIComponent>(bot, ChampionAIComponent{});
        world.AddComponent<ChampionAIResearchDebugComponent>(
            bot,
            ChampionAIResearchDebugComponent{});

        TickContext tc{};
        tc.tickIndex = 17u;
        tc.pEntityMap = &entityMap;
        tc.pWalkable = &walkable;
        tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

        auto& researchState =
            world.GetComponent<ChampionAIResearchDebugComponent>(bot);
        auto& aiState = world.GetComponent<ChampionAIComponent>(bot);
        const auto appendSubmitted = [&](u32_t sequence) -> u8_t
        {
            const u8_t index = researchState.decisionTraceHead;
            AiDecisionTraceV1& trace = researchState.decisionTrace[index];
            trace = ChampionAIResearch::MakeDecisionTraceV1();
            trace.executorState = static_cast<u8_t>(
                AiExecutorStateV1::Submitted);
            trace.commandSequence = sequence;
            researchState.decisionTraceHead = static_cast<u8_t>(
                (researchState.decisionTraceHead + 1u) %
                kChampionAIDebugTraceCapacity);
            if (researchState.decisionTraceCount < kChampionAIDebugTraceCapacity)
                ++researchState.decisionTraceCount;

            const u8_t legacyIndex = aiState.debugDecisionTraceHead;
            ChampionAIDecisionTraceEntry& legacy =
                aiState.debugDecisionTrace[legacyIndex];
            legacy = ChampionAIDecisionTraceEntry{};
            legacy.commandSequence = sequence;
            legacy.executorState = static_cast<u8_t>(
                AiExecutorStateV1::Submitted);
            legacy.comboStep = aiState.comboStep;
            aiState.debugDecisionTraceHead = static_cast<u8_t>(
                (aiState.debugDecisionTraceHead + 1u) %
                kChampionAIDebugTraceCapacity);
            if (aiState.debugDecisionTraceCount < kChampionAIDebugTraceCapacity)
                ++aiState.debugDecisionTraceCount;
            return index;
        };
        const auto seedDraft = [&](u32_t sequence)
        {
            researchState.decisionDraft =
                ChampionAIResearch::MakeDecisionTraceV1();
            researchState.decisionDraft.executorState = static_cast<u8_t>(
                AiExecutorStateV1::Submitted);
            researchState.decisionDraft.commandSequence = sequence;
        };

        const Vec3 origin =
            world.GetComponent<TransformComponent>(bot).GetPosition();
        world.GetComponent<TransformComponent>(castTarget).SetPosition(
            Vec3{ origin.x + 1.f, origin.y, origin.z });

        constexpr u32_t kAcceptedSequence = 100u;
        const u8_t acceptedIndex = appendSubmitted(kAcceptedSequence);
        seedDraft(kAcceptedSequence);
        GameCommand acceptedMove{};
        acceptedMove.kind = eCommandKind::Move;
        acceptedMove.issuerEntity = bot;
        acceptedMove.sequenceNum = kAcceptedSequence;
        acceptedMove.groundPos = Vec3{ origin.x + 2.f, origin.y, origin.z };
        const CommandExecutionResult acceptedResult =
            executor->ExecuteCommand(world, tc, acceptedMove);
        const AiDecisionTraceV1& acceptedTrace =
            researchState.decisionTrace[acceptedIndex];
        const ChampionAIDecisionTraceEntry& acceptedLegacy =
            aiState.debugDecisionTrace[acceptedIndex];
        if (acceptedResult.state != eCommandExecutionState::Accepted ||
            acceptedResult.commandSequence != kAcceptedSequence ||
            acceptedTrace.executorState != static_cast<u8_t>(AiExecutorStateV1::Accepted) ||
            acceptedTrace.executorReason != static_cast<u16_t>(eCommandExecutionReason::None) ||
            researchState.decisionDraft.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Accepted) ||
            acceptedLegacy.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Accepted) ||
            acceptedLegacy.executorReason !=
                static_cast<u16_t>(eCommandExecutionReason::None))
        {
            std::printf("[SimLab][CommandOutcome] FAIL: accepted move did not close matching trace/draft\n");
            return false;
        }

        constexpr u32_t kRejectedSequence = 101u;
        const u8_t rejectedIndex = appendSubmitted(kRejectedSequence);
        constexpr u32_t kUnrelatedDraftSequence = 901u;
        seedDraft(kUnrelatedDraftSequence);
        GameCommand rejectedMove{};
        rejectedMove.kind = eCommandKind::Move;
        rejectedMove.issuerEntity = bot;
        rejectedMove.sequenceNum = kRejectedSequence;
        rejectedMove.groundPos = Vec3{
            (std::numeric_limits<f32_t>::quiet_NaN)(),
            0.f,
            0.f
        };
        const CommandExecutionResult rejectedResult =
            executor->ExecuteCommand(world, tc, rejectedMove);
        const AiDecisionTraceV1& rejectedTrace =
            researchState.decisionTrace[rejectedIndex];
        const ChampionAIDecisionTraceEntry& rejectedLegacy =
            aiState.debugDecisionTrace[rejectedIndex];
        if (rejectedResult.state != eCommandExecutionState::Rejected ||
            rejectedResult.reason != eCommandExecutionReason::InvalidPayload ||
            rejectedTrace.executorState != static_cast<u8_t>(AiExecutorStateV1::Rejected) ||
            rejectedTrace.executorReason !=
                static_cast<u16_t>(eCommandExecutionReason::InvalidPayload) ||
            researchState.decisionDraft.commandSequence != kUnrelatedDraftSequence ||
            researchState.decisionDraft.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Submitted) ||
            rejectedLegacy.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Rejected) ||
            rejectedLegacy.executorReason !=
                static_cast<u16_t>(eCommandExecutionReason::InvalidPayload))
        {
            std::printf("[SimLab][CommandOutcome] FAIL: reject reason or sequence isolation mismatch\n");
            return false;
        }

        constexpr u32_t kUnmatchedTraceSequence = 102u;
        const u8_t unmatchedIndex = appendSubmitted(kUnmatchedTraceSequence);
        seedDraft(kUnmatchedTraceSequence);
        GameCommand unmatchedMove = acceptedMove;
        unmatchedMove.sequenceNum = 103u;
        unmatchedMove.groundPos = Vec3{ origin.x + 3.f, origin.y, origin.z };
        const CommandExecutionResult unmatchedResult =
            executor->ExecuteCommand(world, tc, unmatchedMove);
        if (unmatchedResult.state != eCommandExecutionState::Accepted ||
            researchState.decisionTrace[unmatchedIndex].executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Submitted) ||
            researchState.decisionDraft.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Submitted))
        {
            std::printf("[SimLab][CommandOutcome] FAIL: non-matching sequence overwrote pending trace\n");
            return false;
        }

        constexpr u32_t kPendingSequence = 104u;
        const u8_t pendingIndex = appendSubmitted(kPendingSequence);
        seedDraft(kPendingSequence);
        GameCommand unconvertedCommand{};
        unconvertedCommand.kind = eCommandKind::LevelSkill;
        unconvertedCommand.issuerEntity = bot;
        unconvertedCommand.sequenceNum = kPendingSequence;
        unconvertedCommand.slot = static_cast<u8_t>(eSkillSlot::Q);
        const CommandExecutionResult pendingResult =
            executor->ExecuteCommand(world, tc, unconvertedCommand);
        if (pendingResult.state != eCommandExecutionState::Unknown ||
            researchState.decisionTrace[pendingIndex].executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Submitted) ||
            researchState.decisionDraft.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Submitted))
        {
            std::printf("[SimLab][CommandOutcome] FAIL: unknown handler fabricated a final outcome\n");
            return false;
        }

        auto& ranks = world.GetComponent<SkillRankComponent>(bot);
        constexpr u8_t kCadenceProbeSlot = static_cast<u8_t>(eSkillSlot::Q);
        ranks.ranks[kCadenceProbeSlot] = 1u;
        aiState.fSkillCastMinInterval = 3.25f;
        aiState.fSkillCastCooldownTimer = 0.f;
        aiState.comboStep = 2u;
        constexpr u32_t kAcceptedCastSequence = 105u;
        const u8_t acceptedCastIndex = appendSubmitted(kAcceptedCastSequence);
        seedDraft(kAcceptedCastSequence);
        GameCommand acceptedCast{};
        acceptedCast.kind = eCommandKind::CastSkill;
        acceptedCast.issuerEntity = bot;
        acceptedCast.sequenceNum = kAcceptedCastSequence;
        acceptedCast.slot = kCadenceProbeSlot;
        acceptedCast.targetEntity = castTarget;
        acceptedCast.groundPos =
            world.GetComponent<TransformComponent>(castTarget).GetPosition();
        acceptedCast.direction = WintersMath::DirectionXZ(origin, acceptedCast.groundPos);
        const CommandExecutionResult acceptedCastResult =
            executor->ExecuteCommand(world, tc, acceptedCast);
        const ChampionAIDecisionTraceEntry& acceptedCastLegacy =
            aiState.debugDecisionTrace[acceptedCastIndex];
        if (acceptedCastResult.state != eCommandExecutionState::Accepted ||
            std::fabs(aiState.fSkillCastCooldownTimer - 3.25f) > 0.0001f ||
            acceptedCastLegacy.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Accepted) ||
            acceptedCastLegacy.executorReason !=
                static_cast<u16_t>(eCommandExecutionReason::None))
        {
            std::printf(
                "[SimLab][CommandOutcome] FAIL: accepted fresh cast did not commit configured policy interval\n");
            return false;
        }

        tc.tickIndex = 1000u;
        if (world.HasComponent<EzrealPendingCastComponent>(bot))
            world.RemoveComponent<EzrealPendingCastComponent>(bot);
        if (world.HasComponent<ActionStateComponent>(bot))
            world.RemoveComponent<ActionStateComponent>(bot);
        if (world.HasComponent<CombatActionComponent>(bot))
            world.RemoveComponent<CombatActionComponent>(bot);
        world.GetComponent<ChampionComponent>(bot).mana = 0.f;
        world.GetComponent<SkillStateComponent>(bot)
            .slots[kCadenceProbeSlot].cooldownRemaining = 0.f;
        aiState.fSkillCastCooldownTimer = 0.f;
        aiState.comboTarget = castTarget;
        aiState.comboStep = 4u;
        constexpr u32_t kRejectedCastSequence = 106u;
        const u8_t rejectedCastIndex = appendSubmitted(kRejectedCastSequence);
        seedDraft(kRejectedCastSequence);
        aiState.comboStep = 5u;
        GameCommand rejectedCast = acceptedCast;
        rejectedCast.sequenceNum = kRejectedCastSequence;
        const CommandExecutionResult rejectedCastResult =
            executor->ExecuteCommand(world, tc, rejectedCast);
        const ChampionAIDecisionTraceEntry& rejectedCastLegacy =
            aiState.debugDecisionTrace[rejectedCastIndex];
        if (rejectedCastResult.state != eCommandExecutionState::Rejected ||
            rejectedCastResult.reason !=
                eCommandExecutionReason::InsufficientResource ||
            aiState.fSkillCastCooldownTimer != 0.f ||
            aiState.comboStep != 4u ||
            rejectedCastLegacy.executorState !=
                static_cast<u8_t>(AiExecutorStateV1::Rejected) ||
            rejectedCastLegacy.executorReason !=
                static_cast<u16_t>(eCommandExecutionReason::InsufficientResource) ||
            rejectedCastLegacy.blockReason !=
                eChampionAIDecisionBlockReason::CommandRejected)
        {
            std::printf(
                "[SimLab][CommandOutcome] FAIL: rejected cast result=%u reason=%u timer=%.3f step=%u traceState=%u traceReason=%u block=%u\n",
                static_cast<unsigned>(rejectedCastResult.state),
                static_cast<unsigned>(rejectedCastResult.reason),
                aiState.fSkillCastCooldownTimer,
                static_cast<unsigned>(aiState.comboStep),
                static_cast<unsigned>(rejectedCastLegacy.executorState),
                static_cast<unsigned>(rejectedCastLegacy.executorReason),
                static_cast<unsigned>(rejectedCastLegacy.blockReason));
            return false;
        }

        std::printf(
            "[SimLab][CommandOutcome] PASS: exact sequence close; accepted cast commits cadence; rejected cast preserves timer/step\n");
        return true;
    }

    bool_t RunKeyframeRestoreDeterminismProbe()
    {
        constexpr u64_t kSeed = 42ull;
        constexpr u64_t kKeyframeTick = 300ull;
        constexpr u64_t kEndTick = 600ull;

        static constexpr eChampion kProbeRoster[10] =
        {
            eChampion::YASUO, eChampion::ZED, eChampion::ASHE, eChampion::ANNIE, eChampion::LEESIN,
            eChampion::RIVEN, eChampion::SYLAS, eChampion::VIEGO, eChampion::YONE, eChampion::JAX,
        };
        static constexpr u8_t kProbeSkillSlots[4] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        FlatWalkable walkable;

        const Vec3 blueAnchor = GetGameSimRosterSpawnPosition(0, 0);
        const Vec3 redAnchor = GetGameSimRosterSpawnPosition(5, 1);
        const f32_t centerX = (blueAnchor.x + redAnchor.x) * 0.5f;
        const f32_t centerZ = (blueAnchor.z + redAnchor.z) * 0.5f;

        // RunMatch와 동일한 스크립트/시스템 본문. 무중단 실행과 복원-재실행이
        // 같은 본문을 공유하므로 비교가 자기완결적이다(본문이 바뀌면 즉시 FAIL로 드러난다).
        const auto stepTick = [&](CWorld& world, DeterministicRng& rng,
            EntityIdMap& entityMap, ICommandExecutor& executor,
            const std::vector<EntityID>& champs, u32_t& seq,
            std::vector<GameCommand>& pendingCommands, u64_t tick) -> u64_t
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

            if (CBuffSystem::PruneExpiredTickBuffs(world, tc))
                CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());

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
                    executor.ExecuteCommand(world, tc, cmd);
                }
                if ((tick + i * 7) % 45 == 0)
                {
                    GameCommand cmd{};
                    cmd.kind = eCommandKind::BasicAttack;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.targetEntity = enemy;
                    executor.ExecuteCommand(world, tc, cmd);
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
                    cmd.slot = kProbeSkillSlots[rng.NextU32() % 4];
                    cmd.targetEntity = enemy;
                    cmd.groundPos = enemyPos;
                    cmd.direction = dir;
                    executor.ExecuteCommand(world, tc, cmd);
                }
                if ((tick + i * 13) % 90 == 0)
                {
                    GameCommand cmd{};
                    cmd.kind = eCommandKind::LevelSkill;
                    cmd.issuerEntity = e;
                    cmd.issuedAtTick = tick;
                    cmd.sequenceNum = ++seq;
                    cmd.slot = kProbeSkillSlots[rng.NextU32() % 4];
                    executor.ExecuteCommand(world, tc, cmd);
                }
            }

            GameplayStatus::TickStatusEffects(world, tc);
            GameplayStatus::TickForcedMotions(world, tc);
            CSpellbookFormOverrideSystem::Execute(world, tc);
            CAreaAuraSystem::Execute(world, tc);
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CBuffSystem::AdvanceDurationsAfterStat(world, tc);
            CSkillCooldownSystem::Execute(world, tc);
            CRecallSystem::Execute(world, tc);
            CGoldIncomeSystem::Execute(world, tc);
            CWaypointPatrolSystem::Execute(world, tc);
            CCombatActionSystem::Execute(world, tc);
            CMoveSystem::Execute(world, tc);
            CJungleAISystem::Execute(world, tc, pendingCommands);
            CAttackChaseSystem::Execute(world, tc, pendingCommands);
            for (const auto& cmd : pendingCommands)
                executor.ExecuteCommand(world, tc, cmd);
            pendingCommands.clear();
            AnnieGameSim::Tick(world, tc);
            AsheGameSim::Tick(world, tc);
            EzrealGameSim::Tick(world, tc);
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
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
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
            return hash;
        };

        // 무중단 실행 + 키프레임 캡처
        CWorld worldA;
        DeterministicRng rngA(kSeed);
        EntityIdMap entityMapA;
        auto executorA = CDefaultCommandExecutor::Create();
        std::vector<EntityID> champs;
        champs.reserve(10);
        for (u8_t slot = 0; slot < 10; ++slot)
        {
            const u8_t team = slot < 5 ? 0 : 1;
            champs.push_back(SpawnChampion(worldA, entityMapA, kProbeRoster[slot], team, slot));
        }

        std::vector<GameCommand> pendingA;
        u32_t seqA = 0;
        std::vector<u64_t> baselineHashes;
        baselineHashes.reserve(static_cast<size_t>(kEndTick));

        std::vector<u8_t> keyframeBytes;
        u32_t seqAtKeyframe = 0;

        for (u64_t tick = 1; tick <= kEndTick; ++tick)
        {
            baselineHashes.push_back(stepTick(
                worldA, rngA, entityMapA, *executorA, champs, seqA, pendingA, tick));
            if (tick == kKeyframeTick)
            {
                if (!SimCheckpoint::SaveWorldKeyframe(
                    worldA, rngA, entityMapA, tick, keyframeBytes))
                {
                    std::printf("[SimLab][Keyframe] FAIL: keyframe save failed at tick %llu\n",
                        static_cast<unsigned long long>(tick));
                    return false;
                }
                seqAtKeyframe = seqA;
            }
        }

        // 직렬화 자기일관성: restore -> save 재직렬화 바이트 동일
        {
            CWorld worldT;
            DeterministicRng rngT(kSeed);
            EntityIdMap entityMapT;
            u64_t restoredTickT = 0;
            if (!SimCheckpoint::RestoreWorldKeyframe(
                worldT, rngT, entityMapT, restoredTickT, keyframeBytes))
            {
                std::printf("[SimLab][Keyframe] FAIL: restore into fresh world failed\n");
                return false;
            }
            std::vector<u8_t> resavedBytes;
            if (!SimCheckpoint::SaveWorldKeyframe(
                worldT, rngT, entityMapT, restoredTickT, resavedBytes) ||
                resavedBytes != keyframeBytes)
            {
                std::printf("[SimLab][Keyframe] FAIL: save->restore->save bytes differ\n");
                return false;
            }
        }

        // 복원 후 재실행 — 무중단 해시와 원소 단위 비교
        CWorld worldB;
        DeterministicRng rngB(0ull);
        EntityIdMap entityMapB;
        u64_t restoredTick = 0;
        if (!SimCheckpoint::RestoreWorldKeyframe(
            worldB, rngB, entityMapB, restoredTick, keyframeBytes) ||
            restoredTick != kKeyframeTick)
        {
            std::printf("[SimLab][Keyframe] FAIL: restore for replay failed (tick=%llu)\n",
                static_cast<unsigned long long>(restoredTick));
            return false;
        }

        auto executorB = CDefaultCommandExecutor::Create();
        std::vector<GameCommand> pendingB;
        u32_t seqB = seqAtKeyframe;
        for (u64_t tick = kKeyframeTick + 1; tick <= kEndTick; ++tick)
        {
            const u64_t hash = stepTick(
                worldB, rngB, entityMapB, *executorB, champs, seqB, pendingB, tick);
            const u64_t expected = baselineHashes[static_cast<size_t>(tick - 1)];
            if (hash != expected)
            {
                std::printf(
                    "[SimLab][Keyframe] FAIL: replay diverged at tick %llu (replay=%016llX baseline=%016llX)\n",
                    static_cast<unsigned long long>(tick),
                    static_cast<unsigned long long>(hash),
                    static_cast<unsigned long long>(expected));
                return false;
            }
        }

        std::printf(
            "[SimLab][Keyframe] PASS: save@%llu -> restore -> replay %llu..%llu matches uninterrupted run (blob=%zu bytes)\n",
            static_cast<unsigned long long>(kKeyframeTick),
            static_cast<unsigned long long>(kKeyframeTick + 1),
            static_cast<unsigned long long>(kEndTick),
            keyframeBytes.size());
        return true;
    }

    struct LiveAiResearchTransition
    {
        AiDecisionTraceV1 trace{};
        u64_t nextStateHash = 0u;
        f32_t reward = 0.f;
        bool_t bTerminal = false;
        bool_t bTruncated = false;
    };

    bool_t IsNonPlaceholderLowerSha256(const char* value)
    {
        if (!value || std::strlen(value) != 64u)
            return false;

        bool_t bHasNonZero = false;
        for (size_t i = 0u; i < 64u; ++i)
        {
            const char c = value[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return false;
            bHasNonZero = bHasNonZero || c != '0';
        }
        return bHasNonZero;
    }

    bool_t TryParseU64(const char* value, u64_t& outValue)
    {
        if (!value || *value == '\0' || *value == '-')
            return false;

        errno = 0;
        char* pEnd = nullptr;
        const unsigned long long parsed = std::strtoull(value, &pEnd, 10);
        if (errno == ERANGE || pEnd == value || !pEnd || *pEnd != '\0')
            return false;

        outValue = static_cast<u64_t>(parsed);
        return true;
    }

    u32_t CandidateBitForKind(u8_t candidateKind)
    {
        switch (static_cast<AiCandidateKindV1>(candidateKind))
        {
        case AiCandidateKindV1::Retreat:
            return kAiCandidateRetreatBitV1;
        case AiCandidateKindV1::Fight:
            return kAiCandidateFightBitV1;
        case AiCandidateKindV1::Farm:
            return kAiCandidateFarmBitV1;
        case AiCandidateKindV1::Siege:
            return kAiCandidateSiegeBitV1;
        default:
            return 0u;
        }
    }

    bool_t IsObservedByTrace(
        const AiObservationV1& observation,
        u32_t netEntityId)
    {
        return netEntityId == 0u ||
            netEntityId == observation.selfNetEntityId ||
            netEntityId == observation.enemyChampionNetEntityId ||
            netEntityId == observation.enemyMinionNetEntityId ||
            netEntityId == observation.enemyStructureNetEntityId ||
            netEntityId == observation.alliedWaveNetEntityId;
    }

    bool_t IsPromotableLiveTrace(const AiDecisionTraceV1& trace)
    {
        if (trace.schemaVersion != kAiDecisionTraceSchemaVersionV1 ||
            trace.byteSize != sizeof(AiDecisionTraceV1) ||
            trace.candidateCount == 0u ||
            trace.candidateCount > kAiDecisionCandidateCapacityV1 ||
            trace.observation.factTick != trace.tick ||
            trace.observation.selfNetEntityId == 0u ||
            trace.observation.provenanceFlags !=
                kAiObservationTeamFilteredFlagV1 ||
            trace.commandSequence == 0u)
        {
            return false;
        }

        const u8_t accepted = static_cast<u8_t>(AiExecutorStateV1::Accepted);
        const u8_t rejected = static_cast<u8_t>(AiExecutorStateV1::Rejected);
        if (trace.executorState != accepted && trace.executorState != rejected)
            return false;

        const u32_t selectedBit =
            CandidateBitForKind(trace.selectedCandidateKind);
        if (selectedBit == 0u ||
            (trace.actionMask.legalCandidateMask & selectedBit) == 0u ||
            (trace.actionMask.illegalCandidateMask & selectedBit) != 0u)
        {
            return false;
        }

        u8_t selectedCount = 0u;
        for (u8_t i = 0u; i < trace.candidateCount; ++i)
        {
            const AiCandidateEvidenceV1& candidate = trace.candidates[i];
            if (!IsObservedByTrace(
                trace.observation,
                candidate.targetNetEntityId))
            {
                return false;
            }
            if ((candidate.flags & kAiCandidateSelectedFlagV1) == 0u)
                continue;

            ++selectedCount;
            if (candidate.candidateKind != trace.selectedCandidateKind ||
                (candidate.flags & kAiCandidateLegalFlagV1) == 0u)
            {
                return false;
            }
        }

        return selectedCount == 1u &&
            IsObservedByTrace(
                trace.observation,
                trace.commandTargetNetEntityId);
    }

    bool_t TryFindPromotableTraceAtTick(
        CWorld& world,
        EntityID bot,
        u64_t tick,
        AiDecisionTraceV1& outTrace)
    {
        if (bot == NULL_ENTITY ||
            !world.IsAlive(bot) ||
            !world.HasComponent<ChampionAIResearchDebugComponent>(bot))
        {
            return false;
        }

        const ChampionAIResearchDebugComponent& research =
            world.GetComponent<ChampionAIResearchDebugComponent>(bot);
        const u8_t traceCount = (std::min)(
            research.decisionTraceCount,
            kChampionAIDebugTraceCapacity);
        const AiDecisionTraceV1* found = nullptr;
        for (u8_t offset = 0u; offset < traceCount; ++offset)
        {
            const u8_t index = static_cast<u8_t>(
                (research.decisionTraceHead +
                    kChampionAIDebugTraceCapacity - traceCount + offset) %
                kChampionAIDebugTraceCapacity);
            const AiDecisionTraceV1& trace = research.decisionTrace[index];
            if (trace.tick != tick || !IsPromotableLiveTrace(trace))
                continue;
            if (!found || trace.commandSequence > found->commandSequence)
                found = &trace;
        }

        if (!found)
            return false;
        outTrace = *found;
        return true;
    }

    bool_t TryMapCandidateToDebugAction(
        u8_t candidateKind,
        eChampionAIAction& outAction)
    {
        switch (static_cast<AiCandidateKindV1>(candidateKind))
        {
        case AiCandidateKindV1::Retreat:
            outAction = eChampionAIAction::Retreat;
            return true;
        case AiCandidateKindV1::Fight:
            outAction = eChampionAIAction::AttackChampion;
            return true;
        case AiCandidateKindV1::Farm:
            outAction = eChampionAIAction::AttackMinion;
            return true;
        case AiCandidateKindV1::Siege:
            outAction = eChampionAIAction::AttackStructure;
            return true;
        default:
            return false;
        }
    }

    bool_t AssertVisibleInfluenceMap(
        const AiInfluenceMapV1& map,
        NetEntityId enemyNetId)
    {
        const u8_t threatNow = static_cast<u8_t>(
            AiInfluenceLayerV1::ThreatNow);
        const u8_t threatBelief = static_cast<u8_t>(
            AiInfluenceLayerV1::ThreatBelief);
        const u8_t escapeCost = static_cast<u8_t>(
            AiInfluenceLayerV1::EscapeCost);
        bool_t bFoundThreatNow = false;
        for (u16_t i = 0u; i < kAiInfluenceCellCapacityV1; ++i)
        {
            const AiInfluenceCellV1& cell = map.cells[i];
            if (cell.values[threatNow] > 0.f)
            {
                bFoundThreatNow = true;
                if (cell.dominantSourceNetEntityIds[threatNow] != enemyNetId)
                    return false;
            }
            if (cell.values[threatBelief] != 0.f ||
                cell.values[escapeCost] != 0.f)
            {
                return false;
            }
        }
        return bFoundThreatNow;
    }

    bool_t AssertHiddenInfluenceMap(
        const AiInfluenceMapV1& map,
        NetEntityId enemyNetId)
    {
        const u8_t threatNow = static_cast<u8_t>(
            AiInfluenceLayerV1::ThreatNow);
        const u8_t threatBelief = static_cast<u8_t>(
            AiInfluenceLayerV1::ThreatBelief);
        const u8_t escapeCost = static_cast<u8_t>(
            AiInfluenceLayerV1::EscapeCost);
        bool_t bFoundThreatBelief = false;
        for (u16_t i = 0u; i < kAiInfluenceCellCapacityV1; ++i)
        {
            const AiInfluenceCellV1& cell = map.cells[i];
            if (cell.values[threatNow] != 0.f ||
                cell.values[escapeCost] != 0.f)
            {
                return false;
            }
            if (cell.values[threatBelief] > 0.f)
            {
                bFoundThreatBelief = true;
                if (cell.dominantSourceNetEntityIds[threatBelief] != enemyNetId)
                    return false;
            }
        }
        return bFoundThreatBelief;
    }

    f32_t ResolveHpRatio(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<HealthComponent>(entity))
            return 0.f;

        const HealthComponent& health =
            world.GetComponent<HealthComponent>(entity);
        return health.fMaximum > 0.f
            ? std::clamp(health.fCurrent / health.fMaximum, 0.f, 1.f)
            : 0.f;
    }

    f32_t ResolveObservableHpAdvantage(
        CWorld& world,
        EntityID self,
        EntityID enemy)
    {
        return ResolveHpRatio(world, self) - ResolveHpRatio(world, enemy);
    }

    u64_t ComputeAuthoritativeResearchStateHash(
        CWorld& world,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        u64_t tick,
        const std::vector<EntityID>& sourceEntities)
    {
        const auto hashEntity = [&](u64_t& hash, EntityID entity)
        {
            const NetEntityId netId = entityMap.ToNet(entity);
            HashU64(hash, netId);
            HashU64(hash, world.IsAlive(entity) ? 1u : 0u);
            if (!world.IsAlive(entity))
                return;

            const auto hashEntityRef = [&](EntityID value)
            {
                HashU64(hash, entityMap.ToNet(value));
            };

            HashU64(hash, world.HasComponent<TransformComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<TransformComponent>(entity))
            {
                const TransformComponent& transform =
                    world.GetComponent<TransformComponent>(entity);
                const Vec3 position = transform.GetPosition();
                const Vec3 rotation = transform.GetRotation();
                const Vec3 scale = transform.GetScale();
                HashF32(hash, position.x);
                HashF32(hash, position.y);
                HashF32(hash, position.z);
                HashF32(hash, rotation.x);
                HashF32(hash, rotation.y);
                HashF32(hash, rotation.z);
                HashF32(hash, scale.x);
                HashF32(hash, scale.y);
                HashF32(hash, scale.z);
            }

            HashU64(hash, world.HasComponent<HealthComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<HealthComponent>(entity))
            {
                const HealthComponent& health =
                    world.GetComponent<HealthComponent>(entity);
                HashF32(hash, health.fCurrent);
                HashF32(hash, health.fMaximum);
                HashU64(hash, health.bIsDead ? 1u : 0u);
            }

            HashU64(hash, world.HasComponent<ChampionComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<ChampionComponent>(entity))
            {
                const ChampionComponent& champion =
                    world.GetComponent<ChampionComponent>(entity);
                HashU64(hash, static_cast<u8_t>(champion.id));
                HashU64(hash, static_cast<u8_t>(champion.team));
                HashF32(hash, champion.hp);
                HashF32(hash, champion.maxHp);
                HashF32(hash, champion.mana);
                HashF32(hash, champion.maxMana);
                HashF32(hash, champion.shield);
                HashF32(hash, champion.moveSpeed);
                HashU64(hash, champion.level);
                for (const f32_t cooldown : champion.cooldowns)
                    HashF32(hash, cooldown);
            }

            HashU64(hash, world.HasComponent<ShieldComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<ShieldComponent>(entity))
            {
                const ShieldComponent& shield =
                    world.GetComponent<ShieldComponent>(entity);
                HashF32(hash, shield.fCurrent);
                HashF32(hash, shield.fMaximum);
                HashU64(hash, shield.uExpireTick);
            }

            HashU64(hash, world.HasComponent<SkillStateComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<SkillStateComponent>(entity))
            {
                const SkillStateComponent& skills =
                    world.GetComponent<SkillStateComponent>(entity);
                for (const SkillSlotRuntime& slot : skills.slots)
                {
                    HashF32(hash, slot.cooldownRemaining);
                    HashF32(hash, slot.cooldownDuration);
                    HashU64(hash, slot.currentStage);
                    HashF32(hash, slot.stageWindow);
                }
            }

            HashU64(hash, world.HasComponent<ActionStateComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<ActionStateComponent>(entity))
            {
                const ActionStateComponent& action =
                    world.GetComponent<ActionStateComponent>(entity);
                HashU64(hash, action.actionId);
                HashU64(hash, action.startTick);
                HashU64(hash, action.lockEndTick);
                HashU64(hash, action.sequence);
                HashU64(hash, action.commandSequence);
                HashU64(hash, static_cast<u8_t>(action.sourceChampion));
                HashU64(hash, action.sourceSlot);
                HashU64(hash, action.stage);
                HashU64(hash, static_cast<u8_t>(action.movePolicy));
                HashU64(hash, action.bHasQueuedMove ? 1u : 0u);
                HashU64(hash, action.queuedMoveSequence);
                HashF32(hash, action.queuedMoveTarget.x);
                HashF32(hash, action.queuedMoveTarget.y);
                HashF32(hash, action.queuedMoveTarget.z);
                HashF32(hash, action.queuedMoveDirection.x);
                HashF32(hash, action.queuedMoveDirection.y);
                HashF32(hash, action.queuedMoveDirection.z);
            }

            HashU64(hash, world.HasComponent<CombatActionComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<CombatActionComponent>(entity))
            {
                const CombatActionComponent& action =
                    world.GetComponent<CombatActionComponent>(entity);
                HashU64(hash, static_cast<u8_t>(action.eKind));
                HashU64(hash, static_cast<u8_t>(action.eMovePolicy));
                HashU64(hash, static_cast<u8_t>(action.eSourceChampion));
                HashU64(hash, action.uSlot);
                HashU64(hash, action.uStage);
                HashU64(hash, action.uFlags);
                hashEntityRef(action.entityTarget);
                HashU64(hash, action.uSequenceNum);
                HashU64(hash, action.uOwnerActionSequence);
                HashU64(hash, action.uStartTick);
                HashU64(hash, action.uImpactTick);
                HashU64(hash, action.uEndTick);
                HashU64(hash, action.bImpactIssued ? 1u : 0u);
                HashU64(hash, action.bQueuedMove ? 1u : 0u);
                HashF32(hash, action.vQueuedMoveTarget.x);
                HashF32(hash, action.vQueuedMoveTarget.y);
                HashF32(hash, action.vQueuedMoveTarget.z);
                HashF32(hash, action.vQueuedMoveDirection.x);
                HashF32(hash, action.vQueuedMoveDirection.y);
                HashF32(hash, action.vQueuedMoveDirection.z);
            }

            HashU64(hash, world.HasComponent<MoveTargetComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<MoveTargetComponent>(entity))
            {
                const MoveTargetComponent& move =
                    world.GetComponent<MoveTargetComponent>(entity);
                HashF32(hash, move.target.x);
                HashF32(hash, move.target.y);
                HashF32(hash, move.target.z);
                HashF32(hash, move.arriveRadius);
                HashU64(hash, move.pathCount);
                HashU64(hash, move.pathIndex);
                HashU64(hash, move.bHasTarget ? 1u : 0u);
                for (u16_t i = 0u; i < move.pathCount; ++i)
                {
                    HashF32(hash, move.pathWaypoints[i].x);
                    HashF32(hash, move.pathWaypoints[i].y);
                    HashF32(hash, move.pathWaypoints[i].z);
                }
            }

            HashU64(hash, world.HasComponent<AttackChaseComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<AttackChaseComponent>(entity))
            {
                const AttackChaseComponent& chase =
                    world.GetComponent<AttackChaseComponent>(entity);
                hashEntityRef(chase.target);
                HashU64(hash, chase.sequenceNum);
                HashU64(hash, chase.commandKind);
                HashU64(hash, chase.slot);
                HashU64(hash, chase.itemId);
                HashF32(hash, chase.effectiveRange);
                HashF32(hash, chase.repathTimer);
                HashU64(hash, chase.bActive ? 1u : 0u);
            }

            HashU64(hash, world.HasComponent<ChampionAIComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<ChampionAIComponent>(entity))
            {
                const ChampionAIComponent& ai =
                    world.GetComponent<ChampionAIComponent>(entity);
                HashU64(hash, static_cast<u8_t>(ai.state));
                HashU64(hash, static_cast<u8_t>(ai.intent));
                HashU64(hash, static_cast<u8_t>(ai.lastAction));
                HashU64(hash, static_cast<u8_t>(ai.divePhase));
                hashEntityRef(ai.lockedChampion);
                hashEntityRef(ai.comboTarget);
                HashU64(hash, ai.comboStep);
                HashF32(hash, ai.decisionTimer);
                HashF32(hash, ai.intentHoldTimer);
                HashU64(hash, ai.nextCommandSequence);
                HashU64(hash, ai.debugAvailableActionMask);
                HashU64(hash, ai.debugAvailableSkillMask);
            }

            HashU64(hash, world.HasComponent<MinionComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<MinionComponent>(entity))
            {
                const MinionComponent& minion =
                    world.GetComponent<MinionComponent>(entity);
                HashU64(hash, static_cast<u8_t>(minion.team));
                HashU64(hash, minion.laneType);
                HashU64(hash, minion.roleType);
                HashF32(hash, minion.hp);
                HashF32(hash, minion.maxHp);
            }

            HashU64(hash, world.HasComponent<StructureComponent>(entity) ? 1u : 0u);
            if (world.HasComponent<StructureComponent>(entity))
            {
                const StructureComponent& structure =
                    world.GetComponent<StructureComponent>(entity);
                HashU64(hash, static_cast<u8_t>(structure.team));
                HashU64(hash, structure.kind);
                HashU64(hash, structure.tier);
                HashU64(hash, structure.lane);
                HashF32(hash, structure.hp);
                HashF32(hash, structure.maxHp);
            }
        };

        u64_t hash = 1469598103934665603ull;
        HashU64(hash, tick);
        HashU64(hash, rng.GetState());
        HashU64(hash, entityMap.GetNextNetId());

        std::vector<EntityID> entities = sourceEntities;
        std::sort(
            entities.begin(),
            entities.end(),
            [&](EntityID lhs, EntityID rhs)
            {
                return entityMap.ToNet(lhs) < entityMap.ToNet(rhs);
            });
        for (const EntityID entity : entities)
            hashEntity(hash, entity);
        return hash;
    }

    u64_t ComputeAuthoritativeResearchStateHash(
        CWorld& world,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        u64_t tick,
        EntityID self,
        EntityID enemy)
    {
        return ComputeAuthoritativeResearchStateHash(
            world,
            rng,
            entityMap,
            tick,
            std::vector<EntityID>{ self, enemy });
    }

    bool_t WriteLiveAiResearchMetadata(
        const std::filesystem::path& metadataPath,
        const char* rulesHash,
        const char* definitionHash,
        u64_t seed,
        const char* episodeId,
        const char* scenarioId,
        u64_t policyRevision,
        const std::vector<LiveAiResearchTransition>& transitions)
    {
        const std::string path = metadataPath.string();
        FILE* stream = nullptr;
        if (fopen_s(&stream, path.c_str(), "wb") != 0 || !stream)
            return false;

        bool_t bWriteOk = true;
        const auto write = [&](const char* format, auto... args)
        {
            if (bWriteOk && std::fprintf(stream, format, args...) < 0)
                bWriteOk = false;
        };

        write("{\n");
        write("  \"schema_version\": 1,\n");
        write("  \"episode_id\": \"%s\",\n", episodeId);
        write("  \"scenario_id\": \"%s\",\n", scenarioId);
        // Epoch zero is reserved for legacy/unknown snapshot compatibility.
        // A newly-authored deterministic episode starts on the first timeline.
        write("  \"timeline_epoch\": 1,\n");
        write("  \"branch_id\": 0,\n");
        write("  \"seed\": %llu,\n", static_cast<unsigned long long>(seed));
        write("  \"rules_hash\": \"%s\",\n", rulesHash);
        write("  \"definition_hash\": \"%s\",\n", definitionHash);
        write(
            "  \"policy_revision\": %llu,\n",
            static_cast<unsigned long long>(policyRevision));
        write("  \"transitions\": [\n");
        for (size_t i = 0u; i < transitions.size(); ++i)
        {
            const LiveAiResearchTransition& transition = transitions[i];
            write("    {\n");
            write("      \"trace_index\": %zu,\n", i);
            write(
                "      \"next_state_hash\": \"%016llx\",\n",
                static_cast<unsigned long long>(transition.nextStateHash));
            write("      \"reward\": %.9g,\n", static_cast<double>(transition.reward));
            write(
                "      \"terminal\": %s,\n",
                transition.bTerminal ? "true" : "false");
            write(
                "      \"truncated\": %s\n",
                transition.bTruncated ? "true" : "false");
            write("    }%s\n", i + 1u == transitions.size() ? "" : ",");
        }
        write("  ]\n");
        write("}\n");

        const int closeResult = std::fclose(stream);
        return bWriteOk && closeResult == 0;
    }

    bool_t ExportLiveAiResearchSmoke(
        const std::filesystem::path& outputDirectory,
        const char* rulesHash,
        const char* definitionHash,
        u64_t seed)
    {
        constexpr u64_t kTickCount = 240u;
        constexpr u64_t kHiddenAssertionTick = 60u;
        const Vec3 visibleEnemyPosition{ 4.f, 0.f, 0.f };
        const Vec3 hiddenEnemyPosition{ 50.f, 0.f, 50.f };

        CWorld world;
        DeterministicRng rng(seed);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();
        const EntityID bot = SpawnChampion(
            world,
            entityMap,
            eChampion::ASHE,
            0u,
            0u);
        const EntityID enemy = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            1u,
            5u);

        world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(enemy).SetPosition(
            visibleEnemyPosition);

        ChampionAIComponent ai{};
        ai.champion = eChampion::ASHE;
        ai.team = eTeam::Blue;
        ai.brainType = eChampionAIBrainType::RuleBased;
        ai.state = eChampionAIState::LaneCombat;
        ai.lastAction = eChampionAIAction::AttackChampion;
        ai.intent = eChampionAIIntent::AttackChampion;
        ai.intentHoldTimer = ai.intentHoldDuration;
        ai.decisionTimer = 0.f;
        ai.laneGoal = visibleEnemyPosition;
        ai.safeAnchor = Vec3{ -4.f, 0.f, 0.f };
        ai.retreatGoal = ai.safeAnchor;
        ai.midDefenseAnchor = Vec3{};
        world.AddComponent<ChampionAIComponent>(bot, ai);

        const NetEntityId enemyNetId = entityMap.ToNet(enemy);
        if (enemyNetId == NULL_NET_ENTITY)
        {
            std::printf("[SimLab][AILiveSmoke] FAIL: enemy NetEntityId missing\n");
            return false;
        }

        std::vector<LiveAiResearchTransition> transitions;
        std::vector<GameCommand> aiCommands;
        std::vector<GameCommand> pendingCommands;
        bool_t bVisibleInfluenceAsserted = false;
        bool_t bHiddenInfluenceAsserted = false;

        for (u64_t tick = 1u; tick <= kTickCount; ++tick)
        {
            if (tick == kHiddenAssertionTick)
            {
                world.GetComponent<TransformComponent>(enemy).SetPosition(
                    hiddenEnemyPosition);
                world.GetComponent<ChampionAIComponent>(bot).decisionTimer = 0.f;
            }
            else if (tick == kHiddenAssertionTick + 1u)
            {
                world.GetComponent<TransformComponent>(enemy).SetPosition(
                    visibleEnemyPosition);
            }
            if (tick == kTickCount)
            {
                world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
                world.GetComponent<TransformComponent>(enemy).SetPosition(
                    visibleEnemyPosition);
                if (world.HasComponent<CombatActionComponent>(bot))
                    world.RemoveComponent<CombatActionComponent>(bot);
                if (world.HasComponent<AttackChaseComponent>(bot))
                    world.RemoveComponent<AttackChaseComponent>(bot);
                if (world.HasComponent<MoveTargetComponent>(bot))
                    world.RemoveComponent<MoveTargetComponent>(bot);
                if (world.HasComponent<RecallComponent>(bot))
                    world.RemoveComponent<RecallComponent>(bot);
                if (world.HasComponent<ActionStateComponent>(bot))
                    world.GetComponent<ActionStateComponent>(bot) =
                        ActionStateComponent{};

                ChampionAIComponent& finalAI =
                    world.GetComponent<ChampionAIComponent>(bot);
                finalAI.state = eChampionAIState::LaneCombat;
                finalAI.intent = eChampionAIIntent::AttackChampion;
                finalAI.lockedChampion = enemy;
                finalAI.comboTarget = NULL_ENTITY;
                finalAI.diveTarget = NULL_ENTITY;
                finalAI.divePhase = eChampionAIDivePhase::None;
                finalAI.bMidDefenseActive = false;
                finalAI.intentHoldTimer = 0.f;
                finalAI.decisionTimer = 0.f;
                // Skip an unlearned combo step that may legitimately consume
                // a decision without submitting a command. The boundary
                // contract needs one accepted, team-filtered decision at the
                // exact truncation tick, so force the normal post-combo
                // attack-or-chase path for this final smoke tick only.
                finalAI.fPostComboBATimer = 1.f;
                finalAI.bPostComboBAAllowed = true;
                // The decision pass re-derives bPostComboBAAllowed from live
                // HP ratios (ShouldContinueBasicAttackAfterCombo). Pin the
                // enemy below the bot's HP ratio so the forced fight window
                // survives that re-derivation regardless of balance tuning —
                // otherwise this contract flips on any damage-model change.
                if (world.HasComponent<HealthComponent>(enemy))
                {
                    auto& enemyHealth =
                        world.GetComponent<HealthComponent>(enemy);
                    enemyHealth.fCurrent = (std::min)(
                        enemyHealth.fCurrent, enemyHealth.fMaximum * 0.5f);
                    if (world.HasComponent<ChampionComponent>(enemy))
                    {
                        world.GetComponent<ChampionComponent>(enemy).hp =
                            enemyHealth.fCurrent;
                    }
                }
            }

            TickContext tc{};
            tc.tickIndex = tick;
            tc.fDt = DeterministicTime::kFixedDt;
            tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
            tc.pRng = &rng;
            tc.pEntityMap = &entityMap;
            tc.localPlayer = NULL_ENTITY;
            tc.pWalkable = &walkable;
            tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

            const f32_t hpAdvantageBefore =
                ResolveObservableHpAdvantage(world, bot, enemy);

            GameplayStatus::TickStatusEffects(world, tc);
            GameplayStatus::TickForcedMotions(world, tc);
            if (CBuffSystem::PruneExpiredTickBuffs(world, tc))
                CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            aiCommands.clear();
            CChampionAISystem::Execute(world, tc, aiCommands);

            const ChampionAIResearchDebugComponent& currentResearch =
                world.GetComponent<ChampionAIResearchDebugComponent>(bot);
            if (tick == 1u)
            {
                bVisibleInfluenceAsserted = AssertVisibleInfluenceMap(
                    currentResearch.influenceMap,
                    enemyNetId);
                if (!bVisibleInfluenceAsserted)
                {
                    std::printf(
                        "[SimLab][AILiveSmoke] FAIL: visible ThreatNow/FlatWalkable contract\n");
                    return false;
                }
            }
            if (tick == kHiddenAssertionTick)
            {
                bHiddenInfluenceAsserted = AssertHiddenInfluenceMap(
                    currentResearch.influenceMap,
                    enemyNetId);
                if (!bHiddenInfluenceAsserted)
                {
                    std::printf(
                        "[SimLab][AILiveSmoke] FAIL: hidden ThreatBelief/FlatWalkable contract\n");
                    return false;
                }
            }

            for (const GameCommand& command : aiCommands)
                executor->ExecuteCommand(world, tc, command);

            CSpellbookFormOverrideSystem::Execute(world, tc);
            CAreaAuraSystem::Execute(world, tc);
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CBuffSystem::AdvanceDurationsAfterStat(world, tc);
            CSkillCooldownSystem::Execute(world, tc);
            CRecallSystem::Execute(world, tc);
            CGoldIncomeSystem::Execute(world, tc);
            CWaypointPatrolSystem::Execute(world, tc);
            CCombatActionSystem::Execute(world, tc);
            CMoveSystem::Execute(world, tc);
            CJungleAISystem::Execute(world, tc, pendingCommands);
            CAttackChaseSystem::Execute(world, tc, pendingCommands);
            for (const GameCommand& command : pendingCommands)
                executor->ExecuteCommand(world, tc, command);
            pendingCommands.clear();
            AnnieGameSim::Tick(world, tc);
            AsheGameSim::Tick(world, tc);
            EzrealGameSim::Tick(world, tc);
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
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CDeathSystem::Execute(world, tc);

            if (!world.IsAlive(bot) ||
                !world.IsAlive(enemy) ||
                ResolveHpRatio(world, bot) <= 0.f ||
                ResolveHpRatio(world, enemy) <= 0.f)
            {
                std::printf(
                    "[SimLab][AILiveSmoke] FAIL: contract smoke reached an unexpected terminal state\n");
                return false;
            }

            const f32_t reward =
                ResolveObservableHpAdvantage(world, bot, enemy) -
                hpAdvantageBefore;

            const ChampionAIResearchDebugComponent& finalizedResearch =
                world.GetComponent<ChampionAIResearchDebugComponent>(bot);
            std::vector<AiDecisionTraceV1> tickTraces;
            const u8_t traceCount = (std::min)(
                finalizedResearch.decisionTraceCount,
                kChampionAIDebugTraceCapacity);
            for (u8_t offset = 0u; offset < traceCount; ++offset)
            {
                const u8_t index = static_cast<u8_t>(
                    (finalizedResearch.decisionTraceHead +
                        kChampionAIDebugTraceCapacity - traceCount + offset) %
                    kChampionAIDebugTraceCapacity);
                const AiDecisionTraceV1& trace =
                    finalizedResearch.decisionTrace[index];
                if (trace.tick == tick && IsPromotableLiveTrace(trace))
                    tickTraces.push_back(trace);
            }

            if (!tickTraces.empty())
            {
                const u64_t stateHash = ComputeAuthoritativeResearchStateHash(
                    world,
                    rng,
                    entityMap,
                    tick,
                    bot,
                    enemy);

                std::sort(
                    tickTraces.begin(),
                    tickTraces.end(),
                    [](const AiDecisionTraceV1& lhs, const AiDecisionTraceV1& rhs)
                    {
                        if (lhs.observation.selfNetEntityId !=
                            rhs.observation.selfNetEntityId)
                        {
                            return lhs.observation.selfNetEntityId <
                                rhs.observation.selfNetEntityId;
                        }
                        return lhs.commandSequence < rhs.commandSequence;
                    });
                for (const AiDecisionTraceV1& trace : tickTraces)
                {
                    transitions.push_back(
                        LiveAiResearchTransition{ trace, stateHash, reward });
                }
            }
        }

        if (!bVisibleInfluenceAsserted ||
            !bHiddenInfluenceAsserted ||
            transitions.empty() ||
            transitions.back().trace.tick != kTickCount)
        {
            const u64_t lastTraceTick = transitions.empty()
                ? 0u
                : transitions.back().trace.tick;
            const ChampionAIComponent& failedAI =
                world.GetComponent<ChampionAIComponent>(bot);
            const ChampionAIResearchDebugComponent& failedResearch =
                world.GetComponent<ChampionAIResearchDebugComponent>(bot);
            std::printf(
                "[SimLab][AILiveSmoke] FAIL: visible=%u hidden=%u records=%zu last=%llu expected=%llu state=%u intent=%u action=%u research=%u\n",
                bVisibleInfluenceAsserted ? 1u : 0u,
                bHiddenInfluenceAsserted ? 1u : 0u,
                transitions.size(),
                static_cast<unsigned long long>(lastTraceTick),
                static_cast<unsigned long long>(kTickCount),
                static_cast<u32_t>(failedAI.state),
                static_cast<u32_t>(failedAI.intent),
                static_cast<u32_t>(failedAI.lastAction),
                static_cast<u32_t>(failedResearch.decisionTraceCount));
            for (u8_t offset = 0u;
                offset < failedResearch.decisionTraceCount;
                ++offset)
            {
                const u8_t index = static_cast<u8_t>(
                    (failedResearch.decisionTraceHead +
                        kChampionAIDebugTraceCapacity -
                        failedResearch.decisionTraceCount + offset) %
                    kChampionAIDebugTraceCapacity);
                const AiDecisionTraceV1& trace =
                    failedResearch.decisionTrace[index];
                if (trace.tick != kTickCount)
                    continue;
                std::printf(
                    "[SimLab][AILiveSmoke] final trace selected=%u legal=%08X illegal=%08X command=%u seq=%u exec=%u reason=%u enemy=%u\n",
                    static_cast<u32_t>(trace.selectedCandidateKind),
                    trace.actionMask.legalCandidateMask,
                    trace.actionMask.illegalCandidateMask,
                    static_cast<u32_t>(trace.commandKind),
                    trace.commandSequence,
                    static_cast<u32_t>(trace.executorState),
                    static_cast<u32_t>(trace.executorReason),
                    trace.observation.enemyChampionNetEntityId);
            }
            return false;
        }

        std::sort(
            transitions.begin(),
            transitions.end(),
            [](const LiveAiResearchTransition& lhs,
               const LiveAiResearchTransition& rhs)
            {
                if (lhs.trace.tick != rhs.trace.tick)
                    return lhs.trace.tick < rhs.trace.tick;
                if (lhs.trace.observation.selfNetEntityId !=
                    rhs.trace.observation.selfNetEntityId)
                {
                    return lhs.trace.observation.selfNetEntityId <
                        rhs.trace.observation.selfNetEntityId;
                }
                return lhs.trace.commandSequence < rhs.trace.commandSequence;
            });
        for (size_t i = 1u; i < transitions.size(); ++i)
        {
            const AiDecisionTraceV1& previous = transitions[i - 1u].trace;
            const AiDecisionTraceV1& current = transitions[i].trace;
            if (previous.tick == current.tick &&
                previous.observation.selfNetEntityId ==
                    current.observation.selfNetEntityId &&
                previous.commandSequence == current.commandSequence)
            {
                std::printf(
                    "[SimLab][AILiveSmoke] FAIL: duplicate trace identity\n");
                return false;
            }
        }
        transitions.back().bTruncated = true;

        std::error_code directoryError;
        std::filesystem::create_directories(
            outputDirectory,
            directoryError);
        if (directoryError)
        {
            std::printf(
                "[SimLab][AILiveSmoke] FAIL: output directory: %s\n",
                directoryError.message().c_str());
            return false;
        }

        std::vector<AiDecisionTraceV1> traces;
        traces.reserve(transitions.size());
        for (const LiveAiResearchTransition& transition : transitions)
            traces.push_back(transition.trace);

        const std::filesystem::path capturePath =
            outputDirectory / "decision_trace_v1.bin";
        const std::filesystem::path metadataPath =
            outputDirectory / "episode_metadata.json";
        const std::string capturePathText = capturePath.string();
        char episodeId[96]{};
        sprintf_s(
            episodeId,
            "simlab-live-ai-research-smoke-v1-%llu",
            static_cast<unsigned long long>(seed));
        if (!Winters::AIResearchTools::WriteAiDecisionTraceCaptureV1(
            capturePathText.c_str(),
            traces.data(),
            traces.size()) ||
            !WriteLiveAiResearchMetadata(
                metadataPath,
                rulesHash,
                definitionHash,
                seed,
                episodeId,
                "simlab-1v1-observable-lane-v1",
                1u,
                transitions))
        {
            std::printf("[SimLab][AILiveSmoke] FAIL: capture/metadata write\n");
            return false;
        }

        std::printf(
            "[SimLab][AILiveSmoke] PASS: records=%zu ticks=%llu pure-team-filtered final legal; visible/hidden influence asserted\n",
            transitions.size(),
            static_cast<unsigned long long>(kTickCount));
        std::printf(
            "[SimLab][AILiveSmoke] capture=%s metadata=%s\n",
            capturePathText.c_str(),
            metadataPath.string().c_str());
        return true;
    }

    enum class MeasuredAiScenarioFamily : u8_t
    {
        Fight = 0u,
        Retreat,
        Farm,
        Siege,
    };

    bool_t TryParseMeasuredAiScenarioFamily(
        const char* value,
        MeasuredAiScenarioFamily& outFamily)
    {
        if (std::strcmp(value, "fight") == 0)
            outFamily = MeasuredAiScenarioFamily::Fight;
        else if (std::strcmp(value, "retreat") == 0)
            outFamily = MeasuredAiScenarioFamily::Retreat;
        else if (std::strcmp(value, "farm") == 0)
            outFamily = MeasuredAiScenarioFamily::Farm;
        else if (std::strcmp(value, "siege") == 0)
            outFamily = MeasuredAiScenarioFamily::Siege;
        else
            return false;
        return true;
    }

    bool_t IsSafeMeasuredScenarioId(const char* value)
    {
        if (!value)
            return false;
        const size_t length = std::strlen(value);
        if (length == 0u || length > 64u)
            return false;
        for (size_t i = 0u; i < length; ++i)
        {
            const char c = value[i];
            if (!((c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '-' || c == '_'))
            {
                return false;
            }
        }
        return true;
    }

    EntityID SpawnMeasuredAiTarget(
        CWorld& world,
        EntityIdMap& entityMap,
        GameplayStateQuery::eGameplayTargetKind kind,
        eTeam team,
        const Vec3& position)
    {
        const EntityID entity = SpawnStatusProbeTarget(
            world,
            kind,
            team,
            position);
        if (entity == NULL_ENTITY)
            return NULL_ENTITY;

        NetEntityIdComponent net{};
        net.netId = entityMap.IssueNew(entity);
        world.AddComponent<NetEntityIdComponent>(entity, net);
        return entity;
    }

    void SetMeasuredChampionHpRatio(
        CWorld& world,
        EntityID entity,
        f32_t ratio)
    {
        HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        health.fCurrent = health.fMaximum * std::clamp(ratio, 0.01f, 1.f);
        ChampionComponent& champion = world.GetComponent<ChampionComponent>(entity);
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
    }

    bool_t ExportMeasuredAiResearchEpisode(
        const std::filesystem::path& outputDirectory,
        MeasuredAiScenarioFamily family,
        const char* familyName,
        const char* scenarioId,
        eTeam botTeam,
        const char* sideName,
        const char* rulesHash,
        const char* definitionHash,
        u64_t seed)
    {
        constexpr u8_t kLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        constexpr u64_t kDecisionTick = 1u;
        const eTeam enemyTeam = botTeam == eTeam::Blue
            ? eTeam::Red
            : eTeam::Blue;
        const f32_t direction = botTeam == eTeam::Blue ? 1.f : -1.f;
        const f32_t seedUnit = static_cast<f32_t>(seed % 17u) / 16.f;
        const f32_t targetDistance = 3.25f + 1.50f * seedUnit;
        const Vec3 botPosition{};
        const Vec3 visibleTargetPosition{ direction * targetDistance, 0.f, 0.f };
        const Vec3 hiddenEnemyPosition{ direction * 50.f, 0.f, 50.f };

        CWorld world;
        DeterministicRng rng(seed);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID bot = SpawnChampion(
            world,
            entityMap,
            eChampion::ASHE,
            static_cast<u8_t>(botTeam),
            0u);
        const EntityID enemy = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(enemyTeam),
            5u);
        world.GetComponent<TransformComponent>(bot).SetPosition(botPosition);
        world.GetComponent<TransformComponent>(enemy).SetPosition(
            family == MeasuredAiScenarioFamily::Fight ||
            family == MeasuredAiScenarioFamily::Retreat
                ? visibleTargetPosition
                : hiddenEnemyPosition);

        ChampionAIComponent ai{};
        ai.champion = eChampion::ASHE;
        ai.team = botTeam;
        ai.difficulty = 1u;
        ai.lane = kLane;
        ai.activeLane = kLane;
        ai.brainType = eChampionAIBrainType::RuleBased;
        ai.state = eChampionAIState::LaneCombat;
        ai.lastAction = eChampionAIAction::FollowWave;
        ai.intent = eChampionAIIntent::FarmMinion;
        ai.decisionTimer = 0.f;
        ai.intentHoldTimer = 0.f;
        ai.laneGoal = Vec3{ direction * 12.f, 0.f, 0.f };
        ai.safeAnchor = Vec3{ -direction * 6.f, 0.f, 0.f };
        ai.retreatGoal = ai.safeAnchor;
        ai.midDefenseAnchor = botPosition;
        if (family == MeasuredAiScenarioFamily::Fight)
        {
            ai.fPostComboBATimer = 1.f;
            ai.bPostComboBAAllowed = true;
        }
        world.AddComponent<ChampionAIComponent>(bot, ai);

        const f32_t selfHp = family == MeasuredAiScenarioFamily::Retreat
            ? 0.04f + 0.05f * seedUnit
            : 0.72f + 0.24f * seedUnit;
        const f32_t enemyHp = family == MeasuredAiScenarioFamily::Fight
            ? 0.18f + 0.30f * (1.f - seedUnit)
            : 0.82f + 0.16f * seedUnit;
        SetMeasuredChampionHpRatio(world, bot, selfHp);
        SetMeasuredChampionHpRatio(world, enemy, enemyHp);
        const u8_t selfLevel = static_cast<u8_t>(6u + seed % 5u);
        const u8_t enemyLevel = static_cast<u8_t>(
            6u + (seed * 7u) % 5u);
        world.GetComponent<StatComponent>(bot).level = selfLevel;
        world.GetComponent<StatComponent>(enemy).level = enemyLevel;
        world.GetComponent<ChampionComponent>(bot).level = selfLevel;
        world.GetComponent<ChampionComponent>(enemy).level = enemyLevel;

        std::vector<EntityID> scenarioEntities{ bot, enemy };
        if (family == MeasuredAiScenarioFamily::Farm ||
            family == MeasuredAiScenarioFamily::Retreat)
        {
            const EntityID minion = SpawnMeasuredAiTarget(
                world,
                entityMap,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                enemyTeam,
                visibleTargetPosition);
            if (minion == NULL_ENTITY)
                return false;
            MinionComponent& minionComponent =
                world.GetComponent<MinionComponent>(minion);
            minionComponent.laneType = kLane;
            minionComponent.hp = 45.f + 5.f * seedUnit;
            minionComponent.maxHp = 100.f;
            HealthComponent& minionHealth =
                world.GetComponent<HealthComponent>(minion);
            minionHealth.fCurrent = minionComponent.hp;
            minionHealth.fMaximum = minionComponent.maxHp;
            scenarioEntities.push_back(minion);
        }
        else if (family == MeasuredAiScenarioFamily::Siege)
        {
            const EntityID structure = SpawnMeasuredAiTarget(
                world,
                entityMap,
                GameplayStateQuery::eGameplayTargetKind::Structure,
                enemyTeam,
                visibleTargetPosition);
            const EntityID alliedWave = SpawnMeasuredAiTarget(
                world,
                entityMap,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                botTeam,
                Vec3{ visibleTargetPosition.x - direction, 0.f, 0.f });
            if (structure == NULL_ENTITY || alliedWave == NULL_ENTITY)
                return false;

            StructureComponent& structureComponent =
                world.GetComponent<StructureComponent>(structure);
            structureComponent.kind = static_cast<u32_t>(
                Winters::Map::eObjectKind::Structure_Turret);
            structureComponent.tier = static_cast<u32_t>(
                Winters::Map::eTurretTier::Outer);
            structureComponent.lane = kLane;
            structureComponent.hp = 2200.f + 300.f * seedUnit;
            structureComponent.maxHp = 3000.f;
            HealthComponent& structureHealth =
                world.GetComponent<HealthComponent>(structure);
            structureHealth.fCurrent = structureComponent.hp;
            structureHealth.fMaximum = structureComponent.maxHp;

            MinionComponent& waveComponent =
                world.GetComponent<MinionComponent>(alliedWave);
            waveComponent.laneType = kLane;
            MinionStateComponent& waveState =
                world.GetComponent<MinionStateComponent>(alliedWave);
            waveState.team = botTeam;
            scenarioEntities.push_back(structure);
            scenarioEntities.push_back(alliedWave);
        }

        TickContext tc{};
        tc.tickIndex = kDecisionTick;
        tc.fDt = DeterministicTime::kFixedDt;
        tc.fSimulatedTimeSec = DeterministicTime::TickToSec(kDecisionTick);
        tc.pRng = &rng;
        tc.pEntityMap = &entityMap;
        tc.localPlayer = NULL_ENTITY;
        tc.pWalkable = &walkable;
        tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

        std::vector<GameCommand> commands;
        CChampionAISystem::Execute(world, tc, commands);
        for (const GameCommand& command : commands)
            executor->ExecuteCommand(world, tc, command);

        if (!world.HasComponent<ChampionAIResearchDebugComponent>(bot))
        {
            std::printf(
                "[SimLab][AIMeasured] FAIL: no research trace family=%s scenario=%s\n",
                familyName,
                scenarioId);
            return false;
        }

        const ChampionAIResearchDebugComponent& research =
            world.GetComponent<ChampionAIResearchDebugComponent>(bot);
        const AiDecisionTraceV1* acceptedTrace = nullptr;
        const u8_t traceCount = (std::min)(
            research.decisionTraceCount,
            kChampionAIDebugTraceCapacity);
        for (u8_t offset = 0u; offset < traceCount; ++offset)
        {
            const u8_t index = static_cast<u8_t>(
                (research.decisionTraceHead +
                    kChampionAIDebugTraceCapacity - traceCount + offset) %
                kChampionAIDebugTraceCapacity);
            const AiDecisionTraceV1& trace = research.decisionTrace[index];
            if (trace.tick == kDecisionTick &&
                trace.executorState == static_cast<u8_t>(AiExecutorStateV1::Accepted) &&
                IsPromotableLiveTrace(trace))
            {
                acceptedTrace = &trace;
            }
        }

        const AiCandidateKindV1 expected =
            family == MeasuredAiScenarioFamily::Fight
                ? AiCandidateKindV1::Fight
                : family == MeasuredAiScenarioFamily::Retreat
                    ? AiCandidateKindV1::Retreat
                    : family == MeasuredAiScenarioFamily::Farm
                        ? AiCandidateKindV1::Farm
                        : AiCandidateKindV1::Siege;
        if (!acceptedTrace ||
            acceptedTrace->selectedCandidateKind != static_cast<u8_t>(expected))
        {
            std::printf(
                "[SimLab][AIMeasured] FAIL: expected=%u actual=%u commands=%zu family=%s scenario=%s\n",
                static_cast<u32_t>(expected),
                acceptedTrace
                    ? static_cast<u32_t>(acceptedTrace->selectedCandidateKind)
                    : 0u,
                commands.size(),
                familyName,
                scenarioId);
            return false;
        }

        u32_t legalCount = 0u;
        for (u32_t bits = acceptedTrace->actionMask.legalCandidateMask;
             bits != 0u;
             bits >>= 1u)
        {
            legalCount += bits & 1u;
        }
        if (legalCount < 2u)
        {
            std::printf(
                "[SimLab][AIMeasured] FAIL: fewer than two legal candidates mask=%08X\n",
                acceptedTrace->actionMask.legalCandidateMask);
            return false;
        }

        const u64_t stateHash = ComputeAuthoritativeResearchStateHash(
            world,
            rng,
            entityMap,
            kDecisionTick,
            scenarioEntities);
        std::vector<LiveAiResearchTransition> transitions{
            LiveAiResearchTransition{
                *acceptedTrace,
                stateHash,
                0.f,
                false,
                true }
        };

        std::error_code directoryError;
        std::filesystem::create_directories(outputDirectory, directoryError);
        if (directoryError)
        {
            std::printf(
                "[SimLab][AIMeasured] FAIL: output directory: %s\n",
                directoryError.message().c_str());
            return false;
        }

        char episodeId[192]{};
        sprintf_s(
            episodeId,
            "simlab-bc-v1-%s-%s-%llu",
            scenarioId,
            sideName,
            static_cast<unsigned long long>(seed));
        const std::filesystem::path capturePath =
            outputDirectory / "decision_trace_v1.bin";
        const std::filesystem::path metadataPath =
            outputDirectory / "episode_metadata.json";
        const std::string capturePathText = capturePath.string();
        if (!Winters::AIResearchTools::WriteAiDecisionTraceCaptureV1(
                capturePathText.c_str(),
                &transitions[0].trace,
                1u) ||
            !WriteLiveAiResearchMetadata(
                metadataPath,
                rulesHash,
                definitionHash,
                seed,
                episodeId,
                scenarioId,
                1u,
                transitions))
        {
            std::printf("[SimLab][AIMeasured] FAIL: capture/metadata write\n");
            return false;
        }

        std::printf(
            "[SimLab][AIMeasured] PASS: family=%s scenario=%s side=%s seed=%llu selected=%u legal=%08X state=%016llX\n",
            familyName,
            scenarioId,
            sideName,
            static_cast<unsigned long long>(seed),
            static_cast<u32_t>(acceptedTrace->selectedCandidateKind),
            acceptedTrace->actionMask.legalCandidateMask,
            static_cast<unsigned long long>(stateHash));
        return true;
    }

    bool_t ReadBinaryFile(
        const std::filesystem::path& path,
        std::vector<u8_t>& outBytes)
    {
        const std::string pathText = path.string();
        FILE* stream = nullptr;
        if (fopen_s(&stream, pathText.c_str(), "rb") != 0 || !stream)
            return false;
        if (_fseeki64(stream, 0, SEEK_END) != 0)
        {
            std::fclose(stream);
            return false;
        }
        const __int64 byteCount = _ftelli64(stream);
        if (byteCount <= 0 || _fseeki64(stream, 0, SEEK_SET) != 0)
        {
            std::fclose(stream);
            return false;
        }
        outBytes.resize(static_cast<size_t>(byteCount));
        const size_t readCount = std::fread(
            outBytes.data(),
            1u,
            outBytes.size(),
            stream);
        const int closeResult = std::fclose(stream);
        return readCount == outBytes.size() && closeResult == 0;
    }

    bool_t ComputeSha256Hex(
        const std::vector<u8_t>& bytes,
        std::string& outHex)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectLength = 0u;
        DWORD hashLength = 0u;
        DWORD written = 0u;

        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0u);
        if (status < 0)
            return false;

        status = BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &written,
            0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashLength),
                sizeof(hashLength),
                &written,
                0u);
        }

        std::vector<u8_t> object;
        std::vector<u8_t> digest;
        if (status >= 0 && hashLength == 32u)
        {
            object.resize(objectLength);
            digest.resize(hashLength);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                static_cast<ULONG>(object.size()),
                nullptr,
                0u,
                0u);
        }
        else if (status >= 0)
        {
            status = static_cast<NTSTATUS>(-1);
        }

        if (status >= 0)
        {
            status = BCryptHashData(
                hash,
                const_cast<PUCHAR>(
                    reinterpret_cast<const UCHAR*>(bytes.data())),
                static_cast<ULONG>(bytes.size()),
                0u);
        }
        if (status >= 0)
        {
            status = BCryptFinishHash(
                hash,
                digest.data(),
                static_cast<ULONG>(digest.size()),
                0u);
        }

        if (hash != nullptr)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0u);
        if (status < 0)
            return false;

        static constexpr char kHex[] = "0123456789abcdef";
        outHex.clear();
        outHex.reserve(digest.size() * 2u);
        for (const u8_t byte : digest)
        {
            outHex.push_back(kHex[byte >> 4u]);
            outHex.push_back(kHex[byte & 0x0Fu]);
        }
        return true;
    }

    u64_t ParseSha256Prefix(const char* value)
    {
        u64_t prefix = 0u;
        for (size_t i = 0u; i < 16u; ++i)
        {
            const char c = value[i];
            const u64_t nibble = c >= '0' && c <= '9'
                ? static_cast<u64_t>(c - '0')
                : static_cast<u64_t>(c - 'a' + 10);
            prefix = (prefix << 4u) | nibble;
        }
        return prefix;
    }

    bool_t VerifyAiShadowPolicyArtifact(
        const std::filesystem::path& artifactPath,
        const std::filesystem::path& tracePath)
    {
        std::vector<u8_t> artifactBytes;
        std::vector<u8_t> traceBytes;
        if (!ReadBinaryFile(artifactPath, artifactBytes) ||
            !ReadBinaryFile(tracePath, traceBytes) ||
            traceBytes.size() < sizeof(AiDecisionTraceV1) ||
            traceBytes.size() % sizeof(AiDecisionTraceV1) != 0u)
        {
            std::printf(
                "[SimLab][AIShadowParity] FAIL: artifact/trace read or size\n");
            return false;
        }

        ChampionAIShadowPolicyArtifactV1 artifact{};
        if (!DecodeChampionAIShadowPolicyArtifactV1(
                artifactBytes.data(),
                artifactBytes.size(),
                artifact))
        {
            std::printf("[SimLab][AIShadowParity] FAIL: artifact decode\n");
            return false;
        }

        AiDecisionTraceV1 trace{};
        std::memcpy(&trace, traceBytes.data(), sizeof(trace));
        const ChampionAIShadowDecisionV1 decision =
            EvaluateChampionAIShadowPolicyV1(&artifact, trace);
        if (decision.status != eChampionAIShadowStatusV1::Evaluated)
        {
            std::printf(
                "[SimLab][AIShadowParity] FAIL: inference status=%u\n",
                static_cast<u32_t>(decision.status));
            return false;
        }

        std::printf(
            "[SimLab][AIShadowParity] PASS: revision=%llu active=%u shadow=%u legal=%08X logits=%.9g,%.9g,%.9g,%.9g margin=%.9g top=%u contribution=%.9g\n",
            static_cast<unsigned long long>(artifact.policyRevision),
            static_cast<u32_t>(decision.activeCandidateKind),
            static_cast<u32_t>(decision.shadowCandidateKind),
            decision.legalCandidateMask,
            static_cast<double>(decision.logits[0]),
            static_cast<double>(decision.logits[1]),
            static_cast<double>(decision.logits[2]),
            static_cast<double>(decision.logits[3]),
            static_cast<double>(decision.selectedMargin),
            static_cast<u32_t>(decision.topFeatureIndex),
            static_cast<double>(decision.topFeatureContribution));
        return true;
    }

    struct ActiveAiPolicyCanaryStats
    {
        u64_t completedTick = 0u;
        u32_t evaluatedDecisionCount = 0u;
        u32_t appliedDecisionCount = 0u;
        u32_t agreementDecisionCount = 0u;
        u32_t interventionDecisionCount = 0u;
        u32_t appliedInterventionCount = 0u;
        u32_t fallbackDecisionCount = 0u;
        u32_t safetyOverrideCount = 0u;
        u32_t rejectedCommandCount = 0u;
        f32_t episodeReturn = 0.f;
        u64_t finalStateHash = 0u;
        bool_t bTerminal = false;
        bool_t bTruncated = false;
        const char* outcome = "time_limit";
    };

    bool_t WriteActiveAiPolicyCanaryReport(
        const std::filesystem::path& reportPath,
        const ChampionAIShadowPolicyArtifactV1& artifact,
        const char* policySha256,
        const char* sideName,
        u64_t seed,
        u64_t tickLimit,
        size_t transitionCount,
        const ActiveAiPolicyCanaryStats& stats)
    {
        const std::string path = reportPath.string();
        FILE* stream = nullptr;
        if (fopen_s(&stream, path.c_str(), "wb") != 0 || !stream)
            return false;

        const int writeResult = std::fprintf(
            stream,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"artifact_type\": \"ActiveMacroCanaryReportV1\",\n"
            "  \"runtime_mode\": \"SIMLAB_DEBUG_CHECKPOINT_TWO_PASS_CANARY\",\n"
            "  \"scenario_id\": \"simlab-1v1-active-macro-lane-v1\",\n"
            "  \"side\": \"%s\",\n"
            "  \"seed\": %llu,\n"
            "  \"tick_limit\": %llu,\n"
            "  \"completed_tick\": %llu,\n"
            "  \"policy_revision\": %llu,\n"
            "  \"source_policy_revision\": %llu,\n"
            "  \"policy_sha256\": \"%s\",\n"
            "  \"policy_binary_sha256_prefix\": \"%016llx\",\n"
            "  \"transition_count\": %zu,\n"
            "  \"evaluated_decision_count\": %u,\n"
            "  \"applied_decision_count\": %u,\n"
            "  \"agreement_decision_count\": %u,\n"
            "  \"intervention_decision_count\": %u,\n"
            "  \"applied_intervention_count\": %u,\n"
            "  \"fallback_decision_count\": %u,\n"
            "  \"safety_override_count\": %u,\n"
            "  \"rejected_command_count\": %u,\n"
            "  \"episode_return\": %.9g,\n"
            "  \"terminal\": %s,\n"
            "  \"truncated\": %s,\n"
            "  \"outcome\": \"%s\",\n"
            "  \"final_state_hash\": \"%016llx\",\n"
            "  \"dataset_usage\": \"EVALUATION_AND_DAGGER_STATE_DISCOVERY_ONLY\",\n"
            "  \"eligible_as_imitation_expert_input\": false,\n"
            "  \"performance_claim\": \"CANARY_ONLY_NOT_PROMOTED\"\n"
            "}\n",
            sideName,
            static_cast<unsigned long long>(seed),
            static_cast<unsigned long long>(tickLimit),
            static_cast<unsigned long long>(stats.completedTick),
            static_cast<unsigned long long>(artifact.policyRevision),
            static_cast<unsigned long long>(artifact.sourcePolicyRevision),
            policySha256,
            static_cast<unsigned long long>(artifact.binarySha256Prefix),
            transitionCount,
            stats.evaluatedDecisionCount,
            stats.appliedDecisionCount,
            stats.agreementDecisionCount,
            stats.interventionDecisionCount,
            stats.appliedInterventionCount,
            stats.fallbackDecisionCount,
            stats.safetyOverrideCount,
            stats.rejectedCommandCount,
            static_cast<double>(stats.episodeReturn),
            stats.bTerminal ? "true" : "false",
            stats.bTruncated ? "true" : "false",
            stats.outcome,
            static_cast<unsigned long long>(stats.finalStateHash));
        const int closeResult = std::fclose(stream);
        return writeResult >= 0 && closeResult == 0;
    }

    bool_t ExportActiveAiMacroEpisode(
        const std::filesystem::path& outputDirectory,
        const std::filesystem::path& artifactPath,
        const char* expectedPolicySha256,
        eTeam botTeam,
        const char* sideName,
        const char* rulesHash,
        const char* definitionHash,
        u64_t seed,
        u64_t tickLimit)
    {
#if !defined(_DEBUG)
        (void)outputDirectory;
        (void)artifactPath;
        (void)expectedPolicySha256;
        (void)botTeam;
        (void)sideName;
        (void)rulesHash;
        (void)definitionHash;
        (void)seed;
        (void)tickLimit;
        std::printf(
            "[SimLab][AIActive] FAIL: active macro canary requires Debug\n");
        return false;
#else
        std::vector<u8_t> artifactBytes;
        ChampionAIShadowPolicyArtifactV1 artifact{};
        std::string actualPolicySha256;
        if (!ReadBinaryFile(artifactPath, artifactBytes) ||
            !ComputeSha256Hex(artifactBytes, actualPolicySha256) ||
            actualPolicySha256 != expectedPolicySha256 ||
            !DecodeChampionAIShadowPolicyArtifactV1(
                artifactBytes.data(),
                artifactBytes.size(),
                artifact))
        {
            std::printf(
                "[SimLab][AIActive] FAIL: artifact read/SHA/decode\n");
            return false;
        }
        artifact.binarySha256Prefix = ParseSha256Prefix(
            actualPolicySha256.c_str());

        constexpr u8_t kLane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        constexpr const char* kScenarioId =
            "simlab-1v1-active-macro-lane-v1";
        const eTeam enemyTeam = botTeam == eTeam::Blue
            ? eTeam::Red
            : eTeam::Blue;
        const f32_t direction = botTeam == eTeam::Blue ? 1.f : -1.f;
        const f32_t seedUnit = static_cast<f32_t>(seed % 17u) / 16.f;
        const Vec3 botPosition{};
        const Vec3 enemyPosition{
            direction * (3.5f + seedUnit),
            0.f,
            0.f };
        const Vec3 minionPosition{
            direction * (4.25f + 0.5f * seedUnit),
            0.f,
            0.f };
        const Vec3 structurePosition{ direction * 14.f, 0.f, 0.f };
        const Vec3 alliedWavePosition{ direction * 12.5f, 0.f, 0.f };

        CWorld world;
        DeterministicRng rng(seed);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        EntityID bot = SpawnChampion(
            world,
            entityMap,
            eChampion::ASHE,
            static_cast<u8_t>(botTeam),
            botTeam == eTeam::Blue ? 0u : 5u);
        EntityID enemy = SpawnChampion(
            world,
            entityMap,
            eChampion::JAX,
            static_cast<u8_t>(enemyTeam),
            enemyTeam == eTeam::Blue ? 0u : 5u);
        world.GetComponent<TransformComponent>(bot).SetPosition(botPosition);
        world.GetComponent<TransformComponent>(enemy).SetPosition(enemyPosition);

        ChampionAIComponent ai{};
        ai.champion = eChampion::ASHE;
        ai.team = botTeam;
        ai.difficulty = 1u;
        ai.lane = kLane;
        ai.activeLane = kLane;
        ai.brainType = eChampionAIBrainType::RuleBased;
        ai.state = eChampionAIState::LaneCombat;
        ai.lastAction = eChampionAIAction::FollowWave;
        ai.intent = eChampionAIIntent::FarmMinion;
        ai.decisionTimer = 0.f;
        ai.intentHoldTimer = 0.f;
        ai.laneGoal = Vec3{ direction * 18.f, 0.f, 0.f };
        ai.safeAnchor = Vec3{ -direction * 8.f, 0.f, 0.f };
        ai.retreatGoal = ai.safeAnchor;
        ai.midDefenseAnchor = botPosition;
        ai.fPostComboBATimer = 1.f;
        ai.bPostComboBAAllowed = true;
        world.AddComponent<ChampionAIComponent>(bot, ai);

        SetMeasuredChampionHpRatio(
            world,
            bot,
            0.78f + 0.18f * seedUnit);
        SetMeasuredChampionHpRatio(
            world,
            enemy,
            0.68f + 0.20f * (1.f - seedUnit));

        const u8_t selfLevel = static_cast<u8_t>(6u + seed % 5u);
        const u8_t enemyLevel = static_cast<u8_t>(6u + (seed * 7u) % 5u);
        world.GetComponent<StatComponent>(bot).level = selfLevel;
        world.GetComponent<StatComponent>(enemy).level = enemyLevel;
        world.GetComponent<ChampionComponent>(bot).level = selfLevel;
        world.GetComponent<ChampionComponent>(enemy).level = enemyLevel;

        EntityID minion = SpawnMeasuredAiTarget(
            world,
            entityMap,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            enemyTeam,
            minionPosition);
        EntityID structure = SpawnMeasuredAiTarget(
            world,
            entityMap,
            GameplayStateQuery::eGameplayTargetKind::Structure,
            enemyTeam,
            structurePosition);
        EntityID alliedWave = SpawnMeasuredAiTarget(
            world,
            entityMap,
            GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
            botTeam,
            alliedWavePosition);
        if (minion == NULL_ENTITY ||
            structure == NULL_ENTITY ||
            alliedWave == NULL_ENTITY)
        {
            std::printf("[SimLab][AIActive] FAIL: scenario target spawn\n");
            return false;
        }

        MinionComponent& minionComponent =
            world.GetComponent<MinionComponent>(minion);
        minionComponent.laneType = kLane;
        minionComponent.hp = 65.f + 20.f * seedUnit;
        minionComponent.maxHp = 100.f;
        HealthComponent& minionHealth =
            world.GetComponent<HealthComponent>(minion);
        minionHealth.fCurrent = minionComponent.hp;
        minionHealth.fMaximum = minionComponent.maxHp;

        StructureComponent& structureComponent =
            world.GetComponent<StructureComponent>(structure);
        structureComponent.kind = static_cast<u32_t>(
            Winters::Map::eObjectKind::Structure_Turret);
        structureComponent.tier = static_cast<u32_t>(
            Winters::Map::eTurretTier::Outer);
        structureComponent.lane = kLane;
        structureComponent.hp = 2600.f;
        structureComponent.maxHp = 3000.f;
        HealthComponent& structureHealth =
            world.GetComponent<HealthComponent>(structure);
        structureHealth.fCurrent = structureComponent.hp;
        structureHealth.fMaximum = structureComponent.maxHp;

        MinionComponent& alliedWaveComponent =
            world.GetComponent<MinionComponent>(alliedWave);
        alliedWaveComponent.laneType = kLane;
        MinionStateComponent& alliedWaveState =
            world.GetComponent<MinionStateComponent>(alliedWave);
        alliedWaveState.team = botTeam;

        const NetEntityId botNetId = entityMap.ToNet(bot);
        const NetEntityId enemyNetId = entityMap.ToNet(enemy);
        const std::vector<NetEntityId> scenarioNetIds{
            botNetId,
            enemyNetId,
            entityMap.ToNet(minion),
            entityMap.ToNet(structure),
            entityMap.ToNet(alliedWave) };
        const auto resolveScenarioEntities = [&]()
        {
            std::vector<EntityID> entities;
            entities.reserve(scenarioNetIds.size());
            for (const NetEntityId netId : scenarioNetIds)
            {
                const EntityID entity = entityMap.FromNet(netId);
                if (entity != NULL_ENTITY)
                    entities.push_back(entity);
            }
            return entities;
        };

        std::vector<GameCommand> aiCommands;
        std::vector<GameCommand> pendingCommands;
        std::vector<LiveAiResearchTransition> transitions;
        LiveAiResearchTransition pendingTransition{};
        bool_t bHasPendingTransition = false;
        f32_t pendingStartHpAdvantage = 0.f;
        ActiveAiPolicyCanaryStats stats{};

        const auto isDead = [&](EntityID entity)
        {
            if (entity == NULL_ENTITY || !world.IsAlive(entity))
                return true;
            if (!world.HasComponent<HealthComponent>(entity))
                return false;
            const HealthComponent& health =
                world.GetComponent<HealthComponent>(entity);
            return health.bIsDead || health.fCurrent <= 0.f;
        };

        for (u64_t tick = 1u; tick <= tickLimit; ++tick)
        {
            bot = entityMap.FromNet(botNetId);
            enemy = entityMap.FromNet(enemyNetId);
            if (bot == NULL_ENTITY || enemy == NULL_ENTITY)
            {
                std::printf(
                    "[SimLab][AIActive] FAIL: actor binding lost tick=%llu\n",
                    static_cast<unsigned long long>(tick));
                return false;
            }

            TickContext tc{};
            tc.tickIndex = tick;
            tc.fDt = DeterministicTime::kFixedDt;
            tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
            tc.pRng = &rng;
            tc.pEntityMap = &entityMap;
            tc.localPlayer = NULL_ENTITY;
            tc.pWalkable = &walkable;
            tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

            GameplayStatus::TickStatusEffects(world, tc);
            GameplayStatus::TickForcedMotions(world, tc);
            if (CBuffSystem::PruneExpiredTickBuffs(world, tc))
                CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());

            const std::vector<EntityID> preDecisionEntities =
                resolveScenarioEntities();
            const f32_t preDecisionHpAdvantage =
                ResolveObservableHpAdvantage(world, bot, enemy);
            const u64_t preDecisionStateHash =
                ComputeAuthoritativeResearchStateHash(
                    world,
                    rng,
                    entityMap,
                    tick,
                    preDecisionEntities);

            bool_t bPolicyEvaluated = false;
            u8_t authoredCandidateKind = 0u;
            u8_t proposedCandidateKind = 0u;
            const ChampionAIComponent& aiBeforeDecision =
                world.GetComponent<ChampionAIComponent>(bot);
            const bool_t bRegularDecisionDue =
                aiBeforeDecision.decisionTimer <= tc.fDt &&
                aiBeforeDecision.debugForcedDecisionCount == 0u;

            aiCommands.clear();
            if (bRegularDecisionDue)
            {
                std::vector<u8_t> keyframeBytes;
                if (!SimCheckpoint::SaveWorldKeyframe(
                        world,
                        rng,
                        entityMap,
                        tick,
                        keyframeBytes))
                {
                    std::printf(
                        "[SimLab][AIActive] FAIL: preflight keyframe save tick=%llu\n",
                        static_cast<unsigned long long>(tick));
                    return false;
                }

                std::vector<GameCommand> preflightCommands;
                CChampionAISystem::Execute(
                    world,
                    tc,
                    preflightCommands,
                    &artifact);
                if (world.HasComponent<ChampionAIResearchDebugComponent>(bot))
                {
                    const ChampionAIResearchDebugComponent& research =
                        world.GetComponent<ChampionAIResearchDebugComponent>(bot);
                    if (research.bShadowDecisionPresent &&
                        research.shadowDecision.tick == tick &&
                        research.shadowDecision.shadowStatus == static_cast<u8_t>(
                            eChampionAIShadowStatusV1::Evaluated))
                    {
                        bPolicyEvaluated = true;
                        authoredCandidateKind =
                            research.shadowDecision.shadowActiveCandidateKind;
                        proposedCandidateKind =
                            research.shadowDecision.shadowSelectedCandidateKind;
                    }
                }

                u64_t restoredTick = 0u;
                if (!SimCheckpoint::RestoreWorldKeyframe(
                        world,
                        rng,
                        entityMap,
                        restoredTick,
                        keyframeBytes) ||
                    restoredTick != tick)
                {
                    std::printf(
                        "[SimLab][AIActive] FAIL: preflight restore tick=%llu restored=%llu\n",
                        static_cast<unsigned long long>(tick),
                        static_cast<unsigned long long>(restoredTick));
                    return false;
                }

                bot = entityMap.FromNet(botNetId);
                enemy = entityMap.FromNet(enemyNetId);
                if (bot == NULL_ENTITY || enemy == NULL_ENTITY)
                {
                    std::printf(
                        "[SimLab][AIActive] FAIL: binding lost after restore\n");
                    return false;
                }

                if (bPolicyEvaluated)
                {
                    if (CandidateBitForKind(authoredCandidateKind) == 0u)
                    {
                        std::printf(
                            "[SimLab][AIActive] FAIL: invalid authored candidate=%u\n",
                            static_cast<u32_t>(authoredCandidateKind));
                        return false;
                    }
                    eChampionAIAction debugAction{};
                    if (!TryMapCandidateToDebugAction(
                            proposedCandidateKind,
                            debugAction))
                    {
                        std::printf(
                            "[SimLab][AIActive] FAIL: unsupported proposal=%u\n",
                            static_cast<u32_t>(proposedCandidateKind));
                        return false;
                    }

                    GameCommand control{};
                    control.kind = eCommandKind::AIDebugControl;
                    control.targetEntity = bot;
                    control.itemId = static_cast<u16_t>(debugAction);
                    control.slot = 0u;
                    executor->ExecuteCommand(world, tc, control);
                    ++stats.evaluatedDecisionCount;
                    if (authoredCandidateKind == proposedCandidateKind)
                        ++stats.agreementDecisionCount;
                    else
                        ++stats.interventionDecisionCount;
                }
                else
                {
                    ++stats.fallbackDecisionCount;
                }
            }

            CChampionAISystem::Execute(world, tc, aiCommands, &artifact);
            for (const GameCommand& command : aiCommands)
                executor->ExecuteCommand(world, tc, command);

            AiDecisionTraceV1 finalizedTrace{};
            const bool_t bHasFinalizedTrace = TryFindPromotableTraceAtTick(
                world,
                bot,
                tick,
                finalizedTrace);
            if (bPolicyEvaluated)
            {
                bool_t bApplied = bHasFinalizedTrace &&
                    finalizedTrace.selectedCandidateKind ==
                        proposedCandidateKind;
                if (world.HasComponent<ChampionAIResearchDebugComponent>(bot))
                {
                    const ChampionAIResearchDebugComponent& research =
                        world.GetComponent<ChampionAIResearchDebugComponent>(bot);
                    if (research.bShadowDecisionPresent &&
                        research.shadowDecision.tick == tick &&
                        research.shadowDecision.shadowStatus == static_cast<u8_t>(
                            eChampionAIShadowStatusV1::Evaluated) &&
                        research.shadowDecision.shadowSelectedCandidateKind !=
                            proposedCandidateKind)
                    {
                        std::printf(
                            "[SimLab][AIActive] FAIL: preflight/final proposal mismatch tick=%llu\n",
                            static_cast<unsigned long long>(tick));
                        return false;
                    }
                }

                if (bApplied)
                    ++stats.appliedDecisionCount;
                if (authoredCandidateKind != proposedCandidateKind && bApplied)
                    ++stats.appliedInterventionCount;
                else if (authoredCandidateKind != proposedCandidateKind)
                    ++stats.safetyOverrideCount;

                GameCommand clearControl{};
                clearControl.kind = eCommandKind::AIDebugControl;
                clearControl.targetEntity = bot;
                clearControl.itemId = kChampionAIDebugClearOverrideItemId;
                executor->ExecuteCommand(world, tc, clearControl);
            }

            if (bHasFinalizedTrace)
            {
                if (bHasPendingTransition)
                {
                    pendingTransition.nextStateHash = preDecisionStateHash;
                    pendingTransition.reward =
                        preDecisionHpAdvantage - pendingStartHpAdvantage;
                    if (!std::isfinite(pendingTransition.reward))
                    {
                        std::printf(
                            "[SimLab][AIActive] FAIL: non-finite interval reward\n");
                        return false;
                    }
                    stats.episodeReturn += pendingTransition.reward;
                    transitions.push_back(pendingTransition);
                }

                pendingTransition = LiveAiResearchTransition{};
                pendingTransition.trace = finalizedTrace;
                pendingStartHpAdvantage = preDecisionHpAdvantage;
                bHasPendingTransition = true;
                if (finalizedTrace.executorState == static_cast<u8_t>(
                        AiExecutorStateV1::Rejected))
                {
                    ++stats.rejectedCommandCount;
                }
            }

            CSpellbookFormOverrideSystem::Execute(world, tc);
            CAreaAuraSystem::Execute(world, tc);
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CBuffSystem::AdvanceDurationsAfterStat(world, tc);
            CSkillCooldownSystem::Execute(world, tc);
            CRecallSystem::Execute(world, tc);
            CGoldIncomeSystem::Execute(world, tc);
            CWaypointPatrolSystem::Execute(world, tc);
            CCombatActionSystem::Execute(world, tc);
            CMoveSystem::Execute(world, tc);
            CJungleAISystem::Execute(world, tc, pendingCommands);
            CAttackChaseSystem::Execute(world, tc, pendingCommands);
            for (const GameCommand& command : pendingCommands)
                executor->ExecuteCommand(world, tc, command);
            pendingCommands.clear();
            AnnieGameSim::Tick(world, tc);
            AsheGameSim::Tick(world, tc);
            EzrealGameSim::Tick(world, tc);
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
            CStatSystem::Execute(world, ServerData::GetLoLGameplayDefinitionPack());
            CDeathSystem::Execute(world, tc);

            stats.completedTick = tick;
            const bool_t bBotDead = isDead(bot);
            const bool_t bEnemyDead = isDead(enemy);
            const bool_t bTerminal = bBotDead || bEnemyDead;
            const bool_t bTimeLimit = tick == tickLimit;
            if (!bTerminal && !bTimeLimit)
                continue;

            const std::vector<EntityID> finalEntities =
                resolveScenarioEntities();
            stats.finalStateHash = ComputeAuthoritativeResearchStateHash(
                world,
                rng,
                entityMap,
                tick,
                finalEntities);
            stats.bTerminal = bTerminal;
            stats.bTruncated = !bTerminal;
            stats.outcome = bBotDead && bEnemyDead
                ? "draw"
                : bEnemyDead
                    ? "actor_win"
                    : bBotDead
                        ? "actor_loss"
                        : "time_limit";

            if (!bHasPendingTransition)
            {
                std::printf(
                    "[SimLab][AIActive] FAIL: episode ended without a final decision\n");
                return false;
            }

            const f32_t finalHpAdvantage =
                ResolveObservableHpAdvantage(world, bot, enemy);
            pendingTransition.nextStateHash = stats.finalStateHash;
            pendingTransition.reward =
                finalHpAdvantage - pendingStartHpAdvantage;
            pendingTransition.bTerminal = bTerminal;
            pendingTransition.bTruncated = !bTerminal;
            if (!std::isfinite(pendingTransition.reward))
            {
                std::printf(
                    "[SimLab][AIActive] FAIL: non-finite final reward\n");
                return false;
            }
            stats.episodeReturn += pendingTransition.reward;
            transitions.push_back(pendingTransition);
            bHasPendingTransition = false;
            break;
        }

        if (transitions.empty() ||
            stats.completedTick == 0u ||
            stats.evaluatedDecisionCount == 0u ||
            stats.appliedDecisionCount == 0u ||
            stats.evaluatedDecisionCount !=
                stats.agreementDecisionCount +
                    stats.interventionDecisionCount ||
            stats.interventionDecisionCount !=
                stats.appliedInterventionCount +
                    stats.safetyOverrideCount ||
            stats.appliedDecisionCount > stats.evaluatedDecisionCount ||
            stats.appliedInterventionCount > stats.appliedDecisionCount ||
            (!stats.bTerminal && !stats.bTruncated))
        {
            std::printf(
                "[SimLab][AIActive] FAIL: records=%zu evaluated=%u applied=%u interventions=%u applied_interventions=%u terminal=%u truncated=%u\n",
                transitions.size(),
                stats.evaluatedDecisionCount,
                stats.appliedDecisionCount,
                stats.interventionDecisionCount,
                stats.appliedInterventionCount,
                stats.bTerminal ? 1u : 0u,
                stats.bTruncated ? 1u : 0u);
            return false;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(outputDirectory, directoryError);
        if (directoryError)
        {
            std::printf(
                "[SimLab][AIActive] FAIL: output directory: %s\n",
                directoryError.message().c_str());
            return false;
        }

        std::vector<AiDecisionTraceV1> traces;
        traces.reserve(transitions.size());
        for (const LiveAiResearchTransition& transition : transitions)
            traces.push_back(transition.trace);

        char episodeId[192]{};
        sprintf_s(
            episodeId,
            "simlab-active-macro-v1-%s-%llu-r%llu-p%016llx",
            sideName,
            static_cast<unsigned long long>(seed),
            static_cast<unsigned long long>(artifact.policyRevision),
            static_cast<unsigned long long>(artifact.binarySha256Prefix));
        const std::filesystem::path capturePath =
            outputDirectory / "decision_trace_v1.bin";
        const std::filesystem::path metadataPath =
            outputDirectory / "episode_metadata.json";
        const std::filesystem::path reportPath =
            outputDirectory / "active_policy_canary_v1.json";
        const std::string capturePathText = capturePath.string();
        if (!Winters::AIResearchTools::WriteAiDecisionTraceCaptureV1(
                capturePathText.c_str(),
                traces.data(),
                traces.size()) ||
            !WriteLiveAiResearchMetadata(
                metadataPath,
                rulesHash,
                definitionHash,
                seed,
                episodeId,
                kScenarioId,
                artifact.policyRevision,
                transitions) ||
            !WriteActiveAiPolicyCanaryReport(
                reportPath,
                artifact,
                actualPolicySha256.c_str(),
                sideName,
                seed,
                tickLimit,
                transitions.size(),
                stats))
        {
            std::printf(
                "[SimLab][AIActive] FAIL: capture/metadata/report write\n");
            return false;
        }

        std::printf(
            "[SimLab][AIActive] PASS: side=%s seed=%llu ticks=%llu records=%zu evaluated=%u applied=%u interventions=%u applied_interventions=%u fallback=%u safety=%u rejected=%u return=%.6f outcome=%s state=%016llX\n",
            sideName,
            static_cast<unsigned long long>(seed),
            static_cast<unsigned long long>(stats.completedTick),
            transitions.size(),
            stats.evaluatedDecisionCount,
            stats.appliedDecisionCount,
            stats.interventionDecisionCount,
            stats.appliedInterventionCount,
            stats.fallbackDecisionCount,
            stats.safetyOverrideCount,
            stats.rejectedCommandCount,
            static_cast<double>(stats.episodeReturn),
            stats.outcome,
            static_cast<unsigned long long>(stats.finalStateHash));
        return true;
#endif
    }

    bool_t RunChampionAIShadowPolicyContractProbe()
    {
        f64_t benchmarkNanosecondsPerEvaluation = 0.0;
        ChampionAIShadowPolicyArtifactV1 artifact{};
        artifact.policyRevision = 2u;
        artifact.sourcePolicyRevision = 1u;
        artifact.featureOrderSha256Prefix =
            kChampionAIShadowFeatureOrderSha256PrefixV1;
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
            artifact.normalizationInverseScale[i] = 1.f;
        artifact.weights[0] = 1.f;
        artifact.weights[1] = 2.f;
        artifact.weights[2] = 100.f;
        artifact.weights[3] = 200.f;

        AiDecisionTraceV1 trace = ChampionAIResearch::MakeDecisionTraceV1();
        trace.tick = 1u;
        trace.candidateCount = kAiDecisionCandidateCapacityV1;
        trace.selectedCandidateKind = static_cast<u8_t>(AiCandidateKindV1::Fight);
        trace.observation.factTick = trace.tick;
        trace.observation.selfNetEntityId = 1u;
        trace.actionMask.legalCandidateMask =
            kAiCandidateRetreatBitV1 | kAiCandidateFightBitV1;
        trace.actionMask.illegalCandidateMask =
            kAiAllCandidateBitsV1 & ~trace.actionMask.legalCandidateMask;
        for (u8_t i = 0u; i < kAiDecisionCandidateCapacityV1; ++i)
        {
            AiCandidateEvidenceV1& candidate = trace.candidates[i];
            candidate.candidateKind = static_cast<u8_t>(i + 1u);
            const u32_t bit = 1u << i;
            candidate.flags = (trace.actionMask.legalCandidateMask & bit) != 0u
                ? kAiCandidateLegalFlagV1
                : 0u;
        }

        const AiDecisionTraceV1 traceBefore = trace;
        const ChampionAIShadowDecisionV1 masked =
            EvaluateChampionAIShadowPolicyV1(&artifact, trace);
        if (masked.status != eChampionAIShadowStatusV1::Evaluated ||
            masked.shadowCandidateKind != static_cast<u8_t>(AiCandidateKindV1::Fight) ||
            masked.logits[1] != 2.f ||
            std::memcmp(&trace, &traceBefore, sizeof(trace)) != 0)
        {
            std::printf(
                "[SimLab][AIShadow] FAIL: legal mask, argmax, or purity\n");
            return false;
        }

        constexpr u32_t kBenchmarkEvaluationCount = 10000u;
        u64_t benchmarkChecksum = 0u;
        const auto benchmarkStart = std::chrono::steady_clock::now();
        for (u32_t i = 0u; i < kBenchmarkEvaluationCount; ++i)
        {
            const ChampionAIShadowDecisionV1 measured =
                EvaluateChampionAIShadowPolicyV1(&artifact, trace);
            benchmarkChecksum += measured.shadowCandidateKind;
            benchmarkChecksum += measured.topFeatureIndex;
        }
        const auto benchmarkEnd = std::chrono::steady_clock::now();
        benchmarkNanosecondsPerEvaluation =
            static_cast<f64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                benchmarkEnd - benchmarkStart).count()) /
            static_cast<f64_t>(kBenchmarkEvaluationCount);
        if (benchmarkChecksum == 0u ||
            !std::isfinite(benchmarkNanosecondsPerEvaluation))
        {
            std::printf("[SimLab][AIShadow] FAIL: inference benchmark\n");
            return false;
        }

        artifact.weights[0] = 0.f;
        artifact.weights[1] = 0.f;
        const ChampionAIShadowDecisionV1 tied =
            EvaluateChampionAIShadowPolicyV1(&artifact, trace);
        if (tied.status != eChampionAIShadowStatusV1::Evaluated ||
            tied.shadowCandidateKind != static_cast<u8_t>(AiCandidateKindV1::Retreat) ||
            tied.selectedMargin != 0.f)
        {
            std::printf("[SimLab][AIShadow] FAIL: stable tie-break\n");
            return false;
        }

        trace.actionMask.legalCandidateMask = kAiCandidateRetreatBitV1;
        trace.actionMask.illegalCandidateMask =
            kAiAllCandidateBitsV1 & ~trace.actionMask.legalCandidateMask;
        trace.selectedCandidateKind =
            static_cast<u8_t>(AiCandidateKindV1::Retreat);
        for (u8_t i = 0u; i < kAiDecisionCandidateCapacityV1; ++i)
        {
            trace.candidates[i].flags = i == 0u
                ? kAiCandidateLegalFlagV1
                : 0u;
        }
        const ChampionAIShadowDecisionV1 singleLegal =
            EvaluateChampionAIShadowPolicyV1(&artifact, trace);
        if (singleLegal.status !=
            eChampionAIShadowStatusV1::InsufficientLegalCandidates)
        {
            std::printf("[SimLab][AIShadow] FAIL: single-legal status\n");
            return false;
        }

        AiDecisionTraceV1 unsupportedTrace = traceBefore;
        unsupportedTrace.selectedCandidateKind =
            static_cast<u8_t>(AiCandidateKindV1::None);
        const ChampionAIShadowDecisionV1 unsupportedDecision =
            EvaluateChampionAIShadowPolicyV1(&artifact, unsupportedTrace);
        if (unsupportedDecision.status !=
            eChampionAIShadowStatusV1::InvalidTrace)
        {
            std::printf("[SimLab][AIShadow] FAIL: unsupported active candidate gate\n");
            return false;
        }

        artifact.normalizationInverseScale[0] =
            (std::numeric_limits<f32_t>::denorm_min)();
        const ChampionAIShadowDecisionV1 subnormalArtifact =
            EvaluateChampionAIShadowPolicyV1(&artifact, traceBefore);
        if (subnormalArtifact.status !=
            eChampionAIShadowStatusV1::InvalidArtifact)
        {
            std::printf("[SimLab][AIShadow] FAIL: subnormal artifact gate\n");
            return false;
        }

        artifact.normalizationInverseScale[0] = 0.f;
        const ChampionAIShadowDecisionV1 invalidArtifact =
            EvaluateChampionAIShadowPolicyV1(&artifact, traceBefore);
        if (invalidArtifact.status != eChampionAIShadowStatusV1::InvalidArtifact)
        {
            std::printf("[SimLab][AIShadow] FAIL: invalid artifact gate\n");
            return false;
        }

        struct NonInterferenceOutcome
        {
            u64_t authoritativeHash = 0u;
            eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
            eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
            std::vector<GameCommand> commands;
            std::vector<u8_t> keyframeBytes;
            bool_t bKeyframeSaved = false;
            bool_t bSawShadowEvaluated = false;
            bool_t bSawShadowDisagreed = false;
            u32_t shadowEvaluationCount = 0u;
        };
        const auto runDecision = [](
            const ChampionAIShadowPolicyArtifactV1* policy)
        {
            NonInterferenceOutcome outcome{};
            CWorld world;
            DeterministicRng rng(20260713u);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID bot = SpawnChampion(
                world,
                entityMap,
                eChampion::ASHE,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            const EntityID enemy = SpawnChampion(
                world,
                entityMap,
                eChampion::JAX,
                static_cast<u8_t>(eTeam::Red),
                5u);
            world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(enemy).SetPosition(
                Vec3{ 4.f, 0.f, 0.f });
            SetMeasuredChampionHpRatio(world, bot, 0.90f);
            SetMeasuredChampionHpRatio(world, enemy, 0.25f);

            ChampionAIComponent ai{};
            ai.champion = eChampion::ASHE;
            ai.team = eTeam::Blue;
            ai.brainType = eChampionAIBrainType::RuleBased;
            ai.state = eChampionAIState::LaneCombat;
            ai.intent = eChampionAIIntent::FarmMinion;
            ai.decisionTimer = 0.f;
            ai.laneGoal = Vec3{ 12.f, 0.f, 0.f };
            ai.safeAnchor = Vec3{ -6.f, 0.f, 0.f };
            ai.retreatGoal = ai.safeAnchor;
            world.AddComponent<ChampionAIComponent>(bot, ai);

            constexpr u64_t kProbeTicks = 300u;
            u64_t cumulativeHash = 1469598103934665603ull;
            for (u64_t tick = 1u; tick <= kProbeTicks; ++tick)
            {
                TickContext tc{};
                tc.tickIndex = tick;
                tc.fDt = DeterministicTime::kFixedDt;
                tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
                tc.pRng = &rng;
                tc.pEntityMap = &entityMap;
                tc.pWalkable = &walkable;
                tc.pDefinitions = &ServerData::GetLoLGameplayDefinitionPack();

                std::vector<GameCommand> tickCommands;
                CChampionAISystem::Execute(world, tc, tickCommands, policy);
                for (const GameCommand& command : tickCommands)
                {
                    executor->ExecuteCommand(world, tc, command);
                    outcome.commands.push_back(command);
                }

                const u64_t tickHash = ComputeAuthoritativeResearchStateHash(
                    world,
                    rng,
                    entityMap,
                    tick,
                    bot,
                    enemy);
                HashU64(cumulativeHash, tickHash);
                if (world.HasComponent<ChampionAIResearchDebugComponent>(bot))
                {
                    const auto& research =
                        world.GetComponent<ChampionAIResearchDebugComponent>(bot);
                    if (research.bShadowDecisionPresent &&
                        research.shadowDecision.tick == tick &&
                        research.shadowDecision.shadowStatus ==
                            static_cast<u8_t>(eChampionAIShadowStatusV1::Evaluated))
                    {
                        ++outcome.shadowEvaluationCount;
                        outcome.bSawShadowEvaluated = true;
                        outcome.bSawShadowDisagreed =
                            outcome.bSawShadowDisagreed ||
                            research.shadowDecision.bShadowDisagreed;
                    }
                }
            }

            const ChampionAIComponent& finalAI =
                world.GetComponent<ChampionAIComponent>(bot);
            outcome.intent = finalAI.intent;
            outcome.action = finalAI.lastAction;
            outcome.authoritativeHash = cumulativeHash;
            outcome.bKeyframeSaved = SimCheckpoint::SaveWorldKeyframe(
                world,
                rng,
                entityMap,
                kProbeTicks,
                outcome.keyframeBytes);
            return outcome;
        };
        ChampionAIShadowPolicyArtifactV1 disagreementArtifact{};
        disagreementArtifact.policyRevision = 2u;
        disagreementArtifact.sourcePolicyRevision = 1u;
        disagreementArtifact.featureOrderSha256Prefix =
            kChampionAIShadowFeatureOrderSha256PrefixV1;
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
            disagreementArtifact.normalizationInverseScale[i] = 1.f;
        disagreementArtifact.weights[0] = 10.f;
        disagreementArtifact.weights[1] = -10.f;

        const NonInterferenceOutcome disabled = runDecision(nullptr);
        const NonInterferenceOutcome enabled = runDecision(&disagreementArtifact);
        const auto sameCommand = [](const GameCommand& lhs, const GameCommand& rhs)
        {
            return lhs.kind == rhs.kind &&
                lhs.issuerEntity == rhs.issuerEntity &&
                lhs.issuedAtTick == rhs.issuedAtTick &&
                lhs.sequenceNum == rhs.sequenceNum &&
                lhs.slot == rhs.slot &&
                lhs.targetEntity == rhs.targetEntity &&
                lhs.groundPos.x == rhs.groundPos.x &&
                lhs.groundPos.y == rhs.groundPos.y &&
                lhs.groundPos.z == rhs.groundPos.z &&
                lhs.direction.x == rhs.direction.x &&
                lhs.direction.y == rhs.direction.y &&
                lhs.direction.z == rhs.direction.z &&
                lhs.itemId == rhs.itemId;
        };
        bool_t bCommandsEqual =
            disabled.commands.size() == enabled.commands.size();
        for (size_t i = 0u; bCommandsEqual && i < disabled.commands.size(); ++i)
            bCommandsEqual = sameCommand(disabled.commands[i], enabled.commands[i]);
        if (!bCommandsEqual ||
            disabled.authoritativeHash != enabled.authoritativeHash ||
            disabled.intent != enabled.intent ||
            disabled.action != enabled.action ||
            !disabled.bKeyframeSaved ||
            !enabled.bKeyframeSaved ||
            disabled.keyframeBytes != enabled.keyframeBytes ||
            !enabled.bSawShadowEvaluated ||
            !enabled.bSawShadowDisagreed ||
            enabled.shadowEvaluationCount == 0u)
        {
            std::printf(
                "[SimLab][AIShadow] FAIL: shadow changed authoritative result or disagreement evidence missing\n");
            return false;
        }

        std::printf(
            "[SimLab][AIShadow] PASS: legal-only argmax, tie-break, purity, single-legal, unsupported active candidate, subnormal/invalid artifact, 300-tick command/state/keyframe non-interference\n");
        std::printf(
            "[SimLab][AIShadow] benchmark_debug_ns_per_eval=%.1f evaluated_decisions_300_ticks=%u max_dot_terms_per_eval=%u\n",
            benchmarkNanosecondsPerEvaluation,
            enabled.shadowEvaluationCount,
            static_cast<u32_t>(
                kChampionAIShadowFeatureCountV1 *
                kChampionAIShadowCandidateCountV1));
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1 &&
        std::strcmp(argv[1], "--run-ai-active-macro-episode") == 0)
    {
        if (argc != 10)
        {
            std::printf(
                "Usage: SimLab.exe --run-ai-active-macro-episode <out-dir> <policy.wbc> <policy_sha256> <blue|red> <rules_sha256> <definition_sha256> <seed> <tick-limit>\n");
            return 2;
        }

        eTeam botTeam = eTeam::Neutral;
        if (std::strcmp(argv[5], "blue") == 0)
            botTeam = eTeam::Blue;
        else if (std::strcmp(argv[5], "red") == 0)
            botTeam = eTeam::Red;
        else
        {
            std::printf(
                "[SimLab][AIActive] input error: side must be blue or red\n");
            return 2;
        }

        if (!IsNonPlaceholderLowerSha256(argv[4]) ||
            !IsNonPlaceholderLowerSha256(argv[6]) ||
            !IsNonPlaceholderLowerSha256(argv[7]))
        {
            std::printf(
                "[SimLab][AIActive] input error: hashes must be non-placeholder lowercase 64hex\n");
            return 2;
        }

        u64_t seed = 0u;
        u64_t tickLimit = 0u;
        if (!TryParseU64(argv[8], seed) ||
            !TryParseU64(argv[9], tickLimit) ||
            tickLimit < 30u ||
            tickLimit > 18000u)
        {
            std::printf(
                "[SimLab][AIActive] input error: seed must be unsigned and tick-limit must be 30..18000\n");
            return 2;
        }

        RegisterAllChampionHooks();
        return ExportActiveAiMacroEpisode(
            std::filesystem::path(argv[2]),
            std::filesystem::path(argv[3]),
            argv[4],
            botTeam,
            argv[5],
            argv[6],
            argv[7],
            seed,
            tickLimit)
            ? 0
            : 1;
    }

    if (argc > 1 &&
        std::strcmp(argv[1], "--verify-ai-shadow-policy") == 0)
    {
        if (argc != 4)
        {
            std::printf(
                "Usage: SimLab.exe --verify-ai-shadow-policy <policy.wbc> <decision_trace_v1.bin>\n");
            return 2;
        }
        return VerifyAiShadowPolicyArtifact(
            std::filesystem::path(argv[2]),
            std::filesystem::path(argv[3]))
            ? 0
            : 1;
    }

    if (argc > 1 &&
        std::strcmp(argv[1], "--export-ai-research-episode") == 0)
    {
        if (argc != 9)
        {
            std::printf(
                "Usage: SimLab.exe --export-ai-research-episode <out-dir> <fight|retreat|farm|siege> <scenario-id> <blue|red> <rules_sha256> <definition_sha256> <seed>\n");
            return 2;
        }

        MeasuredAiScenarioFamily family{};
        if (!TryParseMeasuredAiScenarioFamily(argv[3], family) ||
            !IsSafeMeasuredScenarioId(argv[4]))
        {
            std::printf(
                "[SimLab][AIMeasured] input error: invalid family or scenario id\n");
            return 2;
        }

        eTeam botTeam = eTeam::Neutral;
        if (std::strcmp(argv[5], "blue") == 0)
            botTeam = eTeam::Blue;
        else if (std::strcmp(argv[5], "red") == 0)
            botTeam = eTeam::Red;
        else
        {
            std::printf(
                "[SimLab][AIMeasured] input error: side must be blue or red\n");
            return 2;
        }

        if (!IsNonPlaceholderLowerSha256(argv[6]) ||
            !IsNonPlaceholderLowerSha256(argv[7]))
        {
            std::printf(
                "[SimLab][AIMeasured] input error: hashes must be non-placeholder lowercase 64hex\n");
            return 2;
        }

        u64_t seed = 0u;
        if (!TryParseU64(argv[8], seed))
        {
            std::printf(
                "[SimLab][AIMeasured] input error: seed must be an unsigned decimal integer\n");
            return 2;
        }

        RegisterAllChampionHooks();
        return ExportMeasuredAiResearchEpisode(
            std::filesystem::path(argv[2]),
            family,
            argv[3],
            argv[4],
            botTeam,
            argv[5],
            argv[6],
            argv[7],
            seed)
            ? 0
            : 1;
    }

    if (argc > 1 &&
        std::strcmp(argv[1], "--export-ai-research-smoke") == 0)
    {
        if (argc != 5 && argc != 6)
        {
            std::printf(
                "Usage: SimLab.exe --export-ai-research-smoke <out-dir> <rules_sha256> <definition_sha256> [seed]\n");
            return 2;
        }
        if (!IsNonPlaceholderLowerSha256(argv[3]) ||
            !IsNonPlaceholderLowerSha256(argv[4]))
        {
            std::printf(
                "[SimLab][AILiveSmoke] input error: hashes must be non-placeholder lowercase 64hex\n");
            return 2;
        }

        u64_t seed = 42u;
        if (argc == 6 && !TryParseU64(argv[5], seed))
        {
            std::printf(
                "[SimLab][AILiveSmoke] input error: seed must be an unsigned decimal integer\n");
            return 2;
        }

        RegisterAllChampionHooks();
        return ExportLiveAiResearchSmoke(
            std::filesystem::path(argv[2]),
            argv[3],
            argv[4],
            seed)
            ? 0
            : 1;
    }

    if (argc > 1 && std::strcmp(argv[1], "--ezreal-only") == 0)
    {
        std::printf("[SimLab] Ezreal projectile authority probe only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunEzrealProjectileAuthorityProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }

    if (argc > 1 && std::strcmp(argv[1], "--kalista-projectile-only") == 0)
    {
        std::printf("[SimLab] Kalista projectile authority probe only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunKalistaProjectileAuthorityProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }

    if (argc > 1 && std::strcmp(argv[1], "--kalista-sentinel-only") == 0)
    {
        std::printf("[SimLab] Kalista sentinel authority probe only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunKalistaSentinelAuthorityProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }

    if (argc > 1 && std::strcmp(argv[1], "--item-move-speed-only") == 0)
    {
        RegisterAllChampionHooks();
        const bool_t bPass = RunItemMoveSpeedScaleProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }

    if (argc > 1 && std::strcmp(argv[1], "--shield-only") == 0)
    {
        RegisterAllChampionHooks();
        const bool_t bPass = RunServerAuthoritativeShieldProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }

    const u64_t tickCount = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1800ull;
    const u64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42ull;

    std::printf("[SimLab] ticks=%llu seed=%llu (5v5 scripted match, 30Hz)\n",
        static_cast<unsigned long long>(tickCount),
        static_cast<unsigned long long>(seed));

    RegisterAllChampionHooks();

    const bool_t bServerAuthoritativeStatusProbePass =
        RunServerAuthoritativeStatusProbe();
    const bool_t bServerAuthoritativeShieldProbePass =
        RunServerAuthoritativeShieldProbe();
    const bool_t bSylasUltimateCoverageProbePass =
        RunSylasUltimateCoverageProbe();
    const bool_t bKalistaOathswornFateCallProbePass =
        RunKalistaOathswornFateCallProbe();
    const bool_t bChampionAIStateGateCommitmentProbePass =
        RunChampionAIStateGateCommitmentProbe();
    const bool_t bCombatActionGenerationProbePass =
        RunCombatActionGenerationProbe();
    const bool_t bKalistaProjectileAuthorityProbePass =
        RunKalistaProjectileAuthorityProbe();
    const bool_t bKalistaSentinelAuthorityProbePass =
        RunKalistaSentinelAuthorityProbe();
    const bool_t bEzrealProjectileAuthorityProbePass =
        RunEzrealProjectileAuthorityProbe();
    const bool_t bChampionAIObservationFowProbePass =
        RunChampionAIObservationFowProbe();
    const bool_t bChampionAIMidDefenseDeterminismProbePass =
        RunChampionAIMidDefenseDeterminismProbe();
    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();
    const bool_t bViegoPossessionProbePass = RunViegoPossessionProbe();
    const bool_t bActionMovePolicyProbePass = RunActionMovePolicyProbe();
    const bool_t bPracticeDefinitionOverlayProbePass =
        RunPracticeDefinitionOverlayProbe();
    const bool_t bItemMoveSpeedScaleProbePass =
        RunItemMoveSpeedScaleProbe();
    const bool_t bAttackSpeedLabMatrixProbePass =
        RunAttackSpeedLabMatrixProbe();
    const bool_t bAuthoredNavGridProbePass = RunAuthoredNavGridProbe();
    const bool_t bKeyframeTransactionalFailureProbePass =
        RunKeyframeTransactionalFailureProbe();
    const bool_t bEntityIdMapBijectionProbePass =
        RunEntityIdMapBijectionProbe();
    const bool_t bCommandExecutionOutcomeTraceProbePass =
        RunCommandExecutionOutcomeTraceProbe();
    const bool_t bKeyframeRestoreDeterminismProbePass =
        RunKeyframeRestoreDeterminismProbe();
    const bool_t bChampionAIShadowPolicyContractProbePass =
        RunChampionAIShadowPolicyContractProbe();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    const bool_t bGameplayFormulaDataDrivenProbePass =
        RunGameplayFormulaDataDrivenProbe();
    const bool_t bBladeOfTheRuinedKingProbePass =
        RunBladeOfTheRuinedKingProbe();
    const bool_t bSkillRankGateProbePass =
        RunSkillRankGateProbe();

    bool bPass = bServerAuthoritativeStatusProbePass &&
        bServerAuthoritativeShieldProbePass &&
        bSylasUltimateCoverageProbePass &&
        bKalistaOathswornFateCallProbePass &&
        bChampionAIStateGateCommitmentProbePass &&
        bCombatActionGenerationProbePass &&
        bKalistaProjectileAuthorityProbePass &&
        bKalistaSentinelAuthorityProbePass &&
        bEzrealProjectileAuthorityProbePass &&
        bChampionAIObservationFowProbePass &&
        bChampionAIMidDefenseDeterminismProbePass &&
        bYoneEReturnProbePass &&
        bViegoPossessionProbePass &&
        bActionMovePolicyProbePass &&
        bPracticeDefinitionOverlayProbePass &&
        bItemMoveSpeedScaleProbePass &&
        bGameplayFormulaDataDrivenProbePass &&
        bBladeOfTheRuinedKingProbePass &&
        bSkillRankGateProbePass &&
        bAttackSpeedLabMatrixProbePass &&
        bAuthoredNavGridProbePass &&
        bKeyframeTransactionalFailureProbePass &&
        bEntityIdMapBijectionProbePass &&
        bCommandExecutionOutcomeTraceProbePass &&
        bKeyframeRestoreDeterminismProbePass &&
        bChampionAIShadowPolicyContractProbePass;

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
