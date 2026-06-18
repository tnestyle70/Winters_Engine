Session - Jax ExecuteDive, server-authoritative F Flash, Champion AI Blackboard/HFSM debug rendering

1. 반영해야 하는 코드

이번 세션의 기준 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`이다. Flash와 Jax dive는 클라이언트가 직접 위치를 바꾸지 않고 서버 `GameCommand`로만 실행한다. `ChampionGameDataDB` S1 facade가 이미 있으므로 Flash range/cooldown과 AI 판단 수치의 read path는 새 하드코딩 호출부를 만들지 않고 DB facade를 통해 들어가게 한다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

`struct ChampionGameDataSkill` 아래에 아래 코드를 추가:

```cpp
struct ChampionGameDataSummonerSpell
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
```

기존 코드:

```cpp
struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    f32_t visualYawOffset = 0.f;
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
};
```

아래로 교체:

```cpp
struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    f32_t visualYawOffset = 0.f;
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
    ChampionGameDataSummonerSpell summonerSpells[2] = {};
};
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h

기존 코드:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion);
}
```

아래로 교체:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion);

    f32_t ResolveSummonerSpellRange(u16_t spellId);
    f32_t ResolveSummonerSpellCooldown(u16_t spellId);
}
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

기존 코드:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion)
    {
        return GetDefaultChampionVisualYawOffset(champion);
    }
}
```

아래로 교체:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion)
    {
        return GetDefaultChampionVisualYawOffset(champion);
    }

    f32_t ResolveSummonerSpellRange(u16_t spellId)
    {
        switch (spellId)
        {
        case 4u: return 4.25f;
        default: return 0.f;
        }
    }

    f32_t ResolveSummonerSpellCooldown(u16_t spellId)
    {
        switch (spellId)
        {
        case 4u: return 300.f;
        default: return 0.f;
        }
    }
}
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionScore.h

기존 코드:

```cpp
struct ChampionScoreComponent
{
	static constexpr u8_t kSummonerSpellSlotCount = 2;
	static constexpr u16_t kSummonerSpellFlash = 4;
	static constexpr u16_t kSummonerSpellIgnite = 14;

	u16_t iKills = 0;
	u16_t iDeaths = 0;
	u16_t iAssists = 0;
	u16_t iSummonerSpellIds[kSummonerSpellSlotCount] =
	{
		kSummonerSpellFlash,
		kSummonerSpellIgnite
	};
};
```

아래로 교체:

```cpp
struct ChampionScoreComponent
{
	static constexpr u8_t kSummonerSpellSlotCount = 2;
	static constexpr u16_t kSummonerSpellFlash = 4;
	static constexpr u16_t kSummonerSpellIgnite = 14;

	u16_t iKills = 0;
	u16_t iDeaths = 0;
	u16_t iAssists = 0;
	u16_t iSummonerSpellIds[kSummonerSpellSlotCount] =
	{
		kSummonerSpellFlash,
		kSummonerSpellIgnite
	};
};

struct SummonerSpellStateComponent
{
	static constexpr u8_t kSlotCount = ChampionScoreComponent::kSummonerSpellSlotCount;

	f32_t cooldownRemaining[kSlotCount] = {};
	f32_t cooldownDuration[kSlotCount] = {};
};
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedEventComponent.h

기존 코드:

```cpp
enum class eKillFeedObjectKind : u8_t
{
    None = 0,
    Champion,
    Turret,
    Inhibitor,
    Dragon,
    Baron,
};
```

아래에 추가:

```cpp
inline constexpr u32_t kGlobalEffectFlashBlink = 0xF1A50001u;
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:

```fbs
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9
```

아래로 교체:

```fbs
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9,
    Flash = 10
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 코드:

```cpp
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9,
};
```

아래로 교체:

```cpp
    Recall = 7,
    RecallCancel = 8,
    AIDebugControl = 9,
    Flash = 10,
};
```

기존 코드:

```cpp
    void HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
    void HandleAIDebugControl(CWorld&, const TickContext&, const GameCommand&);
```

아래로 교체:

```cpp
    void HandleRecall(CWorld&, const TickContext&, const GameCommand&);
    void HandleRecallCancel(CWorld&, const TickContext&, const GameCommand&);
    void HandleAIDebugControl(CWorld&, const TickContext&, const GameCommand&);
    void HandleFlash(CWorld&, const TickContext&, const GameCommand&);
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

기존 코드:

```cpp
	void SendRecall(CClientNetwork& net);
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
```

아래로 교체:

```cpp
	void SendRecall(CClientNetwork& net);
	void SendFlash(CClientNetwork& net, const Vec3& groundPos,
		const Vec3& direction);
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
```

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`GetCommandKindName`의 `case eCommandKind::AIDebugControl:` 블록 아래에 추가:

```cpp
        case eCommandKind::Flash:
            return "Flash";
```

기존 코드:

```cpp
void CCommandSerializer::SendRecall(CClientNetwork& net)
{
    GameCommandWire wire{};
    wire.kind = eCommandKind::Recall;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;

    //Smoke ???ｌ쓣寃?
    SendSingle(net, wire);
}
```

아래에 추가:

```cpp
void CCommandSerializer::SendFlash(CClientNetwork& net, const Vec3& groundPos,
    const Vec3& direction)
{
    if (!IsValidMoveGroundPos(groundPos))
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::Flash;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.groundPos = groundPos;
    wire.direction = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);

    SendSingle(net, wire);
}
```

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp

기존 코드:

```cpp
        if (in.IsKeyPressed('D'))
            scene.TriggerFlash();
```

아래로 교체:

```cpp
        if (in.IsKeyPressed('F'))
        {
            ClearNetworkAttackIntent();
            scene.TriggerFlash();
        }
```

1-11. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    bool_t   IsDbgShowMinionMovement() const { return m_bDbgShowMinionMovement; }
    void     SetDbgShowMinionMovement(bool_t b) { m_bDbgShowMinionMovement = b; }
```

아래에 추가:

```cpp
    bool_t   IsDbgShowChampionAIText() const { return m_bDbgShowChampionAIText; }
    void     SetDbgShowChampionAIText(bool_t b) { m_bDbgShowChampionAIText = b; }
    bool_t   IsDbgShowChampionAIRanges() const { return m_bDbgShowChampionAIRanges; }
    void     SetDbgShowChampionAIRanges(bool_t b) { m_bDbgShowChampionAIRanges = b; }
```

기존 코드:

```cpp
    bool_t m_bDbgShowChampions = true;
    bool_t m_bDbgShowMinionMovement = true;
```

아래로 교체:

```cpp
    bool_t m_bDbgShowChampions = true;
    bool_t m_bDbgShowMinionMovement = true;
    bool_t m_bDbgShowChampionAIText = true;
    bool_t m_bDbgShowChampionAIRanges = true;
```

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`CScene_InGame::TriggerFlash()` 전체를 아래로 교체:

```cpp
void CScene_InGame::TriggerFlash()
{
    if (!m_pPlayerTransform || !m_pCamera)
        return;

    const Vec3 cursor = ResolveMouseMapSurfacePos();
    const Vec3 origin = m_pPlayerTransform->GetPosition();
    const f32_t dx = cursor.x - origin.x;
    const f32_t dz = cursor.z - origin.z;
    const f32_t lenSq = dx * dx + dz * dz;
    if (lenSq < 0.001f)
        return;

    const f32_t len = std::sqrt(lenSq);
    const f32_t nx = dx / len;
    const f32_t nz = dz / len;
    const Vec3 direction{ nx, 0.f, nz };

    if (m_bNetworkAuthoritativeGameplay &&
        m_pCommandSerializer &&
        m_pNetworkView &&
        m_pNetworkView->IsConnected())
    {
        m_pCommandSerializer->SendFlash(*m_pNetworkView, cursor, direction);
        return;
    }

    if (m_fFlashCooldownLeft > 0.f)
        return;

    const f32_t useLen = (len > m_fFlashRange) ? m_fFlashRange : len;
    Vec3 dest{ origin.x + nx * useLen, origin.y, origin.z + nz * useLen };
    (void)TryProjectToMapSurface(dest, 0.05f);

    SetPlayerPosition(dest);
    m_fFlashCooldownLeft = m_fFlashCooldown;
}
```

1-13. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
    ChampionScoreComponent score{};
    m_world.AddComponent<ChampionScoreComponent>(entity, score);
```

아래에 추가:

```cpp
    SummonerSpellStateComponent summonerSpellState{};
    m_world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);
```

1-14. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp

기존 include 블록에서 아래 코드:

```cpp
#include "Shared/GameSim/Components/SkillStateComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionScore.h"
```

`void CSkillCooldownSystem::Execute(CWorld& world, const TickContext& tc)` 내부에서 `UpdateKalistaPassiveDash(world, tc);` 바로 아래에 추가:

```cpp
    const auto spellEntities =
        DeterministicEntityIterator<SummonerSpellStateComponent>::CollectSorted(world);
    for (EntityID entity : spellEntities)
    {
        auto& spells = world.GetComponent<SummonerSpellStateComponent>(entity);
        for (u8_t i = 0; i < SummonerSpellStateComponent::kSlotCount; ++i)
        {
            if (spells.cooldownRemaining[i] > 0.f)
            {
                spells.cooldownRemaining[i] -= tc.fDt;
                if (spells.cooldownRemaining[i] <= 0.f)
                {
                    spells.cooldownRemaining[i] = 0.f;
                    spells.cooldownDuration[i] = 0.f;
                }
            }
            else
            {
                spells.cooldownDuration[i] = 0.f;
            }
        }
    }
```

1-15. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 include 블록에서 아래 코드:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionScore.h"
```

`CDefaultCommandExecutor::ExecuteCommand` switch에서 기존 코드:

```cpp
    case eCommandKind::AIDebugControl:
        HandleAIDebugControl(world, tc, cmd);
        break;
    default:
        break;
```

아래로 교체:

```cpp
    case eCommandKind::AIDebugControl:
        HandleAIDebugControl(world, tc, cmd);
        break;
    case eCommandKind::Flash:
        HandleFlash(world, tc, cmd);
        break;
    default:
        break;
```

`HandleRecallCancel` 아래, `HandleAIDebugControl` 위에 아래 코드를 추가:

```cpp
void CDefaultCommandExecutor::HandleFlash(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (!world.HasComponent<TransformComponent>(cmd.issuerEntity))
        return;
    if (!GameplayStateQuery::CanMove(world, cmd.issuerEntity))
        return;

    auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
    const Vec3 origin = transform.GetLocalPosition();
    Vec3 rawTarget = cmd.groundPos;
    rawTarget.y = origin.y;

    const f32_t dx = rawTarget.x - origin.x;
    const f32_t dz = rawTarget.z - origin.z;
    const f32_t lenSq = dx * dx + dz * dz;
    if (lenSq <= 0.001f)
        return;

    u8_t flashSlot = 0u;
    bool_t bHasFlash = true;
    if (world.HasComponent<ChampionScoreComponent>(cmd.issuerEntity))
    {
        bHasFlash = false;
        const auto& score = world.GetComponent<ChampionScoreComponent>(cmd.issuerEntity);
        for (u8_t i = 0; i < ChampionScoreComponent::kSummonerSpellSlotCount; ++i)
        {
            if (score.iSummonerSpellIds[i] == ChampionScoreComponent::kSummonerSpellFlash)
            {
                flashSlot = i;
                bHasFlash = true;
                break;
            }
        }
    }
    if (!bHasFlash)
        return;

    auto& spells = world.HasComponent<SummonerSpellStateComponent>(cmd.issuerEntity)
        ? world.GetComponent<SummonerSpellStateComponent>(cmd.issuerEntity)
        : world.AddComponent<SummonerSpellStateComponent>(cmd.issuerEntity, SummonerSpellStateComponent{});
    if (spells.cooldownRemaining[flashSlot] > 0.f)
        return;

    const f32_t range =
        ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
    const f32_t cooldown =
        ChampionGameDataDB::ResolveSummonerSpellCooldown(ChampionScoreComponent::kSummonerSpellFlash);
    if (range <= 0.f || cooldown <= 0.f)
        return;

    const f32_t len = std::sqrt(lenSq);
    const f32_t useLen = std::min(len, range);
    const f32_t nx = dx / len;
    const f32_t nz = dz / len;
    Vec3 dest{ origin.x + nx * useLen, origin.y, origin.z + nz * useLen };

    if (tc.pWalkable)
    {
        Vec3 resolved{};
        if (!tc.pWalkable->TryResolveMoveTarget(origin, dest, resolved))
            return;
        resolved.y = origin.y;
        dest = resolved;
    }

    ClearMoveTarget(world, cmd.issuerEntity);
    ClearAttackChase(world, cmd.issuerEntity);
    ClearCombatAction(world, cmd.issuerEntity);
    CancelRecall(world, cmd.issuerEntity);

    transform.SetPosition(dest);
    spells.cooldownRemaining[flashSlot] = cooldown;
    spells.cooldownDuration[flashSlot] = cooldown;

    ReplicatedEventComponent flashEvent{};
    flashEvent.kind = eReplicatedEventKind::EffectTrigger;
    flashEvent.sourceEntity = cmd.issuerEntity;
    flashEvent.effectId = kGlobalEffectFlashBlink;
    flashEvent.position = origin;
    flashEvent.direction = Vec3{ dest.x - origin.x, 0.f, dest.z - origin.z };
    flashEvent.durationMs = 400u;
    flashEvent.startTick = tc.tickIndex;
    EnqueueReplicatedEvent(world, flashEvent);
}
```

1-16. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 include 블록에서 아래 코드:

```cpp
#include "GamePlay/VisualHookRegistry.h"
```

아래에 추가:

```cpp
#include "GameObject/Champion/Ezreal/Ezreal_FxPresets.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
```

`CEventApplier::ApplyEffectTrigger`에서 아래 기존 코드 바로 아래:

```cpp
    const u32_t effectId = ev->effectId();
```

아래에 추가:

```cpp
    if (effectId == kGlobalEffectFlashBlink)
    {
        const Vec3 origin{ ev->posX(), ev->posY(), ev->posZ() };
        const Vec3 delta{ ev->dirX(), ev->dirY(), ev->dirZ() };
        const Vec3 dest{ origin.x + delta.x, origin.y + delta.y, origin.z + delta.z };
        const f32_t lifetime = ev->durationMs() > 0
            ? static_cast<f32_t>(ev->durationMs()) / 1000.f
            : 0.4f;
        Ezreal::Fx::SpawnEFlash(world, origin, dest, lifetime);
        return;
    }
```

1-17. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`ChampionAIComponent.h`는 `eCommandKind`를 직접 포함하지 않는다. Last command kind는 Snapshot/debug 전용 byte로 저장한다.

기존 코드:

```cpp
enum class eChampionAIState : u8_t
{
	MoveToOuterTurret,
	WaitForWave,
	LaneCombat,
	Retreat,
	Recalling,
	Dead,
};
```

아래로 교체:

```cpp
enum class eChampionAIState : u8_t
{
	MoveToOuterTurret,
	WaitForWave,
	LaneCombat,
	Diving,
	Retreat,
	Recalling,
	Dead,
};
```

기존 코드:

```cpp
enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	Retreat,
	Recall,
};
```

아래로 교체:

```cpp
enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	UseFlashEscape,
	Retreat,
	Recall,
};
```

기존 코드:

```cpp
enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	AttackChampion,
	SiegeStructure,
	Retreat,
	Recall,
};
```

아래로 교체:

```cpp
enum class eChampionAIIntent : u8_t
{
	FarmMinion,
	AttackChampion,
	ExecuteDive,
	SiegeStructure,
	Retreat,
	Recall,
};
```

`enum class eChampionAIDebugControlMode` 아래에 추가:

```cpp
enum class eChampionAIDivePhase : u8_t
{
	None,
	EngageQ,
	ArmW,
	BasicAttack,
	ExtraBasicAttack,
	FlashExit,
	ExitMove,
};
```

기존 코드:

```cpp
	ChampionAITuningParam postComboBAWindow{ 0.80f, 0.80f, 0.f, 5.f, false };
};
```

아래로 교체:

```cpp
	ChampionAITuningParam postComboBAWindow{ 0.80f, 0.80f, 0.f, 5.f, false };
	ChampionAITuningParam lowHpExecuteThreshold{ 0.10f, 0.10f, 0.01f, 0.50f, false };
	ChampionAITuningParam diveScanRange{ 11.f, 11.f, 1.f, 40.f, false };
	ChampionAITuningParam diveExtraBAWindow{ 1.80f, 1.80f, 0.f, 5.f, false };
};
```

기존 코드:

```cpp
inline constexpr u32_t kChampionAIActionBitAttackStructure = 1u << 4;
inline constexpr u32_t kChampionAIActionBitRetreat = 1u << 5;
```

아래로 교체:

```cpp
inline constexpr u32_t kChampionAIActionBitAttackStructure = 1u << 4;
inline constexpr u32_t kChampionAIActionBitUseFlashEscape = 1u << 5;
inline constexpr u32_t kChampionAIActionBitRetreat = 1u << 6;
```

`ChampionAIComponent`에서 아래 기존 코드:

```cpp
	EntityID comboTarget = NULL_ENTITY;
	u8_t comboStep = 0;
```

아래에 추가:

```cpp
	EntityID lowHpEnemyChampion = NULL_ENTITY;
	EntityID diveTarget = NULL_ENTITY;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	u8_t diveExtraBACount = 0;
```

`ChampionAIComponent`에서 아래 기존 코드:

```cpp
	f32_t fPostComboBAWindow = 0.80f;
	f32_t fPostComboBATimer = 0.f;
```

아래에 추가:

```cpp
	f32_t fLowHpExecuteThreshold = 0.10f;
	f32_t fDiveScanRange = 11.f;
	f32_t fDiveExtraBAWindow = 1.80f;
	f32_t fDiveExtraBATimer = 0.f;
```

`ChampionAIComponent`에서 아래 기존 코드:

```cpp
	f32_t fDecisionTurretDanger = 0.f;
	u32_t nextCommandSequence = 1;
```

아래에 추가:

```cpp
	f32_t fDecisionLowHpEnemyRatio = 1.f;
	f32_t fDecisionLowHpEnemyDistance = 999.f;
	f32_t fDecisionChampionScanRange = 0.f;
	f32_t fDecisionDiveScanRange = 0.f;
	f32_t fDecisionFlashRange = 0.f;
	u8_t debugLastCommandKind = 0;
	u8_t debugLastCommandSlot = 0;
	EntityID debugLastCommandTarget = NULL_ENTITY;
	Vec3 debugLastCommandPos{};
	u32_t nextCommandSequence = 1;
```

`ChampionAIDebugComponent`에서 아래 기존 코드:

```cpp
	u32_t targetNetId = 0;
```

아래에 추가:

```cpp
	u32_t lowHpEnemyNetId = 0;
	u32_t diveTargetNetId = 0;
	u32_t lastCommandTargetNetId = 0;
	u8_t lastCommandKind = 0;
	u8_t lastCommandSlot = 0;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
```

`ChampionAIDebugComponent`에서 아래 기존 코드:

```cpp
	f32_t fTurretDanger = 0.f;
```

아래에 추가:

```cpp
	f32_t fLowHpEnemyRatio = 1.f;
	f32_t fLowHpEnemyDistance = 999.f;
	f32_t fChampionScanRange = 0.f;
	f32_t fDiveScanRange = 0.f;
	f32_t fFlashRange = 0.f;
	Vec3 lastCommandPos{ 0.f, 0.f, 0.f };
```

1-18. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp

`GetChampionAIComboPlan` 안의 기존 Jax combo:

```cpp
    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        4
    };
```

아래로 교체:

```cpp
    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        3
    };
```

1-19. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

기존 include 블록에서 아래 코드:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionScore.h"
```

`ChampionAIContext` 안에서 아래 기존 코드:

```cpp
        EntityID enemyChampion = NULL_ENTITY;
        EntityID enemyMinion = NULL_ENTITY;
```

아래에 추가:

```cpp
        EntityID lowHpEnemyChampion = NULL_ENTITY;
        EntityID diveTarget = NULL_ENTITY;
```

`ChampionAIContext` 안에서 아래 기존 코드:

```cpp
        f32_t enemyDistance = 999.f;
```

아래에 추가:

```cpp
        f32_t lowHpEnemyRatio = 1.f;
        f32_t lowHpEnemyDistance = 999.f;
```

`IsSkillReady` 아래에 추가:

```cpp
    bool_t TryFindFlashSlot(CWorld& world, EntityID self, u8_t& outSlot)
    {
        outSlot = 0u;
        if (!world.HasComponent<ChampionScoreComponent>(self))
            return true;

        const auto& score = world.GetComponent<ChampionScoreComponent>(self);
        for (u8_t i = 0; i < ChampionScoreComponent::kSummonerSpellSlotCount; ++i)
        {
            if (score.iSummonerSpellIds[i] == ChampionScoreComponent::kSummonerSpellFlash)
            {
                outSlot = i;
                return true;
            }
        }
        return false;
    }

    bool_t IsFlashReady(CWorld& world, EntityID self)
    {
        u8_t slot = 0u;
        if (!TryFindFlashSlot(world, self, slot))
            return false;
        if (!world.HasComponent<SummonerSpellStateComponent>(self))
            return true;
        return world.GetComponent<SummonerSpellStateComponent>(self).cooldownRemaining[slot] <= 0.f;
    }
```

`CommandName` switch에 `case eCommandKind::Flash:` 추가:

```cpp
        case eCommandKind::Flash:
            return "Flash";
```

`EmitSkillCommand` 아래에 추가:

```cpp
    bool_t EmitFlashCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        const Vec3& desiredGoal,
        const char* reason,
        std::vector<GameCommand>& outCommands)
    {
        if (!IsFlashReady(world, self))
            return false;

        const f32_t flashRange =
            ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
        if (flashRange <= 0.f)
            return false;

        const Vec3 dir = WintersMath::DirectionXZ(selfPos, desiredGoal);
        if (dir.x == 0.f && dir.z == 0.f)
            return false;

        ai.lastAction = eChampionAIAction::UseFlashEscape;
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::Flash);
        cmd.groundPos = desiredGoal;
        cmd.direction = dir;
        outCommands.push_back(cmd);

        LogChampionAICommand(reason, tc, self, ai, champion, selfPos, desiredGoal,
            NULL_ENTITY, cmd.kind, cmd.slot);
        return true;
    }
```

`FindEnemyChampion` 아래에 추가:

```cpp
    EntityID FindLowHpEnemyChampion(
        CWorld& world,
        EntityID self,
        eTeam myTeam,
        const Vec3& pos,
        f32_t range,
        f32_t hpThreshold,
        f32_t& outHpRatio,
        f32_t& outDistance)
    {
        EntityID best = NULL_ENTITY;
        f32_t bestScore = -1.f;
        f32_t bestDistSq = 999.f * 999.f;
        outHpRatio = 1.f;
        outDistance = 999.f;

        const f32_t rangeSq = range * range;
        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID e, ChampionComponent& champion, TransformComponent& transform)
            {
                if (world.HasComponent<PracticeDummyTag>(e))
                    return;
                if (champion.team == myTeam || !IsAliveTarget(world, e))
                    return;
                if (!GameplayStateQuery::CanBeTargetedBy(world, self, e))
                    return;

                const f32_t distSq = WintersMath::DistanceSqXZ(pos, transform.GetPosition());
                if (distSq > rangeSq)
                    return;

                const f32_t hpRatio = HealthRatio(world, e);
                if (hpRatio > hpThreshold)
                    return;

                const f32_t score = (hpThreshold - hpRatio) * 100.f -
                    std::sqrt(std::max(0.f, distSq));
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDistSq = distSq;
                    outHpRatio = hpRatio;
                    best = e;
                }
            });

        outDistance = (best != NULL_ENTITY) ? std::sqrt(std::max(0.f, bestDistSq)) : 999.f;
        return best;
    }
```

`ApplyChampionAIProfileAndTuning`에서 `postComboBAWindow` 적용 블록 아래에 추가:

```cpp
        ai.fLowHpExecuteThreshold = ResolveChampionAITuningParam(
            ai.tuning.lowHpExecuteThreshold, 0.10f, bOverrideProfile);
        ai.fDiveScanRange = ResolveChampionAITuningParam(
            ai.tuning.diveScanRange, 11.f, bOverrideProfile);
        ai.fDiveExtraBAWindow = ResolveChampionAITuningParam(
            ai.tuning.diveExtraBAWindow, 1.80f, bOverrideProfile);
```

`BuildChampionAIContext`에서 `ctx.enemyMinion = FindEnemyMinion(...)` 호출 위에 추가:

```cpp
        ctx.lowHpEnemyChampion = FindLowHpEnemyChampion(
            world,
            self,
            champion.team,
            selfPos,
            ai.fDiveScanRange,
            ai.fLowHpExecuteThreshold,
            ctx.lowHpEnemyRatio,
            ctx.lowHpEnemyDistance);
        ctx.diveTarget = ctx.lowHpEnemyChampion;
```

`UpdateChampionAIDecisionEvidence`에서 `ai.fDecisionTurretDanger = ctx.turretDanger;` 아래에 추가:

```cpp
        ai.lowHpEnemyChampion = ctx.lowHpEnemyChampion;
        ai.fDecisionLowHpEnemyRatio = ctx.lowHpEnemyRatio;
        ai.fDecisionLowHpEnemyDistance = ctx.lowHpEnemyDistance;
        ai.fDecisionChampionScanRange = ai.championScanRange;
        ai.fDecisionDiveScanRange = ai.fDiveScanRange;
        ai.fDecisionFlashRange =
            ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
```

`ActionBit` switch에 아래 case 추가:

```cpp
        case eChampionAIAction::UseFlashEscape:
            return kChampionAIActionBitUseFlashEscape;
```

`BuildChampionAIAvailableActionMask`에서 `CanAttackChampion(ai, ctx)` 블록 아래에 추가:

```cpp
        if (ctx.lowHpEnemyChampion != NULL_ENTITY &&
            IsFlashReady(world, self))
            mask |= kChampionAIActionBitUseFlashEscape;
```

위 변경을 위해 `BuildChampionAIAvailableActionMask` signature를 아래로 교체:

```cpp
    u32_t BuildChampionAIAvailableActionMask(
        CWorld& world,
        EntityID self,
        const ChampionAIComponent& ai,
        const ChampionAIContext& ctx)
```

그리고 호출부를 아래로 교체:

```cpp
            ai.debugAvailableActionMask =
                BuildChampionAIAvailableActionMask(world, self, ai, ctx);
```

`TryStartChampionAttack` 아래에 추가:

```cpp
    bool_t TryExecuteJaxDive(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (champion.id != eChampion::JAX)
            return false;

        EntityID target = ai.diveTarget;
        if (target == NULL_ENTITY || !IsAliveTarget(world, target))
        {
            target = ctx.lowHpEnemyChampion;
            if (target == NULL_ENTITY)
                return false;
            ai.diveTarget = target;
            ai.divePhase = eChampionAIDivePhase::EngageQ;
            ai.diveExtraBACount = 0u;
            ai.fDiveExtraBATimer = ai.fDiveExtraBAWindow;
        }

        Vec3 targetPos{};
        if (!TryGetPosition(world, target, targetPos))
            return false;

        ai.state = eChampionAIState::Diving;
        SetChampionAIIntent(ai, eChampionAIIntent::ExecuteDive, true);
        ai.lockedChampion = target;

        const f32_t targetHp = HealthRatio(world, target);
        const f32_t distance =
            std::sqrt(std::max(0.f, WintersMath::DistanceSqXZ(selfPos, targetPos)));
        const bool_t bTargetDeadOrFinished =
            !IsAliveTarget(world, target) ||
            targetHp <= 0.f ||
            ai.divePhase == eChampionAIDivePhase::FlashExit;

        if (bTargetDeadOrFinished)
            ai.divePhase = eChampionAIDivePhase::FlashExit;

        if (ai.divePhase == eChampionAIDivePhase::EngageQ)
        {
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, static_cast<u8_t>(eSkillSlot::Q),
                "jax-dive-q", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::ArmW;
                return true;
            }
            if (distance > ctx.attackRange + 0.25f)
                return EmitMoveCommand(world, tc, self, ai, champion.id, selfPos,
                    targetPos, eChampionAIAction::AttackChampion,
                    "jax-dive-chase", outCommands);
            ai.divePhase = eChampionAIDivePhase::ArmW;
        }

        if (ai.divePhase == eChampionAIDivePhase::ArmW)
        {
            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos,
                target, static_cast<u8_t>(eSkillSlot::W),
                "jax-dive-w", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::BasicAttack;
                return true;
            }
            ai.divePhase = eChampionAIDivePhase::BasicAttack;
        }

        if (ai.divePhase == eChampionAIDivePhase::BasicAttack)
        {
            if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "jax-dive-ba", outCommands))
            {
                ai.divePhase = eChampionAIDivePhase::ExtraBasicAttack;
                return true;
            }
        }

        if (ai.divePhase == eChampionAIDivePhase::ExtraBasicAttack)
        {
            ai.fDiveExtraBATimer = std::max(0.f, ai.fDiveExtraBATimer - tc.fDt);
            if (IsAliveTarget(world, target) &&
                targetHp <= ai.fLowHpExecuteThreshold &&
                ai.diveExtraBACount < 2u &&
                ai.fDiveExtraBATimer > 0.f)
            {
                if (EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                    target, eChampionAIAction::AttackChampion,
                    "jax-dive-extra-ba", outCommands))
                {
                    ++ai.diveExtraBACount;
                    return true;
                }
            }
            ai.divePhase = eChampionAIDivePhase::FlashExit;
        }

        if (ai.divePhase == eChampionAIDivePhase::FlashExit)
        {
            if (EmitFlashCommand(world, tc, self, ai, champion.id, selfPos,
                ai.safeAnchor, "jax-dive-flash-exit", outCommands))
            {
                ai.diveTarget = NULL_ENTITY;
                ai.divePhase = eChampionAIDivePhase::ExitMove;
                return true;
            }

            ai.divePhase = eChampionAIDivePhase::ExitMove;
        }

        if (ai.divePhase == eChampionAIDivePhase::ExitMove)
        {
            ai.diveTarget = NULL_ENTITY;
            ai.divePhase = eChampionAIDivePhase::None;
            return EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
        }

        return false;
    }
```

`ExecuteLaneCombat`에서 아래 기존 retreat gate:

```cpp
        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking))
        {
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }
```

아래로 교체:

```cpp
        if (TryExecuteJaxDive(world, tc, self, ai, champion, selfPos, ctx, outCommands))
            return;

        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking))
        {
            ai.diveTarget = NULL_ENTITY;
            ai.divePhase = eChampionAIDivePhase::None;
            EmitRetreat(world, tc, self, ai, champion, selfPos, outCommands);
            return;
        }
```

`Execute`에서 아래 기존 코드:

```cpp
            ai.fPostComboBATimer = std::max(0.f, ai.fPostComboBATimer - tc.fDt);
            if (ai.fPostComboBATimer <= 0.f)
                ai.bPostComboBAAllowed = false;
```

아래에 추가:

```cpp
            ai.fDiveExtraBATimer = std::max(0.f, ai.fDiveExtraBATimer - tc.fDt);
```

1-20. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`EntitySnapshot` 안에서 기존 코드:

```fbs
    kills:ushort;
    deaths:ushort;
    assists:ushort;
    summonerSpellIds:[ushort];
```

아래로 교체:

```fbs
    kills:ushort;
    deaths:ushort;
    assists:ushort;
    summonerSpellIds:[ushort];
    summonerSpellCooldowns:[float];
    summonerSpellCooldownDurations:[float];

    aiDebugAvailableActionMask:uint;
    aiDebugAvailableSkillMask:uint;
    aiDebugTargetNet:uint;
    aiDebugLowHpEnemyNet:uint;
    aiDebugDiveTargetNet:uint;
    aiDebugLastCommandTargetNet:uint;
    aiDebugLastCommandKind:ubyte;
    aiDebugLastCommandSlot:ubyte;
    aiDebugDivePhase:ubyte;
    aiDebugChampionScore:float;
    aiDebugFarmScore:float;
    aiDebugStructureScore:float;
    aiDebugSelfHpRatio:float;
    aiDebugEnemyHpRatio:float;
    aiDebugEnemyDistance:float;
    aiDebugAttackRange:float;
    aiDebugTurretDanger:float;
    aiDebugLowHpEnemyRatio:float;
    aiDebugLowHpEnemyDistance:float;
    aiDebugChampionScanRange:float;
    aiDebugDiveScanRange:float;
    aiDebugFlashRange:float;
    aiDebugLastCommandX:float;
    aiDebugLastCommandY:float;
    aiDebugLastCommandZ:float;
```

`Shared/Schemas/run_codegen.bat` 실행 후 `Shared/Schemas/Generated/cpp/Command_generated.h`와 `Shared/Schemas/Generated/cpp/Snapshot_generated.h`의 새 signature를 기준으로 호출부를 맞춘다.

1-21. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`summonerSpellIds` 선언 아래에 추가:

```cpp
        std::vector<f32_t> summonerSpellCooldowns;
        std::vector<f32_t> summonerSpellCooldownDurations;
```

`ChampionScoreComponent` snapshot 수집 블록 아래에 추가:

```cpp
        if (world.HasComponent<SummonerSpellStateComponent>(entity))
        {
            const auto& spells = world.GetComponent<SummonerSpellStateComponent>(entity);
            summonerSpellCooldowns.reserve(SummonerSpellStateComponent::kSlotCount);
            summonerSpellCooldownDurations.reserve(SummonerSpellStateComponent::kSlotCount);
            for (u8_t i = 0; i < SummonerSpellStateComponent::kSlotCount; ++i)
            {
                summonerSpellCooldowns.push_back(spells.cooldownRemaining[i]);
                summonerSpellCooldownDurations.push_back(spells.cooldownDuration[i]);
            }
        }
```

`summonerSpellOffset` 생성 아래에 추가:

```cpp
        const auto summonerSpellCooldownOffset = fbb.CreateVector(summonerSpellCooldowns);
        const auto summonerSpellCooldownDurationOffset =
            fbb.CreateVector(summonerSpellCooldownDurations);
```

`ChampionAIComponent` snapshot 수집 블록 안에서 local 변수들을 준비하고 `CreateEntitySnapshot` 뒤쪽에 넘긴다. FlatBuffers codegen 이후 signature가 바뀌므로 정확한 인자 위치는 생성 파일을 확인한다.

CONFIRM_NEEDED: `Shared/Schemas/run_codegen.bat` 실행 뒤 generated `CreateEntitySnapshot` signature 확인.

1-22. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`summonerSpellIds` 적용 블록 아래에 추가:

```cpp
            if (const auto* pSpellCooldowns = es->summonerSpellCooldowns())
            {
                SummonerSpellStateComponent spells{};
                const u32_t count = std::min<u32_t>(
                    pSpellCooldowns->size(),
                    SummonerSpellStateComponent::kSlotCount);
                for (u32_t i = 0; i < count; ++i)
                    spells.cooldownRemaining[i] = pSpellCooldowns->Get(i);

                if (const auto* pDurations = es->summonerSpellCooldownDurations())
                {
                    const u32_t durationCount = std::min<u32_t>(
                        pDurations->size(),
                        SummonerSpellStateComponent::kSlotCount);
                    for (u32_t i = 0; i < durationCount; ++i)
                        spells.cooldownDuration[i] = pDurations->Get(i);
                }

                if (world.HasComponent<SummonerSpellStateComponent>(e))
                    world.GetComponent<SummonerSpellStateComponent>(e) = spells;
                else
                    world.AddComponent<SummonerSpellStateComponent>(e, spells);
            }
```

기존 `ChampionAIDebugComponent` 적용 블록에서 `debug.availableSkillMask = ...` 아래에 추가:

```cpp
                debug.availableActionMask = es->aiDebugAvailableActionMask();
                debug.availableSkillMask = es->aiDebugAvailableSkillMask();
                debug.targetNetId = es->aiDebugTargetNet();
                debug.lowHpEnemyNetId = es->aiDebugLowHpEnemyNet();
                debug.diveTargetNetId = es->aiDebugDiveTargetNet();
                debug.lastCommandTargetNetId = es->aiDebugLastCommandTargetNet();
                debug.lastCommandKind = es->aiDebugLastCommandKind();
                debug.lastCommandSlot = es->aiDebugLastCommandSlot();
                debug.divePhase =
                    static_cast<eChampionAIDivePhase>(es->aiDebugDivePhase());
                debug.fChampionDecisionScore = es->aiDebugChampionScore();
                debug.fFarmDecisionScore = es->aiDebugFarmScore();
                debug.fStructureDecisionScore = es->aiDebugStructureScore();
                debug.fSelfHpRatio = es->aiDebugSelfHpRatio();
                debug.fEnemyHpRatio = es->aiDebugEnemyHpRatio();
                debug.fEnemyDistance = es->aiDebugEnemyDistance();
                debug.fAttackRange = es->aiDebugAttackRange();
                debug.fTurretDanger = es->aiDebugTurretDanger();
                debug.fLowHpEnemyRatio = es->aiDebugLowHpEnemyRatio();
                debug.fLowHpEnemyDistance = es->aiDebugLowHpEnemyDistance();
                debug.fChampionScanRange = es->aiDebugChampionScanRange();
                debug.fDiveScanRange = es->aiDebugDiveScanRange();
                debug.fFlashRange = es->aiDebugFlashRange();
                debug.lastCommandPos = Vec3{
                    es->aiDebugLastCommandX(),
                    es->aiDebugLastCommandY(),
                    es->aiDebugLastCommandZ()
                };
```

1-23. C:/Users/tnest/Desktop/Winters/Client/Public/UI/DebugDrawSystem.h

기존 코드:

```cpp
        static void DrawMinionMovement(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

아래에 추가:

```cpp
        static void DrawChampionAIDebug(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

1-24. C:/Users/tnest/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

기존 include 블록에서 아래 코드:

```cpp
#include "ECS/Components/GameplayComponents.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

`CDebugDrawSystem::Render`에서 아래 기존 코드:

```cpp
        if (pScene->IsDbgShowMinionMovement()) DrawMinionMovement(world, pScene, pDraw, mVP);
```

아래에 추가:

```cpp
        if (pScene->IsDbgShowChampionAIText() || pScene->IsDbgShowChampionAIRanges())
            DrawChampionAIDebug(world, pScene, pDraw, mVP);
```

`DrawMinionMovement` 함수 아래에 추가:

```cpp
    void CDebugDrawSystem::DrawChampionAIDebug(CWorld& w, CScene_InGame* s,
        ImDrawList* pDraw, const DirectX::XMMATRIX& mVP)
    {
        w.ForEach<ChampionComponent, ChampionAIDebugComponent, TransformComponent>(
            [&](EntityID entity, ChampionComponent& champion,
                ChampionAIDebugComponent& debug, TransformComponent& tf)
            {
                if (!debug.bPresent)
                    return;

                const Vec3 pos = tf.GetPosition();
                if (s->IsDbgShowChampionAIRanges())
                {
                    DrawWireCylinder(pDraw, mVP, pos, debug.fChampionScanRange, 0.05f, 0x55FFFF00u, 48);
                    DrawWireCylinder(pDraw, mVP, pos, debug.fDiveScanRange, 0.08f, 0x660000FFu, 48);
                    DrawWireCylinder(pDraw, mVP, pos, debug.fAttackRange, 0.11f, 0x88FFFFFFu, 32);
                    DrawWireCylinder(pDraw, mVP, pos, debug.fFlashRange, 0.14f, 0x88FFFF00u, 32);
                }

                if (!s->IsDbgShowChampionAIText())
                    return;

                ImVec2 labelPos{};
                if (!WorldToScreen(mVP, Vec3{ pos.x + 1.0f, pos.y + 2.2f, pos.z }, labelPos))
                    return;

                char label[256]{};
                sprintf_s(label,
                    "ai e%u c%u state=%u intent=%u action=%u phase=%u\n"
                    "target=%u low=%u hp=%.0f%% dist=%.1f turret=%.2f\n"
                    "cmd=%u slot=%u tgt=%u",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(champion.id),
                    static_cast<u32_t>(debug.state),
                    static_cast<u32_t>(debug.intent),
                    static_cast<u32_t>(debug.action),
                    static_cast<u32_t>(debug.divePhase),
                    debug.targetNetId,
                    debug.lowHpEnemyNetId,
                    debug.fLowHpEnemyRatio * 100.f,
                    debug.fLowHpEnemyDistance,
                    debug.fTurretDanger,
                    static_cast<u32_t>(debug.lastCommandKind),
                    static_cast<u32_t>(debug.lastCommandSlot),
                    debug.lastCommandTargetNetId);
                pDraw->AddText(labelPos, 0xFFFFFFFFu, label);
            });
    }
```

1-25. C:/Users/tnest/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 코드:

```cpp
            bool bm = pScene->IsDbgShowMinionMovement();
            if (ImGui::Checkbox("Minion cells / move vectors", &bm)) pScene->SetDbgShowMinionMovement(bm);
```

아래에 추가:

```cpp
            bool baiText = pScene->IsDbgShowChampionAIText();
            if (ImGui::Checkbox("Champion AI text", &baiText)) pScene->SetDbgShowChampionAIText(baiText);

            bool baiRanges = pScene->IsDbgShowChampionAIRanges();
            if (ImGui::Checkbox("Champion AI ranges", &baiRanges)) pScene->SetDbgShowChampionAIRanges(baiRanges);
```

`All On` 버튼 블록에서 아래 기존 코드:

```cpp
            pScene->SetDbgShowMinionMovement(true);
```

아래에 추가:

```cpp
            pScene->SetDbgShowChampionAIText(true);
            pScene->SetDbgShowChampionAIRanges(true);
```

`Sylas Only` 버튼 블록에서 아래 기존 코드:

```cpp
            pScene->SetDbgShowMinionMovement(false);
```

아래에 추가:

```cpp
            pScene->SetDbgShowChampionAIText(false);
            pScene->SetDbgShowChampionAIRanges(false);
```

1-26. C:/Users/tnest/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

`ChampionAIStateName` switch에 추가:

```cpp
		case eChampionAIState::Diving: return "Diving";
```

`ChampionAIActionName` switch에 추가:

```cpp
		case eChampionAIAction::UseFlashEscape: return "UseFlashEscape";
```

`ChampionAIIntentName` switch에 추가:

```cpp
		case eChampionAIIntent::ExecuteDive: return "ExecuteDive";
```

`SkillSlotName` 아래에 추가:

```cpp
	const char* ChampionAIDivePhaseName(eChampionAIDivePhase phase)
	{
		switch (phase)
		{
		case eChampionAIDivePhase::None: return "None";
		case eChampionAIDivePhase::EngageQ: return "EngageQ";
		case eChampionAIDivePhase::ArmW: return "ArmW";
		case eChampionAIDivePhase::BasicAttack: return "BasicAttack";
		case eChampionAIDivePhase::ExtraBasicAttack: return "ExtraBasicAttack";
		case eChampionAIDivePhase::FlashExit: return "FlashExit";
		case eChampionAIDivePhase::ExitMove: return "ExitMove";
		default: return "Unknown";
		}
	}
```

Selected Champion AI 표시에서 기존 코드:

```cpp
		ImGui::Text("State: %s   Intent: %s   Action: %s   TargetNet: %u",
			ChampionAIStateName(debug.state),
			ChampionAIIntentName(debug.intent),
			ChampionAIActionName(debug.action),
			debug.targetNetId);
```

아래에 추가:

```cpp
		ImGui::Text("Dive: phase=%s target=%u lowHpTarget=%u lowHp=%.0f%% lowDist=%.1f",
			ChampionAIDivePhaseName(debug.divePhase),
			debug.diveTargetNetId,
			debug.lowHpEnemyNetId,
			debug.fLowHpEnemyRatio * 100.f,
			debug.fLowHpEnemyDistance);
		ImGui::Text("Ranges: champion=%.1f dive=%.1f attack=%.1f flash=%.1f turret=%.2f",
			debug.fChampionScanRange,
			debug.fDiveScanRange,
			debug.fAttackRange,
			debug.fFlashRange,
			debug.fTurretDanger);
		ImGui::Text("LastCmd: kind=%u slot=%u target=%u pos=(%.1f %.1f %.1f)",
			static_cast<u32_t>(debug.lastCommandKind),
			static_cast<u32_t>(debug.lastCommandSlot),
			debug.lastCommandTargetNetId,
			debug.lastCommandPos.x,
			debug.lastCommandPos.y,
			debug.lastCommandPos.z);
```

Actions 영역에서 `Retreat` 버튼 앞에 추가:

```cpp
		RenderActionButton(pScene, debug.netId, "FlashExit", eChampionAIAction::UseFlashEscape,
			HasChampionAIAction(debug, eChampionAIAction::UseFlashEscape));
		ImGui::SameLine();
```

2. 검증

아래 순서대로 검증한다.

```powershell
cd C:\Users\tnest\Desktop\Winters
.\Shared\Schemas\run_codegen.bat
rg -n "Flash = 10|summonerSpellCooldowns|aiDebugLowHpEnemyNet" Shared/Schemas Shared/Schemas/Generated/cpp
rg -n "HandleFlash|SendFlash|kGlobalEffectFlashBlink|ResolveSummonerSpellRange" Shared Client Server
rg -n "ExecuteDive|UseFlashEscape|TryExecuteJaxDive|DrawChampionAIDebug" Shared Client
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

런타임 검증은 아래 순서로 한다.

```text
1. 사람 플레이어 아무 챔피언으로 접속한다.
2. F를 누르고 마우스 위치 방향으로 점멸한다.
3. 서버 snapshot으로 최종 위치가 갱신되고, 클라이언트 로컬 직접 이동 없이 위치가 따라오는지 본다.
4. Flash 직후 Ezreal.E.BlinkFlash cue가 origin -> dest 구간으로 1회만 재생되는지 본다.
5. Flash cooldown 중 F 재입력이 서버에서 거절되는지 본다.
6. 적 Jax bot과 10% 이하 HP 챔피언을 포탑 안에 둔다.
7. Jax가 lowHpEnemy를 감지하고 intent=ExecuteDive, state=Diving으로 바뀌는지 본다.
8. Jax가 Q -> W -> BA -> extra BA 후 FlashExit로 safeAnchor 쪽에 빠져나오는지 본다.
9. Champion AI text가 Jax 오른쪽에 표시되고 champion/dive/attack/flash range 원이 보이는지 본다.
10. AIDebugPanel에서 lowHp target, dive target, phase, last command, ranges, turret danger가 snapshot 기반으로 보이는지 본다.
```

수락 기준은 아래다.

```text
Client TriggerFlash network path가 SetPlayerPosition을 직접 호출하지 않는다.
Server CommandExecutor만 Flash 최종 위치와 cooldown을 결정한다.
AI Flash도 사람 Flash와 같은 eCommandKind::Flash 경로를 사용한다.
Jax dive는 기존 일반 CanAttackChampion turret reject를 우회하되 low HP target + Flash ready 조건에서만 시작한다.
Jax dive 종료는 FlashExit 또는 fallback RetreatMove로 끝난다.
Champion AI debug text/ranges는 Snapshot으로 받은 ChampionAIDebugComponent만 읽는다.
ChampionGameDataDB facade가 Flash range/cooldown의 단일 read path가 된다.
```
