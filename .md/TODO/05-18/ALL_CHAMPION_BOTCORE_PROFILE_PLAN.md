Session - Custom Scene 봇 선택 시 모든 등록 챔피언이 서버 BotLaneAIComponent와 얇은 ChampionProfile로 플레이 가능하게 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAIPolicy.cpp

기존 코드:

```cpp
    constexpr BotChampionProfile MakeDefaultProfile()
    {
        return BotChampionProfile{
            eChampion::END,
            1.5f,
            6.f,
            10.f,
            18.f,
            14.f,
            1.00f,
            0.00f,
            0.35f,
            0.55f,
            1.00f,
            1.00f,
            1.00f,
            1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }
```

아래로 교체:

```cpp
    constexpr BotChampionProfile MakeDefaultProfile()
    {
        return BotChampionProfile{
            eChampion::END,
            1.5f,
            6.f,
            10.f,
            18.f,
            14.f,
            1.00f,
            0.00f,
            0.35f,
            0.55f,
            1.00f,
            1.00f,
            1.00f,
            1.00f,
            {},
            0
        };
    }
```

`MakeMasterYiProfile()` 바로 아래에 아래 코드 추가:

```cpp
    constexpr BotChampionProfile MakeAnnieProfile()
    {
        return BotChampionProfile{
            eChampion::ANNIE, 6.f, 9.f, 12.f, 18.f, 16.f,
            0.85f, 1.00f, 0.45f, 0.62f,
            1.05f, 1.10f, 1.05f, 0.80f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 0.8f },
            },
            2
        };
    }

    constexpr BotChampionProfile MakeGarenProfile()
    {
        return BotChampionProfile{
            eChampion::GAREN, 1.5f, 6.5f, 10.f, 18.f, 14.f,
            1.10f, 0.00f, 0.42f, 0.60f,
            0.90f, 0.95f, 1.05f, 1.05f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::E), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeIreliaProfile()
    {
        return BotChampionProfile{
            eChampion::IRELIA, 1.75f, 7.f, 10.f, 18.f, 14.f,
            1.15f, 0.10f, 0.42f, 0.60f,
            0.95f, 1.00f, 1.05f, 1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeKalistaProfile()
    {
        return BotChampionProfile{
            eChampion::KALISTA, 5.5f, 9.f, 12.f, 18.f, 16.f,
            0.85f, 1.35f, 0.45f, 0.65f,
            1.10f, 1.15f, 1.10f, 0.80f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeRivenProfile()
    {
        return BotChampionProfile{
            eChampion::RIVEN, 1.5f, 7.f, 10.f, 18.f, 14.f,
            1.15f, 0.00f, 0.40f, 0.58f,
            0.90f, 0.95f, 1.05f, 1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeSylasProfile()
    {
        return BotChampionProfile{
            eChampion::SYLAS, 1.5f, 7.f, 10.f, 18.f, 14.f,
            1.05f, 0.00f, 0.42f, 0.60f,
            0.95f, 1.00f, 1.00f, 1.00f,
            {},
            0
        };
    }

    constexpr BotChampionProfile MakeViegoProfile()
    {
        return BotChampionProfile{
            eChampion::VIEGO, 1.5f, 7.f, 10.f, 18.f, 14.f,
            1.10f, 0.00f, 0.40f, 0.58f,
            0.95f, 1.00f, 1.05f, 1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeYasuoProfile()
    {
        return BotChampionProfile{
            eChampion::YASUO, 1.75f, 7.f, 10.f, 18.f, 14.f,
            1.10f, 0.10f, 0.42f, 0.60f,
            0.95f, 1.00f, 1.05f, 1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }

    constexpr BotChampionProfile MakeYoneProfile()
    {
        return BotChampionProfile{
            eChampion::YONE, 1.75f, 7.f, 10.f, 18.f, 14.f,
            1.10f, 0.10f, 0.42f, 0.60f,
            0.95f, 1.00f, 1.05f, 1.00f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::W), 0.f, 0.8f },
            },
            2
        };
    }

    constexpr BotChampionProfile MakeZedProfile()
    {
        return BotChampionProfile{
            eChampion::ZED, 1.5f, 8.f, 10.f, 18.f, 14.f,
            1.05f, 0.20f, 0.40f, 0.58f,
            0.95f, 1.00f, 1.00f, 0.95f,
            {
                BotSkillRule{ static_cast<u8_t>(eSkillSlot::Q), 0.f, 1.f },
            },
            1
        };
    }
```

기존 코드:

```cpp
    static constexpr BotChampionProfile s_Default = MakeDefaultProfile();
    static constexpr BotChampionProfile s_Ashe = MakeAsheProfile();
    static constexpr BotChampionProfile s_Ezreal = MakeEzrealProfile();
    static constexpr BotChampionProfile s_Fiora = MakeFioraProfile();
    static constexpr BotChampionProfile s_Jax = MakeJaxProfile();
    static constexpr BotChampionProfile s_LeeSin = MakeLeeSinProfile();
    static constexpr BotChampionProfile s_Kindred = MakeKindredProfile();
    static constexpr BotChampionProfile s_MasterYi = MakeMasterYiProfile();
```

아래로 교체:

```cpp
    static constexpr BotChampionProfile s_Default = MakeDefaultProfile();
    static constexpr BotChampionProfile s_Annie = MakeAnnieProfile();
    static constexpr BotChampionProfile s_Ashe = MakeAsheProfile();
    static constexpr BotChampionProfile s_Ezreal = MakeEzrealProfile();
    static constexpr BotChampionProfile s_Fiora = MakeFioraProfile();
    static constexpr BotChampionProfile s_Garen = MakeGarenProfile();
    static constexpr BotChampionProfile s_Irelia = MakeIreliaProfile();
    static constexpr BotChampionProfile s_Jax = MakeJaxProfile();
    static constexpr BotChampionProfile s_Kalista = MakeKalistaProfile();
    static constexpr BotChampionProfile s_Kindred = MakeKindredProfile();
    static constexpr BotChampionProfile s_LeeSin = MakeLeeSinProfile();
    static constexpr BotChampionProfile s_MasterYi = MakeMasterYiProfile();
    static constexpr BotChampionProfile s_Riven = MakeRivenProfile();
    static constexpr BotChampionProfile s_Sylas = MakeSylasProfile();
    static constexpr BotChampionProfile s_Viego = MakeViegoProfile();
    static constexpr BotChampionProfile s_Yasuo = MakeYasuoProfile();
    static constexpr BotChampionProfile s_Yone = MakeYoneProfile();
    static constexpr BotChampionProfile s_Zed = MakeZedProfile();
```

기존 `switch (champion)`의 `case eChampion::ASHE:` 위에 아래 코드 추가:

```cpp
    case eChampion::ANNIE:
        return s_Annie;
```

기존 `case eChampion::JAX:` 아래와 기존 `case eChampion::LEESIN:` 주변을 아래 형태가 되도록 보강:

```cpp
    case eChampion::GAREN:
        return s_Garen;
    case eChampion::IRELIA:
        return s_Irelia;
    case eChampion::JAX:
        return s_Jax;
    case eChampion::KALISTA:
        return s_Kalista;
    case eChampion::KINDRED:
        return s_Kindred;
    case eChampion::LEESIN:
        return s_LeeSin;
    case eChampion::MASTERYI:
        return s_MasterYi;
    case eChampion::RIVEN:
        return s_Riven;
    case eChampion::SYLAS:
        return s_Sylas;
    case eChampion::VIEGO:
        return s_Viego;
    case eChampion::YASUO:
        return s_Yasuo;
    case eChampion::YONE:
        return s_Yone;
    case eChampion::ZED:
        return s_Zed;
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAISystem.cpp

`TryEmitAsheFightBT(...)` 바로 아래에 추가:

```cpp
    bool_t TryEmitEzrealFightBT(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        BotLaneAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const BotLaneAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        const EntityID target = ctx.enemyChampion;
        if (target == NULL_ENTITY)
            return false;

        const u8_t q = static_cast<u8_t>(eSkillSlot::Q);
        const f32_t qRange = GetDefaultChampionSkillRange(champion.id, q);
        if (qRange > 0.f &&
            ctx.selfHpRatio > 0.35f &&
            ctx.turretDanger < 1.00f &&
            ctx.enemyDistance <= qRange &&
            EmitSkillCommand(world, tc, self, ai, champion.id, selfPos, target, q,
                "bt-ezreal-q-poke", outCommands))
        {
            return true;
        }

        return false;
    }
```

기존 코드:

```cpp
        case eChampion::ASHE:
            if (TryEmitAsheFightBT(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return true;
            break;
        default:
            break;
```

아래로 교체:

```cpp
        case eChampion::ASHE:
            if (TryEmitAsheFightBT(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return true;
            break;
        case eChampion::EZREAL:
            if (TryEmitEzrealFightBT(world, tc, self, ai, champion, selfPos, ctx, outCommands))
                return true;
            break;
        default:
            break;
```

1-3. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    u8_t ResolveServerStructureLane(eTeam team, u32_t kind, u32_t tier, const Vec3& pos) const;
    Vec3 ResolveBotLaneAdvanceGoal(eTeam team, u8_t lane) const;
    Vec3 ResolveBotLaneWaitGoal(eTeam team, u8_t lane);
```

아래로 교체:

```cpp
    u8_t ResolveServerStructureLane(eTeam team, u32_t kind, u32_t tier, const Vec3& pos) const;
    u8_t ResolveInitialBotLane(const LobbySlotState& slot) const;
    Vec3 ResolveBotLaneAdvanceGoal(eTeam team, u8_t lane) const;
    Vec3 ResolveBotLaneWaitGoal(eTeam team, u8_t lane);
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/BotLaneAISystem.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/BotLaneAISystem.h"
#include "Shared/GameSim/Systems/BotLaneAIPolicy.h"
```

`CGameRoom::ResolveBotLaneAdvanceGoal(...)` 바로 위에 추가:

```cpp
u8_t CGameRoom::ResolveInitialBotLane(const LobbySlotState& slot) const
{
    if (!slot.bBot || slot.bDummy)
        return GetGameSimRosterLane(slot.slotId);

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

```

기존 코드:

```cpp
        BotLaneAIComponent ai{};
        ai.champion = slot.champion;
        ai.team = static_cast<eTeam>(slot.team);
        ai.difficulty = slot.botDifficulty;
        ai.lane = GetGameSimRosterLane(slot.slotId);
        ai.decisionTimer = kBotInitialDecisionDelaySec;
        ai.vRetreatGoal = spawnPos;
        const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
        ai.laneGoal = waypointCount > 0u
            ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
            : GetGameSimLaneGatherPosition(ai.lane, slot.team);

        if (slot.champion == eChampion::ASHE)
        {
            ai.championScanRange = 9.f;
            ai.minionScanRange = 12.f;
            ai.leashRange = 16.f;
        }
        else
        {
            ai.championScanRange = 6.f;
            ai.minionScanRange = 10.f;
            ai.leashRange = 14.f;
        }

        m_world.AddComponent<BotLaneAIComponent>(entity, ai);
```

아래로 교체:

```cpp
        BotLaneAIComponent ai{};
        const BotChampionProfile& profile = GetBotChampionProfile(slot.champion);
        ai.champion = slot.champion;
        ai.team = static_cast<eTeam>(slot.team);
        ai.difficulty = slot.botDifficulty;
        ai.lane = ResolveInitialBotLane(slot);
        ai.decisionTimer = kBotInitialDecisionDelaySec;
        ai.vRetreatGoal = spawnPos;
        ai.championScanRange = profile.championScanRange;
        ai.minionScanRange = profile.minionScanRange;
        ai.structureScanRange = profile.structureScanRange;
        ai.leashRange = profile.leashRange;
        ai.fRetreatUntilHpRatio = profile.reengageHpRatio;

        const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
        ai.laneGoal = waypointCount > 0u
            ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
            : GetGameSimLaneGatherPosition(ai.lane, slot.team);

        m_world.AddComponent<BotLaneAIComponent>(entity, ai);
```

2. 검증

미검증:
- 이 문서는 구현 계획만 작성했다. 코드 변경, 빌드, 런타임 스모크는 아직 수행하지 않았다.

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- Custom Scene에서 등록 챔피언 `Ezreal/Fiora/Jax/Kindred/LeeSin/MasterYi/Annie/Ashe/Yone/Irelia/Yasuo/Kalista/Sylas/Viego/Garen/Zed/Riven`을 각각 봇 슬롯으로 선택하고 게임 시작.
- 각 봇 챔피언 엔티티에 서버 `BotLaneAIComponent`가 붙고, AI Debug Panel에서 `MoveToLane`, `FarmMinion`, `FightChampion` 또는 `Kite`, `Retreat`, `AttackStructure` 상태가 전환되는지 확인.
- 봇 라인이 슬롯 고정 `GetGameSimRosterLane(slot.slotId)`가 아니라 `ResolveInitialBotLane(slot)` 결과로 top/mid/bot에 분산되는지 `[BotAI] lane goal` 로그로 확인.
- 공통 BotCore 기준 행동 확인: 라인 이동, BA, 막타 시도, 저체력 후퇴, 미니언이 타워를 받아줄 때 구조물 공격.
- 기존 기준 챔프 확인: Jax/Fiora/Ashe는 기존 BT 행동이 유지되고, Ezreal은 Q poke BT가 우선 동작한다.
- `cast-skill reject` 로그가 Sylas 같은 스킬 비활성 프로필에서 반복 스팸으로 나오지 않는지 확인.

확인 필요:
- Sylas는 Shared GameSim 쪽 런타임 스킬 기본값/서버 스킬 구현이 명확하지 않으므로 이번 계획에서는 스킬 자동 사용을 끄고 BA/Farm/Retreat/Siege만 보장한다.
- 새 `.h/.cpp` 파일을 추가하지 않으므로 `.vcxproj`/`.filters` 변경은 필요 없어야 한다.
