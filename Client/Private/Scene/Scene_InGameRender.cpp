// Scene_InGameRender.cpp — CScene_InGame의 프레임 렌더 오케스트레이션 책임 TU.
// Stage 1 (mechanical split): Scene_InGame.cpp에서 verbatim 이동. 동작/시그니처/호출순서 불변.
// 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
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
#include "ECS/Components/CoreComponents.h"   // ColliderComponent
#include "ECS/Systems/MinionAISystem.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/Systems/MCTSSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "ECS/Systems/TurretProjectileSystem.h"
#include "ECS/Systems/MinionPerformanceSystem.h"
#include "ECS/Systems/YoneSoulSpawnSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/BushVolumeIndex.h"
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
#include "GameObject/Champion/Zed/ZedFxPresets.h"
#include "GameObject/Champion/Annie/Annie_Components.h"
#include "GameObject/Champion/Ashe/Ashe_Components.h"
#include "GameObject/Champion/Irelia/Irelia_Skills.h"
#include "GameObject/Champion/Jax/Jax_Components.h"
#include "GameObject/Champion/Kalista/Kalista_Skills.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "Shared/GameSim/Components/HealthComponent.h"
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
#include "GameContext.h"
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
#include "ECS/Components/GameplayComponents.h"   // Stun/Slow/Disarm
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include "Renderer/RHISceneRenderer.h"
#include "Renderer/RenderWorldSnapshot.h"

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

namespace
{
    Vec3 GameplayForwardFromVisualYaw(eChampion champion, f32_t yaw)
    {
        const f32_t gameplayYaw = yaw - ClientData::ResolveChampionModelYawOffset(champion);
        return Vec3{ std::sinf(gameplayYaw), 0.f, std::cosf(gameplayYaw) };
    }

    void FlushTransformForRender(TransformComponent& tf)
    {
        if (tf.m_bLocalDirty)
        {
            DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalScale));
            DirectX::XMVECTOR rot = DirectX::XMQuaternionRotationRollPitchYaw(
                tf.m_LocalRotation.x,
                tf.m_LocalRotation.y,
                tf.m_LocalRotation.z);
            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&tf.m_LocalPosition));
            DirectX::XMMATRIX local = DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rot, pos);
            DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&tf.m_LocalMatrix), local);
            tf.m_bLocalDirty = false;
            tf.m_bWorldDirty = true;
        }

        if (tf.m_bWorldDirty)
        {
            tf.m_WorldMatrix = tf.m_LocalMatrix;
            tf.m_bWorldDirty = false;
        }
    }

    Mat4 BuildContactShadowWorld(const TransformComponent& tf,
        f32_t fSize,
        f32_t fYOffset)
    {
        const Vec3 vPos = tf.GetPosition();
        const DirectX::XMMATRIX matScale =
            DirectX::XMMatrixScaling(fSize, 1.f, fSize * 0.72f);
        const DirectX::XMMATRIX matTrans =
            DirectX::XMMatrixTranslation(vPos.x, vPos.y + fYOffset, vPos.z);

        Mat4 matWorld{};
        DirectX::XMStoreFloat4x4(
            reinterpret_cast<DirectX::XMFLOAT4X4*>(&matWorld.m),
            matScale * matTrans);

        return matWorld;
    }

    void RenderAttackRangePreview(
        CScene_InGame& scene,
        const Mat4& matViewProjection,
        IRHIDevice* pDevice,
        bool_t bUseDX12RHI)
    {
        CTransform* pPlayerTransform = scene.GetPlayerTransformPtr();
        if (!scene.IsShowAttackRange() || !pPlayerTransform)
            return;

        const f32_t fRadius = GameplayQuery::ResolveAttackRangePreviewRadius(
            scene.GetWorld(),
            scene.GetPlayerEntity(),
            scene.GetPlayerChampionId(),
            scene.GetBasicAttackRange(),
            scene.IsNetworkAuthoritativeGameplay());

        const Vec3& vPos = pPlayerTransform->GetPosition();
        const DirectX::XMMATRIX matScale =
            DirectX::XMMatrixScaling(fRadius * 2.f, 1.f, fRadius * 2.f);
        const DirectX::XMMATRIX matTrans =
            DirectX::XMMatrixTranslation(vPos.x, vPos.y + 0.02f, vPos.z);

        Mat4 matWorld{};
        DirectX::XMStoreFloat4x4(
            reinterpret_cast<DirectX::XMFLOAT4X4*>(&matWorld.m),
            matScale * matTrans);

        if (bUseDX12RHI &&
            scene.GetRHIUtilityPlaneRenderer() &&
            scene.GetRHIAttackRangeTexture().IsValid() &&
            pDevice)
        {
            scene.GetRHIUtilityPlaneRenderer()->Draw(
                pDevice,
                scene.GetRHIAttackRangeTexture(),
                matWorld,
                matViewProjection,
                { 1.f, 1.f, 1.f, 1.f },
                { 0.f, 0.f, 1.f, 1.f },
                { 0.f, 0.f },
                0.02f,
                0.f,
                eBlendPreset::AlphaBlend);
            return;
        }

        if (scene.GetAttackRangePlane() && scene.GetAttackRangeTexture())
        {
            scene.GetAttackRangePlane()->SetWorld(matWorld);
            scene.GetAttackRangePlane()->Render(pDevice, matViewProjection);
        }
    }
}

void CScene_InGame::OnRender()
{
    Mat4 vp = m_pCamera->GetViewProjection();
    const Vec3 cameraWorld = m_pCamera ? m_pCamera->GetEye() : Vec3{};
    const u64_t lastSnapshotTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    u32_t yawTraceNetId = 0;
    u32_t yawTraceCommandSeq = 0;
    f32_t yawTraceProtectedYaw = 0.f;
    const bool_t bHasYawTraceProtection = m_pSnapshotApplier &&
        m_pSnapshotApplier->GetLocalMoveYawProtectionDebug(
            yawTraceNetId,
            yawTraceCommandSeq,
            yawTraceProtectedYaw);
    (void)yawTraceNetId;
    (void)yawTraceProtectedYaw;
    CGameInstance* pGameInstance = CGameInstance::Get();
    IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
    void* pAmbientOcclusionSRV =
        m_pWhiteTexture ? m_pWhiteTexture->GetNativeSRV() : nullptr;
    const bool_t bUseDX12RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX12;
    const bool_t bUseDX11RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX11;
    const bool_t bSSAOEnabled = m_pSSAOPass && m_pSSAOPass->GetEnabled();

    const u8_t localTeam = UI::QueryLocalTeam(m_World);
    const bool_t bRevealAllForPlayback = ShouldRevealAllForPlayback();

    if (bUseDX11RHI && m_pNormalPass && bSSAOEnabled)
    {
        WINTERS_PROFILE_SCOPE("Render::NormalPass");
        m_pNormalPass->Begin(pDevice);

        m_Map.UpdateCamera(vp, cameraWorld);
        m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
        m_Map.RenderNormalPassFrustumCulled(
            m_pNormalPass->GetStaticShader(),
            m_pNormalPass->GetStaticPipeline(),
            m_pNormalPass->GetSkinnedShader(),
            m_pNormalPass->GetSkinnedPipeline(),
            vp);

        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
                if (m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderNormalPassWithVisibility(
                        m_pNormalPass->GetStaticShader(),
                        m_pNormalPass->GetStaticPipeline(),
                        m_pNormalPass->GetSkinnedShader(),
                        m_pNormalPass->GetSkinnedPipeline(),
                        visibility.mask);
                }
                else
                {
                    rc.pRenderer->RenderNormalPassFrustumCulled(
                        m_pNormalPass->GetStaticShader(),
                        m_pNormalPass->GetStaticPipeline(),
                        m_pNormalPass->GetSkinnedShader(),
                        m_pNormalPass->GetSkinnedPipeline(),
                        vp);
                }
            });

        m_pNormalPass->End(pDevice);

        if (m_pSSAOPass && m_pSSAOPass->GetEnabled())
        {
            WINTERS_PROFILE_SCOPE("Render::SSAO");
            m_pSSAOPass->Execute(
                pDevice,
                m_pNormalPass->GetDepthSRVNative(),
                m_pNormalPass->GetNormalSRVNative(),
                vp);

            if (m_pSSAOPass->GetOutputSRVNative())
                pAmbientOcclusionSRV = m_pSSAOPass->GetOutputSRVNative();
        }
    }

    {
        WINTERS_PROFILE_SCOPE("Map::Render");
        WINTERS_PROFILE_COUNT("Map::MeshCount", m_Map.GetMeshCount());

        {
            WINTERS_PROFILE_SCOPE("Map::UpdateCamera");
            m_Map.UpdateCamera(vp, cameraWorld);
        }
        {
            WINTERS_PROFILE_SCOPE("Map::UpdateTransform");
            m_Map.UpdateTransform(m_MapTransform.GetWorldMatrix());
        }
        {
            WINTERS_PROFILE_SCOPE("Map::SetAmbientOcclusion");
            m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
        }
        if (m_pRHISceneRenderer && m_pRHISceneRenderer->IsReady())
        {
            WINTERS_PROFILE_SCOPE("Map::RHISceneSnapshot");

            RenderWorldSnapshot snapshot{};
            snapshot.view.matViewProjection = vp;
            snapshot.view.vCameraWorld = cameraWorld;

            const u32_t appendedCount =
                m_Map.AppendRenderSnapshotMeshesFrustumCulled(snapshot, vp);
            WINTERS_PROFILE_COUNT("Map::RHISceneSnapshotMeshes", appendedCount);

            if (appendedCount > 0)
                m_pRHISceneRenderer->Render(pDevice, snapshot);
        }
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            m_Map.RenderFrustumCulled(vp);
        }
    }

    {
        WINTERS_PROFILE_SCOPE("Champion::Render");
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                TransformComponent& tf)
            {
                if (rc.bSceneManaged) return;
                if (!rc.bVisible || !rc.pRenderer) return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback)) return;
                FlushTransformForRender(tf);
                rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                const bool_t bLocalYawTraceTarget =
                    IsNetworkAuthoritativeGameplay() &&
                    e == GetPlayerEntity();
                if (bLocalYawTraceTarget && bHasYawTraceProtection)
                {
                    rc.pRenderer->SetYawTraceContext(
                        lastSnapshotTick,
                        static_cast<u32_t>(e),
                        static_cast<u32_t>(champion.id),
                        yawTraceCommandSeq,
                        tf.GetRotation().y,
                        GameplayForwardFromVisualYaw(champion.id, tf.GetRotation().y));
                }
                else if (bLocalYawTraceTarget)
                {
                    rc.pRenderer->ClearYawTraceContext();
                }
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
                if (m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderWithVisibility(visibility.mask);
                }
                else
                {
                    rc.pRenderer->RenderFrustumCulled(vp);
                }
            }
        );
    }

    {
        WINTERS_PROFILE_SCOPE("Structure::Render");
        CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
    }
    {
        WINTERS_PROFILE_SCOPE("Jungle::Render");
        CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    }
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);

    CAmbientProp_Manager::Get()->Render(vp, cameraWorld);

    if (!bUseDX12RHI && m_pContactShadowPlane)
    {
        WINTERS_PROFILE_SCOPE("ContactShadow::Render");
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                m_pContactShadowPlane->SetWorld(
                    BuildContactShadowWorld(tf, 1.22f, 0.055f));
                m_pContactShadowPlane->Render(pDevice, vp);
            });
    }

    if (!bRevealAllForPlayback && m_pFogOfWarRenderer)
    {
        WINTERS_PROFILE_SCOPE("FogOfWar::Render");
        const Engine::CVisionSystem::FowProjection Projection =
            m_pVisionSystem
                ? m_pVisionSystem->GetFowProjection()
                : Engine::CVisionSystem::FowProjection{};
        m_pFogOfWarRenderer->RenderWorldOverlay(
            pDevice,
            vp,
            Projection.vWorldAtUv00,
            Projection.vWorldAtUv10,
            Projection.vWorldAtUv01,
            0.05f);
    }

    {
        WINTERS_PROFILE_SCOPE("DebugDraw::Render");
        UI::CDebugDrawSystem::Render(m_World, this, vp);
    }

    {
        WINTERS_PROFILE_SCOPE("AttackRange::Render");
        RenderAttackRangePreview(*this, vp, pDevice, bUseDX12RHI);
    }

    if (m_pFxMeshSystem && m_pCamera)
        m_pFxMeshSystem->Render(m_World, m_pCamera.get());
    if (m_pFxBeamSystem && m_pCamera)
        m_pFxBeamSystem->Render(m_World, m_pCamera.get());
    if (m_pFxSystem && m_pCamera)
        m_pFxSystem->Render(m_World, m_pCamera.get());

    {
        WINTERS_PROFILE_SCOPE("UIOverlay::Render");
        CGameInstance::Get()->UI_Render_Overlay(vp);
    }

    {
        WINTERS_PROFILE_SCOPE("Minimap::Render");
        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        const f32_t fScreenWidth =
            DisplaySize.x > 0.f ? DisplaySize.x : kFallbackScreenWidth;
        const f32_t fScreenHeight =
            DisplaySize.y > 0.f ? DisplaySize.y : kFallbackScreenHeight;

        UI::MinimapFrameState MinimapState{};
        UI::BuildMinimapFrameState(
            m_World,
            m_pFogOfWarRenderer.get(),
            GetPlayerTeam(),
            fScreenWidth,
            fScreenHeight,
            bRevealAllForPlayback,
            MinimapState);
        if (m_pCamera)
        {
            MinimapState.vCameraWorldCenter = m_pCamera->GetAt();
            MinimapState.bShowCameraBounds = true;
        }
        UI::CMinimapPanel::RenderRuntime(MinimapState);
    }

    RenderDeathScreenOverlay();
}
