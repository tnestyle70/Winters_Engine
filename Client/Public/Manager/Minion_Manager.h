#pragma once
#include "Defines.h"
#include "Renderer/ModelRenderer.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include <cstdio>
#include <deque>
#include <unordered_map>

enum class eMinionType : uint32_t { Melee = 0, Ranged = 1, Siege = 2, Super = 3, Tibbers = 4, End };
enum class eMinionTeam : uint32_t { Blue = 0, Red = 1, End };
enum class eMinionWay : uint32_t { Top = 0, Mid = 1, Bottom = 2, End };

struct MinionStateComponent;
struct RenderComponent;

class CMinion_Manager final
{
    DECLARE_SINGLETON(CMinion_Manager)
    CMinion_Manager() = default;

public:
    ~CMinion_Manager() = default;

    HRESULT Initialize(CWorld* pWorld);
    void    Shutdown();

    void    Tick(f32_t fDeltaTime);
    void    TickVisuals(f32_t fDeltaTime, const Mat4* pViewProj = nullptr);
    void    Render(const Mat4& matVP, const Vec3& vCameraWorld = Vec3{},
        void* pAmbientOcclusionSRV = nullptr,
        bool_t bIgnoreFogOfWar = false);
    u32_t   AppendRenderSnapshotMeshes(RenderWorldSnapshot& snapshot,
        const Mat4& matVP,
        bool_t bIgnoreFogOfWar = false);
    void    Clear();

    void    Set_Enabled(bool_t b) { m_bEnabled = b; m_fSpawnTimer = 0.f; }
    bool_t  Get_Enabled() const { return m_bEnabled; }
    void    Set_SpawnInterval(f32_t s) { m_fSpawnInterval = s; }
    f32_t   Get_SpawnInterval() const { return m_fSpawnInterval; }
    uint32_t Get_Count() const { return static_cast<uint32_t>(m_vecEntities.size()); }

    void    OnImGui_Tuner();
    void    DEBUG_SpawnWaveNow();
    void PrewarmNetworkVisualResources();

    static void GetWayPoints(eMinionTeam eTeam, eMinionWay eWay,
        const Vec3** ppOut, uint32_t* pCountOut);
    static const char* ResolveModelPath(eMinionType eType, eMinionTeam eTeam);
    void QueueNetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeam);
    uint32_t ProcessQueueNetworkVisual(uint32_t maxCreates);
    uint32_t GetQueuedNetworkVisualCount() const {
        return static_cast<uint32_t>(m_deqPendingNetworkVisuals.size());
    }
    bool_t  Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeam);
    void    Release_NetworkVisual(EntityID entity);

public:
    // 파일 I/O (Stage1.dat 에서 CMapDataIO 가 호출)
    HRESULT Save_ToFile(FILE* pFile) const;
    HRESULT Load_FromFile(FILE* pFile);
    void    LoadDefaults();   // 파일 없거나 로드 실패 시 기존 하드코딩 값으로 부트스트랩

    // 에디터 편집 API
    i32_t  Add_Waypoint(eMinionTeam team, eMinionWay lane, const Vec3& pos);   // 레인 끝에 추가, 인덱스 반환
    bool_t Remove_Waypoint(eMinionTeam team, eMinionWay lane, u32_t index);
    bool_t Set_Waypoint(eMinionTeam team, eMinionWay lane, u32_t index, const Vec3& pos);
    void   Clear_Waypoints(eMinionTeam team, eMinionWay lane);
    u32_t  Get_WaypointCount(eMinionTeam team, eMinionWay lane) const;
    const Vec3* Get_WaypointPtr(eMinionTeam team, eMinionWay lane, u32_t index) const;

    // Editor 선택 상태 (에디터가 이 값을 통해 "현재 편집 중인 레인" 지정)
    void Set_EditLane(eMinionTeam team, eMinionWay lane) { m_EditTeam = team; m_EditLane = lane; }
    eMinionTeam Get_EditTeam() const { return m_EditTeam; }
    eMinionWay  Get_EditLane() const { return m_EditLane; }

private:
    // 6 레인 × N 웨이포인트 (static g_Waypoints_* 대체)
    // [team(2)][lane(3)] — Blue/Red × Top/Mid/Bot
    std::vector<Vec3> m_vecWaypoints[2][3];

    eMinionTeam m_EditTeam = eMinionTeam::Blue;
    eMinionWay  m_EditLane = eMinionWay::Mid;

    // 순차 스폰 (라운드 단위)
    static constexpr i32_t kRoundsPerWave = 6;   // 근접3 + 원거리3
    i32_t m_iCurrentRound = kRoundsPerWave;   // 시작 시 "대기" 상태
    f32_t m_fNextRoundCountdown = 0.f;
    f32_t m_fSpawnDelay = 0.5f;   // 라운드 간 간격

    i32_t m_iWalkAnimIndex = 0;           // 애니 런타임 

private:
    enum class eMinionVisualPhase : uint8_t
    {
        Base,
        Attack,
        Recover,
        Death
    };

    struct MinionVisualPlaybackState
    {
        eMinionVisualPhase phase = eMinionVisualPhase::Base;
        uint32_t lastActionSeq = 0;
        uint16_t lastAnimId = 0;
        uint8_t baseState = 0xff;
        f32_t phaseTimer = 0.f;
        bool_t bPendingAttack = false;
    };

    struct NetworkVisualRequest
    {
        EntityID entity = NULL_ENTITY;
        eMinionType type = eMinionType::Melee;
        eMinionTeam team = eMinionTeam::Blue;
    };

    void     DoSpawnWave();
    EntityID Spawn_Minion(eMinionType eType, eMinionTeam eTeam, eMinionWay eWay);
    std::unique_ptr<ModelRenderer> AcquireNetworkRenderer(
        eMinionType eType,
        eMinionTeam eTeam,
        const char* pPath);
    void PoolNetworkRenderer(
        eMinionType eType,
        eMinionTeam eTeam,
        std::unique_ptr<ModelRenderer> pRenderer);
    void     UpdateMinionVisual(EntityID entity,
        MinionStateComponent& ms,
        RenderComponent& rc,
        f32_t fDeltaTime);

    CWorld* m_pWorld = nullptr;
    std::vector<EntityID>                                        m_vecEntities;
    std::vector<EntityID>                                        m_vecSpawnedThisTick;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, MinionVisualPlaybackState>       m_mapVisualStates;
    std::deque<NetworkVisualRequest> m_deqPendingNetworkVisuals;
    std::vector<std::unique_ptr<ModelRenderer>> m_vecNetworkRendererPool[2][5];
    u32_t m_uNetworkPoolHitsThisFrame = 0u;
    u32_t m_uNetworkColdCreatesThisFrame = 0u;

    f32_t   m_fSpawnTimer = 0.f;
    f32_t   m_fSpawnInterval = 20.f;
    f32_t   m_fVisualScale = 0.006f;
    bool_t  m_bEnabled = false;
    bool_t  m_bInitialized = false;
};
