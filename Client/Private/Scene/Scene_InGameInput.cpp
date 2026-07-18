// Scene_InGameInput.cpp ??CScene_InGame???�력/?�게팅/?�투 ?�도/???�망 ?�력??책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp?�서 verbatim ?�동. ?�작/?�그?�처/?�출?�서 불�?.
// ?�계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
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
#include "Map/MapDataIO.h"
#include "Core/CInput.h"
#include "WintersPaths.h"
#include "GameInstance.h"
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/NavigationThrottleSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/Pathfinder.h"

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

#include "GameObject/ChampionDef.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
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
#include "Shared/GameSim/Definitions/WardDefinitions.h"
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
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <vector>

namespace
{
    bool_t s_bWReleasePending = false;
    EntityID s_NetworkAttackTarget = NULL_ENTITY;
    u32_t s_uNetworkAttackCommandFrame = 0u;
    u32_t s_uNetworkAttackMissLogCount = 0u;
    constexpr u32_t kNetworkAttackCommandIntervalFrames = 6u;

    void TraceWGate(
        const char* pEvent,
        eChampion champion,
        bool_t bDown,
        bool_t bPressed,
        bool_t bReleased,
        bool_t bReleasePending,
        bool_t bStagePending,
        bool_t bImGuiKeyboard,
        bool_t bDispatched)
    {
#if defined(_DEBUG)
        static u32_t s_uTraceCount = 0u;
        if (s_uTraceCount >= 96u)
            return;

        char message[256]{};
        sprintf_s(
            message,
            "[WGate][ClientInput] event=%s champ=%u down=%u press=%u edgeRelease=%u releasePending=%u stagePending=%u imgui=%u dispatched=%u\n",
            pEvent ? pEvent : "-",
            static_cast<u32_t>(champion),
            bDown ? 1u : 0u,
            bPressed ? 1u : 0u,
            bReleased ? 1u : 0u,
            bReleasePending ? 1u : 0u,
            bStagePending ? 1u : 0u,
            bImGuiKeyboard ? 1u : 0u,
            bDispatched ? 1u : 0u);
        OutputDebugStringA(message);
        ++s_uTraceCount;
#else
        (void)pEvent;
        (void)champion;
        (void)bDown;
        (void)bPressed;
        (void)bReleased;
        (void)bReleasePending;
        (void)bStagePending;
        (void)bImGuiKeyboard;
        (void)bDispatched;
#endif
    }

    u8_t ResolvePingWheelDirectionCode(f32_t fCenterX, f32_t fCenterY,
        f32_t fMouseX, f32_t fMouseY)
    {
        const f32_t dx = fMouseX - fCenterX;
        const f32_t dy = fMouseY - fCenterY;
        constexpr f32_t kDeadZone = 18.f;
        if (dx * dx + dy * dy < kDeadZone * kDeadZone)
            return 0u;

        const f32_t ax = dx < 0.f ? -dx : dx;
        const f32_t ay = dy < 0.f ? -dy : dy;
        if (ax >= ay)
            return dx >= 0.f ? 1u : 4u;

        return dy < 0.f ? 2u : 3u;
    }

    void ClearNetworkAttackIntent()
    {
        s_NetworkAttackTarget = NULL_ENTITY;
        s_uNetworkAttackCommandFrame = 0u;
    }

    bool_t HasPendingSkillStage(CScene_InGame& scene, u8_t slot)
    {
        CWorld& world = scene.GetWorld();
        const EntityID player = scene.GetPlayerEntity();
        if (player == NULL_ENTITY ||
            !world.HasComponent<SkillStateComponent>(player) ||
            slot >= 5u)
        {
            return false;
        }

        const auto& skillSlot =
            world.GetComponent<SkillStateComponent>(player).slots[slot];
        return skillSlot.currentStage == 1 && skillSlot.stageWindow > 0.f;
    }
}

void CScene_InGame::SetEntityHoverOutline(EntityID entity, bool_t bEnabled)
{
    if (entity == NULL_ENTITY ||
        !m_World.IsAlive(entity) ||
        !m_World.HasComponent<RenderComponent>(entity))
    {
        return;
    }

    RenderComponent& render = m_World.GetComponent<RenderComponent>(entity);
    if (!render.pRenderer)
        return;

    if (bEnabled)
        render.pRenderer->SetHoverOutline(Vec4{ 1.f, 0.04f, 0.02f, 1.f }, 1.15f);
    else
        render.pRenderer->ClearHoverOutline();
}

void CScene_InGame::SetHoveredTarget(EntityID entity, eTeam team)
{
    const bool_t bShouldOutline =
        entity != NULL_ENTITY &&
        (team == eTeam::Neutral ||
            (team != eTeam::TEAM_END && team != m_PlayerTeam));

    if (m_OutlinedHoverEntity != NULL_ENTITY &&
        (m_OutlinedHoverEntity != entity || !bShouldOutline))
    {
        SetEntityHoverOutline(m_OutlinedHoverEntity, false);
        m_OutlinedHoverEntity = NULL_ENTITY;
    }

    m_HoveredEntity = entity;
    m_HoveredTeam = team;

    if (bShouldOutline)
    {
        SetEntityHoverOutline(entity, true);
        m_OutlinedHoverEntity = entity;
    }
}

void CScene_InGame::UpdateTargeting()
{
    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);

    const CDynamicCamera* pCamera = GetCameraPtr();
    if (!pCamera)
        return;
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    const auto ray = CInput::Get().GetMouseWorldRay(
        *pCamera, static_cast<i32_t>(g_iWinSizeX),
        static_cast<i32_t>(g_iWinSizeY));

    EntityID hoveredEntity = NULL_ENTITY;
    eTeam hoveredTeam = eTeam::TEAM_END;

    GameplayQuery::TryFindHoverTarget(
        GetWorld(),
        GetPlayerEntity(),
        GetPlayerTeam(),
        ray.Origin,
        ray.Dir,
        GetChampionHitRadius(),
        GetChampionHitHeight(),
        hoveredEntity,
        hoveredTeam);
    SetHoveredTarget(hoveredEntity, hoveredTeam);
}

bool_t CScene_InGame::TryResolveMinimapClickTarget(Vec3& vOutWorldPos) const
{
    const CInput& input = CInput::Get();
    UI::MinimapFrameState MinimapInputState{};
    const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
    MinimapInputState.Projection = UI::GetDefaultMinimapProjection();
    MinimapInputState.fScreenWidth =
        DisplaySize.x > 0.f ? DisplaySize.x : kFallbackScreenWidth;
    MinimapInputState.fScreenHeight =
        DisplaySize.y > 0.f ? DisplaySize.y : kFallbackScreenHeight;

    return UI::TryResolveMinimapClickWorldPos(
        MinimapInputState,
        static_cast<f32_t>(input.GetMouseX()),
        static_cast<f32_t>(input.GetMouseY()),
        vOutWorldPos);
}

bool_t CScene_InGame::UpdatePingWheelInput(bool_t bImGuiMouse)
{
    auto& in = CInput::Get();
    const bool_t bCtrlDown = in.IsKeyDown(VK_CONTROL);
    if (bImGuiMouse || !bCtrlDown || IsPlayerDead())
    {
        const bool_t bWasActive = m_bPingWheelActive;
        if (m_bPingWheelActive)
        {
            m_bPingWheelActive = false;
            CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
        }
        return bWasActive;
    }

    if (in.IsRButtonPressed())
    {
        m_bPingWheelActive = true;
        m_fPingWheelCenterX = static_cast<f32_t>(in.GetMouseX());
        m_fPingWheelCenterY = static_cast<f32_t>(in.GetMouseY());
        m_vPingWheelWorldPos = ResolveMouseMapSurfacePos();
    }

    if (!m_bPingWheelActive)
        return false;

    if (!in.IsRButtonDown())
    {
        const f32_t fMouseX = static_cast<f32_t>(in.GetMouseX());
        const f32_t fMouseY = static_cast<f32_t>(in.GetMouseY());
        const u8_t iDirection = ResolvePingWheelDirectionCode(
            m_fPingWheelCenterX,
            m_fPingWheelCenterY,
            fMouseX,
            fMouseY);
        CGameInstance::Get()->UI_Push_MapPing(m_vPingWheelWorldPos, iDirection);
        m_bPingWheelActive = false;
        CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
        return true;
    }

    CGameInstance::Get()->UI_Set_PingWheel(
        true,
        m_fPingWheelCenterX,
        m_fPingWheelCenterY,
        static_cast<f32_t>(in.GetMouseX()),
        static_cast<f32_t>(in.GetMouseY()));
    return true;
}

void CScene_InGame::UpdateCombatInput(bool& outSkipGroundMove)
{
    outSkipGroundMove = false;

    auto& in = CInput::Get();
    const bool bImGuiMouse = ImGui::GetIO().WantCaptureMouse;
    const bool bImGuiKbd = ImGui::GetIO().WantCaptureKeyboard;
    const u8_t wSlot = static_cast<u8_t>(eSkillSlot::W);
    const bool_t bWDown = in.IsKeyDown('W');
    const bool_t bWPressed = in.IsKeyPressed('W');
    const bool_t bWReleased = in.IsKeyReleased('W');
    const bool_t bWStagePending = HasPendingSkillStage(*this, wSlot);
    if (bWPressed || bWReleased ||
        (bImGuiKbd && !bWDown && (s_bWReleasePending || bWStagePending)))
    {
        TraceWGate(
            bImGuiKbd && !bWDown && (s_bWReleasePending || bWStagePending)
                ? "frame-deferred"
                : "frame-edge",
            GetPlayerChampionId(),
            bWDown,
            bWPressed,
            bWReleased,
            s_bWReleasePending,
            bWStagePending,
            bImGuiKbd,
            false);
    }
    if (IsPlayerDead())
    {
        outSkipGroundMove = true;
        ApplyPlayerDeathInputLock();
        return;
    }
    if (UpdatePingWheelInput(bImGuiMouse))
    {
        outSkipGroundMove = true;
        return;
    }

    if (!HasPlayerRenderer())
        return;

    if (CGameInstance::Get()->UI_IsPointerOverActorInventory() &&
        (in.IsLButtonDown() || in.IsLButtonPressed() || in.IsLButtonReleased()))
    {
        outSkipGroundMove = true;
        return;
    }

    const bool_t bKalistaCarried =
        m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<ReplicatedStateComponent>(m_PlayerEntity) &&
        (m_World.GetComponent<ReplicatedStateComponent>(m_PlayerEntity).stateFlags &
            kSnapshotStateKalistaCarriedFlag) != 0u;
    if (bKalistaCarried)
    {
        outSkipGroundMove = true;
        if (!bImGuiMouse && in.IsLButtonPressed() &&
            m_pCommandSerializer && m_pNetworkView &&
            m_pNetworkView->IsConnected())
        {
            const Vec3 launchTarget = ResolveMouseMapSurfacePos();
            m_pCommandSerializer->SendKalistaFateCallLaunch(
                *m_pNetworkView,
                launchTarget);
        }
        return;
    }

    if (!bImGuiKbd &&
        IsNetworkAuthoritativeGameplay() &&
        m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<InventoryComponent>(m_PlayerEntity) &&
        m_pCommandSerializer && m_pNetworkView &&
        m_pNetworkView->IsConnected())
    {
        const auto& inventory =
            m_World.GetComponent<InventoryComponent>(m_PlayerEntity);
        for (u8_t slot = 0u; slot < InventoryComponent::kMaxSlots; ++slot)
        {
            if (!in.IsKeyPressed('1' + slot))
                continue;

            const u16_t itemId = inventory.itemIds[slot];
            if (itemId == 0u)
                continue;

            const Vec3 itemPos = ResolveMouseMapSurfacePos();
            const Vec3 origin = m_pPlayerTransform
                ? m_pPlayerTransform->GetPosition()
                : itemPos;
            NetEntityId targetNet = NULL_NET_ENTITY;
            if (m_pEntityIdMap && GetHoveredEntity() != NULL_ENTITY)
                targetNet = m_pEntityIdMap->ToNet(GetHoveredEntity());
            ClearNetworkAttackIntent();
            m_pCommandSerializer->SendUseItem(
                *m_pNetworkView,
                slot,
                itemId,
                itemPos,
                WintersMath::DirectionXZ(origin, itemPos, Vec3{}),
                targetNet);
        }
    }

    if (IsPlayerStunned())
        return;

    Vec3 minimapClickTarget{};
    const bool_t bMinimapLeftClick =
        !bImGuiMouse &&
        in.IsLButtonPressed() &&
        TryResolveMinimapClickTarget(minimapClickTarget);
    const bool_t bAttackMoveClick =
        !bImGuiMouse && !bMinimapLeftClick && in.IsKeyDown('A') && in.IsLButtonPressed();
    const bool_t bBasicAttackClick =
        !bImGuiMouse && (in.IsRButtonPressed() || bAttackMoveClick);

    if (!bImGuiKbd)
    {
        if (in.IsKeyPressed('P'))
            CGameInstance::Get()->UI_Toggle_InGameShop();

        if (in.IsKeyPressed('L'))
        {
            static bool_t s_bCursorLocked = false;
            s_bCursorLocked = !s_bCursorLocked;
            HWND hWnd = in.GetWindowHandle();
            if (s_bCursorLocked && hWnd)
            {
                RECT rcClient{};
                GetClientRect(hWnd, &rcClient);
                POINT ptTL{ rcClient.left, rcClient.top };
                POINT ptBR{ rcClient.right, rcClient.bottom };
                ClientToScreen(hWnd, &ptTL);
                ClientToScreen(hWnd, &ptBR);
                RECT rcClip{ ptTL.x, ptTL.y, ptBR.x, ptBR.y };
                ClipCursor(&rcClip);
            }
            else
            {
                ClipCursor(nullptr);
            }
        }

        if (in.IsKeyPressed('B') && IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = GetCommandSerializer();
            CClientNetwork* pNetworkView = GetNetworkView();
            if (pCommandSerializer && pNetworkView && pNetworkView->IsConnected())
            {
                ClearNetworkAttackIntent();
                pCommandSerializer->SendRecall(*pNetworkView);
            }
        }

        if (in.IsKeyPressed('F'))
        {
            ClearNetworkAttackIntent();
            TriggerFlash();
        }

        const auto dispatchKey = [&](u8_t slot, i32_t key)
        {
            const eSkillInputActivation activation =
                ResolveLocalSkillInputActivation(slot);
            const bool_t bPressed = in.IsKeyPressed(key);
            const bool_t bDown = in.IsKeyDown(key);
            const bool_t bPending = HasPendingSkillStage(*this, slot);

            if (activation == eSkillInputActivation::PressRelease)
            {
                if (bPressed && !bPending)
                {
                    ClearNetworkAttackIntent();
                    const bool_t bDispatched = DispatchSkillInput(slot, 1u);
                    if (slot == wSlot)
                    {
                        s_bWReleasePending =
                            bDispatched && HasPendingSkillStage(*this, slot);
                        TraceWGate(
                            "stage1-arm",
                            GetPlayerChampionId(),
                            bDown,
                            bPressed,
                            in.IsKeyReleased(key),
                            s_bWReleasePending,
                            HasPendingSkillStage(*this, slot),
                            bImGuiKbd,
                            bDispatched);
                    }
                }
                else if (!bDown &&
                    (bPending || (slot == wSlot && s_bWReleasePending)))
                {
                    ClearNetworkAttackIntent();
                    const bool_t bDispatched = DispatchSkillInput(slot, 2u);
                    if (slot == wSlot)
                    {
                        TraceWGate(
                            "stage2-release",
                            GetPlayerChampionId(),
                            bDown,
                            bPressed,
                            in.IsKeyReleased(key),
                            s_bWReleasePending,
                            HasPendingSkillStage(*this, slot),
                            bImGuiKbd,
                            bDispatched);
                        s_bWReleasePending = false;
                    }
                }
            }
            else if (bPressed)
            {
                ClearNetworkAttackIntent();
                DispatchSkillInput(slot);
            }
            if (slot == wSlot && activation != eSkillInputActivation::PressRelease)
                s_bWReleasePending = false;
        };

        dispatchKey(static_cast<u8_t>(eSkillSlot::Q), 'Q');
        dispatchKey(wSlot, 'W');
        dispatchKey(static_cast<u8_t>(eSkillSlot::E), 'E');
        dispatchKey(static_cast<u8_t>(eSkillSlot::R), 'R');
    }

    if (IsNetworkAuthoritativeGameplay())
    {
        if (bBasicAttackClick)
        {
            TryQueueLocalPassiveDashFromCursor();

            const Vec3 vCursorGround = ResolveMouseMapSurfacePos();
            const EntityID target = GameplayQuery::FindAttackTargetNearCursor(
                GetWorld(),
                GetPlayerEntity(),
                GetHoveredEntity(),
                GetPlayerTeam(),
                vCursorGround,
                bAttackMoveClick,
                GetPlayerChampionId(),
                GetBasicAttackRange());

            if (target == NULL_ENTITY)
            {
                if (s_uNetworkAttackMissLogCount < 32u)
                {
                    Winters::DevSmoke::Log(
                        "[BA] network attack intent miss hover=%u hoverTeam=%d playerTeam=%d\n",
                        static_cast<u32_t>(GetHoveredEntity()),
                        static_cast<i32_t>(GetHoveredTeam()),
                        static_cast<i32_t>(GetPlayerTeam()));
                    ++s_uNetworkAttackMissLogCount;
                }
                ClearNetworkAttackIntent();
            }
            else
            {
                s_NetworkAttackTarget = target;
                s_uNetworkAttackCommandFrame = kNetworkAttackCommandIntervalFrames;
                char dbg[128]{};
                sprintf_s(dbg,
                    "[BA] network attack intent target=%u hover=%u\n",
                    static_cast<u32_t>(s_NetworkAttackTarget),
                    static_cast<u32_t>(GetHoveredEntity()));
                Winters::DevSmoke::Log("%s", dbg);
            }
        }

        DriveNetworkAttackIntent(outSkipGroundMove);
        if (outSkipGroundMove)
            return;
    }

    if (!IsNetworkAuthoritativeGameplay() && bBasicAttackClick)
    {
        if (TryQueueLocalPassiveDashFromCursor())
        {
            outSkipGroundMove = true;
        }
        else
        {
            if (GetLastActionTimer() > 0.f
                && GetLastActionLabel()
                && std::strncmp(GetLastActionLabel(), "attack", 6) == 0)
            {
                PreemptAction("Move");
            }

            const bool fired = DispatchSkillInput(static_cast<uint8_t>(eSkillSlot::BasicAttack));
            if (fired)
                outSkipGroundMove = true;
            else
            {
                PreemptAction("Move");
            }
        }
    }
}

void CScene_InGame::FirePlayerAction(const char* actionKey)
{
    if (IsPlayerDead())
        return;

    using namespace Engine;
    eChampion champ = GetPlayerChampionId();

    const ChampionDef* cd = FindClientChampionDef(champ);
    if (!cd) return;

    string key = actionKey;
    if (strcmp(actionKey, "attack") == 0)
        key = cd->basicAttackKey;

    string animKey = string(cd->animPrefix) + key;
    m_pPlayerRenderer->PlayAnimationByName(animKey);

    m_pLastActionLabel = actionKey;
    m_fLastActionTimer = 1.2f;
}

bool CScene_InGame::IsEnemyOfPlayer(EntityID entity)
{
    if (entity == NULL_ENTITY)
        return false;

    const eTeam team = GameplayStateQuery::ResolveEntityTeam(m_World, entity);

    return team != eTeam::TEAM_END && team != m_PlayerTeam;
}

void CScene_InGame::ProtectNetworkAttackYaw(
    CClientNetwork* pNetworkView,
    u32_t commandSeq,
    const Vec3& facingTarget)
{
    if (commandSeq == 0 ||
        !pNetworkView ||
        !m_pSnapshotApplier ||
        !m_pPlayerTransform)
    {
        return;
    }

    const Vec3 origin = m_pPlayerTransform->GetPosition();
    const Vec3 facingDirection =
        WintersMath::DirectionXZ(origin, facingTarget, Vec3{});
    if (facingDirection.x == 0.f && facingDirection.z == 0.f)
        return;

    const f32_t predictedYaw = ResolveChampionVisualYawNear(
        GetPlayerChampionId(),
        facingDirection,
        m_pPlayerTransform->GetRotation().y);

    SetPlayerYaw(predictedYaw);
    m_pSnapshotApplier->ProtectLocalMoveYaw(
        pNetworkView->GetMyNetEntityId(),
        commandSeq,
        predictedYaw);
    if (GetPlayerChampionId() == eChampion::KALISTA)
    {
        SetKalistaPassiveDashFaceDir(facingDirection);
    }
}

void CScene_InGame::DriveNetworkAttackIntent(bool& outSkipGroundMove)
{
    if (s_NetworkAttackTarget == NULL_ENTITY)
        return;

    outSkipGroundMove = true;

    if (m_bAnnieTibbersCommandMode)
    {
        if (ResolveOwnedTibbersEntity() == NULL_ENTITY ||
            !m_pCommandSerializer ||
            !m_pNetworkView ||
            !m_pEntityIdMap)
        {
            m_bAnnieTibbersCommandMode = false;
            ClearNetworkAttackIntent();
            outSkipGroundMove = false;
            return;
        }

        const NetEntityId targetNet =
            m_pEntityIdMap->ToNet(s_NetworkAttackTarget);
        if (targetNet == NULL_NET_ENTITY)
            return;

        Vec3 targetPosition{};
        if (m_World.HasComponent<TransformComponent>(s_NetworkAttackTarget))
        {
            targetPosition = m_World.GetComponent<TransformComponent>(
                s_NetworkAttackTarget).GetPosition();
        }
        m_pCommandSerializer->SendCompanionCommand(
            *m_pNetworkView,
            targetNet,
            targetPosition,
            eCompanionCommandMode::Attack);
        ClearNetworkAttackIntent();
        return;
    }

    if (!GameplayStateQuery::CanAttack(m_World, m_PlayerEntity))
        return;

    if (!m_bNetworkAuthoritativeGameplay ||
        !GameplayQuery::IsValidAttackTarget(
            m_World,
            m_PlayerEntity,
            s_NetworkAttackTarget,
            m_PlayerTeam))
    {
        ClearNetworkAttackIntent();
        outSkipGroundMove = false;
        return;
    }

    if (!m_pCommandSerializer || !m_pNetworkView || !m_pEntityIdMap)
    {
        Winters::DevSmoke::Log("[BA] network basic-attack intent skipped: network objects missing\n");
        return;
    }

    if (s_uNetworkAttackCommandFrame < kNetworkAttackCommandIntervalFrames)
    {
        ++s_uNetworkAttackCommandFrame;
        return;
    }
    s_uNetworkAttackCommandFrame = 0u;

    const NetEntityId targetNet = m_pEntityIdMap->ToNet(s_NetworkAttackTarget);
    if (targetNet == NULL_NET_ENTITY)
    {
        char dbg[192]{};
        sprintf_s(dbg,
            "[BA] network basic-attack intent cleared: target has no netId entity=%u\n",
            static_cast<u32_t>(s_NetworkAttackTarget));
        Winters::DevSmoke::Log("%s", dbg);
        ClearNetworkAttackIntent();
        outSkipGroundMove = false;
        return;
    }

    Vec3 cursorGround{};
    Vec3 direction{};
    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_PlayerEntity) &&
        m_World.HasComponent<TransformComponent>(s_NetworkAttackTarget))
    {
        const Vec3 origin = m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
        cursorGround = m_World.GetComponent<TransformComponent>(s_NetworkAttackTarget).GetPosition();
        const f32_t dx = cursorGround.x - origin.x;
        const f32_t dz = cursorGround.z - origin.z;
        const f32_t lenSq = dx * dx + dz * dz;
        if (lenSq > 0.0001f)
        {
            const f32_t invLen = 1.f / std::sqrtf(lenSq);
            direction = Vec3{ dx * invLen, 0.f, dz * invLen };
        }
    }
    else if (m_pCamera)
    {
        cursorGround = ResolveMouseMapSurfacePos();

        if (m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        {
            const Vec3 origin = m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
            const f32_t dx = cursorGround.x - origin.x;
            const f32_t dz = cursorGround.z - origin.z;
            const f32_t lenSq = dx * dx + dz * dz;
            if (lenSq > 0.0001f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                direction = Vec3{ dx * invLen, 0.f, dz * invLen };
            }
        }
    }

    const u32_t attackSeq =
        m_pCommandSerializer->SendBasicAttack(
            *m_pNetworkView,
            targetNet,
            cursorGround,
            direction);
    ProtectNetworkAttackYaw(m_pNetworkView, attackSeq, cursorGround);
    ClearNetworkAttackIntent();
    outSkipGroundMove = true;
}

bool_t CScene_InGame::IsPlayerDead() const
{
    if (m_PlayerEntity == NULL_ENTITY)
        return false;

    if (m_World.HasComponent<HealthComponent>(m_PlayerEntity))
    {
        const HealthComponent& Health =
            m_World.GetComponent<HealthComponent>(m_PlayerEntity);
        if (Health.bIsDead || Health.fCurrent <= 0.f)
            return true;
    }

    if (m_World.HasComponent<ReplicatedStateComponent>(m_PlayerEntity))
    {
        const ReplicatedStateComponent& State =
            m_World.GetComponent<ReplicatedStateComponent>(m_PlayerEntity);
        if ((State.stateFlags & kSnapshotStateDeadFlag) != 0u)
            return true;
    }

    if (m_World.HasComponent<PoseStateComponent>(m_PlayerEntity))
    {
        const PoseStateComponent& Pose =
            m_World.GetComponent<PoseStateComponent>(m_PlayerEntity);
        if (Pose.poseId == static_cast<u16_t>(ePoseStateId::Dead))
            return true;
    }

    return false;
}

void CScene_InGame::ResetLocalControlHandoffState()
{
    if (m_bPingWheelActive)
    {
        m_bPingWheelActive = false;
        CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
    }

    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);
    ClearNetworkAttackIntent();
    ClearActiveSkillRuntime();
    ResetLocalSkillRuntimeState();
    m_bAnnieTibbersCommandMode = false;

    m_bMoving = false;
    m_bDashActive = false;
    m_fDashElapsed = 0.f;
    m_DashTargetEntity = NULL_ENTITY;
    m_bKalistaPassiveDashActive = false;
    m_fKalistaPassiveDashElapsed = 0.f;
    m_vKalistaPassiveDashStart = {};
    m_vKalistaPassiveDashEnd = {};
    m_bKalistaPassiveDashAnimActive = false;
    m_uKalistaLastPassiveDashActionSeq = 0u;
    m_bKalistaPassiveDashMoveCommandPending = false;
    m_bYasuoDashActive = false;
    m_fYasuoDashElapsed = 0.f;
    m_YasuoDashTargetEntity = NULL_ENTITY;
    m_bYasuoRActive = false;
    m_fYasuoRElapsed = 0.f;
    m_YasuoRTarget = NULL_ENTITY;
    m_iYasuoRHitsFired = 0;
    m_fYasuoRPrevHitTime = 0.f;
    m_fLastActionTimer = 0.f;
    m_fEndTransitionTimer = 0.f;
    m_pPendingEndAnim = nullptr;

    if (m_pPlayerTransform)
        m_vPlayerDest = m_pPlayerTransform->GetPosition();

    if (m_PlayerEntity != NULL_ENTITY)
    {
        if (m_World.HasComponent<MoveTargetComponent>(m_PlayerEntity))
            m_World.GetComponent<MoveTargetComponent>(m_PlayerEntity) = MoveTargetComponent{};
        if (m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
        {
            NavAgentComponent& Agent =
                m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
            Agent.bHasGoal = false;
            Agent.bPathDirty = false;
            Agent.pathCellsX.clear();
            Agent.pathCellsY.clear();
            Agent.iPathIndex = 0;
        }
    }
}

void CScene_InGame::ApplyPlayerDeathInputLock()
{
    ResetLocalControlHandoffState();
}

void CScene_InGame::RenderDeathScreenOverlay()
{
    if (!IsPlayerDead() || !m_pWhiteTexture)
        return;

    const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
    const u32_t iScreenWidth = static_cast<u32_t>(
        DisplaySize.x > 0.f ? DisplaySize.x : kFallbackScreenWidth);
    const u32_t iScreenHeight = static_cast<u32_t>(
        DisplaySize.y > 0.f ? DisplaySize.y : kFallbackScreenHeight);
    if (iScreenWidth == 0u || iScreenHeight == 0u)
        return;

    CGameInstance* pGameInstance = CGameInstance::Get();
    if (!pGameInstance->UI_Begin_RawImagePass(iScreenWidth, iScreenHeight, false))
        return;

    pGameInstance->UI_Draw_RawImage(
        m_pWhiteTexture->GetNativeSRV(),
        0.f,
        0.f,
        static_cast<f32_t>(iScreenWidth),
        static_cast<f32_t>(iScreenHeight),
        Vec4(0.f, 0.f, 1.f, 1.f),
        Vec4(0.12f, 0.12f, 0.12f, 0.64f));
    pGameInstance->UI_End_RawImagePass();
}
