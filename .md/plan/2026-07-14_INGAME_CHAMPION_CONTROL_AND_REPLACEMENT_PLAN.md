Session - 숫자 8 Attack Speed Lab에서 현재 5:5 챔피언은 서버 권위 조작권만 이전하고 미출전 챔피언은 현재 human 슬롯을 레벨 6·골드 10000의 fresh entity로 교체하며, 구현 예산 70%는 안전한 전환 경로에·30%는 17챔피언 순환 결과표와 0.8/2.5 비교 캡처에 고정한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`enum class ePracticeOperation : uint16_t`의 아래 기존 코드를:

```cpp
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
    Count = 21,
```

아래로 교체:

```cpp
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
    TakeControlRosterChampion = 21,
    ReplaceControlledChampion = 22,
    Count = 23,
```

아래 기존 코드 바로 아래에 추가:

```cpp
// ApplyItemStatOverride: flags = (itemId << 8) | eItemStatOverrideField, value = 대체값.
```

```cpp
// TakeControlRosterChampion: targetNet = 현재 10개 roster 중 조작할 bot NetId.
// ReplaceControlledChampion: flags = 새 championId. 현재 human slot은 유지하고 fresh entity로 교체.
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

`enum PracticeOperation : ushort`의 아래 기존 코드를:

```fbs
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20
```

아래로 교체:

```fbs
    ApplyChampionStatOverride = 17,
    ClearChampionStatOverrides = 18,
    ApplyItemStatOverride = 19,
    ClearItemStatOverrides = 20,
    TakeControlRosterChampion = 21,
    ReplaceControlledChampion = 22
```

### 1-3. C:/Users/user/Desktop/Winters/Server/Public/Game/LobbyAuthority.h

`class CLobbyAuthority final`의 public 영역에서 아래 기존 코드 바로 아래에 추가:

```cpp
    void SetSlotNetId(u8_t slotId, NetEntityId netId);
```

```cpp
    LobbyAuthorityResult TransferInGameHumanControl(
        u32_t sessionId,
        NetEntityId targetNetId);
    LobbyAuthorityResult ReplaceInGameControlledChampion(
        u32_t sessionId,
        eChampion champion,
        NetEntityId newNetId);
```

### 1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/LobbyAuthority.cpp

아래 기존 코드 바로 위에 추가:

```cpp
bool CLobbyAuthority::TryGetSessionSlot(u32_t sessionId, u8_t& outSlotId) const
```

```cpp
LobbyAuthorityResult CLobbyAuthority::TransferInGameHumanControl(
    u32_t sessionId,
    NetEntityId targetNetId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;
    if (m_phase != eRoomPhase::InGame || targetNetId == NULL_NET_ENTITY)
        return result;

    const auto sourceIt = m_sessionToSlot.find(sessionId);
    if (sourceIt == m_sessionToSlot.end())
        return result;

    LobbySlotState& source = m_slots[sourceIt->second];
    LobbySlotState* pTarget = nullptr;
    for (LobbySlotState& slot : m_slots)
    {
        if (slot.netId == targetNetId)
        {
            pTarget = &slot;
            break;
        }
    }

    if (!pTarget ||
        pTarget == &source ||
        !source.bHuman ||
        source.sessionId != sessionId ||
        !pTarget->bBot ||
        pTarget->bHuman ||
        pTarget->bDummy)
    {
        return result;
    }

    source.bHuman = false;
    source.bBot = true;
    source.sessionId = 0u;
    source.bReady = true;
    if (source.botDifficulty == 0u)
        source.botDifficulty = 2u;
    if (!IsValidLobbyBotLane(source.botLane))
        source.botLane = GetDefaultLobbyBotLane(source.slotId);

    pTarget->bHuman = true;
    pTarget->bBot = false;
    pTarget->sessionId = sessionId;
    pTarget->bReady = true;
    m_sessionToSlot[sessionId] = pTarget->slotId;

    result.bSendHello = true;
    result.helloNetId = pTarget->netId;
    result.helloChampion = pTarget->champion;
    result.helloTeam = pTarget->team;
    SetMessage("practice control transferred to roster champion");
    IncrementRevision(result);
    return result;
}

LobbyAuthorityResult CLobbyAuthority::ReplaceInGameControlledChampion(
    u32_t sessionId,
    eChampion champion,
    NetEntityId newNetId)
{
    LobbyAuthorityResult result{};
    result.sessionId = sessionId;
    if (m_phase != eRoomPhase::InGame ||
        newNetId == NULL_NET_ENTITY ||
        champion == eChampion::NONE ||
        champion == eChampion::END)
    {
        return result;
    }

    const auto sourceIt = m_sessionToSlot.find(sessionId);
    if (sourceIt == m_sessionToSlot.end())
        return result;

    LobbySlotState& slot = m_slots[sourceIt->second];
    if (!slot.bHuman || slot.bBot || slot.sessionId != sessionId)
        return result;

    slot.champion = champion;
    slot.netId = newNetId;
    slot.bReady = true;

    result.bSendHello = true;
    result.helloNetId = slot.netId;
    result.helloChampion = slot.champion;
    result.helloTeam = slot.team;
    SetMessage("practice controlled champion replaced");
    IncrementRevision(result);
    return result;
}
```

`CLobbyAuthority::TryStartGame`의 아래 기존 코드를:

```cpp
    bool_t bHasHuman = false;
    for (const LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman)
        {
            bHasHuman = true;
            break;
        }
    }
    if (!bHasHuman)
```

아래로 교체:

```cpp
    u32_t humanCount = 0u;
    for (const LobbySlotState& slot : m_slots)
    {
        if (slot.bHuman)
            ++humanCount;
    }
    if (humanCount == 0u)
```

같은 함수의 아래 기존 코드를:

```cpp
    if (ShouldUseRedSylasSmokeRoster())
        EnsureRedSylasSmokeRoster(m_slots, kGameRosterSlotCount);
```

아래로 교체:

```cpp
    if (ShouldUseAttackSpeedLabRoster())
    {
        if (humanCount != 1u)
        {
            SetMessage(FormatLobbyCommandLog(
                "reject",
                sessionId,
                Shared::Schema::LobbyCommandKind::StartGame,
                kInvalidGameRosterSlot,
                eChampion::END,
                "attack speed lab requires exactly one human"));
            return false;
        }
        EnsureAttackSpeedLabRoster(m_slots, kGameRosterSlotCount);
    }
    else if (ShouldUseRedSylasSmokeRoster())
    {
        EnsureRedSylasSmokeRoster(m_slots, kGameRosterSlotCount);
    }
```

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSmokeRoster.h

아래 기존 코드 바로 아래에 추가:

```cpp
bool_t ShouldUseRedSylasSmokeRoster();
```

```cpp
bool_t ShouldUseAttackSpeedLabRoster();
void EnsureAttackSpeedLabRoster(LobbySlotState* pSlots, u32_t slotCount);
```

### 1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSmokeRoster.cpp

아래 기존 include 바로 아래에 추가:

```cpp
#include <Windows.h>
```

```cpp
#include <array>
```

anonymous namespace의 `kSmokeRedSylasMaxHp` 바로 아래에 추가:

```cpp
    constexpr std::array<eChampion, kGameRosterSlotCount>
        kAttackSpeedLabRoster =
    {
        eChampion::IRELIA,
        eChampion::YASUO,
        eChampion::ASHE,
        eChampion::ANNIE,
        eChampion::LEESIN,
        eChampion::RIVEN,
        eChampion::SYLAS,
        eChampion::VIEGO,
        eChampion::YONE,
        eChampion::JAX,
    };

    u8_t ResolveAttackSpeedLabBotLane(u8_t slotId)
    {
        switch (slotId % 5u)
        {
        case 1u:
            return kGameSimLaneTop;
        case 2u:
            return kGameSimLaneMid;
        case 3u:
        case 4u:
            return kGameSimLaneBot;
        default:
            return kGameSimLaneMid;
        }
    }
```

`ShouldUseRedSylasSmokeRoster` 바로 위에 추가:

```cpp
bool_t ShouldUseAttackSpeedLabRoster()
{
#if defined(_DEBUG)
    return HasServerFlag(L"--attack-speed-lab");
#else
    return false;
#endif
}

void EnsureAttackSpeedLabRoster(
    LobbySlotState* pSlots,
    u32_t slotCount)
{
    if (!pSlots || slotCount != kGameRosterSlotCount)
        return;

    for (u32_t i = 0u; i < slotCount; ++i)
    {
        LobbySlotState& slot = pSlots[i];
        if (slot.bHuman)
        {
            slot.bBot = false;
            slot.bDummy = false;
            continue;
        }

        slot = LobbySlotState{};
        slot.slotId = static_cast<u8_t>(i);
        slot.team = i < 5u ? 0u : 1u;
        slot.bBot = true;
        slot.bReady = true;
        slot.champion = kAttackSpeedLabRoster[i];
        slot.botDifficulty = 2u;
        slot.botLane = ResolveAttackSpeedLabBotLane(slot.slotId);
    }
}

```

### 1-7. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

`CGameRoom` private 메서드 영역에서 아래 기존 코드 바로 아래에 추가:

```cpp
    void ClearPracticeSpawns();
```

```cpp
    void CommitPendingPracticeControlChange(const TickContext& tc);
```

아래 기존 코드 바로 아래에 추가:

```cpp
    EntityID SpawnChampionForLobbySlot(LobbySlotState& slot);
```

```cpp
    void ConfigureChampionControlRole(
        EntityID entity,
        const LobbySlotState& slot);
```

아래 기존 코드 바로 아래에 추가:

```cpp
    std::vector<EntityID> m_PracticeSpawnedEntities;
```

```cpp
    enum class PracticeControlChangeKind : u8_t
    {
        None = 0u,
        TakeRosterChampion,
        ReplaceControlledChampion,
    };

    struct PendingPracticeControlChange
    {
        PracticeControlChangeKind Kind = PracticeControlChangeKind::None;
        u32_t SessionId = 0u;
        NetEntityId SourceNetId = NULL_NET_ENTITY;
        NetEntityId TargetNetId = NULL_NET_ENTITY;
        eChampion Champion = eChampion::END;
    };

    PendingPracticeControlChange m_PendingPracticeControlChange{};
```

### 1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

아래 기존 코드 바로 위에 추가:

```cpp
EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)
```

```cpp
void CGameRoom::ConfigureChampionControlRole(
    EntityID entity,
    const LobbySlotState& slot)
{
    if (!m_world.IsAlive(entity))
        return;

    if (!slot.bBot || slot.bDummy)
    {
        if (m_world.HasComponent<ChampionAIComponent>(entity))
            m_world.RemoveComponent<ChampionAIComponent>(entity);
        if (m_world.HasComponent<ChampionAIResearchDebugComponent>(entity))
            m_world.RemoveComponent<ChampionAIResearchDebugComponent>(entity);
        return;
    }

    const Vec3 spawnPos = m_world.HasComponent<TransformComponent>(entity)
        ? m_world.GetComponent<TransformComponent>(entity).GetPosition()
        : GetSpawnPositionForLobbySlot(slot);
    ChampionAIComponent ai{};
    const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
    ai.champion = slot.champion;
    ai.team = static_cast<eTeam>(slot.team);
    ai.difficulty = slot.botDifficulty;
    ai.lane = CServerAICommandProducer::ResolveInitialBotLane(
        slot,
        GetGameSimRosterLane(slot.slotId));
    ai.activeLane = ai.lane;
    ai.brainType = ai.difficulty >= 2u
        ? eChampionAIBrainType::PlayerLike
        : eChampionAIBrainType::RuleBased;
    ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
    ai.retreatGoal = spawnPos;
    ai.championScanRange = profile.championScanRange;
    ai.minionScanRange = profile.minionScanRange;
    ai.structureScanRange = profile.structureScanRange;
    ai.leashRange = profile.leashRange;
    ai.retreatHpRatio = profile.retreatHpRatio;
    ai.reengageHpRatio = profile.reengageHpRatio;

    const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
    ai.laneGoal = waypointCount > 0u
        ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
        : GetGameSimLaneGatherPosition(ai.lane, slot.team);
    ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
    ai.midDefenseAnchor = ResolveChampionAISafeAnchor(
        ai.team,
        kChampionAIMidLane);
    ai.retreatGoal = ai.safeAnchor;

    if (m_world.HasComponent<ChampionAIComponent>(entity))
        m_world.GetComponent<ChampionAIComponent>(entity) = ai;
    else
        m_world.AddComponent<ChampionAIComponent>(entity, ai);
}
```

`CGameRoom::SpawnChampionForLobbySlot` 안의 `if (slot.bBot && !slot.bDummy)`로 시작하는 `ChampionAIComponent ai{}` 구성 블록 전체를:

```cpp
    if (slot.bBot && !slot.bDummy)
    {
        ChampionAIComponent ai{};
        const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
        ai.champion = slot.champion;
        ai.team = static_cast<eTeam>(slot.team);
        ai.difficulty = slot.botDifficulty;
        ai.lane = CServerAICommandProducer::ResolveInitialBotLane(
            slot,
            GetGameSimRosterLane(slot.slotId));
        ai.activeLane = ai.lane;
        ai.brainType = ai.difficulty >= 2u
            ? eChampionAIBrainType::PlayerLike
            : eChampionAIBrainType::RuleBased;
        ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
        ai.retreatGoal = spawnPos;
        ai.championScanRange = profile.championScanRange;
        ai.minionScanRange = profile.minionScanRange;
        ai.structureScanRange = profile.structureScanRange;
        ai.leashRange = profile.leashRange;
        ai.retreatHpRatio = profile.retreatHpRatio;
        ai.reengageHpRatio = profile.reengageHpRatio;

        const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
        ai.laneGoal = waypointCount > 0u
            ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
            : GetGameSimLaneGatherPosition(ai.lane, slot.team);
        ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
        ai.midDefenseAnchor = ResolveChampionAISafeAnchor(
            ai.team,
            kChampionAIMidLane);
        ai.retreatGoal = ai.safeAnchor;

        m_world.AddComponent<ChampionAIComponent>(entity, ai);
    }
```

아래로 교체:

```cpp
    ConfigureChampionControlRole(entity, slot);
```

### 1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

anonymous namespace의 `ResetPracticeCooldowns` 함수 바로 아래에 추가:

```cpp
    void ClearChampionControlIntent(
        CWorld& world,
        EntityID entity)
    {
        if (!world.IsAlive(entity))
            return;
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
        if (world.HasComponent<AttackChaseComponent>(entity))
            world.RemoveComponent<AttackChaseComponent>(entity);
    }

    bool_t HasStrictAttackSpeedLabRoster(
        const LobbySlotState* pSlots,
        u32_t slotCount)
    {
        if (!pSlots || slotCount != kGameRosterSlotCount)
            return false;

        u32_t activeCount = 0u;
        u32_t humanCount = 0u;
        u32_t botCount = 0u;
        u32_t blueCount = 0u;
        u32_t redCount = 0u;
        for (u32_t i = 0u; i < slotCount; ++i)
        {
            const LobbySlotState& slot = pSlots[i];
            if (slot.bHuman == slot.bBot || slot.bDummy)
                return false;
            if (slot.team > 1u)
                return false;

            ++activeCount;
            humanCount += slot.bHuman ? 1u : 0u;
            botCount += slot.bBot ? 1u : 0u;
            blueCount += slot.team == 0u ? 1u : 0u;
            redCount += slot.team == 1u ? 1u : 0u;
        }

        return activeCount == 10u &&
            humanCount == 1u &&
            botCount == 9u &&
            blueCount == 5u &&
            redCount == 5u;
    }

    void TransferPracticeControlState(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        if (!world.IsAlive(source) || !world.IsAlive(target) || source == target)
            return;

        if (world.HasComponent<PracticePlayerComponent>(source))
        {
            const PracticePlayerComponent state =
                world.GetComponent<PracticePlayerComponent>(source);
            if (world.HasComponent<PracticePlayerComponent>(target))
                world.GetComponent<PracticePlayerComponent>(target) = state;
            else
                world.AddComponent<PracticePlayerComponent>(target, state);
            world.RemoveComponent<PracticePlayerComponent>(source);
        }

        const auto spawned =
            DeterministicEntityIterator<PracticeSpawnedTag>::CollectSorted(world);
        for (EntityID entity : spawned)
        {
            if (!world.IsAlive(entity))
                continue;
            auto& tag = world.GetComponent<PracticeSpawnedTag>(entity);
            if (tag.ownerEntity == source)
                tag.ownerEntity = target;
        }
    }
```

`TryHandlePracticeControl`의 `switch (cmd.practiceOperation)` 안에서 `case ePracticeOperation::SpawnChampion:` 바로 위에 추가:

```cpp
    case ePracticeOperation::TakeControlRosterChampion:
    {
        if (!HasStrictAttackSpeedLabRoster(
            GetLobbySlots(),
            GetLobbySlotCount()))
        {
            return Finish(false, "strict-5v5-human1-bot9-required");
        }
        if (m_PendingPracticeControlChange.Kind !=
            PracticeControlChangeKind::None)
        {
            return Finish(false, "control-change-already-pending");
        }
        if (cmd.targetEntity == NULL_ENTITY ||
            cmd.targetEntity == cmd.issuerEntity ||
            !m_world.IsAlive(cmd.targetEntity) ||
            !m_world.HasComponent<ChampionComponent>(cmd.targetEntity))
        {
            return Finish(false, "roster-control-target-invalid");
        }

        const NetEntityId sourceNet = m_entityMap.ToNet(cmd.issuerEntity);
        const NetEntityId targetNet = m_entityMap.ToNet(cmd.targetEntity);
        bool_t bRosterBot = false;
        const LobbySlotState* pSlots = GetLobbySlots();
        for (u32_t i = 0u; pSlots && i < GetLobbySlotCount(); ++i)
        {
            if (pSlots[i].netId == targetNet &&
                pSlots[i].bBot &&
                !pSlots[i].bHuman &&
                !pSlots[i].bDummy)
            {
                bRosterBot = true;
                break;
            }
        }
        if (sourceNet == NULL_NET_ENTITY ||
            targetNet == NULL_NET_ENTITY ||
            !bRosterBot)
        {
            return Finish(false, "target-is-not-roster-bot");
        }

        m_PendingPracticeControlChange.Kind =
            PracticeControlChangeKind::TakeRosterChampion;
        m_PendingPracticeControlChange.SessionId = cmd.sourceSessionId;
        m_PendingPracticeControlChange.SourceNetId = sourceNet;
        m_PendingPracticeControlChange.TargetNetId = targetNet;
        return Finish(true, "roster-control-change-pending");
    }

    case ePracticeOperation::ReplaceControlledChampion:
    {
        if (!HasStrictAttackSpeedLabRoster(
            GetLobbySlots(),
            GetLobbySlotCount()))
        {
            return Finish(false, "strict-5v5-human1-bot9-required");
        }
        if (m_PendingPracticeControlChange.Kind !=
            PracticeControlChangeKind::None)
        {
            return Finish(false, "control-change-already-pending");
        }
        if (m_bSimPaused)
            return Finish(false, "replacement-requires-running-simulation");
        if (cmd.practiceFlags >
            static_cast<u32_t>((std::numeric_limits<u8_t>::max)()))
        {
            return Finish(false, "replacement-champion-id-out-of-range");
        }

        const eChampion champion = static_cast<eChampion>(
            static_cast<u8_t>(cmd.practiceFlags));
        const GameplayDefinitionPack& definitions =
            ServerData::GetLoLGameplayDefinitionPack();
        if (champion == eChampion::NONE ||
            champion == eChampion::END ||
            !definitions.FindChampion(champion))
        {
            return Finish(false, "replacement-champion-not-registered");
        }

        const NetEntityId sourceNet = m_entityMap.ToNet(cmd.issuerEntity);
        if (sourceNet == NULL_NET_ENTITY)
            return Finish(false, "replacement-source-net-missing");

        m_PendingPracticeControlChange.Kind =
            PracticeControlChangeKind::ReplaceControlledChampion;
        m_PendingPracticeControlChange.SessionId = cmd.sourceSessionId;
        m_PendingPracticeControlChange.SourceNetId = sourceNet;
        m_PendingPracticeControlChange.Champion = champion;
        return Finish(true, "controlled-champion-replacement-pending");
    }

```

아래 기존 코드 바로 위에 추가:

```cpp
void CGameRoom::ClearPracticeSpawns()
```

```cpp
void CGameRoom::CommitPendingPracticeControlChange(const TickContext& tc)
{
#if !defined(_DEBUG)
    (void)tc;
    m_PendingPracticeControlChange = {};
#else
    const PendingPracticeControlChange pending =
        m_PendingPracticeControlChange;
    m_PendingPracticeControlChange = {};
    if (pending.Kind == PracticeControlChangeKind::None ||
        !m_pLobbyAuthority)
    {
        return;
    }

    EntityID source = NULL_ENTITY;
    if (!m_sessionBinding.TryGetAlive(
        pending.SessionId,
        m_world,
        source) ||
        m_entityMap.ToNet(source) != pending.SourceNetId)
    {
        OutputDebugStringA(
            "[PracticeControl] commit rejected: source binding changed\n");
        return;
    }

    u8_t sourceSlotId = kInvalidGameRosterSlot;
    if (!m_pLobbyAuthority->TryGetSessionSlot(
        pending.SessionId,
        sourceSlotId))
    {
        OutputDebugStringA(
            "[PracticeControl] commit rejected: human slot missing\n");
        return;
    }

    if (pending.Kind == PracticeControlChangeKind::TakeRosterChampion)
    {
        const EntityID target = m_entityMap.FromNet(pending.TargetNetId);
        if (target == NULL_ENTITY || !m_world.IsAlive(target))
            return;

        u8_t targetSlotId = kInvalidGameRosterSlot;
        const LobbySlotState* pSlots = GetLobbySlots();
        for (u32_t i = 0u; pSlots && i < GetLobbySlotCount(); ++i)
        {
            if (pSlots[i].netId == pending.TargetNetId)
            {
                targetSlotId = pSlots[i].slotId;
                break;
            }
        }
        if (targetSlotId == kInvalidGameRosterSlot)
            return;

        const LobbyAuthorityResult result =
            m_pLobbyAuthority->TransferInGameHumanControl(
                pending.SessionId,
                pending.TargetNetId);
        if (!result.bSendHello)
            return;

        LobbySlotState* pSourceSlot =
            m_pLobbyAuthority->TryGetSlot(sourceSlotId);
        LobbySlotState* pTargetSlot =
            m_pLobbyAuthority->TryGetSlot(targetSlotId);
        if (!pSourceSlot || !pTargetSlot)
            return;

        TransferPracticeControlState(m_world, source, target);
        ClearChampionControlIntent(m_world, source);
        ClearChampionControlIntent(m_world, target);
        ConfigureChampionControlRole(source, *pSourceSlot);
        ConfigureChampionControlRole(target, *pTargetSlot);
        m_sessionBinding.Bind(pending.SessionId, target);

        m_keyframes.clear();
        m_pendingRewindToTick = 0u;
        ApplyLobbyAuthorityResult(result);
        OutputDebugStringA(
            "[PracticeControl] roster control transfer committed\n");
        return;
    }

    if (pending.Kind ==
        PracticeControlChangeKind::ReplaceControlledChampion)
    {
        LobbySlotState* pSourceSlot =
            m_pLobbyAuthority->TryGetSlot(sourceSlotId);
        if (!pSourceSlot)
            return;

        const GameplayDefinitionPack& definitions =
            ServerData::GetLoLGameplayDefinitionPack();
        if (!definitions.FindChampion(pending.Champion))
            return;

        const Vec3 sourcePosition =
            m_world.HasComponent<TransformComponent>(source)
                ? m_world.GetComponent<TransformComponent>(source).GetPosition()
                : GetSpawnPositionForLobbySlot(*pSourceSlot);

        LobbySlotState replacementSlot = *pSourceSlot;
        replacementSlot.netId = NULL_NET_ENTITY;
        replacementSlot.champion = pending.Champion;
        const EntityID replacement =
            SpawnChampionForLobbySlot(replacementSlot);
        if (replacement == NULL_ENTITY || !m_world.IsAlive(replacement))
        {
            m_sessionBinding.Bind(pending.SessionId, source);
            return;
        }

        m_world.GetComponent<TransformComponent>(replacement).SetPosition(
            sourcePosition);
        PositionDiscontinuityComponent& discontinuity =
            m_world.HasComponent<PositionDiscontinuityComponent>(replacement)
                ? m_world.GetComponent<PositionDiscontinuityComponent>(replacement)
                : m_world.AddComponent<PositionDiscontinuityComponent>(
                    replacement,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;

        const LobbyAuthorityResult result =
            m_pLobbyAuthority->ReplaceInGameControlledChampion(
                pending.SessionId,
                pending.Champion,
                replacementSlot.netId);
        if (!result.bSendHello)
        {
            m_entityMap.Unbind(replacementSlot.netId);
            m_world.DestroyEntity(replacement);
            m_sessionBinding.Bind(pending.SessionId, source);
            return;
        }

        TransferPracticeControlState(m_world, source, replacement);
        m_lastBroadcastActionSeq.erase(source);
        m_entityMap.Unbind(pending.SourceNetId);
        m_world.DestroyEntity(source);
        m_sessionBinding.Bind(pending.SessionId, replacement);
        if (m_pSpatialSystem)
            m_pSpatialSystem->Execute(m_world, DeterministicTime::kFixedDt);
        if (m_pLagCompensation)
        {
            m_pLagCompensation->Reset();
            m_pLagCompensation->RecordHistory(m_world, tc.tickIndex);
        }

        m_keyframes.clear();
        m_pendingRewindToTick = 0u;
        ApplyLobbyAuthorityResult(result);
        OutputDebugStringA(
            "[PracticeControl] controlled champion replacement committed\n");
    }
#endif
}
```

같은 파일의 `CGameRoom::TickPausedControlLane` 끝에 있는 아래 기존 코드를:

```cpp
    Phase_BroadcastSnapshot(tc);
}
```

아래로 교체:

```cpp
    if (m_PendingPracticeControlChange.Kind ==
        PracticeControlChangeKind::TakeRosterChampion)
    {
        CommitPendingPracticeControlChange(tc);
    }
    Phase_BroadcastSnapshot(tc);
}
```

### 1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

`CGameRoom::Tick`의 아래 기존 코드를:

```cpp
    Phase_DrainCommands(tc);
    Phase_ServerBotAI(tc);
    Phase_ExecuteCommands(tc);
    Phase_SimulationSystems(tc);
```

아래로 교체:

```cpp
    Phase_DrainCommands(tc);
    Phase_ServerBotAI(tc);
    Phase_ExecuteCommands(tc);
    CommitPendingPracticeControlChange(tc);
    Phase_SimulationSystems(tc);
```

### 1-11. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/SnapshotApplier.h

`CSnapshotApplier` public 영역의 아래 기존 코드 바로 아래에 추가:

```cpp
    void SetOnRemoveEntityCallback(OnRemoveEntityFn fn) { m_onRemoveEntity = std::move(fn); }
```

```cpp
    u32_t GetLocalNetId() const { return m_localNetId; }
    u32_t GetLastAckedCommandSequence() const
    {
        return m_lastAckedCommandSequence;
    }
```

private 멤버 영역의 아래 기존 코드 바로 아래에 추가:

```cpp
    u32_t m_localNetId = 0;
```

```cpp
    u32_t m_lastAckedCommandSequence = 0u;
```

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`CSnapshotApplier::OnHello`의 아래 기존 코드를:

```cpp
    if (hello->yourNetId() != 0)
        m_localNetId = hello->yourNetId();
```

아래로 교체:

```cpp
    if (hello->yourNetId() != 0 &&
        hello->yourNetId() != m_localNetId)
    {
        m_localMoveYawProtection = {};
        m_localNetId = hello->yourNetId();
    }
```

`CSnapshotApplier::OnSnapshot`의 아래 기존 코드를:

```cpp
    const u32_t snapshotLocalNetId = snapshot->yourNetId();
    if (m_localNetId == 0 && snapshotLocalNetId != 0)
        m_localNetId = snapshotLocalNetId;
    if (m_localNetId != 0 &&
        snapshotLocalNetId != 0 &&
        snapshotLocalNetId != m_localNetId)
    {
        static u32_t s_localNetIdMismatchLogCount = 0;
        if (s_localNetIdMismatchLogCount < 8)
        {
            char msg[160]{};
            sprintf_s(msg,
                "[SnapshotApplier] snapshot local net mismatch hello=%u snapshot=%u\n",
                m_localNetId,
                snapshotLocalNetId);
            OutputDebugStringA(msg);
            ++s_localNetIdMismatchLogCount;
        }
    }
    const u32_t localNetId = (m_localNetId != 0)
        ? m_localNetId
        : snapshotLocalNetId;
```

아래로 교체:

```cpp
    const u32_t snapshotLocalNetId = snapshot->yourNetId();
    if (snapshotLocalNetId != 0 && snapshotLocalNetId != m_localNetId)
    {
        char msg[160]{};
        sprintf_s(msg,
            "[SnapshotApplier] authoritative local net changed old=%u new=%u\n",
            m_localNetId,
            snapshotLocalNetId);
        OutputDebugStringA(msg);
        m_localMoveYawProtection = {};
        m_localNetId = snapshotLocalNetId;
    }
    const u32_t localNetId = m_localNetId;
```

같은 함수에서 아래 기존 코드 바로 아래에 추가:

```cpp
    const u32_t lastAckedCommandSeq = snapshot->lastAckedCommandSeq();
```

```cpp
    m_lastAckedCommandSequence = lastAckedCommandSeq;
```

full snapshot stale entity 정리 루프의 아래 기존 코드 바로 아래에 추가:

```cpp
        const bool_t bServerMinion =
            world.HasComponent<MinionComponent>(entity) ||
            world.HasComponent<MinionStateComponent>(entity);
```

```cpp
        const bool_t bServerChampion =
            world.HasComponent<ChampionComponent>(entity) &&
            world.HasComponent<ServerIdComponent>(entity);
```

`CSnapshotApplier::EnsureEntity`의 `if (e != NULL_ENTITY)` 블록에서 아래 기존 코드 바로 위에 추가:

```cpp
        if (kind == Shared::Schema::EntityKind::Turret ||
```

```cpp
        m_seenNetIds.insert(netId);
        if (kind == Shared::Schema::EntityKind::Champion)
            MarkServerId(world, e, netId);

```

같은 루프의 아래 기존 코드를:

```cpp
        if (!bBranchStaleRemote &&
            !bServerMinion &&
            !bViegoSoul &&
            !bKalistaSentinel &&
            !bWard &&
            !bProjectilePresentation)
```

아래로 교체:

```cpp
        if (!bBranchStaleRemote &&
            !bServerChampion &&
            !bServerMinion &&
            !bViegoSoul &&
            !bKalistaSentinel &&
            !bWard &&
            !bProjectilePresentation)
```

### 1-13. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

private 메서드 영역의 아래 기존 코드 바로 아래에 추가:

```cpp
    void BindPlayerToECSChampion(EntityID entity);
```

```cpp
    bool_t ApplyAuthoritativePlayerNetId(NetEntityId netId);
    void ClearPureECSChampionAlias(EntityID entity);
    void ResetLocalControlHandoffState();
```

### 1-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp

아래 기존 함수 전체를:

```cpp
void CScene_InGame::ApplyPlayerDeathInputLock()
{
    if (m_bPingWheelActive)
    {
        m_bPingWheelActive = false;
        CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
    }

    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);
    ClearNetworkAttackIntent();
    ClearActiveSkillRuntime();
    ResetLocalSkillRuntimeState();
    m_bKalistaPassiveDashActive = false;
    m_fKalistaPassiveDashElapsed = 0.f;
    m_vKalistaPassiveDashStart = {};
    m_vKalistaPassiveDashEnd = {};
    m_uKalistaLastPassiveDashActionSeq = 0u;
    m_bAnnieTibbersCommandMode = false;

    m_bMoving = false;
    m_bDashActive = false;
    m_fDashElapsed = 0.f;
    m_DashTargetEntity = NULL_ENTITY;
    m_bYasuoDashActive = false;
    m_fYasuoDashElapsed = 0.f;
    m_YasuoDashTargetEntity = NULL_ENTITY;
    m_bYasuoRActive = false;
    m_fYasuoRElapsed = 0.f;
    m_YasuoRTarget = NULL_ENTITY;
    m_iYasuoRHitsFired = 0;
    m_fYasuoRPrevHitTime = 0.f;
    m_fLastActionTimer = 0.f;
    m_fEndTransitionTimer = 0.f;
    m_pPendingEndAnim = nullptr;

    if (m_pPlayerTransform)
        m_vPlayerDest = m_pPlayerTransform->GetPosition();

    if (m_PlayerEntity != NULL_ENTITY)
    {
        if (m_World.HasComponent<MoveTargetComponent>(m_PlayerEntity))
            m_World.GetComponent<MoveTargetComponent>(m_PlayerEntity) = MoveTargetComponent{};
        if (m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
        {
            NavAgentComponent& Agent =
                m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
            Agent.bHasGoal = false;
            Agent.bPathDirty = false;
            Agent.pathCellsX.clear();
            Agent.pathCellsY.clear();
            Agent.iPathIndex = 0;
        }
    }
}
```

아래로 교체:

```cpp
void CScene_InGame::ResetLocalControlHandoffState()
{
    if (m_bPingWheelActive)
    {
        m_bPingWheelActive = false;
        CGameInstance::Get()->UI_Set_PingWheel(false, 0.f, 0.f, 0.f, 0.f);
    }

    SetHoveredTarget(NULL_ENTITY, eTeam::TEAM_END);
    ClearNetworkAttackIntent();
    ClearActiveSkillRuntime();
    ResetLocalSkillRuntimeState();
    m_bAnnieTibbersCommandMode = false;

    m_bMoving = false;
    m_bDashActive = false;
    m_fDashElapsed = 0.f;
    m_DashTargetEntity = NULL_ENTITY;
    m_bYasuoDashActive = false;
    m_fYasuoDashElapsed = 0.f;
    m_YasuoDashTargetEntity = NULL_ENTITY;
    m_bYasuoRActive = false;
    m_fYasuoRElapsed = 0.f;
    m_YasuoRTarget = NULL_ENTITY;
    m_iYasuoRHitsFired = 0;
    m_fYasuoRPrevHitTime = 0.f;
    m_fLastActionTimer = 0.f;
    m_fEndTransitionTimer = 0.f;
    m_pPendingEndAnim = nullptr;

    if (m_pPlayerTransform)
        m_vPlayerDest = m_pPlayerTransform->GetPosition();

    if (m_PlayerEntity != NULL_ENTITY)
    {
        if (m_World.HasComponent<MoveTargetComponent>(m_PlayerEntity))
            m_World.GetComponent<MoveTargetComponent>(m_PlayerEntity) = MoveTargetComponent{};
        if (m_World.HasComponent<NavAgentComponent>(m_PlayerEntity))
        {
            NavAgentComponent& Agent =
                m_World.GetComponent<NavAgentComponent>(m_PlayerEntity);
            Agent.bHasGoal = false;
            Agent.bPathDirty = false;
            Agent.pathCellsX.clear();
            Agent.pathCellsY.clear();
            Agent.iPathIndex = 0;
        }
    }
}

void CScene_InGame::ApplyPlayerDeathInputLock()
{
    ResetLocalControlHandoffState();
}
```

### 1-15. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

`InitializeNetworkSession`의 new entity callback을 아래 기존 코드에서:

```cpp
            [this](u32_t netId, u8_t championId, u8_t team) -> EntityID
            {
                (void)netId;
                return SpawnChampionEntity(
                    static_cast<eChampion>(championId),
                    static_cast<eTeam>(team));
            });
```

아래로 교체:

```cpp
            [this](u32_t netId, u8_t championId, u8_t team) -> EntityID
            {
                (void)netId;
                const eChampion champion =
                    static_cast<eChampion>(championId);
                const EntityID entity = SpawnChampionEntity(
                    champion,
                    static_cast<eTeam>(team));
                if (entity != NULL_ENTITY)
                    AssignPureECSChampionAlias(champion, entity);
                return entity;
            });
```

Hello 처리의 아래 기존 코드를:

```cpp
                if (localNetEntity != NULL_ENTITY)
                {
                    m_PlayerEntity = localNetEntity;
                    BindPlayerToECSChampion(m_PlayerEntity);
                }
```

아래로 교체:

```cpp
                if (localNetEntity != NULL_ENTITY)
                    ApplyAuthoritativePlayerNetId(bindNetId);
```

Snapshot 처리의 아래 기존 코드를:

```cpp
                snapshotApplier.OnSnapshot(m_World, entityMap, payload, len);
```

아래로 교체:

```cpp
                snapshotApplier.OnSnapshot(m_World, entityMap, payload, len);
                ApplyAuthoritativePlayerNetId(
                    snapshotApplier.GetLocalNetId());
```

`CScene_InGame::OnAuthoritativeSnapshot` 바로 위에 추가:

```cpp
void CScene_InGame::ClearPureECSChampionAlias(EntityID entity)
{
    if (m_SylasEntity == entity)
        m_SylasEntity = NULL_ENTITY;
    if (m_FioraEntity == entity)
        m_FioraEntity = NULL_ENTITY;
    if (m_JaxEntity == entity)
        m_JaxEntity = NULL_ENTITY;
    if (m_AnnieEntity == entity)
        m_AnnieEntity = NULL_ENTITY;
    if (m_AsheEntity == entity)
        m_AsheEntity = NULL_ENTITY;
    if (m_YoneEntity == entity)
        m_YoneEntity = NULL_ENTITY;
}

bool_t CScene_InGame::ApplyAuthoritativePlayerNetId(NetEntityId netId)
{
    if (netId == NULL_NET_ENTITY || !m_pEntityIdMap)
        return false;

    const EntityID nextPlayer = m_pEntityIdMap->FromNet(netId);
    if (nextPlayer == NULL_ENTITY ||
        !m_World.IsAlive(nextPlayer) ||
        !m_World.HasComponent<ChampionComponent>(nextPlayer))
    {
        return false;
    }

    if (m_pNetworkView)
        m_pNetworkView->SetMyNetEntityId(netId);
    if (nextPlayer == m_PlayerEntity)
        return true;

    const EntityID previousPlayer = m_PlayerEntity;
    ResetLocalControlHandoffState();
    m_NetworkMovePredictions.clear();
    m_uLastAckedMovePredictionSeq = 0u;

    const auto InvalidateNetworkAnimationState = [this](EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        m_NetworkActionAnimStates.erase(entity);
        m_NetworkChampionMoving.erase(entity);
    };
    InvalidateNetworkAnimationState(previousPlayer);
    InvalidateNetworkAnimationState(nextPlayer);

    if (previousPlayer != NULL_ENTITY &&
        m_World.IsAlive(previousPlayer) &&
        m_World.HasComponent<LocalPlayerTag>(previousPlayer))
    {
        m_World.RemoveComponent<LocalPlayerTag>(previousPlayer);
    }
    if (!m_World.HasComponent<LocalPlayerTag>(nextPlayer))
        m_World.AddComponent<LocalPlayerTag>(nextPlayer);

    m_PlayerEntity = nextPlayer;
    BindPlayerToECSChampion(nextPlayer);
    SyncActorHUDStateToEngineUI();

    char msg[160]{};
    sprintf_s(msg,
        "[InGameControl] rebound old=%u new=%u net=%u\n",
        static_cast<u32_t>(previousPlayer),
        static_cast<u32_t>(nextPlayer),
        netId);
    OutputDebugStringA(msg);
    return true;
}
```

remove entity callback의 아래 기존 코드 바로 위에 추가:

```cpp
                Viego::Fx::StopSoulIdle(m_World, entity);
```

```cpp
                if (entity == m_PlayerEntity)
                {
                    m_PlayerEntity = NULL_ENTITY;
                    m_pPlayerRenderer = nullptr;
                    m_pPlayerTransform = nullptr;
                }
                ClearPureECSChampionAlias(entity);
                UI::CAttackSpeedLab::OnEntityRemoved(m_World, entity);
```

### 1-16. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/GameSessionClient.cpp

`CGameSessionClient::OnHello` 함수 전체를 아래로 교체:

```cpp
void CGameSessionClient::OnHello(const u8_t* payload, u32_t len)
{
    if (!payload || len == 0)
        return;

    flatbuffers::Verifier verifier(payload, len);
    if (!Shared::Schema::VerifyHelloBuffer(verifier))
        return;

    const auto* hello = Shared::Schema::GetHello(payload);
    if (!hello)
        return;

    const u32_t helloSessionId = hello->sessionId();
    const u32_t helloNetId = hello->yourNetId();
    const eChampion helloChampion =
        static_cast<eChampion>(hello->championId());

    m_lobbyContext.bUseNetworkRoster = true;
    m_lobbyContext.MySessionId = helloSessionId;
    if (helloNetId != 0u)
        m_lobbyContext.MyNetId = helloNetId;
    if (helloChampion != eChampion::END)
        m_lobbyContext.SelectedChampion = helloChampion;
    m_lobbyContext.MyTeam = hello->team();

    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        const GameRosterSlot& slot = m_lobbyContext.Roster[i];
        const bool_t bNetMatch =
            helloNetId != 0u && slot.netId == helloNetId;
        const bool_t bSessionFallback =
            helloNetId == 0u &&
            helloSessionId != 0u &&
            slot.bHuman &&
            slot.sessionId == helloSessionId;
        if (!bNetMatch && !bSessionFallback)
            continue;

        m_lobbyContext.MySlotId = slot.slotId;
        if (helloNetId == 0u && slot.netId != 0u)
            m_lobbyContext.MyNetId = slot.netId;
        break;
    }

    if (m_pNetwork)
    {
        m_pNetwork->SetMySessionId(helloSessionId);
        if (helloNetId != 0u)
            m_pNetwork->SetMyNetEntityId(helloNetId);
    }
}
```

### 1-17. C:/Users/user/Desktop/Winters/Client/Public/UI/AttackSpeedLab.h

`CAttackSpeedLab` public 영역의 아래 기존 코드 바로 아래에 추가:

```cpp
        static void ResetRuntime();
```

```cpp
        static void OnEntityRemoved(
            const CWorld& world,
            EntityID entity);
```

### 1-18. C:/Users/user/Desktop/Winters/Client/Private/UI/AttackSpeedLab.cpp

아래 기존 include 바로 아래에 추가:

```cpp
#include "Network/Client/CommandSerializer.h"
```

```cpp
#include "Network/Client/SnapshotApplier.h"
#include "Shared/GameSim/Components/GoldComponent.h"
```

anonymous namespace의 `LabState` 바로 위에 추가:

```cpp
    enum class ControlRequestKind : u8_t
    {
        None = 0u,
        TakeRosterChampion,
        FreshReplace,
    };
```

`CAttackSpeedLab::ResetRuntime` 함수 바로 아래에 추가:

```cpp
    void CAttackSpeedLab::OnEntityRemoved(
        const CWorld& world,
        EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return;

        const u64_t handle = world.GetEntityHandle(entity).ToU64();
        g_AppliedByEntity.erase(handle);
        if (g_Lab.selectedEntity == entity)
            g_Lab.selectedEntity = NULL_ENTITY;
        if (g_Lab.uPendingEntityHandle == handle)
        {
            g_Lab.uPendingEntityHandle = 0u;
            g_Lab.uPendingCommandSequence = 0u;
            g_Lab.fPendingAttackSpeed = 0.f;
            g_Lab.status =
                "Previous AS target was removed; wait for replacement confirmation.";
        }
    }
```

`LabState`의 아래 기존 코드를:

```cpp
        f32_t fPendingAttackSpeed = 0.f;
        std::string status = "Press 8 to load the tuning JSON.";
        std::array<LabEntry, kChampionSlotCount> entries{};
```

아래로 교체:

```cpp
        f32_t fPendingAttackSpeed = 0.f;
        ControlRequestKind PendingControlKind = ControlRequestKind::None;
        u32_t PendingControlSequence = 0u;
        NetEntityId PendingSourceNet = NULL_NET_ENTITY;
        NetEntityId PendingTargetNet = NULL_NET_ENTITY;
        eChampion RequestedChampion = eChampion::IRELIA;
        std::string status = "Press 8 to load the tuning JSON.";
        std::array<LabEntry, kChampionSlotCount> entries{};
```

`SendPractice` 함수 전체를:

```cpp
    u32_t SendPractice(
        CScene_InGame* pScene,
        ePracticeOperation operation,
        f32_t value,
        u8_t slot,
        NetEntityId targetNet)
    {
        return pScene->GetCommandSerializer()->SendPracticeControl(
            *pScene->GetNetworkView(),
            operation,
            value,
            0u,
            slot,
            {},
            targetNet);
    }
```

아래로 교체:

```cpp
    u32_t SendPractice(
        CScene_InGame* pScene,
        ePracticeOperation operation,
        f32_t value,
        u8_t slot,
        NetEntityId targetNet,
        u32_t flags = 0u)
    {
        return pScene->GetCommandSerializer()->SendPracticeControl(
            *pScene->GetNetworkView(),
            operation,
            value,
            flags,
            slot,
            {},
            targetNet);
    }
```

`RefreshPendingState` 함수 바로 아래에 추가:

```cpp
    void ClearPendingControlRequest()
    {
        g_Lab.PendingControlKind = ControlRequestKind::None;
        g_Lab.PendingControlSequence = 0u;
        g_Lab.PendingSourceNet = NULL_NET_ENTITY;
        g_Lab.PendingTargetNet = NULL_NET_ENTITY;
    }

    void RefreshPendingControlState(CScene_InGame* pScene)
    {
        if (!pScene ||
            g_Lab.PendingControlKind == ControlRequestKind::None)
        {
            return;
        }

        const EntityID player = pScene->GetPlayerEntity();
        if (!IsTunableEntity(pScene->GetWorld(), player))
            return;

        const NetEntityId playerNet = ResolveTargetNetId(pScene, player);
        const eChampion playerChampion = static_cast<eChampion>(
            pScene->GetWorld().GetComponent<ChampionComponent>(player).id.value);
        EntityIdMap* pEntityMap = pScene->GetEntityIdMap();
        const bool_t bOldEntityGone =
            pEntityMap &&
            pEntityMap->FromNet(g_Lab.PendingSourceNet) == NULL_ENTITY;
        const bool_t bCanonicalBaseline =
            pScene->GetWorld().HasComponent<StatComponent>(player) &&
            pScene->GetWorld().HasComponent<GoldComponent>(player) &&
            pScene->GetWorld().GetComponent<StatComponent>(player).level == 6u &&
            pScene->GetWorld().GetComponent<GoldComponent>(player).amount == 10000u;
        const bool_t bTakeConfirmed =
            g_Lab.PendingControlKind == ControlRequestKind::TakeRosterChampion &&
            playerNet == g_Lab.PendingTargetNet;
        const bool_t bReplaceConfirmed =
            g_Lab.PendingControlKind == ControlRequestKind::FreshReplace &&
            playerNet != NULL_NET_ENTITY &&
            playerNet != g_Lab.PendingSourceNet &&
            playerChampion == g_Lab.RequestedChampion &&
            bOldEntityGone &&
            bCanonicalBaseline;
        if (!bTakeConfirmed && !bReplaceConfirmed)
        {
            const CSnapshotApplier* pSnapshotApplier =
                pScene->GetSnapshotApplier();
            if (pSnapshotApplier &&
                g_Lab.PendingControlSequence != 0u &&
                pSnapshotApplier->GetLastAckedCommandSequence() >=
                    g_Lab.PendingControlSequence)
            {
                g_Lab.status =
                    "Control request acknowledged but authority/invariants did not confirm; inspect trace, then Cancel Pending Control.";
            }
            return;
        }

        g_Lab.selectedEntity = player;
        ClearPendingControlRequest();
        g_Lab.status = bTakeConfirmed
            ? "Controlled: roster ownership, AI role, camera, HUD, and input rebound."
            : "Fresh replacement ready: level 6, gold 10000; Apply AS when ready.";
    }
```

`CAttackSpeedLab::Render`의 아래 기존 코드 바로 아래에 추가:

```cpp
        RefreshPendingState(pScene);
```

```cpp
        RefreshPendingControlState(pScene);
```

champion table의 아래 기존 코드 바로 아래에 추가:

```cpp
            ImGui::EndTable();
        }
```

```cpp
        const EntityID playerEntity = pScene->GetPlayerEntity();
        const NetEntityId playerNet = ResolveTargetNetId(pScene, playerEntity);
        const NetEntityId selectedNet = ResolveTargetNetId(
            pScene,
            g_Lab.selectedEntity);
        const CSnapshotApplier* pSnapshotApplier =
            pScene->GetSnapshotApplier();
        const bool_t bSimulationPaused =
            pSnapshotApplier &&
            pSnapshotApplier->GetTimelineState().simPaused;
        const bool_t bCanTakeControl =
            CanSendToServer(pScene) &&
            selectedNet != NULL_NET_ENTITY &&
            selectedNet != playerNet &&
            g_Lab.PendingControlKind == ControlRequestKind::None;

        ImGui::TextDisabled(
            "Existing roster: preserve live state. Fresh replace: reset to comparable baseline.");
        ImGui::BeginDisabled(!bCanTakeControl);
        if (ImGui::Button("Take Control of Selected", ImVec2(220.f, 0.f)))
        {
            SendPractice(
                pScene,
                ePracticeOperation::SetEnabled,
                1.f,
                0u,
                NULL_NET_ENTITY);
            const u32_t sequence = SendPractice(
                pScene,
                ePracticeOperation::TakeControlRosterChampion,
                0.f,
                0u,
                selectedNet);
            if (sequence != 0u)
            {
                g_Lab.PendingControlKind =
                    ControlRequestKind::TakeRosterChampion;
                g_Lab.PendingControlSequence = sequence;
                g_Lab.PendingSourceNet = playerNet;
                g_Lab.PendingTargetNet = selectedNet;
                g_Lab.status =
                    "Pending control transfer; wait for authoritative Hello/Snapshot.";
            }
        }
        ImGui::EndDisabled();

        const char* pRequestedName =
            GetChampionDisplayName(g_Lab.RequestedChampion);
        if (ImGui::BeginCombo("All Registered Champions", pRequestedName))
        {
            for (u16_t raw = 1u; raw < 255u; ++raw)
            {
                const eChampion candidate = static_cast<eChampion>(raw);
                if (!FindChampionDef(candidate))
                    continue;
                const bool_t bSelected = candidate == g_Lab.RequestedChampion;
                if (ImGui::Selectable(
                    GetChampionDisplayName(candidate),
                    bSelected))
                {
                    g_Lab.RequestedChampion = candidate;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool_t bCanFreshReplace =
            CanSendToServer(pScene) &&
            !bSimulationPaused &&
            playerNet != NULL_NET_ENTITY &&
            FindChampionDef(g_Lab.RequestedChampion) &&
            g_Lab.PendingControlKind == ControlRequestKind::None;
        ImGui::BeginDisabled(!bCanFreshReplace);
        if (ImGui::Button("Fresh Replace My Slot", ImVec2(220.f, 0.f)))
        {
            SendPractice(
                pScene,
                ePracticeOperation::SetEnabled,
                1.f,
                0u,
                NULL_NET_ENTITY);
            const u32_t sequence = SendPractice(
                pScene,
                ePracticeOperation::ReplaceControlledChampion,
                0.f,
                0u,
                NULL_NET_ENTITY,
                static_cast<u32_t>(g_Lab.RequestedChampion));
            if (sequence != 0u)
            {
                g_Lab.PendingControlKind = ControlRequestKind::FreshReplace;
                g_Lab.PendingControlSequence = sequence;
                g_Lab.PendingSourceNet = playerNet;
                g_Lab.PendingTargetNet = NULL_NET_ENTITY;
                g_Lab.status =
                    "Pending fresh replacement; old NetId must disappear before AS Apply.";
            }
        }
        ImGui::EndDisabled();

        if (bSimulationPaused)
        {
            ImGui::TextColored(
                ImVec4(1.f, 0.75f, 0.2f, 1.f),
                "Resume simulation before Fresh Replace; Take Control remains available.");
        }
        if (g_Lab.PendingControlKind != ControlRequestKind::None &&
            ImGui::Button("Cancel Pending Control", ImVec2(220.f, 0.f)))
        {
            ClearPendingControlRequest();
            g_Lab.status = "Pending control request cleared locally.";
        }
```

`Apply` 버튼 바로 위의 아래 기존 코드를:

```cpp
        const bool_t bCanApply =
            CanSendToServer(pScene) && targetNet != NULL_NET_ENTITY;
```

아래로 교체:

```cpp
        const bool_t bCanApply =
            CanSendToServer(pScene) &&
            targetNet != NULL_NET_ENTITY &&
            g_Lab.PendingControlKind == ControlRequestKind::None;
```

### 1-19. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp

아래 기존 include 바로 아래에 추가:

```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

```cpp
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

`class CGameRoomIntegrationProbeAccess`의 public 영역에서 `PrepareBotMatch` 바로 위에 추가:

```cpp
#if defined(_DEBUG)
    static bool_t RunPracticeChampionControlProbe(std::string& outError)
    {
        constexpr u32_t kSessionId = 7001u;
        auto room = CGameRoom::Create(7001u);
        if (!room || !room->m_pLobbyAuthority)
        {
            outError = "practice control probe room creation failed";
            return false;
        }

        static constexpr std::array<eChampion, kGameRosterSlotCount> kRoster =
        {
            eChampion::YASUO,
            eChampion::ZED,
            eChampion::ASHE,
            eChampion::ANNIE,
            eChampion::LEESIN,
            eChampion::RIVEN,
            eChampion::SYLAS,
            eChampion::VIEGO,
            eChampion::YONE,
            eChampion::JAX,
        };

        LobbySlotState* slots = room->m_pLobbyAuthority->GetSlots();
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            LobbySlotState slot{};
            slot.slotId = static_cast<u8_t>(i);
            slot.team = i < 5u ? 0u : 1u;
            slot.bHuman = i == 0u;
            slot.bBot = i != 0u;
            slot.sessionId = i == 0u ? kSessionId : 0u;
            slot.champion = kRoster[i];
            slot.botDifficulty = 2u;
            slot.bReady = true;
            slot.bLocked = true;
            slots[i] = slot;
        }
        room->m_pLobbyAuthority->m_phase = eRoomPhase::InGame;
        room->m_pLobbyAuthority->m_hostSessionId = kSessionId;
        room->m_pLobbyAuthority->m_sessionToSlot[kSessionId] = 0u;
        room->SpawnChampionsFromLobby();
        room->m_bPracticeModeEnabled = true;

        const EntityID original = room->m_entityMap.FromNet(slots[0].netId);
        const EntityID target = room->m_entityMap.FromNet(slots[1].netId);
        PracticePlayerComponent options{};
        options.optionFlags = kPracticeNoCooldownFlag;
        options.revision = 1u;
        room->m_world.AddComponent<PracticePlayerComponent>(original, options);

        TickContext tc{};
        tc.tickIndex = 1u;
        room->m_PendingPracticeControlChange.Kind =
            CGameRoom::PracticeControlChangeKind::TakeRosterChampion;
        room->m_PendingPracticeControlChange.SessionId = kSessionId;
        room->m_PendingPracticeControlChange.SourceNetId = slots[0].netId;
        room->m_PendingPracticeControlChange.TargetNetId = slots[1].netId;
        room->CommitPendingPracticeControlChange(tc);

        EntityID controlled = NULL_ENTITY;
        if (!room->m_sessionBinding.TryGet(kSessionId, controlled) ||
            controlled != target ||
            slots[0].bHuman || !slots[0].bBot ||
            !slots[1].bHuman || slots[1].bBot ||
            !room->m_world.HasComponent<ChampionAIComponent>(original) ||
            room->m_world.HasComponent<ChampionAIComponent>(target) ||
            !room->m_world.HasComponent<PracticePlayerComponent>(target) ||
            room->m_world.HasComponent<PracticePlayerComponent>(original))
        {
            outError = "roster control transfer invariant failed";
            return false;
        }

        const NetEntityId replacedNet = slots[1].netId;
        room->m_PendingPracticeControlChange.Kind =
            CGameRoom::PracticeControlChangeKind::ReplaceControlledChampion;
        room->m_PendingPracticeControlChange.SessionId = kSessionId;
        room->m_PendingPracticeControlChange.SourceNetId = replacedNet;
        room->m_PendingPracticeControlChange.Champion = eChampion::IRELIA;
        tc.tickIndex = 2u;
        room->CommitPendingPracticeControlChange(tc);

        if (!room->m_sessionBinding.TryGet(kSessionId, controlled) ||
            controlled == NULL_ENTITY ||
            !room->m_world.IsAlive(controlled) ||
            room->m_entityMap.FromNet(replacedNet) != NULL_ENTITY ||
            slots[1].netId == NULL_NET_ENTITY ||
            slots[1].netId == replacedNet ||
            slots[1].champion != eChampion::IRELIA ||
            room->m_world.GetComponent<ChampionComponent>(controlled).id !=
                eChampion::IRELIA ||
            room->m_world.GetComponent<StatComponent>(controlled).level != 6u ||
            room->m_world.GetComponent<GoldComponent>(controlled).amount != 10000u ||
            !room->m_world.HasComponent<IreliaSimComponent>(controlled) ||
            room->m_world.GetComponent<HealthComponent>(controlled).fCurrent !=
                room->m_world.GetComponent<HealthComponent>(controlled).fMaximum ||
            room->m_world.GetComponent<ChampionComponent>(controlled).mana !=
                room->m_world.GetComponent<ChampionComponent>(controlled).maxMana)
        {
            outError = "fresh controlled champion replacement invariant failed";
            return false;
        }

        const WorldMetrics metrics = CollectMetrics(*room);
        if (metrics.championCount != kGameRosterSlotCount ||
            metrics.botCount != kGameRosterSlotCount - 1u ||
            !metrics.bEntityMapConsistent)
        {
            outError = "practice control probe roster cardinality failed";
            return false;
        }

        std::cout
            << "PROBE practice_champion_control=PASS champions="
            << metrics.championCount
            << " bots=" << metrics.botCount
            << " controlled_net=" << slots[1].netId
            << '\n';
        return true;
    }
#endif

```

`main`에서 `START ticks=` 출력 바로 위에 추가:

```cpp
#if defined(_DEBUG)
    std::string practiceControlError;
    if (!CGameRoomIntegrationProbeAccess::RunPracticeChampionControlProbe(
        practiceControlError))
    {
        std::cerr
            << "RESULT status=FAIL reason=practice_champion_control_probe detail=\""
            << practiceControlError
            << "\"\n";
        return 1;
    }
#endif

```

## 2. 검증

검증 상태:

- 2026-07-14 기준 코드 반영, Command schema C++/Go 재생성 일치 확인, Server/Client Debug x64 직렬 빌드, Shared boundary 검사, Take/Fresh Debug 통합 프로브를 완료했다.
- 실제 창을 띄운 카메라·HUD·FOW·입력 및 17챔피언별 공격속도 0.8/2.5 육안 캡처는 아래 수동 확인 항목으로 남긴다.

검증 명령:

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
cmd /c .\Shared\Schemas\run_codegen.bat
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
Tools/Bin/Debug/SimLab.exe 1800 42
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 1 -Configuration Debug -SkipServerBuild
git diff --check
```

자동 확인:

- runtime `ePracticeOperation::Count == 23`과 generated `PracticeOperation::MAX == 22` 정적 검사가 Client/Server에서 통과해야 한다.
- control probe에서 roster champion 10, human 1, `ChampionAIComponent` 9가 유지되어야 한다.
- `Take Control` 뒤 source에 AI가 생기고 target AI가 제거되며 `PracticePlayerComponent`가 target으로 이동해야 한다.
- `Fresh Replace` 뒤 old NetId가 `EntityIdMap`에서 사라지고 새 NetId가 human slot·SessionBinding·Snapshot `yourNetId`에 일치해야 한다.
- fresh entity가 canonical level 6, gold 10000, full HP/MP, 새 champion 전용 Sim component를 가져야 한다.
- 전환 성공 시 이전 rewind keyframe이 삭제되고 다음 keyframe부터 새 Lobby/SessionBinding과 일치해야 한다.
- full snapshot에서 사라진 old server champion이 client World, renderer map, interpolation/action cache, EntityIdMap에서 제거되어야 한다.
- fresh replace commit이 command 실행 직후·simulation 전에 완료되어 spatial index, owner-invalid projectile/summon 정리, snapshot이 같은 tick 기준으로 일치해야 한다.

수동 확인:

- Debug Server를 정상 5:5 roster로 실행하고 Debug Client로 접속한다.
- 숫자 8 패널에서 같은 팀 bot, 반대 팀 bot, 다시 원래 팀 bot 순서로 `Take Control`한다.
- 매 전환마다 카메라, HUD, FOW 팀, 미니맵, hover outline, 스킬 입력, K/D/A local 판정이 새 Player와 일치하는지 확인한다.
- 새 Player는 즉시 수동 입력만 받고 이전 Player는 다음 tick부터 bot 이동·공격을 재개하는지 확인한다.
- 다른 human, dummy, invalid NetId, 중복 pending 요청이 거부되는지 확인한다.
- `All Registered Champions`에서 현재 roster에 없는 챔피언을 `Fresh Replace My Slot`하고 화면과 서버 모두 챔피언이 정확히 10명인지 확인한다.
- fresh replace 직후 old model/체력바/FX/outline이 남지 않고 새 챔피언이 현재 위치에서 level 6·gold 10000으로 시작하는지 확인한다.
- pause 중에는 `Fresh Replace`가 비활성/서버 거부되고 `Take Control`만 즉시 가능하며, resume 뒤 replacement가 정상 수락되는지 확인한다.
- control request가 거부되거나 invariant를 만족하지 못하면 Apply가 잠긴 채 상태 문구가 표시되고 `Stop Waiting (UI Only)`로 로컬 대기 상태를 즉시 복구하는지 확인한다. 이미 서버가 수락한 결과는 계속 권위 상태로 취급한다.
- Kalista passive dash 중 `Take Control`한 뒤 새 Player가 이전 dash start/end/elapsed/action sequence를 사용하지 않는지 확인한다.
- 각 등록 챔피언마다 `Fresh Replace -> AS 0.8 Apply -> 10초 공격 캡처 -> AS 2.5 Apply -> 10초 공격 캡처`를 반복한다.
- Save JSON 후 같은 챔피언을 다시 fresh replace하고 Apply했을 때 저장값과 실제 Observed AS/공격 주기/애니메이션이 일치하는지 확인한다.
- 연결 종료 후 재접속하면 마지막으로 옮긴 human slot/NetId에 다시 붙는지 확인한다.
- ceiling 산출물로 17챔피언 `0.8/2.5`, action cadence, animation 이상 여부, old ghost 여부를 한 표에 기록하고 Irelia 대표 비교 영상을 남긴다.

추가 회귀 확인:

- Release harness에서는 `_DEBUG` probe가 컴파일 경로에서 제외되고 기존 soak만 그대로 실행되는지 확인한다.
- fresh replace 직전 old source가 만들었거나 old source를 겨냥한 projectile·pending hit·damage request·replicated event가 새 baseline에 섞이지 않고 같은 commit에서 정리되는지 확인한다.
- snapshot identity fallback은 `OnSnapshot` 전체 적용이 끝난 뒤에만 player rebind를 수행해 default transform/HUD를 먼저 잡지 않는지 bounded Debug trace로 확인한다.

## 3. 2026-07-14 구현·빌드 결과

- 숫자 8은 `Client/Bin/Resource/Config/Practice/attack_speed_tuning.json`을 다시 읽고 Attack Speed Lab을 연다.
- `Take Control of Selected`는 현재 authoritative 10-slot roster의 bot만 대상으로 하며, 새 Player는 기존 전투 상태를 유지하고 이전 Player는 bot AI 역할로 돌아간다.
- `Fresh Baseline Replace My Slot`은 등록 챔피언을 중복 허용으로 선택해 현재 human slot만 새 NetId의 level 6·gold 10000 entity로 교체한다.
- control 완료는 command ACK와 서로 독립적인 Hello/Snapshot NetId가 모두 일치해야 확정된다. 대기 복구는 `Stop Waiting (UI Only)`이며 서버 권위 결과를 취소하지 않는다.
- 공격속도 animation correction은 command ACK, replicated observed AS, 동일 champion을 모두 확인한 뒤에만 적용된다.
- Fresh destroy 전 minion/turret/jungle/champion AI 및 champion-specific raw `EntityID` 관계를 scrub해 entity index 재사용에 따른 stale target 재결합을 차단했다.

완료된 검증:

```text
SharedBoundary: PASS
Command schema C++ regeneration: MATCH
Command schema Go regeneration: MATCH
Server Debug x64 /m:1 /nr:false: PASS
Client Debug x64 /m:1 /nr:false: PASS
CONTROL_PROBE: PASS (champions=10, human=1, bots=9, blue=5, red=5,
                     level=6, gold=10000, tool_revision=2, replay_commands=2)
1-tick bot soak: PASS
```

자동 프로브 증거:

- `.md/build/evidence/s024_bot_soak/debug_ticks_1_seed_42_20260714_200902_529_87064503/run_01/soak_output.txt`

최종 실행 산출물:

- `Server/Bin/Debug/WintersServer.exe`
- `Client/Bin/Debug/WintersGame.exe`

전달 주의:

- `Shared/Schemas/Generated/go/Shared/Schema/PracticeOperation.go`, Attack Speed Lab 소스, runtime tuning JSON, 이 계획서와 harness는 현재 큰 공유 dirty worktree 안의 untracked 파일을 포함한다. 후속 커밋에서는 누락 없이 명시적으로 포함해야 한다.
- replay 파일은 새 command payload를 보존하지만 Client ReplayPlayer가 Command record를 재실행하지 않으므로 Take/Fresh의 행동 검증은 위 live integration probe와 인게임 수동 확인을 기준으로 한다.
