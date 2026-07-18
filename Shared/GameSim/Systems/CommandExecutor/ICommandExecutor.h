#pragma once

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

#include <cstdint>
#include <memory>

struct GameplayDefinitionPack;

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
    virtual bool_t TryGetHistoricalStateAtTick(
        EntityHandle hEntity,
        u64_t uExpectedTick,
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
    const GameplayDefinitionPack* pDefinitions = nullptr;
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
    Flash = 10,
    CompanionCommand = 11,
    PracticeControl = 12,
    ReorderItem = 13,
};

enum class eCompanionCommandMode : uint16_t
{
    Move = 0,
    Attack = 1,
};

enum class ePracticeOperation : uint16_t
{
    None = 0,
    SetEnabled = 1,
    SetOptions = 2,
    RestoreHealthMana = 3,
    ResetCooldowns = 4,
    AddGold = 5,
    SetLevel = 6,
    Teleport = 7,
    SpawnMinion = 8,
    ClearPracticeSpawns = 9,
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11,
    SetSimulationPaused = 12,
    StepSimulationTicks = 13,
    SetSimulationTimeScale = 14,
    RewindSimulationSeconds = 15,
    SpawnChampion = 16,
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
    TakeControlRosterChampion = 21,
    ReplaceControlledChampion = 22,
    ApplyStructureStatOverride = 23,
    ClearStructureStatOverrides = 24,
    ReloadGameplayDefinitions = 25,
    Count = 26,
};

// ApplyChampionStatOverride: slot = eChampionStatOverrideId, value = 대체값, targetNet 지정 가능.
// ApplyItemStatOverride: flags = (itemId << 8) | eItemStatOverrideField, value = 대체값.
// TakeControlRosterChampion: targetNet = authoritative 10-player roster bot NetId.
// ReplaceControlledChampion: flags = eChampion; replace only the current human slot.
// ReloadGameplayDefinitions: 서버가 진실 JSON 3종을 재파싱해 활성 정의 팩을 교체 (Debug 전용, value/flags 미사용).
enum class eChampionStatOverrideId : uint8_t
{
    None = 0,
    BaseHp = 1,
    HpPerLevel = 2,
    BaseMana = 3,
    ManaPerLevel = 4,
    BaseAd = 5,
    AdPerLevel = 6,
    BaseAp = 7,
    ApPerLevel = 8,
    BaseArmor = 9,
    ArmorPerLevel = 10,
    BaseMr = 11,
    MrPerLevel = 12,
    BaseAttackSpeed = 13,
    AttackSpeedPerLevel = 14,
    BaseMoveSpeed = 15,
    BaseAttackRange = 16,
    // Practice-only final value applied after level, item, rune, and buff modifiers.
    EffectiveAttackSpeed = 17,
    Count = 18,
};

// ApplyStructureStatOverride: slot = eStructureStatOverrideId, value = 대체값.
//   MaxHp 계열은 해당 kind 의 살아있는 모든 구조물의 현재/최대 체력을 함께 재설정한다.
//   TurretAttackDamage 는 모든 TurretAIComponent 의 attackDamage 를 재설정한다.
// ClearStructureStatOverrides: 정의 팩 기본값(SpawnObjectGameplayDefs)으로 복원.
enum class eStructureStatOverrideId : uint8_t
{
    None = 0,
    TurretMaxHp = 1,
    InhibitorMaxHp = 2,
    NexusMaxHp = 3,
    TurretAttackDamage = 4,
    Count = 5,
};

enum class eItemStatOverrideField : uint8_t
{
    None = 0,
    Price = 1,
    FlatAd = 2,
    FlatAp = 3,
    FlatHealth = 4,
    FlatMana = 5,
    FlatArmor = 6,
    FlatMr = 7,
    BonusAttackSpeed = 8,
    CritChance = 9,
    AbilityHaste = 10,
    FlatMoveSpeed = 11,
    LifeSteal = 12,
    FlatMagicPen = 13,
    Lethality = 14,
    CritDamageBonus = 15,
    PercentMoveSpeed = 16,
    ArmorPenPercent = 17,
    BonusArmorPenPercent = 18,
    MagicPenPercent = 19,
    Count = 20,
};

inline constexpr u32_t kPracticeInfiniteHealthFlag = 1u << 0;
inline constexpr u32_t kPracticeInfiniteManaFlag = 1u << 1;
inline constexpr u32_t kPracticeNoCooldownFlag = 1u << 2;
inline constexpr u32_t kPracticeInfiniteGoldFlag = 1u << 3;
inline constexpr u32_t kPracticeAllOptionFlags =
    kPracticeInfiniteHealthFlag |
    kPracticeInfiniteManaFlag |
    kPracticeNoCooldownFlag |
    kPracticeInfiniteGoldFlag;

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
    ePracticeOperation practiceOperation = ePracticeOperation::None;
    f32_t practiceValue = 0.f;
    u32_t practiceFlags = 0u;
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
    uint32_t sourceSessionId = 0u;
    ePracticeOperation practiceOperation = ePracticeOperation::None;
    f32_t practiceValue = 0.f;
    u32_t practiceFlags = 0u;
};

// Stable command-execution outcomes are consumed by server feedback and AI
// research traces. Keep the explicit values append-only once exported.
enum class eCommandExecutionState : uint8_t
{
    Unknown = 0u,
    Accepted = 1u,
    Rejected = 2u,
};

enum class eCommandExecutionReason : uint16_t
{
    None = 0u,
    UnsupportedCommand = 1u,
    InvalidIssuer = 2u,
    IssuerNotAlive = 3u,
    StateBlocked = 4u,
    ActionBlocked = 5u,
    PossessionPending = 6u,
    InvalidPayload = 7u,
    MissingComponent = 8u,
    InvalidTarget = 9u,
    DeadTarget = 10u,
    UntargetableTarget = 11u,
    FriendlyTarget = 12u,
    Cooldown = 13u,
    UnlearnedSkill = 14u,
    InvalidSkillStage = 15u,
    InsufficientResource = 16u,
    OutOfRange = 17u,
    NavigationBlocked = 18u,
    NoActiveRecall = 19u,
    MissingSummonerSpell = 20u,
    ChampionRuleBlocked = 21u,
    CarriedStateBlocked = 22u,
};

struct CommandExecutionResult
{
    eCommandExecutionState state = eCommandExecutionState::Unknown;
    eCommandExecutionReason reason = eCommandExecutionReason::None;
    uint32_t commandSequence = 0u;
    Vec3 resolvedPosition{};

    static CommandExecutionResult Unknown(
        uint32_t sequence,
        eCommandExecutionReason why = eCommandExecutionReason::UnsupportedCommand)
    {
        return { eCommandExecutionState::Unknown, why, sequence, {} };
    }

    static CommandExecutionResult Accepted(
        uint32_t sequence,
        const Vec3& position = {})
    {
        return {
            eCommandExecutionState::Accepted,
            eCommandExecutionReason::None,
            sequence,
            position
        };
    }

    static CommandExecutionResult Rejected(
        uint32_t sequence,
        eCommandExecutionReason why)
    {
        return { eCommandExecutionState::Rejected, why, sequence, {} };
    }
};

struct SkillCommandFeedback
{
    CommandExecutionResult result{};
    u8_t authoritativeSkillSlot = 0xFFu;
    u8_t authoritativeSkillStage = 0u;
    u64_t stageWindowEndTick = 0u;
};

class ICommandExecutor
{
public:
    virtual ~ICommandExecutor() = default;

    virtual CommandExecutionResult ExecuteCommand(CWorld& world, const TickContext& tc,
        const GameCommand& cmd) = 0;
};

class CDefaultCommandExecutor final : public ICommandExecutor
{
public:
    static std::unique_ptr<CDefaultCommandExecutor> Create();
    ~CDefaultCommandExecutor() override = default;

    CommandExecutionResult ExecuteCommand(CWorld& world, const TickContext& tc,
        const GameCommand& cmd) override;

private:
    CDefaultCommandExecutor() = default;

    CommandExecutionResult HandleMove(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleCastSkill(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleBasicAttack(CWorld&, const TickContext&, const GameCommand&);
    void HandleLevelSkill(CWorld&, const TickContext&, const GameCommand&);
    void HandleBuyItem(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleUseItem(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleReorderItem(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
    void HandleAIDebugControl(CWorld&, const TickContext&, const GameCommand&);
    CommandExecutionResult HandleFlash(CWorld&, const TickContext&, const GameCommand&);
    void HandleCompanionCommand(CWorld&, const TickContext&, const GameCommand&);
};

GameCommand BuildServerCommand(const GameCommandWire& wire,
    uint32_t sessionId, EntityID controlledEntity,
    const EntityIdMap& map);

void ExecuteExpiredSkillCharges(
    ICommandExecutor& executor,
    CWorld& world,
    const TickContext& tc);
