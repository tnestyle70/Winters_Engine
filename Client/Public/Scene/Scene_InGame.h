#pragma once
#include "IScene.h"
#include "Renderer/CubeRenderer.h"
#include "DynamicCamera.h"
#include "Core/CTransform.h"
#include "Renderer/ModelRenderer.h"
#include "ECS/Systems/TransformSystem.h"
#include "ECS/SystemScheduler.h"
#include "Renderer/PlaneRenderer.h"
#include "Renderer/NormalPass.h"
#include "Renderer/SSAOPass.h"
#include "Renderer/FogOfWarRenderer.h"
#include "Renderer/RHIFxSpriteRenderer.h"
#include "Resource/Texture.h"
#include "RHI/RHIHandles.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/SkillDef.h"
#include "GameObject/ChampionDef.h"
#include "Manager/Navigation/NavGrid.h"
#include "ECS/Systems/NavigationSystem.h"
#include "ECS/Systems/VisionSystem.h"
#include "ECS/BushVolumeIndex.h"
#include "GameContext.h"
#include "Network/Client/EventApplier.h"

#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/UltWaveSystem.h"
#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "Renderer/FxStaticMeshRenderer.h"

#include "GameObject/FX/WindWallSystem.h"
#include "GameObject/Champion/Yasuo/YasuoProjectileSystem.h"
#include "GameObject/Champion/Yasuo/PendingHitSystem.h"
#include "GameObject/Champion/Yasuo/Yasuo_Tuning.h"
#include "GameObject/Champion/Kalista/KalistaProjectileSystem.h"
#include "GameObject/Champion/Kalista/KalistaRendSystem.h"
#include "GameObject/Champion/Kalista/KalistaFxPresets.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <string>
#include <unordered_map>

class CClientNetwork;
class CCommandSerializer;
class CSnapshotApplier;
class CInGameBootstrapBridge;
class CInGameChampionStateBridge;
class CInGameCombatInputBridge;
class CInGameLifecycleBridge;
class CInGamePlayerControlBridge;
class CInGamePlayerTransformBridge;
class CInGameRenderBridge;
class CInGameSkillDispatchBridge;

namespace Engine
{
    class CGameplayCollisionSystem;
    class CMinionSeparationSystem;
    class CMapSurfaceSampler;
}

class CScene_InGame final : public IScene
{
    friend class CInGameBootstrapBridge;
    friend class CInGameChampionStateBridge;
    friend class CInGameCombatInputBridge;
    friend class CInGameLifecycleBridge;
    friend class CInGamePlayerControlBridge;
    friend class CInGamePlayerTransformBridge;
    friend class CInGameRenderBridge;
    friend class CInGameSkillDispatchBridge;

public:
    CScene_InGame();
    ~CScene_InGame() override;

    bool OnEnter()              override;
    void OnExit()               override;
    void OnUpdate(f32_t dt)     override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender()             override;
    void OnSnapshot(const u8_t* bytes, u32_t len);
    void OnImGui()              override;

public:
    //Entity
    EntityID GetHoveredEntity() const { return m_HoveredEntity; }
    eTeam GetHoveredTeam() const { return m_HoveredTeam; }
    eTeam GetPlayerTeam() const { return m_PlayerTeam; }

    void SetHoveredTarget(EntityID entity, eTeam team);

    bool_t IsPlayerStunned() const
    {
        return m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<StunComponent>(m_PlayerEntity);
    }

    // Combat Debug
    bool_t   IsShowCombatDebug() const { return m_bShowCombatDebug; }
    void     SetShowCombatDebug(bool_t b) { m_bShowCombatDebug = b; }
    EntityID GetSylasEntity()    const { return m_SylasEntity; }
    EntityID GetPlayerEntity()   const { return m_PlayerEntity; }

    bool_t HasPlayerRenderer() const { return m_pPlayerRenderer != nullptr; }
    bool_t IsNetworkAuthoritativeGameplay() const { return m_bNetworkAuthoritativeGameplay; }
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
    Engine::CGameplayCollisionSystem* GetGameplayCollisionSystem() const { return m_pGameplayCollisionSystem.get(); }
    Engine::CMinionSeparationSystem* GetMinionSeparationSystem() const { return m_pMinionSeparationSystem; }

private:
    bool_t m_bShowAIDebug = true;
    bool_t m_bShowUITuner = true;
    bool_t m_bShowLegacyInGameDebug = false;
    bool_t m_bShowRenderDebug = true;
    bool_t m_bDbgShowNavGrid = true;
    bool_t m_bDbgShowPathNavGrid = false;
    bool_t m_bDbgShowStructures = true;
    bool_t m_bDbgShowColliders = true;
    bool_t m_bDbgShowChampions = true;
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
    DeterministicRng m_localRng{ 0x9E3779B97F4A7C15ull };

    // Cube (placeholder)
    CubeRenderer    m_Cube;
    CTransform      m_CubeTransform;
    f32_t           m_fElapsed = 0.f;

    // Camera
    unique_ptr<CDynamicCamera> m_pCamera;

    //BlendCache - Plane
    std::unique_ptr<CPlaneRenderer> m_pAttackRangePlane;
    std::unique_ptr<CTexture>       m_pAttackRangeTex;
    std::unique_ptr<CPlaneRenderer> m_pContactShadowPlane;
    std::unique_ptr<CRHIFxSpriteRenderer> m_pRHIUtilityPlaneRenderer;
    RHITextureHandle                m_hRHIAttackRangeTex = {};
    std::unique_ptr<CTexture>       m_pWhiteTexture;
    std::unique_ptr<Engine::CNormalPass> m_pNormalPass;
    std::unique_ptr<Engine::CSSAOPass> m_pSSAOPass;
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
    CBushVolumeIndex m_BushIndex;
    Engine::CVisionSystem* m_pVisionSystem = nullptr;
    unique_ptr<Engine::CGameplayCollisionSystem> m_pGameplayCollisionSystem;
    Engine::CMinionSeparationSystem* m_pMinionSeparationSystem = nullptr;
    const char* m_pPendingEndAnim = nullptr;
    f32_t       m_fEndTransitionTimer = 0.f;
    const SkillDef* m_pLastDispatchedSkill = nullptr;

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

    SkillDef        m_ActiveSkillDefStorage{};
    CastSkillCommand m_ActiveSkillCommandStorage{};
    const SkillDef* m_pActiveSkillDef = nullptr;
    f32_t           m_fActivePrevFrame = 0.f;
    bool_t          m_bLogFrameEvents = false;

    bool_t          m_bShowAttackRange = false;

    unique_ptr<CNavGrid> m_pNavGrid;

    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid{};

    void SetEntityHoverOutline(EntityID entity, bool_t bEnabled);
    void UpdateTargeting();
    void UpdateCombatInput(bool& outSkipGroundMove);
    void FirePlayerAction(const char* actionKey);
    bool IsEnemyOfPlayer(EntityID entity);

    void PreemptAction(const char* reasonLabel);

    void UpdateDash(f32_t dt);

    bool DispatchSkillInput(uint8_t slot);
    void ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def, u8_t skillStage = 1);
    bool BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd);

    void RotatePlayerToward(eRotateMode mode, const CastSkillCommand& cmd);

    void Mark_StructuresOnNavGrid();

    void InitializeMapSurfaceSampler(bool_t bMapLoaded);
    unique_ptr<CNavGrid> CreateMapNavGrid() const;
    void BakeMapWalkableNavGrid();
    bool_t TryProjectToMapSurface(Vec3& ioPos, f32_t fYOffset = 0.f) const;
    void ProjectGameplayActorsToMapSurface();

    void TriggerFlash();
    void UpdateFlashCooldown(f32_t dt);

    void AssignPureECSChampionAlias(eChampion id, EntityID entity);
    void CreateMapEntity();
    void BindPlayerToECSChampion(EntityID entity);
    void SyncPlayerEntityTransformFromECS();
    void SyncPlayerEntityTransformToECS();
    void CaptureNetworkActorInterpolationStarts();
    void BeginNetworkActorInterpolationForSnapshot(u64_t serverTick);
    void ApplyNetworkActorInterpolation(f32_t dt);
    void UpdateNetworkChampionLocomotion(f32_t dt);
    bool_t IsNetworkChampionMoving(EntityID entity) const;

    CWorld   m_World;

    EntityID m_SylasEntity = NULL_ENTITY;
    EntityID m_FioraEntity = NULL_ENTITY;
    EntityID m_JaxEntity = NULL_ENTITY;
    EntityID m_AnnieEntity = NULL_ENTITY;
    EntityID m_AsheEntity = NULL_ENTITY;
    EntityID m_YoneEntity = NULL_ENTITY;

    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_ChampionRenderers{};
    std::unordered_map<EntityID, Vec3>   m_NetworkChampionPrevPos{};
    std::unordered_map<EntityID, f32_t>  m_NetworkChampionMoveGraceSec{};
    std::unordered_map<EntityID, bool_t> m_NetworkChampionMoving{};

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
    struct NetworkActionAnimationState
    {
        u32_t actionSeq = 0;
        u32_t baseSeq = 0;
        u16_t animId = 0;
        f32_t actionRemainingSec = 0.f;
        f32_t transitionRemainingSec = 0.f;
        f32_t transitionDurationSec = 0.f;
        bool_t bActionActive = false;
        bool_t bTransitionActive = false;
        bool_t bBaseAnimationPending = false;
        bool_t bBaseAnimationInitialized = false;
        bool_t bDesiredMoving = false;
        bool_t bPassiveDashTriggered = false;
        f32_t passiveDashInputGraceSec = 0.f;
        std::string transitionIdleAnim{};
        std::string transitionRunAnim{};
    };
    std::unordered_map<EntityID, NetworkActionAnimationState> m_NetworkActionAnimStates{};
    EntityID m_PlayerEntity = NULL_ENTITY;
    EntityID m_MapEntity = NULL_ENTITY;

    void CreateECSEntities();

    std::unique_ptr<CFxSystem>                       m_pFxSystem;
    std::unique_ptr<CFxBeamSystem>                   m_pFxBeamSystem;
    std::unique_ptr<Engine::CFxStaticMeshRenderer>   m_pFxMeshRenderer;
    std::unique_ptr<CFxMeshSystem>                   m_pFxMeshSystem;
    std::unique_ptr<CIreliaBladeSystem>              m_pIreliaBladeSystem;
    std::unique_ptr<CUltWaveSystem>                  m_pUltWaveSystem;


    bool_t m_bCastFrameFired = false;
    bool_t m_bRecoveryFrameFired = false;

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
    f32_t GetBladeTravelSpeed() const { return Irelia::GetTuning().bladeTravelSpeed; }
    f32_t GetBladeStunSec()     const { return Irelia::GetTuning().bladeStunSec; }
    f32_t GetBladeScale()       const { return Irelia::GetTuning().bladeScale; }
    f32_t GetBladePitch()       const { return Irelia::GetTuning().bladePitch; }
    f32_t GetBladeYaw()         const { return Irelia::GetTuning().bladeYaw; }
    f32_t GetBladeRoll()        const { return Irelia::GetTuning().bladeRoll; }
    f32_t GetBeamScaleAxis()    const { return Irelia::GetTuning().beamScaleAxis; }
    f32_t GetBeamGirth()        const { return Irelia::GetTuning().beamGirth; }
    f32_t GetBeamMeshBaseScale() const { return Irelia::GetTuning().beamMeshBaseScale; }
    f32_t GetBeamYawOffset()    const { return Irelia::GetTuning().beamYawOffset; }
    f32_t GetWaveLength()       const { return Irelia::GetTuning().waveLength; }
    f32_t GetWaveWidth()        const { return Irelia::GetTuning().waveWidth; }
    f32_t GetWaveSpeed()        const { return Irelia::GetTuning().waveSpeed; }
    f32_t GetWaveMaxDist()      const { return Irelia::GetTuning().waveMaxDist; }
    f32_t GetWaveDamage()       const { return Irelia::GetTuning().waveDamage; }
    f32_t GetRFxWidth()         const { return Irelia::GetTuning().rFxWidth; }
    f32_t GetRFxHeight()        const { return Irelia::GetTuning().rFxHeight; }
    f32_t GetRFxYOffset()       const { return Irelia::GetTuning().rFxYOffset; }
    f32_t GetRFxFwdOffset()     const { return Irelia::GetTuning().rFxFwdOffset; }
    f32_t GetRFxYawOffset()     const { return Irelia::GetTuning().rFxYawOffset; }
    void  SetBladeTravelSpeed(f32_t v) { Irelia::GetTuning().bladeTravelSpeed = v; }
    void  SetBladeStunSec(f32_t v) { Irelia::GetTuning().bladeStunSec = v; }
    void  SetBladeScale(f32_t v) { Irelia::GetTuning().bladeScale = (v < 0.001f) ? 0.001f : v; }
    void  SetBladePitch(f32_t v) { Irelia::GetTuning().bladePitch = v; }
    void  SetBladeYaw(f32_t v) { Irelia::GetTuning().bladeYaw = v; }
    void  SetBladeRoll(f32_t v) { Irelia::GetTuning().bladeRoll = v; }
    f32_t GetBladeSpinSpeed() const { return Irelia::GetTuning().bladeSpinSpeed; }
    void  SetBladeSpinSpeed(f32_t v) { Irelia::GetTuning().bladeSpinSpeed = v; }
    Vec4  GetEBladeColor() const { return Irelia::GetTuning().eBladeColor; }
    void  SetEBladeColor(const Vec4& v) { Irelia::GetTuning().eBladeColor = v; }
    Vec4  GetEGroundGlowColor() const { return Irelia::GetTuning().eGroundGlowColor; }
    void  SetEGroundGlowColor(const Vec4& v) { Irelia::GetTuning().eGroundGlowColor = v; }
    Vec4  GetEGroundCoreColor() const { return Irelia::GetTuning().eGroundCoreColor; }
    void  SetEGroundCoreColor(const Vec4& v) { Irelia::GetTuning().eGroundCoreColor = v; }
    Vec4  GetECloseSparkColor() const { return Irelia::GetTuning().eCloseSparkColor; }
    void  SetECloseSparkColor(const Vec4& v) { Irelia::GetTuning().eCloseSparkColor = v; }
    Vec4  GetECloseBeamColor() const { return Irelia::GetTuning().eCloseBeamColor; }
    void  SetECloseBeamColor(const Vec4& v) { Irelia::GetTuning().eCloseBeamColor = v; }
    f32_t GetEGroundYOffset() const { return Irelia::GetTuning().eGroundYOffset; }
    void  SetEGroundYOffset(f32_t v) { Irelia::GetTuning().eGroundYOffset = v; }
    f32_t GetEGroundGlowSize() const { return Irelia::GetTuning().eGroundGlowSize; }
    void  SetEGroundGlowSize(f32_t v) { Irelia::GetTuning().eGroundGlowSize = (v < 0.1f) ? 0.1f : v; }
    f32_t GetEGroundCoreSize() const { return Irelia::GetTuning().eGroundCoreSize; }
    void  SetEGroundCoreSize(f32_t v) { Irelia::GetTuning().eGroundCoreSize = (v < 0.1f) ? 0.1f : v; }
    f32_t GetEGroundSpinSpeed() const { return Irelia::GetTuning().eGroundSpinSpeed; }
    void  SetEGroundSpinSpeed(f32_t v) { Irelia::GetTuning().eGroundSpinSpeed = v; }
    f32_t GetECloseSparkSize() const { return Irelia::GetTuning().eCloseSparkSize; }
    void  SetECloseSparkSize(f32_t v) { Irelia::GetTuning().eCloseSparkSize = (v < 0.1f) ? 0.1f : v; }
    f32_t GetECloseBeamWidth() const { return Irelia::GetTuning().eCloseBeamWidth; }
    void  SetECloseBeamWidth(f32_t v) { Irelia::GetTuning().eCloseBeamWidth = (v < 0.05f) ? 0.05f : v; }
    void  SetBeamScaleAxis(f32_t v) { Irelia::GetTuning().beamScaleAxis = (v < 0.1f) ? 0.1f : v; }
    void  SetBeamGirth(f32_t v) { Irelia::GetTuning().beamGirth = (v < 0.05f) ? 0.05f : v; }
    void  SetBeamMeshBaseScale(f32_t v) { Irelia::GetTuning().beamMeshBaseScale = (v < 0.001f) ? 0.001f : v; }
    void  SetBeamYawOffset(f32_t v) { Irelia::GetTuning().beamYawOffset = v; }
    void  SetWaveLength(f32_t v) { Irelia::GetTuning().waveLength = v; }
    void  SetWaveWidth(f32_t v) { Irelia::GetTuning().waveWidth = v; }
    void  SetWaveSpeed(f32_t v) { Irelia::GetTuning().waveSpeed = v; }
    void  SetWaveMaxDist(f32_t v) { Irelia::GetTuning().waveMaxDist = v; }
    void  SetWaveDamage(f32_t v) { Irelia::GetTuning().waveDamage = v; }
    void  SetRFxWidth(f32_t v) { Irelia::GetTuning().rFxWidth = (v < 0.5f) ? 0.5f : v; }
    void  SetRFxHeight(f32_t v) { Irelia::GetTuning().rFxHeight = (v < 0.5f) ? 0.5f : v; }
    void  SetRFxYOffset(f32_t v) { Irelia::GetTuning().rFxYOffset = v; }
    void  SetRFxFwdOffset(f32_t v) { Irelia::GetTuning().rFxFwdOffset = v; }
    void  SetRFxYawOffset(f32_t v) { Irelia::GetTuning().rFxYawOffset = v; }

    f32_t GetWLayerLifetime()    const { return Irelia::GetTuning().wLayerLifetime; }
    void  SetWLayerLifetime(f32_t v) { Irelia::GetTuning().wLayerLifetime = (v < 0.05f) ? 0.05f : v; }
    f32_t GetWLayerSize()        const { return Irelia::GetTuning().wLayerSize; }
    void  SetWLayerSize(f32_t v) { Irelia::GetTuning().wLayerSize = (v < 0.1f) ? 0.1f : v; }
    Vec4  GetWLayerBladesColor() const { return Irelia::GetTuning().wLayerBladesColor; }
    void  SetWLayerBladesColor(const Vec4& v) { Irelia::GetTuning().wLayerBladesColor = v; }
    Vec4  GetWLayerGlowColor()   const { return Irelia::GetTuning().wLayerGlowColor; }
    void  SetWLayerGlowColor(const Vec4& v) { Irelia::GetTuning().wLayerGlowColor = v; }

    bool  GetRTriangleMode()    const { return Irelia::GetTuning().bRTriangleMode; }
    void  SetRTriangleMode(bool b) { Irelia::GetTuning().bRTriangleMode = b; }
    f32_t GetRTipBoost()        const { return Irelia::GetTuning().rTipBoost; }
    void  SetRTipBoost(f32_t v) { Irelia::GetTuning().rTipBoost = (v < 0.f) ? 0.f : v; }
    f32_t GetRSideShrink()      const { return Irelia::GetTuning().rSideShrink; }
    void  SetRSideShrink(f32_t v) {
        Irelia::GetTuning().rSideShrink = (v < 0.f) ? 0.f : (v > 0.9f ? 0.9f : v);
    }

    f32_t GetYasuoQSpeed() const { return Yasuo::GetTuning().qSpeed; }
    void  SetYasuoQSpeed(f32_t v) { Yasuo::GetTuning().qSpeed = (v < 5.f) ? 5.f : v; }
    f32_t GetYasuoQLifetime() const { return Yasuo::GetTuning().qLifetime; }
    void  SetYasuoQLifetime(f32_t v) { Yasuo::GetTuning().qLifetime = (v < 0.1f) ? 0.1f : v; }
    f32_t GetYasuoQTornadoSpeed() const { return Yasuo::GetTuning().qTornadoSpeed; }
    void  SetYasuoQTornadoSpeed(f32_t v) { Yasuo::GetTuning().qTornadoSpeed = (v < 1.f) ? 1.f : v; }
    f32_t GetYasuoQTornadoLifetime() const { return Yasuo::GetTuning().qTornadoLifetime; }
    void  SetYasuoQTornadoLifetime(f32_t v) { Yasuo::GetTuning().qTornadoLifetime = (v < 0.1f) ? 0.1f : v; }
    f32_t GetYasuoQTornadoScale() const { return Yasuo::GetTuning().qTornadoScale; }
    void  SetYasuoQTornadoScale(f32_t v) { Yasuo::GetTuning().qTornadoScale = (v < 0.001f) ? 0.001f : v; }
    f32_t GetYasuoWLifetime() const { return Yasuo::GetTuning().wLifetime; }
    void  SetYasuoWLifetime(f32_t v) { Yasuo::GetTuning().wLifetime = (v < 0.5f) ? 0.5f : v; }
    f32_t GetYasuoWWidth() const { return Yasuo::GetTuning().wWidth; }
    void  SetYasuoWWidth(f32_t v) { Yasuo::GetTuning().wWidth = (v < 0.5f) ? 0.5f : v; }
    f32_t GetYasuoWHeight() const { return Yasuo::GetTuning().wHeight; }
    void  SetYasuoWHeight(f32_t v) { Yasuo::GetTuning().wHeight = (v < 0.1f) ? 0.1f : v; }
    f32_t GetYasuoEDashDuration() const { return Yasuo::GetTuning().eDashDuration; }
    void  SetYasuoEDashDuration(f32_t v) { Yasuo::GetTuning().eDashDuration = (v < 0.05f) ? 0.05f : v; }
    f32_t GetYasuoRSearchRadius() const { return Yasuo::GetTuning().rSearchRadius; }
    void  SetYasuoRSearchRadius(f32_t v) { Yasuo::GetTuning().rSearchRadius = (v < 1.f) ? 1.f : v; }
    f32_t GetYasuoRSequenceDuration() const { return Yasuo::GetTuning().rSequenceDuration; }
    void  SetYasuoRSequenceDuration(f32_t v) { Yasuo::GetTuning().rSequenceDuration = (v < 0.1f) ? 0.1f : v; }

    //Yasuo Damage
    f32_t GetYasuoQDamage() const { return Yasuo::GetTuning().qDamage; }
    void  SetYasuoQDamage(f32_t v) { Yasuo::GetTuning().qDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoQTornadoDamage() const { return Yasuo::GetTuning().qTornadoDamage; }
    void  SetYasuoQTornadoDamage(f32_t v) { Yasuo::GetTuning().qTornadoDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoQTornadoStunSec() const { return Yasuo::GetTuning().qTornadoStunSec; }
    void  SetYasuoQTornadoStunSec(f32_t v) { Yasuo::GetTuning().qTornadoStunSec = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoEDamage() const { return Yasuo::GetTuning().eDamage; }
    void  SetYasuoEDamage(f32_t v) { Yasuo::GetTuning().eDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoRPerHitDamage() const { return Yasuo::GetTuning().rPerHitDamage; }
    void  SetYasuoRPerHitDamage(f32_t v) { Yasuo::GetTuning().rPerHitDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoRHitInterval() const { return Yasuo::GetTuning().rHitInterval; }
    void  SetYasuoRHitInterval(f32_t v) { Yasuo::GetTuning().rHitInterval = (v < 0.05f) ? 0.05f : v; }
    Vec4  GetYasuoQTornadoColor() const { return Yasuo::GetTuning().qTornadoColor; }
    void  SetYasuoQTornadoColor(const Vec4& v) { Yasuo::GetTuning().qTornadoColor = v; }
    f32_t GetYasuoWMeshScale() const { return Yasuo::GetTuning().wMeshScale; }
    void  SetYasuoWMeshScale(f32_t v) { Yasuo::GetTuning().wMeshScale = (v < 0.001f) ? 0.001f : v; }
    f32_t GetFlashRange() const { return m_fFlashRange; }
    void  SetFlashRange(f32_t v) { m_fFlashRange = (v < 0.5f) ? 0.5f : v; }
    f32_t GetFlashCooldown() const { return m_fFlashCooldown; }
    void  SetFlashCooldown(f32_t v) { m_fFlashCooldown = (v < 1.f) ? 1.f : v; }
    f32_t GetFlashCooldownLeft() const { return m_fFlashCooldownLeft; }
    f32_t GetYasuoQHitDelay() const { return Yasuo::GetTuning().qHitDelay; }
    void  SetYasuoQHitDelay(f32_t v) { Yasuo::GetTuning().qHitDelay = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoEQDelay() const { return Yasuo::GetTuning().eqDelay; }
    void  SetYasuoEQDelay(f32_t v) { Yasuo::GetTuning().eqDelay = (v < 0.f) ? 0.f : v; }
    f32_t GetYasuoEQRadius() const { return Yasuo::GetTuning().eqRadius; }
    void  SetYasuoEQRadius(f32_t v) { Yasuo::GetTuning().eqRadius = (v < 0.5f) ? 0.5f : v; }
    f32_t GetYasuoEQDamage() const { return Yasuo::GetTuning().eqDamage; }
    void  SetYasuoEQDamage(f32_t v) { Yasuo::GetTuning().eqDamage = (v < 0.f) ? 0.f : v; }

    f32_t GetKalistaQSpeed() const { return Kalista::GetTuning().qSpeed; }
    void  SetKalistaQSpeed(f32_t v) { Kalista::GetTuning().qSpeed = (v < 5.f) ? 5.f : v; }
    f32_t GetKalistaQMaxDist() const { return Kalista::GetTuning().qMaxDist; }
    void  SetKalistaQMaxDist(f32_t v) { Kalista::GetTuning().qMaxDist = (v < 1.f) ? 1.f : v; }
    f32_t GetKalistaQRadius() const { return Kalista::GetTuning().qRadius; }
    void  SetKalistaQRadius(f32_t v) { Kalista::GetTuning().qRadius = (v < 0.1f) ? 0.1f : v; }
    f32_t GetKalistaQDamage() const { return Kalista::GetTuning().qDamage; }
    void  SetKalistaQDamage(f32_t v) { Kalista::GetTuning().qDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetKalistaBAFlySpearScale() const { return Kalista::GetTuning().baFlySpearScale; }
    void  SetKalistaBAFlySpearScale(f32_t v) { Kalista::GetTuning().baFlySpearScale = (v < 0.0001f) ? 0.0001f : v; }
    f32_t GetKalistaBAStuckSpearScale() const { return Kalista::GetTuning().baStuckSpearScale; }
    void  SetKalistaBAStuckSpearScale(f32_t v) { Kalista::GetTuning().baStuckSpearScale = (v < 0.0001f) ? 0.0001f : v; }
    f32_t GetKalistaQFlySpearScale() const { return Kalista::GetTuning().qFlySpearScale; }
    void  SetKalistaQFlySpearScale(f32_t v) { Kalista::GetTuning().qFlySpearScale = (v < 0.0001f) ? 0.0001f : v; }
    f32_t GetKalistaQStuckSpearScale() const { return Kalista::GetTuning().qStuckSpearScale; }
    void  SetKalistaQStuckSpearScale(f32_t v) { Kalista::GetTuning().qStuckSpearScale = (v < 0.0001f) ? 0.0001f : v; }
    f32_t GetKalistaPassiveDashDist() const { return Kalista::GetTuning().passiveDashDist; }
    void  SetKalistaPassiveDashDist(f32_t v) { Kalista::GetTuning().passiveDashDist = (v < 0.1f) ? 0.1f : v; }
    f32_t GetKalistaPassiveDashDuration() const { return Kalista::GetTuning().passiveDashDuration; }
    void  SetKalistaPassiveDashDuration(f32_t v) { Kalista::GetTuning().passiveDashDuration = (v < 0.03f) ? 0.03f : v; }
    f32_t GetKalistaPassiveDashAnimSpeed() const { return Kalista::GetTuning().passiveDashAnimSpeed; }
    void  SetKalistaPassiveDashAnimSpeed(f32_t v) { Kalista::GetTuning().passiveDashAnimSpeed = (v < 0.1f) ? 0.1f : v; }
    f32_t GetKalistaPassiveDashInputGrace() const { return Kalista::GetTuning().passiveDashInputGraceSec; }
    void  SetKalistaPassiveDashInputGrace(f32_t v) { Kalista::GetTuning().passiveDashInputGraceSec = (v < 0.f) ? 0.f : v; }
    void  SetKalistaPassiveDashFaceDir(const Vec3& v)
    {
        m_vKalistaPassiveDashFaceDir = v;
        m_bKalistaPassiveDashHasFaceDir = v.x != 0.f || v.z != 0.f;
    }
    f32_t GetKalistaERendBaseDmg() const { return Kalista::GetTuning().rendBaseDamage; }
    void  SetKalistaERendBaseDmg(f32_t v) { Kalista::GetTuning().rendBaseDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetKalistaRendStackDmg() const { return Kalista::GetTuning().rendStackDamage; }
    void  SetKalistaRendStackDmg(f32_t v) { Kalista::GetTuning().rendStackDamage = (v < 0.f) ? 0.f : v; }
    f32_t GetKalistaERendWispSize() const { return Kalista::GetTuning().eRendWispSize; }
    void  SetKalistaERendWispSize(f32_t v) { Kalista::GetTuning().eRendWispSize = (v < 0.1f) ? 0.1f : v; }
    f32_t GetKalistaERendWispLifetime() const { return Kalista::GetTuning().eRendWispLifetime; }
    void  SetKalistaERendWispLifetime(f32_t v) { Kalista::GetTuning().eRendWispLifetime = (v < 0.03f) ? 0.03f : v; }
    f32_t GetKalistaERendWispFps() const { return Kalista::GetTuning().eRendWispAtlasFps; }
    void  SetKalistaERendWispFps(f32_t v) { Kalista::GetTuning().eRendWispAtlasFps = (v < 1.f) ? 1.f : v; }
};
