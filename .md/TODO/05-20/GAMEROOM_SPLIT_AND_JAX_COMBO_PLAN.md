Session - GameRoom Champion AI 분리와 Jax 전용 champion attack combo를 작게 확실히 적용한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomChampionAI.cpp

새 파일:

```cpp
#include "Game/GameRoom.h"
#include "Game/ServerMinionWaveRuntime.h"

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Systems/ChampionAISystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
    constexpr u32_t kStructureKindTurret =
        static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
    constexpr u32_t kLaneTop = static_cast<u32_t>(Winters::Map::eLane::Top);
    constexpr u32_t kLaneMid = static_cast<u32_t>(Winters::Map::eLane::Mid);
    constexpr u32_t kLaneBot = static_cast<u32_t>(Winters::Map::eLane::Bot);
    constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;

    u8_t TeamByte(eTeam team)
    {
        return static_cast<u8_t>(team);
    }

    bool_t IsValidChampionAILane(u8_t lane)
    {
        return lane == static_cast<u8_t>(kLaneTop) ||
            lane == static_cast<u8_t>(kLaneMid) ||
            lane == static_cast<u8_t>(kLaneBot);
    }

    Vec3 NormalizeXZOrForward(const Vec3& v, eTeam team)
    {
        const Vec3 fallback =
            (team == eTeam::Blue) ? Vec3{ -1.f, 0.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
        return WintersMath::NormalizeXZ(
            v,
            fallback,
            std::numeric_limits<f32_t>::epsilon());
    }

    void OutputServerAITrace(const char* pText)
    {
        if (!pText)
            return;

        WintersOutputAIDebugStringA(pText);
    }
}

void CGameRoom::Phase_ServerBotAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    CChampionAISystem::Execute(m_world, tc, m_pendingExecCommands);
}

u8_t CGameRoom::ResolveInitialBotLane(const LobbySlotState& slot) const
{
    if (!slot.bBot || slot.bDummy)
        return GetGameSimRosterLane(slot.slotId);

    if (IsValidChampionAILane(slot.botLane))
        return slot.botLane;

    static constexpr u8_t kBotLanes[] =
    {
        static_cast<u8_t>(kLaneTop),
        static_cast<u8_t>(kLaneMid),
        static_cast<u8_t>(kLaneBot),
    };

    const u32_t seed =
        static_cast<u32_t>(slot.slotId) * 1103515245u ^
        static_cast<u32_t>(slot.team) * 2654435761u ^
        static_cast<u32_t>(slot.botDifficulty) * 2246822519u ^
        static_cast<u32_t>(slot.champion) * 3266489917u;

    return kBotLanes[seed % static_cast<u32_t>(sizeof(kBotLanes) / sizeof(kBotLanes[0]))];
}

Vec3 CGameRoom::ResolveChampionAILaneGoal(eTeam team, u8_t lane) const
{
    const u8_t waypointLane = CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
    Vec3 goal = GetGameSimLaneGatherPosition(lane, TeamByte(team));

    if (waypointCount >= 2u)
    {
        const u32_t index = std::max(1u, waypointCount / 2u);
        goal = GetServerMinionWaypoint(team, waypointLane, index);
    }
    else if (waypointCount == 1u)
    {
        goal = GetServerMinionWaypoint(team, waypointLane, 0u);
    }

    goal.y = 1.f;
    return goal;
}

Vec3 CGameRoom::ResolveChampionAISafeAnchor(eTeam team, u8_t lane)
{
    Vec3 best = GetGameSimLaneGatherPosition(lane, TeamByte(team));
    bool_t bFound = false;
    f32_t bestScore = std::numeric_limits<f32_t>::max();
    const u8_t waypointLane = CServerMinionWaveRuntime::ResolveWaypointLane(team, lane);
    const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);

    auto getWaypoint = [&](u32_t index)
    {
        return GetServerMinionWaypoint(team, waypointLane, index);
    };

    auto scoreLaneDistance = [&](const Vec3& pos) -> f32_t
        {
            if (waypointCount >= 2u)
            {
                f32_t score = std::numeric_limits<f32_t>::max();
                for (u32_t i = 1u; i < waypointCount; ++i)
                {
                    f32_t t = 0.f;
                    const f32_t distSq = WintersMath::DistanceSqPointToSegmentXZ(
                        pos,
                        getWaypoint(i - 1u),
                        getWaypoint(i),
                        &t,
                        std::numeric_limits<f32_t>::epsilon());
                    score = std::min(score, distSq);
                }
                return score;
            }

            if (waypointCount == 1u)
                return WintersMath::DistanceSqXZ(pos, getWaypoint(0u));

            return WintersMath::DistanceSqXZ(pos, best);
        };

    m_world.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID, StructureComponent& structure, TransformComponent& transform)
        {
            if (structure.team != team)
                return;
            if (structure.kind != kStructureKindTurret)
                return;
            if (structure.tier != static_cast<u32_t>(Winters::Map::eTurretTier::Outer))
                return;
            if (structure.lane != lane)
                return;

            const Vec3 towerPos = transform.GetPosition();
            const f32_t score = scoreLaneDistance(towerPos);
            if (score < bestScore)
            {
                bestScore = score;
                best = towerPos;
                bFound = true;
            }
        });

    if (bFound && waypointCount >= 2u)
    {
        const Vec3 start = getWaypoint(0u);
        const Vec3 next = getWaypoint(1u);
        const Vec3 laneDir = NormalizeXZOrForward(
            Vec3{ next.x - start.x, 0.f, next.z - start.z },
            team);
        best = Vec3{
            best.x - laneDir.x * kChampionAISafeAnchorBehindTurret,
            best.y,
            best.z - laneDir.z * kChampionAISafeAnchorBehindTurret
        };
    }
    else if (bFound)
    {
        best.x += (team == eTeam::Blue) ? -kChampionAISafeAnchorBehindTurret : kChampionAISafeAnchorBehindTurret;
    }
    else if (waypointCount > 1u)
    {
        best = getWaypoint(1u);
    }
    else if (waypointCount > 0u)
    {
        best = getWaypoint(0u);
    }

    best.y = 1.f;
    return best;
}

void CGameRoom::RefreshChampionAIGoals()
{
    m_world.ForEach<ChampionAIComponent>(
        [this](EntityID, ChampionAIComponent& ai)
        {
            ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
            ai.retreatGoal = ai.safeAnchor;
            ai.laneGoal = ResolveChampionAILaneGoal(ai.team, ai.lane);
            const u8_t waypointLane = CServerMinionWaveRuntime::ResolveWaypointLane(ai.team, ai.lane);
            char msg[256]{};
            sprintf_s(msg,
                "[ChampionAI] lane goal team=%u champ=%u lane=%u wpLane=%u advance=(%.2f,%.2f,%.2f) safe=(%.2f,%.2f,%.2f)\n",
                static_cast<u32_t>(ai.team),
                static_cast<u32_t>(ai.champion),
                static_cast<u32_t>(ai.lane),
                static_cast<u32_t>(waypointLane),
                ai.laneGoal.x,
                ai.laneGoal.y,
                ai.laneGoal.z,
                ai.safeAnchor.x,
                ai.safeAnchor.y,
                ai.safeAnchor.z);
            OutputServerAITrace(msg);
        });
}
```

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/ChampionAISystem.h"
#include "Shared/GameSim/Systems/BuffSystem.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/BuffSystem.h"
```

삭제할 코드:

```cpp
void CGameRoom::Phase_ServerBotAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    CChampionAISystem::Execute(m_world, tc, m_pendingExecCommands);
}
```

삭제할 범위:
`u8_t CGameRoom::ResolveInitialBotLane(const LobbySlotState& slot) const` 줄부터 `void CGameRoom::RefreshChampionAIGoals()` 함수 끝까지 삭제한다.
다음 줄인 `EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)`은 남긴다.

1-3. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\Private\Game\GameRoom.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\Private\Game\GameRoomChampionAI.cpp" />
```

1-4. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

기존 코드:

```xml
    <ClCompile Include="..\Private\Game\GameRoom.cpp">
      <Filter>02. Game\00. GameRoom</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\Private\Game\GameRoomChampionAI.cpp">
      <Filter>02. Game\00. GameRoom</Filter>
    </ClCompile>
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.h

기존 코드:

```cpp
struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

struct ChampionAIProfile
```

아래로 교체:

```cpp
struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

struct ChampionAIComboStep
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    u16_t itemId = 0;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    f32_t selfHpMinRatio = 0.f;
    f32_t enemyHpMaxRatio = 1.f;
};

struct ChampionAIComboPlan
{
    ChampionAIComboStep steps[6]{};
    u8_t stepCount = 0;
};

struct ChampionAIProfile
```

기존 코드:

```cpp
const ChampionAIProfile& GetChampionAIProfile(eChampion champion);
```

아래에 추가:

```cpp
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion);
```

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
	EntityID lockedChampion = NULL_ENTITY;
	EntityID targetMinion = NULL_ENTITY;
	EntityID targetStructure = NULL_ENTITY;
	EntityID alliedWave = NULL_ENTITY;
```

아래에 추가:

```cpp
	EntityID comboTarget = NULL_ENTITY;
	u8_t comboStep = 0;
```

1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

`GetChampionAIProfile` 함수 끝에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    default:
        return s_Default;
    }
}
```

아래에 추가:

```cpp
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion)
{
    static constexpr ChampionAIComboPlan s_Default{};
    static constexpr ChampionAIComboPlan s_Jax{
        {
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::Q), 0, 0.f, 7.0f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::W), 0, 0.f, 1.75f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::E), 0, 0.f, 2.5f, 0.35f, 1.00f },
            ChampionAIComboStep{ static_cast<u8_t>(eSkillSlot::BasicAttack), 0, 0.f, 1.75f, 0.35f, 1.00f },
        },
        4
    };

    switch (champion)
    {
    case eChampion::JAX:
        return s_Jax;
    default:
        return s_Default;
    }
}
```

1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

기존 코드:

```cpp
    bool_t IsSkillReady(CWorld& world, EntityID self, u8_t slot)
    {
        if (!world.HasComponent<SkillStateComponent>(self))
            return true;
        if (slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;
        return world.GetComponent<SkillStateComponent>(self).slots[slot].cooldownRemaining <= 0.f;
    }
```

아래에 추가:

```cpp
    bool_t CanUseComboStep(
        CWorld& world,
        EntityID self,
        const ChampionAIComboStep& step,
        const ChampionAIContext& ctx)
    {
        if (step.slot >= static_cast<u8_t>(eSkillSlot::SLOT_END))
            return false;
        if (step.selfHpMinRatio > 0.f && ctx.selfHpRatio + 0.001f < step.selfHpMinRatio)
            return false;
        if (step.enemyHpMaxRatio < 0.999f && ctx.enemyHpRatio > step.enemyHpMaxRatio)
            return false;
        if (ctx.enemyDistance + 0.001f < step.minRange)
            return false;
        if (step.maxRange > 0.f && ctx.enemyDistance > step.maxRange)
            return false;
        return IsSkillReady(world, self, step.slot);
    }
```

기존 코드:

```cpp
    bool_t EmitSkillCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        u8_t slot,
        const char* reason,
        std::vector<GameCommand>& outCommands)
```

아래로 교체:

```cpp
    bool_t EmitSkillCommand(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        eChampion champion,
        const Vec3& selfPos,
        EntityID target,
        u8_t slot,
        const char* reason,
        std::vector<GameCommand>& outCommands,
        u16_t itemId = 0u)
```

같은 함수 안에서 기존 코드:

```cpp
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = WintersMath::DirectionXZ(selfPos, targetPos);
```

아래로 교체:

```cpp
        GameCommand cmd = MakeAICommand(ai, tc, self, eCommandKind::CastSkill);
        cmd.slot = slot;
        cmd.itemId = itemId;
        cmd.targetEntity = target;
        cmd.groundPos = targetPos;
        cmd.direction = WintersMath::DirectionXZ(selfPos, targetPos);
```

`TryEmitAttackChampionSkill` 함수 끝 바로 아래에 추가:

기존 코드:

```cpp
    bool_t TryEmitAttackChampionSkill(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIProfile& profile,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const u8_t count = std::min(profile.skillRuleCount, static_cast<u8_t>(4));
        for (u8_t i = 0; i < count; ++i)
        {
            const ChampionAISkillRule& rule = profile.skillRules[i];
            if (rule.score <= 0.f ||
                rule.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) ||
                ctx.enemyDistance + 0.001f < rule.minRange)
            {
                continue;
            }

            if (EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
                rule.slot, "lane-attack-champion-skill", outCommands))
            {
                return true;
            }
        }

        return false;
    }
```

아래에 추가:

```cpp
    bool_t EmitChampionAIComboStep(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIComboStep& step,
        std::vector<GameCommand>& outCommands)
    {
        if (step.slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            return EmitBasicAttackCommand(world, tc, self, ai, champion.id, selfPos,
                target, eChampionAIAction::AttackChampion,
                "combo-attack-champion-ba", outCommands);
        }

        return EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target,
            step.slot, "combo-attack-champion-skill", outCommands, step.itemId);
    }

    bool_t TryEmitAttackChampionCombo(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        EntityID target,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const ChampionAIComboPlan& combo = GetChampionAIComboPlan(champion.id);
        if (combo.stepCount == 0u)
            return false;

        if (ai.comboTarget != target)
        {
            ai.comboTarget = target;
            ai.comboStep = 0u;
        }

        const u8_t stepCount = std::min(combo.stepCount, static_cast<u8_t>(6u));
        for (u8_t attempt = 0u; attempt < stepCount; ++attempt)
        {
            const u8_t index = static_cast<u8_t>((ai.comboStep + attempt) % stepCount);
            const ChampionAIComboStep& step = combo.steps[index];
            if (!CanUseComboStep(world, self, step, ctx))
                continue;

            if (EmitChampionAIComboStep(world, tc, self, ai, champion, selfPos,
                target, step, outCommands))
            {
                const u8_t nextStep = static_cast<u8_t>((index + 1u) % stepCount);
                ai.comboStep = nextStep;
                if (nextStep == 0u)
                    ai.comboTarget = NULL_ENTITY;
                return true;
            }
        }

        return false;
    }
```

기존 코드:

```cpp
        const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
        if (TryEmitAttackChampionSkill(world, tc, self, ai, champion, selfPos,
            target, profile, ctx, outCommands))
        {
            return true;
        }
```

아래로 교체:

```cpp
        if (TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
            target, ctx, outCommands))
        {
            return true;
        }

        const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
        if (TryEmitAttackChampionSkill(world, tc, self, ai, champion, selfPos,
            target, profile, ctx, outCommands))
        {
            return true;
        }
```

기존 코드:

```cpp
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
```

아래로 교체:

```cpp
        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
```

`ExecuteLaneCombat` 함수 안에서 아래 기존 코드를 교체:

기존 코드:

```cpp
        if (ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands))
        {
            return;
        }

        ai.intent = eChampionAIIntent::FarmMinion;
```

아래로 교체:

```cpp
        if (ai.comboTarget == ctx.enemyChampion &&
            ai.comboTarget != NULL_ENTITY &&
            CanHarassChampion(ai, ctx) &&
            TryEmitAttackChampionCombo(world, tc, self, ai, champion, selfPos,
                ctx.enemyChampion, ctx, outCommands))
        {
            return;
        }

        if (ShouldAttackChampion(tc, self, ai, ctx) &&
            TryEmitAttackChampion(world, tc, self, ai, champion, selfPos, ctx, outCommands))
        {
            return;
        }

        ai.intent = eChampionAIIntent::FarmMinion;
        ai.comboTarget = NULL_ENTITY;
        ai.comboStep = 0u;
```

1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

사망 처리 AI clear 블록에서 기존 코드:

```cpp
                ai.lockedChampion = NULL_ENTITY;
                ai.targetMinion = NULL_ENTITY;
                ai.targetStructure = NULL_ENTITY;
                ai.alliedWave = NULL_ENTITY;
                ai.bWaveJoined = false;
                ai.bStructureWaveTanking = false;
                ai.bInsideEnemyTurretDanger = false;
```

아래로 교체:

```cpp
                ai.lockedChampion = NULL_ENTITY;
                ai.targetMinion = NULL_ENTITY;
                ai.targetStructure = NULL_ENTITY;
                ai.alliedWave = NULL_ENTITY;
                ai.comboTarget = NULL_ENTITY;
                ai.comboStep = 0u;
                ai.bWaveJoined = false;
                ai.bStructureWaveTanking = false;
                ai.bInsideEnemyTurretDanger = false;
```

부활 처리 AI clear 블록에서 기존 코드:

```cpp
            ai.lockedChampion = NULL_ENTITY;
            ai.targetMinion = NULL_ENTITY;
            ai.targetStructure = NULL_ENTITY;
            ai.alliedWave = NULL_ENTITY;
            ai.bWaveJoined = false;
            ai.bStructureWaveTanking = false;
            ai.bInsideEnemyTurretDanger = false;
            ai.decisionTimer = 0.25f;
```

아래로 교체:

```cpp
            ai.lockedChampion = NULL_ENTITY;
            ai.targetMinion = NULL_ENTITY;
            ai.targetStructure = NULL_ENTITY;
            ai.alliedWave = NULL_ENTITY;
            ai.comboTarget = NULL_ENTITY;
            ai.comboStep = 0u;
            ai.bWaveJoined = false;
            ai.bStructureWaveTanking = false;
            ai.bInsideEnemyTurretDanger = false;
            ai.decisionTimer = 0.25f;
```

CONFIRM_NEEDED:
- AI Debug UI에 combo step 표시 필드를 추가할지는 이번 범위에서 제외한다. 이번 계획은 서버 행동과 로그 reason 확인까지만 다룬다.
- 전체 챔피언 combo plan은 이번 범위에서 제외한다. hook/fallback 동작이 챔피언별로 다르므로 Jax 검증 후 별도 세션으로 확장한다.

2. 검증

미검증:
- 코드 미반영
- `GameRoomChampionAI.cpp` 분리 후 link/build 미검증
- Jax combo의 실제 `Q -> W -> E -> BasicAttack` 체감 미검증

검증 명령:
- `git diff --check`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- `GameRoomChampionAI.cpp` 계획 코드에 Red top/bot remap helper 복사본이 없고, 기존 `CServerMinionWaveRuntime::ResolveWaypointLane`만 호출되는지 확인.
- F5 서버 권위 흐름에서 Bot AI가 기존 `MoveToOuterTurret -> WaitForWave -> LaneCombat -> Retreat/Recalling` 흐름을 유지하는지 확인.
- AI Debug 패널에서 Jax 봇의 `Champion` 공격 override를 눌렀을 때 `[ChampionAI] ... reason=combo-attack-champion-skill`와 `reason=combo-attack-champion-ba`가 섞여 찍히는지 확인.
- `AttackChampion`이 한 번 뽑힌 뒤 `comboTarget`이 살아있는 동안은 Farm 재추첨보다 Jax combo 진행이 먼저 처리되고, `Q -> W -> E -> BasicAttack` 한 바퀴가 끝나면 `comboTarget`이 비워져 Farm/Retreat 흐름으로 복귀하는지 확인.
- Jax가 적 챔피언 대상으로 Q 도약 후 W/E를 한 번씩 섞고, 기본 공격으로 W empower가 소비되는지 확인.
- 적 챔피언이 없거나 retreat/turret danger 조건이면 combo step이 남아서 다음 대상에게 이어지지 않는지 확인.
