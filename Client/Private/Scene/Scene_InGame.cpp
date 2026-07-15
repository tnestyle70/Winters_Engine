#define _CRT_SECURE_NO_WARNINGS

#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/EventApplier.h"
#include "Network/Client/GameSessionClient.h"
#include "Network/Client/SnapshotApplier.h"
#include "Replay/ReplayPlayer.h"

#include <Windows.h>
#include "Scene/GameplayQuery.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_InGameInternal.h"
#include "Scene/Scene_Editor.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/AmbientProp_Manager.h"
#include "Map/MapDataIO.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "Manager/UI/ActorHUDState.h"
#include "Manager/UI/StatusPanelState.h"
#include "Manager/UI/WorldHealthBarState.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/NavigationThrottleSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "ECS/Components/AIControlComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/NavigationControlComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/Pathfinder.h"

// S030: 게임 종료 산출물 저장 + 메인 메뉴 복귀
#include "ClientShell/ClientShellSession.h"
#include "ClientShell/ClientShellBackendService.h"
#include "Replay/LocalMatchRecord.h"
#include "Scene/Scene_MainMenu.h"
#include "Scene/Scene_MyInfo.h"
#include "UI/AiTraceExport.h"

// [Phase T] UI Panels + DebugDrawSystem
#include "UI/AIDebugPanel.h"
#include "UI/CombatDebugPanel.h"
#include "UI/MapTunerPanel.h"
#include "UI/RenderDebug.h"
#include "UI/DebugDrawSystem.h"
#include "UI/SkillTimingPanel.h"
#include "UI/ChampionTuner.h"
#include "UI/EffectTuner.h"
#include "UI/WfxEffectToolPanel.h"
#include "UI/MinimapPanel.h"
#include "Network/Client/NetworkEventTrace.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

#include "Resource/Animator.h"
#include "Resource/Animation.h"
#include "Renderer/RHISceneRenderer.h"

#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "GameObject/ChampionSpawnService.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "GamePlay/SkillHookRegistry.h"
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

// [Phase T-8] FX / Status / Irelia Blade / Ult Wave
#include "ECS/Systems/StatusEffectSystem.h"
#include "Shared/GameSim/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "ECS/Components/MeshGroupVisibilityComponent.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITextureLoader.h"
#include "RHI/RHITypes.h"
#include "Renderer/FogOfWarRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>

CScene_InGame::CScene_InGame() = default;

CScene_InGame::CScene_InGame(const wstring_t& replayPath)
    : m_bReplayPlaybackMode(true)
    , m_strReplayPath(replayPath)
{
}

CScene_InGame::~CScene_InGame() = default;

namespace
{
    bool_t ShouldRunInGameSkillSmoke()
    {
        return HasCommandLineToken(L"--banpick-smoke")
            && !HasCommandLineToken(L"--smoke-no-skill");
    }

    bool_t IsValidChampionId(eChampion champion)
    {
        return champion != eChampion::END && champion != eChampion::NONE;
    }

    constexpr u8_t kLoLUiSkillSlotR = 4u;

    u8_t ToLoLUIContentId(eChampion champion)
    {
        return static_cast<u8_t>(champion);
    }

    u8_t ToLoLUITeamId(eTeam team)
    {
        return (team == eTeam::TEAM_END)
            ? 255u
            : static_cast<u8_t>(team);
    }

    eChampion ResolveLocalRosterChampion(const MatchContext& context)
    {
        if (context.bUseNetworkRoster)
        {
            for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
            {
                const GameRosterSlot& slot = context.Roster[i];
                if (!slot.bHuman || !IsValidChampionId(slot.champion))
                    continue;
                if (context.MySessionId != 0 && slot.sessionId == context.MySessionId)
                    return slot.champion;
            }

            for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
            {
                const GameRosterSlot& slot = context.Roster[i];
                if (!slot.bHuman || !IsValidChampionId(slot.champion))
                    continue;
                if (context.MyNetId != 0 && slot.netId == context.MyNetId)
                    return slot.champion;
            }

            if (context.MySlotId != kInvalidGameRosterSlot
                && context.MySlotId < kGameRosterSlotCount)
            {
                const GameRosterSlot& slot = context.Roster[context.MySlotId];
                if ((slot.bHuman || slot.bBot) && IsValidChampionId(slot.champion))
                    return slot.champion;
            }
        }

        return context.SelectedChampion;
    }
}

eChampion CScene_InGame::GetPlayerChampionId() const
{
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        return m_World.GetComponent<ChampionComponent>(m_PlayerEntity).id;
    }

    return ResolveLocalRosterChampion(Client::CLoLMatchContextRuntime::Instance().Context());
}

bool CScene_InGame::HasPlayerTransform() const
{
    return m_pPlayerTransform != nullptr;
}

Vec3 CScene_InGame::GetPlayerPosition() const
{
    if (m_pPlayerTransform)
        return m_pPlayerTransform->GetPosition();
    return Vec3{};
}

void CScene_InGame::SetPlayerPosition(const Vec3& v)
{
    if (!m_pPlayerTransform)
        return;

    m_pPlayerTransform->SetPosition(v);
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    m_World.GetComponent<TransformComponent>(m_PlayerEntity).SetPosition(v);
}

f32_t CScene_InGame::GetPlayerYaw() const
{
    return m_pPlayerTransform ? m_pPlayerTransform->GetRotation().y : 0.f;
}

void CScene_InGame::SetPlayerYaw(f32_t yaw)
{
    if (!m_pPlayerTransform)
        return;

    Vec3 rot = m_pPlayerTransform->GetRotation();
    const f32_t previousYaw = rot.y;
    const f32_t normalizedYaw = NormalizeChampionVisualYaw(yaw);
    const f32_t resolvedYaw = MakeChampionVisualYawNear(normalizedYaw, previousYaw);
    m_pPlayerTransform->SetRotation({ rot.x, resolvedYaw, rot.z });

    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    Vec3 ecsRot = tf.GetRotation();
    ecsRot.y = resolvedYaw;
    tf.SetRotation(ecsRot);
}

void CScene_InGame::SyncPlayerEntityTransformFromECS()
{
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;

    if (m_bNetworkAuthoritativeGameplay &&
        m_bKalistaPassiveDashActive)
    {
        SyncPlayerEntityTransformToECS();
        return;
    }

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    m_PlayerEntityTransformCache.SetPosition(tf.GetPosition());
    m_PlayerEntityTransformCache.SetRotation(tf.GetRotation());
    m_PlayerEntityTransformCache.SetScale(tf.GetScale());
}

void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    if (m_PlayerEntity == NULL_ENTITY)
        return;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return;
    if (m_pPlayerTransform != &m_PlayerEntityTransformCache)
        return;

    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    tf.SetPosition(m_PlayerEntityTransformCache.GetPosition());
    tf.SetRotation(m_PlayerEntityTransformCache.GetRotation());
    tf.SetScale(m_PlayerEntityTransformCache.GetScale());
}

void CScene_InGame::SyncActorHUDStateToEngineUI()
{
    Engine::ActorHUDState State{};
    State.iActorContentId = ToLoLUIContentId(GetPlayerChampionId());
    State.SkillIconContentIds.fill(State.iActorContentId);

    if (m_PlayerEntity == NULL_ENTITY ||
        !m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        CGameInstance::Get()->UI_Set_ActorHUDState(&State);
        return;
    }

    const EntityID Entity = m_PlayerEntity;
    const ChampionComponent& Champion = m_World.GetComponent<ChampionComponent>(Entity);

    State.LocalEntity = Entity;
    State.iActorContentId = ToLoLUIContentId(Champion.id);
    State.SkillIconContentIds.fill(State.iActorContentId);
    State.Hp = Champion.hp;
    State.MaxHp = Champion.maxHp;
    State.Mp = Champion.mana;
    State.MaxMp = Champion.maxMana;
    State.Shield = Champion.shield;
    State.Level = Champion.level;
    State.PassiveValue = Champion.mana;
    State.PassiveMax = (Champion.maxMana > 0.f) ? Champion.maxMana : 100.f;
    State.PassiveShield = Champion.shield;
    State.PassiveShieldMax = State.PassiveMax;

    if (m_World.HasComponent<FormOverrideComponent>(Entity))
    {
        const FormOverrideComponent& Form = m_World.GetComponent<FormOverrideComponent>(Entity);
        const u8_t iFormContentId = ToLoLUIContentId(Form.skillChampion);
        if (Form.bActive && iFormContentId != 0u && iFormContentId != 255u)
        {
            for (u32_t Index = 0; Index < State.SkillIconContentIds.size(); ++Index)
            {
                const u8_t Slot = static_cast<u8_t>(Index + 1u);
                if ((Form.skillSlotMask & static_cast<u8_t>(1u << Slot)) != 0u)
                    State.SkillIconContentIds[Index] = iFormContentId;
            }
        }
    }

    if (m_World.HasComponent<SpellbookOverrideComponent>(Entity))
    {
        const SpellbookOverrideComponent& Spellbook =
            m_World.GetComponent<SpellbookOverrideComponent>(Entity);
        const u8_t iSpellbookContentId = ToLoLUIContentId(Spellbook.sourceChampion);
        if (Spellbook.bActive &&
            Spellbook.localSlot == kLoLUiSkillSlotR &&
            iSpellbookContentId != 0u &&
            iSpellbookContentId != 255u)
        {
            State.SkillIconContentIds[3] = iSpellbookContentId;
        }
    }

    for (u32_t Index = 0; Index < State.Cooldowns.size(); ++Index)
    {
        State.Cooldowns[Index] = Champion.cooldowns[Index];
        State.MaxCooldowns[Index] = 0.f;
    }

    if (m_World.HasComponent<SkillStateComponent>(Entity))
    {
        const SkillStateComponent& SkillState = m_World.GetComponent<SkillStateComponent>(Entity);
        for (u32_t Index = 0; Index < State.Cooldowns.size(); ++Index)
        {
            const SkillSlotRuntime& Slot = SkillState.slots[Index + 1u];
            State.Cooldowns[Index] = Slot.cooldownRemaining;
            State.MaxCooldowns[Index] = Slot.cooldownDuration;
            if (State.Cooldowns[Index] <= 0.f)
                State.MaxCooldowns[Index] = 0.f;
            else if (State.MaxCooldowns[Index] < State.Cooldowns[Index])
                State.MaxCooldowns[Index] = State.Cooldowns[Index];
        }
    }

    if (m_World.HasComponent<HealthComponent>(Entity))
    {
        const HealthComponent& Health = m_World.GetComponent<HealthComponent>(Entity);
        State.Hp = Health.fCurrent;
        State.MaxHp = Health.fMaximum;
    }

    if (m_World.HasComponent<ExperienceComponent>(Entity))
    {
        const ExperienceComponent& Experience = m_World.GetComponent<ExperienceComponent>(Entity);
        State.XpCurrent = Experience.current;
        State.XpRequired = Experience.requiredForNextLevel;
        State.XpRatio = (Experience.requiredForNextLevel > 0.f)
            ? (Experience.current / Experience.requiredForNextLevel)
            : 0.f;
        if (Experience.level > 0)
            State.Level = Experience.level;
    }

    if (m_World.HasComponent<SkillRankComponent>(Entity))
    {
        const SkillRankComponent& Ranks = m_World.GetComponent<SkillRankComponent>(Entity);
        const u32_t Count = std::min<u32_t>(
            static_cast<u32_t>(State.SkillRanks.size()),
            SkillRankComponent::kSlotCount);
        for (u32_t Index = 0; Index < Count; ++Index)
            State.SkillRanks[Index] = Ranks.ranks[Index];
        State.SkillPoints = Ranks.pointsAvailable;
    }

    if (m_World.HasComponent<RuneRuntimeComponent>(Entity))
    {
        State.LethalTempoStacks =
            m_World.GetComponent<RuneRuntimeComponent>(Entity).iLethalTempoStacks;
        State.LethalTempoMaxStacks = static_cast<u8_t>(RuneTuning::kLethalTempoMaxStacks);
    }

    if (m_World.HasComponent<GoldComponent>(Entity))
        State.Gold = m_World.GetComponent<GoldComponent>(Entity).amount;

    if (m_World.HasComponent<InventoryComponent>(Entity))
    {
        const InventoryComponent& Inventory = m_World.GetComponent<InventoryComponent>(Entity);
        State.InventoryItemIds.fill(0);
        const u32_t Count = std::min<u32_t>(
            static_cast<u32_t>(State.InventoryItemIds.size()),
            std::min<u32_t>(Inventory.count, InventoryComponent::kMaxSlots));
        for (u32_t Index = 0; Index < Count; ++Index)
            State.InventoryItemIds[Index] = Inventory.itemIds[Index];
    }

    if (m_World.HasComponent<StatComponent>(Entity))
    {
        const StatComponent& Stat = m_World.GetComponent<StatComponent>(Entity);
        State.Ad = Stat.ad;
        State.Ap = Stat.ap;
        State.Armor = Stat.armor;
        State.Mr = Stat.mr;
        State.AttackSpeed = Stat.attackSpeed;
        State.AttackRange = Stat.attackRange;
        State.MoveSpeed = Stat.moveSpeed;
        State.CritChance = Stat.critChance;
        State.AbilityHaste = Stat.abilityHaste;
        if (Stat.level > 0)
            State.Level = Stat.level;
        if (Stat.hpMax > 0.f)
            State.MaxHp = Stat.hpMax;
        if (Stat.manaMax > 0.f)
            State.MaxMp = Stat.manaMax;
    }

    State.bStunned = false;
    if (m_World.HasComponent<GameplayStateComponent>(Entity))
    {
        const auto& Gameplay =
            m_World.GetComponent<GameplayStateComponent>(Entity);
        constexpr u32_t kCrowdControlled =
            kGameplayStateStunnedFlag |
            kGameplayStateAirborneFlag |
            kGameplayStateCannotMoveFlag;
        State.bStunned = (Gameplay.stateFlags & kCrowdControlled) != 0u;
    }
    else
    {
        State.bStunned = m_World.HasComponent<StunComponent>(Entity);
    }
    CGameInstance::Get()->UI_Set_ActorHUDState(&State);
}

void CScene_InGame::SyncStatusPanelStateToEngineUI()
{
    Engine::StatusPanelMatchScore Score{};
    u16_t iBlueKills = 0u;
    u16_t iRedKills = 0u;
    bool_t bScoreFound = false;

    m_World.ForEach<MatchScoreComponent>(
        [&](EntityID, MatchScoreComponent& MatchScore)
        {
            if (bScoreFound)
                return;

            bScoreFound = true;
            Score.iBlueDragons = MatchScore.Blue.iDragons;
            Score.iBlueBarons = MatchScore.Blue.iBarons;
            Score.iBlueDestroyedStructures = MatchScore.Blue.iDestroyedTurrets;
            Score.iRedDragons = MatchScore.Red.iDragons;
            Score.iRedBarons = MatchScore.Red.iBarons;
            Score.iRedDestroyedStructures = MatchScore.Red.iDestroyedTurrets;
            iBlueKills = MatchScore.Blue.iTotalKills;
            iRedKills = MatchScore.Red.iTotalKills;
        });

    std::vector<Engine::StatusPanelActorRow> BlueRows;
    std::vector<Engine::StatusPanelActorRow> RedRows;

    m_World.ForEach<ChampionComponent>(
        [&](EntityID Entity, ChampionComponent& Champion)
        {
            Engine::StatusPanelActorRow Row{};
            Row.Entity = Entity;
            Row.iActorContentId = ToLoLUIContentId(Champion.id);
            Row.iTeam = static_cast<u8_t>(Champion.team);
            Row.iLevel = Champion.level;
            Row.SummonerSpellIds[0] = ChampionScoreComponent::kSummonerSpellFlash;
            Row.SummonerSpellIds[1] = ChampionScoreComponent::kSummonerSpellIgnite;

            if (m_World.HasComponent<ChampionScoreComponent>(Entity))
            {
                const ChampionScoreComponent& ScoreComponent =
                    m_World.GetComponent<ChampionScoreComponent>(Entity);
                Row.iKills = ScoreComponent.iKills;
                Row.iDeaths = ScoreComponent.iDeaths;
                Row.iAssists = ScoreComponent.iAssists;
                for (u32_t Index = 0; Index < Row.SummonerSpellIds.size(); ++Index)
                    Row.SummonerSpellIds[Index] = ScoreComponent.iSummonerSpellIds[Index];
            }

            if (m_World.HasComponent<SummonerSpellStateComponent>(Entity))
            {
                const SummonerSpellStateComponent& SpellState =
                    m_World.GetComponent<SummonerSpellStateComponent>(Entity);
                for (u32_t Index = 0; Index < Row.SummonerCooldowns.size(); ++Index)
                {
                    Row.SummonerCooldowns[Index] = SpellState.cooldownRemaining[Index];
                    Row.SummonerCooldownDurations[Index] = SpellState.cooldownDuration[Index];
                }
            }

            if (m_World.HasComponent<InventoryComponent>(Entity))
            {
                const InventoryComponent& Inventory = m_World.GetComponent<InventoryComponent>(Entity);
                const u32_t Count = std::min<u32_t>(
                    static_cast<u32_t>(Row.InventoryItemIds.size()),
                    std::min<u32_t>(Inventory.count, InventoryComponent::kMaxSlots));
                for (u32_t Index = 0; Index < Count; ++Index)
                    Row.InventoryItemIds[Index] = Inventory.itemIds[Index];
            }

            if (Champion.team == eTeam::Red)
                RedRows.push_back(Row);
            else if (Champion.team == eTeam::Blue)
                BlueRows.push_back(Row);
        });

    auto SortRows = [](std::vector<Engine::StatusPanelActorRow>& Rows)
    {
        std::sort(
            Rows.begin(),
            Rows.end(),
            [](const Engine::StatusPanelActorRow& A, const Engine::StatusPanelActorRow& B)
            {
                if (A.iTeam != B.iTeam)
                    return A.iTeam < B.iTeam;
                return A.Entity < B.Entity;
            });
    };

    SortRows(BlueRows);
    SortRows(RedRows);

    CGameInstance::Get()->UI_Set_StatusPanelState(
        &Score,
        BlueRows.empty() ? nullptr : BlueRows.data(),
        static_cast<u32_t>(BlueRows.size()),
        RedRows.empty() ? nullptr : RedRows.data(),
        static_cast<u32_t>(RedRows.size()));

    bool_t bLocalScoreFound = false;
    u16_t iLocalKills = 0u;
    u16_t iLocalDeaths = 0u;
    u16_t iLocalAssists = 0u;
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ChampionScoreComponent>(m_PlayerEntity))
    {
        const ChampionScoreComponent& LocalScore =
            m_World.GetComponent<ChampionScoreComponent>(m_PlayerEntity);
        iLocalKills = LocalScore.iKills;
        iLocalDeaths = LocalScore.iDeaths;
        iLocalAssists = LocalScore.iAssists;
        bLocalScoreFound = true;
    }

    if (bScoreFound || bLocalScoreFound)
    {
        CGameInstance::Get()->UI_Set_MatchContextHUDScoreStats(
            iBlueKills,
            iRedKills,
            iLocalKills,
            iLocalDeaths,
            iLocalAssists);
    }
}

void CScene_InGame::SyncWorldHealthBarsToEngineUI()
{
    std::vector<Engine::UIWorldHealthBarDesc> Bars;
    Bars.reserve(128u);

    auto ApplyHealthOverride = [this](
        EntityID Entity,
        f32_t& fCurrent,
        f32_t& fMaximum,
        bool_t& bDead)
    {
        if (!m_World.HasComponent<HealthComponent>(Entity))
            return;

        const HealthComponent& Health = m_World.GetComponent<HealthComponent>(Entity);
        fCurrent = Health.fCurrent;
        if (Health.fMaximum > 0.f)
            fMaximum = Health.fMaximum;
        bDead = Health.bIsDead || fCurrent <= 0.f;
    };

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID Entity, ChampionComponent& Champion, TransformComponent& Transform)
        {
            if (UI::IsKalistaCarried(m_World, Entity))
                return;

            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Character;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Champion.hp;
            Bar.fMaximum = Champion.maxHp;
            Bar.fShield = (std::max)(0.f, Champion.shield);
            Bar.fManaCurrent = Champion.mana;
            Bar.fManaMaximum = Champion.maxMana;
            Bar.iTeam = ToLoLUITeamId(Champion.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });

    m_World.ForEach<MinionComponent, TransformComponent>(
        [&](EntityID Entity, MinionComponent& Minion, TransformComponent& Transform)
        {
            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Unit;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Minion.hp;
            Bar.fMaximum = Minion.maxHp;
            Bar.iTeam = ToLoLUITeamId(Minion.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });

    m_World.ForEach<TurretComponent, TransformComponent>(
        [&](EntityID Entity, TurretComponent& Turret, TransformComponent& Transform)
        {
            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Structure;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Turret.hp;
            Bar.fMaximum = Turret.maxHp;
            Bar.iTeam = ToLoLUITeamId(Turret.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });

    // 넥서스/억제기: TurretComponent가 없어 위 포탑 루프에 안 걸린다 — StructureComponent로 수집.
    // 클라 포탑은 두 컴포넌트를 동시 보유하므로 이중 desc 방지를 위해 포탑은 건너뛴다 (S035).
    m_World.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID Entity, StructureComponent& Structure, TransformComponent& Transform)
        {
            if (m_World.HasComponent<TurretComponent>(Entity))
                return;

            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Structure;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Structure.hp;
            Bar.fMaximum = Structure.maxHp;
            Bar.iTeam = ToLoLUITeamId(Structure.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });

    // 정글몹: 에픽 4종(바론/드래곤/블루/레드)은 챔피언형 바(중립=적색 필),
    // 소형 캠프는 미니언과 같은 Unit 기본 폭을 쓴다.
    m_World.ForEach<JungleComponent, TransformComponent>(
        [&](EntityID Entity, JungleComponent& Jungle, TransformComponent& Transform)
        {
            const bool_t bEpic = Jungle.subKind <=
                static_cast<u32_t>(CJungle_Manager::eJungleSub::RedBuff);

            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = bEpic
                ? Engine::UIWorldHealthBarKind::Character
                : Engine::UIWorldHealthBarKind::Unit;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Jungle.hp;
            Bar.fMaximum = Jungle.maxHp;
            Bar.iTeam = ToLoLUITeamId(eTeam::Neutral);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });

    u8_t iLocalTeam = ToLoLUITeamId(m_PlayerTeam);
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        iLocalTeam = ToLoLUITeamId(m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
    }

    CGameInstance::Get()->UI_Set_WorldHealthBars(
        Bars.empty() ? nullptr : Bars.data(),
        static_cast<u32_t>(Bars.size()),
        iLocalTeam);
}

void CScene_InGame::SyncAIResourceStateToEngine()
{
    m_World.ForEach<ChampionComponent>(
        [&](EntityID Entity, ChampionComponent& Champion)
        {
            AIResourceStateComponent& Resource =
                m_World.HasComponent<AIResourceStateComponent>(Entity)
                    ? m_World.GetComponent<AIResourceStateComponent>(Entity)
                    : m_World.AddComponent<AIResourceStateComponent>(Entity);

            Resource.fMana = Champion.mana;
            Resource.fMaxMana = Champion.maxMana;
            for (u32_t i = 0; i < 4; ++i)
                Resource.fCooldowns[i] = Champion.cooldowns[i];
        });
}

void CScene_InGame::SyncNavigationControlStateToEngine()
{
    m_World.ForEach<NavAgentComponent>(
        [&](EntityID Entity, NavAgentComponent&)
        {
            NavigationControlComponent& Control =
                m_World.HasComponent<NavigationControlComponent>(Entity)
                    ? m_World.GetComponent<NavigationControlComponent>(Entity)
                    : m_World.AddComponent<NavigationControlComponent>(Entity);

            Control = NavigationControlComponent{};

            const bool_t bHasMinionState =
                m_World.HasComponent<MinionStateComponent>(Entity);
            Control.bUseReverseFacing =
                bHasMinionState || m_World.HasComponent<MinionComponent>(Entity);

            if (bHasMinionState)
            {
                const MinionStateComponent& State =
                    m_World.GetComponent<MinionStateComponent>(Entity);
                const bool_t bChasing =
                    State.current == MinionStateComponent::Chase;
                Control.bChaseFallbackEnabled = bChasing;
                Control.bThrottleRepath = bChasing;
            }

            if (m_World.HasComponent<GameplayStateComponent>(Entity))
            {
                const auto& Gameplay =
                    m_World.GetComponent<GameplayStateComponent>(Entity);
                Control.bMovementBlocked =
                    (Gameplay.stateFlags & kGameplayStateCannotMoveFlag) != 0u;
                Control.fMoveSpeedMul = Gameplay.fMoveSpeedMul;
            }
            else
            {
                Control.bMovementBlocked =
                    m_World.HasComponent<StunComponent>(Entity);
                if (m_World.HasComponent<SlowComponent>(Entity))
                {
                    Control.fMoveSpeedMul =
                        m_World.GetComponent<SlowComponent>(Entity).fMoveSpeedMul;
                }
            }
        });
}

Vec3 CScene_InGame::GetPlayerForward() const
{
    const f32_t yaw =
        GetPlayerYaw() -
        ClientData::ResolveChampionModelYawOffset(GetPlayerChampionId());
    return { sinf(yaw), 0.f, cosf(yaw) };
}

void CScene_InGame::OnUpdate(f32_t dt)
{
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");

    PollGameEndAndSettings();
    if (m_bReturnToMainMenuRequested)
    {
        m_bReturnToMainMenuRequested = false;
        ChangeToMainMenuScene();
        return;
    }
    if (m_bExitReplayToMyInfoRequested)
    {
        m_bExitReplayToMyInfoRequested = false;
        ChangeToMyInfoScene();
        return;
    }

    CGameInstance::Get()->UI_Set_StatusPanelOpen(CInput::Get().IsKeyDown(VK_TAB));

    if (m_bNetworkAuthoritativeGameplay && m_bNetworkActorInterpolationEnabled)
        CaptureNetworkActorInterpolationStarts();

    const bool_t bNetworkActive = m_bReplayPlaybackMode
        ? false
        : PumpNetwork();

    if (m_bReplayPlaybackMode)
        UpdateReplayPlayback(dt);

    const u64_t appliedSnapshotTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    if (appliedSnapshotTick != 0ull)
        WINTERS_PROFILE_GAUGE("Net::LatestServerTick", appliedSnapshotTick);
    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        appliedSnapshotTick != 0 &&
        appliedSnapshotTick != m_uNetworkActorInterpSnapshotTick)
    {
        BeginNetworkActorInterpolationForSnapshot(appliedSnapshotTick);
        m_uNetworkActorInterpSnapshotTick = appliedSnapshotTick;
    }

    {
        WINTERS_PROFILE_SCOPE("SyncECS");
        SyncPlayerEntityTransformFromECS();
    }
    SyncAIResourceStateToEngine();
    SyncNavigationControlStateToEngine();
    if (m_pVisionSystem)
    {
        u8_t iFowLocalTeam = ToLoLUITeamId(m_PlayerTeam);
        if (m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
        {
            iFowLocalTeam = ToLoLUITeamId(
                m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team);
        }

        if (iFowLocalTeam == 255u)
            m_pVisionSystem->ClearFowLocalTeam();
        else
            m_pVisionSystem->SetFowLocalTeam(iFowLocalTeam);
    }
    {
        WINTERS_PROFILE_SCOPE("Scheduler");

        if (m_pScheduler)
            m_pScheduler->Execute(m_World, dt);
    }

    if (m_pVisionSystem && m_pFogOfWarRenderer && m_pVisionSystem->IsFowTextureDirty())
    {
        m_pFogOfWarRenderer->UpdateTexture(
            m_pVisionSystem->GetFowTextureData(),
            m_pVisionSystem->GetFowTextureDim());
        m_pVisionSystem->ClearFowTextureDirty();
    }


    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        m_bNetworkActorInterpolationEnabled)
    {
        ApplyNetworkActorInterpolation(dt);
    }

    SyncPlayerEntityTransformFromECS();

    if (bNetworkActive || m_bReplayPlaybackMode)
        UpdateNetworkChampionLocomotion(dt);

    m_MapTransform.SetRotation(m_vMapRotation);

    if (!m_bNetworkAuthoritativeGameplay
        && m_SylasEntity != NULL_ENTITY
        && m_World.HasComponent<TransformComponent>(m_SylasEntity))
    {
        m_World.GetComponent<TransformComponent>(m_SylasEntity).SetPosition(m_vSylasTestPos);
    }

    ProjectGameplayActorsToMapSurface();

    const bool_t bPlayerDead = IsPlayerDead();
    if (bPlayerDead)
        ApplyPlayerDeathInputLock();

    bool bSkipGroundMove = false;
    if (!m_bReplayPlaybackMode && !bPlayerDead)
    {
        UpdateTargeting();
        UpdateCombatInput(bSkipGroundMove);
    }

    if (!bPlayerDead && ShouldRunInGameSkillSmoke())
    {
        static bool_t s_bSmokeSkillAttempted = false;
        static bool_t s_bSmokeSkillCastObserved = false;
        static bool_t s_bSmokeSkillArmedLogged = false;
        static f32_t s_fSmokeSkillTimer = 0.f;
        static f32_t s_fSmokeSkillWaitLogTimer = 0.f;

        s_fSmokeSkillTimer += dt;
        s_fSmokeSkillWaitLogTimer += dt;

        if (!s_bSmokeSkillArmedLogged)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] armed entity=%u renderer=%u\n",
                static_cast<u32_t>(m_PlayerEntity),
                m_pPlayerRenderer ? 1u : 0u);
            s_bSmokeSkillArmedLogged = true;
        }

        if (!s_bSmokeSkillAttempted
            && s_fSmokeSkillTimer >= 1.0f
            && m_PlayerEntity != NULL_ENTITY
            && m_pPlayerRenderer != nullptr)
        {
            const eChampion champ = GetPlayerChampionId();
            const bool_t bHasChampion = m_World.HasComponent<ChampionComponent>(m_PlayerEntity);
            const bool_t bHasSkillState = m_World.HasComponent<SkillStateComponent>(m_PlayerEntity);
            const bool_t bDispatched = DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::Q));
            Winters::DevSmoke::Log(
                "[SmokeSkill] Q dispatch champ=%u entity=%u hasChampion=%u hasSkillState=%u ok=%u\n",
                static_cast<u32_t>(champ),
                static_cast<u32_t>(m_PlayerEntity),
                bHasChampion ? 1u : 0u,
                bHasSkillState ? 1u : 0u,
                bDispatched ? 1u : 0u);
            s_bSmokeSkillAttempted = true;
        }
        else if (!s_bSmokeSkillAttempted
            && s_fSmokeSkillTimer >= 1.0f
            && s_fSmokeSkillWaitLogTimer >= 2.0f)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] waiting entity=%u renderer=%u elapsed=%.2f\n",
                static_cast<u32_t>(m_PlayerEntity),
                m_pPlayerRenderer ? 1u : 0u,
                s_fSmokeSkillTimer);
            s_fSmokeSkillWaitLogTimer = 0.f;
        }

        if (s_bSmokeSkillAttempted
            && !s_bSmokeSkillCastObserved
            && m_ActiveSkill.bCastFrameFired
            && m_ActiveSkill.bActive)
        {
            Winters::DevSmoke::Log(
                "[SmokeSkill] castFrame observed champ=%u slot=%u hook=0x%08X\n",
                static_cast<u32_t>(GetPlayerChampionId()),
                static_cast<u32_t>(m_ActiveSkill.slot),
                m_ActiveSkill.legacyHookBridge.castHookId);
            s_bSmokeSkillCastObserved = true;
        }
    }

    const bool bActionLockedBefore = (m_fLastActionTimer > 0.f);
    if (m_fLastActionTimer > 0.f) m_fLastActionTimer -= dt;

    if (m_fEndTransitionTimer > 0.f)
    {
        m_fEndTransitionTimer -= dt;
        if (m_fEndTransitionTimer <= 0.f)
        {
            if (m_pPlayerRenderer && !m_bNetworkAuthoritativeGameplay)
            {
                if (CanResumeBaseAnimation())
                {
                    m_pPlayerRenderer->PlayAnimationByName(m_bMoving ?
                        m_pPlayerRunAnim : m_pPlayerIdleAnim);
                }

            }
            m_pPendingEndAnim = nullptr;
            m_fEndTransitionTimer = 0.f;
        }
    }

    if (!bPlayerDead)
        UpdateDash(dt);

    if (bPlayerDead && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (m_bNetworkAuthoritativeGameplay && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (!bPlayerDead && m_ActiveSkill.bActive && m_pPlayerRenderer)
    {
        const Engine::CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
        if (pAnim)
        {
            const f32_t curF = pAnim->GetCurrentFrame();
            const SkillDef& d = m_ActiveSkill.legacyHookBridge;
            const CastSkillCommand& activeCommand = m_ActiveSkill.command;

            const bool bCastHit =
                !m_ActiveSkill.bCastFrameFired
                && d.visualCastFrame > 0.f
                && pAnim->HasFramePassed(d.visualCastFrame, m_ActiveSkill.prevFrame);
            const bool bRecoveryHit =
                !m_ActiveSkill.bRecoveryFrameFired
                && d.visualRecoveryFrame > 0.f
                && pAnim->HasFramePassed(d.visualRecoveryFrame, m_ActiveSkill.prevFrame);

            if (m_bLogFrameEvents)
            {
                char buf[128];
                if (bCastHit)
                {
                    sprintf_s(buf, "[FrameEvent] CAST slot=%u anim=%s frame=%.1f\n",
                        d.slot, d.animKey ? d.animKey : "?", curF);
                    Winters::DevSmoke::Log("%s", buf);
                }
                if (bRecoveryHit)
                {
                    sprintf_s(buf, "[FrameEvent] RECOVERY slot=%u frame=%.1f\n", d.slot, curF);
                    Winters::DevSmoke::Log("%s", buf);
                }
            }

            if (bCastHit)
            {
                m_ActiveSkill.bCastFrameFired = true;

                // Local/offline path only. Network-authoritative gameplay is handled by server commands.
                const eChampion champ = GetPlayerChampionId();
                GameCommand gameCommand{};
                gameCommand.kind = (d.slot == static_cast<uint8_t>(eSkillSlot::BasicAttack))
                    ? eCommandKind::BasicAttack
                    : eCommandKind::CastSkill;
                gameCommand.issuerEntity = m_PlayerEntity;
                gameCommand.slot = d.slot;
                gameCommand.targetEntity = activeCommand.targetEntityId;
                gameCommand.groundPos = activeCommand.groundPos;
                gameCommand.direction = activeCommand.direction;
                TickContext tickCtx{};
                tickCtx.fDt = dt;
                tickCtx.localPlayer = m_PlayerEntity;

                GameplayHookContext gameCtx{};
                gameCtx.pWorld = &m_World;
                gameCtx.casterEntity = m_PlayerEntity;
                gameCtx.casterTeam = m_PlayerTeam;
                gameCtx.casterChampion = champ;
                gameCtx.skillRank = 1;
                if (m_World.HasComponent<SkillRankComponent>(m_PlayerEntity) &&
                    d.slot < SkillRankComponent::kSlotCount)
                {
                    const u8_t rank = m_World.GetComponent<SkillRankComponent>(m_PlayerEntity).ranks[d.slot];
                    gameCtx.skillRank = (rank == 0) ? 1 : rank;
                }
                gameCtx.pDef = &m_ActiveSkill.legacyHookBridge;
                gameCtx.pCommand = &gameCommand;
                gameCtx.pTickCtx = &tickCtx;
                bool gameplayHandled = false;
                if (d.castHookId != 0)
                {
                    gameplayHandled = CGameplayHookRegistry::Instance().Dispatch(
                        d.castHookId, gameCtx
                    );
                }
                //Client Visual FX/Sound
                VisualHookContext visualCtx{};
                visualCtx.pWorld = &m_World;
                visualCtx.casterEntity = m_PlayerEntity;
                visualCtx.pDef = &m_ActiveSkill.legacyHookBridge;
                visualCtx.pCommand = &activeCommand;
                visualCtx.skillStage = m_ActiveSkill.stage;
                visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                bool visualHandled = false;
                const bool suppressRivenVisualForLegacy =
                    champ == eChampion::RIVEN &&
                    d.castHookId != 0 &&
                    CSkillHookRegistry::Instance().Has(d.castHookId);
                if (d.castHookId != 0 && !suppressRivenVisualForLegacy)
                    visualHandled = CVisualHookRegistry::Instance().Dispatch(
                        d.castHookId, visualCtx);

                // Legacy local skill hook path for offline/practice visuals.
                bool castHandled = false;
                if (d.castHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = &m_ActiveSkill.legacyHookBridge;
                    ctx.pCommand = &activeCommand;
                    ctx.skillStage = m_ActiveSkill.stage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.applyTargetDamage = [this](EntityID target, f32_t damage)
                        {
                            ApplyLocalChampionDamage(
                                target,
                                damage,
                                "SkillHookDamage");
                        };
                    ctx.setLocalLoopAnimations = [this](const char* idle, const char* run, bool_t playNow)
                        {
                            m_pPlayerIdleAnim = idle;
                            m_pPlayerRunAnim = run;
                            if (playNow && m_pPlayerRenderer)
                                m_pPlayerRenderer->PlayAnimationByName(idle, true);
                        };
                    castHandled = CSkillHookRegistry::Instance().Dispatch(
                        d.castHookId, ctx);
                }
                Winters::DevSmoke::Log(
                    "[SmokeSkill] castFrame champ=%u slot=%u hook=0x%08X gameplay=%u visual=%u legacy=%u\n",
                    static_cast<u32_t>(champ),
                    static_cast<u32_t>(d.slot),
                    d.castHookId,
                    gameplayHandled ? 1u : 0u,
                    visualHandled ? 1u : 0u,
                    castHandled ? 1u : 0u);


            }
            if (bRecoveryHit)
            {
                m_ActiveSkill.bRecoveryFrameFired = true;

                //DispatchHook Recovery
                bool recoveryHandled = false;
                if (d.recoveryHookId != 0)
                {
                    SkillHookContext ctx{};
                    ctx.pWorld = &m_World;
                    ctx.casterEntity = m_PlayerEntity;
                    ctx.casterTeam = m_PlayerTeam;
                    ctx.pDef = &m_ActiveSkill.legacyHookBridge;
                    ctx.pCommand = &activeCommand;
                    ctx.skillStage = m_ActiveSkill.stage;
                    ctx.pFxMeshRenderer = m_pFxMeshRenderer.get();
                    ctx.pCasterRenderer = m_pPlayerRenderer;
                    ctx.fGlobalAnimSpeed = m_fGlobalAnimSpeed;
                    ctx.startLocalDash = [this](const Vec3& dir)
                        {
                            StartLocalPassiveDash(dir);
                        };
                    ctx.setLocalDashDuration = [this](f32_t duration)
                        {
                            SetLocalPassiveDashDuration(duration);
                        };
                    ctx.getLocalDashDuration = [this]() -> f32_t
                        {
                            return GetLocalPassiveDashDuration();
                        };
                    ctx.setLocalActionAnimActive = [this](bool_t active)
                        {
                            SetLocalActionAnimActive(active);
                        };
                    recoveryHandled = CSkillHookRegistry::Instance().Dispatch(
                        d.recoveryHookId, ctx
                    );
                }
                (void)recoveryHandled;


            }

            const bool_t bReachedRecoveryFrame =
                d.visualRecoveryFrame > 0.f && curF >= d.visualRecoveryFrame;
            const bool_t bAnimationFinished = !pAnim->IsPlaying();
            if (bReachedRecoveryFrame || bAnimationFinished)
            {
#if defined(_DEBUG)
                if (bAnimationFinished && !bReachedRecoveryFrame)
                {
                    char trace[192]{};
                    sprintf_s(
                        trace,
                        "[SkillRuntime] animation ended before recovery champ=%u slot=%u frame=%.2f recovery=%.2f\n",
                        static_cast<u32_t>(m_ActiveSkill.champion),
                        static_cast<u32_t>(m_ActiveSkill.slot),
                        curF,
                        d.visualRecoveryFrame);
                    OutputDebugStringA(trace);
                }
#endif
                ClearActiveSkillRuntime();
            }
            else
                m_ActiveSkill.prevFrame = curF;
        }
    }

    ZedFx::TickShadowCloneModels(m_World, dt);
    if (!bPlayerDead)
        UpdateLocalChampionRuntime(dt);
    UpdateFlashCooldown(dt);

    UpdateChampionStateTimers(dt);

    // [B-6.6] player cooldown / stage window
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
    {
        auto& ss = m_World.GetComponent<SkillStateComponent>(m_PlayerEntity);
        for (int i = 0; i < 5; ++i)
        {
            if (ss.slots[i].cooldownRemaining > 0.f)
            {
                ss.slots[i].cooldownRemaining -= dt;
                if (ss.slots[i].cooldownRemaining <= 0.f)
                {
                    ss.slots[i].cooldownRemaining = 0.f;
                    ss.slots[i].cooldownDuration = 0.f;
                }
            }
            else
            {
                ss.slots[i].cooldownDuration = 0.f;
            }

            if (ss.slots[i].currentStage == 1 && ss.slots[i].stageWindow > 0.f)
            {
                ss.slots[i].stageWindow -= dt;
                if (ss.slots[i].stageWindow <= 0.f)
                    ss.slots[i].currentStage = 0;
            }
        }
    }

    if (m_pCamera)
        m_pCamera->Update(dt, CInput::Get());

    if (!m_bReplayPlaybackMode && !bPlayerDead)
    {
        UpdatePlayerControl(dt, bNetworkActive, bSkipGroundMove, bActionLockedBefore);
    }


    //ECS owned ModelRenderer
    {
        WINTERS_PROFILE_SCOPE("Champion::AnimUpdate");
        m_World.ForEach<ChampionComponent, RenderComponent>(
            [dt](EntityID, ChampionComponent&, RenderComponent& rc)
            {
                if (rc.bSceneManaged) return;
                if (!rc.pRenderer || !rc.bAnimated) return;
                if (!rc.pRenderer->HasSkeleton()) return;
                rc.pRenderer->Update(dt);
            }
        );
    }

    CAmbientProp_Manager::Get()->Tick(dt);

    CJungle_Manager::Get()->Update(dt);

    if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
    if (m_pFxBeamSystem)      m_pFxBeamSystem->Update(m_World, dt);
    if (m_pFxMeshSystem)      m_pFxMeshSystem->Update(m_World, dt);

    if (!bPlayerDead)
        UpdateLocalPostAnimation();

    {
        if (m_bNetworkAuthoritativeGameplay)
        {
            const Mat4 minionVisualVP = m_pCamera ? m_pCamera->GetViewProjection() : Mat4();
            CMinion_Manager::Get()->TickVisuals(dt, m_pCamera ? &minionVisualVP : nullptr);
        }
        else
        {
            CMinion_Manager::Get()->Tick(dt);
        }
        ProjectGameplayActorsToMapSurface();
    }

    const bool_t bAttackHold = !bPlayerDead && CInput::Get().IsKeyDown('A');
    SetShowAttackRange(bAttackHold);

    SyncActorHUDStateToEngineUI();
    SyncStatusPanelStateToEngineUI();
    SyncWorldHealthBarsToEngineUI();

    const EntityID hoveredEntity = GetHoveredEntity();
    CGameInstance::Get()->UI_Set_AttackMode(bAttackHold);
    CGameInstance::Get()->UI_Set_EnemyHoverCursor(
        !bPlayerDead &&
        hoveredEntity != NULL_ENTITY &&
        IsEnemyOfPlayer(hoveredEntity));
}

void CScene_InGame::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_InGame::PollGameEndAndSettings()
{
    if (m_pEventApplier && !m_bGameEndActive)
    {
        u8_t winningTeam = 0u;
        if (m_pEventApplier->ConsumeGameEndEvent(winningTeam))
        {
            m_bGameEndActive = true;
            m_bLocalVictory = static_cast<u8_t>(m_PlayerTeam) == winningTeam;
            // 정상 종료(넥서스 파괴) 저장 — 서버는 리플레이를 발행하고,
            // 클라는 로컬 전적 + AI trace JSONL을 저장한다 (S030 저장 보증 1/3).
            SaveEndOfMatchArtifacts(m_bLocalVictory ? "victory" : "defeat");
        }
    }

    // S031 잔여: 인게임 기어 설정창(UI_ConsumeMainMenuRequest)은 Engine UI 배선 후 연결.
}

void CScene_InGame::SaveEndOfMatchArtifacts(const char* pResultLabel)
{
    // 리플레이 재생은 관전이므로 전적/trace를 남기지 않는다.
    if (m_bEndOfMatchArtifactsSaved || m_bReplayPlaybackMode)
        return;
    m_bEndOfMatchArtifactsSaved = true;

    Winters::LocalMatchRecord record{};
    record.strUser = CClientShellSession::Instance().GetDisplayName();
    record.strResult = pResultLabel ? pResultLabel : "unknown";
    record.uEndTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    Winters::AppendLocalMatchRecord(record);

    // 온라인 계정이면 매치결과를 백엔드에 보고 (MMR/RP 반영, S035).
    // 비회원/백엔드 미실행이면 서비스 내부 게이트가 조용히 스킵 — 게임 흐름 비차단.
    // "aborted"(강제 이탈)는 보고하지 않는다.
    if (record.strResult == "victory" || record.strResult == "defeat")
    {
        CClientShellBackendService::Instance().RequestReportMatchResult(
            record.strResult == "victory");
    }

    std::string strTracePath;
    Winters::ExportAiDecisionTraceJsonl(m_World, strTracePath);
}

void CScene_InGame::ChangeToMainMenuScene()
{
    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().Disconnect();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MainMenu),
        CScene_MainMenu::Create());
}

void CScene_InGame::ChangeToMyInfoScene()
{
    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MyInfo),
        CScene_MyInfo::Create());
}
