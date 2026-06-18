Session - 5-1 반영 이후 현재 코드베이스에서 150 챔피언 데이터, 10명 매치 vector, Jax dodge, 데이터 스크립트를 완성한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDataDef.h

새 파일:

```cpp
#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
#include "WintersTypes.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace ChampionRuntime
{
    inline constexpr std::size_t kChampionRuntimeSkillSlotCount =
        static_cast<std::size_t>(eSkillSlot::SLOT_END);
    inline constexpr std::size_t kChampionRuntimeMaxSelectedCount =
        kGameRosterSlotCount;
    inline constexpr std::size_t kChampionRuntimeCatalogCapacity = 150;

    enum class eChampionRuntimeDataSource : u8_t
    {
        BuiltInFallback = 0,
        JsonScript = 1,
        LuaExport = 2,
    };

    struct ChampionRuntimeStageDataDef
    {
        u8_t stage = 1;
        eTargetMode targetMode = eTargetMode::Self;
        eRotateMode rotate = eRotateMode::None;
        f32_t lockDurationSec = 0.6f;
        f32_t animPlaySpeed = 1.f;
        f32_t castFrame = 0.f;
        f32_t recoveryFrame = 0.f;
    };

    struct ChampionRuntimeSkillDataDef
    {
        eSkillSlot slot = eSkillSlot::BasicAttack;
        eTargetMode targetMode = eTargetMode::Self;
        f32_t cooldownSec = 0.f;
        f32_t rangeMax = 0.f;
        f32_t manaCost = 0.f;
        u8_t stageCount = 1;
        f32_t stageWindowSec = 0.f;
        std::array<ChampionRuntimeStageDataDef, 2> stages{};
        std::string animKey;
        std::string vfxKey;
        std::string sfxKey;
    };

    struct ChampionRuntimeBasicAttackDataDef
    {
        f32_t cooldownSec = 1.f;
        f32_t rangeMax = 1.5f;
        f32_t windupSec = 0.25f;
        f32_t actionDurationSec = 0.75f;
        f32_t animPlaySpeed = 1.f;
    };

    struct ChampionStatusEffectDataDef
    {
        std::string key;
        eSkillSlot skillSlot = eSkillSlot::BasicAttack;
        eStatusEffectId effectId = eStatusEffectId::None;
        eStatusStackPolicy stackPolicy = eStatusStackPolicy::RefreshDuration;
        u32_t stateFlags = 0u;
        f32_t durationSec = 0.f;
        f32_t moveSpeedMul = 1.f;
        GameplayFeedback::WorldTextFeedbackKind feedbackKind =
            GameplayFeedback::WorldTextFeedbackKind::None;
    };

    struct ChampionFeedbackTextDataDef
    {
        std::string key;
        GameplayFeedback::WorldTextFeedbackKind kind =
            GameplayFeedback::WorldTextFeedbackKind::None;
        std::string text;
        std::string fontKey;
    };

    struct ChampionRuntimeDataDef
    {
        eChampion champion = eChampion::NONE;
        u16_t championNumericId = 0;
        std::string championKey;
        ChampionStatsDef stats{};
        ChampionRuntimeBasicAttackDataDef basicAttack{};
        std::array<ChampionRuntimeSkillDataDef, kChampionRuntimeSkillSlotCount> skills{};
        std::vector<ChampionStatusEffectDataDef> statusEffects{};
        std::vector<ChampionFeedbackTextDataDef> feedbackTexts{};
        eChampionRuntimeDataSource source = eChampionRuntimeDataSource::BuiltInFallback;
    };
}
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionRuntimeData/ChampionRuntimeDataRegistry.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionRuntimeDataDef.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace ChampionRuntime
{
    inline std::string ToChampionKey(eChampion champion)
    {
        switch (champion)
        {
        case eChampion::IRELIA: return "IRELIA";
        case eChampion::YASUO: return "YASUO";
        case eChampion::KALISTA: return "KALISTA";
        case eChampion::SYLAS: return "SYLAS";
        case eChampion::VIEGO: return "VIEGO";
        case eChampion::ANNIE: return "ANNIE";
        case eChampion::ASHE: return "ASHE";
        case eChampion::FIORA: return "FIORA";
        case eChampion::GAREN: return "GAREN";
        case eChampion::RIVEN: return "RIVEN";
        case eChampion::ZED: return "ZED";
        case eChampion::EZREAL: return "EZREAL";
        case eChampion::YONE: return "YONE";
        case eChampion::JAX: return "JAX";
        case eChampion::MASTERYI: return "MASTERYI";
        case eChampion::KINDRED: return "KINDRED";
        case eChampion::LEESIN: return "LEESIN";
        default: return "NONE";
        }
    }

    inline ChampionRuntimeDataDef BuildFallbackChampionRuntimeData(eChampion champion)
    {
        ChampionRuntimeDataDef data{};
        data.champion = champion;
        data.championNumericId = static_cast<u16_t>(static_cast<u8_t>(champion));
        data.championKey = ToChampionKey(champion);
        data.stats = BuildDefaultChampionStatsDef(champion);
        data.source = eChampionRuntimeDataSource::BuiltInFallback;

        const ChampionBasicAttackTimingDefaults basicTiming =
            GetDefaultChampionBasicAttackTiming(champion);
        data.basicAttack.cooldownSec =
            GetDefaultChampionSkillCooldown(
                champion,
                static_cast<u8_t>(eSkillSlot::BasicAttack));
        data.basicAttack.rangeMax =
            GetDefaultChampionSkillRange(
                champion,
                static_cast<u8_t>(eSkillSlot::BasicAttack));
        data.basicAttack.windupSec = basicTiming.fWindupSec;
        data.basicAttack.actionDurationSec = basicTiming.fActionDurationSec;
        data.basicAttack.animPlaySpeed = basicTiming.fAnimPlaySpeed;

        for (std::size_t slotIndex = 0; slotIndex < data.skills.size(); ++slotIndex)
        {
            ChampionRuntimeSkillDataDef& skill = data.skills[slotIndex];
            skill.slot = static_cast<eSkillSlot>(slotIndex);
            skill.cooldownSec =
                GetDefaultChampionSkillCooldown(champion, static_cast<u8_t>(slotIndex));
            skill.rangeMax =
                GetDefaultChampionSkillRange(champion, static_cast<u8_t>(slotIndex));
            skill.stageCount =
                IsDefaultChampionSkillTwoStage(champion, static_cast<u8_t>(slotIndex))
                    ? 2
                    : 1;
            skill.stageWindowSec =
                GetDefaultChampionSkillStageWindowSec(
                    champion,
                    static_cast<u8_t>(slotIndex));

            const u8_t stageCount =
                std::min<u8_t>(skill.stageCount, static_cast<u8_t>(skill.stages.size()));
            for (u8_t stageIndex = 0; stageIndex < stageCount; ++stageIndex)
            {
                const u8_t stage = static_cast<u8_t>(stageIndex + 1u);
                ChampionRuntimeStageDataDef& stageDef = skill.stages[stageIndex];
                const ChampionSkillTimingDefaults timing =
                    GetDefaultChampionSkillTiming(
                        champion,
                        static_cast<u8_t>(slotIndex),
                        stage);
                stageDef.stage = stage;
                stageDef.lockDurationSec = timing.lockDurationSec;
                stageDef.animPlaySpeed = timing.animPlaySpeed;
            }
        }

        return data;
    }

    class CChampionRuntimeDataRegistry
    {
    public:
        static CChampionRuntimeDataRegistry& Instance()
        {
            static CChampionRuntimeDataRegistry s_inst;
            return s_inst;
        }

        void Clear()
        {
            m_entries.clear();
        }

        void Add(eChampion champion, ChampionRuntimeDataDef data)
        {
            if (champion == eChampion::NONE || champion == eChampion::END)
                return;

            data.champion = champion;
            data.championNumericId = static_cast<u16_t>(static_cast<u8_t>(champion));
            if (data.championKey.empty())
                data.championKey = ToChampionKey(champion);
            m_entries[champion] = std::move(data);
        }

        const ChampionRuntimeDataDef* Find(eChampion champion) const
        {
            const auto it = m_entries.find(champion);
            return it != m_entries.end() ? &it->second : nullptr;
        }

        ChampionRuntimeDataDef Resolve(eChampion champion) const
        {
            if (const ChampionRuntimeDataDef* data = Find(champion))
                return *data;

            return BuildFallbackChampionRuntimeData(champion);
        }

        std::size_t Count() const
        {
            return m_entries.size();
        }

    private:
        CChampionRuntimeDataRegistry() = default;
        ~CChampionRuntimeDataRegistry() = default;
        CChampionRuntimeDataRegistry(const CChampionRuntimeDataRegistry&) = delete;
        CChampionRuntimeDataRegistry& operator=(const CChampionRuntimeDataRegistry&) = delete;

        std::unordered_map<eChampion, ChampionRuntimeDataDef> m_entries{};
    };
}
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Runtime/ChampionRuntimeMatchData.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "GameContext.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDataDef.h"
#include "WintersTypes.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ChampionRuntime
{
    struct ChampionRuntimeMatchEntry
    {
        u8_t slotId = kInvalidGameRosterSlot;
        EntityID entity = NULL_ENTITY;
        eChampion champion = eChampion::NONE;
        ChampionRuntimeDataDef data{};
    };

    class ChampionRuntimeMatchData
    {
    public:
        void Clear()
        {
            m_entries.clear();
        }

        bool_t AddChampion(
            u8_t slotId,
            EntityID entity,
            const ChampionRuntimeDataDef& data)
        {
            if (entity == NULL_ENTITY ||
                data.champion == eChampion::NONE ||
                data.champion == eChampion::END)
            {
                return false;
            }

            ChampionRuntimeMatchEntry next{};
            next.slotId = slotId;
            next.entity = entity;
            next.champion = data.champion;
            next.data = data;

            auto it = std::find_if(
                m_entries.begin(),
                m_entries.end(),
                [slotId, entity](const ChampionRuntimeMatchEntry& entry)
                {
                    return entry.slotId == slotId || entry.entity == entity;
                });
            if (it != m_entries.end())
            {
                *it = std::move(next);
                return true;
            }

            if (m_entries.size() >= kChampionRuntimeMaxSelectedCount)
                return false;

            m_entries.push_back(std::move(next));
            return true;
        }

        const std::vector<ChampionRuntimeMatchEntry>& Entries() const
        {
            return m_entries;
        }

        std::size_t Count() const
        {
            return m_entries.size();
        }

        const ChampionRuntimeMatchEntry* FindByEntity(EntityID entity) const
        {
            const auto it = std::find_if(
                m_entries.begin(),
                m_entries.end(),
                [entity](const ChampionRuntimeMatchEntry& entry)
                {
                    return entry.entity == entity;
                });
            return it != m_entries.end() ? &(*it) : nullptr;
        }

    private:
        std::vector<ChampionRuntimeMatchEntry> m_entries{};
    };
}
```

1-4. C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Runtime/ChampionRuntimeMatchData.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

기존 코드:

```cpp
    bool IsRunning() const { return m_bRunning.load(std::memory_order_relaxed); }
```

아래에 추가:

```cpp
    const ChampionRuntime::ChampionRuntimeMatchData& GetChampionRuntimeMatchData() const
    {
        return m_championRuntimeMatch;
    }
```

기존 코드:

```cpp
    EntityID SpawnChampionForLobbySlot(LobbySlotState& slot);
```

아래로 교체:

```cpp
    EntityID SpawnChampionForLobbySlot(
        LobbySlotState& slot,
        const ChampionRuntime::ChampionRuntimeDataDef& runtimeData);
```

기존 코드:

```cpp
    bool_t m_bGameplayObjectsSpawned = false;
    CServerMinionWaveRuntime m_serverMinionWaves{};
    LobbySlotState m_lobbySlots[kGameRosterSlotCount]{};
```

아래로 교체:

```cpp
    bool_t m_bGameplayObjectsSpawned = false;
    CServerMinionWaveRuntime m_serverMinionWaves{};
    ChampionRuntime::ChampionRuntimeMatchData m_championRuntimeMatch{};
    LobbySlotState m_lobbySlots[kGameRosterSlotCount]{};
```

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionRuntimeData/ChampionRuntimeDataRegistry.h"
```

기존 코드:

```cpp
void CGameRoom::SpawnChampionsFromLobby()
{
    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_lobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
            continue;
        if (slot.netId != NULL_NET_ENTITY)
            continue;

        SpawnChampionForLobbySlot(slot);
    }
}
```

아래로 교체:

```cpp
void CGameRoom::SpawnChampionsFromLobby()
{
    m_championRuntimeMatch.Clear();

    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        LobbySlotState& slot = m_lobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
            continue;

        const ChampionRuntime::ChampionRuntimeDataDef runtimeData =
            ChampionRuntime::CChampionRuntimeDataRegistry::Instance().Resolve(
                slot.champion);

        EntityID entity = NULL_ENTITY;
        if (slot.netId != NULL_NET_ENTITY)
        {
            entity = m_entityMap.FromNet(slot.netId);
        }
        else
        {
            entity = SpawnChampionForLobbySlot(slot, runtimeData);
        }

        if (entity != NULL_ENTITY)
            m_championRuntimeMatch.AddChampion(slot.slotId, entity, runtimeData);
    }
}
```

기존 코드:

```cpp
EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)
```

아래로 교체:

```cpp
EntityID CGameRoom::SpawnChampionForLobbySlot(
    LobbySlotState& slot,
    const ChampionRuntime::ChampionRuntimeDataDef& runtimeData)
```

기존 코드:

```cpp
    const ChampionStatsDef statsDef =
        CChampionStatsRegistry::Instance().Resolve(slot.champion);
```

아래로 교체:

```cpp
    const ChampionStatsDef statsDef = runtimeData.stats;
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h

기존 코드:

```cpp
    bool_t CanCast(CWorld& world, EntityID entity);
    bool_t CanBeSeenBy(CWorld& world, EntityID observer, EntityID target);
```

아래로 교체:

```cpp
    bool_t CanCast(CWorld& world, EntityID entity);
    bool_t IsAirborne(CWorld& world, EntityID entity);
    bool_t CanBeSeenBy(CWorld& world, EntityID observer, EntityID target);
```

기존 코드:

```cpp
    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveProjectileHit(CWorld& world, EntityID source, EntityID target);
```

아래로 교체:

```cpp
    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveBasicAttack(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveProjectileHit(CWorld& world, EntityID source, EntityID target);
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp

기존 코드:

```cpp
    bool_t CanMove(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotMoveFlag |
            kGameplayStateStunnedFlag;
        return !HasState(world, entity, kBlocked);
    }
```

아래로 교체:

```cpp
    bool_t CanMove(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotMoveFlag |
            kGameplayStateStunnedFlag |
            kGameplayStateAirborneFlag;
        return !HasState(world, entity, kBlocked);
    }
```

기존 코드:

```cpp
    bool_t CanAttack(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotAttackFlag |
            kGameplayStateStunnedFlag |
            kGameplayStateDisarmedFlag;
        return !HasState(world, entity, kBlocked);
    }
```

아래로 교체:

```cpp
    bool_t CanAttack(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotAttackFlag |
            kGameplayStateStunnedFlag |
            kGameplayStateDisarmedFlag |
            kGameplayStateAirborneFlag;
        return !HasState(world, entity, kBlocked);
    }
```

기존 코드:

```cpp
    bool_t CanCast(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotCastFlag |
            kGameplayStateStunnedFlag;
        return !HasState(world, entity, kBlocked);
    }
```

아래로 교체:

```cpp
    bool_t CanCast(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotCastFlag |
            kGameplayStateStunnedFlag |
            kGameplayStateAirborneFlag;
        return !HasState(world, entity, kBlocked);
    }

    bool_t IsAirborne(CWorld& world, EntityID entity)
    {
        return HasState(world, entity, kGameplayStateAirborneFlag);
    }
```

기존 코드:

```cpp
    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target)
    {
        (void)source;
        return IsAliveGameplayTarget(world, target) &&
            !HasState(world, target, kGameplayStateUntargetableFlag);
    }
```

아래에 추가:

```cpp
    bool_t CanReceiveBasicAttack(CWorld& world, EntityID source, EntityID target)
    {
        if (!CanReceiveDamage(world, source, target))
            return false;
        if (HasState(world, target, kGameplayStateDodgesBasicAttacksFlag))
            return false;
        return true;
    }
```

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h

기존 코드:

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext& tc);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

아래로 교체:

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext& tc);
	bool_t RemoveStatusEffectByStackGroup(CWorld& world, EntityID target,
		u16_t stackGroup);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

1-9. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

기존 코드:

```cpp
    void UpsertEffect(StatusEffectComponent& effects, const StatusEffectApplyDesc& desc)
    {
        if (desc.stackPolicy != eStatusStackPolicy::AddIndependent)
        {
            for (u8_t i = 0; i < effects.count; ++i)
            {
                StatusEffectInstance& effect = effects.active[i];
                if (!IsSameStack(effect, desc))
                    continue;

                effect.effectId = desc.effectId;
                effect.stackPolicy = desc.stackPolicy;
                effect.sourceEntity = desc.sourceEntity;
                effect.stackGroup = desc.stackGroup;
                effect.stateFlags = desc.stateFlags;
                effect.fMoveSpeedMul = desc.fMoveSpeedMul;
                if (desc.stackPolicy == eStatusStackPolicy::KeepLongest)
                    effect.fRemainingSec = (std::max)(effect.fRemainingSec, desc.fDurationSec);
                else
                    effect.fRemainingSec = desc.fDurationSec;
                return;
            }
        }

        StatusEffectInstance next{};
        next.effectId = desc.effectId;
        next.stackPolicy = desc.stackPolicy;
        next.sourceEntity = desc.sourceEntity;
        next.stackGroup = desc.stackGroup;
        next.stateFlags = desc.stateFlags;
        next.fRemainingSec = desc.fDurationSec;
        next.fMoveSpeedMul = desc.fMoveSpeedMul;

        if (effects.count < kMaxStatusEffectInstances)
        {
            effects.active[effects.count++] = next;
            return;
        }

        u8_t replaceIndex = 0;
        for (u8_t i = 1; i < effects.count; ++i)
        {
            if (effects.active[i].fRemainingSec < effects.active[replaceIndex].fRemainingSec)
                replaceIndex = i;
        }
        effects.active[replaceIndex] = next;
    }
```

아래에 추가:

```cpp
    bool_t RemoveEffectByStackGroup(StatusEffectComponent& effects, u16_t stackGroup)
    {
        if (stackGroup == 0u)
            return false;

        bool_t bRemoved = false;
        u8_t write = 0;
        for (u8_t read = 0; read < effects.count; ++read)
        {
            if (effects.active[read].stackGroup == stackGroup)
            {
                bRemoved = true;
                continue;
            }

            if (write != read)
                effects.active[write] = effects.active[read];
            ++write;
        }

        for (u8_t i = write; i < effects.count; ++i)
            effects.active[i] = StatusEffectInstance{};
        effects.count = write;
        return bRemoved;
    }
```

기존 코드:

```cpp
    void ApplyStatusEffect(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext& tc)
    {
        if (!ApplyStatusEffectInternal(world, target, desc))
            return;

        const GameplayFeedback::WorldTextFeedbackKind feedbackKind =
            GameplayFeedback::ResolveStatusFeedbackKind(desc.effectId, desc.stateFlags);
        (void)GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            desc.sourceEntity,
            target,
            feedbackKind);
    }
```

아래에 추가:

```cpp
    bool_t RemoveStatusEffectByStackGroup(
        CWorld& world,
        EntityID target,
        u16_t stackGroup)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            stackGroup == 0u ||
            !world.HasComponent<StatusEffectComponent>(target))
        {
            return false;
        }

        StatusEffectComponent& effects = world.GetComponent<StatusEffectComponent>(target);
        const bool_t bRemoved = RemoveEffectByStackGroup(effects, stackGroup);
        if (bRemoved)
            GameplayStatus::RebuildGameplayState(world, target);
        return bRemoved;
    }
```

1-10. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h

기존 코드:

```cpp
    inline StatusEffectApplyDesc MakeAirborneDesc(
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        StatusEffectApplyDesc desc{};
        desc.effectId = eStatusEffectId::GenericAirborne;
        desc.stackPolicy = eStatusStackPolicy::RefreshDuration;
        desc.sourceEntity = source;
        desc.stackGroup = MakeStatusStackGroup(champion, slot);
        desc.stateFlags =
            kGameplayStateAirborneFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        desc.fDurationSec = durationSec;
        desc.fMoveSpeedMul = 1.f;
        return desc;
    }
```

아래에 추가:

```cpp
    inline StatusEffectApplyDesc MakeDodgeBasicAttackDesc(
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        StatusEffectApplyDesc desc{};
        desc.effectId = eStatusEffectId::JaxCounterStrike;
        desc.stackPolicy = eStatusStackPolicy::RefreshDuration;
        desc.sourceEntity = source;
        desc.stackGroup = MakeStatusStackGroup(champion, slot);
        desc.stateFlags = kGameplayStateDodgesBasicAttacksFlag;
        desc.fDurationSec = durationSec;
        desc.fMoveSpeedMul = 1.f;
        return desc;
    }
```

기존 코드:

```cpp
    inline void ApplyAirborne(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeAirborneDesc(source, champion, slot, durationSec),
            tc);
    }
```

아래에 추가:

```cpp
    inline void ApplyDodgeBasicAttack(
        CWorld& world,
        EntityID target,
        EntityID source,
        eChampion champion,
        eSkillSlot slot,
        f32_t durationSec)
    {
        ApplyStatusEffect(
            world,
            target,
            MakeDodgeBasicAttackDesc(source, champion, slot, durationSec));
    }

    inline bool_t RemoveSkillStatus(
        CWorld& world,
        EntityID target,
        eChampion champion,
        eSkillSlot slot)
    {
        return RemoveStatusEffectByStackGroup(
            world,
            target,
            MakeStatusStackGroup(champion, slot));
    }
```

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

기존 코드:

```cpp
        if (!IsAliveForBasicAttackImpact(world, source) ||
            !IsAliveForBasicAttackImpact(world, target) ||
            !world.HasComponent<HealthComponent>(target) ||
            !GameplayStateQuery::CanAttack(world, source) ||
            !GameplayStateQuery::CanBeTargetedBy(world, source, target))
        {
            return false;
        }
```

아래로 교체:

```cpp
        if (!IsAliveForBasicAttackImpact(world, source) ||
            !IsAliveForBasicAttackImpact(world, target) ||
            !world.HasComponent<HealthComponent>(target) ||
            !GameplayStateQuery::CanAttack(world, source))
        {
            return false;
        }

        if (!GameplayStateQuery::CanBeTargetedBy(world, source, target))
            return false;

        if (!GameplayStateQuery::CanReceiveBasicAttack(world, source, target))
        {
            if (GameplayStateQuery::HasState(
                world,
                target,
                kGameplayStateDodgesBasicAttacksFlag))
            {
                GameplayFeedback::EnqueueWorldTextFeedback(
                    world,
                    tc,
                    source,
                    target,
                    GameplayFeedback::WorldTextFeedbackKind::Dodge);
            }
            return false;
        }
```

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp

기존 코드:

```cpp
                GameplayStatus::ApplyStun(
                    world,
                    tc,
                    target,
                    source,
                    eChampion::JAX,
                    eSkillSlot::E,
                    kJaxEStunDurationSec,
                    eStatusEffectId::JaxCounterStrike);
```

아래로 교체:

```cpp
                GameplayStatus::ApplyStun(
                    world,
                    tc,
                    target,
                    source,
                    eChampion::JAX,
                    eSkillSlot::E,
                    kJaxEStunDurationSec);
```

기존 코드:

```cpp
        state.bCounterStrikeActive = false;
        state.counterTimerSec = 0.f;
        ClearJaxEStageWindow(world, caster);
```

아래에 추가:

```cpp
        GameplayStatus::RemoveSkillStatus(
            world,
            caster,
            eChampion::JAX,
            eSkillSlot::E);
```

기존 코드:

```cpp
        state.bCounterStrikeActive = true;
        state.counterTimerSec = state.counterDurationSec;
        state.counterRank = ctx.skillRank;
        ClearMove(*ctx.pWorld, ctx.casterEntity);
        std::cout << "[JaxSim] E counter start caster=" << ctx.casterEntity << "\n";
```

아래로 교체:

```cpp
        state.bCounterStrikeActive = true;
        state.counterTimerSec = state.counterDurationSec;
        state.counterRank = ctx.skillRank;
        GameplayStatus::ApplyDodgeBasicAttack(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterEntity,
            eChampion::JAX,
            eSkillSlot::E,
            state.counterDurationSec);
        ClearMove(*ctx.pWorld, ctx.casterEntity);
        std::cout << "[JaxSim] E counter start caster=" << ctx.casterEntity << "\n";
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

기존 코드:

```cpp
    bool_t IsAirborne(CWorld& world, EntityID target)
    {
        return target != NULL_ENTITY &&
            world.IsAlive(target) &&
            world.HasComponent<YasuoAirborneComponent>(target);
    }
```

아래로 교체:

```cpp
    bool_t IsAirborne(CWorld& world, EntityID target)
    {
        return target != NULL_ENTITY &&
            world.IsAlive(target) &&
            (world.HasComponent<YasuoAirborneComponent>(target) ||
                GameplayStateQuery::IsAirborne(world, target));
    }
```

1-14. C:/Users/tnest/Desktop/Winters/Data/GameData/Champion/champions.runtime.json

새 파일:

```json
{
  "schemaVersion": 1,
  "runtimeAuthority": "server",
  "championCapacity": 150,
  "champions": [
    {
      "id": "IRELIA",
      "numericId": 1,
      "stats": {
        "baseHp": 600.0,
        "hpPerLevel": 100.0,
        "baseMana": 300.0,
        "manaPerLevel": 50.0,
        "baseAd": 65.0,
        "adPerLevel": 3.5,
        "baseArmor": 30.0,
        "armorPerLevel": 4.0,
        "baseMr": 30.0,
        "mrPerLevel": 1.25,
        "baseAttackSpeed": 0.90,
        "attackSpeedRatio": 0.90,
        "attackSpeedPerLevel": 0.025,
        "baseAttackRange": 2.10,
        "baseMoveSpeed": 5.0,
        "navArriveRadius": 0.15,
        "spatialRadius": 0.75,
        "sightRange": 19.0
      },
      "basicAttack": {
        "cooldownSec": 0.60,
        "rangeMax": 2.10,
        "windupSec": 0.46,
        "actionDurationSec": 0.46,
        "animPlaySpeed": 1.25
      },
      "skills": [
        { "slot": "Q", "cooldownSec": 0.60, "rangeMax": 6.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.36, "animPlaySpeed": 1.20 },
        { "slot": "W", "cooldownSec": 0.60, "rangeMax": 0.00, "manaCost": 0.0, "stageCount": 2, "stageWindowSec": 4.00, "lockDurationSec": 5.00, "animPlaySpeed": 1.00 },
        { "slot": "E", "cooldownSec": 0.60, "rangeMax": 9.00, "manaCost": 0.0, "stageCount": 2, "stageWindowSec": 3.50, "lockDurationSec": 0.90, "animPlaySpeed": 1.05 },
        { "slot": "R", "cooldownSec": 0.60, "rangeMax": 12.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.65, "animPlaySpeed": 1.00 }
      ],
      "statusEffects": [
        { "key": "irelia.r.slow", "slot": "R", "effectId": "GenericSlow", "stateFlags": ["Slowed"], "durationSec": 0.50, "moveSpeedMul": 0.50, "feedbackKind": "Slow", "feedbackTextKey": "status.slow" }
      ]
    },
    {
      "id": "JAX",
      "numericId": 14,
      "stats": {
        "baseHp": 600.0,
        "hpPerLevel": 100.0,
        "baseMana": 300.0,
        "manaPerLevel": 50.0,
        "baseAd": 55.0,
        "adPerLevel": 3.5,
        "baseArmor": 30.0,
        "armorPerLevel": 4.0,
        "baseMr": 30.0,
        "mrPerLevel": 1.25,
        "baseAttackSpeed": 0.60,
        "attackSpeedRatio": 0.60,
        "attackSpeedPerLevel": 0.025,
        "baseAttackRange": 1.50,
        "baseMoveSpeed": 5.0,
        "navArriveRadius": 0.15,
        "spatialRadius": 0.75,
        "sightRange": 19.0
      },
      "basicAttack": {
        "cooldownSec": 1.67,
        "rangeMax": 1.50,
        "windupSec": 1.00,
        "actionDurationSec": 1.00,
        "animPlaySpeed": 1.00
      },
      "skills": [
        { "slot": "Q", "cooldownSec": 5.00, "rangeMax": 7.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.60, "animPlaySpeed": 1.00 },
        { "slot": "W", "cooldownSec": 5.00, "rangeMax": 0.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.50, "animPlaySpeed": 1.00 },
        { "slot": "E", "cooldownSec": 5.00, "rangeMax": 0.00, "manaCost": 0.0, "stageCount": 2, "stageWindowSec": 2.00, "lockDurationSec": 0.70, "animPlaySpeed": 1.00 },
        { "slot": "R", "cooldownSec": 5.00, "rangeMax": 0.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.60, "animPlaySpeed": 1.00 }
      ],
      "statusEffects": [
        { "key": "jax.e.dodge_basic_attack", "slot": "E", "effectId": "JaxCounterStrike", "stateFlags": ["DodgesBasicAttacks"], "durationSec": 2.00, "moveSpeedMul": 1.00, "feedbackKind": "None" },
        { "key": "jax.e2.stun", "slot": "E", "effectId": "GenericStun", "stateFlags": ["Stunned", "CannotMove", "CannotAttack", "CannotCast"], "durationSec": 1.00, "moveSpeedMul": 1.00, "feedbackKind": "Stun", "feedbackTextKey": "status.stun" }
      ]
    },
    {
      "id": "YASUO",
      "numericId": 2,
      "stats": {
        "baseHp": 600.0,
        "hpPerLevel": 100.0,
        "baseMana": 300.0,
        "manaPerLevel": 50.0,
        "baseAd": 55.0,
        "adPerLevel": 3.5,
        "baseArmor": 30.0,
        "armorPerLevel": 4.0,
        "baseMr": 30.0,
        "mrPerLevel": 1.25,
        "baseAttackSpeed": 0.60,
        "attackSpeedRatio": 0.60,
        "attackSpeedPerLevel": 0.025,
        "baseAttackRange": 2.50,
        "baseMoveSpeed": 5.0,
        "navArriveRadius": 0.15,
        "spatialRadius": 0.75,
        "sightRange": 19.0
      },
      "basicAttack": {
        "cooldownSec": 0.60,
        "rangeMax": 2.50,
        "windupSec": 0.50,
        "actionDurationSec": 0.50,
        "animPlaySpeed": 0.85
      },
      "skills": [
        { "slot": "Q", "cooldownSec": 0.60, "rangeMax": 5.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.50, "animPlaySpeed": 0.85 },
        { "slot": "W", "cooldownSec": 0.60, "rangeMax": 4.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.25, "animPlaySpeed": 1.00 },
        { "slot": "E", "cooldownSec": 0.60, "rangeMax": 4.75, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.40, "animPlaySpeed": 1.00 },
        { "slot": "R", "cooldownSec": 0.60, "rangeMax": 14.00, "manaCost": 0.0, "stageCount": 1, "lockDurationSec": 0.60, "animPlaySpeed": 1.00 }
      ],
      "statusEffects": [
        { "key": "yasuo.q3.airborne", "slot": "Q", "effectId": "GenericAirborne", "stateFlags": ["Airborne", "CannotMove", "CannotAttack", "CannotCast"], "durationSec": 1.25, "moveSpeedMul": 1.00, "feedbackKind": "Airborne", "feedbackTextKey": "status.airborne" }
      ]
    }
  ]
}
```

1-15. C:/Users/tnest/Desktop/Winters/Data/GameData/Feedback/gameplay_feedback.json

새 파일:

```json
{
  "schemaVersion": 1,
  "fonts": {
    "status.default": {
      "fontKey": "status.default",
      "fontPath": "Resource/Texture/UI/ux/fonts/notosanscjk-regular.ttf",
      "size": 28,
      "outline": 2,
      "lifetimeSec": 0.70
    }
  },
  "worldText": [
    { "key": "combat.dodge", "kind": "Dodge", "text": "회피", "fontKey": "status.default" },
    { "key": "status.slow", "kind": "Slow", "text": "느려짐", "fontKey": "status.default" },
    { "key": "status.stun", "kind": "Stun", "text": "기절", "fontKey": "status.default" },
    { "key": "status.airborne", "kind": "Airborne", "text": "공중에뜸", "fontKey": "status.default" }
  ],
  "championOverrides": [
    { "champion": "IRELIA", "statusKey": "irelia.r.slow", "textKey": "status.slow" },
    { "champion": "JAX", "statusKey": "jax.e2.stun", "textKey": "status.stun" },
    { "champion": "YASUO", "statusKey": "yasuo.q3.airborne", "textKey": "status.airborne" }
  ]
}
```

1-16. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Data/ChampionRuntimeDataJsonLoader.h

CONFIRM_NEEDED:

```text
현재 nlohmann json.hpp는 Client/Public/Network/Backend/json.hpp 쪽에 있으며 Shared/Server GameSim에서 직접 의존하기 어렵다.
Server가 Data/GameData/Champion/champions.runtime.json을 canonical runtime data로 읽으려면 json.hpp를 Shared 또는 Engine 공용 위치로 이동하거나, 별도 경량 파서를 확정해야 한다.
이 위치가 확정되면 ChampionRuntimeDataJsonLoader.h/.cpp와 Server.vcxproj ClCompile 항목을 별도 계획으로 작성한다.
```

1-17. C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

CONFIRM_NEEDED:

```text
이번 계획의 C++ 추가 파일은 header-only 중심이므로 즉시 ClCompile 항목은 없다.
다만 Visual Studio 필터 노출을 위해 ClInclude 추가가 필요할 수 있다.
/plan-rules 문서 정책상 vcxproj 수정은 사용자가 명시 요청할 때 별도 계획으로 분리한다.
```

2. 검증

1. `.claude/gotchas.md`는 현재 `C:/Users/tnest/Desktop/Winters/.claude/gotchas.md`에 존재하지 않는다. `/plan-rules` read order 중 누락 항목이므로 검증 로그에 남긴다.
2. 현재 코드베이스에는 이미 `GameplayFeedback.h`, `GameplayFeedbackQueue.h`, `StatusEffectRequests.h`, `StatusEffectSystem`의 TickContext overload, Irelia R slow, Yasuo Q3 airborne/R airborne target, Client EventApplier world text 해석이 들어가 있다.
3. 현재 코드베이스에는 아직 `ChampionRuntimeDataDef.h`, `ChampionRuntimeDataRegistry.h`, `ChampionRuntimeMatchData.h`, `Data/GameData` 스크립트, JSON loader가 없다.
4. 현재 Jax E는 E2 stun은 존재하지만, E 활성 중 기본 공격만 회피하는 `kGameplayStateDodgesBasicAttacksFlag` 적용과 기본 공격 impact의 Dodge feedback 처리가 아직 없다.
5. 적용 후 `git diff --check`를 실행한다.
6. 적용 후 `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`를 실행한다.
7. Jax 검증은 적 챔피언/적 미니언 기본 공격이 E 활성 중 `회피` world text를 띄우고 피해를 적용하지 않는지 확인한다. 스킬 피해는 이 dodge flag만으로 막히면 안 된다.
8. Irelia/Yasuo 검증은 Irelia R = `느려짐`, Yasuo Q3 = `공중에뜸`, Yasuo R = airborne 대상만 허용인지 확인한다.
9. 10인 roster 검증은 `GetChampionRuntimeMatchData().Count()`가 매치 시작 후 실제 선택된 슬롯 수와 일치하고, 최대 10을 넘지 않는지 확인한다.
10. 150명 확장 검증은 JSON/Lua loader 확정 전까지는 fallback registry로 기존 플레이가 깨지지 않는 것을 1차 DoD로 둔다.
