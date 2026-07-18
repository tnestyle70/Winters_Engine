#pragma once
#include "IScene.h"
#include "DynamicCamera.h"
#include "Core/CTransform.h"
#include "Renderer/ModelRenderer.h"
#include "ECS/Systems/TransformSystem.h"
#include "ECS/SystemScheduler.h"
#include "Renderer/PlaneRenderer.h"
#include "Renderer/NormalPass.h"
#include "Renderer/SSAOPass.h"
#include "Renderer/PostFxPass.h"
#include "Renderer/FogOfWarRenderer.h"
#include "Renderer/RHIFxSpriteRenderer.h"
#include "Resource/Texture.h"
#include "RHI/RHIHandles.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "GameObject/SkillDef.h"
#include "GameObject/SkillVisualData.h"
#include "GameObject/ChampionDef.h"
#include "Manager/Navigation/NavGrid.h"
#include "ECS/Systems/NavigationSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/ConcealmentVolumeIndex.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Network/Client/EventApplier.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"

#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "GameObject/FX/WindWallSystem.h"
#include "GameObject/Champion/Yasuo/YasuoProjectileSystem.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Champion/Kalista/KalistaProjectileSystem.h"
#include "GameObject/Champion/Kalista/KalistaRendSystem.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class CClientNetwork;
class CCommandSerializer;
class CReplayPlayer;
class CSnapshotApplier;
struct SnapshotTimelineState;
class CRHISceneRenderer;

namespace Engine
{
    class CMapSurfaceSampler;
}

class CScene_InGame final : public IScene
{
public:
    CScene_InGame();
    explicit CScene_InGame(const wstring_t& replayPath);
    ~CScene_InGame() override;

    bool OnEnter()              override;
    void OnExit()               override;
    void OnUpdate(f32_t dt)     override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender()             override;
    void OnImGui()              override;

public:
    //Entity
    EntityID GetHoveredEntity() const { return m_HoveredEntity; }
    eTeam GetHoveredTeam() const { return m_HoveredTeam; }
    eTeam GetPlayerTeam() const { return m_PlayerTeam; }

    void SetHoveredTarget(EntityID entity, eTeam team);

    bool_t IsPlayerStunned() const
    {
        if (m_PlayerEntity == NULL_ENTITY)
            return false;
        if (m_World.HasComponent<GameplayStateComponent>(m_PlayerEntity))
        {
            const auto& gameplay =
                m_World.GetComponent<GameplayStateComponent>(m_PlayerEntity);
            constexpr u32_t kBlocked =
                kGameplayStateStunnedFlag |
                kGameplayStateAirborneFlag |
                kGameplayStateCannotMoveFlag;
            return (gameplay.stateFlags & kBlocked) != 0u;
        }
        return m_World.HasComponent<StunComponent>(m_PlayerEntity);
    }
    bool_t IsPlayerDead() const;

    // Combat Debug
    bool_t   IsShowCombatDebug() const { return m_bShowCombatDebug; }
    void     SetShowCombatDebug(bool_t b) { m_bShowCombatDebug = b; }
    EntityID GetSylasEntity()    const { return m_SylasEntity; }
    EntityID GetPlayerEntity()   const { return m_PlayerEntity; }

    bool_t HasPlayerRenderer() const { return m_pPlayerRenderer != nullptr; }
    bool_t IsNetworkAuthoritativeGameplay() const { return m_bNetworkAuthoritativeGameplay; }
    bool_t IsReplayPlaybackMode() const { return m_bReplayPlaybackMode; }
    bool_t ShouldRevealAllForPlayback() const { return m_bReplayPlaybackMode; }
    CCommandSerializer* GetCommandSerializer() const { return m_pCommandSerializer.get(); }
    CClientNetwork* GetNetworkView() const { return m_pNetworkView; }
    CSnapshotApplier* GetSnapshotApplier() const { return m_pSnapshotApplier.get(); }
    EntityIdMap* GetEntityIdMap() const { return m_pEntityIdMap.get(); }

    eChampion GetPlayerChampionId() const;

    // FX-10 EffectTuner / IreliaFxPresets
    CWorld& GetWorld() { return m_World; }
    const CWorld& GetWorld() const { return m_World; }
    Engine::CFxStaticMeshRenderer* GetFxMeshRenderer() { return m_pFxMeshRenderer.get(); }
    const char* GetLastActionLabel() const { return m_pLastActionLabel; }
    f32_t    GetLastActionTimer()   const { return m_fLastActionTimer; }
    Vec3     GetSylasTestPos()   const { return m_vSylasTestPos; }
    void     SetSylasTestPos(const Vec3& v) { m_vSylasTestPos = v; }
    f32_t GetChampionHitRadius() const { return m_fChampionHitRadius; }
    void  SetChampionHitRadius(f32_t fRadius) { m_fChampionHitRadius = (fRadius < 0.2f) ? 0.2f : fRadius; }
    f32_t GetChampionHitHeight() const { return m_fChampionHitHeight; }
    void  SetChampionHitHeight(f32_t fHeight) { m_fChampionHitHeight = (fHeight < 0.5f) ? 0.5f : fHeight; }

    // Champion Tuner
    f32_t GetAttackSpeedMul()   const { return m_fAttackSpeedMul; }
    void  SetAttackSpeedMul(f32_t v) { m_fAttackSpeedMul = (v < 0.01f) ? 0.01f : v; }
    f32_t GetGlobalAnimSpeed()  const { return m_fGlobalAnimSpeed; }
    void  SetGlobalAnimSpeed(f32_t v) { m_fGlobalAnimSpeed = (v < 0.01f) ? 0.01f : v; }
    f32_t GetBasicAttackRange() const { return m_fBasicAttackRange; }
    void  SetBasicAttackRange(f32_t v) { m_fBasicAttackRange = (v < 0.1f) ? 0.1f : v; }
    bool_t IsSSAOAvailable() const { return m_pSSAOPass != nullptr; }
    void* GetSSAOOutputSRVNative() const
    {
        return m_pSSAOPass ? m_pSSAOPass->GetOutputSRVNative() : nullptr;
    }
    bool_t GetSSAOEnabled() const { return m_pSSAOPass ? m_pSSAOPass->GetEnabled() : false; }
    void SetSSAOEnabled(bool_t b) { if (m_pSSAOPass) m_pSSAOPass->SetEnabled(b); }
    f32_t GetSSAORadius() const { return m_pSSAOPass ? m_pSSAOPass->GetRadius() : 1.25f; }
    void SetSSAORadius(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetRadius(v); }
    f32_t GetSSAOIntensity() const { return m_pSSAOPass ? m_pSSAOPass->GetIntensity() : 1.5f; }
    void SetSSAOIntensity(f32_t v) { if (m_pSSAOPass) m_pSSAOPass->SetIntensity(v); }
    f32_t GetSSAOThicknessHeuristic() const
    {
        return m_pSSAOPass ? m_pSSAOPass->GetThicknessHeuristic() : 0.05f;
    }
    void SetSSAOThicknessHeuristic(f32_t v)
    {
        if (m_pSSAOPass) m_pSSAOPass->SetThicknessHeuristic(v);
    }

    bool_t IsPostFxAvailable() const { return m_pPostFxPass != nullptr; }
    bool_t GetPostFxEnabled() const
    {
        return m_pPostFxPass ? m_pPostFxPass->GetEnabled() : false;
    }
    void SetPostFxEnabled(bool_t bEnabled)
    {
        if (m_pPostFxPass) m_pPostFxPass->SetEnabled(bEnabled);
    }
    Engine::PostFxParams GetPostFxParams() const
    {
        return m_pPostFxPass
            ? m_pPostFxPass->GetParams()
            : Engine::PostFxParams{};
    }
    void SetPostFxParams(const Engine::PostFxParams& params)
    {
        if (m_pPostFxPass) m_pPostFxPass->SetParams(params);
    }

    //Irelia - Q Dash Tuner Approach
    f32_t GetDashDuration() const { return m_fDashDuration; }
    void SetDashDuration(f32_t v) { m_fDashDuration = (v < 0.05f) ? 0.05f : v; }
    f32_t GetDashMeleeRange() const { return m_fDashMeleeRange; }
    void SetDashMeleeRange(f32_t v) { m_fDashMeleeRange = (v < 0.3f) ? 0.3f : v; }

    // Map Tuner
    bool_t   IsShowMapTuner()    const { return m_bShowMapTuner; }
    void     SetShowMapTuner(bool_t b) { m_bShowMapTuner = b; }
    Vec3     GetMapRotation()    const { return m_vMapRotation; }
    void     SetMapRotation(const Vec3& r) { m_vMapRotation = r; }

    // Render Debug
    bool_t   IsShowRenderDebug() const { return m_bShowRenderDebug; }
    void     SetShowRenderDebug(bool_t b) { m_bShowRenderDebug = b; }
    bool_t   IsDbgShowNavGrid() const { return m_bDbgShowNavGrid; }
    void     SetDbgShowNavGrid(bool_t b) { m_bDbgShowNavGrid = b; }
    bool_t   IsDbgShowPathNavGrid() const { return m_bDbgShowPathNavGrid; }
    void     SetDbgShowPathNavGrid(bool_t b) { m_bDbgShowPathNavGrid = b; }
    bool_t   IsDbgShowStructures() const { return m_bDbgShowStructures; }
    void     SetDbgShowStructures(bool_t b) { m_bDbgShowStructures = b; }
    bool_t   IsDbgShowColliders() const { return m_bDbgShowColliders; }
    void     SetDbgShowColliders(bool_t b) { m_bDbgShowColliders = b; }
    bool_t   IsDbgShowChampions() const { return m_bDbgShowChampions; }
    void     SetDbgShowChampions(bool_t b) { m_bDbgShowChampions = b; }
    bool_t   IsDbgShowMinionMovement() const { return m_bDbgShowMinionMovement; }
    void     SetDbgShowMinionMovement(bool_t b) { m_bDbgShowMinionMovement = b; }
    bool_t   IsDbgShowChampionAIText() const { return m_bDbgShowChampionAIText; }
    void     SetDbgShowChampionAIText(bool_t b) { m_bDbgShowChampionAIText = b; }
    bool_t   IsDbgShowChampionAIRanges() const { return m_bDbgShowChampionAIRanges; }
    void     SetDbgShowChampionAIRanges(bool_t b) { m_bDbgShowChampionAIRanges = b; }
    f32_t    GetDbgNavRadius() const { return m_fDbgNavRadius; }
    void     SetDbgNavRadius(f32_t r) { m_fDbgNavRadius = r; }
    f32_t    GetNavPlayableBaseY() const { return m_fNavPlayableBaseY; }
    void     SetNavPlayableBaseY(f32_t v) { m_fNavPlayableBaseY = v; }
    f32_t    GetNavPlayableHeightBand() const { return m_fNavPlayableHeightBand; }
    void     SetNavPlayableHeightBand(f32_t v) { m_fNavPlayableHeightBand = (v < 0.05f) ? 0.05f : v; }
    f32_t    GetNavMinNormalY() const { return m_fNavMinNormalY; }
    void     SetNavMinNormalY(f32_t v) { m_fNavMinNormalY = v; }
    f32_t    GetNavMaxStepHeight() const { return m_fNavMaxStepHeight; }
    void     SetNavMaxStepHeight(f32_t v) { m_fNavMaxStepHeight = (v < 0.05f) ? 0.05f : v; }
    void     RebuildMapWalkableNavGridForDebug();
    static const char* GetSelectedMapMeshPath();
    static const wchar_t* GetSelectedMapSurfacePath();
    static void ConfigureDefaultMapTransform(CTransform& transform);
    void AdoptPreparedMapSurfaceSampler(unique_ptr<Engine::CMapSurfaceSampler> sampler);
    void AdoptPreparedFxSystem(unique_ptr<CFxSystem> fxSystem);
    void AdoptPreparedFxMeshRenderer(
        unique_ptr<Engine::CFxStaticMeshRenderer> renderer);

    // Debug
    const CNavGrid* GetNavGrid()    const { return m_pNavGrid.get(); }
    const CNavGrid* GetPathNavGrid() const { return m_pPathNavGrid.get(); }
    const CDynamicCamera* GetCameraPtr()  const { return m_pCamera.get(); }

    CTransform* GetPlayerTransformPtr() const { return m_pPlayerTransform; }
    bool_t IsShowAttackRange() const { return m_bShowAttackRange; }
    void SetShowAttackRange(bool_t b) { m_bShowAttackRange = b; }
    CRHIFxSpriteRenderer* GetRHIUtilityPlaneRenderer() const { return m_pRHIUtilityPlaneRenderer.get(); }
    RHITextureHandle GetRHIAttackRangeTexture() const { return m_hRHIAttackRangeTex; }
    CPlaneRenderer* GetAttackRangePlane() const { return m_pAttackRangePlane.get(); }
    CTexture* GetAttackRangeTexture() const { return m_pAttackRangeTex.get(); }

    CPlaneRenderer* GetContactShadowPlane() const { return m_pContactShadowPlane.get(); }

    Vec3 ResolveMouseMapSurfacePos() const;
    bool_t TryResolveWalkableMoveTarget(
        const Vec3& rawTarget,
        Vec3& outTarget,
        Vec3* pOutFirstWaypoint = nullptr) const;
    bool_t IsWalkableMoveSegment(const Vec3& from, const Vec3& to, f32_t radiusWorld = 0.f) const;
    bool_t TryResolveNearestWalkablePosition(const Vec3& rawPos, Vec3& outPos, int32_t maxRadius = 8) const;
    void RebuildClientPathNavGrid();
    void UpdateReplayPlayback(f32_t dt);
    void DrawReplayControlPanel();
    bool_t SendStopReplayRequest();

private:
    bool_t m_bShowAIDebug = false;
    bool_t m_bShowUITuner = false;
    bool_t m_bShowWfxEffectTool = false;
    bool_t m_bShowModelAnimPanel = false;
    bool_t m_bShowAttackSpeedLab = false;
    bool_t m_bShowStructureTuner = false;
    bool_t m_bShowReplayControl = false;
    bool_t m_bShowLegacyInGameDebug = false;
    bool_t m_bShowRenderDebug = false;
    bool_t m_bDbgShowNavGrid = false;
    bool_t m_bDbgShowPathNavGrid = false;
    bool_t m_bDbgShowStructures = false;
    bool_t m_bDbgShowColliders = true;
    bool_t m_bDbgShowChampions = true;
    bool_t m_bDbgShowMinionMovement = false;
    bool_t m_bDbgShowChampionAIText = false;
    bool_t m_bDbgShowChampionAIRanges = false;
    f32_t  m_fDbgNavRadius = 40.f;
    f32_t  m_fNavPlayableBaseY = 0.5f;
    f32_t  m_fNavPlayableHeightBand = 1.25f;
    f32_t  m_fNavMinNormalY = 0.60f;
    f32_t  m_fNavMaxStepHeight = 0.65f;

private:
    unique_ptr<CSystemSchedular> m_pScheduler;

    // Phase Sim-3: Schema / prediction boundary placeholders.
    unique_ptr<EntityIdMap> m_pEntityIdMap;
    unique_ptr<CClientNetwork> m_pNetwork;
    CClientNetwork* m_pNetworkView = nullptr;
    bool_t m_bUsingSharedNetwork = false;
    bool_t m_bNetworkAuthoritativeGameplay = false;
    unique_ptr<CSnapshotApplier> m_pSnapshotApplier;
    unique_ptr<CEventApplier> m_pEventApplier;
    unique_ptr<CCommandSerializer> m_pCommandSerializer;
    bool_t m_bReplayPlaybackMode = false;
    wstring_t m_strReplayPath;
    unique_ptr<CReplayPlayer> m_pReplayPlayer;
    std::string m_strReplayStatus;
    bool_t m_bReplayStopRequested = false;

    // S030: 게임 종료(넥서스 파괴) / 설정창 메인 메뉴 복귀 / 종료 산출물 저장
    bool_t m_bGameEndActive = false;
    bool_t m_bLocalVictory = false;
    bool_t m_bEndOfMatchArtifactsSaved = false;
    bool_t m_bReturnToMainMenuRequested = false;
    bool_t m_bExitReplayToMyInfoRequested = false;
    void PollGameEndAndSettings();
    void SaveEndOfMatchArtifacts(const char* pResultLabel);
    void DrawGameEndOverlay();
    void ChangeToMainMenuScene();
    void ChangeToMyInfoScene();

    // Camera
    unique_ptr<CDynamicCamera> m_pCamera;

    //BlendCache - Plane
    std::unique_ptr<CPlaneRenderer> m_pAttackRangePlane;
    std::unique_ptr<CTexture>       m_pAttackRangeTex;
    std::unique_ptr<CPlaneRenderer> m_pContactShadowPlane;
    std::unique_ptr<CRHIFxSpriteRenderer> m_pRHIUtilityPlaneRenderer;
    std::unique_ptr<CRHISceneRenderer> m_pRHISceneRenderer;
    RHITextureHandle                m_hRHIAttackRangeTex = {};
    std::unique_ptr<CTexture>       m_pWhiteTexture;
    std::unique_ptr<Engine::CNormalPass> m_pNormalPass;
    std::unique_ptr<Engine::CSSAOPass> m_pSSAOPass;
    std::unique_ptr<Engine::CPostFxPass> m_pPostFxPass;
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
    CConcealmentVolumeIndex m_ConcealmentIndex;
    Engine::CVisionSystem* m_pVisionSystem = nullptr;
    const char* m_pPendingEndAnim = nullptr;
    f32_t       m_fEndTransitionTimer = 0.f;
    bool_t      m_bEndTransitionMoving = false;
    SkillVisualData m_LastSkillVisual{};
    bool_t      m_bHasLastSkillVisual = false;

    // Map mesh
    ModelRenderer   m_Map;
    CTransform      m_MapTransform;
    unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;

    // Player Control
    ModelRenderer* m_pPlayerRenderer = nullptr;
    CTransform* m_pPlayerTransform = nullptr;
    CTransform  m_PlayerEntityTransformCache;
    const char* m_pPlayerIdleAnim = nullptr;
    const char* m_pPlayerRunAnim = nullptr;
    std::string m_PlayerIdleAnimStorage{};
    std::string m_PlayerRunAnimStorage{};

    Vec3   m_vPlayerDest{ 0.f, 0.f, 0.f };
    bool   m_bMoving = false;
    bool_t m_bAnnieTibbersCommandMode = false;
    bool_t m_bPingWheelActive = false;
    f32_t  m_fPingWheelCenterX = 0.f;
    f32_t  m_fPingWheelCenterY = 0.f;
    Vec3   m_vPingWheelWorldPos{};
    f32_t m_fAttackSpeedMul = 1.f;
    f32_t m_fGlobalAnimSpeed = 1.f;
    f32_t m_fBasicAttackRange = 1.5f;

    bool_t m_bDashActive = false;
    f32_t m_fDashElapsed = 0.f;
    f32_t m_fDashDuration = 0.3f; //Tuner Available
    f32_t m_fDashMeleeRange = 1.5f;
    Vec3 m_vDashStart = {};
    Vec3 m_vDashEnd = {};
    EntityID m_DashTargetEntity = NULL_ENTITY;

    //Entity Hover
    EntityID m_HoveredEntity = NULL_ENTITY;
    EntityID m_OutlinedHoverEntity = NULL_ENTITY;
    eTeam m_HoveredTeam = eTeam::TEAM_END;
    eTeam m_PlayerTeam = eTeam::Blue;

    f32_t  m_fChampionHitRadius = 1.2f;
    f32_t  m_fChampionHitHeight = 3.f;
    Vec3   m_vSylasTestPos{ 6.f, 3.f, 0.f };

    Vec3   m_vMapRotation{ 0.f, DirectX::XMConvertToRadians(-135.f), 0.f };

    const char* m_pLastActionLabel = "(none)";
    f32_t m_fLastActionTimer = 0.f;
    bool  m_bShowCombatDebug = false;
    bool  m_bShowMapTuner = false;

    struct ActiveSkillRuntime
    {
        bool_t bActive = false;
        eChampion champion = eChampion::END;
        u8_t slot = 0;
        u8_t stage = 1;
        CastSkillCommand command{};
        SkillGameAtomBundle game{};
        SkillVisualData visual{};
        SkillDef legacyHookBridge{};
        f32_t prevFrame = 0.f;
        bool_t bCastFrameFired = false;
        bool_t bRecoveryFrameFired = false;
    };

    ActiveSkillRuntime m_ActiveSkill{};
    bool_t             m_bLogFrameEvents = false;

    bool_t          m_bShowAttackRange = false;

    unique_ptr<CNavGrid> m_pNavGrid;

    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid{};

    void SetEntityHoverOutline(EntityID entity, bool_t bEnabled);
    void UpdateTargeting();
    bool_t TryResolveMinimapClickTarget(Vec3& vOutWorldPos) const;
    bool_t UpdatePingWheelInput(bool_t bImGuiMouse);
    void UpdateCombatInput(bool& outSkipGroundMove);
    void ApplyPlayerDeathInputLock();
    void RenderViegoMistScreenOverlay();
    void RenderDeathScreenOverlay();
    void FirePlayerAction(const char* actionKey);
    bool IsEnemyOfPlayer(EntityID entity);

    void PreemptAction(const char* reasonLabel);

    void UpdateDash(f32_t dt);

    void UpdatePlayerControl(f32_t dt, bool_t bNetworkActive, bool_t bSkipGroundMove, bool_t bActionLockedBefore);
    bool_t IssuePlayerMoveTarget(const Vec3& rawGround, bool_t bNetworkActive, bool_t bSpawnIndicator);
    bool_t PredictLocalMoveYaw(const Vec3& facingTarget, f32_t& outYaw);
    void UpdateChampionStateTimers(f32_t dt);
    void UpdateLocalChampionRuntime(f32_t dt);
    void UpdateLocalPostAnimation();
    bool_t CanResumeBaseAnimation() const;
    bool_t IsLocalActionProtected() const;
    bool_t IsPlayerNetworkMoveInputLocked() const;
    EntityID ResolveOwnedTibbersEntity() const;
    void ResetLocalSkillRuntimeState();
    bool_t TryQueueLocalPassiveDashFromCursor();
    bool_t TriggerNetworkPassiveDashFromAction(u16_t actionId, u32_t actionSeq, bool_t bServerDashLikely);
    bool_t ValidateLocalSkillStart(const SkillDef& def);
    void StartLocalTargetDash(EntityID target);
    void StartLocalUltimateDash(EntityID airborne);
    void StartLocalPassiveDash(const Vec3& vForward);
    void SetLocalActionAnimActive(bool_t active);
    EntityID FindAirborneEnemyNear(const Vec3& origin, f32_t radius);
    void ApplyLocalChampionDamage(EntityID target, f32_t fDamage, const char* pDebugLabel);
    void UpdateLocalTargetDash(f32_t dt);
    void UpdateLocalUltimateSequence(f32_t dt);
    void UpdateLocalPassiveDash(f32_t dt);

    bool DispatchSkillInput(uint8_t slot, u8_t requestedStage = 0);
    eSkillInputActivation ResolveLocalSkillInputActivation(u8_t slot);
    bool_t SendNetworkSkillCommand(u8_t slot, const CastSkillCommand& cmd, u8_t skillStage = 1);
    void ProtectNetworkAttackYaw(CClientNetwork* pNetworkView, u32_t commandSeq, const Vec3& facingTarget);
    void DriveNetworkAttackIntent(bool& outSkipGroundMove);
    void ApplyLocalPrediction(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage = 1);
    bool BuildCastCommand(const SkillTargetSpec& targetSpec, u8_t skillStage, CastSkillCommand& outCmd);

    void RotatePlayerToward(const SkillFacingSpec& facingSpec, u8_t skillStage, const CastSkillCommand& cmd);
    void ClearActiveSkillRuntime();
    void BeginActiveSkillRuntime(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage);

    void Mark_StructuresOnNavGrid();

    void InitializeMapSurfaceSampler(bool_t bMapLoaded, const wchar_t* pSurfaceMeshPath);
    unique_ptr<CNavGrid> CreateMapNavGrid() const;
    void BakeMapWalkableNavGrid();
    bool_t TryProjectToMapSurface(Vec3& ioPos, f32_t fYOffset = 0.f) const;
    void ProjectGameplayActorsToMapSurface();

    void TriggerFlash();
    void UpdateFlashCooldown(f32_t dt);

    void AssignPureECSChampionAlias(eChampion id, EntityID entity);
    void ClearPureECSChampionAlias(EntityID entity);
    void CreateMapEntity();
    void BindPlayerToECSChampion(EntityID entity);
    bool_t ApplyAuthoritativePlayerNetId(NetEntityId netId);
    void ResetLocalControlHandoffState();
    void SyncPlayerEntityTransformFromECS();
    void SyncPlayerEntityTransformToECS();
    void SyncActorHUDStateToEngineUI();
    void SyncStatusPanelStateToEngineUI();
    void SyncWorldHealthBarsToEngineUI();
    void SyncAIResourceStateToEngine();
    void SyncNavigationControlStateToEngine();
    void CaptureNetworkActorInterpolationStarts();
    void BeginNetworkActorInterpolationForSnapshot(u64_t serverTick);
    void ApplyNetworkActorInterpolation(f32_t dt);
    void UpdateNetworkChampionLocomotion(f32_t dt);
    void OnAuthoritativeSnapshot(u64_t serverTick,
        u64_t serverTimeMs,
        u32_t lastAckedCommandSeq,
        u32_t localNetId);
    void OnAuthoritativeCommandResult(
        u64_t serverTick,
        u32_t commandSequence,
        u8_t state,
        u16_t reason,
        u8_t authoritativeSkillSlot,
        u8_t authoritativeSkillStage,
        u64_t stageWindowEndTick);
    void RebaseNetworkTimeline(
        const SnapshotTimelineState& previous,
        const SnapshotTimelineState& next,
        u64_t serverTick);
    void RecordNetworkMovePrediction(u32_t commandSeq,
        const Vec3& vPredictedTarget,
        const Vec3& vFacingDirection);
    void PruneAckedNetworkMovePredictions(u32_t lastAckedCommandSeq);
    bool_t IsNetworkChampionMoving(EntityID entity) const;

    CWorld   m_World;

    EntityID m_SylasEntity = NULL_ENTITY;
    EntityID m_FioraEntity = NULL_ENTITY;
    EntityID m_JaxEntity = NULL_ENTITY;
    EntityID m_AnnieEntity = NULL_ENTITY;
    EntityID m_AsheEntity = NULL_ENTITY;
    EntityID m_YoneEntity = NULL_ENTITY;

    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_ChampionRenderers{};

    // 맵 앰비언트 프롭(새/오리)은 CAmbientProp_Manager(owner)가 소유한다. (Stage 2)
    std::unordered_map<EntityID, Vec3>   m_NetworkChampionPrevPos{};
    std::unordered_map<EntityID, f32_t>  m_NetworkChampionMoveGraceSec{};
    std::unordered_map<EntityID, bool_t> m_NetworkChampionMoving{};
    bool_t m_bPlayerVoiceMoveInitialized = false;
    f32_t  m_fPlayerVoiceMoveDelayRemainingSec = 0.f;
    u32_t  m_uPlayerVoiceSelectionCounter = 0u;

    struct NetworkSnapshotInterpState
    {
        Vec3 vPendingStartPos{};
        Vec3 vPendingStartRot{};
        Vec3 vStartPos{};
        Vec3 vStartRot{};
        Vec3 vTargetPos{};
        Vec3 vTargetRot{};
        f32_t fElapsedSec = 0.f;
        f32_t fDurationSec = 0.06f;
        u64_t uSourceServerTick = 0;
        bool_t bActive = false;
        bool_t bHasPendingStart = false;
    };
    std::unordered_map<EntityID, NetworkSnapshotInterpState> m_NetworkActorInterpStates{};
    u64_t  m_uNetworkActorInterpSnapshotTick = 0;
    bool_t m_bNetworkActorInterpolationEnabled = true;
    struct NetworkMovePrediction
    {
        u32_t commandSeq = 0;
        Vec3 vPredictedTarget{};
        Vec3 vFacingDirection{};
        f32_t fAgeSec = 0.f;
    };
    std::deque<NetworkMovePrediction> m_NetworkMovePredictions{};
    u32_t m_uLastAckedMovePredictionSeq = 0;
    u32_t m_uLastSkillCommandResultSeq[5]{};
    f32_t m_fLocalCorrectionBlendSec = 0.08f;
    struct NetworkActionAnimationState
    {
        u32_t actionSeq = 0;
        u32_t baseSeq = 0;
        u16_t actionId = 0;
        f32_t actionRemainingSec = 0.f;
        f32_t transitionRemainingSec = 0.f;
        f32_t transitionDurationSec = 0.f;
        bool_t bActionActive = false;
        bool_t bTransitionActive = false;
        bool_t bLoopAction = false;
        bool_t bBaseAnimationPending = false;
        bool_t bBaseAnimationInitialized = false;
        bool_t bDesiredMoving = false;
        bool_t bTransitionMoving = false;
        bool_t bPassiveDashTriggered = false;
        f32_t passiveDashInputGraceSec = 0.f;
        std::string transitionIdleAnim{};
        std::string transitionRunAnim{};
    };
    std::unordered_map<EntityID, NetworkActionAnimationState> m_NetworkActionAnimStates{};
    EntityID m_PlayerEntity = NULL_ENTITY;
    EntityID m_MapEntity = NULL_ENTITY;

    void CreateECSEntities();
    EntityID SpawnChampionEntity(eChampion champion, eTeam team);
    void InitializeNetworkSession();
    bool_t PumpNetwork();
    void ReplayLastNetworkHelloIfShared();

    std::unique_ptr<CFxSystem>                       m_pFxSystem;
    std::unique_ptr<CFxBeamSystem>                   m_pFxBeamSystem;
    std::unique_ptr<Engine::CFxStaticMeshRenderer>   m_pFxMeshRenderer;
    std::unique_ptr<CFxMeshSystem>                   m_pFxMeshSystem;
    std::unique_ptr<CIreliaBladeSystem>              m_pIreliaBladeSystem;


    f32_t m_fFlashRange = 4.25f;
    f32_t m_fFlashCooldown = 300.f;
    f32_t m_fFlashCooldownLeft = 0.f;

    //Kalista passive Sentinel's March
    bool_t m_bKalistaPassiveDashActive = false;
    f32_t  m_fKalistaPassiveDashElapsed = 0.f;
    Vec3   m_vKalistaPassiveDashStart{};
    Vec3   m_vKalistaPassiveDashEnd{};
    bool_t m_bKalistaPassiveDashAnimActive = false;
    u32_t  m_uKalistaLastPassiveDashActionSeq = 0;
    bool_t m_bKalistaPassiveDashMoveCommandPending = false;
    bool_t m_bKalistaPassiveDashTriggerAfterMove = false;
    u16_t  m_uKalistaPassiveDashTriggerAnimId = 0;
    u32_t  m_uKalistaPassiveDashTriggerActionSeq = 0;
    Vec3 m_vKalistaPassiveDashFaceDir{};
    bool_t m_bKalistaPassiveDashHasFaceDir = false;

    bool_t   m_bYasuoDashActive = false;
    f32_t    m_fYasuoDashElapsed = 0.f;
    Vec3     m_vYasuoDashStart{};
    Vec3     m_vYasuoDashEnd{};
    EntityID m_YasuoDashTargetEntity = NULL_ENTITY;

    bool_t   m_bYasuoRActive = false;
    f32_t    m_fYasuoRElapsed = 0.f;
    EntityID m_YasuoRTarget = NULL_ENTITY;
    i32_t    m_iYasuoRHitsFired = 0;
    f32_t    m_fYasuoRPrevHitTime = 0.f;

    std::unique_ptr<CWindWallSystem> m_pWindWallSystem;
    std::unique_ptr<CYasuoProjectileSystem> m_pYasuoProjectileSystem;
    std::unique_ptr<CPendingHitSystem> m_pPendingHitSystem;
    std::unique_ptr<CKalistaProjectileSystem> m_pKalistaProjectileSystem;
    std::unique_ptr<CKalistaRendSystem> m_pKalistaRendSystem;

    private:
        //PlayerTransform Adapter
        bool HasPlayerTransform() const; //guard ?≪닔
        Vec3 GetPlayerPosition() const;
        void SetPlayerPosition(const Vec3& v);
        f32_t GetPlayerYaw() const;
        void SetPlayerYaw(f32_t yaw);
        Vec3 GetPlayerForward() const;

public:
    void  SetKalistaPassiveDashFaceDir(const Vec3& v)
    {
        m_vKalistaPassiveDashFaceDir = v;
        m_bKalistaPassiveDashHasFaceDir = v.x != 0.f || v.z != 0.f;
    }
};
