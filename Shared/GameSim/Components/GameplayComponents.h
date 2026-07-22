#pragma once
#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include <cstddef>
#include <deque>
#include <type_traits>

// ─────────────────────────────────────────────────────────────
// Gameplay Components — Phase B-6 ECS 마이그레이션
//
// 네트워크 친화 POD 설계:
//  - RenderComponent is split into ECS/Components/RenderComponent.h (client-only)
//  - 값 멤버만 Shared/Schemas/*.fbs 스키마와 1:1 매핑 예정
//  - Transform은 TransformComponent.h 단일 소스
// ─────────────────────────────────────────────────────────────

// 팀 구분 (Blue/Red/Neutral)
enum class eTeam : uint8_t
{
    Blue = 0,
    Red = 1,
    Neutral = 2,
    TEAM_END
};

// 맵 오브젝트 카테고리
enum class eMapObjectKind : uint8_t
{
    Structure = 0,   // Nexus / Inhibitor / Turret
    Jungle = 1,   // Baron / Dragon / Blue / Krug / Gromp / Wolf
    Minion = 2,   // Melee / Ranged / Siege / Super
    KIND_END
};

// 챔피언 — LoL 5v5 플레이어 제어 엔티티
// Shared/Schemas/Components.fbs의 ChampionSnapshot과 1:1 매핑 예정
// ─────────────────────────────────────────────────────────────
struct GameplayContentId
{
    u8_t value = 255u;

    constexpr GameplayContentId() = default;
    explicit constexpr GameplayContentId(u8_t iValue) : value(iValue) {}

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    explicit constexpr GameplayContentId(T id) : value(static_cast<u8_t>(id)) {}

    GameplayContentId& operator=(u8_t iValue)
    {
        value = iValue;
        return *this;
    }

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    GameplayContentId& operator=(T id)
    {
        value = static_cast<u8_t>(id);
        return *this;
    }

    constexpr operator u8_t() const { return value; }

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    constexpr operator T() const { return static_cast<T>(value); }
};

constexpr bool operator==(GameplayContentId lhs, GameplayContentId rhs)
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(GameplayContentId lhs, GameplayContentId rhs)
{
    return !(lhs == rhs);
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool operator==(GameplayContentId lhs, T rhs)
{
    return lhs.value == static_cast<u8_t>(rhs);
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool operator==(T lhs, GameplayContentId rhs)
{
    return static_cast<u8_t>(lhs) == rhs.value;
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool operator!=(GameplayContentId lhs, T rhs)
{
    return !(lhs == rhs);
}

template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool operator!=(T lhs, GameplayContentId rhs)
{
    return !(lhs == rhs);
}

inline constexpr GameplayContentId kInvalidGameplayContentId{ static_cast<u8_t>(255u) };

struct ChampionComponent
{
    GameplayContentId id = kInvalidGameplayContentId;
    eTeam     team = eTeam::Blue;
    f32_t     hp = 100.f;
    f32_t     maxHp = 100.f;
    f32_t     mana = 100.f;
    f32_t     maxMana = 100.f;
    f32_t shield = 0.f;
    f32_t     moveSpeed = 8.f;
    f32_t     cooldowns[4]{ 0.f, 0.f, 0.f, 0.f };   // Q/W/E/R
    uint8_t   level = 1;
};

struct ExperienceComponent
{
    f32_t current = 0.f;
    f32_t requiredForNextLevel = 280.f;
    f32_t total = 0.f;
    u8_t  level = 1;
};

// 로컬 플레이어 식별용 마커 (Tier 1: GameInstance.SelectedChampion과는 다른 레이어)
struct LocalPlayerTag {};

// ─────────────────────────────────────────────────────────────
// 미니언 — LoL 3라인 웨이브
// ─────────────────────────────────────────────────────────────
struct MinionComponent
{
    eTeam   team = eTeam::Blue;
    uint8_t laneType = 0;     // 0=Top, 1=Mid, 2=Bot
    uint8_t roleType = 0;     // 0=Melee, 1=Ranged, 2=Siege, 3=Super
    f32_t   hp = 100.f;
    f32_t   maxHp = 100.f;
};

// ─────────────────────────────────────────────────────────────
// 포탑 — 고정 위치 + 이벤트 기반 동기화
// ─────────────────────────────────────────────────────────────
struct TurretComponent
{
    eTeam    team = eTeam::Blue;
    f32_t    hp = 3000.f;
    f32_t    maxHp = 3000.f;
    EntityID targetId = NULL_ENTITY;
    uint8_t  laneType = 0;
    uint8_t  tier = 0;        // 0=Outer, 1=Inner, 2=Inhib, 3=Nexus
};

// 정글 몬스터 / 구조물 (Nexus/Inhibitor) 공통 태그
struct TurretAIComponent
{
    EntityID attackTargetId = NULL_ENTITY;
    EntityID aggroTargetId = NULL_ENTITY;
    f32_t attackRange = 7.75f;
    f32_t attackCooldown = 0.f;
    f32_t attackCooldownMax = 1.0f;
    f32_t attackDamage = 150.f;
    f32_t projectileSpeed = 18.f;
    f32_t aggroLockTimer = 0.f;
    bool_t bActive = true;
    u8_t reservedTail[3]{};
};

static_assert(sizeof(TurretAIComponent) == 36u);

struct StructureProjectileComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    EntityID targetEntity = NULL_ENTITY;
    EntityHandle sourceHandle = NULL_ENTITY_HANDLE;
    EntityHandle targetHandle = NULL_ENTITY_HANDLE;
    u32_t uProjectileNetAtSpawn = 0u;
    u32_t uSourceNetAtSpawn = 0u;
    u32_t uTargetNetAtSpawn = 0u;
    u16_t uContactOrdinal = 0u;
    Vec3 currentPos{};
    Vec3 direction{ 0.f, 0.f, 1.f };
    f32_t maxDistance = 48.f;
    f32_t traveledDistance = 0.f;
    f32_t speed = 18.f;
    f32_t damage = 150.f;
    f32_t hitRadius = 0.35f;
};

struct TowerAggroNotifyComponent
{
    EntityID attackerEntity = NULL_ENTITY;
    EntityID victimEntity = NULL_ENTITY;
    f32_t priorityDuration = 2.0f;
};

struct JungleMonsterTag { u8_t reserved = 0u; };
struct NexusTag { u8_t reserved = 0u; };
struct InhibitorTag { u8_t reserved = 0u; };

struct InhibitorRespawnComponent
{
    f32_t fRespawnDelaySec = 300.f;
    f32_t fRespawnTimerSec = 0.f;
    bool_t bRespawnPending = false;
    u8_t reserved[3]{};
};

static_assert(sizeof(JungleMonsterTag) == 1u);
static_assert(sizeof(NexusTag) == 1u);
static_assert(sizeof(InhibitorTag) == 1u);
static_assert(sizeof(InhibitorRespawnComponent) == 12u);
static_assert(std::is_trivially_copyable_v<InhibitorRespawnComponent>);

// ─────────────────────────────────────────────────────────────
// 맵 오브젝트 일반 메타 — 에디터 Inspector / Save·Load 재사용
// ─────────────────────────────────────────────────────────────
struct MapObjectComponent
{
    eMapObjectKind kind = eMapObjectKind::Structure;
    const char*    name = nullptr;       // ObjectLayout.txt 매칭 키
    const char*    category = nullptr;
};

// ─────────────────────────────────────────────────────────────
// 서버 권위 ID 매핑 — Phase 4 네트워크 통합 시점에 사용
//   serverEntityId == 0: 로컬 전용 엔티티 (이펙트 더미 등)
//   serverEntityId != 0: 서버가 발급한 권위 ID
// ─────────────────────────────────────────────────────────────
struct ServerIdComponent
{
    uint32_t serverEntityId = 0;
};

// Yasuo Q variant state and third-cast tornado counter.
struct YasuoStateComponent
{
    uint8_t qStackCount = 0;
    u8_t reservedQStackAlignment[3]{};
    f32_t qStackTimer = 0.f;
    bool bEActive = false;
    u8_t reservedEActiveAlignment[3]{};
    f32_t eActiveTimer = 0.f;

    f32_t fPassiveFlow = 0.f;
    f32_t fPassiveFlowMax = 100.f;
    f32_t fPassiveShieldRemaining = 0.f;
    f32_t fPassiveShieldMax = 125.f;
    f32_t fPassiveShieldTimer = 0.f;
    f32_t fPassiveLastX = 0.f;
    f32_t fPassiveLastZ = 0.f;
    bool_t bPassivePositionInitialized = false;
    u8_t reservedPassivePositionAlignment[3]{};
    u64_t uPassiveLastObservedDiscontinuityTick = 0u;
};

static_assert(sizeof(YasuoStateComponent) == 56u);
static_assert(offsetof(YasuoStateComponent, qStackTimer) == 4u);
static_assert(offsetof(YasuoStateComponent, eActiveTimer) == 12u);
static_assert(offsetof(YasuoStateComponent, fPassiveLastX) == 36u);
static_assert(offsetof(YasuoStateComponent, uPassiveLastObservedDiscontinuityTick) == 48u);

//RivenStateComponent - ECS
struct RivenStateComponent
{
    //Q 
    uint8_t qStackCount = 0;
    u8_t reservedQTimerAlignment[3]{};
    f32_t qStackTimer = 0.f;
    //R - Blade of the Exile
    bool bUlted = false;
    bool bWindSlashAvailable = false;
    u8_t reservedUltTimerAlignment[2]{};
    f32_t fUltTimer = 0.f;
    //E 
    f32_t fShieldRemaining = 0.f;
    f32_t fShieldTimer = 0.f;
};

static_assert(sizeof(RivenStateComponent) == 24u);
static_assert(offsetof(RivenStateComponent, qStackTimer) == 4u);
static_assert(offsetof(RivenStateComponent, fUltTimer) == 12u);

struct SkillSlotRuntime {
    f32_t   cooldownRemaining = 0.f;
    f32_t cooldownDuration = 0.f;
    uint8_t currentStage = 0;   // 0 = idle, 1 = stage1 발동 완료 — stage2 대기
    f32_t   stageWindow = 0.f; // currentStage==1 일 때 남은 입력 윈도우
};

struct SkillStateComponent {
    SkillSlotRuntime slots[5];  // BA/Q/W/E/R
};
// 구조물 공통 (Nexus/Inhibitor/Turret)
struct StructureComponent
{
    eTeam    team = eTeam::Blue;
    u8_t     reservedKindAlignment[3]{};
    uint32_t kind = 0;   // Winters::Map::eObjectKind cast
    uint32_t tier = 0;   // Winters::Map::eTurretTier cast
    uint32_t lane = 0;   // Winters::Map::eLane cast
    f32_t    hp = 3000.f;
    f32_t    maxHp = 3000.f;
};

static_assert(sizeof(StructureComponent) == 24u);
static_assert(offsetof(StructureComponent, kind) == 4u);

// 정글 몹 (이름에서 중복 C 제거)
struct JungleComponent
{
    uint32_t subKind = 0;   // eJungleSub static_cast
    uint32_t campId = 0;
    f32_t    hp = 1000.f;
    f32_t    maxHp = 1000.f;
    Vec3     vSpawnPosition{};
    f32_t    fRespawnDelaySec = 30.f;
    f32_t    fRespawnTimerSec = 0.f;
    bool_t   bRespawnPending = false;
    u8_t     reserved[3]{};
};

enum class eObjectiveBuffKind : u8_t
{
    Baron = 0,
    Elder = 1,
    Blue = 2,
    Red = 3,
    Count = 4,
};

struct ObjectiveBuffComponent
{
    u64_t expireTicks[static_cast<u8_t>(eObjectiveBuffKind::Count)]{};
};

struct ObjectiveBurnState
{
    EntityID source = NULL_ENTITY;
    u64_t expireTick = 0u;
    u64_t nextTick = 0u;
};

struct ObjectiveBurnComponent
{
    ObjectiveBurnState elder{};
    ObjectiveBurnState red{};
};

struct BaronEmpoweredMinionComponent
{
    f32_t baseMaxHp = 0.f;
    f32_t baseAttackDamage = 0.f;
    f32_t hpMultiplier = 1.f;
    f32_t attackDamageMultiplier = 1.f;
    f32_t scaleMultiplier = 1.f;
};

struct MinionStateComponent
{
    enum State : uint8_t
    {
        Idle,
        LaneMove,
        Chase,
        Attack,
        Dead
    };
    State       current = LaneMove;
    State       visualState = Idle;
    u8_t        reservedAttackTargetAlignment[2]{};
    EntityID    attackTargetId = NULL_ENTITY;
    f32_t       attackCooldown = 0.f;
    f32_t       attackDamage = 10.f;
    f32_t       attackRange = 2.f;
    f32_t       attackCooldownMax = 1.f;
    f32_t       sightRange = 15.f;
    f32_t       moveSpeed = 5.f;
    f32_t       attackWindup = 0.35f;
    f32_t       attackRecovery = 0.35f;
    f32_t       attackTimer = 0.f;
    bool_t      bHitFired = false;
    bool_t      bAttackAnimRequested = false;
    u8_t        reservedDeathTimerAlignment[2]{};
    f32_t       deathTimer = 0.f;
    f32_t       targetScanCooldown = 0.f;
    f32_t       targetScanInterval = 0.2f;
    f32_t       animUpdateAccumulator = 0.f;
    f32_t       animUpdateInterval = 1.f / 15.f;

    // 웨이포인트 기반 이동 (Part C 에서 NavAgentComponent 로 교체될 필드)
    uint32_t    currentWaypoint = 0;
    static constexpr uint16_t PathMaxWaypoints = 64;
    Vec3        PathWaypoints[PathMaxWaypoints]{};
    Vec3        PathTarget{};
    Vec3        PathResolvedTarget{};
    uint16_t    PathCount = 0;
    uint16_t    PathIndex = 0;
    float       PathRebuildCooldown = 0.f;
    uint8_t     BlockedMoveFrames = 0;

    // 라인·팀·타입 (Minion_Manager::Tick 이 웨이포인트 배열을 선택할 때 사용)
    eTeam       team = eTeam::Blue;
    uint8_t     type = 0;   // eMinionType cast (Melee/Ranged/Siege/Super)
    uint8_t     lane = 0;   // eMinionWay cast (Top/Mid/Bot)
};

static_assert(sizeof(MinionStateComponent) == 876u);
static_assert(offsetof(MinionStateComponent, attackTargetId) == 4u);
static_assert(offsetof(MinionStateComponent, deathTimer) == 48u);
// CommandQueue — 모든 자율 유닛의 Layer 1 (Intent) 출력.
//  - 플레이어 입력과 AI 판단이 동일한 큐에 커맨드 push.
//  - 클라 전용: 서버는 자체 큐 사용, 본 컴포넌트는 네트워크 직렬화 X
enum class eCommandType : uint8_t
{
    None = 0,
    Move,
    Stop, 
    Hold,
    Attack,
    AttackMove,
    Cast,
    End
};

struct Command
{
    eCommandType type = eCommandType::End;
    Vec3 vTargetPos = { 0.f, 0.f, 0.f };
    EntityID targetEntity = NULL_ENTITY;
    uint32_t iSkillSlot = 0;
    bool_t bShiftQueue = false;
};

struct CommandQueueComponent
{
    std::deque<Command> queue;
    Command current = {};
    bool_t bActive = false;
};
//공격 가능 여부 - 포탑 Outer -> Nexus 순으로 진행
struct TargetableTag { u8_t reserved = 0u; };

// Practice/smoke targets remain player-targetable but are ignored by lane AI.
struct PracticeDummyTag { u8_t reserved = 0u; };

static_assert(sizeof(TargetableTag) == 1u);
static_assert(sizeof(PracticeDummyTag) == 1u);

struct PracticeSpawnedTag
{
    EntityID ownerEntity = NULL_ENTITY;
};

struct PracticePlayerComponent
{
    u32_t optionFlags = 0u;
    u32_t revision = 0u;
};

struct PracticeSkillEffectOverrideEntry
{
    u8_t slot = 0u;
    u8_t paramId = 0u;
    f32_t value = 0.f;
};

struct PracticeSkillEffectOverrideComponent
{
    static constexpr u8_t kMaxEntries = 32u;
    PracticeSkillEffectOverrideEntry entries[kMaxEntries]{};
    u8_t count = 0u;
    u32_t revision = 0u;
};

// Practice-only champion stat overlay.
// Base fields apply to definition input; EffectiveAttackSpeed applies after runtime modifiers.
struct PracticeChampionStatOverrideEntry
{
    u8_t statId = 0u;   // eChampionStatOverrideId
    f32_t value = 0.f;
};

struct PracticeChampionStatOverrideComponent
{
    static constexpr u8_t kMaxEntries = 32u;
    PracticeChampionStatOverrideEntry entries[kMaxEntries]{};
    u8_t count = 0u;
    u32_t revision = 0u;
};

// Practice 전용 아이템 수치 오버레이 (보유 챔피언 단위).
// 전역 ItemRegistry는 불변으로 두고, 이 엔티티의 스탯 합산/구매 가격에만 적용한다.
struct PracticeItemStatOverrideEntry
{
    u16_t itemId = 0u;
    u8_t fieldId = 0u;  // eItemStatOverrideField
    f32_t value = 0.f;
};

struct PracticeItemStatOverrideComponent
{
    static constexpr u8_t kMaxEntries = 32u;
    PracticeItemStatOverrideEntry entries[kMaxEntries]{};
    u8_t count = 0u;
    u32_t revision = 0u;
};

//Phase T- 8 상태 이상 컴포넌트 - CStatusEffectSystem 이 매프레임 Tick 
struct StunComponent
{
    f32_t fRemaining = 0.f; //스턴 이후 남은 시간
    EntityID sourceEntity = NULL_ENTITY; //어떤 챔피언이 걸었는가? 
};

enum class eStatusEffectId : uint16_t
{
    None = 0,
    GenericStun = 1,
    GenericSlow = 2,
    GenericDisarm = 3,
    ViegoMist = 4,
    AsheVolleySlow = 5,
    AsheCrystalArrowStun = 6,
    KalistaFateCallUntargetable = 7,
    JaxCounterStrike = 8,
    ZedDeathMark = 9,
    GenericAirborne = 10,
    KalistaOathswornRitual = 11,
    ZhonyaStasis = 12,
};

enum class eStatusStackPolicy : uint8_t
{
    RefreshDuration,
    AddIndependent,
    KeepLongest,
    StackMagnitude,
};

inline constexpr u32_t kGameplayStateStunnedFlag = 1u << 0;
inline constexpr u32_t kGameplayStateSlowedFlag = 1u << 1;
inline constexpr u32_t kGameplayStateDisarmedFlag = 1u << 2;
inline constexpr u32_t kGameplayStateInvisibleFlag = 1u << 3;
inline constexpr u32_t kGameplayStateUntargetableFlag = 1u << 4;
inline constexpr u32_t kGameplayStateCannotMoveFlag = 1u << 5;
inline constexpr u32_t kGameplayStateCannotAttackFlag = 1u << 6;
inline constexpr u32_t kGameplayStateCannotCastFlag = 1u << 7;
inline constexpr u32_t kGameplayStateAirborneFlag = 1u << 8;
inline constexpr u32_t kGameplayStateDodgesBasicAttacksFlag = 1u << 9;
inline constexpr u32_t kGameplayStateInvulnerableFlag = 1u << 10;
inline constexpr u32_t kGameplayStateStasisVisualFlag = 1u << 11;

inline constexpr u8_t kMaxStatusEffectInstances = 16;

struct StatusEffectApplyDesc
{
    eStatusEffectId effectId = eStatusEffectId::None;
    eStatusStackPolicy stackPolicy = eStatusStackPolicy::RefreshDuration;
    EntityID sourceEntity = NULL_ENTITY;
    u16_t stackGroup = 0;
    u32_t stateFlags = 0u;
    f32_t fDurationSec = 0.f;
    f32_t fMoveSpeedMul = 1.f;
};

struct StatusEffectInstance
{
    eStatusEffectId effectId = eStatusEffectId::None;
    eStatusStackPolicy stackPolicy = eStatusStackPolicy::RefreshDuration;
    u8_t reservedSourceAlignment = 0u;
    EntityID sourceEntity = NULL_ENTITY;
    u16_t stackGroup = 0;
    u16_t reservedStateFlagsAlignment = 0u;
    u32_t stateFlags = 0u;
    f32_t fRemainingSec = 0.f;
    f32_t fMoveSpeedMul = 1.f;
};

struct StatusEffectComponent
{
    StatusEffectInstance active[kMaxStatusEffectInstances]{};
    u8_t count = 0;
    u8_t reservedTail[3]{};
};

static_assert(sizeof(StatusEffectInstance) == 24u);
static_assert(offsetof(StatusEffectInstance, sourceEntity) == 4u);
static_assert(offsetof(StatusEffectInstance, stateFlags) == 12u);
static_assert(sizeof(StatusEffectComponent) == 388u);
static_assert(offsetof(StatusEffectComponent, count) == 384u);

struct GameplayStateComponent
{
    u32_t stateFlags = 0u;
    f32_t fMoveSpeedMul = 1.f;
};

enum class eForcedMotionKind : uint8_t
{
    None = 0,
    AirborneArc,
    GatherAirborneArc,
};

struct ForcedMotionComponent
{
    eForcedMotionKind kind = eForcedMotionKind::None;
    EntityID sourceEntity = NULL_ENTITY;
    Vec3 start{};
    Vec3 end{};
    f32_t fElapsedSec = 0.f;
    f32_t fDurationSec = 0.f;
    f32_t fArcHeight = 0.f;
};

struct PositionDiscontinuityComponent
{
    u64_t uTick = 0u;
};

//둔화 상태 이상
struct SlowComponent
{
    f32_t fRemaining = 0.f;
    f32_t fMoveSpeedMul = 0.5f;
    EntityID sourceEntity = NULL_ENTITY;
};
struct DisarmComponent
{
    f32_t fRemaining = 0.f;
    EntityID sourceEntity = NULL_ENTITY;
};
