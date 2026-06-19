#define _CRT_SECURE_NO_WARNINGS
#include "Manager/Minion_Manager.h"
#include "Map/MapDataFormats.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/MinionPerformanceComponents.h"
#include "ECS/SpatialIndex.h"
#include "ProfilerAPI.h"
#include "Scene/RenderVisibilityFilter.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/PoseStateComponent.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
    static constexpr bool_t kMinionDebugOutput = false;
    static constexpr f32_t kMinionAvoidancePadding = 0.05f;
    static constexpr const char* kTibbersModelPath =
        "Client/Bin/Resource/Texture/Character/Annie/tibber.wmesh";
    static constexpr const wchar_t* kTibbersTexturePath =
        L"Client/Bin/Resource/Texture/Character/Annie/tibber_base.png";
    static constexpr f32_t kTibbersVisualScale = 0.01f;
    static constexpr f32_t kMinionBaseAnimUpdateInterval = 1.f / 8.f;
    static constexpr f32_t kMinionHighPriorityAnimUpdateInterval = 1.f / 16.f;
    static constexpr uint64_t kMinionAnimUpdateBudget = 3u;
    static constexpr f32_t kMinionScreenCullMargin = 48.f;
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;

	void OutputMinionDebug(const char* msg)
	{
		(void)msg;
	}

    f32_t ResolveMinionAnimPhase(EntityID entity, f32_t interval)
    {
        uint32_t h = entity * 747796405u + 2891336453u;
        h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
        h = (h >> 22u) ^ h;
        return interval * static_cast<f32_t>(h & 1023u) / 1024.f;
    }

    bool_t IsWorldPointNearScreen(const Mat4& matViewProj, const Vec3& vWorld)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.f || display.y <= 0.f)
            return true;

        XMVECTOR v = XMVectorSet(vWorld.x, vWorld.y, vWorld.z, 1.f);
        v = XMVector4Transform(v, matViewProj.ToXMMATRIX());

        const f32_t w = XMVectorGetW(v);
        if (w <= 0.01f)
            return false;

        const f32_t nx = XMVectorGetX(v) / w;
        const f32_t ny = XMVectorGetY(v) / w;
        const f32_t sx = (nx * 0.5f + 0.5f) * display.x;
        const f32_t sy = (1.f - (ny * 0.5f + 0.5f)) * display.y;

        return sx >= -kMinionScreenCullMargin &&
            sx <= display.x + kMinionScreenCullMargin &&
            sy >= -kMinionScreenCullMargin &&
            sy <= display.y + kMinionScreenCullMargin;
    }

    void FlushTransformForRender(TransformComponent& tf)
    {
        if (tf.m_bLocalDirty)
        {
            XMVECTOR scale = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&tf.m_LocalScale));
            XMVECTOR rot = XMQuaternionRotationRollPitchYaw(
                tf.m_LocalRotation.x,
                tf.m_LocalRotation.y,
                tf.m_LocalRotation.z);
            XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&tf.m_LocalPosition));
            XMMATRIX local = XMMatrixAffineTransformation(scale, XMVectorZero(), rot, pos);
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&tf.m_LocalMatrix), local);
            tf.m_bLocalDirty = false;
            tf.m_bWorldDirty = true;
        }

        if (tf.m_bWorldDirty)
        {
            tf.m_WorldMatrix = tf.m_LocalMatrix;
            tf.m_bWorldDirty = false;
        }
    }

    void FaceMoveDirection(TransformComponent& tf, const Vec3& dir)
    {
        if (fabsf(dir.x) < 0.0001f && fabsf(dir.z) < 0.0001f)
            return;

        const f32_t yaw = atan2f(-dir.x, -dir.z);
        const Vec3 rot = tf.GetRotation();
        tf.SetRotation({ rot.x, yaw, rot.z });
    }

    bool_t IsMinionMoveBlockingKind(eSpatialKind kind)
    {
        return kind == eSpatialKind::Champion ||
            kind == eSpatialKind::Minion ||
            kind == eSpatialKind::JungleMob;
    }

    f32_t ResolveAgentRadius(CWorld* pWorld, EntityID entity)
    {
        if (pWorld && entity != NULL_ENTITY && pWorld->HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, pWorld->GetComponent<SpatialAgentComponent>(entity).radius);

        return 0.5f;
    }

    bool_t IsSeparatingCandidate(
        const Vec3& vCurrent, const Vec3& vCandidate, const Vec3& vBlockerPos, f32_t minDistSq)
    {
        const f32_t currentDistSq = WintersMath::DistanceSqXZ(vCurrent, vBlockerPos);

        if (currentDistSq >= minDistSq)
            return false;

        const f32_t candidateDistSq = WintersMath::DistanceSqXZ(vCandidate, vBlockerPos);
        return candidateDistSq > currentDistSq + 0.0001f;
    }

    bool_t DoesEntityBlockCandidate(
        CWorld* pWorld,
        EntityID self,
        EntityID other,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        if (!pWorld)
            return false;
        if (other == self)
            return false;
        if (!pWorld->IsAlive(other) ||
            !pWorld->HasComponent<SpatialAgentComponent>(other) ||
            !pWorld->HasComponent<TransformComponent>(other))
        {
            return false;
        }

        const auto& agent = pWorld->GetComponent<SpatialAgentComponent>(other);
        if (!IsMinionMoveBlockingKind(agent.kind))
            return false;

        if (pWorld->HasComponent<HealthComponent>(other))
        {
            const auto& health = pWorld->GetComponent<HealthComponent>(other);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }

        const f32_t minDist =
            radius + (std::max)(0.2f, agent.radius) + kMinionAvoidancePadding;
        const Vec3 otherPos = pWorld->GetComponent<TransformComponent>(other).GetPosition();
        const f32_t minDistSq = minDist * minDist;
        return WintersMath::DistanceSqXZ(candidate, otherPos) < minDistSq &&
            !IsSeparatingCandidate(current, candidate, otherPos, minDistSq);
    }

    bool_t IsCandidateClearAgainstEntities(
        CWorld* pWorld,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius,
        const std::vector<EntityID>& candidates)
    {
        for (EntityID other : candidates)
        {
            if (DoesEntityBlockCandidate(pWorld, self, other, current, candidate, radius))
                return false;
        }
        return true;
    }

    bool_t IsCandidateClear(
        CWorld* pWorld,
        EntityID self,
        const Vec3& current,
        const Vec3& candidate,
        f32_t radius)
    {
        if (!pWorld)
            return true;

        bool_t bClear = true;
        pWorld->ForEach<SpatialAgentComponent, TransformComponent>(
            function<void(EntityID, SpatialAgentComponent&, TransformComponent&)>(
                [&](EntityID other, SpatialAgentComponent&, TransformComponent&)
                {
                    if (!bClear || other == self)
                        return;
                    if (DoesEntityBlockCandidate(pWorld, self, other, current, candidate, radius))
                        bClear = false;
                }));

        return bClear;
    }

    Vec3 ResolveAvoidedMoveDirection(
        CWorld* pWorld,
        EntityID self,
        const Vec3& pos,
        const Vec3& desired,
        f32_t step)
    {
        static constexpr f32_t kAngles[] = {
            0.f,
            0.610865f, -0.610865f,
            1.22173f, -1.22173f,
            1.570796f, -1.570796f
        };

        const f32_t radius = ResolveAgentRadius(pWorld, self);
        static thread_local std::vector<EntityID> s_vecSpatialCandidates;
        const std::vector<EntityID>* pSpatialCandidates = nullptr;
        if (pWorld)
        {
            if (CSpatialIndex* pSpatial = pWorld->Get_SpatialIndex())
            {
                s_vecSpatialCandidates.clear();
                constexpr u32_t kMoveBlockerMask =
                    SpatialMask(eSpatialKind::Champion) |
                    SpatialMask(eSpatialKind::Minion) |
                    SpatialMask(eSpatialKind::JungleMob);
                pSpatial->QueryRadius(
                    pos,
                    radius + step + kMinionAvoidancePadding,
                    kMoveBlockerMask,
                    0u,
                    s_vecSpatialCandidates);
                pSpatialCandidates = &s_vecSpatialCandidates;
            }
        }

        for (const f32_t angle : kAngles)
        {
            const Vec3 dir = WintersMath::RotateXZ(desired, angle);
            const Vec3 candidate{
                pos.x + dir.x * step,
                pos.y,
                pos.z + dir.z * step
            };

            const bool_t bClear = pSpatialCandidates
                ? IsCandidateClearAgainstEntities(
                    pWorld, self, pos, candidate, radius, *pSpatialCandidates)
                : IsCandidateClear(pWorld, self, pos, candidate, radius);
            if (bClear)
                return dir;
        }

        return Vec3{};
    }

}

static constexpr f32_t kArriveRadius = 0.8f;   // ?⑥씠?ъ씤???꾨떖 ?먯젙
static constexpr f32_t kDefaultMinionScale = 0.006f;
static constexpr f32_t kFallbackMinionAttackSeconds = 0.75f;
static constexpr f32_t kMinionRecoverSeconds = 0.22f;
static constexpr uint8_t kInvalidMinionVisualBaseState = 0xff;

namespace
{
    bool_t IsMinionMoveVisualState(MinionStateComponent::State state)
    {
        return state == MinionStateComponent::LaneMove ||
            state == MinionStateComponent::Chase;
    }

    MinionStateComponent::State ResolveMinionBaseVisualState(
        const MinionStateComponent& ms,
        const PoseStateComponent* pPose)
    {
        if (pPose)
        {
            const auto poseId = static_cast<ePoseStateId>(pPose->poseId);
            if (poseId == ePoseStateId::Run)
                return MinionStateComponent::LaneMove;
            if (poseId == ePoseStateId::Idle)
                return MinionStateComponent::Idle;
        }

        if (IsMinionMoveVisualState(ms.current))
            return ms.current;

        return MinionStateComponent::Idle;
    }

    const char* ResolveMinionBaseAnimationKeyword(
        ModelRenderer& renderer,
        MinionStateComponent::State state)
    {
        if (IsMinionMoveVisualState(state))
            return renderer.HasAnimationByName("run") ? "run" : nullptr;

        if (renderer.HasAnimationByName("idle"))
            return "idle";
        if (renderer.HasAnimationByName("run"))
            return "run";

        return nullptr;
    }

    f32_t ResolveMinionAnimationSeconds(
        ModelRenderer& renderer,
        const char* pKeyword,
        f32_t fallbackSeconds)
    {
        if (!pKeyword)
            return fallbackSeconds;

        const f32_t seconds = renderer.GetAnimationDurationSecondsByName(pKeyword);
        return (seconds > 0.01f) ? seconds : fallbackSeconds;
    }
}

// ?????????????????????????????????????????????????????????????
// Initialize / Shutdown / Clear
// ?????????????????????????????????????????????????????????????
HRESULT CMinion_Manager::Initialize(CWorld* pWorld)
{
    // Phase 5-A ?꾩냽: m_pWorld ?щ컮?몃뵫??guard ?욎쑝濡?(Structure_Manager ? ?숈씪 ?댁쑀).
    m_pWorld = pWorld;
    if (m_bInitialized) return S_OK;
    m_fSpawnTimer  = 0.f;
    m_bInitialized = true;

    // ?⑥씠?ъ씤?멸? 鍮꾩뼱 ?덉쑝硫?湲곕낯媛??곸옱 (Stage1.dat 濡쒕뱶媛 ?ㅼ씠????뼱?)
    bool empty = true;
    for (auto& row : m_vecWaypoints)
        for (auto& v : row)
            if (!v.empty()) { empty = false; break; }
    if (empty) LoadDefaults();
    return S_OK;
}

void CMinion_Manager::Shutdown()
{
    Clear();
    m_pWorld       = nullptr;
    m_bInitialized = false;
    // 二쇱쓽: m_vecWaypoints ???좎? (Editor/InGame ???꾪솚 媛??곗씠??蹂댁〈)
}

void CMinion_Manager::Clear()
{
    if (m_pWorld)
        for (auto id : m_vecEntities) m_pWorld->DestroyEntity(id);
    m_vecEntities.clear();
    m_vecSpawnedThisTick.clear();
    m_mapRenderers.clear();
    m_mapVisualStates.clear();
    m_deqPendingNetworkVisuals.clear();
    for (auto& teamPool : m_vecNetworkRendererPool)
    {
        for (auto& typePool : teamPool)
            typePool.clear();
    }

    m_iCurrentRound = kRoundsPerWave;
    m_fNextRoundCountdown = 0.f;
}

// ?????????????????????????????????????????????????????????????
// Tick ???ㅽ룿 ??대㉧ + ?⑥씠?ъ씤???대룞
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::Tick(f32_t fDeltaTime)
{
    WINTERS_PROFILE_SCOPE("Minion::Tick");
    if (!m_pWorld || !m_bEnabled) return;
    m_vecSpawnedThisTick.clear();
    m_fSpawnTimer += fDeltaTime;
    //Wave Trigger - ?⑥씠釉??꾩껜 ?ㅽ룿 二쇨린 愿由?
    if (m_fSpawnTimer >= m_fSpawnInterval)
    {
        m_fSpawnTimer = 0.f;
        DoSpawnWave();
    }

    i32_t iStuckChase = 0;
    m_pWorld->ForEach<MinionStateComponent, VelocityComponent>(
        [&iStuckChase](EntityID, MinionStateComponent& ms, VelocityComponent& vel)
        {
            if (ms.current == MinionStateComponent::Chase && vel.fSpeed <= 0.f)
                ++iStuckChase;
        });
    WINTERS_PROFILE_COUNT("Minion::StuckChase", iStuckChase);

    // Chase velocity is applied before the lane waypoint skip.
    m_pWorld->ForEach<MinionStateComponent, TransformComponent, VelocityComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&, VelocityComponent&)>(
        [this, fDeltaTime](EntityID id, MinionStateComponent& ms, TransformComponent& tf, VelocityComponent& vel)
        {
            if (ms.current != MinionStateComponent::Chase)
                return;
            if (vel.fSpeed <= 0.f)
                return;

            const Vec3 vCur = tf.GetPosition();
            const f32_t step = vel.fSpeed * fDeltaTime;
            const Vec3 vMoveDir = ResolveAvoidedMoveDirection(
                m_pWorld,
                id,
                vCur,
                vel.vDirection,
                step);
            if (fabsf(vMoveDir.x) + fabsf(vMoveDir.z) <= 0.0001f)
                return;

            vel.vDirection = vMoveDir;
            FaceMoveDirection(tf, vMoveDir);
            tf.SetPosition({
                vCur.x + vMoveDir.x * step,
                vCur.y,
                vCur.z + vMoveDir.z * step
                });
        }));

    // ?쇱슫??吏꾪뻾
    m_fNextRoundCountdown -= fDeltaTime;
    if (m_fNextRoundCountdown <= 0.f && m_iCurrentRound < kRoundsPerWave)
    {
        const eMinionType type = (m_iCurrentRound < 3)
            ? eMinionType::Melee : eMinionType::Ranged;

        // 3 lanes 횞 2 teams = 6留덈━ ?숈떆 ?ㅽ룿
        for (auto way : { eMinionWay::Top, eMinionWay::Mid, eMinionWay::Bottom })
            for (auto team : { eMinionTeam::Blue, eMinionTeam::Red })
            {
                const EntityID id = Spawn_Minion(type, team, way);
                if (id != NULL_ENTITY)
                    m_vecSpawnedThisTick.push_back(id);
            }

        ++m_iCurrentRound;
        m_fNextRoundCountdown = m_fSpawnDelay;   // += ???= 濡?catch-up 諛⑹?
    }

    {
        WINTERS_PROFILE_SCOPE("Minion::WaypointMove");
        m_pWorld->ForEach<MinionStateComponent, TransformComponent, VelocityComponent>(
            [this, fDeltaTime](EntityID id, MinionStateComponent& ms,
                TransformComponent& xform, VelocityComponent& vel)
            {
                if (ms.current == MinionStateComponent::Dead) return;
                if (std::find(m_vecSpawnedThisTick.begin(), m_vecSpawnedThisTick.end(), id) != m_vecSpawnedThisTick.end())
                {
                    vel.vDirection = { 0.f, 0.f, 0.f };
                    vel.fSpeed = 0.f;
                    return;
                }
                // Phase 5-A ?꾩냽: AI 媛 ?寃?援먯쟾 以묒씠硫??⑥씠?ъ씤???대룞 以묐떒.
                //  CMinionAISystem ??ms.current = Attack ?쇰줈 ?ㅼ젙 ??Manager ???ш린??return.
                //  ?寃잛쓣 ?껋쑝硫?AI 媛 ms.current = Idle 濡??섎룎由щ?濡??먮룞 蹂듦?.
                if (ms.current == MinionStateComponent::Attack ||
                    ms.current == MinionStateComponent::Chase ||
                    ms.current == MinionStateComponent::Dead)
                    return;

                const eMinionTeam team = (ms.team == eTeam::Blue) ? eMinionTeam::Blue : eMinionTeam::Red;
                const eMinionWay  lane = static_cast<eMinionWay>(ms.lane);
                const Vec3* pWPs = nullptr;
                uint32_t    iCnt = 0;
                CMinion_Manager::GetWayPoints(team, lane, &pWPs, &iCnt);
                if (!pWPs || ms.currentWaypoint >= iCnt)
                {
                    vel.fSpeed = 0.f;
                    ms.current = MinionStateComponent::Idle;
                    return;
                }

                const Vec3 vCur = xform.GetPosition();
                const Vec3 vGoal = pWPs[ms.currentWaypoint];
                Vec3  vDelta = { vGoal.x - vCur.x, 0.f, vGoal.z - vCur.z };
                const f32_t fDist = sqrtf(vDelta.x * vDelta.x + vDelta.z * vDelta.z);

                if (fDist < kArriveRadius)
                {
                    ms.currentWaypoint++;
                    return;
                }

                Vec3 vDir = { vDelta.x / fDist, 0.f, vDelta.z / fDist };
                const f32_t step = ms.moveSpeed * fDeltaTime;
                const Vec3 vMoveDir = ResolveAvoidedMoveDirection(
                    m_pWorld,
                    id,
                    vCur,
                    vDir,
                    step);
                if (fabsf(vMoveDir.x) + fabsf(vMoveDir.z) <= 0.0001f)
                {
                    vel.vDirection = {};
                    vel.fSpeed = 0.f;
                    return;
                }

                vDir = vMoveDir;
                vel.vDirection = vDir;
                vel.fSpeed = ms.moveSpeed;

                FaceMoveDirection(xform, vDir);
                xform.SetPosition({
                    vCur.x + vDir.x * step,
                    vCur.y,
                    vCur.z + vDir.z * step
                    });
                ms.current = MinionStateComponent::LaneMove;
            });
    }   // Minion::WaypointMove

    TickVisuals(fDeltaTime);
}

void CMinion_Manager::UpdateMinionVisual(
    EntityID entity,
    MinionStateComponent& ms,
    RenderComponent& rc,
    f32_t fDeltaTime)
{
    if (!rc.pRenderer)
        return;

    auto& visual = m_mapVisualStates[entity];

    const PoseStateComponent* pPose = nullptr;
    if (m_pWorld && m_pWorld->HasComponent<PoseStateComponent>(entity))
        pPose = &m_pWorld->GetComponent<PoseStateComponent>(entity);

    const ActionStateComponent* pAction = nullptr;
    if (m_pWorld && m_pWorld->HasComponent<ActionStateComponent>(entity))
        pAction = &m_pWorld->GetComponent<ActionStateComponent>(entity);

    if (ms.current == MinionStateComponent::Dead)
    {
        if (visual.phase != eMinionVisualPhase::Death)
        {
            if (rc.pRenderer->HasAnimationByName("death"))
                rc.pRenderer->PlayAnimationByNameAdvanced("death", false, false, 1.f);
            visual.phase = eMinionVisualPhase::Death;
            visual.baseState = kInvalidMinionVisualBaseState;
        }

        ms.visualState = MinionStateComponent::Dead;
        ms.bAttackAnimRequested = false;
        return;
    }

    const uint16_t actionId = pAction
        ? pAction->actionId
        : static_cast<uint16_t>(eActionStateId::None);
    const uint32_t actionSeq = pAction ? pAction->sequence : 0u;
    const ePoseStateId poseId = pPose
        ? static_cast<ePoseStateId>(pPose->poseId)
        : ePoseStateId::None;
    const bool_t bNetworkBaseAnimation =
        pPose &&
        (poseId == ePoseStateId::Run ||
            poseId == ePoseStateId::Idle);
    const bool_t bNetworkBasicAttack =
        pAction &&
        static_cast<eActionStateId>(actionId) == eActionStateId::BasicAttack &&
        actionSeq != 0u &&
        (visual.lastActionSeq != actionSeq || visual.lastAnimId != actionId);
    const bool_t bLocalBasicAttack =
        actionSeq == 0u &&
        ms.current == MinionStateComponent::Attack &&
        ms.bAttackAnimRequested;

    if (bNetworkBasicAttack || bLocalBasicAttack)
    {
        visual.lastActionSeq = actionSeq;
        visual.lastAnimId = actionId;
        ms.bAttackAnimRequested = false;

        if (visual.phase == eMinionVisualPhase::Recover)
        {
            visual.bPendingAttack = true;
            return;
        }

        if (rc.pRenderer->HasAnimationByName("attack"))
        {
            rc.pRenderer->PlayAnimationByNameAdvanced("attack", false, false, 1.f);
            visual.phase = eMinionVisualPhase::Attack;
            visual.phaseTimer = ResolveMinionAnimationSeconds(
                *rc.pRenderer,
                "attack",
                kFallbackMinionAttackSeconds);
            visual.baseState = kInvalidMinionVisualBaseState;
            ms.visualState = MinionStateComponent::Attack;
            ms.animUpdateAccumulator = kMinionHighPriorityAnimUpdateInterval;
            return;
        }

        visual.phase = eMinionVisualPhase::Base;
        visual.baseState = kInvalidMinionVisualBaseState;
    }

    if (bNetworkBaseAnimation &&
        (visual.phase == eMinionVisualPhase::Attack ||
            visual.phase == eMinionVisualPhase::Recover))
    {
        visual.phase = eMinionVisualPhase::Base;
        visual.phaseTimer = 0.f;
        visual.baseState = kInvalidMinionVisualBaseState;
        visual.bPendingAttack = false;
    }

    if (visual.phase == eMinionVisualPhase::Attack)
    {
        visual.phaseTimer -= fDeltaTime;
        if (visual.phaseTimer > 0.f)
            return;

        if (rc.pRenderer->HasAnimationByName("attack"))
        {
            const f32_t attackSeconds = ResolveMinionAnimationSeconds(
                *rc.pRenderer,
                "attack",
                kFallbackMinionAttackSeconds);
            const f32_t reverseSpeed = attackSeconds / kMinionRecoverSeconds;
            rc.pRenderer->PlayAnimationByNameAdvanced("attack", false, true, reverseSpeed);
            visual.phase = eMinionVisualPhase::Recover;
            visual.phaseTimer = kMinionRecoverSeconds;
            return;
        }

        visual.phase = eMinionVisualPhase::Base;
        visual.baseState = kInvalidMinionVisualBaseState;
    }

    if (visual.phase == eMinionVisualPhase::Recover)
    {
        visual.phaseTimer -= fDeltaTime;
        if (visual.phaseTimer > 0.f)
            return;

        if (visual.bPendingAttack && rc.pRenderer->HasAnimationByName("attack"))
        {
            visual.bPendingAttack = false;
            rc.pRenderer->PlayAnimationByNameAdvanced("attack", false, false, 1.f);
            visual.phase = eMinionVisualPhase::Attack;
            visual.phaseTimer = ResolveMinionAnimationSeconds(
                *rc.pRenderer,
                "attack",
                kFallbackMinionAttackSeconds);
            visual.baseState = kInvalidMinionVisualBaseState;
            ms.visualState = MinionStateComponent::Attack;
            ms.animUpdateAccumulator = kMinionHighPriorityAnimUpdateInterval;
            return;
        }

        visual.bPendingAttack = false;
        visual.phase = eMinionVisualPhase::Base;
        visual.baseState = kInvalidMinionVisualBaseState;
    }

    const MinionStateComponent::State baseState =
        ResolveMinionBaseVisualState(ms, pPose);
    const uint8_t baseStateValue = static_cast<uint8_t>(baseState);

    if (visual.baseState != baseStateValue)
    {
        const char* pBaseKeyword =
            ResolveMinionBaseAnimationKeyword(*rc.pRenderer, baseState);
        if (pBaseKeyword)
            rc.pRenderer->PlayAnimationByNameAdvanced(pBaseKeyword, true, false, 1.f);

        visual.baseState = baseStateValue;
    }

    visual.phase = eMinionVisualPhase::Base;
    ms.visualState = baseState;
    ms.bAttackAnimRequested = false;
}

// ?????????????????????????????????????????????????????????????
// Render
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::TickVisuals(f32_t fDeltaTime, const Mat4* pViewProj)
{
    if (!m_pWorld)
        return;

    ProcessQueueNetworkVisual(kNetworkVisualBindBudgetPerFrame);

    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);

    m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
        [this, fDeltaTime, localTeam, pViewProj](
            EntityID id,
            MinionStateComponent& ms,
            RenderComponent& rc,
            TransformComponent& tf)
        {
            if (!rc.bVisible || !rc.pRenderer)
                return;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, false))
                return;
            if (pViewProj && !IsWorldPointNearScreen(*pViewProj, tf.GetPosition()))
                return;

            UpdateMinionVisual(id, ms, rc, fDeltaTime);
        }
    );

    m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
        [fDeltaTime](EntityID, MinionStateComponent& ms, RenderComponent& rc)
        {
            if (ms.current != MinionStateComponent::Dead)
                return;
            if (ms.deathTimer <= 0.f)
                return;

            ms.deathTimer -= fDeltaTime;
            if (ms.deathTimer <= 0.f)
                rc.bVisible = false;
        });

    {
        WINTERS_PROFILE_SCOPE("Minion::AnimUpdate");
        uint64_t animCount = 0;
        uint64_t skippedCount = 0;
        uint64_t visibilitySkippedCount = 0;
        uint64_t screenSkippedCount = 0;
        uint64_t budgetSkippedCount = 0;
        m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
            [this, fDeltaTime, localTeam, pViewProj,
                &animCount, &skippedCount, &visibilitySkippedCount,
                &screenSkippedCount, &budgetSkippedCount](
                EntityID id,
                MinionStateComponent& ms,
                RenderComponent& rc,
                TransformComponent& tf)
            {
                if (!rc.bVisible || !rc.pRenderer)
                    return;
                if (rc.bSceneManaged || !rc.bAnimated || !rc.pRenderer->HasSkeleton())
                {
                    ++skippedCount;
                    return;
                }
                if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, false))
                {
                    ++skippedCount;
                    ++visibilitySkippedCount;
                    ms.animUpdateAccumulator = 0.f;
                    return;
                }
                if (pViewProj && !IsWorldPointNearScreen(*pViewProj, tf.GetPosition()))
                {
                    ++skippedCount;
                    ++screenSkippedCount;
                    ms.animUpdateAccumulator = 0.f;
                    return;
                }

                const bool_t bHighPriorityAnim =
                    ms.current == MinionStateComponent::Attack ||
                    ms.current == MinionStateComponent::Dead;
                const f32_t updateInterval = bHighPriorityAnim
                    ? kMinionHighPriorityAnimUpdateInterval
                    : (std::max)(ms.animUpdateInterval, kMinionBaseAnimUpdateInterval);

                ms.animUpdateAccumulator += fDeltaTime;
                if (ms.animUpdateAccumulator < updateInterval)
                {
                    ++skippedCount;
                    return;
                }

                if (!bHighPriorityAnim && animCount >= kMinionAnimUpdateBudget)
                {
                    ++skippedCount;
                    ++budgetSkippedCount;
                    return;
                }

                rc.pRenderer->Update(ms.animUpdateAccumulator);
                ms.animUpdateAccumulator = std::fmod(ms.animUpdateAccumulator, updateInterval);
                ++animCount;
            });
        WINTERS_PROFILE_COUNT("Anim::UpdateCalls", animCount);
        WINTERS_PROFILE_COUNT("Anim::Skipped", skippedCount);
        WINTERS_PROFILE_COUNT("Anim::VisibilitySkipped", visibilitySkippedCount);
        WINTERS_PROFILE_COUNT("Anim::ScreenSkipped", screenSkippedCount);
        WINTERS_PROFILE_COUNT("Anim::BudgetSkipped", budgetSkippedCount);
    }
}

void CMinion_Manager::Render(const Mat4& matVP, const Vec3& vCameraWorld,
    void* pAmbientOcclusionSRV,
    bool_t bIgnoreFogOfWar)
{
    WINTERS_PROFILE_SCOPE("Minion::Render");
    if (!m_pWorld) return;
    const u8_t localTeam = UI::QueryLocalTeam(*m_pWorld);
    uint64_t candidateCount = 0;
    uint64_t visibleCount = 0;
    uint64_t fowSkippedCount = 0;
    uint64_t meshCount = 0;

    m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
        [&](EntityID id, MinionStateComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            ++candidateCount;
            if (!UI::IsRenderableForLocal(*m_pWorld, id, localTeam, bIgnoreFogOfWar))
            {
                ++fowSkippedCount;
                return;
            }

            ++visibleCount;
            meshCount += rc.pRenderer->GetMeshCount();
            // Update(dt) ??Tick ??RenderComponent ForEach ?먯꽌 ?섑뻾 (?ш린???뚮뜑留?
            FlushTransformForRender(xform);
            rc.pRenderer->SetAmbientOcclusionSRV(pAmbientOcclusionSRV);
            rc.pRenderer->UpdateCamera(matVP, vCameraWorld);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->RenderFrustumCulled(matVP);
        });

    WINTERS_PROFILE_COUNT("Minion::RenderCandidates", candidateCount);
    WINTERS_PROFILE_COUNT("Minion::RenderVisible", visibleCount);
    WINTERS_PROFILE_COUNT("Minion::RenderFowSkipped", fowSkippedCount);
    WINTERS_PROFILE_COUNT("Minion::RenderMeshCount", meshCount);
}

// ?????????????????????????????????????????????????????????????
// Spawn ??ECS ?뷀떚???앹꽦
// ?????????????????????????????????????????????????????????????
bool_t CMinion_Manager::Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeamParam)
{
    if (!m_pWorld || entity == NULL_ENTITY)
        return false;

    if (static_cast<u32_t>(eType) >= static_cast<u32_t>(eMinionType::End))
        eType = eMinionType::Melee;

    if (eTeamParam == eMinionTeam::End)
        eTeamParam = eMinionTeam::Blue;

    if (m_pWorld->HasComponent<RenderComponent>(entity))
    {
        auto& existing = m_pWorld->GetComponent<RenderComponent>(entity);
        if (existing.pRenderer)
        {
            m_mapVisualStates.emplace(entity, MinionVisualPlaybackState{});
            return true;
        }
    }

    const char* pPath = ResolveModelPath(eType, eTeamParam);
    if (!pPath)
    {
        OutputMinionDebug("[MinionVisual] bind FAIL: ResolveModelPath returned null\n");
        return false;
    }

    auto pRenderer = AcquireNetworkRenderer(eType, eTeamParam, pPath);
    if (!pRenderer)
    {
        char msg[512]{};
        sprintf_s(msg,
            "[MinionVisual] bind FAIL: AcquireNetworkRenderer failed entity=%u type=%u team=%u path=%s\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(eType),
            static_cast<u32_t>(eTeamParam),
            pPath);
        OutputMinionDebug(msg);
        return false;
    }

    const u32_t animCount = pRenderer->GetAnimationCount();
    if (animCount > 0)
        pRenderer->PlayAnimationByName("run", true);

    const f32_t fVisualScale = (eType == eMinionType::Tibbers) ? kTibbersVisualScale : m_fVisualScale;

    if (!m_pWorld->HasComponent<TransformComponent>(entity))
        m_pWorld->AddComponent<TransformComponent>(entity);
    m_pWorld->GetComponent<TransformComponent>(entity).SetScale(fVisualScale);

    const eTeam simTeam = (eTeamParam == eMinionTeam::Red) ? eTeam::Red : eTeam::Blue;

    if (!m_pWorld->HasComponent<MinionStateComponent>(entity))
    {
        MinionStateComponent state{};
        state.team = simTeam;
        state.type = static_cast<u8_t>(eType);
        state.animUpdateInterval = kMinionBaseAnimUpdateInterval;
        state.animUpdateAccumulator =
            ResolveMinionAnimPhase(entity, state.animUpdateInterval);
        m_pWorld->AddComponent<MinionStateComponent>(entity, state);
    }
    else
    {
        auto& state = m_pWorld->GetComponent<MinionStateComponent>(entity);
        state.team = simTeam;
        state.type = static_cast<u8_t>(eType);
        state.animUpdateInterval = kMinionBaseAnimUpdateInterval;
        state.animUpdateAccumulator =
            ResolveMinionAnimPhase(entity, state.animUpdateInterval);
    }

    if (!m_pWorld->HasComponent<MinionComponent>(entity))
    {
        MinionComponent minion{};
        minion.team = simTeam;
        minion.roleType = static_cast<u8_t>(eType);
        m_pWorld->AddComponent<MinionComponent>(entity, minion);
    }
    else
    {
        auto& minion = m_pWorld->GetComponent<MinionComponent>(entity);
        minion.team = simTeam;
        minion.roleType = static_cast<u8_t>(eType);
    }

    if (!m_pWorld->HasComponent<TargetableTag>(entity))
        m_pWorld->AddComponent<TargetableTag>(entity);

    if (!m_pWorld->HasComponent<SpatialAgentComponent>(entity))
    {
        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Minion;
        spatial.team = static_cast<u8_t>(simTeam);
        spatial.radius = 0.5f;
        m_pWorld->AddComponent<SpatialAgentComponent>(entity, spatial);
    }

    if (!m_pWorld->HasComponent<ColliderComponent>(entity))
    {
        ColliderComponent collider{};
        collider.vHalfExtents = { 0.5f, 1.0f, 0.5f };
        collider.vOffset = { 0.f, 0.5f, 0.f };
        collider.bIsTrigger = false;
        m_pWorld->AddComponent<ColliderComponent>(entity, collider);
    }

    if (!m_pWorld->HasComponent<VisibilityComponent>(entity))
        m_pWorld->AddComponent<VisibilityComponent>(entity);

    RenderComponent* pRenderComponent = nullptr;
    if (!m_pWorld->HasComponent<RenderComponent>(entity))
        pRenderComponent = &m_pWorld->AddComponent<RenderComponent>(entity);
    else
        pRenderComponent = &m_pWorld->GetComponent<RenderComponent>(entity);

    pRenderComponent->pRenderer = pRenderer.get();
    pRenderComponent->bVisible = true;
    pRenderComponent->bAnimated = true;
    pRenderComponent->bSceneManaged = false;

    m_mapRenderers[entity] = std::move(pRenderer);
    m_mapVisualStates[entity] = MinionVisualPlaybackState{};
    if (std::find(m_vecEntities.begin(), m_vecEntities.end(), entity) == m_vecEntities.end())
        m_vecEntities.push_back(entity);

    char msg[384]{};
    sprintf_s(msg,
        "[MinionVisual] bind network entity=%u type=%u team=%u anims=%u skeleton=%d path=%s\n",
        static_cast<u32_t>(entity),
        static_cast<u32_t>(eType),
        static_cast<u32_t>(eTeamParam),
        animCount,
        pRenderComponent->pRenderer->HasSkeleton() ? 1 : 0,
        pPath);
    OutputMinionDebug(msg);
    return true;
}

void CMinion_Manager::Release_NetworkVisual(EntityID entity)
{
    if (entity == NULL_ENTITY)
        return;

    m_deqPendingNetworkVisuals.erase(
        std::remove_if(
            m_deqPendingNetworkVisuals.begin(),
            m_deqPendingNetworkVisuals.end(),
            [entity](const NetworkVisualRequest& request)
            {
                return request.entity == entity;
            }
        ),
        m_deqPendingNetworkVisuals.end()
    );

    eMinionType type = eMinionType::Melee;
    eMinionTeam team = eMinionTeam::Blue;
    if (m_pWorld && m_pWorld->IsAlive(entity) &&
        m_pWorld->HasComponent<MinionStateComponent>(entity))
    {
        const auto& state = m_pWorld->GetComponent<MinionStateComponent>(entity);
        type = static_cast<eMinionType>(state.type);
        team = (state.team == eTeam::Red) ? eMinionTeam::Red : eMinionTeam::Blue;
    }

    if (m_pWorld && m_pWorld->IsAlive(entity) &&
        m_pWorld->HasComponent<RenderComponent>(entity))
    {
        auto& rc = m_pWorld->GetComponent<RenderComponent>(entity);
        rc.pRenderer = nullptr;
        rc.bVisible = false;
        rc.bAnimated = false;
    }

    auto rendererIt = m_mapRenderers.find(entity);
    if (rendererIt != m_mapRenderers.end())
    {
        PoolNetworkRenderer(type, team, std::move(rendererIt->second));
        m_mapRenderers.erase(rendererIt);
    }
    m_mapVisualStates.erase(entity);
    m_vecEntities.erase(
        std::remove(m_vecEntities.begin(), m_vecEntities.end(), entity),
        m_vecEntities.end());
    m_vecSpawnedThisTick.erase(
        std::remove(m_vecSpawnedThisTick.begin(), m_vecSpawnedThisTick.end(), entity),
        m_vecSpawnedThisTick.end());
}

EntityID CMinion_Manager::Spawn_Minion(eMinionType eType, eMinionTeam eTeamParam, eMinionWay eWay)
{
    static const char* typeNames[] = { "Melee", "Ranged", "Siege", "Super", "Tibbers" };
    const char* typeName = (static_cast<u32_t>(eType) < 5) ? typeNames[static_cast<u32_t>(eType)] : "???";

    if (!m_pWorld)
    {
        OutputMinionDebug("[Minion] Spawn FAIL: m_pWorld == nullptr\n");
        return NULL_ENTITY;
    }

    const Vec3* pWPs = nullptr;
    uint32_t    iCnt = 0;
    GetWayPoints(eTeamParam, eWay, &pWPs, &iCnt);
    if (!pWPs || iCnt == 0)
    {
        char m[128];
        sprintf_s(m, "[Minion] Spawn FAIL: no waypoints (%s, team=%u, lane=%u)\n",
            typeName, static_cast<u32_t>(eTeamParam), static_cast<u32_t>(eWay));
        OutputMinionDebug(m);
        return NULL_ENTITY;
    }

    const char* pPath = ResolveModelPath(eType, eTeamParam);
    if (!pPath)
    {
        OutputMinionDebug("[Minion] Spawn FAIL: ResolveModelPath returned null\n");
        return NULL_ENTITY;
    }

    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
    {
        char m[512];
        sprintf_s(m, "[Minion] Spawn FAIL: ModelRenderer::Initialize failed for %s\n  path=%s\n",
            typeName, pPath);
        OutputMinionDebug(m);
        return NULL_ENTITY;
    }
    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);

    // ?좊땲硫붿씠??媛쒖닔 濡쒓렇 + 泥??좊땲 ?먮룞 ?ъ깮 (猷⑦봽)
    const uint32_t animCnt = pRenderer->GetAnimationCount();
    {
        char m[256];
        sprintf_s(m, "[Minion] Spawn OK: %s team=%u lane=%u anims=%u skeleton=%d\n",
            typeName, static_cast<u32_t>(eTeamParam), static_cast<u32_t>(eWay),
            animCnt, pRenderer->HasSkeleton() ? 1 : 0);
        OutputMinionDebug(m);
    }

    // Spawn_Minion ?대?, PlayAnimation(idx) ???
    if (animCnt > 0)
    {
        pRenderer->PlayAnimationByName("run", true);   // ?대쫫 keyword 留ㅼ묶
    }

    EntityID id = m_pWorld->CreateEntity();

    auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
    xform.SetPosition(pWPs[0]);
    const f32_t fVisualScale = (eType == eMinionType::Tibbers) ? kTibbersVisualScale : m_fVisualScale;
    xform.SetScale(fVisualScale);

    auto& ms = m_pWorld->AddComponent<MinionStateComponent>(id);
    ms.current         = MinionStateComponent::LaneMove;
    ms.currentWaypoint = (iCnt >= 2) ? 1u : 0u;
    ms.team            = (eTeamParam == eMinionTeam::Blue) ? eTeam::Blue : eTeam::Red;
    ms.type            = static_cast<uint8_t>(eType);
    ms.lane            = static_cast<uint8_t>(eWay);
    switch (eType)
    {
    // sightRange ????긽 attackRange 蹂대떎 異⑸텇???ш쾶 (媛먯? ??異붽꺽 ???ш굅由?吏꾩엯 ?먮쫫 蹂댁옣).
    case eMinionType::Melee:  ms.moveSpeed = 4.0f; ms.attackRange = 1.5f;  ms.sightRange = 12.f; ms.attackDamage = 20.f;   break;
    case eMinionType::Ranged: ms.moveSpeed = 4.0f; ms.attackRange = 8.f;   ms.sightRange = 14.f; ms.attackDamage = 30.f;   break;
    case eMinionType::Siege:  ms.moveSpeed = 3.5f; ms.attackRange = 10.0f; ms.sightRange = 16.f; ms.attackDamage = 40.f;  break;
    case eMinionType::Super:  ms.moveSpeed = 5.0f; ms.attackRange = 2.0f;  ms.sightRange = 14.f; ms.attackDamage = 100.f; break;
    case eMinionType::Tibbers: ms.moveSpeed = 5.2f; ms.attackRange = 2.2f; ms.sightRange = 14.f; ms.attackDamage = 80.f; break;
    default: break;
    }

    const u32_t scanSeed = (static_cast<u32_t>(id) * 1103515245u + 12345u) & 7u;
    ms.targetScanCooldown = 0.03f * static_cast<f32_t>(scanSeed);
    ms.targetScanInterval = 0.16f + 0.02f * static_cast<f32_t>(scanSeed % 5u);
    ms.animUpdateInterval = kMinionBaseAnimUpdateInterval;
    ms.animUpdateAccumulator = ResolveMinionAnimPhase(id, ms.animUpdateInterval);

    auto& hp = m_pWorld->AddComponent<HealthComponent>(id);
    hp.fMaximum = hp.fCurrent =
        (eType == eMinionType::Tibbers) ? 1500.f : ((eType == eMinionType::Super) ? 1000.f : 450.f);

    //Velocity!
    auto& vel = m_pWorld->AddComponent<VelocityComponent>(id);
    vel.vDirection = { 0.f, 0.f, 0.f };
    vel.fSpeed = 0.f;

    m_pWorld->AddComponent<TargetableTag>(id);

    auto& mc = m_pWorld->AddComponent<MinionComponent>(id);
    mc.team = ms.team;
    mc.laneType = ms.lane;
    mc.roleType = ms.type;
    mc.hp = hp.fCurrent;
    mc.maxHp = hp.fMaximum;

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Minion;
    spatial.team = static_cast<u8_t>(ms.team);
    spatial.radius = 0.5f;
    m_pWorld->AddComponent<SpatialAgentComponent>(id, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 1.0f, spatial.radius };
    collider.vOffset = { 0.f, 0.5f, 0.f };
    collider.bIsTrigger = false;
    m_pWorld->AddComponent<ColliderComponent>(id, collider);

    VisionSourceComponent vision{};
    vision.sightRange = ms.sightRange;
    m_pWorld->AddComponent<VisionSourceComponent>(id, vision);
    m_pWorld->AddComponent<VisibilityComponent>(id);

    //NavAgent - First Waypoint!
    NavAgentComponent agent;
    agent.vTarget = (iCnt >= 2) ? pWPs[1] : pWPs[0];
    agent.fSpeed = ms.moveSpeed;
    agent.fArriveRadius = 0.5f;
    agent.bHasGoal = true;
    agent.bPathDirty = true;
    m_pWorld->AddComponent<NavAgentComponent>(id, agent);
    // Nav repath throttle state.
    m_pWorld->AddComponent<MinionNavThrottleComponent>(id);

    auto& rc = m_pWorld->AddComponent<RenderComponent>(id);
    rc.pRenderer = pRenderer.get();
    rc.bVisible  = true;

#if defined(_DEBUG)
    {
        static i32_t s_spawnLogCount = 0;
        if (s_spawnLogCount < 48)
        {
            char dbg[512];
            sprintf_s(dbg,
                "[MinionSpawn] #%d id=%u team=%u lane=%u type=%u state=%u wp=%u pos=(%.2f,%.2f,%.2f) navTarget=(%.2f,%.2f,%.2f) hasGoal=%d dirty=%d\n",
                s_spawnLogCount,
                static_cast<u32_t>(id),
                static_cast<u32_t>(eTeamParam),
                static_cast<u32_t>(eWay),
                static_cast<u32_t>(eType),
                static_cast<u32_t>(ms.current),
                ms.currentWaypoint,
                pWPs[0].x, pWPs[0].y, pWPs[0].z,
                agent.vTarget.x, agent.vTarget.y, agent.vTarget.z,
                agent.bHasGoal ? 1 : 0,
                agent.bPathDirty ? 1 : 0);
            OutputMinionDebug(dbg);
            ++s_spawnLogCount;
        }
    }
#endif

    m_mapRenderers[id] = std::move(pRenderer);
    m_mapVisualStates[id] = MinionVisualPlaybackState{};
    m_vecEntities.push_back(id);
    return id;
}

std::unique_ptr<ModelRenderer> CMinion_Manager::AcquireNetworkRenderer(
    eMinionType eType, eMinionTeam eTeam, const char* pPath)
{
    const u32_t teamIndex = (eTeam == eMinionTeam::Blue) ? 0u : 1u;
    const u32_t typeIndex = static_cast<u32_t>(eType);
    if (typeIndex >= 5u)
        return nullptr;

    auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
    if (!pool.empty())
    {
        std::unique_ptr<ModelRenderer> pRenderer = std::move(pool.back());
        pool.pop_back();
        return pRenderer;
    }

    std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
        return nullptr;

    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);

    return pRenderer;
}

void CMinion_Manager::PoolNetworkRenderer(
    eMinionType eType, eMinionTeam eTeam, std::unique_ptr<ModelRenderer> pRenderer)
{
    if (!pRenderer)
        return;

    const u32_t teamIndex = (eTeam == eMinionTeam::Blue) ? 0u : 1u;
    const u32_t typeIndex = static_cast<u32_t>(eType);

    if (typeIndex >= 5u)
        return;

    m_vecNetworkRendererPool[teamIndex][typeIndex].push_back(std::move(pRenderer));
}

// ?????????????????????????????????????????????????????????????
// DoSpawnWave ??3?쇱씤 횞 2?, 洹쇱젒 3 + ?먭굅由?3
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::DoSpawnWave()
{
    m_iCurrentRound = 0;
    m_fNextRoundCountdown = 0.f;   // 泥??쇱슫??利됱떆
}

void CMinion_Manager::DEBUG_SpawnWaveNow() { DoSpawnWave(); }

void CMinion_Manager::PrewarmNetworkVisualResources()
{
    WINTERS_PROFILE_SCOPE("MinionVisual::Prewarm");

    uint64_t loadedCount = 0u;
    uint64_t failedCount = 0u;

    for (u32_t teamIndex = 0u;
        teamIndex < static_cast<u32_t>(eMinionTeam::End);
        ++teamIndex)
    {
        const eMinionTeam team = static_cast<eMinionTeam>(teamIndex);

        for (u32_t typeIndex = 0u;
            typeIndex < static_cast<u32_t>(eMinionType::End);
            ++typeIndex)
        {
            const eMinionType type = static_cast<eMinionType>(typeIndex);
            const char* pPath = ResolveModelPath(type, team);
            if (!pPath)
            {
                ++failedCount;
                continue;
            }

            if (ModelRenderer::PrewarmModel(pPath))
                ++loadedCount;
            else
                ++failedCount;
        }
    }
    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmLoaded", loadedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmFailed", failedCount);
}

// ?????????????????????????????????????????????????????????????
// GetWayPoints / ResolveModelPath
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::GetWayPoints(eMinionTeam eTeamParam, eMinionWay eWay,
    const Vec3** ppOut, uint32_t* pCountOut)
{
    const u32_t t = static_cast<u32_t>(eTeamParam);
    const u32_t l = static_cast<u32_t>(eWay);
    if (t >= 2 || l >= 3) { *ppOut = nullptr; *pCountOut = 0; return; }

    const auto& v = Get()->m_vecWaypoints[t][l];
    *ppOut     = v.empty() ? nullptr : v.data();
    *pCountOut = static_cast<u32_t>(v.size());
}

const char* CMinion_Manager::ResolveModelPath(eMinionType eType, eMinionTeam eTeamParam)
{
    static const char* paths[2][5] = {
        { "Client/Bin/Resource/Texture/Object/Minion_Order/Melee/order_melee_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Order/Ranged/order_ranged_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Order/Siege/order_siege_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Order/Super/order_super_textured.wmesh",
          kTibbersModelPath },
        { "Client/Bin/Resource/Texture/Object/Minion_Chaos/melee/chaos_melee_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Chaos/ranged/chaos_ranged_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Chaos/siege/chaos_siege_textured.wmesh",
          "Client/Bin/Resource/Texture/Object/Minion_Chaos/super/chaos_super_textured.wmesh",
          kTibbersModelPath },
    };
    uint32_t ti = (eTeamParam == eMinionTeam::Blue) ? 0 : 1;
    uint32_t mi = static_cast<uint32_t>(eType);
    if (mi >= 5) return nullptr;
    return paths[ti][mi];
}

void CMinion_Manager::QueueNetworkVisual(EntityID entity,
    eMinionType eType, eMinionTeam eTeam)
{
    if (!m_pWorld || entity == NULL_ENTITY)
        return;

    if (static_cast<u32_t>(eType) >= static_cast<u32_t>(eMinionType::End))
        eType = eMinionType::Melee;

    if (eTeam == eMinionTeam::End)
        eTeam = eMinionTeam::Blue;

    if (m_pWorld->HasComponent<RenderComponent>(entity))
    {
        auto& rc = m_pWorld->GetComponent<RenderComponent>(entity);
        if (rc.pRenderer)
            return;
    }

    for (NetworkVisualRequest& request : m_deqPendingNetworkVisuals)
    {
        if (request.entity != entity)
            continue;
        request.type = eType;
        request.team = eTeam;
        return;
    }
    //Minion Infomation type create
    NetworkVisualRequest  request{};

    request.entity = entity;
    request.type = eType;
    request.team = eTeam;
    //push to deque
    m_deqPendingNetworkVisuals.push_back(request);
}

uint32_t CMinion_Manager::ProcessQueueNetworkVisual(uint32_t maxCreates)
{
    if (!m_pWorld || maxCreates == 0)
        return 0u;

    uint32_t createdCount = 0u;
    uint32_t skippedCount = 0u;
    uint32_t failedCount = 0u;
    //Create Minion Procedually
    while (!m_deqPendingNetworkVisuals.empty() &&
        createdCount < maxCreates)
    {
        const NetworkVisualRequest request = m_deqPendingNetworkVisuals.front();
        m_deqPendingNetworkVisuals.pop_front();

        if (request.entity == NULL_ENTITY ||
            !m_pWorld->IsAlive(request.entity))
        {
            ++skippedCount;
            continue;
        }
        if (m_pWorld->HasComponent<RenderComponent>(request.entity))
        {
            auto& rc = m_pWorld->GetComponent<RenderComponent>(request.entity);
            if (rc.pRenderer)
            {
                ++skippedCount;
                continue;
            }
        }
        if (Ensure_NetworkVisual(request.entity, request.type, request.team))
            ++createdCount;
        else
            ++failedCount;
    }
    WINTERS_PROFILE_COUNT("MinionVisual::Queue", static_cast<uint64_t>(m_deqPendingNetworkVisuals.size()));
    WINTERS_PROFILE_COUNT("MinionVisual::Created", createdCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Failed", failedCount);

    return createdCount;
}

// ?????????????????????????????????????????????????????????????
// ?몄쭛 API
// ?????????????????????????????????????????????????????????????
i32_t CMinion_Manager::Add_Waypoint(eMinionTeam team, eMinionWay lane, const Vec3& pos)
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return -1;
    m_vecWaypoints[t][l].push_back(pos);
    return static_cast<i32_t>(m_vecWaypoints[t][l].size()) - 1;
}

bool_t CMinion_Manager::Remove_Waypoint(eMinionTeam team, eMinionWay lane, u32_t index)
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return false;
    auto& v = m_vecWaypoints[t][l];
    if (index >= v.size()) return false;
    v.erase(v.begin() + index);
    return true;
}

bool_t CMinion_Manager::Set_Waypoint(eMinionTeam team, eMinionWay lane, u32_t index, const Vec3& pos)
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return false;
    auto& v = m_vecWaypoints[t][l];
    if (index >= v.size()) return false;
    v[index] = pos;
    return true;
}

void CMinion_Manager::Clear_Waypoints(eMinionTeam team, eMinionWay lane)
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return;
    m_vecWaypoints[t][l].clear();
}

u32_t CMinion_Manager::Get_WaypointCount(eMinionTeam team, eMinionWay lane) const
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return 0;
    return static_cast<u32_t>(m_vecWaypoints[t][l].size());
}

const Vec3* CMinion_Manager::Get_WaypointPtr(eMinionTeam team, eMinionWay lane, u32_t index) const
{
    const u32_t t = static_cast<u32_t>(team);
    const u32_t l = static_cast<u32_t>(lane);
    if (t >= 2 || l >= 3) return nullptr;
    const auto& v = m_vecWaypoints[t][l];
    if (index >= v.size()) return nullptr;
    return &v[index];
}

// ?????????????????????????????????????????????????????????????
// LoadDefaults ??湲곗〈 ?섎뱶肄붾뵫 媛믪쓣 珥덇린 遺?몄뒪?몃옪?쇰줈
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::LoadDefaults()
{
    for (auto& row : m_vecWaypoints) for (auto& v : row) v.clear();

    const Vec3 defBlueTop[] = { {-65,0,-55},{-55,0,0},{0,0,55},{55,0,55},{65,0,55} };
    const Vec3 defBlueMid[] = { {-60,0,-60},{0,0,0},{60,0,60} };
    const Vec3 defBlueBot[] = { {-65,0,-55},{-55,0,-55},{0,0,-55},{55,0,0},{65,0,55} };
    const Vec3 defRedTop [] = { {65,0,55},{55,0,55},{0,0,55},{-55,0,0},{-65,0,-55} };
    const Vec3 defRedMid [] = { {60,0,60},{0,0,0},{-60,0,-60} };
    const Vec3 defRedBot [] = { {65,0,55},{55,0,0},{0,0,-55},{-55,0,-55},{-65,0,-55} };

    auto fill = [](std::vector<Vec3>& dst, const Vec3* src, size_t n)
    { dst.assign(src, src + n); };

    fill(m_vecWaypoints[0][0], defBlueTop, std::size(defBlueTop));
    fill(m_vecWaypoints[0][1], defBlueMid, std::size(defBlueMid));
    fill(m_vecWaypoints[0][2], defBlueBot, std::size(defBlueBot));
    fill(m_vecWaypoints[1][0], defRedTop,  std::size(defRedTop));
    fill(m_vecWaypoints[1][1], defRedMid,  std::size(defRedMid));
    fill(m_vecWaypoints[1][2], defRedBot,  std::size(defRedBot));
}

// ?????????????????????????????????????????????????????????????
// Save / Load (Stage1.dat ?듯빀)
// ?????????????????????????????????????????????????????????????
HRESULT CMinion_Manager::Save_ToFile(FILE* pFile) const
{
    if (!pFile) return E_FAIL;

    u32_t total = 0;
    for (u32_t t = 0; t < 2; ++t)
        for (u32_t l = 0; l < 3; ++l)
            total += static_cast<u32_t>(m_vecWaypoints[t][l].size());
    fwrite(&total, sizeof(u32_t), 1, pFile);

    for (u32_t t = 0; t < 2; ++t)
    for (u32_t l = 0; l < 3; ++l)
    {
        const auto& v = m_vecWaypoints[t][l];
        for (u32_t i = 0; i < v.size(); ++i)
        {
            Winters::Map::MinionWaypointEntry e{};
            e.team = t; e.lane = l; e.order = i;
            e.px   = v[i].x; e.py = v[i].y; e.pz = v[i].z;
            fwrite(&e, sizeof(e), 1, pFile);
        }
    }
    return S_OK;
}

HRESULT CMinion_Manager::Load_FromFile(FILE* pFile)
{
    if (!pFile) return E_FAIL;

    for (auto& row : m_vecWaypoints) for (auto& v : row) v.clear();

    u32_t count = 0;
    if (fread(&count, sizeof(u32_t), 1, pFile) != 1) return E_FAIL;

    for (u32_t i = 0; i < count; ++i)
    {
        Winters::Map::MinionWaypointEntry e{};
        if (fread(&e, sizeof(e), 1, pFile) != 1) return E_FAIL;
        if (e.team >= 2 || e.lane >= 3) continue;
        m_vecWaypoints[e.team][e.lane].push_back({ e.px, e.py, e.pz });
    }
    if (count == 0) LoadDefaults();   // 鍮??ㅽ뀒?댁? 諛⑹?
    return S_OK;
}

// ?????????????????????????????????????????????????????????????
// OnImGui_Tuner ??硫ㅻ쾭 踰≫꽣 湲곕컲
// ?????????????????????????????????????????????????????????????
void CMinion_Manager::OnImGui_Tuner()
{
    if (!ImGui::Begin("Minion Tuner")) { ImGui::End(); return; }

    ImGui::Text("Active minions: %u", Get_Count());
    ImGui::Checkbox("Enabled", &m_bEnabled);
    ImGui::SliderFloat("Spawn Interval (s)", &m_fSpawnInterval, 1.f, 30.f);
    if (ImGui::DragFloat("Visual Scale", &m_fVisualScale, 0.0001f, 0.001f, 0.02f, "%.4f"))
    {
        if (m_pWorld)
        {
            for (EntityID entity : m_vecEntities)
            {
                if (m_pWorld->IsAlive(entity) &&
                    m_pWorld->HasComponent<TransformComponent>(entity))
                {
                    m_pWorld->GetComponent<TransformComponent>(entity).SetScale(m_fVisualScale);
                }
            }
        }
    }
    if (ImGui::Button("Reset Visual Scale"))
    {
        m_fVisualScale = kDefaultMinionScale;
        if (m_pWorld)
        {
            for (EntityID entity : m_vecEntities)
            {
                if (m_pWorld->IsAlive(entity) &&
                    m_pWorld->HasComponent<TransformComponent>(entity))
                {
                    m_pWorld->GetComponent<TransformComponent>(entity).SetScale(m_fVisualScale);
                }
            }
        }
    }

    if (ImGui::Button("Spawn Wave Now")) DEBUG_SpawnWaveNow();
    ImGui::SameLine();
    if (ImGui::Button("Clear All"))      Clear();
    ImGui::SameLine();
    if (ImGui::Button("Reset Defaults")) LoadDefaults();

    ImGui::Separator();

    auto DrawLane = [&](const char* title, u32_t t, u32_t l)
    {
        if (!ImGui::CollapsingHeader(title)) return;
        ImGui::PushID(title);
        auto& v = m_vecWaypoints[t][l];
        for (i32_t i = 0; i < static_cast<i32_t>(v.size()); ++i)
        {
            ImGui::PushID(i);
            char lbl[32]; snprintf(lbl, sizeof(lbl), "WP[%d]", i);
            ImGui::DragFloat3(lbl, &v[i].x, 0.5f, -500.f, 500.f, "%.1f");
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) { v.erase(v.begin() + i); ImGui::PopID(); break; }
            ImGui::PopID();
        }
        if (ImGui::Button("+ Add (0,0,0)")) v.push_back({ 0.f, 0.f, 0.f });
        ImGui::PopID();
    };

    DrawLane("Blue->Red Top", 0, 0);
    DrawLane("Blue->Red Mid", 0, 1);
    DrawLane("Blue->Red Bot", 0, 2);
    DrawLane("Red->Blue Top", 1, 0);
    DrawLane("Red->Blue Mid", 1, 1);
    DrawLane("Red->Blue Bot", 1, 2);

    ImGui::End();
}
