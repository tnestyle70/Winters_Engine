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
#include <unordered_set>
#include "Scene/GameplayQuery.h"
#include "Scene/InGameRosterSpawner.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_InGameInternal.h"
#include "Scene/Scene_Editor.h"
#include "Manager/Structure_Manager.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Bush_Manager.h"
#include "Manager/AmbientProp_Manager.h"
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
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
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

    bool_t IsViegoMistInvisible(CWorld& world, EntityID entity, const ChampionComponent& champion)
    {
        if (entity == NULL_ENTITY || champion.id != eChampion::VIEGO)
            return false;
        if (!world.HasComponent<ReplicatedStateComponent>(entity))
            return false;

        const auto& state = world.GetComponent<ReplicatedStateComponent>(entity);
        return (state.stateFlags & kSnapshotStateInvisibleFlag) != 0u;
    }

    void ApplyViegoMistMaterialOverride(
        CWorld& world,
        EntityID entity,
        const ChampionComponent& champion,
        ModelRenderer* pRenderer)
    {
        if (!pRenderer)
            return;

        static std::unordered_set<ModelRenderer*> s_StasisRenderers;
        const bool_t bStasis =
            world.HasComponent<ReplicatedStateComponent>(entity) &&
            (world.GetComponent<ReplicatedStateComponent>(entity)
                .gameplayStateFlags & kGameplayStateStasisVisualFlag) != 0u;
        if (bStasis)
        {
            pRenderer->SetMaterialOverrideColor(
                Vec4{ 1.30f, 0.82f, 0.18f, 1.f }, true);
            s_StasisRenderers.insert(pRenderer);
            return;
        }
        if (s_StasisRenderers.erase(pRenderer) > 0u)
            pRenderer->ClearMaterialOverrideColor();

        if (world.HasComponent<ViegoSoulComponent>(entity))
        {
            pRenderer->SetMaterialOverrideColor(
                Vec4{ 0.20f, 1.05f, 0.72f, 0.80f },
                true);
            return;
        }

        if (champion.id != eChampion::VIEGO)
            return;

        if (IsViegoMistInvisible(world, entity, champion))
        {
            pRenderer->SetMaterialOverrideColor(
                Vec4{ 0.24f, 0.78f, 0.56f, 0.50f },
                true);
        }
        else
        {
            pRenderer->ClearMaterialOverrideColor();
        }
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

    u32_t AppendChampionSnapshotMeshes(
        CWorld& world,
        RenderWorldSnapshot& snapshot,
        const Mat4& matViewProjection,
        const u8_t localTeam,
        bool_t bRevealAllForPlayback)
    {
        u32_t appendedCount = 0;

        world.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (world.HasComponent<ViegoSoulComponent>(e))
                    return;
                if (rc.bAnimated || rc.pRenderer->HasSkeleton())
                    return;
                if (!UI::IsRenderableForLocal(world, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                ApplyViegoMistMaterialOverride(world, e, champion, rc.pRenderer);
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());

                if (world.HasComponent<MeshGroupVisibilityComponent>(e)
                    && world.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = world.GetComponent<MeshGroupVisibilityComponent>(e);
                    appendedCount += rc.pRenderer->AppendRenderSnapshotMeshes(
                        snapshot,
                        visibility.mask);
                }
                else
                {
                    appendedCount += rc.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(
                        snapshot,
                        matViewProjection);
                }
            });

        return appendedCount;
    }

    bool_t IsRHISceneOnlyMode()
    {
        return HasCommandLineToken(L"--rhi-scene-only") ||
            HasCommandLineToken(L"/rhi-scene-only");
    }
}

void CScene_InGame::OnRender()
{
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnRender");
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
    const bool_t bRHISceneReady = m_pRHISceneRenderer && m_pRHISceneRenderer->IsReady();
    const bool_t bRHISceneOnly = bRHISceneReady && IsRHISceneOnlyMode();
    WINTERS_PROFILE_COUNT("RHISceneOnly", bRHISceneOnly ? 1u : 0u);

    const u8_t localTeam = UI::QueryLocalTeam(m_World);
    const bool_t bRevealAllForPlayback = ShouldRevealAllForPlayback();

    // GPU 패스 스코프: 주석 마커 + 패스별 타임스탬프 (이름 리터럴이 게이지 이름으로 저장된다).
    const auto beginGpuPass = [&](const char* pName)
    {
        if (pDevice)
            pDevice->BeginGpuPass(pName);
    };
    const auto endGpuPass = [&]()
    {
        if (pDevice)
            pDevice->EndGpuPass();
    };

    if (!bRHISceneOnly && bUseDX11RHI && m_pNormalPass && bSSAOEnabled)
    {
        WINTERS_PROFILE_SCOPE("Render::NormalPass");
        beginGpuPass("GPU::NormalPass");
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
                if (m_World.HasComponent<ViegoSoulComponent>(e))
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
        endGpuPass();

        if (m_pSSAOPass && m_pSSAOPass->GetEnabled())
        {
            WINTERS_PROFILE_SCOPE("Render::SSAO");
            beginGpuPass("GPU::SSAO");
            m_pSSAOPass->Execute(
                pDevice,
                m_pNormalPass->GetDepthSRVNative(),
                m_pNormalPass->GetNormalSRVNative(),
                vp);
            endGpuPass();

            if (m_pSSAOPass->GetOutputSRVNative())
                pAmbientOcclusionSRV = m_pSSAOPass->GetOutputSRVNative();
        }
    }

    {
        WINTERS_PROFILE_SCOPE("Map::Render");
        WINTERS_PROFILE_COUNT("Map::MeshCount", m_Map.GetMeshCount());
        beginGpuPass("GPU::Map");

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
        if (bRHISceneOnly)
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
        if (!bRHISceneOnly)
        {
            WINTERS_PROFILE_SCOPE("Map::DrawFrustumCulled");
            m_Map.RenderFrustumCulled(vp);
        }
        endGpuPass();
    }

    {
        WINTERS_PROFILE_SCOPE("Champion::Render");
        beginGpuPass("GPU::Champions");
        if (bRHISceneOnly)
        {
            WINTERS_PROFILE_SCOPE("Champion::RHISceneSnapshot");

            RenderWorldSnapshot snapshot{};
            snapshot.view.matViewProjection = vp;
            snapshot.view.vCameraWorld = cameraWorld;

            const u32_t appendedCount = AppendChampionSnapshotMeshes(
                m_World,
                snapshot,
                vp,
                localTeam,
                bRevealAllForPlayback);
            WINTERS_PROFILE_COUNT("Champion::RHISceneSnapshotMeshes", appendedCount);

            if (appendedCount > 0)
                m_pRHISceneRenderer->Render(pDevice, snapshot);
        }

        if (!bRHISceneOnly)
        {
            const auto renderChampionPass = [&](bool_t bSoulPass)
            {
                m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
                    [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                        TransformComponent& tf)
                    {
                    const bool_t bSoul = m_World.HasComponent<ViegoSoulComponent>(e);
                    if (bSoul != bSoulPass) return;
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
                    ApplyViegoMistMaterialOverride(m_World, e, champion, rc.pRenderer);
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
                    });
            };

            renderChampionPass(false);
        }
        endGpuPass();
    }

    if (bRHISceneOnly)
    {
        WINTERS_PROFILE_SCOPE("SceneObjects::RHISceneSnapshot");
        beginGpuPass("GPU::Units");

        RenderWorldSnapshot snapshot{};
        snapshot.view.matViewProjection = vp;
        snapshot.view.vCameraWorld = cameraWorld;

        u32_t appendedCount = 0;
        appendedCount += CStructure_Manager::Get()->AppendRenderSnapshotMeshes(
            snapshot,
            vp,
            bRevealAllForPlayback);
        appendedCount += CJungle_Manager::Get()->AppendRenderSnapshotMeshes(snapshot, vp);
        appendedCount += CMinion_Manager::Get()->AppendRenderSnapshotMeshes(
            snapshot,
            vp,
            bRevealAllForPlayback);
        appendedCount += CBush_Manager::Get()->AppendRenderSnapshotMeshes(snapshot, vp);
        appendedCount += CAmbientProp_Manager::Get()->AppendRenderSnapshotMeshes(
            snapshot,
            vp);
        WINTERS_PROFILE_COUNT("SceneObjects::RHISceneSnapshotMeshes", appendedCount);

        if (appendedCount > 0)
            m_pRHISceneRenderer->Render(pDevice, snapshot);
        endGpuPass();
    }

    if (!bRHISceneOnly)
    {
        beginGpuPass("GPU::Units");
        {
            WINTERS_PROFILE_SCOPE("Structure::Render");
            CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
        }
        {
            WINTERS_PROFILE_SCOPE("Jungle::Render");
            CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
        }
        CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV, bRevealAllForPlayback);
        {
            WINTERS_PROFILE_SCOPE("Bush::Render");
            CBush_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
        }

        {
            WINTERS_PROFILE_SCOPE("AmbientProp::Render");
            CAmbientProp_Manager::Get()->Render(vp, cameraWorld);
        }
        endGpuPass();
    }

    if (!bRHISceneOnly && !bUseDX12RHI && m_pContactShadowPlane)
    {
        WINTERS_PROFILE_SCOPE("ContactShadow::Render");
        beginGpuPass("GPU::Shadows");
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (m_World.HasComponent<ViegoSoulComponent>(e))
                    return;
                if (!UI::IsRenderableForLocal(m_World, e, localTeam, bRevealAllForPlayback))
                    return;

                FlushTransformForRender(tf);
                m_pContactShadowPlane->SetWorld(
                    BuildContactShadowWorld(tf, 1.22f, 0.055f));
                m_pContactShadowPlane->Render(pDevice, vp);
            });
        endGpuPass();
    }

    if (!bRHISceneOnly)
    {
        WINTERS_PROFILE_SCOPE("ViegoSoul::TransparentRender");
        beginGpuPass("GPU::Transparent");
        std::vector<EntityID> souls;
        m_World.ForEach<ViegoSoulComponent>(
            [&](EntityID entity, ViegoSoulComponent&)
            {
                if (!m_World.HasComponent<ChampionComponent>(entity) ||
                    !m_World.HasComponent<RenderComponent>(entity) ||
                    !m_World.HasComponent<TransformComponent>(entity))
                {
                    return;
                }
                auto& rc = m_World.GetComponent<RenderComponent>(entity);
                if (!rc.bSceneManaged && rc.bVisible && rc.pRenderer &&
                    UI::IsRenderableForLocal(
                        m_World, entity, localTeam, bRevealAllForPlayback))
                {
                    souls.push_back(entity);
                }
            });

        std::sort(souls.begin(), souls.end(),
            [&](EntityID lhs, EntityID rhs)
            {
                const Vec3 lhsPos =
                    m_World.GetComponent<TransformComponent>(lhs).GetLocalPosition();
                const Vec3 rhsPos =
                    m_World.GetComponent<TransformComponent>(rhs).GetLocalPosition();
                const f32_t lhsDistSq =
                    (lhsPos.x - cameraWorld.x) * (lhsPos.x - cameraWorld.x) +
                    (lhsPos.y - cameraWorld.y) * (lhsPos.y - cameraWorld.y) +
                    (lhsPos.z - cameraWorld.z) * (lhsPos.z - cameraWorld.z);
                const f32_t rhsDistSq =
                    (rhsPos.x - cameraWorld.x) * (rhsPos.x - cameraWorld.x) +
                    (rhsPos.y - cameraWorld.y) * (rhsPos.y - cameraWorld.y) +
                    (rhsPos.z - cameraWorld.z) * (rhsPos.z - cameraWorld.z);
                return lhsDistSq > rhsDistSq;
            });

        for (const EntityID entity : souls)
        {
            auto& champion = m_World.GetComponent<ChampionComponent>(entity);
            auto& rc = m_World.GetComponent<RenderComponent>(entity);
            auto& tf = m_World.GetComponent<TransformComponent>(entity);
            FlushTransformForRender(tf);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(vp, cameraWorld);
            ApplyViegoMistMaterialOverride(
                m_World, entity, champion, rc.pRenderer);
            rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
            if (m_World.HasComponent<MeshGroupVisibilityComponent>(entity) &&
                m_World.GetComponent<MeshGroupVisibilityComponent>(entity).bEnabled)
            {
                const auto& visibility =
                    m_World.GetComponent<MeshGroupVisibilityComponent>(entity);
                rc.pRenderer->RenderWithVisibility(visibility.mask);
            }
            else
            {
                rc.pRenderer->RenderFrustumCulled(vp);
            }
        }
        endGpuPass();
    }

    if (!bRevealAllForPlayback && m_pFogOfWarRenderer)
    {
        WINTERS_PROFILE_SCOPE("FogOfWar::Render");
        beginGpuPass("GPU::FoW");
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
        endGpuPass();
    }

    {
        WINTERS_PROFILE_SCOPE("DebugDraw::Render");
        UI::CDebugDrawSystem::Render(m_World, this, vp);
    }

    {
        WINTERS_PROFILE_SCOPE("AttackRange::Render");
        RenderAttackRangePreview(*this, vp, pDevice, bUseDX12RHI);
    }

    beginGpuPass("GPU::FX");
    if (m_pFxMeshSystem && m_pCamera)
        m_pFxMeshSystem->Render(m_World, m_pCamera.get());
    if (m_pFxBeamSystem && m_pCamera)
        m_pFxBeamSystem->Render(m_World, m_pCamera.get());
    if (m_pFxSystem && m_pCamera)
        m_pFxSystem->Render(m_World, m_pCamera.get());
    endGpuPass();

    if (bUseDX11RHI && m_pPostFxPass && m_pPostFxPass->GetEnabled())
    {
        WINTERS_PROFILE_SCOPE("Render::PostFx");
        beginGpuPass("GPU::PostFx");
        const Engine::PostFxParams params = m_pPostFxPass->GetParams();
        WINTERS_PROFILE_COUNT("PostFx::BloomEnabled", params.bBloomEnabled ? 1u : 0u);
        m_pPostFxPass->Execute(pDevice);
        endGpuPass();
    }

    {
        WINTERS_PROFILE_SCOPE("UIOverlay::Render");
        beginGpuPass("GPU::HUD");
        CGameInstance::Get()->UI_Render_Overlay(vp);
        endGpuPass();
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

    {
        WINTERS_PROFILE_SCOPE("ScreenOverlay::Render");
        RenderViegoMistScreenOverlay();
        RenderDeathScreenOverlay();
    }
}

void CScene_InGame::RenderViegoMistScreenOverlay()
{
    if (!m_pWhiteTexture ||
        m_PlayerEntity == NULL_ENTITY ||
        !m_World.HasComponent<ChampionComponent>(m_PlayerEntity) ||
        !m_World.HasComponent<ReplicatedStateComponent>(m_PlayerEntity))
    {
        return;
    }

    const auto& champion = m_World.GetComponent<ChampionComponent>(m_PlayerEntity);
    if (!IsViegoMistInvisible(m_World, m_PlayerEntity, champion))
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
        Vec4(0.32f, 0.34f, 0.33f, 0.28f));
    pGameInstance->UI_End_RawImagePass();
}
