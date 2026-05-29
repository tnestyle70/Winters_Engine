#pragma once

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "ECS/Entity.h"

#include <cstdint>
#include <memory>

struct IWalkableQuery
{
    virtual ~IWalkableQuery() = default;

    virtual bool_t IsWalkableXZ(const Vec3& pos) const = 0;
    virtual bool_t SegmentWalkableXZ(const Vec3& from, const Vec3& to, f32_t radiusWorld = 0.f) const = 0;
    virtual bool_t TryClampMoveSegmentXZ(const Vec3& vFrom, const Vec3& vDesired, f32_t fRadiusWorld, Vec3& vOutPosition) const = 0;
    virtual bool_t TryResolveMoveTarget(const Vec3& from, const Vec3& rawTarget, Vec3& outTarget) const = 0;
    virtual bool_t TryBuildMovePath(
        const Vec3& from,
        const Vec3& rawTarget,
        Vec3* pOutWaypoints,
        u16_t maxWaypoints,
        u16_t& outWaypointCount,
        Vec3& outTarget) const = 0;
    virtual bool_t TrySampleHeight(f32_t x, f32_t z, f32_t& outY) const = 0;
};

struct LagCompensatedEntityState
{
    Vec3 vPosition{};
    f32_t  fHp = 0.f;
    bool_t bIsDead = false;
};

struct ILagCompensationQuery
{
    virtual ~ILagCompensationQuery() = default;
    virtual bool_t TryGetHistoricalState(
        EntityID entity,
        u64_t rewindTicks,
        LagCompensatedEntityState& outState) const = 0;
};

struct TickContext
{
    uint64_t tickIndex = 0;
    f32_t fDt = DeterministicTime::kFixedDt;
    f64_t fSimulatedTimeSec = 0;
    DeterministicRng* pRng = nullptr;
    EntityIdMap* pEntityMap = nullptr;
    EntityID localPlayer = NULL_ENTITY;
    const IWalkableQuery* pWalkable = nullptr;
    const ILagCompensationQuery* pLagCompensation = nullptr;
};

enum class eCommandKind : uint8_t
{
    None = 0,
    Move = 1,
    CastSkill = 2,
    BasicAttack = 3,
    LevelSkill = 4,
    BuyItem = 5,
    UseItem = 6,
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9,
};

struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
};

struct GameCommand
{
    eCommandKind kind = eCommandKind::None;
    EntityID issuerEntity = NULL_ENTITY;
    uint64_t issuedAtTick = 0;
    uint32_t sequenceNum = 0;
    u64_t rewindTicks = 0;
    uint8_t slot = 0;
    EntityID targetEntity = NULL_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
};

class ICommandExecutor
{
public:
    virtual ~ICommandExecutor() = default;

    virtual void ExecuteCommand(CWorld& world, const TickContext& tc,
        const GameCommand& cmd) = 0;
};

class CDefaultCommandExecutor final : public ICommandExecutor
{
public:
    static std::unique_ptr<CDefaultCommandExecutor> Create();
    ~CDefaultCommandExecutor() override = default;

    void ExecuteCommand(CWorld& world, const TickContext& tc,
        const GameCommand& cmd) override;

private:
    CDefaultCommandExecutor() = default;

    void HandleMove(CWorld&, const TickContext&, const GameCommand&);
    void HandleCastSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBasicAttack(CWorld&, const TickContext&, const GameCommand&);
    void HandleLevelSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBuyItem(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
    void HandleAIDebugControl(CWorld&, const TickContext&, const GameCommand&);
};

GameCommand BuildServerCommand(const GameCommandWire& wire,
    uint32_t sessionId, EntityID controlledEntity,
    const EntityIdMap& map);
