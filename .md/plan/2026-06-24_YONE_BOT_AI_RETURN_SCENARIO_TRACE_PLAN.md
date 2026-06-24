Session - Yone Bot E stage-2 계약을 런타임 복귀 시나리오로 증명한다.

1. 반영해야 하는 코드

본질:
- 이번 반영으로 `champions.json -> LoL definition pack -> generated gameplay defs -> CommandExecutor stage-2 gate` 계약은 PASS했다.
- 다음 본질은 "AI가 E 2타 명령을 만든다"가 아니라 "서버 GameSim이 E 1타 후 stage window를 열고, E 2타 명령을 받아, anchor로 복귀시키는가"를 결정론적으로 증명하는 것이다.
- 검증은 Client visual, UI, 입력, 렌더러를 거치지 않는다. 서버 권위 GameSim만 관측한다.

현재 코드 증거:
- `C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp`는 `YoneGameSim::RegisterHooks()`와 `YoneSimComponent` 생성을 이미 포함한다.
- `SpawnChampion()`은 `SkillRankComponent`를 만들지만 기본 랭크는 모두 0이다. `CommandExecutor`는 미습득 스킬을 `unlearned`로 거절한다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`는 `cmd.itemId == 2u`, `slot.currentStage == 1`, `slot.stageWindow > 0.f`, `IsSkillTwoStage(...)`일 때만 stage-2를 accept한다.
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yone/YoneGameSim.cpp`는 E 1타에서 `bSoulUnboundActive = true`, `anchorPosition = origin`을 설정하고, E 재시전에서 `StartSoulReturn(...)`을 호출한다.

1-1. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`MatchResult RunMatch(u64_t seed, u64_t tickCount)` 함수 바로 아래에 아래를 추가:

기존 코드:

```cpp
        result.finalHash = finalHash;
        return result;
    }
}
```

아래에 추가:

```cpp

    TickContext MakeProbeTickContext(
        u64_t tick,
        DeterministicRng& rng,
        EntityIdMap& entityMap,
        FlatWalkable& walkable)
    {
        TickContext tc{};
        tc.tickIndex = tick;
        tc.fDt = DeterministicTime::kFixedDt;
        tc.fSimulatedTimeSec = DeterministicTime::TickToSec(tick);
        tc.pRng = &rng;
        tc.pEntityMap = &entityMap;
        tc.localPlayer = NULL_ENTITY;
        tc.pWalkable = &walkable;
        return tc;
    }

    void TickYoneProbe(CWorld& world, const TickContext& tc)
    {
        CSkillCooldownSystem::Execute(world, tc);
        CCombatActionSystem::Execute(world, tc);
        CMoveSystem::Execute(world, tc);
        YoneGameSim::Tick(world, tc);
    }

    f32_t DistanceSqXZLocal(const Vec3& a, const Vec3& b)
    {
        const f32_t dx = a.x - b.x;
        const f32_t dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    bool_t RunYoneEReturnProbe()
    {
        CWorld world;
        DeterministicRng rng(20260624ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID yone = SpawnChampion(world, entityMap, eChampion::YONE, 0, 0);
        const EntityID target = SpawnChampion(world, entityMap, eChampion::JAX, 1, 5);

        world.GetComponent<TransformComponent>(yone).SetPosition(Vec3{ 0.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(target).SetPosition(Vec3{ 6.f, 0.f, 0.f });

        auto& ranks = world.GetComponent<SkillRankComponent>(yone);
        CSkillRankSystem::TryLevelSkill(ranks, static_cast<u8_t>(eSkillSlot::E));

        TickContext tick1 = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand eOut{};
        eOut.kind = eCommandKind::CastSkill;
        eOut.issuerEntity = yone;
        eOut.issuedAtTick = tick1.tickIndex;
        eOut.sequenceNum = 1u;
        eOut.slot = static_cast<u8_t>(eSkillSlot::E);
        eOut.targetEntity = target;
        eOut.groundPos = world.GetComponent<TransformComponent>(target).GetPosition();
        eOut.direction = Vec3{ 1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick1, eOut);
        TickYoneProbe(world, tick1);

        const auto& stateAfterOut = world.GetComponent<YoneSimComponent>(yone);
        if (!stateAfterOut.bSoulUnboundActive || stateAfterOut.bReturning)
        {
            std::printf("[SimLab][YoneE] FAIL: E out did not enter soul state\n");
            return false;
        }

        const Vec3 anchor = stateAfterOut.anchorPosition;

        TickContext tick2 = MakeProbeTickContext(2ull, rng, entityMap, walkable);
        GameCommand eReturn{};
        eReturn.kind = eCommandKind::CastSkill;
        eReturn.issuerEntity = yone;
        eReturn.issuedAtTick = tick2.tickIndex;
        eReturn.sequenceNum = 2u;
        eReturn.slot = static_cast<u8_t>(eSkillSlot::E);
        eReturn.itemId = 2u;
        eReturn.groundPos = anchor;
        eReturn.direction = Vec3{ -1.f, 0.f, 0.f };
        executor->ExecuteCommand(world, tick2, eReturn);

        const auto& stateAfterRecast = world.GetComponent<YoneSimComponent>(yone);
        if (!stateAfterRecast.bSoulUnboundActive || !stateAfterRecast.bReturning)
        {
            std::printf("[SimLab][YoneE] FAIL: E stage-2 did not start return\n");
            return false;
        }

        for (u64_t tick = 3ull; tick <= 40ull; ++tick)
        {
            TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
            TickYoneProbe(world, tc);
        }

        const auto& stateAfterReturn = world.GetComponent<YoneSimComponent>(yone);
        const Vec3 finalPos = world.GetComponent<TransformComponent>(yone).GetPosition();
        if (stateAfterReturn.bSoulUnboundActive || stateAfterReturn.bReturning)
        {
            std::printf("[SimLab][YoneE] FAIL: return did not clear soul state\n");
            return false;
        }
        if (DistanceSqXZLocal(finalPos, anchor) > 0.0001f)
        {
            std::printf("[SimLab][YoneE] FAIL: return did not reach anchor\n");
            return false;
        }

        std::printf("[SimLab][YoneE] PASS: stage-2 return reached anchor=(%.2f,%.2f,%.2f)\n",
            anchor.x,
            anchor.y,
            anchor.z);
        return true;
    }
```

1-2. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`main()` 안에서 `RegisterAllChampionHooks();` 직후와 `bool bPass = true;` 초기화를 아래로 교체:

기존 코드:

```cpp
    RegisterAllChampionHooks();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    bool bPass = true;
```

아래로 교체:

```cpp
    RegisterAllChampionHooks();

    const bool_t bYoneEReturnProbePass = RunYoneEReturnProbe();

    const MatchResult runA = RunMatch(seed, tickCount);
    const MatchResult runB = RunMatch(seed, tickCount);
    const MatchResult runC = RunMatch(seed + 1, tickCount);

    bool bPass = bYoneEReturnProbePass;
```

이유:
- SimLab 기본 실행에 포함해야 `Verify-LoLDataDrivenPipeline.ps1`가 요네 E runtime contract까지 자동 검증한다.
- 별도 옵션을 만들면 호출 누락이 생긴다. 현재 단계의 본질은 기능 토글이 아니라 회귀 방지다.
- 이 테스트는 GameSim 상태만 읽고, authoritative 결과를 직접 수정하지 않는다.

중단 조건:
- `RunYoneEReturnProbe()`가 `SkillRankComponent`, `SkillStateComponent`, stage window, Yone dash 완료 중 하나를 안정적으로 관측하지 못하면 코드 반영 전에 멈추고 해당 컴포넌트 앵커를 다시 확인한다.
- SimLab의 결정론 hash가 바뀌는 것은 허용되지만, 같은 seed replay PASS와 seed sensitivity PASS는 유지해야 한다.

2. 검증

검증 명령:

```powershell
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64
Tools/Bin/Debug/SimLab.exe
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-BotAiValidation.ps1 -SkipFullPipeline
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

성공 기준:
- SimLab 출력에 `[SimLab][YoneE] PASS: stage-2 return reached anchor=...`가 나온다.
- 같은 seed replay hash는 동일하고, seed+1 hash는 달라야 한다.
- `Run-BotAiValidation.ps1 -SkipFullPipeline`의 `Yone E stage-2 contract audit`가 PASS해야 한다.
- full data-driven pipeline이 `GameSim`, `Server`, `Client`, `SimLab` 빌드와 regression까지 PASS해야 한다.

핸드오프 메모:
- 이 계획은 "요네 AI가 언제 복귀할지"를 바꾸지 않는다.
- 이 계획은 "요네 E 재시전이 서버 GameSim에서 실제 복귀로 끝나는지"만 증명한다.
- 이 검증을 먼저 넣은 뒤에 tactic scoring을 건드려야 AI 판단 변경의 실패 원인을 분리할 수 있다.
