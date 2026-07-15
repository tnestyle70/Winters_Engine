Session - 미드 레인 타워(아군/적군 무관) 파괴 시 전 봇을 기존 GroupMidDefense 경로로 미드에 집결시킨다.

Bot AI는 GameCommand 생산자다. 이 계획은 월드 상태를 읽고 기존 `EmitMoveCommand` 경로로 명령만 내며, 게임플레이 진실을 직접 변형하지 않는다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h

`struct ChampionAIContext`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    bool_t bAlliedOuterTurretLost = false;
```

아래에 추가:

```cpp
    bool_t bMidLaneTurretLost = false;
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`HasAlliedOuterTurretLost` 함수 전체 바로 아래에 추가 (anonymous namespace 내부):

기존 코드:

```cpp
    bool_t HasAlliedOuterTurretLost(CWorld& world, eTeam team)
    {
        bool_t bLost = false;
        const u32_t turretKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
        const u32_t outerTier =
            static_cast<u32_t>(Winters::Map::eTurretTier::Outer);

        world.ForEach<StructureComponent>(
            [&](EntityID entity, StructureComponent& structure)
            {
                if (bLost ||
                    structure.team != team ||
                    structure.kind != turretKind ||
                    structure.tier != outerTier)
                {
                    return;
                }

                if (!IsAliveTarget(world, entity))
                    bLost = true;
            });

        return bLost;
    }
```

아래에 추가:

```cpp
    // 미드 레인 타워(팀 무관, 티어 무관)가 하나라도 파괴되면 true.
    // 양 팀 봇이 같은 조건으로 GroupMidDefense에 진입해 미드에서 5:5가 형성된다.
    // 넥서스 타워는 lane=Base라 이 필터에 걸리지 않는다.
    bool_t HasMidLaneTurretLost(CWorld& world)
    {
        bool_t bLost = false;
        const u32_t turretKind =
            static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret);
        const u32_t midLane =
            static_cast<u32_t>(Winters::Map::eLane::Mid);

        world.ForEach<StructureComponent>(
            [&](EntityID entity, StructureComponent& structure)
            {
                if (bLost ||
                    structure.kind != turretKind ||
                    structure.lane != midLane)
                {
                    return;
                }

                if (!IsAliveTarget(world, entity))
                    bLost = true;
            });

        return bLost;
    }
```

`BuildChampionAIContext` 끝부분(`return ctx;` 직전)에서 아래 코드를:

기존 코드:

```cpp
        ctx.bAlliedOuterTurretLost = ai.bMidDefenseActive;
        ctx.midDefenseAnchor = ai.midDefenseAnchor;
        if (ai.decisionTimer <= 0.f)
        {
            ctx.bAlliedOuterTurretLost =
                ai.bMidDefenseActive ||
                HasAlliedOuterTurretLost(world, champion.team);
            ctx.midDefenseAnchor = ResolveMidDefenseAnchor(
                world,
                champion.team,
                self,
                ai.safeAnchor);
        }
```

아래로 교체:

```cpp
        ctx.bAlliedOuterTurretLost = ai.bMidDefenseActive;
        ctx.bMidLaneTurretLost = ai.bMidDefenseActive;
        ctx.midDefenseAnchor = ai.midDefenseAnchor;
        if (ai.decisionTimer <= 0.f)
        {
            ctx.bAlliedOuterTurretLost =
                ai.bMidDefenseActive ||
                HasAlliedOuterTurretLost(world, champion.team);
            ctx.bMidLaneTurretLost =
                ai.bMidDefenseActive ||
                HasMidLaneTurretLost(world);
            ctx.midDefenseAnchor = ResolveMidDefenseAnchor(
                world,
                champion.team,
                self,
                ai.safeAnchor);
        }
```

`Execute`의 per-bot 람다 안 GroupMidDefense 진입 조건에서 아래 코드를:

기존 코드:

```cpp
            if (!ai.bMidDefenseActive &&
                ctx.bAlliedOuterTurretLost &&
                ctx.bCanMove &&
                ctx.enemyChampion == NULL_ENTITY &&
                ai.comboTarget == NULL_ENTITY &&
                ai.divePhase == eChampionAIDivePhase::None)
```

아래로 교체:

```cpp
            if (!ai.bMidDefenseActive &&
                (ctx.bAlliedOuterTurretLost || ctx.bMidLaneTurretLost) &&
                ctx.bCanMove &&
                ctx.enemyChampion == NULL_ENTITY &&
                ai.comboTarget == NULL_ENTITY &&
                ai.divePhase == eChampionAIDivePhase::None)
```

## 2. 검증

반영 완료 (2026-07-15):

- 계획서 그대로 2파일 4편집 적용.
- 전체 솔루션 빌드 에러 0, WintersGame.exe/WintersServer.exe/SimLab.exe 링크 성공.
- SimLab PASS — 해시 18110C0D7C01FA27로 변경 전과 동일 (시나리오에 미드 타워 파괴가 없어 래치 미발동 = 비침습 증명).
- 서버 스모크 15초 클린.

미검증:

- 런타임에서 미드 타워 파괴 → 양 팀 봇 미드 집결 미검증 (인게임 게이트)

검증 명령:

- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
- Tools/Bin/Debug/SimLab.exe → PASS 유지 확인 (AIShadow/CommandOutcome 시나리오 포함)

수동 확인 (인게임 5:5):

- F4 Structure Tuner → Low HP Preset으로 미드 외곽 타워를 빠르게 파괴.
- 파괴 직후 양 팀 봇 전원이 combo/dive 비활성 상태가 되는 결정 틱(0.2s 주기)에 각자 자기 진영 미드 살아있는 최저 티어 타워 뒤 포메이션 슬롯으로 이동하는지 확인.
- 탑/봇 레인 봇도 레인을 떠나 미드로 합류하는지, 미드 존 안에서 적 접근 시 교전(ExecuteLaneCombat 전환)하는지 확인.
- 리콜 중(Recalling) 봇은 리콜 종료 후 합류하는지 확인 (기존 stickiness 동작).

확인 필요:

- 한 팀의 미드 타워가 전부 파괴된 극단 상황에서 해당 팀 앵커가 `ai.safeAnchor` fallback으로 자연스럽게 동작하는지 인게임 확인 (ResolveMidDefenseAnchor의 기존 fallback 경로).

동작 메모 (기존 재사용 범위):

- 집결 실행/포메이션/교전 전환/래치 해제는 전부 기존 GroupMidDefense 경로 그대로: `ExecuteGroupMidDefense`가 매 결정마다 `ai.midDefenseAnchor = ctx.midDefenseAnchor`로 갱신, 포메이션 슬롯은 `ResolveMidDefenseAnchor`, 래치는 라이프사이클 리셋(`ResetChampionAIForLifecycle`)에서만 해제.
- 진입 게이트(bCanMove, 주변 적 챔피언 없음, combo/dive 없음)는 hard safety → active commitment → new utility 순서 준수를 위해 유지. 전투 중인 봇은 커밋 종료 후 다음 결정 틱에 합류한다.
