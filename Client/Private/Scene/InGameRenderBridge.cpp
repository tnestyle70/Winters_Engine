#include "Scene/InGameRenderBridge.h"

#include "GameInstance.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Systems/VisionSystem.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Structure_Manager.h"
#include "Network/Client/SnapshotApplier.h"
#include "ProfilerAPI.h"
#include "Renderer/ModelRenderer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "Scene/GameplayQuery.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "UI/DebugDrawSystem.h"

#include <cmath>
#include <cstdio>

namespace
{
    constexpr f32_t kYawHalfTurnTolerance = 0.35f;

    bool_t IsYawHalfTurn(f32_t yawDelta)
    {
        return std::fabs(std::fabs(NormalizeChampionVisualYaw(yawDelta)) - WintersMath::kPi) <=
            kYawHalfTurnTolerance;
    }

    Vec3 GameplayForwardFromVisualYaw(eChampion champion, f32_t yaw)
    {
        const f32_t gameplayYaw = yaw - GetDefaultChampionVisualYawOffset(champion);
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

    void OutputLocalPlayerRenderYawTrace(
        CScene_InGame& scene,
        EntityID entity,
        const ChampionComponent& champion,
        const TransformComponent& tf,
        u64_t lastSnapshotTick)
    {
        if (!scene.IsNetworkAuthoritativeGameplay() ||
            entity != scene.GetPlayerEntity())
        {
            return;
        }

        static u32_t s_renderYawTraceCount = 0;
        static bool_t s_hasPrevYaw = false;
        static f32_t s_prevYaw = 0.f;
        static u64_t s_lastLoggedSnapshotTick = 0;

        const f32_t yaw = tf.GetRotation().y;
        const f32_t renderDelta = s_hasPrevYaw
            ? NormalizeChampionVisualYaw(yaw - s_prevYaw)
            : 0.f;
        const bool_t bSnapshotAdvanced = lastSnapshotTick != s_lastLoggedSnapshotTick;
        const bool_t bHalfTurn = IsYawHalfTurn(renderDelta);

        if ((!bSnapshotAdvanced &&
                std::fabs(renderDelta) <= 0.0005f &&
                !bHalfTurn) ||
            s_renderYawTraceCount >= 512u)
        {
            s_prevYaw = yaw;
            s_hasPrevYaw = true;
            return;
        }

        const CTransform* playerTransform = scene.GetPlayerTransformPtr();
        const f32_t cacheYaw = playerTransform
            ? playerTransform->GetRotation().y
            : 0.f;
        const f32_t ecsVsCache = playerTransform
            ? NormalizeChampionVisualYaw(yaw - cacheYaw)
            : 0.f;
        const f32_t visualYawOffset = GetDefaultChampionVisualYawOffset(champion.id);
        const Vec3 gameplayForward =
            GameplayForwardFromVisualYaw(champion.id, yaw);
        const Vec3 matrixForward = WintersMath::NormalizeXZ(
            tf.GetWorldMatrix().TransformDirection(Vec3{ 0.f, 0.f, 1.f }),
            Vec3{},
            0.0001f);
        const Vec3 matrixRight = WintersMath::NormalizeXZ(
            tf.GetWorldMatrix().TransformDirection(Vec3{ 1.f, 0.f, 0.f }),
            Vec3{},
            0.0001f);
        const Vec3 matrixBack = WintersMath::NormalizeXZ(
            tf.GetWorldMatrix().TransformDirection(Vec3{ 0.f, 0.f, -1.f }),
            Vec3{},
            0.0001f);
        const f32_t matrixVsGameplayDot =
            matrixForward.x * gameplayForward.x + matrixForward.z * gameplayForward.z;
        const f32_t rightVsGameplayDot =
            matrixRight.x * gameplayForward.x + matrixRight.z * gameplayForward.z;
        const f32_t backVsGameplayDot =
            matrixBack.x * gameplayForward.x + matrixBack.z * gameplayForward.z;
        const bool_t bMatrixOpposesGameplay = matrixVsGameplayDot < -0.75f;

        char msg[1152]{};
        sprintf_s(
            msg,
            "[YawTrace][RenderApply] snap=%llu entity=%u champion=%u tfYaw=%.4f cacheYaw=%.4f ecsVsCache=%.4f prevRenderYaw=%.4f renderDelta=%.4f halfTurn=%u offset=%.4f gameplayF=(%.3f,%.3f) matrixF=(%.3f,%.3f) matrixR=(%.3f,%.3f) matrixBack=(%.3f,%.3f) matrixVsGameplayDot=%.4f rightVsGameplayDot=%.4f backVsGameplayDot=%.4f matrixOpposesGameplay=%u pos=(%.3f,%.3f,%.3f)\n",
            static_cast<unsigned long long>(lastSnapshotTick),
            static_cast<u32_t>(entity),
            static_cast<u32_t>(champion.id),
            yaw,
            cacheYaw,
            ecsVsCache,
            s_hasPrevYaw ? s_prevYaw : yaw,
            renderDelta,
            bHalfTurn ? 1u : 0u,
            visualYawOffset,
            gameplayForward.x,
            gameplayForward.z,
            matrixForward.x,
            matrixForward.z,
            matrixRight.x,
            matrixRight.z,
            matrixBack.x,
            matrixBack.z,
            matrixVsGameplayDot,
            rightVsGameplayDot,
            backVsGameplayDot,
            bMatrixOpposesGameplay ? 1u : 0u,
            tf.GetPosition().x,
            tf.GetPosition().y,
            tf.GetPosition().z);
        OutputDebugStringA(msg);

        s_prevYaw = yaw;
        s_hasPrevYaw = true;
        s_lastLoggedSnapshotTick = lastSnapshotTick;
        ++s_renderYawTraceCount;
    }
}

void CInGameRenderBridge::Render(CScene_InGame& scene)
{
    Mat4 vp = scene.m_pCamera->GetViewProjection();
    const Vec3 cameraWorld = scene.m_pCamera ? scene.m_pCamera->GetEye() : Vec3{};
    const u64_t lastSnapshotTick = scene.m_pSnapshotApplier
        ? scene.m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    u32_t yawTraceNetId = 0;
    u32_t yawTraceCommandSeq = 0;
    f32_t yawTraceProtectedYaw = 0.f;
    const bool_t bHasYawTraceProtection = scene.m_pSnapshotApplier &&
        scene.m_pSnapshotApplier->GetLocalMoveYawProtectionDebug(
            yawTraceNetId,
            yawTraceCommandSeq,
            yawTraceProtectedYaw);
    (void)yawTraceNetId;
    (void)yawTraceProtectedYaw;
    CGameInstance* pGameInstance = CGameInstance::Get();
    IRHIDevice* pDevice = pGameInstance->Get_RHIDevice();
    void* pAmbientOcclusionSRV =
        scene.m_pWhiteTexture ? scene.m_pWhiteTexture->GetNativeSRV() : nullptr;
    const bool_t bUseDX12RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX12;
    const bool_t bUseDX11RHI = pDevice && pDevice->GetBackend() == eRHIBackend::DX11;

    const u8_t localTeam = UI::QueryLocalTeam(scene.m_World);

    if (bUseDX11RHI && scene.m_pNormalPass)
    {
        scene.m_pNormalPass->Begin(pDevice);

        scene.m_Map.UpdateCamera(vp, cameraWorld);
        scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
        scene.m_Map.RenderNormalPass(
            scene.m_pNormalPass->GetStaticShader(),
            scene.m_pNormalPass->GetStaticPipeline(),
            scene.m_pNormalPass->GetSkinnedShader(),
            scene.m_pNormalPass->GetSkinnedPipeline());

        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(scene.m_World, e, localTeam))
                    return;

                FlushTransformForRender(tf);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                rc.pRenderer->UpdateTransform(tf.GetWorldMatrix());
                if (scene.m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && scene.m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = scene.m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderNormalPassWithVisibility(
                        scene.m_pNormalPass->GetStaticShader(),
                        scene.m_pNormalPass->GetStaticPipeline(),
                        scene.m_pNormalPass->GetSkinnedShader(),
                        scene.m_pNormalPass->GetSkinnedPipeline(),
                        visibility.mask);
                }
                else
                {
                    rc.pRenderer->RenderNormalPass(
                        scene.m_pNormalPass->GetStaticShader(),
                        scene.m_pNormalPass->GetStaticPipeline(),
                        scene.m_pNormalPass->GetSkinnedShader(),
                        scene.m_pNormalPass->GetSkinnedPipeline());
                }
            });

        scene.m_pNormalPass->End(pDevice);

        if (scene.m_pSSAOPass && scene.m_pSSAOPass->GetEnabled())
        {
            scene.m_pSSAOPass->Execute(
                pDevice,
                scene.m_pNormalPass->GetDepthSRVNative(),
                scene.m_pNormalPass->GetNormalSRVNative(),
                vp);

            if (scene.m_pSSAOPass->GetOutputSRVNative())
                pAmbientOcclusionSRV = scene.m_pSSAOPass->GetOutputSRVNative();
        }
    }

    scene.m_Map.UpdateCamera(vp, cameraWorld);
    scene.m_Map.UpdateTransform(scene.m_MapTransform.GetWorldMatrix());
    scene.m_Map.SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
    scene.m_Map.Render();

    scene.m_Cube.UpdateCamera(vp);
    scene.m_Cube.UpdateTransform(scene.m_CubeTransform.GetWorldMatrix());
    scene.m_Cube.Render();

    {
        WINTERS_PROFILE_SCOPE("Champion::Render");
        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, RenderComponent& rc,
                TransformComponent& tf)
            {
                if (rc.bSceneManaged) return;
                if (!rc.bVisible || !rc.pRenderer) return;
                if (!UI::IsRenderableForLocal(scene.m_World, e, localTeam)) return;
                FlushTransformForRender(tf);
                OutputLocalPlayerRenderYawTrace(
                    scene,
                    e,
                    champion,
                    tf,
                    lastSnapshotTick);
                rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
                rc.pRenderer->UpdateCamera(vp, cameraWorld);
                const bool_t bLocalYawTraceTarget =
                    scene.IsNetworkAuthoritativeGameplay() &&
                    e == scene.GetPlayerEntity();
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
                if (scene.m_World.HasComponent<MeshGroupVisibilityComponent>(e)
                    && scene.m_World.GetComponent<MeshGroupVisibilityComponent>(e).bEnabled)
                {
                    const auto& visibility = scene.m_World.GetComponent<MeshGroupVisibilityComponent>(e);
                    rc.pRenderer->RenderWithVisibility(visibility.mask);
                }
                else
                {
                    rc.pRenderer->Render();
                }
            }
        );
    }

    CStructure_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CJungle_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);
    CMinion_Manager::Get()->Render(vp, cameraWorld, pAmbientOcclusionSRV);

    if (!bUseDX12RHI && scene.m_pContactShadowPlane)
    {
        WINTERS_PROFILE_SCOPE("ContactShadow::Render");
        scene.m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent&, RenderComponent& rc, TransformComponent& tf)
            {
                if (rc.bSceneManaged || !rc.bVisible || !rc.pRenderer)
                    return;
                if (!UI::IsRenderableForLocal(scene.m_World, e, localTeam))
                    return;

                FlushTransformForRender(tf);
                scene.m_pContactShadowPlane->SetWorld(
                    BuildContactShadowWorld(tf, 1.22f, 0.055f));
                scene.m_pContactShadowPlane->Render(pDevice, vp);
            });
    }

    if (scene.m_pFogOfWarRenderer)
    {
        scene.m_pFogOfWarRenderer->RenderWorldOverlay(
            pDevice,
            vp,
            Engine::CVisionSystem::FOW_TEX_WORLD_SIZE,
            0.05f);
    }

    UI::CDebugDrawSystem::Render(scene.m_World, &scene, vp);

    RenderAttackRangePreview(scene, vp, pDevice, bUseDX12RHI);

    if (scene.m_pFxMeshSystem && scene.m_pCamera)
        scene.m_pFxMeshSystem->Render(scene.m_World, scene.m_pCamera.get());
    if (scene.m_pFxBeamSystem && scene.m_pCamera)
        scene.m_pFxBeamSystem->Render(scene.m_World, scene.m_pCamera.get());
    if (scene.m_pFxSystem && scene.m_pCamera)
        scene.m_pFxSystem->Render(scene.m_World, scene.m_pCamera.get());

    CGameInstance::Get()->UI_Render_Overlay(vp);
}
